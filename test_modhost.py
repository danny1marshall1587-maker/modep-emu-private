import socket

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect(('127.0.0.1', 5555))

# Send help or list
s.sendall(b"help\n")
data = s.recv(4096)
print("Response to help:\n", data.decode('utf-8', errors='ignore'))

# Check loaded plugins/instances
s.sendall(b"preset_load\n") # or see what instances exist
# We can also check status or param_get
s.close()
