"""Malformed HTTP/3 requests are stream errors, not connection errors.

RFC 9114 4.1.2: a request that violates the header field rules of 4.x must be
rejected with a stream error of type H3_MESSAGE_ERROR while the connection
keeps serving other streams. These tests send deliberately malformed requests
(raw QPACK-encoded, bypassing client-side validation) and then verify the same
connection still answers a well-formed request.
"""

import asyncio
import ssl

import pytest

from .env import H3Conf

from aioquic.asyncio.client import connect
from aioquic.asyncio.protocol import QuicConnectionProtocol
from aioquic.buffer import encode_uint_var
from aioquic.h3.connection import H3_ALPN, H3Connection
from aioquic.h3.events import DataReceived, HeadersReceived
from aioquic.quic import events as quic_events
from aioquic.quic.configuration import QuicConfiguration

import pylsqpack

# RFC 9114 section 8.1.
H3_MESSAGE_ERROR = 0x010E
FRAME_TYPE_DATA = 0x0
FRAME_TYPE_HEADERS = 0x1


class _RawRequestClient(QuicConnectionProtocol):
    """HTTP/3 client that can send arbitrary, even malformed, header blocks."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._http = H3Connection(self._quic)
        self._encoder = pylsqpack.Encoder()
        self.reset_codes = {}
        self._reset_event = asyncio.Event()
        self.status = None
        self._response_done = asyncio.Event()

    def quic_event_received(self, event):
        if isinstance(event, quic_events.StreamReset):
            self.reset_codes[event.stream_id] = event.error_code
            self._reset_event.set()
            return
        for h3_event in self._http.handle_event(event):
            if isinstance(h3_event, HeadersReceived):
                for k, v in h3_event.headers:
                    if k == b":status":
                        self.status = v.decode()
                if h3_event.stream_ended:
                    self._response_done.set()
            elif isinstance(h3_event, DataReceived):
                if h3_event.stream_ended:
                    self._response_done.set()

    async def send_raw_request(self, headers, data=None, timeout=5.0):
        """Encode headers verbatim and return the stream reset code."""
        stream_id = self._quic.get_next_available_stream_id()
        _, payload = self._encoder.encode(stream_id, headers)
        frame = encode_uint_var(FRAME_TYPE_HEADERS) + encode_uint_var(len(payload)) + payload
        if data is not None:
            frame += encode_uint_var(FRAME_TYPE_DATA) + encode_uint_var(len(data)) + data
        self._quic.send_stream_data(stream_id, frame, end_stream=True)
        self.transmit()
        await asyncio.wait_for(self._reset_event.wait(), timeout=timeout)
        return self.reset_codes.get(stream_id)

    async def get(self, authority, path, headers=None, timeout=5.0):
        """Send a well-formed GET and return the response status."""
        stream_id = self._quic.get_next_available_stream_id()
        h = [
            (b":method", b"GET"),
            (b":scheme", b"https"),
            (b":authority", authority.encode()),
            (b":path", path.encode()),
        ]
        h += headers or []
        self._http.send_headers(stream_id=stream_id, headers=h, end_stream=True)
        self.transmit()
        await asyncio.wait_for(self._response_done.wait(), timeout=timeout)
        return self.status


class TestMalformedRequests:
    @pytest.fixture(autouse=True, scope="class")
    def _class_scope(self, env):
        H3Conf(env).add_vhost_test1().install()
        assert env.apache_restart() == 0

    def _authority(self, env):
        return f"test1.{env.http_tld}"

    def _reject_then_serve(self, env, malformed_headers, data=None):
        """Send a malformed request, then a valid one on the same connection.

        Returns (reset_code, follow_up_status).
        """
        authority = self._authority(env)

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
                create_protocol=_RawRequestClient,
            ) as client:
                code = await client.send_raw_request(malformed_headers, data=data)
                status = await client.get(authority, "/index.html")
                return code, status

        return asyncio.run(run())

    def _valid_headers(self, env):
        return [
            (b":method", b"GET"),
            (b":scheme", b"https"),
            (b":authority", self._authority(env).encode()),
            (b":path", b"/index.html"),
        ]

    def test_001_missing_method(self, env):
        headers = [h for h in self._valid_headers(env) if h[0] != b":method"]
        code, status = self._reject_then_serve(env, headers)
        assert code == H3_MESSAGE_ERROR, f"reset code 0x{code:X}"
        assert status == "200", "connection must survive the malformed stream"

    def test_002_missing_path(self, env):
        headers = [h for h in self._valid_headers(env) if h[0] != b":path"]
        code, status = self._reject_then_serve(env, headers)
        assert code == H3_MESSAGE_ERROR, f"reset code 0x{code:X}"
        assert status == "200", "connection must survive the malformed stream"

    def test_003_missing_scheme(self, env):
        headers = [h for h in self._valid_headers(env) if h[0] != b":scheme"]
        code, status = self._reject_then_serve(env, headers)
        assert code == H3_MESSAGE_ERROR, f"reset code 0x{code:X}"
        assert status == "200", "connection must survive the malformed stream"

    def test_004_missing_authority_and_host(self, env):
        headers = [h for h in self._valid_headers(env) if h[0] != b":authority"]
        code, status = self._reject_then_serve(env, headers)
        assert code == H3_MESSAGE_ERROR, f"reset code 0x{code:X}"
        assert status == "200", "connection must survive the malformed stream"

    def test_005_duplicate_pseudo_header(self, env):
        headers = self._valid_headers(env) + [(b":method", b"GET")]
        code, status = self._reject_then_serve(env, headers)
        assert code == H3_MESSAGE_ERROR, f"reset code 0x{code:X}"
        assert status == "200", "connection must survive the malformed stream"

    def test_006_connection_specific_header(self, env):
        headers = self._valid_headers(env) + [(b"connection", b"close")]
        code, status = self._reject_then_serve(env, headers)
        assert code == H3_MESSAGE_ERROR, f"reset code 0x{code:X}"
        assert status == "200", "connection must survive the malformed stream"

    def test_007_te_other_than_trailers(self, env):
        headers = self._valid_headers(env) + [(b"te", b"gzip")]
        code, status = self._reject_then_serve(env, headers)
        assert code == H3_MESSAGE_ERROR, f"reset code 0x{code:X}"
        assert status == "200", "connection must survive the malformed stream"

    def test_008_content_length_mismatch(self, env):
        headers = self._valid_headers(env) + [(b"content-length", b"10")]
        code, status = self._reject_then_serve(env, headers, data=b"abc")
        assert code == H3_MESSAGE_ERROR, f"reset code 0x{code:X}"
        assert status == "200", "connection must survive the malformed stream"

    def test_009_te_trailers_is_allowed(self, env):
        authority = self._authority(env)

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
                create_protocol=_RawRequestClient,
            ) as client:
                return await client.get(authority, "/index.html", headers=[(b"te", b"trailers")])

        status = asyncio.run(run())
        assert status == "200"
