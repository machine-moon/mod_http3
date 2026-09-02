import asyncio
import ssl
import time

import pytest

from .env import H3Conf

from aioquic.asyncio.client import connect
from aioquic.asyncio.protocol import QuicConnectionProtocol
from aioquic.h3.connection import H3_ALPN, H3Connection
from aioquic.h3.events import DataReceived, HeadersReceived
from aioquic.quic import events as quic_events
from aioquic.quic.configuration import QuicConfiguration

IDLE_TIMEOUT = 3


class _IdleClient(QuicConnectionProtocol):
    """Records when the server terminates the connection, and can issue requests on demand."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._http = H3Connection(self._quic)
        self.closed = asyncio.Event()
        self.close_error_code = None
        self._response_done = asyncio.Event()
        self.status = None

    def quic_event_received(self, event):
        if isinstance(event, quic_events.ConnectionTerminated):
            self.close_error_code = event.error_code
            self.closed.set()
            return
        for h3_event in self._http.handle_event(event):
            if isinstance(h3_event, HeadersReceived):
                for k, v in h3_event.headers:
                    if k == b":status":
                        self.status = v.decode()
                if h3_event.stream_ended:
                    self._response_done.set()
            elif isinstance(h3_event, DataReceived) and h3_event.stream_ended:
                self._response_done.set()

    async def get(self, authority, path, timeout=5.0):
        self._response_done.clear()
        self.status = None
        stream_id = self._quic.get_next_available_stream_id()
        self._http.send_headers(stream_id=stream_id, headers=[
            (b":method", b"GET"),
            (b":scheme", b"https"),
            (b":authority", authority.encode()),
            (b":path", path.encode()),
        ], end_stream=True)
        self.transmit()
        await asyncio.wait_for(self._response_done.wait(), timeout=timeout)
        return self.status


class TestIdleTimeout:
    """H3IdleTimeout closes a connection with no application progress, despite QUIC keepalive pings."""

    @pytest.fixture(autouse=True, scope="class")
    def _class_scope(self, env):
        H3Conf(env).add_vhost_test1(h3_idle_timeout=IDLE_TIMEOUT).install()
        assert env.apache_restart() == 0

    def _authority(self, env):
        return f"test1.{env.http_tld}"

    def _config(self, env):
        return QuicConfiguration(
            is_client=True,
            alpn_protocols=H3_ALPN,
            verify_mode=ssl.CERT_NONE,
            server_name=self._authority(env),
            idle_timeout=120.0,
        )

    def test_001_idle_connection_is_closed(self, env):
        """One request, then silence: the server must close well before its own 120s client idle timer."""
        authority = self._authority(env)

        async def run():
            async with connect(env.http_addr, env.https_port,
                               configuration=self._config(env),
                               create_protocol=_IdleClient) as client:
                assert await client.get(authority, "/index.html") == "200"
                start = time.monotonic()
                await asyncio.wait_for(client.closed.wait(), timeout=IDLE_TIMEOUT * 6)
                return time.monotonic() - start, client.close_error_code

        elapsed, error_code = asyncio.run(run())
        assert elapsed >= IDLE_TIMEOUT * 0.5, f"closed after {elapsed:.1f}s, before the timeout elapsed"
        assert elapsed < IDLE_TIMEOUT * 6, f"closed after {elapsed:.1f}s"
        assert error_code == 0, f"expected a clean close, got 0x{error_code:X}"

    def test_002_active_connection_survives_the_timeout(self, env):
        """Requests spaced under the timeout keep the connection alive past it."""
        authority = self._authority(env)

        async def run():
            async with connect(env.http_addr, env.https_port,
                               configuration=self._config(env),
                               create_protocol=_IdleClient) as client:
                deadline = time.monotonic() + IDLE_TIMEOUT * 2
                while time.monotonic() < deadline:
                    if client.closed.is_set():
                        return False
                    assert await client.get(authority, "/index.html") == "200"
                    await asyncio.sleep(IDLE_TIMEOUT / 3)
                return not client.closed.is_set()

        assert asyncio.run(run()), "an active connection must not be closed as idle"
