import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.148', username='patch', password='blokas')

cmd = "python3 -c \"import mod.session; print(mod.session.__file__)\""
stdin, stdout, stderr = ssh.exec_command(cmd)
print(stdout.read().decode())

cmd = "cp /usr/lib/python3/dist-packages/mod/session.py /usr/lib/python3/dist-packages/mod/session.py.bak_nav"
stdin, stdout, stderr = ssh.exec_command(cmd)
print("Backup created")

ssh.close()
