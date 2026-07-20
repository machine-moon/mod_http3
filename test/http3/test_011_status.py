import pytest
import json
from .env import H3Conf

class TestStatus:
    @pytest.fixture(autouse=True, scope='class')
    def _class_scope(self, env):
        H3Conf(env).add([
            "<Location /http3-status>",
            "    SetHandler http3-status",
            "</Location>"
        ]).add_vhost_test1().install()
        assert env.apache_restart() == 0

    def test_001_status_endpoint(self, env):
        # Do a request first to ensure some stats
        url = env.mkurl("https", "test1", "/cgi/echo.py")
        r = env.curl_post_data(url, data="Hello Status", options=["--http3", "-k"])
        assert r.exit_code == 0

        status_url = env.mkurl("https", "test1", "/http3-status")
        r = env.curl_get(status_url, options=["--http3", "-k"])
        assert r.exit_code == 0
        assert r.response["status"] == 200

        stats = json.loads(r.response["body"])
        assert "live_workers" in stats
        assert "total_connections" in stats
        assert "total_streams" in stats
        assert "total_bytes_read" in stats
        assert "total_bytes_written" in stats

        # We did at least one request, so connections should be >= 1
        assert stats["total_connections"] >= 1
        assert stats["total_streams"] >= 1
        assert stats["total_bytes_read"] > 0
        assert stats["total_bytes_written"] > 0
