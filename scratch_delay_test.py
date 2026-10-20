
import ctypes

class LV2_Descriptor(ctypes.Structure):
    pass

LV2_Descriptor._fields_ = [
    ('URI', ctypes.c_char_p),
    ('instantiate', ctypes.CFUNCTYPE(ctypes.c_void_p, ctypes.c_void_p, ctypes.c_double, ctypes.c_char_p, ctypes.c_void_p)),
    ('connect_port', ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_uint32, ctypes.c_void_p)),
    ('activate', ctypes.CFUNCTYPE(None, ctypes.c_void_p)),
    ('run', ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_uint32)),
    ('deactivate', ctypes.CFUNCTYPE(None, ctypes.c_void_p)),
    ('cleanup', ctypes.CFUNCTYPE(None, ctypes.c_void_p)),
    ('extension_data', ctypes.CFUNCTYPE(ctypes.c_void_p, ctypes.c_char_p))
]

audio_buf = (ctypes.c_float * 128)()
ctrl_buf = (ctypes.c_float)(1.0)

dll_path = r'C:\Users\danny\Documents\MOD Desktop\lv2\cyber-puresustain-delay.lv2\cyber_puresustain_delay.dll'
p_dir = r'C:\Users\danny\Documents\MOD Desktop\lv2\cyber-puresustain-delay.lv2'

lib = ctypes.CDLL(dll_path)
lib.lv2_descriptor.restype = ctypes.POINTER(LV2_Descriptor)
desc_ptr = lib.lv2_descriptor(0)
desc = desc_ptr.contents

print('URI from descriptor:', desc.URI.decode())
inst = desc.instantiate(desc_ptr, 48000.0, p_dir.encode('utf-8'), None)
print('Instantiated instance:', inst)

# connect all 11 ports:
# 0: in_l, 1: in_r, 2: out_l, 3: out_r, 4: bypass, 5: time, 6: feedback, 7: melt, 8: tone, 9: duck, 10: mix
for p in range(11):
    if p < 4:
        desc.connect_port(inst, p, ctypes.byref(audio_buf))
    else:
        desc.connect_port(inst, p, ctypes.byref(ctrl_buf))

if desc.activate:
    desc.activate(inst)
print('Activated!')

if desc.run:
    desc.run(inst, 128)
print('Run 128 frames OK!')

if desc.deactivate:
    desc.deactivate(inst)
if desc.cleanup:
    desc.cleanup(inst)
print('Cleanup OK!')
