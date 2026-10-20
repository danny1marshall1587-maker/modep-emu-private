import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.148', username='patch', password='blokas')

cmd = "ls /usr/lib/python3/dist-packages/tornado4"
stdin, stdout, stderr = ssh.exec_command(cmd)
print("LS TORNADO4:", stdout.read().decode())

cmd = "python3 -c 'import pkg_resources; pkg_resources.require(\"mod==0.99.8\"); import tornado; print(tornado.__file__)'"
stdin, stdout, stderr = ssh.exec_command(cmd)
print("PKG_RESOURCES REQUIRE:\n", stdout.read().decode())

ssh.close()
