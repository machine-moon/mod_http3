# Verify

Validate configuration before starting httpd:

```sh
/path/to/httpd/bin/httpd -t
/path/to/httpd/bin/httpd -M | grep http3
ss -ulnp | grep 4433
```

The module list should show `http3_module (shared)`, and the socket inspection should show a UDP listener on the selected port.

## Test HTTP/3

Use a curl build with HTTP/3 support:

```sh
curl -V
curl --http3-only -k -sI https://localhost:4433/
```

`curl -V` must list `HTTP3`. `--http3-only` prevents fallback to HTTP/2 or HTTP/1.1, so a successful response proves a QUIC connection was used.

For trusted local testing, prefer `--cacert /path/to/server.crt` over `-k`.

See [HTTP/3 testing with curl](https://github.com/machine-moon/mod_http3/blob/trunk/docs/testing-with-curl.md) for GET, POST, PUT, concurrent stream, and failure-diagnosis commands.
