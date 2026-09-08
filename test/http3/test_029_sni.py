import os

import pytest

from pyhttpd.certs import HttpdTestCA


class TestSni:
    """Each HTTP/3 host presents its own certificate, chosen by the client's SNI."""

    @pytest.fixture(autouse=True, scope="class")
    def _class_scope(self, env):
        from .env import H3Conf

        # A second certificate with a different subject than the shared test one.
        name = f"test2.{env.http_tld}"
        creds = HttpdTestCA.create_root(name=name, store_dir=env.gen_dir)
        cert = os.path.join(env.gen_dir, "test2-sni.crt")
        key = os.path.join(env.gen_dir, "test2-sni.key")
        creds.save_cert_pem(cert)
        creds.save_pkey_pem(key)

        conf = H3Conf(env)
        conf.add_vhost_test1()
        conf.start_vhost([name], doc_root="htdocs/two", with_ssl=True, with_certificates=False)
        conf.add(f"SSLCertificateFile {cert}")
        conf.add(f"SSLCertificateKeyFile {key}")
        conf.add("Protocols h3 http/1.1")
        conf.end_vhost()
        conf.install()
        assert env.apache_restart() == 0

    def _subject(self, env, host):
        url = env.mkurl("https", host, "/index.html")
        r = env.curl_get(url, options=["--http3-only", "-k", "-v"])
        assert r.exit_code == 0, r.stderr + r.stdout
        assert r.response["protocol"] == "HTTP/3"
        lines = [l for l in r.stderr.splitlines() if "subject:" in l]
        assert lines, r.stderr
        return lines[0]

    def test_001_second_host_gets_its_own_certificate(self, env):
        assert f"test2.{env.http_tld}" in self._subject(env, "test2")

    def test_002_first_host_keeps_the_shared_certificate(self, env):
        assert f"test2.{env.http_tld}" not in self._subject(env, "test1")
