import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.148', username='patch', password='blokas')

cmd = "sed -n '580,630p' /usr/share/mod/html/js/system.js"
stdin, stdout, stderr = ssh.exec_command(cmd)
print("SYSTEM.JS:\n", stdout.read().decode())

cmd = "tail -n 60 /usr/share/mod/html/js/system.js"
stdin, stdout, stderr = ssh.exec_command(cmd)
print("SYSTEM.JS END:\n", stdout.read().decode())

ssh.close()
