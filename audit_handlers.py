import os
import re

WEB_DIR = r"c:\Users\danny\Documents\modep emu\web\web"
JS_DIR = os.path.join(WEB_DIR, "js")

html_text = open(os.path.join(WEB_DIR, "index.html"), "r", encoding="utf-8").read()
js_texts = {}
for fn in sorted(os.listdir(JS_DIR)):
    if fn.endswith(".js"):
        js_texts[fn] = open(os.path.join(JS_DIR, fn), "r", encoding="utf-8").read()

all_js = "\n".join(js_texts.values())

# Find all window.<fn> = or function <fn>( definitions
defined_funcs = set(re.findall(r"window\.([a-zA-Z0-9_]+)\s*=", all_js))
defined_funcs |= set(re.findall(r"function\s+([a-zA-Z0-9_]+)\s*\(", all_js))

# Built-in JS/DOM methods that can appear in inline handlers
builtins = {
    "if", "var", "let", "const", "return", "window", "document", "event", "this",
    "parseInt", "parseFloat", "Math", "Number", "String", "Array", "Object",
    "setTimeout", "clearTimeout", "stopPropagation", "preventDefault", "toggle",
    "open", "toFixed", "toUpperCase", "toLowerCase", "classList", "getElementById"
}

# Extract all inline event handler attributes from index.html and JS template literals
combined_markup = html_text + "\n" + all_js
handlers = re.findall(r'on(?:click|input|change|pointerdown|pointerup|pointercancel)="([^"]+)"', combined_markup)

called_funcs = set()
for h in handlers:
    # find function calls identifier(
    for m in re.finditer(r'(?<![.\w])([a-zA-Z_][a-zA-Z0-9_]*)\s*\(', h):
        fn = m.group(1)
        if fn not in builtins:
            called_funcs.add(fn)

missing = sorted(called_funcs - defined_funcs)
print("Total unique handler functions called in HTML + JS modals:", len(called_funcs))
print("Missing / unexported handler functions:", missing)

# Also check if any function is defined inside an IIFE with `function foo()` but NOT exported to `window.foo`!
window_exports = set(re.findall(r"window\.([a-zA-Z0-9_]+)\s*=", all_js))
unexported_calls = sorted(called_funcs - window_exports)
print("Called in inline DOM attributes but not on window.*:", unexported_calls)
