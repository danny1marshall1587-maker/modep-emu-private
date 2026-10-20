with open('/usr/lib/python3/dist-packages/mod/webserver.py', 'r') as f:
    text = f.read()

target = '''            (r"/mixer/?", web.RedirectHandler, {"url": "/mixer/index.html"}),
            (r"/daw/?", web.RedirectHandler, {"url": "/daw/index.html"}),'''

replacement = '''            (r"/mixer/?", web.RedirectHandler, {"url": "/mixer/index.html"}),
            (r"/mixer/(.*)", web.StaticFileHandler, {"path": "/usr/share/mod/html/mixer", "default_filename": "index.html"}),
            (r"/daw/?", web.RedirectHandler, {"url": "/daw/index.html"}),
            (r"/daw/(.*)", web.StaticFileHandler, {"path": "/usr/share/mod/html/daw", "default_filename": "index.html"}),'''

if target in text:
    text = text.replace(target, replacement, 1)
    with open('/usr/lib/python3/dist-packages/mod/webserver.py', 'w') as f:
        f.write(text)
    print("ROUTES_REPLACED_OK")
else:
    print("TARGET_NOT_FOUND")
