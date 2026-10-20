import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.148', username='patch', password='blokas')

cmd = "sed -n '1316,1380p' /usr/lib/python3/dist-packages/mod/webserver.py"
stdin, stdout, stderr = ssh.exec_command(cmd)
print("SERVER WEBSOCKET ON_MESSAGE 2:\n", stdout.read().decode())

ssh.close()
