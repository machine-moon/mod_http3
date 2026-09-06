import asyncio
import os
import re
import ssl

import pytest

from aioquic.asyncio.client import connect
from aioquic.asyncio.protocol import QuicConnectionProtocol
from aioquic.h3.connection import H3_ALPN, H3Connection
from aioquic.quic import events as quic_events
from aioquic.quic.configuration import QuicConfiguration

from .env import H3Conf
from .test_020_malformed import _RawRequestClient

# RFC 9114 section 8.1.
H3_EXCESSIVE_LOAD = 0x0107


def _read_test_conf(env):
    return open(os.path.join(env.server_dir, "conf", "test.conf")).read()


class _TerminationWatchingClient(_RawRequestClient):
    """Raw client that also records the connection being closed by the server."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.terminated = None
        self.terminated_event = asyncio.Event()

    def quic_event_received(self, event):
        if isinstance(event, quic_events.ConnectionTerminated):
            self.terminated = event
            self.terminated_event.set()
            return
        super().quic_event_received(event)


class _StallingClient(QuicConnectionProtocol):
    """Requests a large file and never opens its flow-control window again."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._http = H3Connection(self._quic)
        self.received = 0

    def starve(self):
        self._quic._write_connection_limits = lambda builder, space: None
        self._quic._write_stream_limits = lambda builder, space, stream: None

    def request(self, authority, path):
        stream_id = self._quic.get_next_available_stream_id()
        self._http.send_headers(stream_id=stream_id, headers=[
            (b":method", b"GET"), (b":scheme", b"https"),
            (b":authority", authority.encode()), (b":path", path.encode()),
        ], end_stream=True)
        self.transmit()

    def quic_event_received(self, event):
        if isinstance(event, quic_events.StreamDataReceived):
            self.received += len(event.data)


class TestStreamLimits:
    """H3StreamTimeout bounds a stalled response; H3MaxStreamErrors bounds an abusive client."""

    @pytest.fixture(autouse=True, scope="class")
    def _class_scope(self, env):
        H3Conf(env).add_vhost_test1().install()
        assert env.apache_restart() == 0
        yield
        for name in ("timeout-big.bin", "stall-big.bin"):
            try:
                os.unlink(os.path.join(env.server_docs_dir, name))
            except OSError:
                pass

    def test_001_defaults_serve_normally(self, env):
        url = env.mkurl("https", "test1", "/")
        r = env.curl_get(url, options=["--http3-only", "-k"])
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response is not None
        assert r.response["status"] == 200
        assert r.response["protocol"] == "HTTP/3"

    def test_002_stream_timeout_in_vhost(self, env):
        H3Conf(env).add_vhost_test1(h3_stream_timeout=30).install()
        assert env.apache_restart() == 0
        assert "H3StreamTimeout 30" in _read_test_conf(env)

    def test_003_stream_timeout_does_not_break_normal_responses(self, env):
        H3Conf(env).add_vhost_test1(h3_stream_timeout=30).install()
        assert env.apache_restart() == 0
        url = env.mkurl("https", "test1", "/")
        r = env.curl_get(url, options=["--http3-only", "-k"])
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response["status"] == 200

    def test_004_large_download_under_timeout(self, env):
        fpath = os.path.join(env.server_docs_dir, "timeout-big.bin")
        with open(fpath, "wb") as fd:
            fd.write(b"x" * (2 * 1024 * 1024))

        H3Conf(env).add_vhost_test1(h3_stream_timeout=10).install()
        assert env.apache_restart() == 0
        url = env.mkurl("https", "test1", "/timeout-big.bin")
        r = env.curl_get(url, options=["--http3-only", "-k"])
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response["status"] == 200
        assert len(r.response["body"]) == 2 * 1024 * 1024

    def test_005_stream_timeout_invalid_value(self, env):
        H3Conf(env).add_vhost_test1(h3_stream_timeout="invalid").install()
        assert env.apache_restart() != 0
        H3Conf(env).add_vhost_test1(h3_stream_timeout=0).install()
        assert env.apache_restart() != 0
        H3Conf(env).add_vhost_test1(h3_stream_timeout=86401).install()
        assert env.apache_restart() != 0

    def test_006_max_stream_errors_in_vhost(self, env):
        H3Conf(env).add_vhost_test1(h3_max_stream_errors=3).install()
        assert env.apache_restart() == 0
        assert "H3MaxStreamErrors 3" in _read_test_conf(env)

    def test_007_max_stream_errors_invalid_value(self, env):
        H3Conf(env).add_vhost_test1(h3_max_stream_errors="invalid").install()
        assert env.apache_restart() != 0
        H3Conf(env).add_vhost_test1(h3_max_stream_errors=0).install()
        assert env.apache_restart() != 0
        H3Conf(env).add_vhost_test1(h3_max_stream_errors=10001).install()
        assert env.apache_restart() != 0

    def test_008_repeated_malformed_requests_close_the_connection(self, env):
        H3Conf(env).add_vhost_test1(h3_max_stream_errors=2).install()
        assert env.apache_restart() == 0

        authority = f"test1.{env.http_tld}"
        # No :method, which RFC 9114 4.1.2 makes malformed.
        malformed = [
            (b":scheme", b"https"),
            (b":authority", authority.encode()),
            (b":path", b"/index.html"),
        ]

        async def run():
            config = QuicConfiguration(
                is_client=True,
                alpn_protocols=H3_ALPN,
                verify_mode=ssl.CERT_NONE,
                server_name=authority,
            )
            async with connect(
                env.http_addr,
                env.https_port,
                configuration=config,
                create_protocol=_TerminationWatchingClient,
            ) as client:
                resets = 0
                for _ in range(6):
                    if client.terminated is not None:
                        break
                    client._reset_event.clear()
                    try:
                        if await client.send_raw_request(malformed, timeout=5.0) is not None:
                            resets += 1
                    except asyncio.TimeoutError:
                        break
                try:
                    await asyncio.wait_for(client.terminated_event.wait(), timeout=5.0)
                except asyncio.TimeoutError:
                    pass
                return resets, client.terminated

        resets, terminated = asyncio.run(run())
        assert terminated is not None, f"connection still open after {resets} stream errors"
        assert terminated.error_code == H3_EXCESSIVE_LOAD, hex(terminated.error_code)

        pattern = re.compile(r".*client-caused stream errors.*")
        assert env.httpd_error_log.scan_recent(pattern, timeout=10), (
            "expected a log line naming H3MaxStreamErrors"
        )

    def test_009_a_client_that_stops_reading_is_abandoned(self, env):
        """H3StreamTimeout must release the worker while the client is still holding the connection."""
        fpath = os.path.join(env.server_docs_dir, "stall-big.bin")
        with open(fpath, "wb") as fd:
            fd.write(b"y" * (8 * 1024 * 1024))

        H3Conf(env).add_vhost_test1(h3_stream_timeout=2, h3_stream_buffer_size=16384).install()
        assert env.apache_restart() == 0

        log_path = env.httpd_error_log.path
        log_start = os.path.getsize(log_path) if os.path.isfile(log_path) else 0
        authority = f"test1.{env.http_tld}"

        async def run():
            config = QuicConfiguration(
                is_client=True, alpn_protocols=H3_ALPN, verify_mode=ssl.CERT_NONE,
                server_name=authority, max_data=16384, max_stream_data=16384,
                idle_timeout=120.0,
            )
            async with connect(env.http_addr, env.https_port, configuration=config,
                               create_protocol=_StallingClient) as client:
                client.starve()
                client.request(authority, "/stall-big.bin")
                for _ in range(24):
                    await asyncio.sleep(0.5)
                    with open(log_path) as fd:
                        fd.seek(log_start)
                        if "made no progress" in fd.read():
                            break
                return client.received

        received = asyncio.run(run())
        assert received < 8 * 1024 * 1024, "the client was supposed to stall, not drain the file"
        with open(log_path) as fd:
            fd.seek(log_start)
            logged = fd.read()
        if "made no progress" not in logged and "QUIC connection terminated" in logged:
            pytest.skip("the starved connection collapsed before the response queue filled")
        assert "made no progress" in logged, (
            "H3StreamTimeout did not abandon the stalled response")
