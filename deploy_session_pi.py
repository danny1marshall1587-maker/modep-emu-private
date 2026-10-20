import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.148', username='patch', password='blokas')

sftp = ssh.open_sftp()
sftp.put('c:\\Users\\danny\\Documents\\modep emu\\session_pi.py', '/tmp/session_pi.py')
sftp.close()

cmd = """
echo blokas | sudo -S cp /tmp/session_pi.py /usr/lib/python3/dist-packages/mod/session.py
echo blokas | sudo -S chmod 644 /usr/lib/python3/dist-packages/mod/session.py
"""
stdin, stdout, stderr = ssh.exec_command(cmd)
print("SUDO CP OUT:", stdout.read().decode())
print("SUDO CP ERR:", stderr.read().decode())

cmd = """python3 -c "
import sys
import tornado4 as tornado
sys.modules['tornado'] = tornado
import mod.session
import mod.webserver
print('Backend modules compiled and imported OK!')
" """
stdin, stdout, stderr = ssh.exec_command(cmd)
print("TEST OUT:\n", stdout.read().decode())
print("TEST ERR:\n", stderr.read().decode())

ssh.close()
