import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.148', username='patch', password='blokas')

cmd = """python3 -c "
import json
with open('/var/modep/bank_nav_midi.json', 'w') as f:
    json.dump({'next_cc': -1, 'prev_cc': -1, 'channel': 0}, f)
print('bank_nav_midi.json initialized')
" """
stdin, stdout, stderr = ssh.exec_command(cmd)
print(stdout.read().decode())
print(stderr.read().decode())

ssh.close()
