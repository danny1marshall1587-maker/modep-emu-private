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
echo blokas | sudo -S systemctl restart modep-mod-ui.service
"""
stdin, stdout, stderr = ssh.exec_command(cmd)
print("OUT:", stdout.read().decode())
print("ERR:", stderr.read().decode())

ssh.close()
print("Updated session.py and restarted modep-mod-ui")
