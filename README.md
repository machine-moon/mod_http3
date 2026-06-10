# mod_http3 -- HTTP/3 module for Apache httpd

An Apache httpd module that adds HTTP/3 support over QUIC.

Status: **experimental**.

## Build

Default build compiles all dependencies (OpenSSL, APR, APR-util, httpd) from submodules:

```sh
git submodule update --init --recursive
cmake -B build
cmake --build build -j$(nproc)
```

Output: `build/lib/mod_http3.so`

To use system-installed dependencies instead, provide `WITH_*` paths:

```sh
cmake -B build -DWITH_SSL=/opt/openssl -DWITH_HTTPD=/opt/httpd
cmake --build build -j$(nproc)
```

See [INSTALL](INSTALL) for full build instructions.

## CMake Options

| Option | Default | Description |
|---|---|---|
| `CMAKE_BUILD_TYPE` | `Release` | `Debug`, `Release` |
| `BUILD_MODULE` | `ON` | Build `mod_http3.so` |
| `BUILD_EXAMPLES` | `ON` | Build example programs |
| `BUILD_TESTS` | `ON` | Build test suite |
| `WITH_SSL` | (empty) | Path to OpenSSL prefix (overrides source build) |
| `WITH_HTTPD` | (empty) | Path to httpd prefix (overrides source build) |
| `WITH_APR` | (empty) | Path to APR prefix (overrides source build) |
| `WITH_APU` | (empty) | Path to APR-util prefix (overrides source build) |
| `ENABLE_ASAN` | `OFF` | Address Sanitizer (requires `Debug`) |
| `ENABLE_UBSAN` | `OFF` | UB Sanitizer (requires `Debug`) |
| `ENABLE_WERROR` | `OFF` | Treat warnings as errors |

See [CONFIGURATION.md](CONFIGURATION.md) for advanced options and dependency internals.

## Deploy

```sh
cmake --install build --prefix /opt/mod_http3
```

Installed files:

```
lib64/httpd/modules/mod_http3.so
etc/httpd/conf.modules.d/10-h3.conf
share/doc/mod_http3/    (CHANGES, NOTICE, AUTHORS)
share/licenses/mod_http3/LICENSE
```

Minimal VirtualHost configuration:

```apache
LoadModule ssl_module     modules/mod_ssl.so
LoadModule headers_module modules/mod_headers.so
LoadModule http3_module   modules/mod_http3.so

EnableMMAP Off
Listen 4433 https

<VirtualHost *:4433>
    ServerName localhost

    SSLEngine on
    SSLCertificateFile    conf/server.crt
    SSLCertificateKeyFile conf/server.key

    H3CertificatePath    conf/server.crt
    H3CertificateKeyPath conf/server.key

    Header always set Alt-Svc "h3=\":4433\"; ma=60; persist=1"

    DocumentRoot htdocs
    <Directory htdocs>
        Require all granted
    </Directory>
</VirtualHost>
```

See [INSTALL](INSTALL) for complete deployment steps.

## Documentation

| File | Contents |
|---|---|
| [INSTALL](INSTALL) | Build, deploy, verify |
| [CONFIGURATION.md](CONFIGURATION.md) | Advanced build options, dependency management |
| [CONFIGURATION_HTTPD.md](CONFIGURATION_HTTPD.md) | httpd directives, VirtualHost, troubleshooting |
| [docs/testing-with-curl.md](docs/testing-with-curl.md) | HTTP/3 testing with curl |
| [docs/testing-examples.md](docs/testing-examples.md) | Example programs |
| [container/README.md](container/README.md) | Container-based testing |

## License

Apache License 2.0. See [LICENSE](LICENSE) and [NOTICE](NOTICE).
