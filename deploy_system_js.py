import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.148', username='patch', password='blokas')

cmd = "cp /usr/share/mod/html/js/system.js /usr/share/mod/html/js/system.js.bak_nav"
stdin, stdout, stderr = ssh.exec_command(cmd)
print("Backup of system.js created")

sftp = ssh.open_sftp()
sftp.put('c:\\Users\\danny\\Documents\\modep emu\\system_pi.js', '/tmp/system_pi.js')
sftp.close()

cmd = """
echo blokas | sudo -S cp /tmp/system_pi.js /usr/share/mod/html/js/system.js
echo blokas | sudo -S chmod 644 /usr/share/mod/html/js/system.js
"""
stdin, stdout, stderr = ssh.exec_command(cmd)
print("SUDO CP SYSTEM.JS OUT:", stdout.read().decode())
print("SUDO CP SYSTEM.JS ERR:", stderr.read().decode())

cmd = "echo blokas | sudo -S systemctl restart modep-mod-ui.service"
stdin, stdout, stderr = ssh.exec_command(cmd)
print("RESTART UI OUT:", stdout.read().decode())
print("RESTART UI ERR:", stderr.read().decode())

ssh.close()
print("Deployed system.js and restarted mod-ui service")
