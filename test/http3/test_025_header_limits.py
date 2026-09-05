import pytest

from .env import H3Conf


class TestHeaderLimits:
    """The core LimitRequest* directives must bound HTTP/3 request headers too."""

    @pytest.fixture(autouse=True, scope="class")
    def _class_scope(self, env):
        H3Conf(env).add_vhost_test1().install()
        assert env.apache_restart() == 0

    def _get(self, env, headers):
        url = env.mkurl("https", "test1", "/")
        options = ["--http3-only", "-k"]
        for name, value in headers:
            options += ["-H", f"{name}: {value}"]
        return env.curl_get(url, options=options)

    def test_001_normal_headers_pass(self, env):
        r = self._get(env, [("X-Small", "value")])
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response is not None
        assert r.response["status"] == 200
        assert r.response["protocol"] == "HTTP/3"

    def test_002_field_over_limitrequestfieldsize_gets_431(self, env):
        H3Conf(env).add_vhost_test1(extra_lines=["LimitRequestFieldSize 1024"]).install()
        assert env.apache_restart() == 0
        r = self._get(env, [("X-Big", "z" * 4096)])
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response is not None
        assert r.response["status"] == 431, r.response["status"]

    def test_003_field_under_limit_still_passes(self, env):
        H3Conf(env).add_vhost_test1(extra_lines=["LimitRequestFieldSize 1024"]).install()
        assert env.apache_restart() == 0
        r = self._get(env, [("X-Ok", "z" * 512)])
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response["status"] == 200

    def test_004_too_many_fields_gets_431(self, env):
        H3Conf(env).add_vhost_test1(extra_lines=["LimitRequestFields 20"]).install()
        assert env.apache_restart() == 0
        r = self._get(env, [(f"X-H{i}", "v") for i in range(40)])
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response is not None
        assert r.response["status"] == 431, r.response["status"]

    def test_005_field_count_under_limit_passes(self, env):
        H3Conf(env).add_vhost_test1(extra_lines=["LimitRequestFields 40"]).install()
        assert env.apache_restart() == 0
        r = self._get(env, [(f"X-H{i}", "v") for i in range(10)])
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response["status"] == 200

    def test_006_server_keeps_serving_after_a_rejection(self, env):
        # A 431 is a per-request answer, so a later request must still be served.
        H3Conf(env).add_vhost_test1(extra_lines=["LimitRequestFieldSize 1024"]).install()
        assert env.apache_restart() == 0

        bad = self._get(env, [("X-Big", "z" * 4096)])
        assert bad.response["status"] == 431
        good = self._get(env, [("X-Small", "v")])
        assert good.exit_code == 0, good.stderr + good.stdout
        assert good.response["status"] == 200
        assert good.response["protocol"] == "HTTP/3"

    def test_007_settings_advertise_the_derived_field_section_size(self, env):
        """The server must tell a client the bound up front, derived from both core limits."""
        import asyncio
        import ssl

        from aioquic.asyncio.client import connect
        from aioquic.asyncio.protocol import QuicConnectionProtocol
        from aioquic.h3.connection import H3_ALPN, H3Connection
        from aioquic.quic.configuration import QuicConfiguration

        H3Conf(env).add_vhost_test1(
            extra_lines=["LimitRequestFields 20", "LimitRequestFieldSize 1024"]
        ).install()
        assert env.apache_restart() == 0

        authority = f"test1.{env.http_tld}"

        class _SettingsClient(QuicConnectionProtocol):
            def __init__(self, *a, **kw):
                super().__init__(*a, **kw)
                self.http = H3Connection(self._quic)

            def quic_event_received(self, event):
                self.http.handle_event(event)

        async def run():
            config = QuicConfiguration(is_client=True, alpn_protocols=H3_ALPN,
                                       verify_mode=ssl.CERT_NONE, server_name=authority)
            async with connect(env.http_addr, env.https_port, configuration=config,
                               create_protocol=_SettingsClient) as client:
                stream_id = client._quic.get_next_available_stream_id()
                client.http.send_headers(stream_id=stream_id, headers=[
                    (b":method", b"GET"), (b":scheme", b"https"),
                    (b":authority", authority.encode()), (b":path", b"/index.html"),
                ], end_stream=True)
                client.transmit()
                for _ in range(50):
                    if client.http.received_settings:
                        break
                    await asyncio.sleep(0.05)
                return client.http.received_settings

        settings = asyncio.run(run())
        assert settings is not None, "no SETTINGS frame received"
        # SETTINGS_MAX_FIELD_SECTION_SIZE, RFC 9114 section 7.2.4.1.
        assert settings.get(0x6) == 20 * (1024 + 32), settings


    def test_008_an_oversized_pseudo_header_is_rejected(self, env):
        """LimitRequestFieldSize must bound :path too, not just regular fields."""
        H3Conf(env).add_vhost_test1(extra_lines=["LimitRequestFieldSize 1024"]).install()
        assert env.apache_restart() == 0
        r = env.curl_get(env.mkurl("https", "test1", "/" + "a" * 4096), options=["--http3-only", "-k"])
        status = r.response["status"] if r.response else None
        assert status != 403, "the over-long :path was processed despite LimitRequestFieldSize"
        assert r.exit_code != 0 or status in (400, 431), f"exit={r.exit_code} status={status}"

    def test_009_a_path_within_the_limit_still_works(self, env):
        H3Conf(env).add_vhost_test1(extra_lines=["LimitRequestFieldSize 4096"]).install()
        assert env.apache_restart() == 0
        r = env.curl_get(env.mkurl("https", "test1", "/index.html"), options=["--http3-only", "-k"])
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response["status"] == 200
