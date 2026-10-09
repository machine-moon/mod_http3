# Architecture

`mod_http3` adds a QUIC/HTTP/3 path to Apache httpd while retaining Apache's request processing, virtual hosts, filters, and configuration model.

```mermaid
flowchart LR
    Client[HTTP/3 client] -->|UDP QUIC + TLS 1.3| Engine[OpenSSL QUIC]
    Engine --> nghttp3[nghttp3 HTTP/3]
    nghttp3 --> Module[mod_http3]
    Module --> httpd[Apache httpd request pipeline]
    httpd --> Module
    Module --> nghttp3
    nghttp3 --> Engine
```

## Layers

- **OpenSSL** owns transport and TLS 1.3: packets, loss recovery, streams and
  the handshake. See [The QUIC layer](#the-quic-layer).
- **nghttp3** handles HTTP/3 frames, streams, and QPACK interactions.
- **mod_http3** bridges QUIC streams with Apache request/response processing.
- **Apache httpd** supplies routing, virtual-host selection, filters, and handlers.
- **APR and APR-util** provide the portable runtime services used by the module and host daemon.

## The QUIC layer

Every OpenSSL QUIC call the module makes lives under `quic/`, behind symbols
prefixed `h3q_`. This is a wrapper, not an abstraction. There is one transport,
OpenSSL's, and the layer exists to keep `SSL*`, `BIO*` and the QUIC listener
out of the rest of the module, not to allow a second implementation.

Nothing outside `quic/` includes an OpenSSL header, and the published API
documentation excludes both `detail/` directories.

The module hands over one `h3q_config`, holding the certificate path, the key
path, and whether to validate client addresses with a Retry packet, and gets
back an opaque `h3q_engine`. Connections and streams are opaque too, so
`<openssl/ssl.h>` stays out of every header above this layer. Failures come
back through a caller-supplied error buffer rather than the log, because
`quic/` has no `server_rec` to log against.

nghttp3 sits above this layer and never sees it. One behaviour is worth
knowing: OpenSSL exposes no per-stream acknowledgements, so the module counts
bytes as acknowledged once `SSL_write_ex` accepts them.

## Important Boundaries

HTTP/3 connections are UDP/QUIC connections, but request processing runs through standard Apache machinery. HTTP/3 is advertised over existing TCP responses using `Alt-Svc`; clients then establish QUIC on the advertised UDP port.

The module's listener presents the certificate of the first VirtualHost that serves HTTP/3 (`h3` in `Protocols` on a host with a mod_ssl certificate); the certificate is loaded in `post_config`, before privileges drop. Name-based virtual host selection then uses the request authority. IP-based virtual hosts remain unsupported because the necessary per-connection local address is not currently recovered.

See the [configuration guide](configuration.md) for operational control points.
