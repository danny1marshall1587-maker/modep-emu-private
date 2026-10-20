import urllib.request

try:
    with urllib.request.urlopen('http://127.0.0.1:18181', timeout=3) as resp:
        html = resp.read().decode('utf-8', errors='ignore')
        print("HTTP 18181 is running!")
        print("Contains patchstorage:", "patchstorage" in html)
        print("Contains mod-cloud-plugins:", "mod-cloud-plugins" in html)
        idx = html.find('id="main-menu"')
        if idx != -1:
            print("main-menu in live server:")
            print(html[idx:idx+400])
except Exception as e:
    print("HTTP 18181 connection error:", e)
