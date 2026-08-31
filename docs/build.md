# Build

The default build compiles OpenSSL, APR, APR-util, httpd and nghttp3 from the
submodules. Use it when the system httpd's module magic number is too old.

```sh
git submodule update --init
git submodule update --init --recursive dependencies/nghttp3
cmake -B build
cmake --build build
```

The module is written to `build/lib/mod_http3.so`.

Only nghttp3 needs recursion, for `lib/sfparse`. Recursing everywhere also
clones OpenSSL's external test submodules, which the build never uses.

## Requirements

| Dependency | Minimum |
| --- | --- |
| OpenSSL | 3.5.0 with QUIC support |
| Apache httpd | 2.5.1+ or 2.4.69+ |
| APR | 1.7.0 |
| APR-util | 1.6.0 |
| nghttp3 | 1.18.0 |

The submodule build produces these. Supply your own with the `WITH_*` options
only if they meet the minimums; a distribution httpd is accepted from 2.4.52 on.

Against httpd trunk the module consumes response buckets directly; against 2.4.x it uses a built-in compatibility path (see `mod_http3/include/h3_compat.h`) that captures the response the way the core `HTTP_HEADER` filter would. On stock MPMs, which lack the optional `ap_mpm_note_extra_connection_added`/`_removed` functions, the module runs in a degraded mode where a graceful child stop does not wait for active QUIC connections to drain; the MPM patch in `.patches/httpd-2.4.66-pr699.patch` restores that.

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

Set `WITH_APR` and `WITH_APU` when APR and APR-util are not part of the httpd
prefix. See the [full installation reference](https://github.com/machine-moon/mod_http3/blob/trunk/INSTALL)
for package builds and every CMake option.