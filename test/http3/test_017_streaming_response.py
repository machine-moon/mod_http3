import re
import subprocess
import time

import pytest


class TestStreamingResponse:
    """Response headers and early body bytes must not wait for handler EOS."""

    @pytest.fixture(autouse=True, scope="class")
    def _class_scope(self, env):
        from .env import H3Conf

        H3Conf(env).add_vhost_test1(
            extra_lines=[
                '<Location "/h3-slow-stream">',
                "    SetHandler aptest-slow-stream",
                "</Location>",
                '<Location "/h3-flood-stream">',
                "    SetHandler aptest-flood-stream",
                "</Location>",
                '<Location "/h3-active-floods">',
                "    SetHandler aptest-active-floods",
                "</Location>",
            ]
        ).install()
        assert env.apache_restart() == 0

    def test_001_response_arrives_before_handler_finishes(self, env):
        url = env.mkurl("https", "test1", "/h3-slow-stream")
        args = [
            env.curl,
            "-sS",
            "--http3-only",
            "--no-buffer",
            *env.curl_resolve_args(url, insecure=True),
            "-w",
            "\nH3_TIMING start=%{time_starttransfer} total=%{time_total} version=%{http_version}\n",
            url,
        ]
        result = subprocess.run(args, capture_output=True, timeout=10, check=False)
        stdout = result.stdout.decode("utf-8", errors="replace")
        stderr = result.stderr.decode("utf-8", errors="replace")

        assert result.returncode == 0, stderr + stdout
        assert "first-chunk\nsecond-chunk\n" in stdout
        match = re.search(
            r"H3_TIMING start=([0-9.]+) total=([0-9.]+) version=([0-9]+)", stdout
        )
        assert match, stdout
        start = float(match.group(1))
        total = float(match.group(2))
        assert match.group(3) == "3", stdout
        assert start < 1.5, f"response was buffered until handler completion: {stdout}"
        assert total >= 2.5, f"slow handler did not exercise progressive delivery: {stdout}"

    def test_002_client_abort_wakes_response_producer(self, env):
        url = env.mkurl("https", "test1", "/h3-flood-stream")
        args = [
            env.curl,
            "-sS",
            "--http3-only",
            "--no-buffer",
            "--max-time",
            "1",
            "--limit-rate",
            "1024",
            *env.curl_resolve_args(url, insecure=True),
            url,
        ]
        result = subprocess.run(args, capture_output=True, timeout=5, check=False)
        assert result.returncode != 0
        assert b"first-chunk\n" in result.stdout

        # The rate-limited client leaves the producer blocked behind the
        # bounded queue. Resetting the stream must wake that worker promptly.
        active_url = env.mkurl("https", "test1", "/h3-active-floods")
        deadline = time.monotonic() + 5
        while True:
            live = env.curl_get(active_url, options=["--http3-only", "-k"])
            if live.exit_code == 0 and live.response["body"].strip() == b"0":
                break
            assert time.monotonic() < deadline, live.stderr + live.stdout
            time.sleep(0.1)
        assert live.response["status"] == 200
        assert live.response["protocol"] == "HTTP/3"
