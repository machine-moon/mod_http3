import os
import json
import hashlib
import pytest
from .env import H3Conf

class TestPostPut:
    @pytest.fixture(autouse=True, scope="class")
    def _class_scope(self, env):
        H3Conf(env).add_vhost_test1().install()
        assert env.apache_restart() == 0

    def test_001_post_small_body(self, env):
        url = env.mkurl("https", "test1", "/cgi/echo.py")
        data = "Hello, World!"
        r = env.curl_post_data(url, data=data, options=["--http3", "-k"])
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response is not None
        assert r.response["status"] == 200
        resp_data = json.loads(r.response["body"])
        assert resp_data["method"] == "POST"
        assert resp_data["body_size"] == len(data)

    def test_002_put_large_body(self, env):
        url = env.mkurl("https", "test1", "/cgi/echo.py")

        # Create a 2MB test file
        fpath = os.path.join(env.gen_dir, "large_test.dat")
        data = b"0123456789ABCDEF" * (2 * 1024 * 1024 // 16) # 2MB
        with open(fpath, "wb") as f:
            f.write(data)

        r = env.curl_raw([url], options=["--http3", "-k", "--upload-file", fpath])
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response is not None
        assert r.response["status"] == 200

        resp_data = json.loads(r.response["body"])
        assert resp_data["method"] == "PUT"
        assert resp_data["body_size"] == len(data)
        assert resp_data["body_sha256"] == hashlib.sha256(data).hexdigest()

    def test_003_post_over_limit_body(self, env):
        # Default H3MaxRequestBodySize is 10MB
        url = env.mkurl("https", "test1", "/cgi/echo.py")

        # Create an 11MB test file
        fpath = os.path.join(env.gen_dir, "too_large_test.dat")
        data = b"0123456789ABCDEF" * (11 * 1024 * 1024 // 16) # 11MB
        with open(fpath, "wb") as f:
            f.write(data)

        r = env.curl_raw([url], options=["--http3", "-k", "--upload-file", fpath])
        # curl may fail with a protocol error or the server may reject it
        assert r.response is not None
        assert r.response["status"] in (413, 0)
