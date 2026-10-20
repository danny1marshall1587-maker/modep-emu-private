import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.148', username='patch', password='blokas')

cmd = "sed -n '370,410p' /usr/share/mod/html/js/desktop.js"
stdin, stdout, stderr = ssh.exec_command(cmd)
print(stdout.read().decode())

cmd = "grep -n -C 5 'hardwareManager' /usr/share/mod/html/js/desktop.js"
stdin, stdout, stderr = ssh.exec_command(cmd)
print(stdout.read().decode())

ssh.close()
