from mod import webserver
from modtools.utils import get_plugin_info_essentials, get_plugin_info

uri = 'http://cyber-audio.co.uk/plugins/cyber-evans-bass-pro-mono'
p = get_plugin_info_essentials(uri)
print("monitoredOutputs essentials:", p.get('monitoredOutputs'))

p2 = get_plugin_info(uri)
print("monitoredOutputs full:", p2.get('monitoredOutputs') if p2 else None)
print("gui monitoredOutputs:", p2.get('gui', {}).get('monitoredOutputs') if p2 else None)
