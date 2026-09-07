import os

import pytest

from .env import H3Conf


class TestErrorDocument:
    """An error status on a vhost with ErrorDocument must not take the child down."""

    @pytest.fixture(autouse=True, scope="class")
    def _class_scope(self, env):
        H3Conf(env).add_vhost_test1(extra_lines=["ErrorDocument 404 /index.html"]).install()
        assert env.apache_restart() == 0

    def test_001_error_status_does_not_kill_the_child(self, env):
        log = env.httpd_error_log.path
        start = os.path.getsize(log) if os.path.isfile(log) else 0
        r = env.curl_get(env.mkurl("https", "test1", "/no-such-path"), options=["--http3-only", "-k"])
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response is not None
        assert r.response["status"] == 404
        with open(log) as fd:
            fd.seek(start)
            assert "Segmentation fault" not in fd.read(), "child died serving an ErrorDocument"

    def test_002_matches_http11(self, env):
        url = env.mkurl("https", "test1", "/no-such-path")
        h3 = env.curl_get(url, options=["--http3-only", "-k"])
        h1 = env.curl_get(url, options=["--http1.1", "-k"])
        assert h3.response is not None and h1.response is not None
        assert h3.response["status"] == h1.response["status"]
        assert h3.response["body"] == h1.response["body"]
