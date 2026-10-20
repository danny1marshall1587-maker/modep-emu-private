import subprocess

out = subprocess.check_output(['objdump', '-d', '--start-address=0x8840', '--stop-address=0x8a90', '/usr/lib/python3/dist-packages/modtools/libmod_utils.so']).decode('latin1', errors='ignore')
print(out)
