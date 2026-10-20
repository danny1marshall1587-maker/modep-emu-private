import os

f1_path = r"C:\Program Files\MOD Desktop\html\index.html"
f2_path = r"c:\Users\danny\Documents\modep emu\modep_app\html\index.html"

f1 = open(f1_path, 'r', encoding='utf-8').read()
f2 = open(f2_path, 'r', encoding='utf-8').read()

print("patchstorage in Program Files HTML:", "patchstorage" in f1)
print("patchstorage in modep_app HTML:", "patchstorage" in f2)
