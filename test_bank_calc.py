import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.148', username='patch', password='blokas')

cmd = """python3 -c "
import sys
import tornado4 as tornado
sys.modules['tornado'] = tornado
import mod.session
import mod.bank
from mod.bank import get_last_bank_and_pedalboard

bank_id, pb_path = get_last_bank_and_pedalboard()
print('get_last_bank_and_pedalboard returns:', bank_id, pb_path)
" """
stdin, stdout, stderr = ssh.exec_command(cmd)
print("OUT:\n", stdout.read().decode())
print("ERR:\n", stderr.read().decode())

ssh.close()
