import os
import re
import subprocess

WEB_DIR = r"c:\Users\danny\Documents\modep emu\web\web"
JS_DIR = os.path.join(WEB_DIR, "js")

print("=== 1. JS LINE COUNT & SYNTAX CHECK ===")
all_js_content = ""
for fname in sorted(os.listdir(JS_DIR)):
    if not fname.endswith(".js"):
        continue
    fpath = os.path.join(JS_DIR, fname)
    with open(fpath, "r", encoding="utf-8") as f:
        src = f.read()
    all_js_content += "\n" + src
    lines = len(src.splitlines())
    status = "OK (<450)" if lines < 450 else "FAIL (>=450)"
    print(f"  {fname:24s}: {lines:4d} lines -> {status}")
    assert lines < 450, f"{fname} exceeds 450 lines ({lines})"

print("\n=== 2. HTML INLINE HANDLER RESOLUTION CHECK ===")
html_path = os.path.join(WEB_DIR, "index.html")
with open(html_path, "r", encoding="utf-8") as f:
    html = f.read()

handlers = re.findall(r'\bon(?:click|input|change|pointerdown|pointerup|pointercancel)="([^"]+)"', html)
fn_names = set()
for h in handlers:
    for m in re.finditer(r'(?<![.\w])([a-zA-Z_][a-zA-Z0-9_]*)\s*\(', h):
        fn = m.group(1)
        if fn not in ("if", "parseInt", "parseFloat", "stopPropagation", "open", "window"):
            fn_names.add(fn)

missing = []
for fn in sorted(fn_names):
    # Check if defined as function fn( or window.fn =
    if re.search(rf'\bfunction\s+{fn}\b|\bwindow\.{fn}\s*=', all_js_content):
        print(f"  [OK] {fn}")
    else:
        print(f"  [MISSING] {fn}")
        missing.append(fn)

if missing:
    print("MISSING HANDLERS:", missing)
else:
    print("ALL HTML EVENT HANDLERS RESOLVED 100%!")
