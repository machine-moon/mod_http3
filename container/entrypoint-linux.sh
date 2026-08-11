#!/bin/sh
set -eu

H3_PORT=${H3_PORT:-8443}
export H3_PORT

root=/src/dependencies/httpd-dist
certs=$root/conf/certs

# If no certificate is mounted, generate one.
if [ ! -s "$certs/server.crt" ]; then
    echo "no certificate mounted at $certs, generating a self-signed one"
    LD_LIBRARY_PATH=/src/dependencies/openssl-dist/lib64 \
    PATH="/src/dependencies/openssl-dist/bin:$PATH" \
        sh /mkcert.sh "$certs"
    chown daemon "$certs/server.key"
    chmod 0400 "$certs/server.key"
fi

exec "$root/bin/httpd" -D FOREGROUND -f "$root/conf/httpd.conf" "$@"
