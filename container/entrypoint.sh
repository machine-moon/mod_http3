#!/bin/sh
set -eu

H3_PORT=${H3_PORT:-8443}
export H3_PORT

root=/src/dependencies/httpd-dist
certs=$root/conf/certs

# If no certificate is mounted, generate a self-signed one.
if [ ! -s "$certs/server.crt" ]; then
    echo "no certificate mounted at $certs, generating a self-signed one"
    if ! out=$(LD_LIBRARY_PATH=/src/quic/third-party/openssl-dist/lib64 OPENSSL_CONF=/dev/null \
        /src/quic/third-party/openssl-dist/bin/openssl req -x509 -newkey rsa:2048 \
        -nodes -days 365 -subj /CN=localhost \
        -keyout "$certs/server.key" -out "$certs/server.crt" 2>&1); then
        printf '%s\n' "$out" >&2
        exit 1
    fi
    chown daemon "$certs/server.key"
    chmod 0400 "$certs/server.key"
fi

exec "$root/bin/httpd" -D FOREGROUND -f "$root/conf/httpd.conf" "$@"
