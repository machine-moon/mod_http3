import os

from pyhttpd.env import HttpdTestEnv, HttpdTestSetup
from pyhttpd.conf import HttpdConf
from pyhttpd.certs import Credentials


class H3TestSetup(HttpdTestSetup):
    """Loads mod_http3 in addition to the standard test modules."""

    def _make_modules_conf(self):
        super()._make_modules_conf()
        path = self.env.mod_http3_path
        if not os.path.isfile(path):
            raise RuntimeError(
                f"mod_http3.so not found at {path} - run `cmake --build build` first"
            )
        with open(os.path.join(self.env.server_dir, "conf/modules.conf"), "a") as fd:
            fd.write(f'LoadModule http3_module "{path}"\n')
            cgid_path = os.path.join(self.env.libexec_dir, "mod_cgid.so")
            if os.path.isfile(cgid_path):
                fd.write(f'LoadModule cgid_module "{cgid_path}"\n')


class H3TestEnv(HttpdTestEnv):
    def __init__(self, pytestconfig=None):
        super().__init__(pytestconfig=pytestconfig)
        self._mod_http3_path = self.config.get("test", "mod_http3_path")
        self._test_cert_file = self.config.get("test", "test_cert_file")
        self._test_key_file = self.config.get("test", "test_key_file")

        self._static_creds = Credentials(
            name="localhost",
            cert=open(self._test_cert_file, "rb").read(),
            pkey=open(self._test_key_file, "rb").read(),
        )
        self._static_creds._cert_file = self._test_cert_file
        self._static_creds._pkey_file = self._test_key_file
        self._httpd_log_modules = ["http3", "ssl"]
        self.add_httpd_conf(["Protocols h3 http/1.1"])

    @property
    def mod_http3_path(self) -> str:
        return self._mod_http3_path

    @property
    def test_cert_file(self) -> str:
        return self._test_cert_file

    @property
    def test_key_file(self) -> str:
        return self._test_key_file

    def issue_certs(self) -> None:
        return

    def get_credentials_for_name(self, dns_name):
        return [self._static_creds]

    def get_ca_pem_file(self, hostname):
        if len(self.get_credentials_for_name(hostname)) > 0:
            return self._test_cert_file
        return None

    def setup_httpd(self, setup=None):
        if setup is None:
            setup = H3TestSetup(env=self)
        return super().setup_httpd(setup=setup)


class H3Conf(HttpdConf):
    def __init__(self, env: H3TestEnv):
        super().__init__(env)

    def add_vhost_test1(
        self,
        proxy_self=False,
        h2proxy_self=False,
        h3_port=True,
        h3_cert_path=None,
        h3_key_path=None,
        h3_max_concurrent_streams=None,
        h3_stream_buffer_size=None,
        h3_max_request_body_size=None,
        h3_max_response_body_size=None,
        h3_alt_svc=None,
        h3_alt_svc_max_age=None,
        h3_handshake_timeout=None,
        h3_idle_timeout=None,
        h3_address_validation=None,
        h3_socket_buffer_size=None,
        h3_session_tickets=None,
        h3_early_data=None,
        extra_lines=None
    ):
        self.start_vhost(
            [f"test1.{self.env.http_tld}"],
            doc_root="htdocs",
            with_ssl=True,
        )
        if h3_port:
            port = h3_port if not isinstance(h3_port, bool) else self.env.https_port
            self.add(f"H3Port {port}")
        
        cert = h3_cert_path if h3_cert_path else self.env.test_cert_file
        key = h3_key_path if h3_key_path else self.env.test_key_file
        self.add(f"H3CertificatePath {cert}")
        self.add(f"H3CertificateKeyPath {key}")

        if h3_max_concurrent_streams is not None:
            self.add(f"H3MaxConcurrentStreams {h3_max_concurrent_streams}")
        if h3_stream_buffer_size is not None:
            self.add(f"H3StreamBufferSize {h3_stream_buffer_size}")
        if h3_max_request_body_size is not None:
            self.add(f"H3MaxRequestBodySize {h3_max_request_body_size}")
        if h3_max_response_body_size is not None:
            self.add(f"H3MaxResponseBodySize {h3_max_response_body_size}")
        if h3_alt_svc is not None:
            val = "on" if h3_alt_svc is True else ("off" if h3_alt_svc is False else h3_alt_svc)
            self.add(f"H3AltSvc {val}")
        if h3_alt_svc_max_age is not None:
            self.add(f"H3AltSvcMaxAge {h3_alt_svc_max_age}")
        if h3_handshake_timeout is not None:
            self.add(f"H3HandshakeTimeout {h3_handshake_timeout}")
        if h3_idle_timeout is not None:
            self.add(f"H3IdleTimeout {h3_idle_timeout}")
        if h3_address_validation is not None:
            val = "on" if h3_address_validation is True else ("off" if h3_address_validation is False else h3_address_validation)
            self.add(f"H3AddressValidation {val}")
        if h3_socket_buffer_size is not None:
            self.add(f"H3SocketBufferSize {h3_socket_buffer_size}")
        if h3_session_tickets is not None:
            val = "on" if h3_session_tickets is True else ("off" if h3_session_tickets is False else h3_session_tickets)
            self.add(f"H3SessionTickets {val}")
        if h3_early_data is not None:
            val = "on" if h3_early_data is True else ("off" if h3_early_data is False else h3_early_data)
            self.add(f"H3EarlyData {val}")

        self.add("Protocols h3 http/1.1")
        for line in extra_lines or []:
            self.add(line)
        self.end_vhost()
        return self
