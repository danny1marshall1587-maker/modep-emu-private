import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.148', username='patch', password='blokas')

cmd = "cp /usr/lib/python3/dist-packages/mod/webserver.py /usr/lib/python3/dist-packages/mod/webserver.py.bak_nav"
stdin, stdout, stderr = ssh.exec_command(cmd)
print("Backup of webserver.py created")

sftp = ssh.open_sftp()
sftp.get('/usr/lib/python3/dist-packages/mod/webserver.py', 'c:\\Users\\danny\\Documents\\modep emu\\webserver_pi.py')
sftp.close()
ssh.close()
print("webserver.py downloaded")
