#!/bin/bash

set -u

HTTPD=/src/dependencies/httpd-dist/bin/httpd
CONF=/src/dependencies/httpd-dist/conf/httpd.conf

[ "${ROLE:-}" = server ] || { echo "UNSUPPORTED ROLE ${ROLE:-<unset>}"; exit 127; }

case "${TESTCASE:-}" in
    http3) ;;
    *) echo "UNSUPPORTED TESTCASE ${TESTCASE:-<unset>}"; exit 127 ;;
esac

/setup.sh

install -D -m 644 -t /interop/certs /certs/cert.pem /certs/priv.key
cp -rT /www /interop/www && chmod -R a+rX /interop/www
chown -R www-data /logs
printf 'H3AddressValidation off\nH3QuicEngine %s\n' "${ENGINE:-openssl}" >/interop/testcase.conf

echo "TESTCASE=$TESTCASE ENGINE=${ENGINE:-openssl}"
"$HTTPD" -t -f "$CONF" || { echo "httpd rejected the configuration"; exit 1; }

exec "$HTTPD" -D FOREGROUND -f "$CONF"
