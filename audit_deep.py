import re
import glob

web_dir = "/home/patch/modep-live-console-release/web"
js_files = glob.glob(f"{web_dir}/js/*.js")

all_js = ""
js_by_file = {}
for jf in js_files:
    with open(jf, "r", encoding="utf-8") as f:
        c = f.read()
        all_js += "\n" + c
        js_by_file[jf.split("/")[-1]] = c

# Find all inline handlers in JS templates and index.html
with open(f"{web_dir}/index.html", "r", encoding="utf-8") as f:
    html = f.read()

handlers = re.findall(r'on(?:click|input|change|dblclick)="([^"]+)"', html + "\n" + all_js)

# Also check functions called across files
for jname, jcode in sorted(js_by_file.items()):
    # find all function foo() declarations inside this file
    funcs = re.findall(r'function\s+([a-zA-Z0-9_]+)\s*\(', jcode)
    print(f"=== {jname} declared functions ({len(funcs)}) ===")
    for fn in funcs:
        # check if exported to window
        is_exported = bool(re.search(rf'window\.{fn}\s*=', all_js))
        if not is_exported:
            print(f"  [NOT ON WINDOW] {fn}")

print("\n=== Checking state objects referenced in inline handlers ===")
state_vars = set()
for h in handlers:
    # find identifiers followed by .
    objs = re.findall(r'\b([a-zA-Z_][a-zA-Z0-9_]*)\.', h)
    for o in objs:
        if o not in {'window', 'document', 'this', 'event', 'Math', 'JSON', 'console', 'location', 'parseFloat', 'parseInt'}:
            state_vars.add(o)

for sv in sorted(state_vars):
    is_win = bool(re.search(rf'window\.{sv}\s*=', all_js))
    print(f"  Object '{sv:25s}' | on window: {is_win}")
