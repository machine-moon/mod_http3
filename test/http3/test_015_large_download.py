import hashlib
import os

import pytest


class TestLargeDownload:
    """A response larger than the QUIC stream send buffer, pulled by a
    rate-limited client, keeps the connection under write backpressure for
    the whole transfer. The send path must block/unblock the stream instead
    of spinning, and the payload must arrive intact."""

    PAYLOAD_SIZE = 2 * 1024 * 1024

    @pytest.fixture(autouse=True, scope="class")
    def _class_scope(self, env):
        from .env import H3Conf

        payload = os.urandom(self.PAYLOAD_SIZE)
        digest = hashlib.sha256(payload).hexdigest()
        with open(os.path.join(env.server_docs_dir, "large.bin"), "wb") as fd:
            fd.write(payload)
        type(self).expected_sha256 = digest

        H3Conf(env).add_vhost_test1().install()
        assert env.apache_restart() == 0

    def test_001_rate_limited_download_intact(self, env):
        url = env.mkurl("https", "test1", "/large.bin")
        r = env.curl_get(url, options=["--http3-only", "-k", "--limit-rate", "600K"])
        assert r.exit_code == 0, r.stderr
        assert r.response is not None
        assert r.response["status"] == 200
        body = r.response["body"]
        assert len(body) == self.PAYLOAD_SIZE
        assert hashlib.sha256(body).hexdigest() == self.expected_sha256

    def test_002_full_speed_download_intact(self, env):
        url = env.mkurl("https", "test1", "/large.bin")
        r = env.curl_get(url, options=["--http3-only", "-k"])
        assert r.exit_code == 0, r.stderr
        assert r.response["status"] == 200
        assert hashlib.sha256(r.response["body"]).hexdigest() == self.expected_sha256
