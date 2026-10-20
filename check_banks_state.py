import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.148', username='patch', password='blokas')

cmd = """python3 -c "
import json
with open('/var/modep/last.json') as f:
    print('last.json:', json.load(f))
with open('/var/modep/banks.json') as f:
    print('banks.json:', json.load(f))
" """
stdin, stdout, stderr = ssh.exec_command(cmd)
print(stdout.read().decode())
print(stderr.read().decode())

ssh.close()
