import sys
from modtools.utils import get_plugin_info_essentials

uri = sys.argv[1] if len(sys.argv) > 1 else 'http://cyber-audio.co.uk/plugins/cyber-evans-bass-pro-mono'
p = get_plugin_info_essentials(uri)
print('URI:', uri)
print('keys:', list(p.keys()) if p else None)
print('error:', p.get('error'))
print('controlInputs count:', len(p.get('controlInputs', [])))
print('monitoredOutputs:', p.get('monitoredOutputs'))
