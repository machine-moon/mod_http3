# Operational Limits

HTTP/3 request and response bodies are buffered by the module. Set limits according to the memory available to each httpd child and the maximum concurrency you accept.

| Directive | Default | Effect |
| --- | --- | --- |
| `H3MaxConnections` | `256` | Refuses new QUIC connections after the per-child limit |
| `H3MaxConcurrentStreams` | `100` | Caps in-flight requests per connection |
| `H3StreamBufferSize` | `65536` | Sets per-stream read/write buffer capacity |
| `H3SocketBufferSize` | `2097152` | Requests QUIC socket send/receive buffer size, capped by the OS |
| `H3MaxRequestBodySize` | `10485760` | Rejects request bodies above 10 MiB |
| `H3MaxResponseBodySize` | unlimited | Replaces excessive buffered responses with HTTP 500 when set |
| `H3HandshakeTimeout` | `10` seconds | Terminates incomplete QUIC/TLS handshakes |
| `H3IdleTimeout` | `300` seconds | Closes idle QUIC connections |
| `H3StreamTimeout` | the server `Timeout` | Abandons a response that makes no progress |
| `H3MaxStreamErrors` | `8` | Closes a connection whose client keeps causing stream errors |

## Response Body Limit

`H3MaxResponseBodySize` is unlimited by default for compatibility. Configure it to cap worst-case per-request memory. A response that exceeds the limit is replaced with `500 Internal Server Error`; the original body may already be partially generated, so its `Content-Length` cannot be trusted.

## Deployment Guidance

Start with conservative limits in exposed deployments. Test realistic download, upload, and concurrent-stream workloads before increasing connection or stream counts. Network-wide denial-of-service mitigation remains outside the module's scope.

The [security policy](security.md) describes the relevant trust boundaries and dependencies.
