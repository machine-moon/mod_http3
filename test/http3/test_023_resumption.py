import asyncio
import os
import ssl

import pytest

from .env import H3Conf

from aioquic.asyncio.client import connect
from aioquic.asyncio.protocol import QuicConnectionProtocol
from aioquic.h3.connection import H3_ALPN
from aioquic.quic import events as quic_events
from aioquic.quic.configuration import QuicConfiguration


def _read_test_conf(env):
    return open(os.path.join(env.server_dir, "conf", "test.conf")).read()


class _TicketClient(QuicConnectionProtocol):
    """Captures the session ticket the server issues and whether a handshake resumed."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.session_resumed = None

    def quic_event_received(self, event):
        if isinstance(event, quic_events.HandshakeCompleted):
            self.session_resumed = event.session_resumed


def _handshake(env, ticket=None):
    """Complete one QUIC handshake; return (session_resumed, ticket_the_server_issued)."""
    authority = f"test1.{env.http_tld}"
    issued = []

    async def run():
        config = QuicConfiguration(
            is_client=True,
            alpn_protocols=H3_ALPN,
            verify_mode=ssl.CERT_NONE,
            server_name=authority,
            session_ticket=ticket,
        )
        config.session_ticket_handler = issued.append
        async with connect(env.http_addr, env.https_port,
                           configuration=config,
                           create_protocol=_TicketClient,
                           session_ticket_handler=issued.append) as client:
            await client.ping()
            return client.session_resumed

    resumed = asyncio.run(run())
    return resumed, (issued[0] if issued else None)


class TestSessionResumption:
    """H3SessionTickets controls TLS ticket issuance and so whether a returning client can resume."""

    @pytest.fixture(autouse=True, scope="class")
    def _class_scope(self, env):
        H3Conf(env).add_vhost_test1().install()
        assert env.apache_restart() == 0

    def test_001_tickets_on_by_default_and_serve(self, env):
        url = env.mkurl("https", "test1", "/")
        r = env.curl_get(url, options=["--http3-only", "-k"])
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response is not None
        assert r.response["status"] == 200
        assert r.response["protocol"] == "HTTP/3"

    def test_002_tickets_off_in_vhost(self, env):
        H3Conf(env).add_vhost_test1(h3_session_tickets=False).install()
        assert env.apache_restart() == 0
        assert "H3SessionTickets off" in _read_test_conf(env)

    def test_003_requests_still_work_without_tickets(self, env):
        H3Conf(env).add_vhost_test1(h3_session_tickets=False).install()
        assert env.apache_restart() == 0
        url = env.mkurl("https", "test1", "/")
        for _ in range(2):
            r = env.curl_get(url, options=["--http3-only", "-k"])
            assert r.exit_code == 0, r.stderr + r.stdout
            assert r.response["status"] == 200
            assert r.response["protocol"] == "HTTP/3"

    def test_004_tickets_on_explicitly(self, env):
        H3Conf(env).add_vhost_test1(h3_session_tickets=True).install()
        assert env.apache_restart() == 0
        assert "H3SessionTickets on" in _read_test_conf(env)
        url = env.mkurl("https", "test1", "/")
        r = env.curl_get(url, options=["--http3-only", "-k"])
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response["status"] == 200

    def test_005_a_ticket_actually_resumes(self, env):
        """With tickets on, a second handshake presenting the ticket must resume."""
        H3Conf(env).add_vhost_test1(h3_session_tickets=True).install()
        assert env.apache_restart() == 0
        resumed, ticket = _handshake(env)
        assert resumed is False, "the first handshake cannot be a resumption"
        assert ticket is not None, "H3SessionTickets on must issue a ticket"
        resumed, _ = _handshake(env, ticket=ticket)
        assert resumed is True, "presenting the issued ticket must resume the session"

    def test_006_tickets_off_issues_none(self, env):
        """With tickets off the server issues none, so a client cannot resume."""
        H3Conf(env).add_vhost_test1(h3_session_tickets=False).install()
        assert env.apache_restart() == 0
        resumed, ticket = _handshake(env)
        assert resumed is False
        assert ticket is None, "H3SessionTickets off must issue no ticket"

    def test_007_ticket_never_advertises_early_data(self, env):
        """OpenSSL's QUIC server drops 0-RTT packets, so our tickets must never invite them."""
        H3Conf(env).add_vhost_test1(h3_session_tickets=True).install()
        assert env.apache_restart() == 0
        _, ticket = _handshake(env)
        assert ticket is not None
        assert not getattr(ticket, "max_early_data_size", 0), (
            "the ticket advertises 0-RTT the server cannot honour; clients will send "
            "early data it discards, and an unclamped value fails RFC 9001 4.6.1")
