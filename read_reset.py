import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.148', username='patch', password='blokas')

cmd = "sed -n '5135,5165p' /usr/lib/python3/dist-packages/mod/host.py"
stdin, stdout, stderr = ssh.exec_command(cmd)
print("RESET CODE:\n", stdout.read().decode())

ssh.close()
