# Deploy

Copy the module to a compatible httpd installation and create certificate files readable by the httpd child user.

```sh
cp build/lib/mod_http3.so /path/to/httpd/modules/
bash scripts/mkcert.sh /path/to/httpd/conf/certs
```

Add `mod_ssl`, then `mod_http3`, before the VirtualHost that configures HTTP/3:

```apache
LoadModule ssl_module modules/mod_ssl.so
LoadModule http3_module modules/mod_http3.so

Listen 4433 https

<VirtualHost *:4433>
    ServerName localhost
    SSLEngine on
    SSLCertificateFile conf/certs/server.crt
    SSLCertificateKeyFile conf/certs/server.key
    H3CertificatePath conf/certs/server.crt
    H3CertificateKeyPath conf/certs/server.key
    DocumentRoot htdocs
    <Directory htdocs>
        Require all granted
    </Directory>
</VirtualHost>
```

Open the UDP port on the server firewall. TCP is still needed for HTTP/1.1 and HTTP/2 clients, and for the initial `Alt-Svc` discovery flow.

```sh
firewall-cmd --permanent --add-port=4433/udp && firewall-cmd --reload
# or
ufw allow 4433/udp
```

Set `H3Port` only when the QUIC listener must use a different UDP port from the configured VirtualHost.
