import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.148', username='patch', password='blokas')

cmd = """python3 -c "
with open('/usr/share/mod/html/js/system.js', 'r') as f:
    code = f.read()

idx = code.find('<!-- TAB 3: SYSTEM -->')
if idx != -1:
    print(code[idx:idx+2500])
else:
    idx2 = code.find('cyber-tab-system')
    print(code[idx2-100:idx2+1500])
" """
stdin, stdout, stderr = ssh.exec_command(cmd)
print(stdout.read().decode())

ssh.close()
