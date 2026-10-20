import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.148', username='patch', password='blokas')

cmd = "python3 -c \"import mod.host; import inspect; print('\n'.join([m for m in dir(mod.host.Host) if 'bank' in m or 'pedal' in m]))\""
stdin, stdout, stderr = ssh.exec_command(cmd)
print("HOST METHODS:\n", stdout.read().decode())

cmd = "grep -n -C 5 'def hmi_load_bank_pedalboard' /usr/lib/python3/dist-packages/mod/host.py"
stdin, stdout, stderr = ssh.exec_command(cmd)
print("HMI_LOAD_BANK_PEDALBOARD:\n", stdout.read().decode())

ssh.close()
