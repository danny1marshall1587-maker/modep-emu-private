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
from mod.bank import list_banks
s.host.userbanks = list_banks([], True, False)

# Current state
bank_id = 0
pedalboards = s.host.userbanks[bank_id]['pedalboards']
cur_path = '/var/modep/pedalboards/acoustic_2_.pedalboard'

# Find cur index
cur_idx = 0
for i, p in enumerate(pedalboards):
    if p['bundle'] == cur_path:
        cur_idx = i
        break

next_idx = (cur_idx + 1) % len(pedalboards)
prev_idx = (cur_idx - 1) % len(pedalboards)

print('Current PB:', pedalboards[cur_idx]['title'])
print('Next PB:', pedalboards[next_idx]['title'])
print('Prev PB:', pedalboards[prev_idx]['title'])
" """
stdin, stdout, stderr = ssh.exec_command(cmd)
print(stdout.read().decode())

ssh.close()
