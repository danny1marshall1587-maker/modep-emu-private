import os
from modtools.utils import get_plugin_info_essentials

# Check all plugins known to modtools
import subprocess
out = subprocess.check_output(['find', '/var/modep/lv2', '/usr/lib/lv2', '-name', 'modgui.ttl'], stderr=subprocess.DEVNULL).decode('utf-8')
for path in out.splitlines():
    with open(path, 'r', errors='ignore') as f:
        content = f.read()
        if 'monitoredOutputs' in content:
            print("Found in modgui.ttl:", path)
            for line in content.splitlines():
                if 'monitoredOutputs' in line or 'symbol' in line:
                    print("  ", line)

print("--- Testing bolliedelay or modmeter ---")
for uri in [
    'http://moddevices.com/plugins/mod-devel/mod-meter-peak',
    'http://moddevices.com/plugins/mod-devel/bolliedelay',
    'http://polyeffects.com/lv2/poly-meter',
    'http://gareus.org/oss/lv2/meters#EBUr128meter'
]:
    p = get_plugin_info_essentials(uri)
    if p and not p.get('error'):
        print(uri, "monitoredOutputs:", p.get('monitoredOutputs'))
