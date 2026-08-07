import os

import pytest


def _read_test_conf(env):
    return open(os.path.join(env.server_dir, "conf", "test.conf")).read()


class TestQpackAndWorkers:
    """QPACK dynamic table and worker pool sizing are configurable and serve correctly."""

    @pytest.fixture(autouse=True, scope="class")
    def _class_scope(self, env):
        from .env import H3Conf

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
        from .env import H3Conf

        H3Conf(env).add_vhost_test1(h3_qpack_table_capacity=65536, h3_qpack_blocked_streams=32).install()
        assert env.apache_restart() == 0
        conf = _read_test_conf(env)
        assert "H3QpackTableCapacity 65536" in conf
        assert "H3QpackBlockedStreams 32" in conf

    def test_003_larger_qpack_table_serves(self, env):
        from .env import H3Conf

        H3Conf(env).add_vhost_test1(h3_qpack_table_capacity=65536, h3_qpack_blocked_streams=32).install()
        assert env.apache_restart() == 0
        r = self._get(env)
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response["status"] == 200

    def test_004_qpack_table_can_be_disabled(self, env):
        # 0 is meaningful: it tells the client not to use the dynamic table.
        from .env import H3Conf

        H3Conf(env).add_vhost_test1(h3_qpack_table_capacity=0, h3_qpack_blocked_streams=0).install()
        assert env.apache_restart() == 0
        assert "H3QpackTableCapacity 0" in _read_test_conf(env)
        r = self._get(env)
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response["status"] == 200

    def test_005_repeated_requests_with_dynamic_table(self, env):
        # With a table advertised, a client may reference earlier field values;
        # every response must still be correct.
        from .env import H3Conf

        H3Conf(env).add_vhost_test1(h3_qpack_table_capacity=4096).install()
        assert env.apache_restart() == 0
        url = env.mkurl("https", "test1", "/")
        for _ in range(5):
            r = env.curl_get(url, options=["--http3-only", "-k", "-H", "Cookie: a=" + "x" * 200])
            assert r.exit_code == 0, r.stderr + r.stdout
            assert r.response["status"] == 200

    def test_006_qpack_invalid_values(self, env):
        from .env import H3Conf

        H3Conf(env).add_vhost_test1(h3_qpack_table_capacity="invalid").install()
        assert env.apache_restart() != 0
        H3Conf(env).add_vhost_test1(h3_qpack_table_capacity=1048577).install()
        assert env.apache_restart() != 0
        H3Conf(env).add_vhost_test1(h3_qpack_blocked_streams=1001).install()
        assert env.apache_restart() != 0

    def test_007_worker_directives_in_vhost(self, env):
        from .env import H3Conf

        H3Conf(env).add_vhost_test1(h3_min_workers=4, h3_max_workers=8, h3_max_worker_idle_seconds=30).install()
        assert env.apache_restart() == 0
        conf = _read_test_conf(env)
        assert "H3MinWorkers 4" in conf
        assert "H3MaxWorkers 8" in conf
        assert "H3MaxWorkerIdleSeconds 30" in conf

    def test_008_small_worker_pool_still_serves_concurrent_requests(self, env):
        from concurrent.futures import ThreadPoolExecutor

        from .env import H3Conf

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
        from .env import H3Conf

        H3Conf(env).add_vhost_test1(h3_min_workers=0).install()
        assert env.apache_restart() != 0
        H3Conf(env).add_vhost_test1(h3_max_workers="invalid").install()
        assert env.apache_restart() != 0
        H3Conf(env).add_vhost_test1(h3_max_worker_idle_seconds=86401).install()
        assert env.apache_restart() != 0

    def test_010_max_below_min_is_corrected_not_fatal(self, env):
        # post_config raises H3MaxWorkers to H3MinWorkers and warns, so a
        # contradictory pair must not stop the server.
        from .env import H3Conf

        H3Conf(env).add_vhost_test1(h3_min_workers=8, h3_max_workers=2).install()
        assert env.apache_restart() == 0
        r = self._get(env)
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response["status"] == 200
