import pytest

from .env import H3Conf


class TestHead:
    """A HEAD response repeats the headers its GET would send and carries no body."""

    @pytest.fixture(autouse=True, scope="class")
    def _class_scope(self, env):
        H3Conf(env).add_vhost_test1().install()
        assert env.apache_restart() == 0

    def _head(self, env, url):
        return env.curl_get(url, options=[
            "--http3-only", "-k", "-I", "-o", "/dev/null", "-w", "%{http_code} %{size_download}",
        ])

    def test_001_head_sends_no_body(self, env):
        r = self._head(env, env.mkurl("https", "test1", "/index.html"))
        assert r.exit_code == 0, r.stderr
        assert r.stdout.strip() == "200 0"

    def test_002_head_still_reports_the_get_content_length(self, env):
        url = env.mkurl("https", "test1", "/index.html")
        get = env.curl_get(url, options=["--http3-only", "-k"])
        head = env.curl_get(url, options=["--http3-only", "-k", "-I"])
        assert get.exit_code == 0, get.stderr
        assert head.exit_code == 0, head.stderr
        assert f"content-length: {len(get.response['body'])}" in head.stdout.lower()

    def test_003_head_on_a_buffered_response(self, env):
        H3Conf(env).add_vhost_test1(h3_max_response_body_size=1048576).install()
        assert env.apache_restart() == 0
        r = self._head(env, env.mkurl("https", "test1", "/index.html"))
        assert r.exit_code == 0, r.stderr
        assert r.stdout.strip() == "200 0"
