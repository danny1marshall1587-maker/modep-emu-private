#!/usr/bin/env python3
"""
fix_patchstorage_engine.py
Fixes the core array transformation bug in patchstorage.js (transformCloudPlugins map return),
fixes header forwarding in webserver.py PatchstorageProxy,
and ensures all cloud patches and plugins display and filter properly.
"""

import os
import sys
import re

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
MODEP_APP_DIR = os.path.join(BASE_DIR, "modep_app")
PATCHSTORAGE_JS = os.path.join(MODEP_APP_DIR, "html", "js", "patchstorage.js")
WEBSERVER_PY = os.path.join(MODEP_APP_DIR, "mod", "webserver.py")

def update_patchstorage_js():
    with open(PATCHSTORAGE_JS, "r", encoding="utf-8") as f:
        code = f.read()

    # 1. Fix transformCloudPlugins array mapping
    old_transform_plural = """    transformCloudPlugins: function (patches) {
        var self = $(this)
        patches.map((patch, i) => {
            patch = self.patchstorageBox('transformCloudPlugin', patch)
        })
        return patches
    },"""

    new_transform_plural = """    transformCloudPlugins: function (patches) {
        var self = $(this);
        if (!Array.isArray(patches)) return [];
        return patches.map(function(patch) {
            return self.patchstorageBox('transformCloudPlugin', patch);
        });
    },"""

    if old_transform_plural in code:
        code = code.replace(old_transform_plural, new_transform_plural)
    else:
        code = re.sub(r'transformCloudPlugins:\s*function\s*\(patches\)\s*\{[\s\S]*?return patches\s*\},', new_transform_plural, code)

    # 2. Fix transformCloudPlugin with robust fallbacks
    new_transform_single = """    transformCloudPlugin: function (p) {
        if (!p) return {};
        p.psid = String(p.id || '');
        p.cloud_revision = p.revision || 1;
        p.name = unescape(p.title || '');
        p.label = unescape(p.title || '');
        p.comment = (p.content) ? unescape(p.content) : (p.excerpt ? unescape(p.excerpt) : '');
        p.thumbnail_href = (p.artwork && p.artwork.thumbnail_url) ? p.artwork.thumbnail_url : "/resources/pedals/default-thumbnail.png";
        p.screenshot_href = (p.artwork && p.artwork.url) ? p.artwork.url : "/resources/pedals/default-screenshot.png";
        p.plugin_count = (p.uids && Array.isArray(p.uids)) ? p.uids.length : 1;
        p.state = (p.state && p.state.slug) ? p.state.slug : 'ready';
        p.uploader = (p.author && p.author.slug) ? p.author.slug : 'community';
        p.status = 'available';
        p.supported = true;

        if (p.uids && Array.isArray(p.uids) && p.uids.length > 0) {
            p.uri = p.uids[0];
        } else {
            p.uri = String(p.id || 'patch_' + Math.random());
        }

        var tags = [];
        if (p.categories && Array.isArray(p.categories)) {
            p.categories.forEach(function(item) {
                if (item && item.slug) {
                    var name = item.slug.replace('-', '').toLowerCase();
                    if (!tags.includes(name)) tags.push(name);
                }
            });
        }
        if (p.tags && Array.isArray(p.tags)) {
            p.tags.forEach(function(item) {
                if (item && item.slug) {
                    var name = item.slug.replace('-', '').toLowerCase();
                    if (!tags.includes(name)) tags.push(name);
                }
            });
        }

        var map = {
            'delay': 'Delay',
            'distortion': 'Distortion',
            'dynamics': 'Dynamics',
            'filter': 'Filter',
            'generator': 'Generator',
            'modulator': 'Modulator',
            'reverb': 'Reverb',
            'simulator': 'Simulator',
            'spatial': 'Spatial',
            'spectral': 'Spectral',
            'controlvoltage': 'ControlVoltage',
            'midi': 'MIDI',
            'utility': 'Utility'
        };

        var foundCat = ['Utility'];
        for (var k in map) {
            if (tags.includes(k)) {
                foundCat = [map[k]];
                break;
            }
        }

        p.category = foundCat;
        p.tags = tags.join(', ');

        return p;
    },"""

    code = re.sub(r'transformCloudPlugin:\s*function\s*\(p\)\s*\{[\s\S]*?return p\s*\},', new_transform_single, code)

    # 3. Fix buildIndex safe checks
    code = re.sub(r'data\.index\s*=\s*`[^`]*`\.toLowerCase\(\)', 'data.index = `${data.comment || ""} ${data.name || ""} ${data.label || ""} ${data.brand || ""} ${data.uploader || ""}`.toLowerCase()', code)

    # 4. Fix singular typo in searchPlugins
    code = code.replace("self.data('cloudPluginLoaded', false)", "self.data('cloudPluginsLoaded', false)")

    # 5. Fix setCategory container resolution
    new_set_category = """    setCategory: function (category) {
        var self = $(this);
        self.find('ul.categories li').removeClass('selected');
        self.find('.plugins-wrapper').hide();
        self.find('#patchstorage-tab-' + category).addClass('selected');
        var targetCanvas = self.find('#patchstorage-content-' + category).length ? self.find('#patchstorage-content-' + category) : self.find('#patch-content-' + category);
        targetCanvas.show().css('display', 'inline-block');
        self.data('category', category);
    },"""

    code = re.sub(r'setCategory:\s*function\s*\(category\)\s*\{[\s\S]*?self\.data\(\'category\',\s*category\)\s*\},', new_set_category, code)

    with open(PATCHSTORAGE_JS, "w", encoding="utf-8") as f:
        f.write(code)
    print(" [+] Fixed modep_app/html/js/patchstorage.js cloud mapping & filtering")

def update_webserver():
    with open(WEBSERVER_PY, "r", encoding="utf-8") as f:
        code = f.read()

    new_proxy = """class PatchstorageProxy(web.RequestHandler):
    @gen.coroutine
    def get(self, path=None):
        self.set_header("Content-Type", "application/json; charset=UTF-8")
        self.set_header("Access-Control-Allow-Origin", "*")
        self.set_header("Access-Control-Expose-Headers", "X-WP-TotalPages, X-WP-Total, x-wp-totalpages, x-wp-total")
        qs = self.request.query
        base_url = "https://patchstorage.com/api/beta/patches"
        if path and path.strip():
            target_url = f"{base_url}/{path.strip()}"
        else:
            target_url = f"{base_url}?{qs}" if qs else base_url

        req = urllib.request.Request(target_url, headers={'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64)'})
        try:
            with urllib.request.urlopen(req, timeout=15) as resp:
                data = resp.read()
                for h in ['X-WP-TotalPages', 'x-wp-totalpages', 'X-WP-Total', 'x-wp-total']:
                    if h in resp.headers:
                        self.set_header('X-WP-TotalPages', resp.headers[h])
                        break
                self.write(data)
        except Exception as e:
            print("[!] Error proxying Patchstorage:", e)
            self.write(json.dumps([]))"""

    code = re.sub(r'class PatchstorageProxy\(web\.RequestHandler\):[\s\S]*?self\.write\(json\.dumps\(\[\]\)\)', new_proxy, code)

    with open(WEBSERVER_PY, "w", encoding="utf-8") as f:
        f.write(code)
    print(" [+] Updated modep_app/mod/webserver.py PatchstorageProxy with header forwarding")

if __name__ == '__main__':
    update_patchstorage_js()
    update_webserver()
    print("\nPatchstorage engine fixed and ready!")
