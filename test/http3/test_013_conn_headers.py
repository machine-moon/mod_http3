import pytest


class TestConnectionHeaders:
    """Connection-specific response headers are forbidden in HTTP/3 field
    sections (RFC 9114 §4.2); clients hard-fail streams that carry them.
    mod_http3 must strip them no matter what handlers or config set."""

    @pytest.fixture(autouse=True, scope="class")
    def _class_scope(self, env):
        from .env import H3Conf

        H3Conf(env).add_vhost_test1(extra_lines=[
            "<Location /index.html>",
            "  Header always set Connection close",
            "  Header always set Keep-Alive \"timeout=5, max=100\"",
            "  Header always set X-Hop-Nominated \"by-connection\"",
            "  Header always merge Connection X-Hop-Nominated",
            "</Location>",
        ]).install()
        assert env.apache_restart() == 0

    def test_001_connection_headers_stripped(self, env):
        url = env.mkurl("https", "test1", "/index.html")
        r = env.curl_get(url, options=["--http3-only", "-k"])
        # Before the fix this died client-side with ERR_MALFORMED_HTTP_HEADER.
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response is not None
        assert r.response["status"] == 200
        headers = r.response["header"]
        assert "connection" not in headers, headers
        assert "keep-alive" not in headers, headers
        # A header nominated by Connection is hop-by-hop too.
        assert "x-hop-nominated" not in headers, headers

    def test_002_normal_headers_survive(self, env):
        url = env.mkurl("https", "test1", "/index.html")
        r = env.curl_get(url, options=["--http3-only", "-k"])
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response["status"] == 200
        assert "content-type" in r.response["header"]
