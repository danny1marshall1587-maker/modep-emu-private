import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.148', username='patch', password='blokas')

cmd = "grep -n -C 5 'navigate_bank_pedalboard' /usr/lib/python3/dist-packages/mod/*.py"
stdin, stdout, stderr = ssh.exec_command(cmd)
print(stdout.read().decode())

cmd = "grep -n -C 5 'bank_midi' /usr/lib/python3/dist-packages/mod/*.py"
stdin, stdout, stderr = ssh.exec_command(cmd)
print(stdout.read().decode())

ssh.close()
