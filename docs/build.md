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
