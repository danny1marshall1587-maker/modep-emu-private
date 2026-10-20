import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.148', username='patch', password='blokas')

cmd = "sed -n '1260,1315p' /usr/lib/python3/dist-packages/mod/webserver.py"
stdin, stdout, stderr = ssh.exec_command(cmd)
print("SERVER WEBSOCKET ON_MESSAGE:\n", stdout.read().decode())

ssh.close()
