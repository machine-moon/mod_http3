# Build

The default build compiles OpenSSL, APR, APR-util, httpd, and nghttp3 from the repository submodules. This is the supported path when system packages do not meet the required httpd module magic number.

```sh
git submodule update --init
git submodule update --init --recursive dependencies/nghttp3
cmake -B build
cmake --build build
```

Only nghttp3 needs its own submodule (`lib/sfparse`). Recursing everywhere also clones OpenSSL's eleven external-test submodules, which the build never uses.

The module is written to `build/lib/mod_http3.so`.

## Requirements

The versions below are what the submodule build produces; supply your own with
the `WITH_*` options only if they meet these minimums.

| Dependency | Minimum |
| --- | --- |
| OpenSSL | 3.5.0 with QUIC support |
| Apache httpd | MMN 20211221 |
| APR | 1.7.0 |
| APR-util | 1.6.0 |
| nghttp3 | 1.18.0 |

Distribution-provided httpd packages usually have an older MMN and are rejected. Use the default source build or provide compatible custom prefixes.

## QUIC engine

`ENABLE_NGTCP2` decides which engines the module contains. The default needs
nothing extra:

```sh
cmake -B build                              # OpenSSL's QUIC only (default)
cmake -B build -DENABLE_NGTCP2=ON           # both, ngtcp2 from the submodule
```

Enabling ngtcp2 also builds `quic/third-party/ngtcp2`, which needs the OpenSSL
built alongside it; point `WITH_NGTCP2` at a prefix to use one you already have.
OpenSSL remains the TLS provider either way.

A build containing both picks one at start-up with `H3QuicEngine`; see
[architecture](architecture.md#quic-engines).

## Custom Prefixes

```sh
git submodule update --init dependencies/nghttp3
cmake -B build \
    -DWITH_SSL=/opt/openssl \
    -DWITH_HTTPD=/opt/httpd \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_TESTS=OFF
cmake --build build
```

Set `WITH_APR` and `WITH_APU` when APR and APR-util are not part of the httpd prefix. See the [full installation reference](https://github.com/machine-moon/mod_http3/blob/trunk/INSTALL) for package builds and every CMake option.
