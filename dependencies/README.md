# Dependencies

mod_http3 uses **git submodules** for all dependencies. By default, all dependencies are built from source at configure time. Provide `WITH_*` CMake variables to override with system-installed versions.

This directory holds what every build needs. **Optional QUIC libraries live under
[`quic/third-party/`](../quic/third-party) instead**, beside the engines that use
them — today that is ngtcp2. OpenSSL stays here: it is always required, since it
provides TLS whichever QUIC engine runs.

---

## Pinned dependency versions

| Dependency  | Submodule path              | Branch          | Version (current) | Notes                          |
|-------------|-----------------------------|-----------------|-------------------|--------------------------------|
| OpenSSL     | `quic/third-party/openssl`      | `openssl-3.5`   | 3.5.7-dev         | QUIC support required (≥ 3.5). |
| httpd       | `dependencies/httpd`        | `trunk`         | 2.5.1-dev         | AP25 API; requires MMN ≥ 20211221/30. |
| APR         | `dependencies/apr`          | `1.7.x`         | 1.7.7             | APR v2-dev (trunk) will subsume APR-util 1.x APIs. |
| APR-util    | `dependencies/apr-util`     | `1.6.x`         | 1.6.4             | Legacy companion library; kept for APR 1.x compatibility. |
| nghttp3     | `dependencies/nghttp3`      | `main`          | 1.17.0            | HTTP/3 framing and QPACK.      |
| ngtcp2      | `quic/third-party/ngtcp2`  | `main` (v1.25.0)| 1.25.0            | QUIC transport; built only for `-DENABLE_NGTCP2=ON`. |

All submodules are shallow (`shallow = true`). Initialise them once:

```sh
git submodule sync
git submodule update --init
git submodule update --init --recursive dependencies/nghttp3
```

---

## Why httpd `trunk`, not `2.4.x`

Apache's **Module Magic Number (MMN)** encodes the ABI version of the httpd module API.

| Source                        | MMN major  | MMN minor | Cookie |
|-------------------------------|------------|-----------|--------|
| httpd `trunk` (2.5.1-dev)     | 20211221   | 30        | AP25   |
| httpd `2.4.x` release         | 20120211   | (varies)  | AP24   |
| Typical OS package (2.4.x)    | 20120211   | (varies)  | AP24   |

mod_http3 uses APR bucket types (`AP_BUCKET_IS_RESPONSE`, etc.) that were introduced in the AP25 API generation. Building against 2.4.x -- whether from the branch or from an OS package -- produces a binary that either fails to link or fails to load at runtime. Targeting `trunk` (AP25) resolves this.

> If this ever stabilises into a 2.5.x release series the submodule branch will be updated accordingly.

---

## Dependency resolution modes

### Default -- Build from source

CMake builds OpenSSL, APR, APR-util, httpd, and nghttp3 from their respective git submodules at **configure time**, installing each into `dependencies/<dep>-dist/`. A small marker file (`dependencies/<dep>-dist/.done`) is used to skip rebuilding dependencies that are already up to date.

**Build order enforced by CMake:**
1. nghttp3 (`dependencies/nghttp3`) -> `dependencies/nghttp3-dist/`
2. OpenSSL (`quic/third-party/openssl`) -> `quic/third-party/openssl-dist/`
3. APR (`dependencies/apr`) -> `dependencies/apr-dist/`
4. APR-util (`dependencies/apr-util`) -> `dependencies/apr-util-dist/`
5. httpd (`dependencies/httpd`) -> `dependencies/httpd-dist/`
6. ngtcp2 (`quic/third-party/ngtcp2`) -> `quic/third-party/ngtcp2-dist/` (only when the ngtcp2 QUIC engine is selected; it consumes the OpenSSL built above)

```sh
# default: builds all dependencies from source (first configure is slow; subsequent ones are instant from cache)
cmake -B build
cmake --build build -j$(nproc)
```

This is the recommended mode for development. Everything is self-contained under the repo.

**To force a clean rebuild of a dependency built from source**, delete its `-dist` dir and re-configure:

```sh
rm -rf quic/third-party/openssl-dist   # re-build OpenSSL
rm -rf dependencies/httpd-dist     # re-build httpd
cmake -B build
```

---

### Override with system packages (`WITH_*` variables)

Provide `WITH_*` paths to use system-installed dependencies instead of building from source. Each `WITH_*` variable overrides the corresponding source build.

| Variable | Overrides | Minimum |
|---|---|---|
| `WITH_SSL=/path` | OpenSSL source build | >= 3.5.0 |
| `WITH_HTTPD=/path` | httpd source build (includes APR/APU resolution via apxs) | MMN >= 20211221 |
| `WITH_APR=/path` | APR source build | >= 1.7.0 |
| `WITH_APU=/path` | APR-util source build | >= 1.6.0 |
| `WITH_NGHTTP3=/path` | nghttp3 source build | ≥ 1.16.0 |
| `WITH_NGTCP2=/path` | ngtcp2 source build | ≥ 1.25.0 |

```sh
cmake -B build -DWITH_SSL=/opt/openssl -DWITH_HTTPD=/opt/httpd
cmake --build build -j$(nproc)
```

If APR and APR-util are installed separately from httpd:

```sh
cmake -B build -DWITH_SSL=/opt/openssl -DWITH_HTTPD=/opt/httpd -DWITH_APR=/opt/apr -DWITH_APU=/opt/apr-util
cmake --build build -j$(nproc)
```

Using a pre-built nghttp3:

```sh
cmake -B build -DWITH_NGHTTP3=/opt/nghttp3
cmake --build build -j$(nproc)
```

> **OS package caveat:** System httpd packages (Ubuntu, Fedora, etc.) ship the 2.4.x AP24 generation (MMN < 20211221). CMake will `FATAL_ERROR` on the MMN check. Use build-from-source mode or a custom prefix install instead.

---

### Mixed mode

You can override individual dependencies while building the rest from source. For example, use system OpenSSL but build httpd from source:

```sh
cmake -B build -DWITH_SSL=/opt/openssl
```

Or build OpenSSL from source but use a system httpd:

```sh
cmake -B build -DWITH_HTTPD=/opt/httpd
```

---

### Patching dependencies built from source

If you need to patch OpenSSL, httpd, or APR/APU, apply the patch to the corresponding submodule and **remove any existing build output** before reconfiguring. Build-from-source mode will then rebuild from the patched sources.

```sh
cd quic/third-party/openssl
git apply /path/to/my.patch
cd ../..

# ensure the previous build output is discarded
rm -rf quic/third-party/openssl-dist

cmake -B build # External autotool builds happen at configuration time
```

The `.done` approach means you can also manually build and install or copy into `dependencies/<dep>-dist/` yourself and create the `.done` file; CMake will skip the automated build step and use whatever is there.

---

## Manual build reference

The following are the exact configure/build commands CMake runs for each dependency. Use these if you want to build a dependency by hand and point `WITH_*` at the result.


---

### 1. OpenSSL (>= 3.5.0)

Submodule: `quic/third-party/openssl` | cmake module: `cmake/modules/openssl.cmake`

Uses OpenSSL's own `./config` wrapper (not autoconf).

```sh
cd quic/third-party/openssl

./config \
  --prefix=$PREFIX \
  --openssldir=$PREFIX/ssl \
  shared

make clean
make -j$(nproc)
make install_sw
```

Hook up: `-DWITH_SSL=$PREFIX`

---

### 2. APR (>= 1.7.0)

Submodule: `dependencies/apr` | cmake module: `cmake/modules/apr.cmake`

Standard autoconf build; no extra flags.

```sh
cd dependencies/apr

./buildconf

./configure \
  --prefix=$PREFIX

make clean
make -j$(nproc)
make install
```

Hook up: `-DWITH_APR=$PREFIX`
---

### 3. APR-util (>= 1.6.0)

Submodule: `dependencies/apr-util` | cmake module: `cmake/modules/apu.cmake`

Must be built **after** APR. `buildconf` takes the APR **source** dir; `./configure` takes the APR **install** prefix.

```sh
cd dependencies/apr-util

./buildconf --with-apr=../apr        # path to APR submodule source

./configure \
  --with-apr=$APR_PREFIX \           # APR install prefix (e.g. dependencies/apr-dist)
  --prefix=$PREFIX

make clean
make -j$(nproc)
make install
```

Hook up: `-DWITH_APU=$PREFIX`

> If `WITH_APR` is set but `WITH_APU` is not, CMake also searches `$WITH_APR/bin/` for `apu-1-config`, so a co-installed APR/APU under one prefix works without specifying `WITH_APU`.

---

### 4. httpd (MMN >= 20211221)

Submodule: `dependencies/httpd` | cmake module: `cmake/modules/httpd.cmake`

Must be built **after** OpenSSL, APR, and APR-util. An `LDFLAGS` rpath is passed so the installed httpd finds the correct OpenSSL at runtime.

```sh
cd dependencies/httpd

./buildconf \
  --with-apr=../apr \                   # APR submodule source dir
  --with-apr-util=../apr-util           # APU submodule source dir

LDFLAGS="-Wl,-rpath,$OPENSSL_LIBDIR" \
./configure \
  --prefix=$PREFIX \
  --with-apr=$APR_PREFIX \              # APR install prefix
  --with-apr-util=$APU_PREFIX \         # APU install prefix
  --enable-so \
  --with-mpm=event \
  --enable-mods-shared=all \
  --enable-ssl \
  --with-ssl=$OPENSSL_PREFIX            # OpenSSL install prefix

make clean
make -j$(nproc)
make install
```


Hook up: `-DWITH_HTTPD=$PREFIX`

---

### 5. nghttp3 (>= 1.16.0)

Submodule: `dependencies/nghttp3` | cmake module: `cmake/modules/nghttp3.cmake`

Uses autoconf. The nested `sfparse` submodule must also be initialised before running `autoreconf`.

```sh
git submodule update --init dependencies/nghttp3
git submodule update --init dependencies/nghttp3/lib/sfparse

cd dependencies/nghttp3

autoreconf -i

./configure \
  --prefix=$PREFIX \
  --enable-lib-only

make clean
make -j$(nproc)
make install
```

Hook up: `-DWITH_NGHTTP3=$PREFIX`

---

---

### 6. ngtcp2 (>= 1.25.0, only for `-DENABLE_NGTCP2=ON`)

Submodule: `quic/third-party/ngtcp2` | cmake module: `cmake/modules/ngtcp2.cmake`

The in-tree build uses ngtcp2's autotools, so it needs `autoconf`, `automake`
and `libtool` on the host. The recipe below uses ngtcp2's own CMake instead,
which does not. Either way it consumes the OpenSSL built above, so build that
first; OpenSSL remains the TLS provider on both engines, and ngtcp2 replaces
only the transport.

```sh
cmake -B quic/third-party/ngtcp2/build -S quic/third-party/ngtcp2 \
  -DCMAKE_INSTALL_PREFIX=$PREFIX \
  -DENABLE_OPENSSL=ON \
  -DENABLE_LIB_ONLY=ON \
  -DOPENSSL_ROOT_DIR=$OPENSSL_PREFIX

cmake --build quic/third-party/ngtcp2/build -j$(nproc)
cmake --install quic/third-party/ngtcp2/build
```

Produces both `libngtcp2` and `libngtcp2_crypto_ossl`; the module needs both. A
distribution ngtcp2 built against the quictls fork ships
`libngtcp2_crypto_quictls` instead and will not work.

Hook up: `-DWITH_NGTCP2=$PREFIX`


---

## Verifying a build-from-source install

```sh
# Confirm httpd version and MMN built from source
dependencies/httpd-dist/bin/apxs -q HTTPD_VERSION
dependencies/httpd-dist/bin/apxs -q HTTPD_MMN       # expect 20211221

# Confirm OpenSSL is the one httpd links
ldd dependencies/httpd-dist/modules/mod_ssl.so | grep ssl
# should show quic/third-party/openssl-dist/lib64/libssl.so, not /usr/lib/...

# Confirm nghttp3 version
grep 'NGHTTP3_VERSION ' dependencies/nghttp3-dist/include/nghttp3/version.h

# Confirm ngtcp2 version (only when the ngtcp2 engine is selected)
grep 'NGTCP2_VERSION ' quic/third-party/ngtcp2-dist/include/ngtcp2/version.h
```
