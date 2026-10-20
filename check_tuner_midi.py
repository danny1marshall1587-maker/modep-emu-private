import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.148', username='patch', password='blokas')

cmd = "python3 -c \"import mod.session; print([x for x in dir(mod.session) if 'Midi' in x or 'Tuner' in x])\""
stdin, stdout, stderr = ssh.exec_command(cmd)
print("SESSION SYMBOLS:", stdout.read().decode())

cmd = "grep -n -C 10 'tuner_midi' /usr/lib/python3/dist-packages/mod/session.py"
stdin, stdout, stderr = ssh.exec_command(cmd)
print("TUNER MIDI GREP:\n", stdout.read().decode())

cmd = "cat /var/modep/tuner_midi.json 2>/dev/null || echo 'No tuner_midi.json'"
stdin, stdout, stderr = ssh.exec_command(cmd)
print("TUNER MIDI JSON:\n", stdout.read().decode())

ssh.close()
