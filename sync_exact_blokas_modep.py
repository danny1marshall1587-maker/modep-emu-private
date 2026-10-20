#!/usr/bin/env python3
"""
sync_exact_blokas_modep.py
Syncs the exact Blokas MODEP (modep-1.13-ps) UI components,
Patchstorage 'P' button, and cloud system directly into modep_app.
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
PATCHSTORAGE_JS = os.path.join(HTML_DIR, "js", "patchstorage.js")

def sync():
    # 1. Update index.html
    with open(INDEX_HTML, "r", encoding="utf-8") as f:
        html = f.read()

    # Main menu buttons
    exact_main_menu = """    <!-- MAIN MENU-->
    <div id="main-menu" class="mod-init-hidden">
        <div id="mod-plugins" class="icon selected" data-message="Constructor"></div>
        <div id="mod-pedalboard" class="icon" data-message="Pedalboards Library"></div>
        <div id="mod-bank" class="icon" data-message="Banks"></div>
        <div id="mod-file-manager" class="icon" data-message="File Manager"></div>
        <div id="mod-cloud-plugins" class="icon" data-message="Plugin Store"></div>
        <div id="patchstorage" class="icon" data-message="Patchstorage"></div>
        <div id="banks-saving">Auto saving banks...</div>
        <div id="mod-update" class="status icon"></div>
        <div id="blokas-update" class="status icon mod-hidden"></div>
        <div id="mod-status" class="status icon mod-hidden"></div>
        <div id="mod-devices" class="status icon"></div>
        <div id="mod-settings" class="status icon mod-settings" data-message="Go to settings screen"></div>
        <div id="mod-ram" class="icon">
            <div class="progress" id="ram-bar" style="width: 100%;">
                <div class="bar"></div> <!-- not sure if this is needed -->
            </div>
            <p class="progress-title" id="ram-bar-text">ram</p>
        </div>"""

    html = re.sub(r'<!-- MAIN MENU-->[\s\S]*?<div id="mod-ram" class="icon">[\s\S]*?<p class="progress-title" id="ram-bar-text">ram</p>\s*</div>', exact_main_menu, html)

    # Exact Blokas Patchstorage Modal
    exact_modal = """    <!-- PATCHSTORAGE LIBRARY -->
    <div id="patchstorage-library" class="mod-hidden mod-init-hidden">
        <div>
            <header>
                <h1 class="bottom top">PATCHSTORAGE</h1>
                <button class="status icon mod-bypass" id="patchstorage_update_all">Refresh</button>
            </header>

            <div class="filter clearfix">
                <h2>Filter</h2>
                <div class="control-group">
                    <input type="checkbox" name="non-installed" id="patchstorage-non-installed" />
                    <label for="patchstorage-non-installed">NOT INSTALLED</label>
                </div>
                <div class="control-group">
                    <input type="checkbox" name="installed" id="patchstorage-installed" />
                    <label for="patchstorage-installed">INSTALLED</label>
                </div>
                <div class="control-group">
                    <input type="checkbox" name="outdated" id="patchstorage-outdated"/>
                    <label for="patchstorage-outdated">OUTDATED</label>
                </div>
                <div class="control-group">
                    <input type="checkbox" name="other" id="patchstorage-other" />
                    <label for="patchstorage-other">LOCAL PLUGINS</label>
                </div>
                <div class="">
                    <input class="form-control" placeholder="Filter by keyword" type="search" autocomplete="off" />
                </div>
                <ul class="categories list list-unstyled" style="margin-bottom: 0px;">
                    <li id="patchstorage-tab-All" class="selected">All</li>
                    <li id="patchstorage-tab-ControlVoltage">Control&nbsp;Voltage</li>
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
                    <h2>Filtered results</h2>
                    <div id="patch-content-All" class="plugins-wrapper"></div>
                    <div id="patch-content-ControlVoltage" class="plugins-wrapper"></div>
                    <div id="patch-content-Delay" class="plugins-wrapper"></div>
                    <div id="patch-content-Distortion" class="plugins-wrapper"></div>
                    <div id="patch-content-Dynamics" class="plugins-wrapper"></div>
                    <div id="patch-content-Filter" class="plugins-wrapper"></div>
                    <div id="patch-content-Generator" class="plugins-wrapper"></div>
                    <div id="patch-content-MIDI" class="plugins-wrapper"></div>
                    <div id="patch-content-Modulator" class="plugins-wrapper"></div>
                    <div id="patch-content-Reverb" class="plugins-wrapper"></div>
                    <div id="patch-content-Simulator" class="plugins-wrapper"></div>
                    <div id="patch-content-Spatial" class="plugins-wrapper"></div>
                    <div id="patch-content-Spectral" class="plugins-wrapper"></div>
                    <div id="patch-content-Utility" class="plugins-wrapper"></div>
                    <div id="patchstorage-content-All" class="plugins-wrapper"></div>
                </div>
            </div>

        </div>
    </div>
    <!-- END PATCHSTORAGE LIBRARY -->"""

    html = re.sub(r'<!-- PATCHSTORAGE LIBRARY -->[\s\S]*?<!-- END PATCHSTORAGE LIBRARY -->', exact_modal, html)

    # Ensure unhides
    html = html.replace("$('#patchstorage').hide()", "$('#patchstorage').show()")
    html = html.replace("$('#mod-cloud-plugins').hide()", "$('#mod-cloud-plugins').show()")

    with open(INDEX_HTML, "w", encoding="utf-8") as f:
        f.write(html)
    print(" [+] Synced exact Blokas MODEP HTML structure into index.html")

    # 2. Update desktop.js
    with open(DESKTOP_JS, "r", encoding="utf-8") as f:
        djs = f.read()

    djs = djs.replace(
        "this.setupApp = function () {",
        "this.setupApp = function () {\n        $('#patchstorage').show();\n        $('#mod-cloud-plugins').show();"
    )

    with open(DESKTOP_JS, "w", encoding="utf-8") as f:
        f.write(djs)
    print(" [+] Synced desktop.js setupApp with Patchstorage and Plugin Store unhiding")

if __name__ == '__main__':
    sync()
    print("\nExact Blokas MODEP synchronization complete!")
