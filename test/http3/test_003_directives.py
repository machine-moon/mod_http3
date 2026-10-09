import os
import socket

import pytest

from .env import H3Conf, H3TestEnv


def _read_modules_conf(env):
    return open(os.path.join(env.server_dir, "conf", "modules.conf")).read()


def _read_test_conf(env):
    return open(os.path.join(env.server_dir, "conf", "test.conf")).read()


class TestH3Directives:
    @pytest.fixture(autouse=True, scope="class")
    def _class_scope(self, env):
        H3Conf(env).add_vhost_test1().install()
        assert env.apache_restart() == 0

    def test_001_http3_module_loaded(self, env):
        conf = _read_modules_conf(env)
        assert "LoadModule http3_module" in conf
        assert env.mod_http3_path in conf

    def test_002_h3port_in_vhost(self, env):
        conf = _read_test_conf(env)
        assert "H3Port" in conf
        assert str(env.https_port) in conf

    def test_003_h3_cert_inherited_from_mod_ssl(self, env):
        # No mod_http3 certificate directive exists; mod_ssl's pair is what QUIC serves.
        conf = _read_test_conf(env)
        assert "SSLCertificateFile" in conf
        assert env.apache_restart() == 0

    def test_004_protocols_h3_in_vhost(self, env):
        conf = _read_test_conf(env)
        assert "Protocols h3 http/1.1" in conf

    def test_005_udp_socket_bound(self, env):
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
            s.settimeout(2)
            try:
                s.sendto(b"\x00", ("127.0.0.1", env.https_port))
            except OSError as e:
                pytest.fail(f"UDP port {env.https_port} not writable: {e}")

    def test_006_tls_socket_listening(self, env):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.settimeout(2)
            assert s.connect_ex(("127.0.0.1", env.https_port)) == 0

    def test_007_handshake_timeout_in_vhost(self, env):
        H3Conf(env).add_vhost_test1(h3_handshake_timeout=15).install()
        assert env.apache_restart() == 0
        conf = _read_test_conf(env)
        assert "H3HandshakeTimeout 15" in conf

    def test_008_handshake_timeout_invalid_value(self, env):
        H3Conf(env).add_vhost_test1(h3_handshake_timeout="invalid").install()
        assert env.apache_restart() != 0
        H3Conf(env).add_vhost_test1(h3_handshake_timeout=999).install()
        assert env.apache_restart() != 0

    def test_009_idle_timeout_in_vhost(self, env):
        H3Conf(env).add_vhost_test1(h3_idle_timeout=600).install()
        assert env.apache_restart() == 0
        conf = _read_test_conf(env)
        assert "H3IdleTimeout 600" in conf

    def test_010_idle_timeout_invalid_value(self, env):
        H3Conf(env).add_vhost_test1(h3_idle_timeout="invalid").install()
        assert env.apache_restart() != 0
        H3Conf(env).add_vhost_test1(h3_idle_timeout=99999999).install()
        assert env.apache_restart() != 0

    def test_011_max_response_body_size_in_vhost(self, env):
        H3Conf(env).add_vhost_test1(h3_max_response_body_size=1048576).install()
        assert env.apache_restart() == 0
        conf = _read_test_conf(env)
        assert "H3MaxResponseBodySize 1048576" in conf

    def test_012_max_response_body_size_invalid_value(self, env):
        H3Conf(env).add_vhost_test1(h3_max_response_body_size="invalid").install()
        assert env.apache_restart() != 0
        H3Conf(env).add_vhost_test1(h3_max_response_body_size=0).install()
        assert env.apache_restart() != 0
