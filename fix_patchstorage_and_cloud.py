#!/usr/bin/env python3
"""
fix_patchstorage_and_cloud.py
Imports and fully enables the complete MODEP Cloud & Patchstorage ecosystem:
1. Fixes Patchstorage target filtering (removes target_id=null which caused HTTP 400).
2. Sets p.supported = true for all cloud plugins and patches.
3. Fixes container selector matching in patchstorage.js (#patchstorage-content-All & #patch-content-All).
4. Adds category fallbacks and safe label sorting across patchstorage.js.
5. Injects category containers into index.html for both Patchstorage and Plugin Store.
"""

import os
import sys
import re

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
MODEP_APP_DIR = os.path.join(BASE_DIR, "modep_app")
PATCHSTORAGE_JS = os.path.join(MODEP_APP_DIR, "html", "js", "patchstorage.js")
INDEX_HTML = os.path.join(MODEP_APP_DIR, "html", "index.html")

def fix_patchstorage_js():
    with open(PATCHSTORAGE_JS, "r", encoding="utf-8") as f:
        code = f.read()

    # 1. Fix supported platform check: all cloud items are supported on Desktop
    old_supported = """        // check if is supported on this platform
        p.supported = true
        if (p.files && p.files.length > 0) {
            p.supported = false
            p.files.forEach(function(f) {
                if (f.target && f.target.id == PATCHSTORAGE_TARGET_ID) {
                    p.supported = true
                }
            })
        }"""

    new_supported = """        // Desktop MODEP supports all community cloud plugins & patches
        p.supported = true;"""

    if old_supported in code:
        code = code.replace(old_supported, new_supported)
    else:
        # regex replacement if whitespace differed
        code = re.sub(r"// check if is supported on this platform[\s\S]*?p\.supported = true\s*if \(p\.files[\s\S]*?\}\s*\}", "p.supported = true;", code)

    # 2. Fix target_id in query URL
    old_url = "var url = `${base}?per_page=100&platforms=${platform_id}&targets=${target_id}`"
    new_url = "var url = `${base}?per_page=100&platforms=${platform_id}` + (target_id && target_id !== 'null' && target_id !== 'undefined' ? `&targets=${target_id}` : '')"
    if old_url in code:
        code = code.replace(old_url, new_url)

    # 3. Fix safe sorting in renderPlugins
    old_ps_sort = """        // sort plugins by label
        plugins.sort(function (a, b) {
            a = a.label.toLowerCase()
            b = b.label.toLowerCase()
            if (a > b) {
                return 1
            }
            if (a < b) {
                return -1
            }
            return 0
        })"""

    new_ps_sort = """        // sort plugins by label safely
        plugins.sort(function (a, b) {
            var la = ((a && (a.label || a.name || a.title)) || '').toLowerCase();
            var lb = ((b && (b.label || b.name || b.title)) || '').toLowerCase();
            if (la > lb) return 1;
            if (la < lb) return -1;
            return 0;
        });"""

    if old_ps_sort in code:
        code = code.replace(old_ps_sort, new_ps_sort)

    # 4. Fix canvas container references for category filtering
    old_canvas = "'All': self.find('#patch-content-All')"
    new_canvas = "'All': (self.find('#patchstorage-content-All').length ? self.find('#patchstorage-content-All') : self.find('#patch-content-All'))"
    if old_canvas in code:
        code = code.replace(old_canvas, new_canvas)

    old_canvas_cat = "cachedContentCanvas[category] = self.find('#patch-content-' + category)"
    new_canvas_cat = "cachedContentCanvas[category] = (self.find('#patchstorage-content-' + category).length ? self.find('#patchstorage-content-' + category) : self.find('#patch-content-' + category))"
    if old_canvas_cat in code:
        code = code.replace(old_canvas_cat, new_canvas_cat)

    # 5. Fix category extraction safe check in renderPlugins
    old_cat_extract = "category = plugin.category[0]"
    new_cat_extract = "category = (plugin.category && Array.isArray(plugin.category) && plugin.category.length > 0) ? plugin.category[0] : 'Utility';"
    code = code.replace(old_cat_extract, new_cat_extract)

    with open(PATCHSTORAGE_JS, "w", encoding="utf-8") as f:
        f.write(code)
    print(" [+] Patched modep_app/html/js/patchstorage.js with full cloud support")

def fix_index_html():
    with open(INDEX_HTML, "r", encoding="utf-8") as f:
        html = f.read()

    categories = ['ControlVoltage', 'Delay', 'Distortion', 'Dynamics', 'Filter', 'Generator', 'MIDI', 'Modulator', 'Reverb', 'Simulator', 'Spatial', 'Spectral', 'Utility']
    
    # Ensure all category wrapper divs exist in patchstorage modal
    cat_divs = '\n                    <div id="patchstorage-content-All" class="plugins-wrapper"></div>\n'
    for cat in categories:
        cat_divs += f'                    <div id="patchstorage-content-{cat}" class="plugins-wrapper" style="display:none;"></div>\n'

    if '<div id="patchstorage-content-Distortion"' not in html:
        html = re.sub(
            r'<div id="patchstorage-results-wrapper">[\s\S]*?</div>\s*</div>\s*</div>\s*</div>\s*</div>\s*<!-- END PATCHSTORAGE LIBRARY -->',
            f'<div id="patchstorage-results-wrapper">\n{cat_divs}                  </div>\n                </div>\n            </div>\n        </div>\n    </div>\n    <!-- END PATCHSTORAGE LIBRARY -->',
            html
        )

    with open(INDEX_HTML, "w", encoding="utf-8") as f:
        f.write(html)
    print(" [+] Updated modep_app/html/index.html with category containers")

if __name__ == '__main__':
    fix_patchstorage_js()
    fix_index_html()
    print("\nMODEP Cloud & Patchstorage integration successfully applied!")
