# mod_http3 httpd Configuration

Apache httpd configuration directives for mod_http3.

For build and installation, see [INSTALL](INSTALL).

## Overview

mod_http3 enables HTTP/3 protocol support in Apache HTTP Server. The module:

- Creates a separate worker thread for HTTP/3 connections over UDP/QUIC
- Uses OpenSSL for QUIC/TLS 1.3 support
- Uses nghttp3 for HTTP/3 protocol handling
- Integrates with Apache's standard request processing pipeline

## Configuration Directives

### H3CertificatePath

**Syntax:** `H3CertificatePath /path/to/certificate.pem`
**Context:** server config, virtual host
**Required:** Yes

Path to the TLS certificate file for HTTP/3 connections. May point to the same file used by `SSLCertificateFile`.

### H3CertificateKeyPath

**Syntax:** `H3CertificateKeyPath /path/to/private-key.pem`
**Context:** server config, virtual host
**Required:** Yes

Path to the TLS private key file for HTTP/3 connections. May point to the same file used by `SSLCertificateKeyFile`.

### H3Port

**Syntax:** `H3Port port`
**Context:** server config, virtual host
**Default:** the port of the VirtualHost that configured HTTP/3

UDP port the QUIC listener binds to. When unset, the module reuses the port of the VirtualHost that carries the `H3CertificatePath`/`H3CertificateKeyPath` pair, so TCP (HTTP/1.1, HTTP/2) and UDP (HTTP/3) share the same port number. Set it explicitly to serve HTTP/3 on a different port.

### H3MaxConcurrentStreams

**Syntax:** `H3MaxConcurrentStreams n`
**Context:** server config, virtual host
**Default:** `100`

Maximum number of concurrent HTTP/3 streams (in-flight requests) per QUIC connection.

### H3MaxConnections

**Syntax:** `H3MaxConnections n`
**Context:** server config, virtual host
**Default:** `256`

Maximum number of concurrent QUIC/HTTP/3 connections per child process. New connection attempts beyond the limit are refused.

### H3StreamBufferSize

**Syntax:** `H3StreamBufferSize bytes`
**Context:** server config, virtual host
**Default:** `65536`

Per-stream read/write buffer size in bytes.

### H3MaxRequestBodySize

**Syntax:** `H3MaxRequestBodySize bytes`
**Context:** server config, virtual host
**Default:** `10485760` (10 MiB)

Maximum HTTP/3 request body size in bytes. Request bodies are fully buffered in memory; requests exceeding the limit are rejected.

### H3MaxResponseBodySize

**Syntax:** `H3MaxResponseBodySize bytes`
**Context:** server config, virtual host
**Default:** unlimited

Maximum HTTP/3 response body size in bytes. Response bodies are fully buffered in memory pending streaming support; this directive is unset (unlimited) by default so existing large-response deployments are unaffected. Set it to bound worst-case per-request memory use. Responses exceeding the limit are replaced with a `500 Internal Server Error` — the real body is already partially generated and discarded at that point, so its `Content-Length` can no longer be trusted.

### H3AltSvc

**Syntax:** `H3AltSvc on|off`
**Context:** server config, virtual host
**Default:** `on`

Whether to advertise HTTP/3 support by injecting an `Alt-Svc` response header. This is how browsers and other TCP clients discover that HTTP/3 is available over UDP. See [Alt-Svc Header](#alt-svc-header).

### H3AltSvcMaxAge

**Syntax:** `H3AltSvcMaxAge seconds`
**Context:** server config, virtual host
**Default:** `86400`

Number of seconds a client may cache the `Alt-Svc` HTTP/3 advertisement (the `ma=` field of the injected header).

### H3HandshakeTimeout

**Syntax:** `H3HandshakeTimeout seconds`
**Context:** server config, virtual host
**Default:** `10`

The timeout duration in seconds for QUIC handshakes to complete. If a connection is accepted but fails to finish the cryptographic TLS/QUIC handshake within this period, it is terminated and its resources are cleaned up. Helps prevent resource exhaustion attacks.

### H3IdleTimeout

**Syntax:** `H3IdleTimeout seconds`
**Context:** server config, virtual host
**Default:** `300`

The idle timeout duration in seconds for QUIC connections. This maps to the standard QUIC `max_idle_timeout` transport parameter. A connection will be closed if no traffic is sent or received within this timeframe. Use a higher value for applications that require long-lived idle connections (e.g., long-polling, WebSockets over HTTP/3).

## VirtualHost Configuration

### Port Detection

The module automatically detects the port from the VirtualHost configuration:

```apache
# HTTP/3 will listen on port 8443
<VirtualHost *:8443>
    ServerName secure.example.com
    H3CertificatePath /etc/httpd/ssl/secure.crt
    H3CertificateKeyPath /etc/httpd/ssl/secure.key
</VirtualHost>
```

Use `H3Port` to bind the QUIC listener to a different UDP port than the VirtualHost's TCP port.

### Multiple VirtualHosts

The module uses the **first VirtualHost** that has both `H3CertificatePath` and `H3CertificateKeyPath` configured:

```apache
# This VirtualHost is used for HTTP/3
<VirtualHost *:4433>
    ServerName primary.example.com
    H3CertificatePath /etc/httpd/ssl/primary.crt
    H3CertificateKeyPath /etc/httpd/ssl/primary.key
</VirtualHost>

# This VirtualHost is ignored for HTTP/3
<VirtualHost *:4433>
    ServerName secondary.example.com
    H3CertificatePath /etc/httpd/ssl/secondary.crt
    H3CertificateKeyPath /etc/httpd/ssl/secondary.key
</VirtualHost>
```

### Alt-Svc Header

mod_http3 injects the `Alt-Svc` response header automatically when HTTP/3 is configured (controlled by [`H3AltSvc`](#h3altsvc), on by default):

```
Alt-Svc: h3=":4433"; ma=86400; persist=1
```

| Field | Meaning |
|---|---|
| `h3=":4433"` | HTTP/3 available on same host, on the `H3Port` |
| `ma=86400` | Advertise for `H3AltSvcMaxAge` seconds |
| `persist=1` | Persist across network changes |

To customize the header beyond `H3AltSvcMaxAge`, set it manually via `mod_headers`; the module does not overwrite an already-present `Alt-Svc` header:

```apache
Header always set Alt-Svc "h3=\":4433\"; ma=60; persist=1"
```

Disable the advertisement entirely with `H3AltSvc off`.

### EnableMMAP

`EnableMMAP` is fully supported. The output filter accepts and processes both raw file and memory-mapped (`MMAP`) data buckets transparently.

## Troubleshooting

### Startup Validation

The module validates configuration during Apache startup:

1. **Certificate Path Check:** `H3CertificatePath` is configured
2. **Key Path Check:** `H3CertificateKeyPath` is configured

If either check fails, Apache refuses to start.

### Testing Configuration

```sh
httpd -t                              # test syntax
httpd -t -D DUMP_VHOSTS               # verbose
httpd -M | grep http3                 # check module
```

### Verifying HTTP/3 Operation

```sh
ss -ulnp | grep httpd                 # check UDP listener
curl --http3 -k https://localhost:4433/   # test transfer
tail -f /var/log/httpd/error_log      # watch logs
```

### Debug Logging

```apache
LogLevel http3:trace8
```

### Log Messages

```
# Successful configuration
h3_post_config: pid=[PID] cert=/path/to/cert key=/path/to/key h3_port=443 mpm=event threaded=1 forked=2 max_threads=25

# Worker thread started
h3_child_init
worker_thread_main

# Errors
mod_http3: H3CertificatePath directive is required but not configured
mod_http3: H3CertificateKeyPath directive is required but not configured
```

### Security

```sh
# Set restrictive permissions
chmod 600 /etc/httpd/ssl/server.key
chown root:root /etc/httpd/ssl/server.key

# Or if Apache runs as a different user
chown apache:apache /etc/httpd/ssl/server.key
```
