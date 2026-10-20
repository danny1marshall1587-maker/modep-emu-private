import os
import subprocess

base = r'c:\Users\danny\Documents\modep emu'
gpp = os.path.join(base, 'tools', 'w64devkit', 'bin', 'g++.exe')
inc = os.path.join(base, 'plugins', 'include')
local_inc = os.path.join(base, 'plugins', 'cyber-feedback-super.lv2', 'include')

compiler_bin = os.path.join(base, 'tools', 'w64devkit', 'bin')
env = os.environ.copy()
env['PATH'] = compiler_bin + os.pathsep + env.get('PATH', '')

test_src = os.path.join(base, 'tests', 'test_cyber_feedback_super.cpp')
test_exe = os.path.join(base, 'tests', 'test_cyber_feedback_super.exe')

cmd = [gpp, '-O3', '-std=c++17', f'-I{inc}', f'-I{local_inc}', test_src, '-o', test_exe]
res = subprocess.run(cmd, capture_output=True, text=True, env=env)
if res.returncode != 0:
    print('Compilation error:', res.stderr)
else:
    print('Running simulation test...')
    run_res = subprocess.run([test_exe], capture_output=True, text=True)
    print(run_res.stdout)
    if run_res.stderr:
        print('Stderr:', run_res.stderr)
