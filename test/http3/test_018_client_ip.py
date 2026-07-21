import json
from concurrent.futures import ThreadPoolExecutor

import pytest


class TestClientIp:
    """The synthesized HTTP/3 connection must carry its UDP peer address."""

    @pytest.fixture(autouse=True, scope="class")
    def _class_scope(self, env):
        from .env import H3Conf

        H3Conf(env).add_vhost_test1(
            extra_lines=[
                '<Location "/cgi/env.py">',
                "    Require ip 127.0.0.0/8",
                "</Location>",
            ]
        ).install()
        assert env.apache_restart() == 0

    def test_001_require_ip_uses_quic_peer(self, env):
        url = env.mkurl("https", "test1", "/cgi/env.py")
        r = env.curl_get(url, options=["--http3-only", "-k"])
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response is not None
        assert r.response["status"] == 200

        payload = json.loads(r.response["body"])
        assert payload["remote_addr"] == "127.0.0.1", payload
        assert 0 < int(payload["remote_port"]) <= 65535, payload

    def test_002_nondefault_loopback_source_is_preserved(self, env):
        url = env.mkurl("https", "test1", "/cgi/env.py")
        r = env.curl_get(
            url,
            options=["--http3-only", "-k", "--interface", "127.0.0.2"],
        )
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response is not None
        assert r.response["status"] == 200
        assert json.loads(r.response["body"])["remote_addr"] == "127.0.0.2"

    def test_003_concurrent_clients_keep_their_own_address(self, env):
        url = env.mkurl("https", "test1", "/cgi/env.py")
        sources = [f"127.0.0.{i}" for i in range(1, 9)]

        def request_from(source):
            r = env.curl_get(
                url,
                options=["--http3-only", "-k", "--interface", source],
            )
            assert r.exit_code == 0, r.stderr + r.stdout
            assert r.response is not None
            assert r.response["status"] == 200
            return json.loads(r.response["body"])["remote_addr"]

        with ThreadPoolExecutor(max_workers=len(sources)) as pool:
            observed = list(pool.map(request_from, sources))
        assert observed == sources
