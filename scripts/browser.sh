#!/bin/bash
set -eu

cd "$(dirname "$0")/.."

usage()
{
    cat >&2 <<EOF
usage: ${0##*/} [browser] [host] [port] [verbose]

  browser   chromium | chrome | firefox   (default: chromium)
  host      server hostname or IP         (default: localhost)
  port      HTTPS / HTTP-3 port           (default: 8443)
  verbose   0 | 1                         (default: 0)

Every argument falls back to an environment variable when omitted:
BROWSER, HTTPD_HOST, HTTPD_PORT, VERBOSE. HTTPD_PATH selects the httpd
install directory (default: dependencies/httpd-dist).

  ${0##*/} firefox 192.168.1.10 443 1
EOF
    exit "${1:-1}"
}

die()
{
    echo "error: $1" >&2
    exit 1
}

case "${1:-}" in
    -h | --help) usage 0 ;;
esac
[ "$#" -le 4 ] || die "too many arguments (expected at most 4, got $#)"

BROWSER="${1:-${BROWSER:-chromium}}"
HTTPD_HOST="${2:-${HTTPD_HOST:-localhost}}"
HTTPD_PORT="${3:-${HTTPD_PORT:-8443}}"
VERBOSE="${4:-${VERBOSE:-0}}"

case "$BROWSER" in
    chromium | chrome | firefox) ;;
    *) die "browser must be chromium, chrome or firefox (got '$BROWSER')" ;;
esac
case "$HTTPD_HOST" in
    '' | *[![:alnum:].:_-]*) die "invalid host '$HTTPD_HOST'" ;;
esac
case "$HTTPD_PORT" in
    '' | *[!0-9]*) die "port must be numeric (got '$HTTPD_PORT')" ;;
esac
[ "$HTTPD_PORT" -ge 1 ] && [ "$HTTPD_PORT" -le 65535 ] || die "port out of range (got '$HTTPD_PORT')"
case "$VERBOSE" in
    0 | 1) ;;
    *) die "verbose must be 0 or 1 (got '$VERBOSE')" ;;
esac

HTTPD_PATH="${HTTPD_PATH:-$(pwd)/dependencies/httpd-dist}"
[ -d "$HTTPD_PATH" ] || die "httpd directory not found: $HTTPD_PATH"
HTTPD_PATH="$(cd "$HTTPD_PATH" && pwd)"

URL="https://$HTTPD_HOST:$HTTPD_PORT/"
CRT="$HTTPD_PATH/conf/certs"
PROFILE="$HOME/.cache/mod-http3-$BROWSER-dev"

rm -rf "$PROFILE"

if [ ! -f "$CRT/server.crt" ]; then
    die "no certs found. run ./scripts/mkcert.sh $CRT first"
fi

echo "Launching $BROWSER"
echo "  URL:    $URL"

if [ "$BROWSER" = "firefox" ]; then
    command -v certutil >/dev/null || die "certutil not found. install libnss3-tools (Debian) or nss-tools (Fedora)"
    if [ ! -f "$CRT/ca.crt" ]; then
        die "no CA cert found. re-run ./scripts/mkcert.sh $CRT to generate ca.crt"
    fi
    mkdir -p "$PROFILE"
    # Firefox (mozilla::pkix) needs a trusted CA, not a directly-trusted leaf.
    certutil -A -n "mod_http3-CA" -t "C,," -i "$CRT/ca.crt" -d "sql:$PROFILE"
    cat > "$PROFILE/user.js" <<EOF
user_pref("network.http.http3.enable", true);
user_pref("network.http.http3.alt-svc-mapping-for-testing", "$HTTPD_HOST;h3=:$HTTPD_PORT");
user_pref("network.http.http3.disable_when_third_party_roots_found", false);
EOF
    if [ "$VERBOSE" = "1" ]; then export MOZ_LOG="neqo:5,nsHttp:5"; fi
    exec firefox -no-remote -profile "$PROFILE" "$URL"
fi

case "$BROWSER" in
    chromium) BIN="chromium-browser" ;;
    chrome) BIN="google-chrome" ;;
esac

SPKI=$(openssl x509 -in "$CRT/server.crt" -pubkey -noout \
    | openssl pkey -pubin -outform der \
    | openssl dgst -sha256 -binary | base64)

echo "  SPKI:   $SPKI"

VFLAG=""
if [ "$VERBOSE" = "1" ]; then VFLAG="--v=1"; fi

exec "$BIN" \
    --user-data-dir="$PROFILE" \
    --ignore-certificate-errors \
    --allow-insecure-localhost \
    --ignore-certificate-errors-spki-list="$SPKI" \
    --origin-to-force-quic-on="$HTTPD_HOST:$HTTPD_PORT" \
    --enable-logging=stderr \
    $VFLAG \
    "$URL"
