import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.148', username='patch', password='blokas')

cmd = """python3 -c "
import os, sys
os.environ['MOD_DATA_DIR'] = '/var/modep'
os.environ['MOD_USER_PEDALBOARDS_DIR'] = '/var/modep/pedalboards'
import tornado4 as tornado
sys.modules['tornado'] = tornado
import mod.session

s = mod.session.SESSION
# Initialize host's userbanks like end_session / init
from mod.bank import list_banks
s.host.userbanks = list_banks([], True, False)
print('User banks loaded count:', len(s.host.userbanks))
for i, b in enumerate(s.host.userbanks):
    print('Bank', i, b['title'])
    for j, p in enumerate(b['pedalboards']):
        print('   ', j, p['title'], p['bundle'])
" """
stdin, stdout, stderr = ssh.exec_command(cmd)
print(stdout.read().decode())
print(stderr.read().decode())

ssh.close()
