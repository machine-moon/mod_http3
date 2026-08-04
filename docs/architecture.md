# Architecture

`mod_http3` adds a QUIC/HTTP/3 path to Apache httpd while retaining Apache's request processing, virtual hosts, filters, and configuration model.

```mermaid
flowchart LR
    Client[HTTP/3 client] -->|UDP QUIC + TLS 1.3| Engine[QUIC engine\nOpenSSL or ngtcp2]
    Engine --> nghttp3[nghttp3 HTTP/3]
    nghttp3 --> Module[mod_http3]
    Module --> httpd[Apache httpd request pipeline]
    httpd --> Module
    Module --> nghttp3
    nghttp3 --> Engine
```

## Layers

- **The QUIC engine** owns transport: packets, loss recovery, and streams. It is
  chosen when the module is built, and is either OpenSSL's own QUIC
  implementation or ngtcp2. See [QUIC engines](#quic-engines).
- **OpenSSL** owns TLS 1.3 on both paths; ngtcp2 uses it through
  `libngtcp2_crypto_ossl`, so there is only ever one TLS stack.
- **nghttp3** handles HTTP/3 frames, streams, and QPACK interactions.
- **mod_http3** bridges QUIC streams with Apache request/response processing.
- **Apache httpd** supplies routing, virtual-host selection, filters, and handlers.
- **APR and APR-util** provide the portable runtime services used by the module and host daemon.

## QUIC engines

The transport sits behind one internal interface, `quic/quic/include/quic.h`.
Which engines a build contains is decided at compile time; which one runs is
decided at start-up.

```sh
cmake -B build                              # OpenSSL only (default)
cmake -B build -DENABLE_NGTCP2=ON           # OpenSSL and ngtcp2
```

```apache
H3QuicEngine ngtcp2                         # default: openssl
```

Naming an engine the build does not contain is a fatal configuration error, so
httpd refuses to start rather than quietly falling back. A running server
reports the engine in use through the `http3-status` handler's `quic_backend`
field. OpenSSL provides TLS on both paths, so there is only ever one TLS stack.

Each engine lives in `quic/<name>/`, exposing `quic_<name>_api()` from
`quic/<name>/include/quic_<name>.h` and keeping its own types in `src/detail/`.
Adding one means creating that directory, an `add_subdirectory()` line in
`quic/CMakeLists.txt`, and a row in `quic_engines[]` in
`mod_http3/src/h3_quic.c`; the engine appends its sources, include directory
and transport library to the `mod_http3-quic` target itself.

That library depends on nothing but OpenSSL and each engine's own transport, so
no APR type, httpd type or module symbol appears anywhere under `quic/`. The
module passes one `quic_config`: a `quic_cred` naming a certificate by path or
by PEM buffer, a `quic_settings` carrying RFC 9000 transport parameters, a
`quic_callbacks` table of events, and a `quic_io` saying how datagrams move
— so the contract names no socket, and `quic_io_udp_init()` supplies the
ordinary UDP implementation. Engines report failures through an error buffer
rather than logging, and `quic/quic/src/quic_tls.c` builds the one TLS context every
engine serves from. Because the transport libraries are linked privately,
ngtcp2's headers stay off the include path of every translation unit outside
`quic/`.

An engine maps what it can of `quic_settings` and documents the rest in a
`@note` on its ops table. Selection lives in `quic/quic/src/quic_registry.c`, so
`quic/quic/include/quic.h` is the only header a caller includes and every
`quic_<engine>.h` is private to the library: callers name an engine with
`quic_select()` and list what a build offers with `quic_engine_count()` and
`quic_engine_name_at()`. `quic/null/` implements the whole contract and carries
nothing; it is the standing proof that adding a backend touches its own
directory, one `add_subdirectory()` and one registry row — all inside `quic/`.

nghttp3 sits above the interface and is unaffected by the choice. The engines
differ in one behaviour worth knowing: OpenSSL exposes no per-stream
acknowledgements, so the module counts bytes as acknowledged once OpenSSL
accepts them, while ngtcp2 reports real ones. Response buffers are therefore
released later, and more accurately, on ngtcp2.

## Important Boundaries

HTTP/3 connections are UDP/QUIC connections, but request processing runs through standard Apache machinery. HTTP/3 is advertised over existing TCP responses using `Alt-Svc`; clients then establish QUIC on the advertised UDP port.

The module uses the first VirtualHost with both `H3CertificatePath` and `H3CertificateKeyPath` for its listener. Name-based virtual host selection then uses the request authority. IP-based virtual hosts remain unsupported because the necessary per-connection local address is not currently recovered by either engine.

See the [configuration guide](configuration.md) for operational control points.
