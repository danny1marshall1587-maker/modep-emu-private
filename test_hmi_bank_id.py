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

print('supports_factory_banks:', s.host.supports_factory_banks)
print('numUserBanks:', len(s.host.userbanks))

# Let's see what hmi_load_bank_pedalboard checks:
# if self.supports_factory_banks: ...
# else:
#     if bank_id == 0:
#         pedalboards = self.alluserpedalboards
#     else:
#         pedalboards = self.userbanks[bank_id - 1]['pedalboards']
# So for userbank[0] ('live'), bank_id = 1 !
# And pedalboard_index = 0 ('Guitar main') or 1 ('acoustic (2)')
bank_id = 1
pb_index = 0
pedalboards = s.host.userbanks[bank_id - 1]['pedalboards']
print('Bank target:', s.host.userbanks[bank_id - 1]['title'])
print('PB target:', pedalboards[pb_index]['title'], pedalboards[pb_index]['bundle'])
" """
stdin, stdout, stderr = ssh.exec_command(cmd)
print(stdout.read().decode())

ssh.close()
