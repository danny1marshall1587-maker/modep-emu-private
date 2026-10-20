import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.148', username='patch', password='blokas')

cmd = "sed -n '308,350p' /usr/lib/python3/dist-packages/mod/session.py"
stdin, stdout, stderr = ssh.exec_command(cmd)
print("WEB_PARAM_ADDR IMPLEMENTATION:\n", stdout.read().decode())

ssh.close()
