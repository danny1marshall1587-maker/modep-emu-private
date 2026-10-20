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
inst_id = s.host.mapper.get_id('/pedalboard')
print('/pedalboard instance id:', inst_id)
print('pluginData:', s.host.plugins.get(inst_id, None))
" """
stdin, stdout, stderr = ssh.exec_command(cmd)
print(stdout.read().decode())

ssh.close()
