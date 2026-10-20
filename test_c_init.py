from modtools import utils
# Call get_plugin_list or similar to see how modtools initializes the world
utils.utils.init()
info = utils.utils.get_plugin_info_essentials(b"http://cyber-audio.co.uk/plugins/cyber-evans-bass-pro-mono")
print("info ptr after init:", bool(info))
if info:
    print("monitoredOutputs:", utils.structToDict(info.contents).get("monitoredOutputs"))
