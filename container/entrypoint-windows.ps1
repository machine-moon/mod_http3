#Requires -Version 5

$ErrorActionPreference = 'Stop'

$root  = 'C:/httpd'
$certs = Join-Path $root 'conf/certs'
$logs  = Join-Path $root 'logs'
$port  = if ($env:H3_PORT) { $env:H3_PORT } else { '8443' }
$env:H3_PORT = $port

New-Item -ItemType Directory -Force -Path $certs, $logs | Out-Null

# If no certificate is mounted, generate one.
$crt = Join-Path $certs 'server.crt'
if (-not (Test-Path $crt) -or (Get-Item $crt).Length -eq 0) {
    Write-Host "no certificate mounted at $certs, generating a self-signed one"
    & C:/mkcert.ps1 -OutDir $certs -OpenSsl 'C:/httpd/bin/openssl.exe'
    if ($LASTEXITCODE -ne 0) { exit 1 }
}

# Windows has no /proc/self/fd; relay the log files to stdout.
foreach ($f in 'error.log', 'access.log') {
    $path = Join-Path $logs $f
    if (-not (Test-Path $path)) { New-Item -ItemType File -Force -Path $path | Out-Null }
    # -NoNewWindow so the child inherits this console and its output is ours.
    Start-Process powershell -NoNewWindow -ArgumentList `
        '-NoProfile', '-Command', "Get-Content -LiteralPath '$path' -Wait -Tail 0" | Out-Null
}

& (Join-Path $root 'bin/httpd.exe') -D FOREGROUND -f (Join-Path $root 'conf/httpd.conf') @args
exit $LASTEXITCODE
