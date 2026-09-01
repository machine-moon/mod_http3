import os
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
        path = os.path.join(env.server_docs_dir, "goaway-big.bin")
        with open(path, "wb") as fd:
            fd.write(b"g" * (4 * 1024 * 1024))
        yield
        try:
            os.unlink(path)
        except OSError:
            pass

    def test_001_goaway_sent_on_graceful_restart(self, env):
        url = env.mkurl("https", "test1", "/goaway-big.bin")
        log_path = env.httpd_error_log.path
        log_start = os.path.getsize(log_path) if os.path.isfile(log_path) else 0

        def do_get(_i):
            return env.curl_get(url, options=["--http3-only", "-k"])

        def connection_established():
            with open(log_path) as fd:
                fd.seek(log_start)
                return "accepted new QUIC connection" in fd.read()

        with ThreadPoolExecutor(max_workers=10) as pool:
            futures = [pool.submit(do_get, i) for i in range(10)]
            deadline = time.monotonic() + 10
            while not connection_established() and time.monotonic() < deadline:
                time.sleep(0.05)
            assert connection_established(), "no HTTP/3 connection reached the server"
            assert env.apache_reload() == 0
            results = [f.result() for f in futures]

        # Established connections must be wound down gracefully.
        succeeded = [r for r in results if r.exit_code == 0 and r.response is not None]
        for r in succeeded:
            assert r.response["status"] == 200
            assert r.response["protocol"] == "HTTP/3"

        deadline = time.monotonic() + 10
        while True:
            with open(log_path) as fd:
                fd.seek(log_start)
                if "sent HTTP/3 GOAWAY" in fd.read():
                    break
            assert time.monotonic() < deadline, "no GOAWAY logged for the draining connections"
            time.sleep(0.1)

        # Server must serve requests successfully after restart.
        url = env.mkurl("https", "test1", "/index.html")
        assert env.is_live()
        deadline = time.monotonic() + 30
        while True:
            r = env.curl_get(url, options=["--http3-only", "-k"])
            if r.exit_code == 0 or time.monotonic() >= deadline:
                break
            time.sleep(0.5)
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response["status"] == 200
        assert r.response["protocol"] == "HTTP/3"
