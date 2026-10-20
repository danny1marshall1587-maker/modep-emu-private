import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.148', username='patch', password='blokas')

sftp = ssh.open_sftp()
sftp.get('/usr/lib/python3/dist-packages/mod/session.py', 'c:\\Users\\danny\\Documents\\modep emu\\session_pi.py')
sftp.close()
ssh.close()
print('session.py fetched successfully')
