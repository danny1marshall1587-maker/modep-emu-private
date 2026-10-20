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
s.host.factorybanks = []
s.host.alluserpedalboards = []
s.host.allfactorypedalboards = []

print('userbanks:', len(s.host.userbanks))
def callback(ok):
    print('Callback:', ok)

s.host.hmi_load_bank_pedalboard(1, 0, callback, from_hmi=False)
" """
stdin, stdout, stderr = ssh.exec_command(cmd)
print("OUT:\n", stdout.read().decode())
print("ERR:\n", stderr.read().decode())

ssh.close()
