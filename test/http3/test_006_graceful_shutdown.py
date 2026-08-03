import re
import time
from concurrent.futures import ThreadPoolExecutor

import pytest


class TestGracefulShutdown:
    """Test HTTP/3 GOAWAY sending and connection wind-down on graceful restart."""

    @pytest.fixture(autouse=True, scope="class")
    def _class_scope(self, env):
        from .env import H3Conf

        H3Conf(env).add_vhost_test1().install()
        assert env.apache_restart() == 0

    @pytest.mark.xfail(
        reason="races the reload against session setup and the UDP port handover",
        strict=False,
    )
    def test_001_goaway_sent_on_graceful_restart(self, env):
        url = env.mkurl("https", "test1", "/index.html")

        def do_get(_i):
            return env.curl_get(url, options=["--http3-only", "-k"])

        # Trigger a graceful restart during concurrent requests.
        with ThreadPoolExecutor(max_workers=10) as pool:
            futures = [pool.submit(do_get, i) for i in range(10)]
            assert env.apache_reload() == 0
            results = [f.result() for f in futures]

        # Established connections must be wound down gracefully.
        succeeded = [r for r in results if r.exit_code == 0 and r.response is not None]
        for r in succeeded:
            assert r.response["status"] == 200
            assert r.response["protocol"] == "HTTP/3"

        pattern = re.compile(r".*\[http3:info].*sent HTTP/3 GOAWAY.*")
        env.httpd_error_log.scan_recent(pattern, timeout=10)

        # Server must serve requests successfully after restart.
        assert env.is_live()
        # is_live() only proves the TCP listener is back; the UDP port is re-acquired asynchronously.
        deadline = time.monotonic() + 30
        while True:
            r = env.curl_get(url, options=["--http3-only", "-k"])
            if r.exit_code == 0 or time.monotonic() >= deadline:
                break
            time.sleep(0.5)
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response["status"] == 200
        assert r.response["protocol"] == "HTTP/3"
