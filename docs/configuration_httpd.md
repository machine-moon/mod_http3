# mod_http3 httpd Configuration

Apache httpd configuration directives for mod_http3.

For build and installation, see [INSTALL](../INSTALL).

## Overview

mod_http3 enables HTTP/3 protocol support in Apache HTTP Server. The module:

- Creates a separate worker thread for HTTP/3 connections over UDP/QUIC
- Uses OpenSSL for QUIC/TLS 1.3 support
- Uses nghttp3 for HTTP/3 protocol handling
- Integrates with Apache's standard request processing pipeline

## Configuration Directives

### Enabling HTTP/3 on a host

A VirtualHost serves HTTP/3 when `h3` is in its `Protocols` and mod_ssl has a
certificate for it -- the same two things mod_http2 needs for `h2`:

```apache
<VirtualHost *:443>
    ServerName www.example.com
    SSLEngine on
    SSLCertificateFile    /etc/httpd/ssl/www.crt
    SSLCertificateKeyFile /etc/httpd/ssl/www.key
    Protocols h3 h2 http/1.1
</VirtualHost>
```

The certificate and key mod_ssl resolved for the host -- `SSLCertificateFile`
pairs and anything mod_md manages -- are loaded for QUIC as well, during
startup while httpd still runs privileged, so a key readable only by root works
as it does for mod_ssl. A host with `h3` in `Protocols` but no mod_ssl
certificate (`SSLEngine off`, or mod_ssl not loaded) does not serve HTTP/3.

### H3Port

**Syntax:** `H3Port port`
**Context:** server config, virtual host
**Default:** the port of the VirtualHost that configured HTTP/3

UDP port the QUIC listener binds to. When unset, the module reuses the port of the VirtualHost that serves HTTP/3, so TCP (HTTP/1.1, HTTP/2) and UDP (HTTP/3) share the same port number. Set it explicitly to serve HTTP/3 on a different port.

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

### H3SocketBufferSize

**Syntax:** `H3SocketBufferSize bytes`
**Context:** server config, virtual host
**Default:** `2097152`

Bytes requested for the QUIC socket's send and receive buffers (`SO_SNDBUF` and `SO_RCVBUF`). The operating system caps what it grants -- on Linux through `net.core.wmem_max` and `net.core.rmem_max` -- and a refused or capped request is logged at `info` level rather than treated as an error, so raising this alone may not take effect. A receive buffer left at the OS default overflows once a single connection runs at speed, and every dropped datagram costs a retransmit and a congestion-window reduction.

### H3StreamBufferSize

**Syntax:** `H3StreamBufferSize bytes`
**Context:** server config, virtual host
**Default:** `65536`

Per-stream request and streaming-response buffer size in bytes. For responses,
this bounds the producer/consumer queue between Apache request workers and the
QUIC event thread; a full queue applies backpressure to the request worker.

### H3MaxRequestBodySize

**Syntax:** `H3MaxRequestBodySize bytes`
**Context:** server config, virtual host
**Default:** `10485760` (10 MiB)

Maximum HTTP/3 request body size in bytes. Request bodies are fully buffered in memory; requests exceeding the limit are rejected.

### H3MaxResponseBodySize

**Syntax:** `H3MaxResponseBodySize bytes`
**Context:** server config, virtual host
**Default:** unlimited

Maximum HTTP/3 response body size in bytes. By default, responses stream through
the bounded queue controlled by `H3StreamBufferSize` and are not retained in
full. Setting an explicit finite limit switches that virtual host to bounded
whole-response buffering, allowing an over-limit response to be replaced with
a `500 Internal Server Error` before its headers or partial body are sent.

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

### H3SessionTickets

**Syntax:** `H3SessionTickets on|off`
**Context:** server config, virtual host
**Default:** `on`

Whether to issue TLS 1.3 session tickets. A returning client that presents a ticket resumes its session and skips a certificate verification, which is the difference between a two-round-trip and a one-round-trip reconnect. The ticket keys are created before httpd forks, so every child process resumes tickets issued by any other; a ticket from before a restart falls back to a full handshake. Turn this off to force a full handshake on every connection.

### H3AddressValidation

**Syntax:** `H3AddressValidation on|off`
**Context:** server config, virtual host
**Default:** `on`

Whether to validate a client's source address before accepting a connection. When on, the server answers each new connection with a QUIC Retry packet (RFC 9000 section 8.1.2) and completes the handshake only after the client echoes the token back, which proves the client can receive at the address it claims. This is the defence against address-spoofed amplification attacks.

Turning it off removes one round trip from every connection, at the cost of that protection. Leave it on for internet-facing deployments. It exists mainly for interoperability testing, where a test may require a handshake that completes without an intervening Retry.

### http3-status

Runtime counters as JSON. The handler is not mapped anywhere by default; give
it a location first:

```apache
<Location /http3-status>
    SetHandler http3-status
</Location>
```

### H3StreamTimeout

**Syntax:** `H3StreamTimeout seconds`
**Context:** server config, virtual host
**Default:** the server's `Timeout`

How long a response may make no progress before it is abandoned. A client that opens a stream and then stops reading otherwise leaves the request worker blocked on the per-stream response queue indefinitely: the QUIC connection stays alive on keepalives and the idle enforcement in [`H3IdleTimeout`](#h3idletimeout) deliberately skips a connection that still has a request running. The window is measured per chunk of progress, not over the whole response, so a slow but advancing transfer is never cut off. Equivalent to mod_http2's `H2StreamTimeout`.

The deadline is evaluated on the QUIC event thread, which already holds the connection lock, so a blocked worker does not have to win that lock to be released. Note that a client which keeps making a little progress resets the window on every chunk, so this bounds a stalled transfer rather than a slow one.

### H3MaxStreamErrors

**Syntax:** `H3MaxStreamErrors n`
**Context:** server config, virtual host
**Default:** `8`

How many client-caused stream errors one connection may produce before it is closed with `H3_EXCESSIVE_LOAD`. A malformed request is answered as a stream error so the connection keeps serving its other streams (RFC 9114 section 4.1.2), which on its own would let a client send malformed requests indefinitely at no cost. Equivalent to mod_http2's `H2MaxStreamErrors`.

### H3QpackTableCapacity

**Syntax:** `H3QpackTableCapacity bytes`
**Context:** server config, virtual host
**Default:** `4096`

Bytes of QPACK dynamic table the server allows a client to use when encoding request header fields, advertised as `SETTINGS_QPACK_MAX_TABLE_CAPACITY`. With `0` the client may not use the dynamic table at all and must send every field literally, so repeated requests re-send their cookies and `User-Agent` in full. A larger table trades memory per connection for smaller requests.

### H3QpackBlockedStreams

**Syntax:** `H3QpackBlockedStreams n`
**Context:** server config, virtual host
**Default:** `0`

How many requests may wait on a QPACK dynamic table insert that has not arrived yet, advertised as `SETTINGS_QPACK_BLOCKED_STREAMS`. `0`, the default, forbids blocking; a client may still use the dynamic table, but only for entries the server has already acknowledged. Only meaningful when [`H3QpackTableCapacity`](#h3qpacktablecapacity) is non-zero.

Raise this only for a trusted client population. While a stream is blocked, nghttp3 buffers everything the client sends on it without bound, and neither [`H3MaxRequestBodySize`](#h3maxrequestbodysize) nor any other request limit applies to those bytes, so each permitted blocked stream is memory a client can grow at will.

### H3MinWorkers

**Syntax:** `H3MinWorkers n`
**Context:** server config, virtual host
**Default:** `16`

Request worker threads started per child process. HTTP/3 requests are dispatched to this pool rather than handled on the QUIC event thread.

### H3MaxWorkers

**Syntax:** `H3MaxWorkers n`
**Context:** server config, virtual host
**Default:** `64`

Maximum request worker threads per child process. This caps how many HTTP/3 requests one child can process at once, independently of the MPM's own thread settings. A value below [`H3MinWorkers`](#h3minworkers) is raised to match it, with a warning.

### H3MaxWorkerIdleSeconds

**Syntax:** `H3MaxWorkerIdleSeconds seconds`
**Context:** server config, virtual host
**Default:** `600`

How long an idle request worker is kept before it exits, letting the pool shrink back towards [`H3MinWorkers`](#h3minworkers) after a burst. Equivalent to mod_http2's `H2MaxWorkerIdleSeconds`.

## VirtualHost Configuration

### Port Detection

The module automatically detects the port from the VirtualHost configuration:

```apache
# HTTP/3 will listen on port 8443
<VirtualHost *:8443>
    ServerName secure.example.com
    SSLEngine on
    SSLCertificateFile    /etc/httpd/ssl/secure.crt
    SSLCertificateKeyFile /etc/httpd/ssl/secure.key
    Protocols h3 h2 http/1.1
</VirtualHost>
```

Use `H3Port` to bind the QUIC listener to a different UDP port than the VirtualHost's TCP port.

### Multiple VirtualHosts

The QUIC listener presents the certificate of the **first VirtualHost** that serves HTTP/3; every other HTTP/3 host still advertises `Alt-Svc` and is selected by request authority:

```apache
# This VirtualHost's certificate is the one QUIC presents
<VirtualHost *:4433>
    ServerName primary.example.com
    SSLEngine on
    SSLCertificateFile    /etc/httpd/ssl/primary.crt
    SSLCertificateKeyFile /etc/httpd/ssl/primary.key
    Protocols h3 h2 http/1.1
</VirtualHost>

# Served over HTTP/3 too, but with primary's certificate
<VirtualHost *:4433>
    ServerName secondary.example.com
    SSLEngine on
    SSLCertificateFile    /etc/httpd/ssl/secondary.crt
    SSLCertificateKeyFile /etc/httpd/ssl/secondary.key
    Protocols h3 h2 http/1.1
</VirtualHost>
```

### TLS Environment Variables

mod_ssl does not manage HTTP/3 connections, so it publishes no TLS environment for them. The module supplies the following under the same names mod_ssl uses, so existing CGI scripts, `mod_rewrite` conditions and log formats keep working over HTTP/3:

| Variable | Example | Notes |
| --- | --- | --- |
| `HTTPS` | `on` | Always set for HTTP/3 requests |
| `SSL_PROTOCOL` | `TLSv1.3` | QUIC requires TLS 1.3, so this is always `TLSv1.3` |
| `SSL_CIPHER` | `TLS_AES_128_GCM_SHA256` | Negotiated cipher suite |
| `SSL_CIPHER_USEKEYSIZE` | `128` | Key bits actually used |
| `SSL_CIPHER_ALGKEYSIZE` | `128` | The algorithm's full strength |
| `SSL_CIPHER_EXPORT` | `false` | Always `false`; TLS 1.3 has no export ciphers |
| `SSL_SESSION_RESUMED` | `Initial` | `Resumed` when the client presented a session ticket |

These are set for every HTTP/3 request, without needing `SSLOptions +StdEnvVars`, and are computed once per connection. Certificate-derived variables (`SSL_SERVER_*`, `SSL_CLIENT_*`) and `SSL_SESSION_ID` are not published; HTTP/3 requests do not use client certificates in this module.

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

1. At least one host serves HTTP/3: `h3` in `Protocols` on an `SSLEngine on` host
2. That host's certificate and key load

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
mod_http3: serving HTTP/3 with mod_ssl certificate /path/to/cert
h3_post_config: pid=[PID] h3_port=443 mpm=event threaded=1 forked=2 max_threads=25

# Worker thread started
h3_child_init
worker_thread_main

# Errors
mod_http3: no host serves HTTP/3: add h3 to Protocols on a host with SSLEngine on
mod_http3: loading certificate /path/to/cert with key /path/to/key failed: ...
```

### Security

```sh
# Set restrictive permissions
chmod 600 /etc/httpd/ssl/server.key
chown root:root /etc/httpd/ssl/server.key

# Or if Apache runs as a different user
chown apache:apache /etc/httpd/ssl/server.key
```
