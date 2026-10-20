import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.148', username='patch', password='blokas')

cmd = "python3 -c \"import mod.system_api; print([x for x in dir(mod.system_api) if not x.startswith('_')])\""
stdin, stdout, stderr = ssh.exec_command(cmd)
print("SYSTEM_API FUNCTIONS:\n", stdout.read().decode())

ssh.close()
