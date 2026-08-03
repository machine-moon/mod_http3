# Containers

Two images are published from this repository, and they are aimed at very
different people.

| Image | What it is for |
|---|---|
| `ghcr.io/machine-moon/mod_http3` | Running mod_http3. Pull it and you have an HTTP/3 server. |
| `ghcr.io/machine-moon/mod_http3-interop` | The endpoint the QUIC Interop Runner drives. Not meant to be run by hand. |

Commands below use `podman`. Substitute `docker` — the flags are identical.

## Run the server

Everything it needs is in the image: the configuration, a small demo site, and
a self-signed certificate it mints on first start.

```sh
podman run --rm --name mod_http3 -p 8443:8443/udp ghcr.io/machine-moon/mod_http3:latest
```

HTTP/3 runs over UDP, hence the `/udp`. A bare `-p 8443:8443` publishes TCP
only, which gives you a server that answers HTTP/1.1 and never completes a QUIC
handshake. Add `-p 8443:8443` alongside if you also want HTTP/1.1, HTTP/2 and
`Alt-Svc` discovery on TCP.

In another terminal:

```sh
curl --http3-only -k -sI https://localhost:8443/
```

```
HTTP/3 200
content-type: text/html
```

`--http3-only` refuses to fall back, so `HTTP/3` here proves QUIC carried it.
Your curl needs HTTP/3 support — `curl -V` must list `HTTP3` in its features.
Most distribution builds do not have it; see
[HTTP/3 testing with curl](https://github.com/machine-moon/mod_http3/blob/trunk/docs/testing-with-curl.md).

`-k` is needed because the certificate is self-signed. Mount your own to drop
it, as below.

## Use your own certificate

The generated certificate is regenerated on every start and is fine for a demo,
not for anything else. Mount a real one over the certificate directory:

```sh
bash scripts/mkcert.sh ./certs

podman run --rm -p 8443:8443/udp \
    -v ./certs:/src/dependencies/httpd-dist/conf/certs:ro \
    ghcr.io/machine-moon/mod_http3:latest
```

The httpd child runs as `daemon`, and mod_http3 opens the QUIC socket in that
child, so **the private key has to be readable by `daemon`**. `mkcert.sh` writes
it `0600`, which is right for a host install and wrong here:

```sh
chmod 0644 ./certs/server.key
```

If you skip that, httpd starts, the TCP listener works, and QUIC handshakes fail
with a permission error in the log.

## Serve your own content

```sh
podman run --rm -p 8443:8443/udp \
    -v ./public:/src/dependencies/httpd-dist/htdocs:ro \
    ghcr.io/machine-moon/mod_http3:latest
```

## Change the port

The baked configuration takes its port from `H3_PORT`, so moving it needs no
mount:

```sh
podman run --rm -e H3_PORT=8888 -p 8888:8888/udp ghcr.io/machine-moon/mod_http3:latest
```

## Change the configuration

The baked configuration is [`container/httpd.conf`](https://github.com/machine-moon/mod_http3/blob/trunk/container/httpd.conf).
Copy it, edit it, mount it back:

```sh
podman run --rm -p 8443:8443/udp \
    -v ./httpd.conf:/src/dependencies/httpd-dist/conf/httpd.conf:ro \
    ghcr.io/machine-moon/mod_http3:latest
```

Anything the baked configuration does not expose needs this — `H3_PORT` is the
only setting wired to an environment variable.

Every `H3*` directive is documented in
[httpd Directives](configuration_httpd.md).

The published image is built with both QUIC engines, so `H3QuicEngine ngtcp2`
in a mounted configuration switches the transport without rebuilding anything.
It defaults to `openssl`.

## Development with compose

Working on the module itself is easier with
[`container/compose.yml`](https://github.com/machine-moon/mod_http3/blob/trunk/container/compose.yml),
which builds from your checkout and mounts the config, certificates and content
over the baked ones:

```sh
bash scripts/mkcert.sh container/certs
cd container
podman compose up -d --build
podman compose ps          # wait for "healthy"
podman compose logs -f
podman compose down -v
```

A cold build takes about ten minutes — OpenSSL, APR, APR-util, nghttp3 and httpd
are all compiled from source.

## Which tag to pull

| Tag | Points at |
|---|---|
| `:latest` | The most recent build of `trunk` |
| `:X.Y.Z` | A release, retagged from the exact image that release was tested with |
| `:<commit sha>` | Any single trunk build, for pinning or bisecting |

Pin a version for anything reproducible:

```sh
podman pull ghcr.io/machine-moon/mod_http3:0.0.50
```

## The interop endpoint

`mod_http3-interop` is a different kind of image. It exists so the
[QUIC Interop Runner](https://interop.seemann.io/quic) can pull a mod_http3
server and pit it against other QUIC implementations. It takes no arguments and
reads its whole configuration from environment variables the runner injects.

Run it by hand and it tells you so:

```sh
podman run --rm ghcr.io/machine-moon/mod_http3-interop:latest
```

```
UNSUPPORTED ROLE <unset>
```

Exit code 127. That is the contract, not a fault — the runner requires an
endpoint to answer 127 for anything it does not implement, and uses that to
decide what to run.

What it is useful for is running the matrix without a local build: register the
tag with the [QUIC Interop Runner](https://github.com/quic-interop/quic-interop-runner)
and it pulls the endpoint itself.

Every commit is published under its own sha, and every release also lands as
`:X.Y.Z` and `:latest`. See [QUIC Interop Testing](interop.md) for what the
matrix means and how to read the results.

## Troubleshooting

**`curl` says the connection failed, or hangs.** Check the mapping says `/udp`.
`-p 8443:8443` publishes TCP only, and QUIC then has no path at all.

**`curl: option --http3-only: the installed libcurl was built without…`.** Your
curl has no HTTP/3 support. `curl -V | grep HTTP3` confirms it either way.

**HTTP/1.1 works but HTTP/3 does not.** Almost always certificate permissions —
see above. `podman logs mod_http3` shows the error from the child process.

**`Invalid command 'H3CertificatePath'`.** The configuration you mounted does not
load the module. It needs `LoadModule http3_module modules/mod_http3.so`.
