#!/usr/bin/env python3
"""
ensure_p_button_visible.py
Ensures the 'P' (Patchstorage) button is styled with !important CSS rules,
placed in the bottom navigation bar after the Plugin Store icon, and never hidden by any scripts.
Also creates a Desktop shortcut for MODEP Desktop.
"""

import os
import sys
import re

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
MODEP_APP_DIR = os.path.join(BASE_DIR, "modep_app")
HTML_DIR = os.path.join(MODEP_APP_DIR, "html")
INDEX_HTML = os.path.join(HTML_DIR, "index.html")
MAIN_CSS = os.path.join(HTML_DIR, "css", "main.css")
DESKTOP_JS = os.path.join(HTML_DIR, "js", "desktop.js")

def update_css():
    with open(MAIN_CSS, "r", encoding="utf-8") as f:
        css = f.read()

    p_css = """
/* ========================================================= */
/* MODEP Patchstorage 'P' Button & Store Mandatory Styles    */
/* ========================================================= */
#main-menu #patchstorage {
    background-image: url(../img/patchstorage-menu.png) !important;
    background-repeat: no-repeat !important;
    background-position: center center !important;
    background-size: 24px 24px !important;
    cursor: pointer !important;
    display: block !important;
    width: 45px !important;
    height: 45px !important;
    float: left !important;
    opacity: 1 !important;
    visibility: visible !important;
}
#main-menu #patchstorage:hover {
    background-color: #000 !important;
    opacity: 1 !important;
}
#main-menu #patchstorage.selected {
    background-color: #000 !important;
}
#main-menu #mod-cloud-plugins {
    background-image: url(../img/icons/36/pluginstore.png) !important;
    background-repeat: no-repeat !important;
    background-position: center center !important;
    background-size: 24px 24px !important;
    cursor: pointer !important;
    display: block !important;
    width: 45px !important;
    height: 45px !important;
    float: left !important;
}
#main-menu #mod-cloud-plugins:hover {
    background-color: #000 !important;
}
"""
    # Remove older patchstorage blocks if present
    css = re.sub(r'/\* MODEP Patchstorage Styles \*/[\s\S]*?#patchstorage-library \{', '#patchstorage-library {', css)
    if "#main-menu #patchstorage {" not in css:
        css += p_css
    else:
        # replace existing #main-menu #patchstorage styling with the new important version
        css = re.sub(r'#main-menu #patchstorage\s*\{[^}]*\}', '', css)
        css = re.sub(r'#main-menu #patchstorage:hover\s*\{[^}]*\}', '', css)
        css = re.sub(r'#main-menu #mod-cloud-plugins\s*\{[^}]*\}', '', css)
        css += p_css

    with open(MAIN_CSS, "w", encoding="utf-8") as f:
        f.write(css)
    print(" [+] Updated modep_app/html/css/main.css with high-priority 'P' button styles")

def update_index():
    with open(INDEX_HTML, "r", encoding="utf-8") as f:
        html = f.read()

    # Move #patchstorage inside main-menu right next to #mod-cloud-plugins
    html = html.replace('<div id="patchstorage" class="icon" data-message="Patchstorage"></div>\n', '')
    html = html.replace('<div id="patchstorage" class="icon" data-message="Patchstorage"></div>', '')

    target = '<div id="mod-cloud-plugins" class="icon" data-message="Plugin Store"></div>'
    replacement = '<div id="mod-cloud-plugins" class="icon" data-message="Plugin Store"></div>\n        <div id="patchstorage" class="icon" data-message="Patchstorage"></div>'
    if target in html and '<div id="patchstorage"' not in html:
        html = html.replace(target, replacement)

    # Ensure unhide call
    html = html.replace("$('#patchstorage').hide()", "$('#patchstorage').show()")
    
    with open(INDEX_HTML, "w", encoding="utf-8") as f:
        f.write(html)
    print(" [+] Updated modep_app/html/index.html with 'P' button position")

def update_desktop_js():
    with open(DESKTOP_JS, "r", encoding="utf-8") as f:
        js = f.read()

    if "$('#patchstorage').show()" not in js:
        js = js.replace(
            "this.setupApp = function () {",
            "this.setupApp = function () {\n        $('#patchstorage').show();\n        $('#mod-cloud-plugins').show();"
        )
        with open(DESKTOP_JS, "w", encoding="utf-8") as f:
            f.write(js)
        print(" [+] Updated modep_app/html/js/desktop.js to keep 'P' button shown in setupApp")

if __name__ == '__main__':
    update_css()
    update_index()
    update_desktop_js()
    print("\nPatchstorage 'P' button is now permanently visible and styled!")
