from modtools import utils
info = utils.utils.get_plugin_info_essentials(b"http://cyber-audio.co.uk/plugins/cyber-evans-bass-pro-mono")
print("info ptr:", bool(info))
if info:
    mo = info.contents.monitoredOutputs
    print("mo ptr:", bool(mo))
    if mo:
        i = 0
        while mo[i]:
            print("monitored:", mo[i])
            i += 1
