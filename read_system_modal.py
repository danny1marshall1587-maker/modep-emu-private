import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.148', username='patch', password='blokas')

cmd = """python3 -c "
with open('/usr/share/mod/html/js/system.js', 'r') as f:
    code = f.read()

idx = code.find('CyberSystemManagerUI.prototype.buildModal')
if idx != -1:
    print(code[idx:idx+2500])
else:
    print('buildModal not found, looking for modal HTML:')
    idx2 = code.find('cyber-sys-modal')
    print(code[idx2-200:idx2+1000])
" """
stdin, stdout, stderr = ssh.exec_command(cmd)
print(stdout.read().decode())

ssh.close()
