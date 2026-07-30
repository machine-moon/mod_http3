import asyncio
import hashlib
import os
import ssl

import pytest

from aioquic.asyncio.client import connect
from aioquic.h3.connection import H3_ALPN
from aioquic.quic.configuration import QuicConfiguration

from .test_008_stream_multiplexing import _MuxClient


class TestLargeDownload:
    """A rate-limited pull of an oversized response must arrive intact under write backpressure."""

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


class TestFileBucketDownload:
    """With EnableMMAP off apr_bucket_read splits the file bucket, and the filter must follow the tail."""

    PAYLOAD_SIZE = 64 * 1024

    @pytest.fixture(autouse=True, scope="class")
    def _class_scope(self, env):
        from .env import H3Conf

        payload = os.urandom(self.PAYLOAD_SIZE)
        with open(os.path.join(env.server_docs_dir, "nommap.bin"), "wb") as fd:
            fd.write(payload)
        type(self).expected_sha256 = hashlib.sha256(payload).hexdigest()

        H3Conf(env).add_vhost_test1(extra_lines=["EnableMMAP Off"]).install()
        assert env.apache_restart() == 0

    def test_001_body_survives_the_bucket_split(self, env):
        authority = f"test1.{env.http_tld}"

        async def run():
            config = QuicConfiguration(
                is_client=True, alpn_protocols=H3_ALPN, verify_mode=ssl.CERT_NONE, server_name=authority
            )
            async with connect(
                env.http_addr, env.https_port, configuration=config, create_protocol=_MuxClient
            ) as client:
                sid = client.start_get(authority, "/nommap.bin")
                client.transmit()
                await asyncio.wait_for(client.done[sid].wait(), timeout=15)
                return client.status[sid], client.body[sid]

        status, body = asyncio.run(run())
        assert status == "200"
        assert len(body) == self.PAYLOAD_SIZE
        assert hashlib.sha256(body).hexdigest() == self.expected_sha256
