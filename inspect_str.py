with open('/usr/lib/python3/dist-packages/modtools/libmod_utils.so', 'rb') as f:
    d = f.read()

# 0x1c000 + 0x738 = 0x1c738
print("String at 0x1c738:", d[0x1c738:0x1c750])
