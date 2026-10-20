#!/usr/bin/env python3
"""
enable_cloud_and_patchstorage_proxy.py
Adds local high-performance CORS-free proxy endpoints for MOD Cloud and Patchstorage
into modep_app/mod/webserver.py and configures html/index.html.
"""

import os
import sys
import re

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
MODEP_APP_DIR = os.path.join(BASE_DIR, "modep_app")
WEBSERVER_PY = os.path.join(MODEP_APP_DIR, "mod", "webserver.py")
INDEX_HTML = os.path.join(MODEP_APP_DIR, "html", "index.html")

def update_webserver():
    with open(WEBSERVER_PY, "r", encoding="utf-8") as f:
        code = f.read()

    # 1. Ensure urllib imports
    if "import urllib.request" not in code:
        code = code.replace("import time\n", "import time\nimport urllib.request\nimport urllib.parse\n")

    # 2. Add Proxy Handler Classes if not present
    if "class CloudPluginsProxy" not in code:
        proxy_classes = """
# =========================================================================
# MODEP Cloud & Patchstorage Proxy Handlers (Bypasses Browser CORS)
# =========================================================================
_CLOUD_CACHE = {}

class CloudPluginsProxy(web.RequestHandler):
    @gen.coroutine
    def get(self):
        self.set_header("Content-Type", "application/json; charset=UTF-8")
        self.set_header("Access-Control-Allow-Origin", "*")
        now = time.time()
        if 'plugins' in _CLOUD_CACHE and now - _CLOUD_CACHE['plugins']['time'] < 300:
            self.write(_CLOUD_CACHE['plugins']['data'])
            return

        url = "https://api.mod.audio/v2/lv2/plugins"
        req = urllib.request.Request(url, headers={'User-Agent': 'MODEP-Desktop/1.0'})
        try:
            with urllib.request.urlopen(req, timeout=12) as resp:
                data = resp.read()
                _CLOUD_CACHE['plugins'] = {'time': now, 'data': data}
                self.write(data)
        except Exception as e:
            print("[!] Error fetching cloud plugins:", e)
            self.write(json.dumps([]))

class CloudFeaturedPluginsProxy(web.RequestHandler):
    @gen.coroutine
    def get(self):
        self.set_header("Content-Type", "application/json; charset=UTF-8")
        self.set_header("Access-Control-Allow-Origin", "*")
        url = "https://api.mod.audio/v2/lv2/plugins/featured"
        req = urllib.request.Request(url, headers={'User-Agent': 'MODEP-Desktop/1.0'})
        try:
            with urllib.request.urlopen(req, timeout=12) as resp:
                self.write(resp.read())
        except Exception as e:
            self.write(json.dumps([]))

class CloudLabsPluginsProxy(web.RequestHandler):
    @gen.coroutine
    def get(self):
        self.set_header("Content-Type", "application/json; charset=UTF-8")
        self.set_header("Access-Control-Allow-Origin", "*")
        url = "https://api-labs.mod.audio/v2/lv2/plugins"
        req = urllib.request.Request(url, headers={'User-Agent': 'MODEP-Desktop/1.0'})
        try:
            with urllib.request.urlopen(req, timeout=12) as resp:
                self.write(resp.read())
        except Exception as e:
            self.write(json.dumps([]))

class PatchstorageProxy(web.RequestHandler):
    @gen.coroutine
    def get(self, path=None):
        self.set_header("Content-Type", "application/json; charset=UTF-8")
        self.set_header("Access-Control-Allow-Origin", "*")
        qs = self.request.query
        base_url = "https://patchstorage.com/api/beta/patches"
        if path and path.strip():
            target_url = f"{base_url}/{path.strip()}"
        else:
            target_url = f"{base_url}?{qs}" if qs else base_url

        req = urllib.request.Request(target_url, headers={'User-Agent': 'MODEP-Desktop/1.0'})
        try:
            with urllib.request.urlopen(req, timeout=15) as resp:
                self.write(resp.read())
        except Exception as e:
            print("[!] Error proxying Patchstorage:", e)
            self.write(json.dumps([]))
# =========================================================================
"""
        code = code.replace("class TemplateHandler(TimelessRequestHandler):", proxy_classes + "\nclass TemplateHandler(TimelessRequestHandler):")

    # 3. Add URL routes in webserver.py
    routes_marker = '(r"/(index.html)?$", TemplateHandler),'
    proxy_routes = """(r"/cloud/lv2/plugins/?$", CloudPluginsProxy),
            (r"/cloud/lv2/plugins/featured/?$", CloudFeaturedPluginsProxy),
            (r"/cloud/labs/lv2/plugins/?$", CloudLabsPluginsProxy),
            (r"/lv2/plugins/?$", CloudPluginsProxy),
            (r"/lv2/plugins/featured/?$", CloudFeaturedPluginsProxy),
            (r"/patchstorage/patches/?(.*)$", PatchstorageProxy),
            (r"/api/beta/patches/?(.*)$", PatchstorageProxy),
            (r"/(index.html)?$", TemplateHandler),"""

    if "(r\"/cloud/lv2/plugins/?$\", CloudPluginsProxy)" not in code:
        code = code.replace(routes_marker, proxy_routes)

    # 4. Update index context in webserver.py
    code = re.sub(r"'cloud_url':\s*CLOUD_HTTP_ADDRESS,", "'cloud_url': '',", code)
    code = re.sub(r"'cloud_labs_url':\s*CLOUD_LABS_HTTP_ADDRESS,", "'cloud_labs_url': '/cloud/labs',", code)
    code = re.sub(r"'patchstorage_api_url':\s*[^,]+,", "'patchstorage_api_url': '/patchstorage/patches',", code)

    with open(WEBSERVER_PY, "w", encoding="utf-8") as f:
        f.write(code)
    print(" [+] Updated modep_app/mod/webserver.py with Proxy Handlers and URL routes")

def update_index_html():
    with open(INDEX_HTML, "r", encoding="utf-8") as f:
        html = f.read()

    # Ensure SITEURL is local proxy
    html = re.sub(r"SITEURL\s*=\s*'[^']*'", "SITEURL = ''", html)
    html = re.sub(r"CLOUD_LABS_URL\s*=\s*'[^']*'", "CLOUD_LABS_URL = '/cloud/labs'", html)
    html = re.sub(r"PATCHSTORAGE_API_URL\s*=\s*'[^']*'", "PATCHSTORAGE_API_URL = '/patchstorage/patches'", html)

    # Make sure Plugin Store and 'P' buttons are permanently unhidden
    html = html.replace("$('#mod-cloud-plugins').hide()", "$('#mod-cloud-plugins').show()")
    html = html.replace("$('#patchstorage').hide()", "$('#patchstorage').show()")

    with open(INDEX_HTML, "w", encoding="utf-8") as f:
        f.write(html)
    print(" [+] Updated modep_app/html/index.html with local proxy URLs")

if __name__ == '__main__':
    update_webserver()
    update_index_html()
    print("\nCloud & Patchstorage Proxy fully enabled!")
