import socket
import sys

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.settimeout(2.0)
s.connect(("127.0.0.1", 5555))

param_names = {
    2: "Bypass",
    3: "Style",
    4: "BPM Override",
    5: "Accent Sens",
    8: "Groove",
    9: "Overall Vel",
    13: "Mute Drums",
    15: "Detected BPM",
    16: "Current Section",
    17: "Energy Level",
    18: "Strum Only Mode",
    20: "Stop Button",
    21: "Fill Here Button",
    22: "Manual Chorus Button"
}

for p, name in param_names.items():
    s.sendall(f"param_get effect_9 {p}\n".encode())
    resp = s.recv(1024).decode().strip()
    print(f"Port {p:2d} ({name:20s}): {resp}")

s.close()
