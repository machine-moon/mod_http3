#!/usr/bin/env python3
import os
import json


def main():
    response = {
        "https": os.environ.get("HTTPS", ""),
        "request_scheme": os.environ.get("REQUEST_SCHEME", ""),
        "server_protocol": os.environ.get("SERVER_PROTOCOL", ""),
        "remote_addr": os.environ.get("REMOTE_ADDR", ""),
        "remote_port": os.environ.get("REMOTE_PORT", ""),
        "ssl_protocol": os.environ.get("SSL_PROTOCOL", ""),
        "ssl_cipher": os.environ.get("SSL_CIPHER", ""),
        "ssl_cipher_usekeysize": os.environ.get("SSL_CIPHER_USEKEYSIZE", ""),
        "ssl_cipher_algkeysize": os.environ.get("SSL_CIPHER_ALGKEYSIZE", ""),
        "ssl_cipher_export": os.environ.get("SSL_CIPHER_EXPORT", ""),
        "ssl_session_resumed": os.environ.get("SSL_SESSION_RESUMED", ""),
    }

    print("Content-Type: application/json")
    print()
    print(json.dumps(response))


if __name__ == "__main__":
    main()
