import os

import pytest


def _read_test_conf(env):
    return open(os.path.join(env.server_dir, "conf", "test.conf")).read()


class TestSocketBuffer:
    """H3SocketBufferSize sizes the QUIC socket buffers and never blocks startup."""

    @pytest.fixture(autouse=True, scope="class")
    def _class_scope(self, env):
        from .env import H3Conf

        H3Conf(env).add_vhost_test1().install()
        assert env.apache_restart() == 0

    def test_001_default_serves_requests(self, env):
        # The default buffer request must not stop the socket from being usable.
        url = env.mkurl("https", "test1", "/")
        r = env.curl_get(url, options=["--http3-only", "-k"])
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response is not None
        assert r.response["status"] == 200
        assert r.response["protocol"] == "HTTP/3"

    def test_002_explicit_size_in_vhost(self, env):
        from .env import H3Conf

        H3Conf(env).add_vhost_test1(h3_socket_buffer_size=4194304).install()
        assert env.apache_restart() == 0
        assert "H3SocketBufferSize 4194304" in _read_test_conf(env)

    def test_003_explicit_size_serves_requests(self, env):
        from .env import H3Conf

        H3Conf(env).add_vhost_test1(h3_socket_buffer_size=4194304).install()
        assert env.apache_restart() == 0
        url = env.mkurl("https", "test1", "/")
        r = env.curl_get(url, options=["--http3-only", "-k"])
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response is not None
        assert r.response["status"] == 200
        assert r.response["protocol"] == "HTTP/3"

    def test_004_size_the_os_will_cap_still_starts(self, env):
        # The OS caps SO_RCVBUF at rmem_max; a capped grant is logged, not fatal.
        from .env import H3Conf

        H3Conf(env).add_vhost_test1(h3_socket_buffer_size=67108864).install()
        assert env.apache_restart() == 0
        url = env.mkurl("https", "test1", "/")
        r = env.curl_get(url, options=["--http3-only", "-k"])
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response["status"] == 200

    def test_005_invalid_value(self, env):
        from .env import H3Conf

        H3Conf(env).add_vhost_test1(h3_socket_buffer_size="invalid").install()
        assert env.apache_restart() != 0
        H3Conf(env).add_vhost_test1(h3_socket_buffer_size=0).install()
        assert env.apache_restart() != 0
        H3Conf(env).add_vhost_test1(h3_socket_buffer_size=67108865).install()
        assert env.apache_restart() != 0

    def test_006_many_concurrent_requests(self, env):
        # A batched read path must deliver every datagram of a burst, not just
        # the first of each batch.
        from concurrent.futures import ThreadPoolExecutor

        from .env import H3Conf

        H3Conf(env).add_vhost_test1(h3_socket_buffer_size=4194304).install()
        assert env.apache_restart() == 0

        url = env.mkurl("https", "test1", "/")

        def fetch(_):
            return env.curl_get(url, options=["--http3-only", "-k"])

        with ThreadPoolExecutor(max_workers=8) as pool:
            results = list(pool.map(fetch, range(24)))

        for r in results:
            assert r.exit_code == 0, r.stderr + r.stdout
            assert r.response is not None
            assert r.response["status"] == 200
            assert r.response["protocol"] == "HTTP/3"
