import sys

with open("/usr/lib/python3/dist-packages/mod/webserver.py", "r", encoding="utf-8") as f:
    content = f.read()

target = "class TimelessStaticFileHandler(web.StaticFileHandler):"
replacement = """class DawStaticFileHandler(web.StaticFileHandler):
    def set_default_headers(self):
        self.set_header("Cross-Origin-Opener-Policy", "same-origin")
        self.set_header("Cross-Origin-Embedder-Policy", "require-corp")

class TimelessStaticFileHandler(web.StaticFileHandler):"""

if target in content and "class DawStaticFileHandler" not in content:
    content = content.replace(target, replacement, 1)

old_daw = '(r"/daw/(.*)", web.StaticFileHandler, {"path": "/usr/share/mod/html/daw", "default_filename": "index.html"})'
new_daw = '(r"/daw/(.*)", DawStaticFileHandler, {"path": "/usr/share/mod/html/daw", "default_filename": "index.html"})'

if old_daw in content:
    content = content.replace(old_daw, new_daw, 1)

with open("/usr/lib/python3/dist-packages/mod/webserver.py", "w", encoding="utf-8") as f:
    f.write(content)

print("SUCCESS")
