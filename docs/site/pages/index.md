# mod_http3

## HTTP/3 for Apache httpd

`mod_http3` is an Apache httpd module that serves HTTP/3 over QUIC. It integrates with the standard httpd request pipeline while adding a UDP/QUIC listener, TLS 1.3 handling through OpenSSL, and HTTP/3 framing through nghttp3.

The module advertises HTTP/3 with `Alt-Svc` by default, allowing compatible clients to discover the UDP endpoint from a TCP response.

## Start Here

1. [Build](build.md) the module and its pinned dependencies.
2. [Deploy](deploy.md) it into a custom httpd installation.
3. [Verify](verify.md) the UDP listener, module load, and an HTTP/3 request.
4. Read the [Directive Guide](configuration.md) before setting production limits.

## Status

Configuration and C API may change between releases. Read the [versioning policy](https://github.com/machine-moon/mod_http3/blob/trunk/VERSIONING) and [release process](releases.md) before upgrading.

## Primary Components

| Component | Responsibility |
| --- | --- |
| Apache httpd | Request routing, virtual hosts, filters, and module hosting |
| OpenSSL 3.5+ | QUIC transport and TLS 1.3 |
| nghttp3 | HTTP/3 framing and stream state |
| APR / APR-util | Portable threads, pools, and sockets |
