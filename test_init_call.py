from modtools.utils import init, get_plugin_info_essentials

init()
uri = 'http://cyber-audio.co.uk/plugins/cyber-evans-bass-pro-mono'
p = get_plugin_info_essentials(uri)
print('URI:', uri)
print('error:', p.get('error'))
print('controlInputs:', len(p.get('controlInputs', [])))
print('monitoredOutputs:', p.get('monitoredOutputs'))

print('\nTesting guitar-midi:')
uri_gm = 'http://moddevices.com/plugins/danny/guitar-midi'
pgm = get_plugin_info_essentials(uri_gm)
print('guitar-midi monitoredOutputs:', pgm.get('monitoredOutputs'))
