import pytest


class TestVhosts:
    """Name-based virtual hosts must be selected from :authority over HTTP/3,
    not pinned to the H3-enabled base vhost."""

    @pytest.fixture(autouse=True, scope="class")
    def _class_scope(self, env):
        from .env import H3Conf

        conf = H3Conf(env)
        conf.add_vhost_test1()
        conf.start_vhost([f"test2.{env.http_tld}"], doc_root="htdocs/two", with_ssl=True)
        conf.add("Protocols h3 http/1.1")
        conf.end_vhost()
        conf.install()
        assert env.apache_restart() == 0

    def test_001_second_vhost_content_over_h3(self, env):
        url = env.mkurl("https", "test2", "/index.html")
        r = env.curl_get(url, options=["--http3-only", "-k"])
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response is not None
        assert r.response["status"] == 200
        assert b"VHOST-TWO-CONTENT" in r.response["body"]

    def test_002_first_vhost_unaffected_over_h3(self, env):
        url = env.mkurl("https", "test1", "/index.html")
        r = env.curl_get(url, options=["--http3-only", "-k"])
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response is not None
        assert r.response["status"] == 200
        assert b"VHOST-TWO-CONTENT" not in r.response["body"]
