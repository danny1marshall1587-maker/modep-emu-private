import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.148', username='patch', password='blokas')

sftp = ssh.open_sftp()
sftp.get('/usr/share/mod/html/js/system.js', 'c:\\Users\\danny\\Documents\\modep emu\\system_pi.js')
sftp.close()
ssh.close()
print('system.js fetched successfully')
