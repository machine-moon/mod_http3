#!/usr/bin/env python3
import os
import sys
import json
import hashlib

def main():
    body = sys.stdin.buffer.read()
    response = {
        "method": os.environ.get("REQUEST_METHOD", ""),
        "body_size": len(body),
        "body_sha256": hashlib.sha256(body).hexdigest() if body else None,
    }

    print("Content-Type: application/json")
    print()
    print(json.dumps(response))

if __name__ == "__main__":
    main()
