import os
import re
import shutil
import subprocess

import pytest

from .env import H3Conf

OS_RMEM_MAX = "/proc/sys/net/core/rmem_max"


def _read_test_conf(env):
    return open(os.path.join(env.server_dir, "conf", "test.conf")).read()


def _sysctl_max(path):
    try:
        return int(open(path).read().strip())
    except OSError:
        return None


def _udp_recv_buffer(port):
    """Kernel SO_RCVBUF of the UDP socket bound to `port`, as ss reports it, or None."""
    if not shutil.which("ss"):
        return None
    out = subprocess.run(["ss", "-uapnm", "sport = :%d" % port],
                         capture_output=True, text=True).stdout
    sizes = [int(m) for m in re.findall(r"skmem:\(r\d+,rb(\d+),", out)]
    return max(sizes) if sizes else None


def _expected_recv_buffer(requested):
    """Linux caps SO_RCVBUF at rmem_max and then reports double the stored value."""
    rmem_max = _sysctl_max(OS_RMEM_MAX)
    if rmem_max is None:
        return None
    return min(requested, rmem_max) * 2


class TestSocketBuffer:
    """H3SocketBufferSize sizes the QUIC socket buffers and never blocks startup."""

    @pytest.fixture(autouse=True, scope="class")
    def _class_scope(self, env):
        H3Conf(env).add_vhost_test1().install()
        assert env.apache_restart() == 0

    def _assert_serves(self, env):
        r = env.curl_get(env.mkurl("https", "test1", "/"), options=["--http3-only", "-k"])
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response is not None
        assert r.response["status"] == 200
        assert r.response["protocol"] == "HTTP/3"

    def _assert_buffer(self, env, requested):
        expected = _expected_recv_buffer(requested)
        got = _udp_recv_buffer(env.https_port)
        if expected is None or got is None:
            pytest.skip("kernel socket buffer not observable here")
        assert got == expected, f"SO_RCVBUF is {got}, expected {expected} for a {requested} byte request"

    def test_001_default_serves_requests(self, env):
        self._assert_serves(env)

    def test_002_default_sizes_the_socket(self, env):
        self._assert_buffer(env, 2097152)

    def test_003_explicit_size_in_vhost(self, env):
        H3Conf(env).add_vhost_test1(h3_socket_buffer_size=4194304).install()
        assert env.apache_restart() == 0
        assert "H3SocketBufferSize 4194304" in _read_test_conf(env)
        self._assert_serves(env)
        self._assert_buffer(env, 4194304)

    def test_004_size_the_os_will_cap_still_starts(self, env):
        H3Conf(env).add_vhost_test1(h3_socket_buffer_size=67108864).install()
        assert env.apache_restart() == 0
        self._assert_serves(env)
        self._assert_buffer(env, 67108864)

    def test_005_invalid_value(self, env):
        H3Conf(env).add_vhost_test1(h3_socket_buffer_size="invalid").install()
        assert env.apache_restart() != 0
        H3Conf(env).add_vhost_test1(h3_socket_buffer_size=0).install()
        assert env.apache_restart() != 0
        H3Conf(env).add_vhost_test1(h3_socket_buffer_size=67108865).install()
        assert env.apache_restart() != 0

    def test_006_many_concurrent_requests(self, env):
        from concurrent.futures import ThreadPoolExecutor

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
