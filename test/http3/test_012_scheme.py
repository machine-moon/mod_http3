import pytest


class TestScheme:
    """HTTP/3 requests must be treated as TLS by httpd: https scheme in
    self-referential redirects and the standard TLS environment for scripts."""

    @pytest.fixture(autouse=True, scope="class")
    def _class_scope(self, env):
        from .env import H3Conf

        H3Conf(env).add_vhost_test1().install()
        assert env.apache_restart() == 0

    def test_001_redirect_keeps_https_scheme(self, env):
        # mod_dir issues a self-referential redirect for a directory
        # without a trailing slash; it must not downgrade to http://.
        url = env.mkurl("https", "test1", "/subdir")
        r = env.curl_get(url, options=["--http3-only", "-k"])
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response is not None
        assert r.response["status"] == 301
        location = r.response["header"]["location"]
        assert location.startswith("https://"), f"downgraded Location: {location}"

    def test_002_cgi_sees_tls_environment(self, env):
        import json

        url = env.mkurl("https", "test1", "/cgi/env.py")
        r = env.curl_get(url, options=["--http3-only", "-k"])
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response is not None
        assert r.response["status"] == 200
        payload = json.loads(r.response["body"])
        assert payload["request_scheme"] == "https", payload
        assert payload["https"] == "on", payload
