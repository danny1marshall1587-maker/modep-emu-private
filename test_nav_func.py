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
s.host.factorybanks = []

def nav(direction):
    last_bank, last_pb = get_last_bank_and_pedalboard()
    print('Initial state:', last_bank, last_pb)
    if not s.host.userbanks:
        return False

    # In MODEP, last_bank is bank index (e.g. 0 for first user bank)
    user_bank_idx = last_bank if (0 <= last_bank < len(s.host.userbanks)) else 0
    bank = s.host.userbanks[user_bank_idx]
    pedalboards = bank.get('pedalboards', [])
    if not pedalboards:
        return False

    cur_idx = 0
    for idx, pb in enumerate(pedalboards):
        if pb['bundle'] == last_pb:
            cur_idx = idx
            break

    new_idx = (cur_idx + direction) % len(pedalboards)
    # Target bank id for hmi_load_bank_pedalboard:
    # when supports_factory_banks is False, bank_id 1 corresponds to userbanks[0]
    target_bank_id = (user_bank_idx + 2) if s.host.supports_factory_banks else (user_bank_idx + 1)

    print('Switching in bank %d (%s): index %d (%s) -> index %d (%s), target_bank_id=%d' % (
        user_bank_idx, bank['title'],
        cur_idx, pedalboards[cur_idx]['title'],
        new_idx, pedalboards[new_idx]['title'],
        target_bank_id
    ))
    return target_bank_id, new_idx

nav(1)
nav(-1)
" """
stdin, stdout, stderr = ssh.exec_command(cmd)
print("OUT:\n", stdout.read().decode())
print("ERR:\n", stderr.read().decode())

ssh.close()
