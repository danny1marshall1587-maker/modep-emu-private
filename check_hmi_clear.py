import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.148', username='patch', password='blokas')

cmd = "grep -n -C 5 'def clear(' /usr/lib/python3/dist-packages/mod/hmi.py /usr/lib/python3/dist-packages/mod/development.py"
stdin, stdout, stderr = ssh.exec_command(cmd)
print(stdout.read().decode())

ssh.close()
