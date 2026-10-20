import os

extracted_dir = r"c:\Users\danny\Documents\modep emu\extracted_modep"
for root, dirs, files in os.walk(extracted_dir):
    rel = os.path.relpath(root, extracted_dir)
    depth = rel.count(os.sep)
    if depth <= 2:
        print(f"[{depth}] {rel} -> {len(dirs)} dirs, {len(files)} files")
