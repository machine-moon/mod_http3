# PowerShell twin of mkcert.sh; keep the two in step.
[CmdletBinding()]
param(
    [string]$OutDir = 'certs',
    [string]$OpenSsl = 'openssl'
)

$ErrorActionPreference = 'Stop'

$caKey = Join-Path $OutDir 'ca.key'
$caCrt = Join-Path $OutDir 'ca.crt'
$key   = Join-Path $OutDir 'server.key'
$crt   = Join-Path $OutDir 'server.crt'
$csr   = Join-Path $OutDir 'server.csr'
$ext   = Join-Path $OutDir 'extfile.cnf'

if (-not (Get-Command $OpenSsl -ErrorAction SilentlyContinue) -and -not (Test-Path $OpenSsl)) {
    Write-Error "openssl not found at '$OpenSsl'"
}
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
foreach ($f in $key, $crt, $caCrt) {
    if (Test-Path $f) { Write-Error "error: keys already exist in '$OutDir/'" }
}

# openssl reports progress on stderr, which ErrorActionPreference=Stop raises.
$ErrorActionPreference = 'Continue'
function OpenSsl { $out = & $OpenSsl @args 2>&1; if ($LASTEXITCODE -ne 0) { $out | ForEach-Object { Write-Host $_ }; exit 1 } }

# Firefox rejects a self-signed leaf, so mint a local CA and sign one with it.
$env:OPENSSL_CONF = 'NUL'

# 1. Local CA (import this into the browser trust store).
OpenSsl req -x509 -newkey rsa:4096 -days 365 -nodes -keyout $caKey -out $caCrt `
    -subj '/CN=mod_http3 local CA' `
    -addext 'basicConstraints=critical,CA:TRUE' `
    -addext 'keyUsage=critical,keyCertSign,cRLSign'

# 2. Leaf key + CSR.
OpenSsl req -newkey rsa:4096 -nodes -keyout $key -out $csr -subj '/CN=localhost'

# 3. Sign the leaf with the CA (SAN + serverAuth, CA:FALSE).
@'
subjectAltName=DNS:localhost,IP:127.0.0.1,IP:::1
basicConstraints=critical,CA:FALSE
keyUsage=critical,digitalSignature,keyEncipherment
extendedKeyUsage=serverAuth
'@ | Set-Content $ext -Encoding ascii
OpenSsl x509 -req -in $csr -CA $caCrt -CAkey $caKey -CAcreateserial -days 365 -out $crt -extfile $ext

Remove-Item $ext, $csr, (Join-Path $OutDir 'ca.srl') -Force -ErrorAction SilentlyContinue

Write-Host "Generated:"
Write-Host "  CA key      : $caKey"
Write-Host "  CA crt      : $caCrt   (import into the browser to trust the leaf)"
Write-Host "  private key : $key"
Write-Host "  public  crt : $crt"
