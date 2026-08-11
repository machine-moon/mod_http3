# Container

Run Apache httpd with mod_http3 in a container. The container builds everything from source and serves HTTP/3 on port 8443.

Commands use `podman`/`podman compose`. Substitute `docker`/`docker compose` - flags and compose format are identical.

## Files

```
container/
  Containerfile.linux      Builds OpenSSL, APR, httpd and the module on Debian trixie.
  Containerfile.windows    Packages a host-built server tree into Server Core.
  compose.yml              Builds and runs the Linux image (profile `linux`),
                           or runs the published Windows image (profile `windows`).
  entrypoint-linux.sh      Mints a certificate when none is mounted, then execs httpd.
  entrypoint-windows.ps1   Mints a certificate, relays the logs to stdout, runs httpd.
  httpd-linux.conf         Server configuration, baked in and mountable over.
  httpd-windows.conf       Server configuration with Windows paths.
  certs/                   TLS certificate and key, minted by scripts/mkcert.sh.
  static/                  Document root, baked into htdocs and mountable over.
```

## Quick Start

The published image carries the configuration, the demo site and a certificate
it mints on first start, so one command gives you a live HTTP/3 server:

```sh
podman run --rm -p 8443:8443/udp ghcr.io/machine-moon/mod_http3:latest
curl --http3-only -k -sS https://localhost:8443/ -o /dev/null -w '%{http_version}\n'
```

HTTP/3 is UDP, so the mapping needs `/udp`; add `-p 8443:8443` as well for
HTTP/1.1 and HTTP/2 on TCP. `-e H3_PORT=8888` moves the port without mounting
anything.

Tags are `:latest` from trunk, `:<commit sha>` for any trunk build, and `:X.Y.Z`
for a release. Each also comes as `-linux` and `-windows`; the unsuffixed tag is
Linux. The certificate is self-signed and minted on every start, so mount your
own for anything that outlives a demo.

See [docs/containers.md](../docs/containers.md) for the full guide.

## Building it yourself

Generate certificates:

```sh
bash scripts/mkcert.sh container/certs
```

Build and start:

```sh
cd container
podman compose --profile linux up -d --build
```

Every service sits behind a profile, so a bare `compose up` starts nothing. A
daemon serves one container OS: `--profile windows` runs the published Windows
image rather than building one; see
[docs/containers.md](../docs/containers.md#the-windows-image) for why.

Wait for health check:

```sh
podman compose --profile linux ps
# STATUS: healthy (after start_period)
```

Test:

```sh
curl --http3 -k -sI https://localhost:8443/
```

Stop:

```sh
podman compose --profile linux down -v
```

## httpd-linux.conf Key Directives

```apache
LoadModule http3_module modules/mod_http3.so
EnableMMAP Off
Listen 8443 https

<VirtualHost *:8443>
    SSLEngine on
    Protocols h3

    H3CertificatePath    conf/certs/server.crt
    H3CertificateKeyPath conf/certs/server.key
    H3Port               8443
</VirtualHost>
```

## Further Testing

For detailed testing with curl, see [docs/testing-with-curl.md](../docs/testing-with-curl.md).

## Troubleshooting

```sh
podman logs mod_http3_dev
```

| Error | Cause | Fix |
|---|---|---|
| `Cannot load .../mod_http3.so` | Build failed | Check build output |
| `Invalid command 'H3CertificatePath'` | Module not loaded | Verify LoadModule line |
| `Permission denied` | SELinux | Add `:Z` to volume mounts |
| HTTP/3 not working but HTTP/2 is | UDP port not mapped | Check `podman port mod_http3_dev` |

Force clean rebuild:

```sh
podman compose --profile linux build --no-cache
```
