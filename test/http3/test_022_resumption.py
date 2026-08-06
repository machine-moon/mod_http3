import os
import re

import pytest


def _read_test_conf(env):
    return open(os.path.join(env.server_dir, "conf", "test.conf")).read()


class TestSessionResumption:
    """H3SessionTickets controls TLS ticket issuance; H3EarlyData is honoured only where the engine can."""

    @pytest.fixture(autouse=True, scope="class")
    def _class_scope(self, env):
        from .env import H3Conf

        H3Conf(env).add_vhost_test1().install()
        assert env.apache_restart() == 0

    def test_001_tickets_on_by_default_and_serve(self, env):
        url = env.mkurl("https", "test1", "/")
        r = env.curl_get(url, options=["--http3-only", "-k"])
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response is not None
        assert r.response["status"] == 200
        assert r.response["protocol"] == "HTTP/3"

    def test_002_tickets_off_in_vhost(self, env):
        from .env import H3Conf

        H3Conf(env).add_vhost_test1(h3_session_tickets=False).install()
        assert env.apache_restart() == 0
        assert "H3SessionTickets off" in _read_test_conf(env)

    def test_003_requests_still_work_without_tickets(self, env):
        # Turning resumption off must cost a round trip, never correctness.
        from .env import H3Conf

        H3Conf(env).add_vhost_test1(h3_session_tickets=False).install()
        assert env.apache_restart() == 0
        url = env.mkurl("https", "test1", "/")
        for _ in range(2):
            r = env.curl_get(url, options=["--http3-only", "-k"])
            assert r.exit_code == 0, r.stderr + r.stdout
            assert r.response["status"] == 200
            assert r.response["protocol"] == "HTTP/3"

    def test_004_tickets_on_explicitly(self, env):
        from .env import H3Conf

        H3Conf(env).add_vhost_test1(h3_session_tickets=True).install()
        assert env.apache_restart() == 0
        assert "H3SessionTickets on" in _read_test_conf(env)
        url = env.mkurl("https", "test1", "/")
        r = env.curl_get(url, options=["--http3-only", "-k"])
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response["status"] == 200

    def test_005_early_data_off_by_default(self, env):
        from .env import H3Conf

        H3Conf(env).add_vhost_test1().install()
        assert env.apache_restart() == 0
        assert "H3EarlyData" not in _read_test_conf(env)

    def test_006_early_data_on_warns_when_engine_cannot(self, env):
        # The OpenSSL QUIC stack has no server-side 0-RTT. Enabling early data
        # must start the server and say plainly that it has no effect, rather
        # than leave an operator believing 0-RTT is running.
        from .env import H3Conf

        H3Conf(env).add_vhost_test1(h3_early_data=True).install()
        assert env.apache_restart() == 0
        assert "H3EarlyData on" in _read_test_conf(env)

        url = env.mkurl("https", "test1", "/")
        r = env.curl_get(url, options=["--http3-only", "-k"])
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response["status"] == 200

        engine = os.environ.get("H3_QUIC_ENGINE", "openssl")
        if engine in ("openssl", "null"):
            pattern = re.compile(r".*does not accept 0-RTT data.*")
            assert env.httpd_error_log.scan_recent(pattern, timeout=10), (
                "expected a warning that the engine ignores H3EarlyData"
            )

    def test_007_early_data_off_explicitly(self, env):
        from .env import H3Conf

        H3Conf(env).add_vhost_test1(h3_early_data=False).install()
        assert env.apache_restart() == 0
        assert "H3EarlyData off" in _read_test_conf(env)
        url = env.mkurl("https", "test1", "/")
        r = env.curl_get(url, options=["--http3-only", "-k"])
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response["status"] == 200
