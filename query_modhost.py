import socket

s = socket.socket()
s.connect(("localhost", 5555))
s.sendall(b"param_get 39 style\0")
s.settimeout(2)
try:
    print(s.recv(4096))
except Exception as e:
    print("Error:", e)
s.close()
