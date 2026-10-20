import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.148', username='patch', password='blokas')

cmd = """python3 -c "
import mod.session
import inspect
s = mod.session.SESSION
print('Host attrs:', [x for x in dir(s.host) if 'bank' in x.lower() or 'pedal' in x.lower()])
print('userbanks:', getattr(s.host, 'userbanks', None))
print('pedalboard_path:', getattr(s.host, 'pedalboard_path', None))
print('bank_id:', getattr(s.host, 'bank_id', None))
" """
stdin, stdout, stderr = ssh.exec_command(cmd)
print(stdout.read().decode())
print(stderr.read().decode())

ssh.close()
