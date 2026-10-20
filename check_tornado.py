import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.148', username='patch', password='blokas')

cmd = "python3 -c \"import sys; [print(p) for p in sys.path if 'tornado' in p or True]; import tornado; print(tornado.__file__)\""
stdin, stdout, stderr = ssh.exec_command(cmd)
print("OUT:", stdout.read().decode())
print("ERR:", stderr.read().decode())

ssh.close()
