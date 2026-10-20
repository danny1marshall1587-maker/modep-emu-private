#!/usr/bin/env python3
import socket

for port in [5555, 5556, 5557, 5558, 5559]:
    try:
        s = socket.socket()
        s.settimeout(2.0)
        s.connect(("127.0.0.1", port))
        s.sendall(b"help\0")
        resp = b""
        while not resp.endswith(b"\0"):
            chunk = s.recv(1024)
            if not chunk:
                break
            resp += chunk
        print(f"=== PORT {port} ===")
        print(resp.decode(errors="ignore").strip())
        s.close()
    except Exception as e:
        print(f"Port {port} error: {e}")
