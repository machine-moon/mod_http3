#!/bin/bash
set -eu

cd "$(dirname "$0")/.."

cmake -B build -G Ninja --fresh && cmake --build build

HTTPD_PATH="${1:-$(pwd)/dependencies/httpd-dist}"

if [ ! -d "$HTTPD_PATH" ]; then
    echo "Error: httpd path does not exist: $HTTPD_PATH" >&2
    echo "Usage: $0 [path-to-httpd]" >&2
    exit 1
fi

HTTPD_PATH="$(cd "$HTTPD_PATH" && pwd)"
HTTPD_CONF="$HTTPD_PATH/conf/httpd.conf"
HTTPD_CERTS="$HTTPD_PATH/conf/certs"
HTTPD_MODULE="$HTTPD_PATH/modules/mod_http3.so"
HTTPD_HTDOCS="$HTTPD_PATH/htdocs"
CERTS="container/certs"

cp build/lib/mod_http3.so "$HTTPD_MODULE"

mkdir -p "$HTTPD_PATH/conf"
sed "s|/src/dependencies/httpd-dist|$HTTPD_PATH|g" container/httpd-linux.conf > "$HTTPD_CONF"

if [ ! -f "$CERTS/server.crt" ] || [ ! -f "$CERTS/server.key" ]; then
    scripts/mkcert.sh "$CERTS"
fi

mkdir -p "$HTTPD_CERTS"
cp -a "$CERTS/." "$HTTPD_CERTS/"

mkdir -p "$HTTPD_HTDOCS"
rm -rf "$HTTPD_HTDOCS"/*
cp -a container/static/. "$HTTPD_HTDOCS/"

H3_PORT="${H3_PORT:-8443}"

echo "Launch GDB with:"
echo "  H3_PORT=$H3_PORT gdb --args $HTTPD_PATH/bin/httpd -X -f $HTTPD_CONF"
echo ""
echo "Or run directly:"
echo "  H3_PORT=$H3_PORT $HTTPD_PATH/bin/httpd -X -f $HTTPD_CONF"
echo ""
