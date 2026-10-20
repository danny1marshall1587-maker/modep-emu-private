import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.148', username='patch', password='blokas')

cmd = "sed -n '570,610p' /usr/share/mod/html/js/host.js"
stdin, stdout, stderr = ssh.exec_command(cmd)
print("LOADING END IN HOST.JS:\n", stdout.read().decode())

ssh.close()
