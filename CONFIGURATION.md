# Build Configuration

Advanced build options, dependency management, and build internals.

For quick start and deployment, see [INSTALL](INSTALL).

For httpd runtime directives (`H3CertificatePath`, VirtualHost), see [CONFIGURATION_HTTPD.md](CONFIGURATION_HTTPD.md).

## Build Commands

| Command | Description |
|---|---|
| `cmake -B build` | Configure |
| `cmake --build build -j$(nproc)` | Build |
| `cmake --build build -j$(nproc) --target tests` | Build + run tests |
| `cmake --build build -j$(nproc) --target package` | Create ZIP/TGZ |
| `cmake -LH -N -B build` | Print all cache variables |

```sh
# Module only (with system deps via WITH_SSL/WITH_HTTPD)
git submodule update --init dependencies/nghttp3

# Tests
git submodule update --init dependencies/googletest
```

Reset submodules:

```sh
git submodule foreach --recursive 'git reset --hard || :'
git submodule foreach --recursive 'git clean -ffdx || :'
```

## Build from Source

By default, CMake builds all dependencies from their submodules at configure time. Results are installed into `dependencies/<dep>-dist/` and cached with sentinel files (`.done`).

Build order:

1. OpenSSL -> `dependencies/openssl-dist/`
2. APR -> `dependencies/apr-dist/`
3. APR-util -> `dependencies/apr-util-dist/`
4. httpd -> `dependencies/httpd-dist/`

nghttp3 and googletest are always built from submodules regardless of build options.

Force a rebuild:

```sh
rm -rf dependencies/openssl-dist
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

> Distro-packaged httpd (Ubuntu, Fedora, etc.) ships with MMN < 20211221 and will fail configure. Use build-from-source mode instead.

```sh
cmake -B build -DWITH_SSL=/opt/openssl -DWITH_HTTPD=/opt/httpd
cmake --build build -j$(nproc)
```

If APR and APR-util are installed separately from httpd:

```sh
cmake -B build -DWITH_SSL=/opt/openssl -DWITH_HTTPD=/opt/httpd -DWITH_APR=/opt/apr -DWITH_APU=/opt/apr-util
cmake --build build -j$(nproc)
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
cmake --build build -j$(nproc) --target tests

# UBSan
rm -rf build
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_UBSAN=ON
cmake --build build -j$(nproc) --target tests
```

## Dependency Internals

### Sentinel Files

Each dependency built from source writes `.done` to its output directory (e.g., `dependencies/openssl-dist/.done`). CMake checks for this file before rebuilding. Delete it to force rebuild.

### Build Logs

Build logs are written to `dependencies/<dep>-dist/logs/`:

- `openssl-dist/logs/openssl-configure.log`
- `openssl-dist/logs/openssl-build.log`
- `openssl-dist/logs/openssl-install.log`

### Patching Dependencies

Apply patch, remove build output, reconfigure:

```sh
cd dependencies/openssl
git apply /path/to/my.patch
cd ../..

rm -rf dependencies/openssl-dist
cmake -B build
```

### Verifying a Build

```sh
# Confirm httpd version and MMN
dependencies/httpd-dist/bin/apxs -q HTTPD_VERSION
dependencies/httpd-dist/bin/apxs -q HTTPD_MMN       # expect 20211221

# Confirm OpenSSL is the one httpd links
ldd dependencies/httpd-dist/modules/mod_ssl.so | grep ssl
# should show dependencies/openssl-dist/lib64/libssl.so
```
