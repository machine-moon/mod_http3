#!/usr/bin/env python3
import os
import json


def main():
    response = {
        "https": os.environ.get("HTTPS", ""),
        "request_scheme": os.environ.get("REQUEST_SCHEME", ""),
        "server_protocol": os.environ.get("SERVER_PROTOCOL", ""),
    }

    print("Content-Type: application/json")
    print()
    print(json.dumps(response))


if __name__ == "__main__":
    main()
