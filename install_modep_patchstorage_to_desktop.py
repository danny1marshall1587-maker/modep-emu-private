#!/usr/bin/env python3
"""
install_modep_patchstorage_to_desktop.py
Integrates the complete Blokas MODEP Patchstorage Plugin Store,
the "P" bottom-bar button, and Cloud Plugin browser into MODEP Desktop (modep_app).
"""

import os
import sys
import shutil

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
MOD_DESKTOP_DIR = os.path.join(BASE_DIR, "modep_app")
HTML_DIR = os.path.join(MOD_DESKTOP_DIR, "html")
MOD_DIR = os.path.join(MOD_DESKTOP_DIR, "mod")
MODEP_SRC = os.path.join(BASE_DIR, "modep_src")

def install_patchstorage():
    print("================================================================")
    print("      Installing Patchstorage & Plugin Store into MODEP App     ")
    print("================================================================")

    if not os.path.exists(MOD_DESKTOP_DIR):
        print(f"[!] Error: {MOD_DESKTOP_DIR} not found.")
        return False

    # 1. Copy template files into html/include/
    include_dir = os.path.join(HTML_DIR, "include")
    os.makedirs(include_dir, exist_ok=True)
    for tname in ["patchstorage_plugin.html", "patchstorage_plugin_info.html"]:
        src = os.path.join(MODEP_SRC, "html", "include", tname)
        dst = os.path.join(include_dir, tname)
        shutil.copy2(src, dst)
        print(f" [+] Copied template: include/{tname}")

    # 2. Copy JS files into html/js/
    js_src = os.path.join(MODEP_SRC, "html", "js", "patchstorage.js")
    js_dst = os.path.join(HTML_DIR, "js", "patchstorage.js")
    shutil.copy2(js_src, js_dst)
    print(" [+] Copied js/patchstorage.js")

    # 3. Copy image assets
    for img_rel in [os.path.join("img", "patchstorage-menu.png"), os.path.join("img", "icons", "36", "pluginstore.png")]:
        src = os.path.join(MODEP_SRC, "html", img_rel)
        dst = os.path.join(HTML_DIR, img_rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copy2(src, dst)
        print(f" [+] Copied {img_rel}")

    # 4. Patch mod/webserver.py
    webserver_py = os.path.join(MOD_DIR, "webserver.py")
    with open(webserver_py, "r", encoding="utf-8") as f:
        web_code = f.read()

    if "'patchstorage_enabled'" not in web_code:
        target_marker = "'preferences': json.dumps(SESSION.prefs.prefs),"
        replacement = """'preferences': json.dumps(SESSION.prefs.prefs),
            'patchstorage_enabled': 'true',
            'patchstorage_api_url': 'https://patchstorage.com/api/beta/patches',
            'patchstorage_platform_id': '8046',
            'patchstorage_target_id': '',"""
        if target_marker in web_code:
            web_code = web_code.replace(target_marker, replacement)
            with open(webserver_py, "w", encoding="utf-8") as f:
                f.write(web_code)
            print(" [+] Updated mod/webserver.py with Patchstorage context variables")

    # 5. Patch html/js/desktop.js
    desktop_js = os.path.join(HTML_DIR, "js", "desktop.js")
    with open(desktop_js, "r", encoding="utf-8") as f:
        djs_code = f.read()

    if "makePatchstorageBox" not in djs_code:
        djs_code = djs_code.replace(
            "cloudPluginBoxTrigger: $('<div>'),",
            "cloudPluginBoxTrigger: $('<div>'),\n        patchstorageBox: $('<div>'),\n        patchstorageBoxTrigger: $('<div>'),"
        )

        djs_code = djs_code.replace(
            "this.cloudPluginBox = self.makeCloudPluginBox(elements.cloudPluginBox,",
            """this.patchstorageBox = self.makePatchstorageBox(elements.patchstorageBox, elements.patchstorageBoxTrigger);
    this.cloudPluginBox = self.makeCloudPluginBox(elements.cloudPluginBox,"""
        )

        djs_code = djs_code.replace(
            "elements.cloudPluginBoxTrigger.statusTooltip()",
            "elements.cloudPluginBoxTrigger.statusTooltip();\n    elements.patchstorageBoxTrigger.statusTooltip();"
        )

        patchstorage_method = """
Desktop.prototype.makePatchstorageBox = function (el, trigger) {
    var self = this;
    return el.patchstorageBox({
        trigger: trigger,
        windowManager: this.windowManager,
        removePluginBundles: function (bundles, callback) {
            if (!confirm('You are about to remove this plugin and any other in the same bundle.'))
                return;
            self.previousPedalboardList = null;
            $.ajax({
                url: '/package/uninstall',
                data: JSON.stringify(bundles),
                method: 'POST',
                success: function(resp) {
                    if (resp.ok) {
                        callback(resp);
                    } else {
                        new Notification('error', "Could not uninstall bundle: " + resp.error);
                    }
                },
                error: function () {
                    new Notification('error', "Failed to uninstall plugin");
                }
            });
        }
    });
};
"""
        djs_code += patchstorage_method

        with open(desktop_js, "w", encoding="utf-8") as f:
            f.write(djs_code)
        print(" [+] Updated html/js/desktop.js with makePatchstorageBox")

    # 6. Patch html/css/main.css for Patchstorage & 'P' icon
    main_css = os.path.join(HTML_DIR, "css", "main.css")
    with open(main_css, "r", encoding="utf-8") as f:
        css_code = f.read()

    if "#patchstorage" not in css_code:
        extra_css = """
/* MODEP Patchstorage Styles */
#main-menu #patchstorage {
    background-image: url(../img/patchstorage-menu.png);
    background-repeat: no-repeat;
    background-position: center;
    background-size: 24px 24px;
    cursor: pointer;
}
#main-menu #patchstorage:hover {
    background-color: rgba(255, 255, 255, 0.15);
}
#main-menu #mod-cloud-plugins {
    background-image: url(../img/icons/36/pluginstore.png);
    background-repeat: no-repeat;
    background-position: center;
    background-size: 24px 24px;
    cursor: pointer;
}
#patchstorage-library {
    background: #20242c;
    bottom: 45px;
    left: 0;
    overflow: auto;
    position: fixed;
    right: 0;
    top: 0;
    z-index: 999;
}
#patchstorage-library h1 {
    color: #fff;
    display: inline-block;
    font-size: 24px;
    line-height: 35px;
    margin: 10px 0 18px 15px;
}
#patchstorage-library .filter {
    bottom: 45px;
    left: 0;
    position: fixed;
    top: 60px;
    width: 240px;
    overflow-y: auto;
    background: #181b22;
    padding: 10px 15px;
    border-right: 1px solid #30363d;
}
#patchstorage-library .filter h2 {
    color: #00e5ff;
    font-size: 14px;
    text-transform: uppercase;
    margin: 15px 0 8px 0;
}
#patchstorage-library .filter .categories li {
    list-style: none;
    color: #8b949e;
    padding: 6px 10px;
    cursor: pointer;
    border-radius: 4px;
    font-size: 13px;
}
#patchstorage-library .filter .categories li:hover,
#patchstorage-library .filter .categories li.selected {
    color: #fff;
    background: #004d61;
}
#patchstorage-library .patchstorage-plugins {
    margin-left: 260px;
    padding: 20px;
}
.patchstorage-info {
    position: fixed;
    top: 40px;
    left: 10%;
    right: 10%;
    bottom: 60px;
    background: #1c2128;
    border: 1px solid #30363d;
    border-radius: 8px;
    padding: 20px;
    z-index: 1000;
    overflow-y: auto;
    color: #e6edf3;
}
"""
        css_code += extra_css
        with open(main_css, "w", encoding="utf-8") as f:
            f.write(css_code)
        print(" [+] Appended Patchstorage & 'P' icon styles to html/css/main.css")

    # 7. Patch html/index.html
    index_html = os.path.join(HTML_DIR, "index.html")
    with open(index_html, "r", encoding="utf-8") as f:
        html_code = f.read()

    if "js/patchstorage.js" not in html_code:
        # Add script tag
        html_code = html_code.replace(
            '<script type="text/javascript" src="js/cloudplugin.js?v={{version}}"></script>',
            '<script type="text/javascript" src="js/cloudplugin.js?v={{version}}"></script>\n<script type="text/javascript" src="js/patchstorage.js?v={{version}}"></script>'
        )

        # Add global vars
        html_code = html_code.replace(
            "var CLOUD_LABS_URL = '{{cloud_labs_url}}';",
            """var CLOUD_LABS_URL = '{{cloud_labs_url}}';
    var PATCHSTORAGE_ENABLED = 'true';
    var PATCHSTORAGE_API_URL = 'https://patchstorage.com/api/beta/patches';
    var PATCHSTORAGE_PLATFORM_ID = '8046';
    var PATCHSTORAGE_TARGET_ID = null;"""
        )

        # Add elements in desktop init
        html_code = html_code.replace(
            "cloudPluginBoxTrigger: $('#mod-cloud-plugins'),",
            """cloudPluginBoxTrigger: $('#mod-cloud-plugins'),
        patchstorageBox: $('#patchstorage-library'),
        patchstorageBoxTrigger: $('#patchstorage'),"""
        )

        # Unhide cloud plugins & add patchstorage button
        html_code = html_code.replace(
            "$('#mod-cloud-plugins').hide()",
            "// $('#mod-cloud-plugins').hide() -- enabled for full MODEP experience\n        $('#mod-cloud-plugins').show()"
        )

        # Add the 'P' button into main-menu
        if '<div id="patchstorage"' not in html_code:
            html_code = html_code.replace(
                '<div id="main-menu" class="mod-init-hidden">',
                '<div id="main-menu" class="mod-init-hidden">\n        <div id="patchstorage" class="icon" data-message="Patchstorage"></div>'
            )

        # Add the patchstorage-library modal container
        patchstorage_modal = """
    <!-- PATCHSTORAGE LIBRARY -->
    <div id="patchstorage-library" class="mod-hidden mod-init-hidden">
        <div class="row-fluid clearfix">
            <div class="span12 content">
                <header class="clearfix">
                    <h1 class="bottom top">PATCHSTORAGE</h1>
                    <button class="status icon mod-bypass" id="patchstorage_update_all" style="margin-left: 20px;">Refresh</button>
                    <input type="search" placeholder="Search Patchstorage..." style="margin-left: 20px; padding: 6px 12px;" />
                </header>
                <div class="filter">
                    <h2>Show only</h2>
                    <div class="control-group">
                        <input type="checkbox" name="non-installed" id="patchstorage-non-installed" />
                        <label for="patchstorage-non-installed" style="color:#fff; margin-left: 5px;">NOT INSTALLED</label>
                    </div>
                    <div class="control-group">
                        <input type="checkbox" name="installed" id="patchstorage-installed" />
                        <label for="patchstorage-installed" style="color:#fff; margin-left: 5px;">INSTALLED</label>
                    </div>
                    <div class="control-group">
                        <input type="checkbox" name="outdated" id="patchstorage-outdated"/>
                        <label for="patchstorage-outdated" style="color:#fff; margin-left: 5px;">OUTDATED</label>
                    </div>
                    <div class="control-group">
                        <input type="checkbox" name="other" id="patchstorage-other" />
                        <label for="patchstorage-other" style="color:#fff; margin-left: 5px;">LOCAL PLUGINS</label>
                    </div>
                    <h2>Categories</h2>
                    <ul class="categories">
                        <li id="patchstorage-tab-All" class="selected">All</li>
                        <li id="patchstorage-tab-ControlVoltage">Control Voltage</li>
                        <li id="patchstorage-tab-Delay">Delay</li>
                        <li id="patchstorage-tab-Distortion">Distortion</li>
                        <li id="patchstorage-tab-Dynamics">Dynamics</li>
                        <li id="patchstorage-tab-Filter">Filter</li>
                        <li id="patchstorage-tab-Generator">Generator</li>
                        <li id="patchstorage-tab-MIDI">MIDI Utility</li>
                        <li id="patchstorage-tab-Modulator">Modulator</li>
                        <li id="patchstorage-tab-Reverb">Reverb</li>
                        <li id="patchstorage-tab-Simulator">Simulator</li>
                        <li id="patchstorage-tab-Spatial">Spatial</li>
                        <li id="patchstorage-tab-Spectral">Spectral</li>
                        <li id="patchstorage-tab-Utility">Utility</li>
                    </ul>
                </div>
                <div class="patchstorage-plugins clearfix js-patchstorage">
                  <div id="patchstorage-results-wrapper">
                    <div id="patchstorage-content-All" class="plugins-wrapper"></div>
                  </div>
                </div>
            </div>
        </div>
    </div>
    <!-- END PATCHSTORAGE LIBRARY -->
"""
        html_code = html_code.replace(
            '<!-- END CLOUD PLUGINS LIBRARY -->',
            '<!-- END CLOUD PLUGINS LIBRARY -->\n' + patchstorage_modal
        )

        with open(index_html, "w", encoding="utf-8") as f:
            f.write(html_code)
        print(" [+] Updated html/index.html with Patchstorage 'P' button & library modal")

    print("\nPatchstorage & Plugin Store successfully installed into MODEP Desktop App!")
    return True

if __name__ == '__main__':
    install_patchstorage()
