# Architecture

`mod_http3` adds a QUIC/HTTP/3 path to Apache httpd while retaining Apache's request processing, virtual hosts, filters, and configuration model.

```mermaid
flowchart LR
    Client[HTTP/3 client] -->|UDP QUIC + TLS 1.3| OpenSSL[OpenSSL QUIC]
    OpenSSL --> nghttp3[nghttp3 HTTP/3]
    nghttp3 --> Module[mod_http3]
    Module --> httpd[Apache httpd request pipeline]
    httpd --> Module
    Module --> nghttp3
    nghttp3 --> OpenSSL
```

## Layers

- **OpenSSL** owns QUIC transport and TLS 1.3.
- **nghttp3** handles HTTP/3 frames, streams, and QPACK interactions.
- **mod_http3** bridges QUIC streams with Apache request/response processing.
- **Apache httpd** supplies routing, virtual-host selection, filters, and handlers.
- **APR and APR-util** provide the portable runtime services used by the module and host daemon.

## Important Boundaries

HTTP/3 connections are UDP/QUIC connections, but request processing runs through standard Apache machinery. HTTP/3 is advertised over existing TCP responses using `Alt-Svc`; clients then establish QUIC on the advertised UDP port.

The module uses the first VirtualHost with both `H3CertificatePath` and `H3CertificateKeyPath` for its listener. Name-based virtual host selection then uses the request authority. IP-based virtual hosts remain unsupported because the necessary per-connection local address is unavailable from the active OpenSSL integration.

See the [configuration guide](configuration.md) for operational control points.
