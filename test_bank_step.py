import paramiko

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.1.148', username='patch', password='blokas')

cmd = """python3 -c "
import os, sys
os.environ['MOD_DATA_DIR'] = '/var/modep'
os.environ['MOD_USER_PEDALBOARDS_DIR'] = '/var/modep/pedalboards'
import tornado4 as tornado
sys.modules['tornado'] = tornado
import mod.session

s = mod.session.SESSION
from mod.bank import list_banks, get_last_bank_and_pedalboard
s.host.userbanks = list_banks([], True, False)

def navigate_bank_pedalboard(direction):
    last_bank, last_pb = get_last_bank_and_pedalboard()
    print('Current state:', last_bank, last_pb)
    if not s.host.userbanks:
        print('No userbanks!')
        return False

    # last_bank: -2 corresponds to bank index 0
    # userbanks index = last_bank if 0 <= last_bank < len(s.host.userbanks) else 0
    bank_idx = last_bank if (0 <= last_bank < len(s.host.userbanks)) else 0
    bank = s.host.userbanks[bank_idx]
    pedalboards = bank.get('pedalboards', [])
    if not pedalboards:
        print('Bank is empty!')
        return False

    cur_idx = 0
    for idx, pb in enumerate(pedalboards):
        if pb['bundle'] == last_pb:
            cur_idx = idx
            break

    if direction > 0:
        new_idx = (cur_idx + 1) % len(pedalboards)
    else:
        new_idx = (cur_idx - 1) % len(pedalboards)

    print('Switching from', cur_idx, pedalboards[cur_idx]['title'], 'to', new_idx, pedalboards[new_idx]['title'])
    # In MODEP, hmi_load_bank_pedalboard takes (bank_id, pedalboard_index, callback, from_hmi)
    # where bank_id for first user bank is userbanks_offset + bank_idx = 1 + 0 = 1, or offset
    # Wait, let's verify what bank_id hmi_load_bank_pedalboard expects when supports_factory_banks is False!
    return bank_idx, new_idx

navigate_bank_pedalboard(1)
navigate_bank_pedalboard(-1)
" """
stdin, stdout, stderr = ssh.exec_command(cmd)
print(stdout.read().decode())
print(stderr.read().decode())

ssh.close()
