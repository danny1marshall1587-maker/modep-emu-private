import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.148', username='patch', password='blokas')

cmd = """python3 -c "
import sys
import tornado4 as tornado
sys.modules['tornado'] = tornado
import mod.session
s = mod.session.SESSION
print('reset method signature:')
import inspect
print(inspect.signature(s.host.reset))
" """
stdin, stdout, stderr = ssh.exec_command(cmd)
print("OUT:\n", stdout.read().decode())
print("ERR:\n", stderr.read().decode())

ssh.close()
