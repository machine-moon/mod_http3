#!/bin/sh
set -eu

OUT_DIR="${1:-certs}"
CAKEY="${OUT_DIR}/ca.key"
CACRT="${OUT_DIR}/ca.crt"
KEYFILE="${OUT_DIR}/server.key"
CRTFILE="${OUT_DIR}/server.crt"

command -v openssl > /dev/null 2>&1 || { echo "openssl not found on PATH" >&2; exit 1; }

mkdir -p "${OUT_DIR}"

if [ -f "$KEYFILE" ] || [ -f "$CRTFILE" ] || [ -f "$CACRT" ]; then
    echo "error: keys already exist in '$OUT_DIR/'" >&2
    exit 1
fi

# A self-signed leaf cannot be validated by Firefox (mozilla::pkix does not
# honour directly-trusted end-entity certs), so mint a local CA and sign a
# leaf with it. Browsers trust the CA; the server presents the leaf.

# 1. Local CA (import this into the browser trust store).
openssl req \
    -x509 \
    -newkey rsa:4096 \
    -days 365 \
    -nodes \
    -keyout "${CAKEY}" \
    -out    "${CACRT}" \
    -subj   "/CN=mod_http3 local CA" \
    -addext "basicConstraints=critical,CA:TRUE" \
    -addext "keyUsage=critical,keyCertSign,cRLSign"

# 2. Leaf key + CSR.
openssl req \
    -newkey rsa:4096 \
    -nodes \
    -keyout "${KEYFILE}" \
    -out    "${OUT_DIR}/server.csr" \
    -subj   "/CN=localhost"

# 3. Sign the leaf with the CA (SAN + serverAuth, CA:FALSE).
EXTFILE="$(mktemp)"
cat > "$EXTFILE" <<EOF
subjectAltName=DNS:localhost,IP:127.0.0.1,IP:::1
basicConstraints=critical,CA:FALSE
keyUsage=critical,digitalSignature,keyEncipherment
extendedKeyUsage=serverAuth
EOF
openssl x509 \
    -req \
    -in       "${OUT_DIR}/server.csr" \
    -CA       "${CACRT}" \
    -CAkey    "${CAKEY}" \
    -CAcreateserial \
    -days     365 \
    -out      "${CRTFILE}" \
    -extfile  "$EXTFILE"

rm -f "$EXTFILE" "${OUT_DIR}/server.csr" "${OUT_DIR}/ca.srl"
chmod 600 "${KEYFILE}" "${CAKEY}"

echo "Generated:"
echo "  CA key      : $CAKEY"
echo "  CA crt      : $CACRT   (import into the browser to trust the leaf)"
echo "  private key : $KEYFILE"
echo "  public  crt : $CRTFILE"
