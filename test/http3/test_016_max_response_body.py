import os

import pytest


class TestMaxResponseBody:
    """H3MaxResponseBodySize bounds in-memory buffering; an opt-in valve, unlimited by default."""

    @pytest.fixture(autouse=True, scope="class")
    def _class_scope(self, env):
        from .env import H3Conf

        with open(os.path.join(env.server_docs_dir, "capped.bin"), "wb") as fd:
            fd.write(os.urandom(2 * 1024 * 1024))

        H3Conf(env).add_vhost_test1(h3_max_response_body_size=1024 * 1024).install()
        assert env.apache_restart() == 0

    def test_001_oversized_response_becomes_500(self, env):
        url = env.mkurl("https", "test1", "/capped.bin")
        r = env.curl_get(url, options=["--http3-only", "-k"])
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response is not None
        assert r.response["status"] == 500

    def test_002_response_under_cap_unaffected(self, env):
        url = env.mkurl("https", "test1", "/index.html")
        r = env.curl_get(url, options=["--http3-only", "-k"])
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response is not None
        assert r.response["status"] == 200
