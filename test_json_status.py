import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.148', username='patch', password='blokas')

cmd = """python3 -c "
import json
with open('/var/modep/last.json') as f:
    last_data = json.load(f)
with open('/var/modep/banks.json') as f:
    banks_data = json.load(f)

print('last_data:', last_data)
# last_data has: {'bank': -2, 'pedalboard': '...', 'supportsDividers': True}
# bank_idx in userbanks: -2 + 2 = 0
bank_idx = last_data['bank'] + (2 if last_data.get('supportsDividers', False) else 1)
print('computed bank_idx:', bank_idx)
current_bank = banks_data[bank_idx]
print('current_bank:', current_bank['title'], [p['title'] for p in current_bank['pedalboards']])
" """
stdin, stdout, stderr = ssh.exec_command(cmd)
print("OUT:\n", stdout.read().decode())
print("ERR:\n", stderr.read().decode())

ssh.close()
