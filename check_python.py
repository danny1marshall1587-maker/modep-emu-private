import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.148', username='patch', password='blokas')

cmd = "head -n 5 /usr/bin/mod-ui"
stdin, stdout, stderr = ssh.exec_command(cmd)
print(stdout.read().decode())

cmd = "python3 -c 'import sys; print(sys.path)'"
stdin, stdout, stderr = ssh.exec_command(cmd)
print(stdout.read().decode())

ssh.close()
