import json
import os
import re
import shutil
import subprocess

import pytest

from .env import H3Conf

# ngtcp2's osslclient is the one client here that sends QUIC 0-RTT application
# data. aioquic negotiates TLS early data but keeps the request for 1-RTT, so it
# cannot drive this path. curl needs an HTTP/3 build with 0-RTT support.
OSSLCLIENT = shutil.which("osslclient")

pytestmark = pytest.mark.skipif(OSSLCLIENT is None, reason="ngtcp2 osslclient not on PATH")


def _run(env, session, tp, extra=None):
    """Run osslclient once against test1; return (exit_code, stdout, stderr)."""
    authority = f"test1.{env.http_tld}"
    args = [OSSLCLIENT, f"--session-file={session}", f"--tp-file={tp}",
            "--timeout=2s", "127.0.0.1", str(env.https_port), f"https://{authority}/"]
    e = dict(os.environ, SSL_CERT_FILE=os.path.join(env.server_dir, "certs", "ca.crt"))
    # The harness mints certs under test/certs; fall back to that CA.
    if not os.path.exists(e["SSL_CERT_FILE"]):
        e["SSL_CERT_FILE"] = env.test_cert_file.replace("server.crt", "ca.crt")
    p = subprocess.run(args + (extra or []), capture_output=True, text=True, env=e, timeout=15)
    return p.returncode, p.stdout, p.stderr


def _h3_200s(env):
    """Count successful HTTP/3 responses in the access log."""
    log = os.path.join(env.server_dir, "logs", "access_log")
    if not os.path.exists(log):
        return 0
    n = 0
    for line in open(log):
        m = re.search(r'"request":\s*"[^"]*HTTP/3[^"]*".*?"status":\s*200', line)
        if m:
            n += 1
    return n


class TestEarlyData:

    @pytest.fixture(autouse=True, scope="class")
    def _scope(self, env):
        H3Conf(env).add_vhost_test1(h3_session_tickets=True, h3_early_data=True).install()
        assert env.apache_restart() == 0

    def test_001_zero_rtt_get_is_answered(self, env, tmp_path):
        session = str(tmp_path / "sess")
        tp = str(tmp_path / "tp")
        # First connection: full handshake, saves the ticket and transport params.
        rc, _, _ = _run(env, session, tp)
        assert rc == 0, "first osslclient run failed"
        assert os.path.getsize(session) > 0, "no session ticket was saved"
        before = _h3_200s(env)
        # Second connection: resumes and sends the GET as 0-RTT.
        rc, _, err = _run(env, session, tp)
        assert rc == 0, "0-RTT osslclient run failed"
        assert "QUIC handshake has completed" in err
        assert _h3_200s(env) > before, "the 0-RTT GET was not answered with 200"
