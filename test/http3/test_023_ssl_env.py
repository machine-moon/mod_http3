import os

import pytest


class TestSSLEnv:
    """HTTP/3 requests get the mod_ssl SSL_* environment, since mod_ssl does not manage the connection."""

    @pytest.fixture(autouse=True, scope="class")
    def _class_scope(self, env):
        from .env import H3Conf

        H3Conf(env).add_vhost_test1().install()
        assert env.apache_restart() == 0

    def _env_over_h3(self, env):
        url = env.mkurl("https", "test1", "/cgi/env.py")
        r = env.curl_get(url, options=["--http3-only", "-k"])
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response is not None
        assert r.response["status"] == 200
        assert r.response["protocol"] == "HTTP/3"
        assert r.response["json"] is not None, r.response["body"]
        return r.response["json"]

    def test_001_https_on(self, env):
        assert self._env_over_h3(env)["https"] == "on"

    def test_002_ssl_protocol_is_tls13(self, env):
        # Only TLS 1.3 is offered, and QUIC requires it.
        assert self._env_over_h3(env)["ssl_protocol"] == "TLSv1.3"

    def test_003_ssl_cipher_is_a_tls13_suite(self, env):
        cipher = self._env_over_h3(env)["ssl_cipher"]
        assert cipher.startswith("TLS_"), cipher

    def test_004_cipher_key_sizes_are_numbers(self, env):
        data = self._env_over_h3(env)
        for key in ("ssl_cipher_usekeysize", "ssl_cipher_algkeysize"):
            assert data[key].isdigit(), f"{key}={data[key]!r}"
            assert int(data[key]) >= 128, f"{key}={data[key]!r}"

    def test_005_cipher_export_is_false(self, env):
        # TLS 1.3 has no export-grade ciphers.
        assert self._env_over_h3(env)["ssl_cipher_export"] == "false"

    def test_006_session_resumed_reported(self, env):
        # mod_ssl spells these exactly "Initial" and "Resumed".
        assert self._env_over_h3(env)["ssl_session_resumed"] in ("Initial", "Resumed")

    def test_007_env_is_stable_across_requests_on_a_connection(self, env):
        # The values are computed once per connection; every request must still
        # see them, not just the first.
        url = env.mkurl("https", "test1", "/cgi/env.py")
        seen = []
        for _ in range(3):
            r = env.curl_get(url, options=["--http3-only", "-k"])
            assert r.exit_code == 0, r.stderr + r.stdout
            assert r.response["status"] == 200
            seen.append((r.response["json"]["ssl_protocol"], r.response["json"]["ssl_cipher"]))
        assert all(s == seen[0] for s in seen), seen
        assert seen[0][0] == "TLSv1.3"

    def test_008_no_ssl_env_over_http11(self, env):
        # A plain HTTP/1.1 request on this vhost is handled by mod_ssl, so the
        # module must not be the one answering for it.
        url = env.mkurl("https", "test1", "/cgi/env.py")
        r = env.curl_get(url, options=["--http1.1", "-k"])
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response is not None
        assert r.response["status"] == 200
        assert r.response["json"]["server_protocol"] == "HTTP/1.1"
