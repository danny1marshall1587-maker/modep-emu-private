import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.148', username='patch', password='blokas')

cmd = """python3 -c "
import sys
sys.path.insert(0, '/usr/lib/python3/dist-packages/tornado4')
import tornado
print('Tornado loaded:', tornado.__file__)
" """
stdin, stdout, stderr = ssh.exec_command(cmd)
print("OUT:", stdout.read().decode())
print("ERR:", stderr.read().decode())

ssh.close()
