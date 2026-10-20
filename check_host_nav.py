import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.148', username='patch', password='blokas')

cmd = "grep -n -C 5 'def setNavigateWithFootswitches' /usr/lib/python3/dist-packages/mod/host.py"
stdin, stdout, stderr = ssh.exec_command(cmd)
print("SET NAVIGATE:\n", stdout.read().decode())

cmd = "grep -n -C 5 'isBankFootswitchNavigationOn' /usr/lib/python3/dist-packages/mod/host.py"
stdin, stdout, stderr = ssh.exec_command(cmd)
print("IS BANK FOOTSWITCH ON:\n", stdout.read().decode())

cmd = "grep -n -C 5 'hmi_bank_navigation' /usr/lib/python3/dist-packages/mod/host.py"
stdin, stdout, stderr = ssh.exec_command(cmd)
print("HMI BANK NAV:\n", stdout.read().decode())

ssh.close()
