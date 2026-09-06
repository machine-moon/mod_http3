import asyncio
import os
import ssl
import subprocess

import pytest

from .env import H3Conf

from aioquic.asyncio.client import connect
from aioquic.asyncio.protocol import QuicConnectionProtocol
from aioquic.h3.connection import H3_ALPN, H3Connection
from aioquic.quic.configuration import QuicConfiguration

# RFC 9204 section 5, registered in RFC 9114 section 11.2.2.
SETTINGS_QPACK_MAX_TABLE_CAPACITY = 0x1
SETTINGS_QPACK_BLOCKED_STREAMS = 0x7


def _read_test_conf(env):
    return open(os.path.join(env.server_dir, "conf", "test.conf")).read()


class TestQpackAndWorkers:
    """QPACK dynamic table and worker pool sizing are configurable and serve correctly."""

    @pytest.fixture(autouse=True, scope="class")
    def _class_scope(self, env):
        H3Conf(env).add_vhost_test1().install()
        assert env.apache_restart() == 0

    def _get(self, env, path="/"):
        url = env.mkurl("https", "test1", path)
        return env.curl_get(url, options=["--http3-only", "-k"])

    def test_001_defaults_serve(self, env):
        r = self._get(env)
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response is not None
        assert r.response["status"] == 200
        assert r.response["protocol"] == "HTTP/3"

    def test_002_qpack_table_in_vhost(self, env):
        H3Conf(env).add_vhost_test1(h3_qpack_table_capacity=65536, h3_qpack_blocked_streams=32).install()
        assert env.apache_restart() == 0
        conf = _read_test_conf(env)
        assert "H3QpackTableCapacity 65536" in conf
        assert "H3QpackBlockedStreams 32" in conf

    def test_003_larger_qpack_table_serves(self, env):
        H3Conf(env).add_vhost_test1(h3_qpack_table_capacity=65536, h3_qpack_blocked_streams=32).install()
        assert env.apache_restart() == 0
        r = self._get(env)
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response["status"] == 200

    def test_004_qpack_table_can_be_disabled(self, env):
        H3Conf(env).add_vhost_test1(h3_qpack_table_capacity=0, h3_qpack_blocked_streams=0).install()
        assert env.apache_restart() == 0
        assert "H3QpackTableCapacity 0" in _read_test_conf(env)
        r = self._get(env)
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response["status"] == 200

    def test_005_repeated_requests_with_dynamic_table(self, env):
        H3Conf(env).add_vhost_test1(h3_qpack_table_capacity=4096).install()
        assert env.apache_restart() == 0
        url = env.mkurl("https", "test1", "/")
        for _ in range(5):
            r = env.curl_get(url, options=["--http3-only", "-k", "-H", "Cookie: a=" + "x" * 200])
            assert r.exit_code == 0, r.stderr + r.stdout
            assert r.response["status"] == 200

    def test_006_qpack_invalid_values(self, env):
        H3Conf(env).add_vhost_test1(h3_qpack_table_capacity="invalid").install()
        assert env.apache_restart() != 0
        H3Conf(env).add_vhost_test1(h3_qpack_table_capacity=1048577).install()
        assert env.apache_restart() != 0
        H3Conf(env).add_vhost_test1(h3_qpack_blocked_streams=1001).install()
        assert env.apache_restart() != 0

    def test_007_worker_directives_in_vhost(self, env):
        H3Conf(env).add_vhost_test1(h3_min_workers=4, h3_max_workers=8, h3_max_worker_idle_seconds=30).install()
        assert env.apache_restart() == 0
        conf = _read_test_conf(env)
        assert "H3MinWorkers 4" in conf
        assert "H3MaxWorkers 8" in conf
        assert "H3MaxWorkerIdleSeconds 30" in conf

    def test_008_small_worker_pool_still_serves_concurrent_requests(self, env):
        from concurrent.futures import ThreadPoolExecutor

        H3Conf(env).add_vhost_test1(h3_min_workers=2, h3_max_workers=4).install()
        assert env.apache_restart() == 0

        url = env.mkurl("https", "test1", "/")

        def fetch(_):
            return env.curl_get(url, options=["--http3-only", "-k"])

        with ThreadPoolExecutor(max_workers=8) as pool:
            results = list(pool.map(fetch, range(16)))
        for r in results:
            assert r.exit_code == 0, r.stderr + r.stdout
            assert r.response["status"] == 200

    def test_009_worker_invalid_values(self, env):
        H3Conf(env).add_vhost_test1(h3_min_workers=0).install()
        assert env.apache_restart() != 0
        H3Conf(env).add_vhost_test1(h3_max_workers="invalid").install()
        assert env.apache_restart() != 0
        H3Conf(env).add_vhost_test1(h3_max_worker_idle_seconds=86401).install()
        assert env.apache_restart() != 0

    def test_010_max_below_min_is_corrected_not_fatal(self, env):
        H3Conf(env).add_vhost_test1(h3_min_workers=8, h3_max_workers=2).install()
        assert env.apache_restart() == 0
        r = self._get(env)
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response["status"] == 200


def _server_settings(env):
    """Return the SETTINGS the server advertises on a fresh connection."""
    authority = f"test1.{env.http_tld}"

    class _C(QuicConnectionProtocol):
        def __init__(self, *a, **kw):
            super().__init__(*a, **kw)
            self.http = H3Connection(self._quic)

        def quic_event_received(self, event):
            self.http.handle_event(event)

    async def run():
        config = QuicConfiguration(is_client=True, alpn_protocols=H3_ALPN,
                                   verify_mode=ssl.CERT_NONE, server_name=authority)
        async with connect(env.http_addr, env.https_port, configuration=config,
                           create_protocol=_C) as client:
            sid = client._quic.get_next_available_stream_id()
            client.http.send_headers(stream_id=sid, headers=[
                (b":method", b"GET"), (b":scheme", b"https"),
                (b":authority", authority.encode()), (b":path", b"/index.html"),
            ], end_stream=True)
            client.transmit()
            for _ in range(50):
                if client.http.received_settings:
                    break
                await asyncio.sleep(0.05)
            return client.http.received_settings

    return asyncio.run(run())


def _httpd_thread_count(env):
    """Threads across the test server's own children, read from /proc; None if unavailable."""
    pid_file = os.path.join(env.server_dir, "logs", "httpd.pid")
    if not os.path.isdir("/proc") or not os.path.isfile(pid_file):
        return None
    try:
        parent = int(open(pid_file).read().strip())
    except (OSError, ValueError):
        return None
    total = 0
    found = False
    for entry in os.listdir("/proc"):
        if not entry.isdigit():
            continue
        try:
            with open(f"/proc/{entry}/stat") as fd:
                ppid = int(fd.read().rsplit(")", 1)[1].split()[1])
            if int(entry) != parent and ppid != parent:
                continue
            total += len(os.listdir(f"/proc/{entry}/task"))
            found = True
        except (OSError, ValueError, IndexError):
            continue
    return total if found else None


class TestTuningTakesEffect:
    """The QPACK settings reach the wire and the worker pool is really sized by the directives."""

    def test_001_default_qpack_settings_are_advertised(self, env):
        H3Conf(env).add_vhost_test1().install()
        assert env.apache_restart() == 0
        settings = _server_settings(env)
        assert settings is not None, "no SETTINGS frame received"
        assert settings.get(SETTINGS_QPACK_MAX_TABLE_CAPACITY) == 4096, settings
        assert settings.get(SETTINGS_QPACK_BLOCKED_STREAMS, 0) == 0, settings

    def test_002_configured_qpack_settings_are_advertised(self, env):
        H3Conf(env).add_vhost_test1(h3_qpack_table_capacity=65536,
                                    h3_qpack_blocked_streams=32).install()
        assert env.apache_restart() == 0
        settings = _server_settings(env)
        assert settings is not None, "no SETTINGS frame received"
        assert settings.get(SETTINGS_QPACK_MAX_TABLE_CAPACITY) == 65536, settings
        assert settings.get(SETTINGS_QPACK_BLOCKED_STREAMS) == 32, settings

    def test_003_zero_capacity_is_advertised_as_zero(self, env):
        H3Conf(env).add_vhost_test1(h3_qpack_table_capacity=0,
                                    h3_qpack_blocked_streams=0).install()
        assert env.apache_restart() == 0
        settings = _server_settings(env)
        assert settings is not None, "no SETTINGS frame received"
        assert settings.get(SETTINGS_QPACK_MAX_TABLE_CAPACITY, 0) == 0, settings
        assert settings.get(SETTINGS_QPACK_BLOCKED_STREAMS, 0) == 0, settings

    def test_004_min_workers_sizes_the_pool(self, env):
        H3Conf(env).add_vhost_test1(h3_min_workers=16).install()
        assert env.apache_restart() == 0
        assert self._serves(env)
        low = _httpd_thread_count(env)

        H3Conf(env).add_vhost_test1(h3_min_workers=48).install()
        assert env.apache_restart() == 0
        assert self._serves(env)
        high = _httpd_thread_count(env)

        if low is None or high is None:
            pytest.skip("thread counts not observable here")
        assert high - low >= 24, f"{low} threads at 16 workers, {high} at 48"

    def _serves(self, env):
        r = env.curl_get(env.mkurl("https", "test1", "/"), options=["--http3-only", "-k"])
        return r.exit_code == 0 and r.response["status"] == 200

    def test_005_one_qpack_directive_does_not_zero_the_other(self, env):
        """Each QPACK directive defaults independently; setting one must not disable the other."""
        H3Conf(env).add_vhost_test1(h3_qpack_table_capacity=65536).install()
        assert env.apache_restart() == 0
        settings = _server_settings(env)
        assert settings is not None, "no SETTINGS frame received"
        assert settings.get(SETTINGS_QPACK_MAX_TABLE_CAPACITY) == 65536, settings
        assert settings.get(SETTINGS_QPACK_BLOCKED_STREAMS, 0) == 0, settings

        H3Conf(env).add_vhost_test1(h3_qpack_blocked_streams=8).install()
        assert env.apache_restart() == 0
        settings = _server_settings(env)
        assert settings is not None, "no SETTINGS frame received"
        assert settings.get(SETTINGS_QPACK_BLOCKED_STREAMS) == 8, settings
        assert settings.get(SETTINGS_QPACK_MAX_TABLE_CAPACITY) == 4096, (
            "H3QpackBlockedStreams alone must not disable the dynamic table", settings)

    def test_006_a_qpack_blocked_stream_is_refused_by_default(self, env):
        """With H3QpackBlockedStreams 0 the server must refuse to sit on a blocked stream."""
        import asyncio
        import ssl

        from aioquic.asyncio.client import connect
        from aioquic.asyncio.protocol import QuicConnectionProtocol
        from aioquic.buffer import encode_uint_var
        from aioquic.h3.connection import H3_ALPN, H3Connection
        from aioquic.quic import events as quic_events
        from aioquic.quic.configuration import QuicConfiguration

        H3Conf(env).add_vhost_test1().install()
        assert env.apache_restart() == 0
        authority = f"test1.{env.http_tld}"

        class _Blocked(QuicConnectionProtocol):
            def __init__(self, *a, **kw):
                super().__init__(*a, **kw)
                self._http = H3Connection(self._quic)
                self.terminated = asyncio.Event()

            def quic_event_received(self, event):
                if isinstance(event, quic_events.ConnectionTerminated):
                    self.terminated.set()

            def send_blocked_headers(self):
                sid = self._quic.get_next_available_stream_id(is_unidirectional=False)
                payload = bytes([0x02, 0x00, 0x80])
                self._quic.send_stream_data(
                    sid, encode_uint_var(0x1) + encode_uint_var(len(payload)) + payload)
                self.transmit()
                return sid

            def send_data(self, sid, chunk):
                self._quic.send_stream_data(
                    sid, encode_uint_var(0x0) + encode_uint_var(len(chunk)) + chunk)
                self.transmit()

        async def run():
            config = QuicConfiguration(is_client=True, alpn_protocols=H3_ALPN,
                                       verify_mode=ssl.CERT_NONE, server_name=authority,
                                       max_data=1 << 28, max_stream_data=1 << 28,
                                       idle_timeout=60.0)
            async with connect(env.http_addr, env.https_port, configuration=config,
                               create_protocol=_Blocked) as client:
                await asyncio.sleep(0.3)
                sid = client.send_blocked_headers()
                for _ in range(40):
                    if client.terminated.is_set():
                        break
                    client.send_data(sid, b"z" * 60000)
                    await asyncio.sleep(0.05)
                try:
                    await asyncio.wait_for(client.terminated.wait(), timeout=5.0)
                except asyncio.TimeoutError:
                    pass
                return client.terminated.is_set()

        assert asyncio.run(run()), (
            "the server accepted a QPACK-blocked stream; nghttp3 buffers those without bound"
        )
