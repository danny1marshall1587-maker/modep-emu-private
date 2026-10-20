import os

paths = [
    r'c:\Users\danny\Documents\modep emu\modep_app\lv2',
    r'C:\Program Files\MOD Desktop\lv2',
    os.path.expanduser(r'~\Documents\MOD Desktop\lv2')
]

for p in paths:
    if os.path.exists(p):
        bundles = [b for b in os.listdir(p) if os.path.isdir(os.path.join(p, b)) and b.endswith('.lv2')]
        print(f"Directory: {p}")
        print(f"  Count: {len(bundles)} LV2 bundles")
        for b in sorted(bundles)[:15]:
            print(f"    - {b}")
        if len(bundles) > 15:
            print(f"    ... and {len(bundles)-15} more")
    else:
        print(f"Directory {p} does not exist.")
