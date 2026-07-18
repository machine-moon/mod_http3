import socket
import ssl
import time

import pytest

from aioquic.h3.connection import H3_ALPN
from aioquic.quic.configuration import QuicConfiguration
from aioquic.quic.connection import QuicConnection


def _is_quic_v1_retry(datagram: bytes) -> bool:
    # RFC 9000 section 17.2: Retry is long-header packet type 0b11.
    return len(datagram) >= 5 and datagram[0] & 0xC0 == 0xC0 and (datagram[0] >> 4) & 0x03 == 0x03 and datagram[1:5] == b"\x00\x00\x00\x01"


class TestRetry:
    @pytest.fixture(autouse=True, scope="class")
    def _class_scope(self, env):
        from .env import H3Conf

        H3Conf(env).add_vhost_test1().install()
        assert env.apache_restart() == 0

    def test_001_one_initial_produces_one_retry(self, env):
        """Do not mistake a client retransmit for duplicate server output."""
        authority = f"test1.{env.http_tld}"
        config = QuicConfiguration(
            is_client=True,
            alpn_protocols=H3_ALPN,
            verify_mode=ssl.CERT_NONE,
            server_name=authority,
        )
        quic = QuicConnection(configuration=config)
        now = time.monotonic()
        target = (env.http_addr, env.https_port)
        quic.connect(target, now=now)
        outgoing = quic.datagrams_to_send(now=now)
        assert len(outgoing) == 1
        initial, _ = outgoing[0]
        assert len(initial) >= 1200

        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.bind(("127.0.0.1", 0))
            sock.settimeout(0.1)
            sock.sendto(initial, target)

            responses = []
            deadline = time.monotonic() + 0.75
            while time.monotonic() < deadline:
                try:
                    responses.append(sock.recv(65535))
                except TimeoutError:
                    pass

        retries = [packet for packet in responses if _is_quic_v1_retry(packet)]
        assert len(retries) == 1, [packet[:8].hex() for packet in responses]

    def test_002_duplicate_initial_produces_at_most_two_retries(self, env):
        """Send the same Initial packet twice in quick succession. A stateless
        QUIC server may answer each received Initial with its own Retry, so one
        or two Retries is compliant; anything more indicates duplicate output."""
        authority = f"test1.{env.http_tld}"
        config = QuicConfiguration(
            is_client=True,
            alpn_protocols=H3_ALPN,
            verify_mode=ssl.CERT_NONE,
            server_name=authority,
        )
        quic = QuicConnection(configuration=config)
        now = time.monotonic()
        target = (env.http_addr, env.https_port)
        quic.connect(target, now=now)
        outgoing = quic.datagrams_to_send(now=now)
        assert len(outgoing) == 1
        initial, _ = outgoing[0]
        assert len(initial) >= 1200

        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.bind(("127.0.0.1", 0))
            sock.settimeout(0.1)
            # Two identical Initials, back to back.
            sock.sendto(initial, target)
            sock.sendto(initial, target)

            responses = []
            deadline = time.monotonic() + 0.75
            while time.monotonic() < deadline:
                try:
                    responses.append(sock.recv(65535))
                except TimeoutError:
                    pass

        retries = [packet for packet in responses if _is_quic_v1_retry(packet)]
        # One Retry per received Initial is compliant; more than that is not.
        assert len(retries) in (1, 2), [packet[:8].hex() for packet in responses]
