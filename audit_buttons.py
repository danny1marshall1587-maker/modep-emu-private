import re
import glob

web_dir = "/home/patch/modep-live-console-release/web"
html_path = f"{web_dir}/index.html"
js_files = glob.glob(f"{web_dir}/js/*.js")

with open(html_path, "r", encoding="utf-8") as f:
    html = f.read()

all_js = ""
js_by_file = {}
for jf in js_files:
    with open(jf, "r", encoding="utf-8") as f:
        c = f.read()
        all_js += "\n" + c
        js_by_file[jf.split("/")[-1]] = c

# Extract all onclick/oninput/onchange in index.html AND in JS template literals
html_handlers = re.findall(r'(on(?:click|input|change|dblclick)="([^"]+)")', html)
js_handlers = re.findall(r'(on(?:click|input|change|dblclick)="([^"]+)")', all_js)

# Extract function calls from handlers
def get_func_calls(expr):
    # ignore simple assignments or this.xxx
    calls = re.findall(r'(?:window\.)?([a-zA-Z_][a-zA-Z0-9_]*)\s*\(', expr)
    ignore = {'if', 'parseInt', 'parseFloat', 'String', 'Number', 'Math', 'setTimeout', 'clearTimeout', 'encodeURIComponent', 'JSON', 'alert', 'confirm'}
    return [c for c in calls if c not in ignore]

all_called_funcs = set()
handler_map = []

for full, expr in html_handlers:
    funcs = get_func_calls(expr)
    for fn in funcs:
        all_called_funcs.add(fn)
    handler_map.append(("index.html", expr, funcs))

for full, expr in js_handlers:
    funcs = get_func_calls(expr)
    for fn in funcs:
        all_called_funcs.add(fn)
    handler_map.append(("JS-Template", expr, funcs))

print(f"Total unique functions called by UI buttons/sliders: {len(all_called_funcs)}")
print("-" * 70)

for fn in sorted(all_called_funcs):
    # Check where it is defined on window or globally
    window_defs = []
    local_defs = []
    is_stub = False
    for jname, jcode in js_by_file.items():
        if re.search(rf'window\.{fn}\s*=', jcode):
            window_defs.append(jname)
            # check if it's a console.log stub
            m = re.search(rf'window\.{fn}\s*=\s*(?:window\.{fn}\s*\|\|\s*)?function\([^\)]*\)\s*\{{([^\}}]*)\}}', jcode)
            if m and 'console.log' in m.group(1) and len(m.group(1).strip().splitlines()) <= 2:
                is_stub = True
        if re.search(rf'function\s+{fn}\s*\(', jcode):
            local_defs.append(jname)

    status = "OK (window)" if window_defs else ("LOCAL_ONLY (NOT ON WINDOW!)" if local_defs else "MISSING!")
    if is_stub:
        status += " [WARNING: STUB ONLY?]"
    print(f"{fn:30s} | {status:35s} | win:{window_defs} | loc:{local_defs}")
