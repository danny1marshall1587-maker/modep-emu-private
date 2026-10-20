import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.148', username='patch', password='blokas')

cmd = """python3 -c "
import sys
import tornado4 as tornado
sys.modules['tornado'] = tornado
import mod.session
s = mod.session.SESSION
print('bank_id in host:', getattr(s.host, 'bank_id', 'NO'))
print('current_bank_id:', getattr(s.host, 'current_bank_id', 'NO'))
print('userbanks:', len(s.host.userbanks))
print('userbanks details:', s.host.userbanks)
print('pedalboard_path in host:', getattr(s.host, 'pedalboard_path', 'NO'))
print('all user pedalboards:', len(s.host.alluserpedalboards))
" """
stdin, stdout, stderr = ssh.exec_command(cmd)
print("OUT:\n", stdout.read().decode())
print("ERR:\n", stderr.read().decode())

ssh.close()
