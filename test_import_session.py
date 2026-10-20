import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.148', username='patch', password='blokas')

cmd = """python3 -c "
import sys
# /usr/lib/python3/dist-packages/tornado4 is itself the package!
import tornado4 as tornado
sys.modules['tornado'] = tornado
import tornado.web, tornado.ioloop
import mod.session
print('mod.session loaded successfully!')
" """
stdin, stdout, stderr = ssh.exec_command(cmd)
print("OUT:", stdout.read().decode())
print("ERR:", stderr.read().decode())

ssh.close()
