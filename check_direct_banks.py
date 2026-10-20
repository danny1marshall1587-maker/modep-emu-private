import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.148', username='patch', password='blokas')

cmd = """python3 -c "
import os
os.environ['MOD_DATA_DIR'] = '/var/modep'
from mod.bank import get_last_bank_and_pedalboard
from mod import safe_json_load

last_bank, last_pb = get_last_bank_and_pedalboard()
print('get_last_bank_and_pedalboard():', last_bank, last_pb)
banks = safe_json_load('/var/modep/banks.json', list)
print('banks from /var/modep/banks.json:', banks)
" """
stdin, stdout, stderr = ssh.exec_command(cmd)
print("OUT:\n", stdout.read().decode())
print("ERR:\n", stderr.read().decode())

ssh.close()
