import urllib.request

try:
    with urllib.request.urlopen("http://127.0.0.1:18181/js/templates.js", timeout=5) as resp:
        content = resp.read().decode('utf-8', errors='ignore')
        print("templates.js size:", len(content))
        print("patchstorage_plugin in TEMPLATES:", "TEMPLATES['patchstorage_plugin']" in content)
        print("patchstorage_plugin_info in TEMPLATES:", "TEMPLATES['patchstorage_plugin_info']" in content)
        print("cloudplugin in TEMPLATES:", "TEMPLATES['cloudplugin']" in content)
        print("cloudplugin_info in TEMPLATES:", "TEMPLATES['cloudplugin_info']" in content)
except Exception as e:
    print("Error:", e)
