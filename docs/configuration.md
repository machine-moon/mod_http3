# Build Configuration

Advanced build options, dependency management, and build internals.

For quick start and deployment, see [INSTALL](../INSTALL).

For httpd runtime directives (`H3CertificatePath`, VirtualHost), see [httpd Configuration](configuration_httpd.md).

The Python suite honours `H3_QUIC_ENGINE`, so a build configured with
`-DENABLE_NGTCP2=ON` can be exercised on either transport:

```sh
H3_QUIC_ENGINE=ngtcp2 pytest test/http3
```

## Build Commands

| Command | Description |
|---|---|
| `cmake -B build` | Configure |
| `cmake --build build` | Build |
| `cmake --build build --target test` | Build + run unit tests |
| `cmake --build build --target pytest` | Run Python tests |
| `cmake --build build --target release` | Create generic Linux ZIP/TAR.GZ, DEB, and RPM packages |
| `cmake -LH -N -B build` | Print all cache variables |

```sh
# Module only (with system deps via WITH_SSL/WITH_HTTPD/WITH_NGHTTP3)
git submodule update --init --recursive dependencies/nghttp3
```

Reset submodules:

```sh
git submodule foreach --recursive 'git reset --hard || :'
git submodule foreach --recursive 'git clean -ffdx || :'
```

## Build from Source

By default, CMake builds all dependencies from their submodules at configure time. Results are installed into `dependencies/<dep>-dist/` and cached with sentinel files (`.done`).

Build order:

1. nghttp3 -> `dependencies/nghttp3-dist/`
2. OpenSSL -> `quic/third-party/openssl-dist/`
3. APR -> `dependencies/apr-dist/`
4. APR-util -> `dependencies/apr-util-dist/`
5. httpd -> `dependencies/httpd-dist/`

Force a rebuild:

```sh
rm -rf quic/third-party/openssl-dist
cmake -B build
```

## System Packages Build

Provide `WITH_*` variables to override individual dependencies with system-installed versions.

| Package | Override | Minimum |
|---|---|---|
| OpenSSL | `WITH_SSL=/path` | >= 3.5.0 |
| httpd (via apxs) | `WITH_HTTPD=/path` | >= 2.4.x AND MMN >= 20211221 |
| APR | `WITH_APR=/path` | >= 1.7.0 |
| APU | `WITH_APU=/path` | >= 1.6.0 |
| nghttp3 | `WITH_NGHTTP3=/path` | >= 1.17.0 |

> Distro-packaged httpd (Ubuntu, Fedora, etc.) ships with MMN < 20211221 and will fail configure. Use build-from-source mode instead.

```sh
cmake -B build -DWITH_SSL=/opt/openssl -DWITH_HTTPD=/opt/httpd
cmake --build build
```

If APR and APR-util are installed separately from httpd:

```sh
cmake -B build -DWITH_SSL=/opt/openssl -DWITH_HTTPD=/opt/httpd -DWITH_APR=/opt/apr -DWITH_APU=/opt/apr-util
cmake --build build
```

## Mixed Mode

Override individual dependencies while building the rest from source:

```sh
cmake -B build -DWITH_SSL=/opt/openssl
```

```sh
cmake -B build -DWITH_HTTPD=/opt/httpd
```

## Sanitizers

Both require `CMAKE_BUILD_TYPE=Debug`.

```sh
# ASan
rm -rf build
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON
cmake --build build --target tests

# UBSan
rm -rf build
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_UBSAN=ON
cmake --build build --target tests
```

## Dependency Internals

### Sentinel Files

Each dependency built from source writes `.done` to its output directory (e.g., `quic/third-party/openssl-dist/.done`). CMake checks for this file before rebuilding. Delete it to force rebuild.

### Build Logs

Build logs are written to `dependencies/<dep>-dist/logs/`:

- `openssl-dist/logs/openssl-configure.log`
- `openssl-dist/logs/openssl-build.log`
- `openssl-dist/logs/openssl-install.log`

### Patching Dependencies

Apply patch, remove build output, reconfigure:

```sh
cd quic/third-party/openssl
git apply /path/to/my.patch
cd ../..

rm -rf quic/third-party/openssl-dist
cmake -B build
```

### Verifying a Build

```sh
# Confirm httpd version and MMN
dependencies/httpd-dist/bin/apxs -q HTTPD_VERSION
dependencies/httpd-dist/bin/apxs -q HTTPD_MMN       # expect 20211221

# Confirm OpenSSL is the one httpd links
ldd dependencies/httpd-dist/modules/mod_ssl.so | grep ssl
# should show quic/third-party/openssl-dist/lib64/libssl.so
```
