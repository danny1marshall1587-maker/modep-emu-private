import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.148', username='patch', password='blokas')

cmd = """python3 -c "
import os
os.environ['MOD_DATA_DIR'] = '/var/modep'
from mod.settings import USER_BANKS_JSON_FILE, LAST_STATE_JSON_FILE
print('USER_BANKS_JSON_FILE:', USER_BANKS_JSON_FILE)
print('LAST_STATE_JSON_FILE:', LAST_STATE_JSON_FILE)
" """
stdin, stdout, stderr = ssh.exec_command(cmd)
print("OUT:\n", stdout.read().decode())
print("ERR:\n", stderr.read().decode())

ssh.close()
