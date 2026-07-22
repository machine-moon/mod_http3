# mod_http3 -- HTTP/3 module for Apache httpd

An Apache httpd module that adds HTTP/3 support over QUIC.

Status: **experimental**.

## Build

Default build compiles all dependencies (OpenSSL, APR, APR-util, httpd) from submodules:

```sh
git submodule update --init --recursive
cmake -B build
cmake --build build
```

Output: `build/lib/mod_http3.so`

To use system-installed dependencies instead, provide `WITH_*` paths:

```sh
cmake -B build -DWITH_SSL=/opt/openssl -DWITH_HTTPD=/opt/httpd
cmake --build build
```

See [INSTALL](INSTALL) for full build instructions.

## CMake Options

| Option | Default | Description |
|---|---|---|
| `CMAKE_BUILD_TYPE` | `Debug` | `Debug`, `Release` |
| `BUILD_MODULE` | `ON` | Build `mod_http3.so` |
| `BUILD_EXAMPLES` | `ON` | Build example programs |
| `BUILD_TESTS` | `ON` | Build test suite |
| `WITH_SSL` | (empty) | Path to OpenSSL prefix (overrides source build) |
| `WITH_HTTPD` | (empty) | Path to httpd prefix (overrides source build) |
| `WITH_APR` | (empty) | Path to APR prefix (overrides source build) |
| `WITH_APU` | (empty) | Path to APR-util prefix (overrides source build) |
| `WITH_NGHTTP3` | (empty) | Path to nghttp3 prefix (overrides source build) |
| `ENABLE_ASAN` | `OFF` | Address Sanitizer (requires `Debug`) |
| `ENABLE_UBSAN` | `OFF` | UB Sanitizer (requires `Debug`) |
| `ENABLE_WERROR` | `OFF` | Treat warnings as errors |

See [Build Configuration](docs/configuration.md) for advanced options and dependency internals.

## Deploy

```sh
cmake --install build --prefix /opt/mod_http3
```

Minimal VirtualHost configuration:

```apache
LoadModule ssl_module     modules/mod_ssl.so
LoadModule http3_module   modules/mod_http3.so

EnableMMAP Off
Listen 4433 https

<VirtualHost *:4433>
    ServerName localhost

    SSLEngine on
    SSLCertificateFile    conf/server.crt
    SSLCertificateKeyFile conf/server.key

    Protocols h3 h2 http/1.1

    H3CertificatePath     conf/server.crt
    H3CertificateKeyPath  conf/server.key

    DocumentRoot htdocs
    <Directory htdocs>
        Require all granted
    </Directory>
</VirtualHost>
```

HTTP/3 support is advertised to clients automatically via an `Alt-Svc` response header (`H3AltSvc`, on by default). See [httpd Configuration](docs/configuration_httpd.md) for all directives and [INSTALL](INSTALL) for complete deployment steps.

## Contributing

We welcome contributions of all forms, including bug reports, documentation updates, code changes, and release testing. For guidelines on our community governance, voting procedures, and coding style, please refer to [CONTRIBUTING.md](CONTRIBUTING.md).

## License

Apache License 2.0. See [LICENSE](LICENSE) and [NOTICE](NOTICE).
