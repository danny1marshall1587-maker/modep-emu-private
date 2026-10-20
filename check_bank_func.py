import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.148', username='patch', password='blokas')

cmd = "python3 -c \"import mod.host; import inspect; print([m for m in dir(mod.host.Host) if 'next' in m.lower() or 'prev' in m.lower()])\""
stdin, stdout, stderr = ssh.exec_command(cmd)
print("NEXT/PREV METHODS IN HOST:\n", stdout.read().decode())

cmd = "grep -n -C 5 'BANK_FUNC_PEDALBOARD_NEXT' /usr/lib/python3/dist-packages/mod/host.py"
stdin, stdout, stderr = ssh.exec_command(cmd)
print("BANK_FUNC_PEDALBOARD_NEXT:\n", stdout.read().decode())

ssh.close()
