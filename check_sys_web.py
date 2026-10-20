import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_key_policy = paramiko.AutoAddPolicy()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.148', username='patch', password='blokas')

cmd = "grep -n -C 5 'CyberSystemManager' /usr/lib/python3/dist-packages/mod/webserver.py"
stdin, stdout, stderr = ssh.exec_command(cmd)
out = stdout.read().decode('utf-8', errors='replace')
print(out.encode('ascii', errors='replace').decode('ascii'))

ssh.close()
