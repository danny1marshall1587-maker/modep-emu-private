import os
import subprocess
import sys

base_dir = r"C:\Users\danny\Documents\MOD Desktop\lv2"
plugins = [
    'harmonic-tremolo.lv2', 'cyber-denoiser.lv2', 'galaxy-strobe-tune.lv2', 
    'dimension-c.lv2', 'Dimension_IV.lv2', 'Dearmondo610.lv2', 'guitar-midi.lv2', 
    'bluesbreaker.lv2', 'aether.lv2', 'nam-loader.lv2', 'cyber-hum-killer.lv2', 
    'smart-fizz-killer.lv2', 'cyber-expression-cv.lv2', 'aelapse.lv2'
]

test_template = """
import ctypes
import os

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
ctrl_buf = [(ctypes.c_float)(1.0) for _ in range(64)]

dll_path = r"{dll_path}"
p_dir = r"{p_dir}"

lib = ctypes.CDLL(dll_path)
lib.lv2_descriptor.restype = ctypes.POINTER(LV2_Descriptor)
desc_ptr = lib.lv2_descriptor(0)
desc = desc_ptr.contents

inst = desc.instantiate(desc_ptr, 48000.0, p_dir.encode('utf-8'), None)
for p in range(32):
    if desc.connect_port:
        desc.connect_port(inst, p, ctypes.byref(audio_buf))

if desc.activate:
    desc.activate(inst)
if desc.run:
    desc.run(inst, 128)
if desc.deactivate:
    desc.deactivate(inst)
if desc.cleanup:
    desc.cleanup(inst)
print("SUCCESS")
"""

print("Testing each plugin individually in separate process:")
for p in plugins:
    p_dir = os.path.join(base_dir, p)
    if not os.path.exists(p_dir):
        print(f"Skipping {p} (not found)")
        continue
    dlls = [f for f in os.listdir(p_dir) if f.endswith('.dll') and not f.endswith('_ui.dll')]
    for d in dlls:
        dll_path = os.path.join(p_dir, d)
        script_code = test_template.format(dll_path=dll_path, p_dir=p_dir)
        with open("scratch_test.py", "w", encoding="utf-8") as f:
            f.write(script_code)
        
        res = subprocess.run([sys.executable, "scratch_test.py"], capture_output=True, text=True)
        if res.returncode == 0 and "SUCCESS" in res.stdout:
            print(f"  [PASS] {p} ({d})")
        else:
            print(f"  [CRASH/FAIL] {p} ({d}) -> exit code {res.returncode}")
            if res.stderr:
                print(f"      STDERR: {res.stderr.strip()[:200]}")
            if res.stdout:
                print(f"      STDOUT: {res.stdout.strip()[:200]}")

if os.path.exists("scratch_test.py"):
    os.remove("scratch_test.py")
