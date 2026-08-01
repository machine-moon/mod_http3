# QUIC Interop Testing

The [QUIC Interop Runner](https://interop.seemann.io/quic) pairs every
registered QUIC implementation with every other one inside a network simulator
and reports a pass/fail cell per test case. mod_http3 takes part as a
**server**: there is no HTTP/3 client in this project, so mod_http3 is tested
against every client in the matrix.

## How it runs

[`.github/workflows/interop.yml`](https://github.com/machine-moon/mod_http3/blob/trunk/.github/workflows/interop.yml)
is a stage of the CI pipeline, so it runs on every push, on every branch, once
the module image has built. There is no local runner script: the
matrix needs a docker daemon, a `tshark` new enough to dissect QUIC, IPv6 on
the host and an hour of wall time, none of which belong in a developer loop.

The `endpoint` job builds `interop/Containerfile`, checks the image answers
`127` for a test case it does not implement, and publishes it as
`ghcr.io/machine-moon/mod_http3-interop:<commit sha>`.

Each `client` job then **pulls that tag back out of the registry** and runs the
matrix against it. Nothing is passed between jobs as a file, so the image the
matrix exercises is byte-for-byte the one the registry serves. The client list
comes from the runner's own `implementations_quic.json`, so a new peer joins
the matrix without a change here, and each job is named after the client it
tests — the check list reads as a per-client result matrix.

A release tags that same image `:X.Y.Z` and `:latest`, so
`mod_http3-interop:latest` always points at an endpoint whose matrix is public
and reproducible.

## Reading the results

Each client job writes its verdict to the workflow summary. A client that does
not implement the `http3` case reports a warning rather than a failure —
nothing reached mod_http3 — and a failing job keeps its logs as an artifact for
two weeks, laid out as `logs/<server>_<client>/<case>/`:

| Path | Contents |
|---|---|
| `output.txt` | Everything the runner, the endpoint and the client printed |
| `server/httpd_error.log` | mod_http3's own log at `LogLevel http3:debug` |
| `server/keys.log` | TLS secrets, for decrypting the pcaps in Wireshark |
| `sim/trace_node_*.pcap` | What actually crossed the simulated link |

A configuration error kills httpd before it opens `httpd_error.log`, so the
first failures of a broken endpoint are only visible on the `server  |` lines
of `output.txt`.

To reproduce a cell by hand, clone the
[runner](https://github.com/quic-interop/quic-interop-runner), add the
published image to its `implementations_quic.json` and run it — that is all the
`client` job does:

```sh
python run.py -s mod_http3 -c quic-go -t http3 -l logs -j results.json
```

## Test case support

`interop/run_endpoint.sh` exits 127 for any case the endpoint does not claim,
which the runner records as *unsupported* rather than failed. Today that is
every case except `http3`.

The runner moves files with **HTTP/0.9 over ALPN `hq-interop`** in all but one
test case — its own `quic.md` puts it as "unless noted otherwise, test cases use
HTTP/0.9 for file transfers" — and mod_http3 only speaks `h3`. A client running
`handshake` offers `hq-interop` alone, so the connection dies in the handshake
with `no_application_protocol` before any QUIC behaviour is exercised:

```
[http3:debug] mod_http3: ALPN: client did not offer h3
[http3:error] QUIC handshake did not complete: ... err=0x178
```

That is a protocol the module does not implement, not a QUIC or HTTP/3 defect.
nginx covers the same ground with a dedicated `http3_hq on` directive that
serves HTTP/0.9 over its HTTP/3 stack for this harness; an equivalent here would
open the other 21 cases. Until then `http3` is the honest claim, and it still
exercises the handshake, QPACK, parallel streams and flow control against every
client in the matrix.

## Troubleshooting

**Every case is unsupported.** The runner refuses an implementation that does
not exit 127 for an unknown test case, and it makes that check with no timeout,
so a hung endpoint hangs the run. The `endpoint` job pre-checks the same thing
with a timeout before any client job starts.

**Every case fails in analysis.** The runner replays the simulator's pcaps
through `tshark`; without 4.5.0 or newer, cases fail in analysis rather than on
the wire.

**The runner cannot start the endpoint.** Its compose file needs docker engine
28.1 or newer for `interface_name`, which is why the client jobs pin one.

**`chrome` reports "Expected exactly 1 handshake. Got: 2".** The browser opens a
second connection and the case demands one. It does the same against nginx, so
treat that cell as a property of the client rather than of the server.
