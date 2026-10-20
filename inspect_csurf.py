import subprocess

out = subprocess.check_output(['objdump', '-d', '--start-address=0x542ebc', '--stop-address=0x542f20', '/opt/REAPER/reaper']).decode('latin1', errors='ignore')
print(out)
