import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.148', username='patch', password='blokas')

cmd = """python3 -c "
with open('/usr/lib/python3/dist-packages/mod/session.py', 'r') as f:
    code = f.read()

idx1 = code.find('class TunerMidiListener')
idx2 = code.find('class Session')
print(code[idx1:idx2])
" """
stdin, stdout, stderr = ssh.exec_command(cmd)
print(stdout.read().decode())

ssh.close()
