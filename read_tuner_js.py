import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.148', username='patch', password='blokas')

cmd = "sed -n '580,630p' /usr/share/mod/html/js/tuner.js"
stdin, stdout, stderr = ssh.exec_command(cmd)
print("TUNER MIDI GUI CODE:\n", stdout.read().decode())

ssh.close()
