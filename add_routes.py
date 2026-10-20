import sys

with open("/usr/lib/python3/dist-packages/mod/webserver.py", "r") as f:
    code = f.read()

target = '(r"/live/?", LiveDeckPageHandler),'
replacement = """(r"/live/?", LiveDeckPageHandler),
            (r"/mixer/?", web.RedirectHandler, {"url": "/mixer/index.html"}),
            (r"/daw/?", web.RedirectHandler, {"url": "/daw/index.html"}),"""

if target in code and "/mixer/?" not in code:
    code = code.replace(target, replacement, 1)
    with open("/usr/lib/python3/dist-packages/mod/webserver.py", "w") as f:
        f.write(code)
    print("ROUTES_ADDED")
else:
    print("ALREADY_PRESENT_OR_TARGET_NOT_FOUND")
