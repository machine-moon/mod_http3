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
    }

    print("Content-Type: application/json")
    print()
    print(json.dumps(response))


if __name__ == "__main__":
    main()
