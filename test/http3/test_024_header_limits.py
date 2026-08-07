import pytest


class TestHeaderLimits:
    """The core LimitRequest* directives must bound HTTP/3 request headers too."""

    @pytest.fixture(autouse=True, scope="class")
    def _class_scope(self, env):
        from .env import H3Conf

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
        from .env import H3Conf

        H3Conf(env).add_vhost_test1(extra_lines=["LimitRequestFieldSize 1024"]).install()
        assert env.apache_restart() == 0
        r = self._get(env, [("X-Big", "z" * 4096)])
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response is not None
        assert r.response["status"] == 431, r.response["status"]

    def test_003_field_under_limit_still_passes(self, env):
        from .env import H3Conf

        H3Conf(env).add_vhost_test1(extra_lines=["LimitRequestFieldSize 1024"]).install()
        assert env.apache_restart() == 0
        r = self._get(env, [("X-Ok", "z" * 512)])
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response["status"] == 200

    def test_004_too_many_fields_gets_431(self, env):
        from .env import H3Conf

        H3Conf(env).add_vhost_test1(extra_lines=["LimitRequestFields 20"]).install()
        assert env.apache_restart() == 0
        r = self._get(env, [(f"X-H{i}", "v") for i in range(40)])
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response is not None
        assert r.response["status"] == 431, r.response["status"]

    def test_005_field_count_under_limit_passes(self, env):
        from .env import H3Conf

        H3Conf(env).add_vhost_test1(extra_lines=["LimitRequestFields 40"]).install()
        assert env.apache_restart() == 0
        r = self._get(env, [(f"X-H{i}", "v") for i in range(10)])
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response["status"] == 200

    def test_006_connection_survives_a_rejected_request(self, env):
        # A 431 is a per-request answer, so the connection must keep serving.
        from .env import H3Conf

        H3Conf(env).add_vhost_test1(extra_lines=["LimitRequestFieldSize 1024"]).install()
        assert env.apache_restart() == 0

        bad = self._get(env, [("X-Big", "z" * 4096)])
        assert bad.response["status"] == 431
        good = self._get(env, [("X-Small", "v")])
        assert good.exit_code == 0, good.stderr + good.stdout
        assert good.response["status"] == 200
        assert good.response["protocol"] == "HTTP/3"
