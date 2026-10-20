import os
import subprocess

base = r'c:\Users\danny\Documents\modep emu'
gpp = os.path.join(base, 'tools', 'w64devkit', 'bin', 'g++.exe')
inc = os.path.join(base, 'plugins', 'include')

compiler_bin = os.path.join(base, 'tools', 'w64devkit', 'bin')
env = os.environ.copy()
env['PATH'] = compiler_bin + os.pathsep + env.get('PATH', '')

for p, src, dll in [
    ('cyber-feedback-super.lv2', 'src/cyber_feedback_super.cpp', 'cyber_feedback_super.dll'),
    ('cyber-feedback-super-mono.lv2', 'src/cyber_feedback_super_mono.cpp', 'cyber_feedback_super_mono.dll')
]:
    pdir = os.path.join(base, 'plugins', p)
    src_f = os.path.join(pdir, src)
    dll_f = os.path.join(pdir, dll)
    local_inc = os.path.join(pdir, 'include')
    cmd = [gpp, '-shared', '-O3', '-mavx', '-static', '-std=c++17', f'-I{inc}', f'-I{local_inc}', src_f, '-o', dll_f]
    res = subprocess.run(cmd, capture_output=True, text=True, env=env)
    if res.returncode != 0:
        print(f'ERROR on {p}:', res.stderr)
    else:
        print(f'SUCCESS on {p}: {os.path.getsize(dll_f)} bytes')
