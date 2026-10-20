#!/usr/bin/env python3
import os
import re

INDEX_PATH = r"modep_app\html\index.html"

with open(INDEX_PATH, 'r', encoding='utf-8') as f:
    content = f.read()

# Replace hardcoded version hashes with {{version}}
content = re.sub(r'\?v=\d+', r'?v={{version}}', content)

# Replace the static JS config block (between <script type="text/javascript"> and var desktop = null)
config_block = """<script type="text/javascript">
{% autoescape None %}

var TEMPLATES = {}
SITEURL = ''
CLOUD_LABS_URL = '/cloud/labs'
PATCHSTORAGE_ENABLED = 'true'
PATCHSTORAGE_API_URL = 'https://patchstorage.com/api/beta/patches'
PATCHSTORAGE_PLATFORM_ID = '8046'
PATCHSTORAGE_TARGET_ID = '8280'
BLOKAS_ENABLED = 'true'
PLUGINS_URL      = '{{plugins_url}}'
PEDALBOARDS_URL  = '{{pedalboards_url}}'
PEDALBOARDS_LABS_URL  = '{{pedalboards_labs_url}}'
CONTROLCHAIN_URL = '{{controlchain_url}}'
LV2_PLUGIN_DIR   = '{{lv2_plugin_dir}}'
VERSION          = '{{version}}'
BIN_COMPAT       = '{{bin_compat}}'
PLATFORM         = '{{platform}}'
var SAMPLERATE   = {{sampleRate}}
var NOTIFICATIONS_ENABLED = true
var HARDWARE_PROFILE = JSON.parse(atob('{{hardware_profile}}'))
var DEFAULT_ICON_TEMPLATE = '{{default_icon_template}}'
var DEFAULT_SETTINGS_TEMPLATE = '{{default_settings_template}}'
var DEFAULT_PEDALBOARD = '{{default_pedalboard}}'
var FAVORITES = {{favorites}}
var PREFERENCES = {{preferences}}
var ADDRESSING_PAGES = {{addressing_pages}}
var DEBUG = true;
if (!DEBUG) {
    console = console || {}
    console.log = function() {}
}
"""

pattern = r'<script type="text/javascript">\s*var TEMPLATES[\s\S]*?var DEBUG = true;[\s\S]*?console\.log = function\(\) \{\}\s*\}'
if re.search(pattern, content):
    content = re.sub(pattern, config_block.strip(), content)
    print(" [+] Replaced static config block with Tornado template variables!")
else:
    print(" [!] Pattern not found directly, searching boundaries...")
    start_idx = content.find('<script type="text/javascript">\nvar TEMPLATES = {}')
    if start_idx == -1:
        start_idx = content.find('<script type="text/javascript">\n\nvar TEMPLATES = {}')
    end_idx = content.find('// for mod-app control')
    if start_idx != -1 and end_idx != -1:
        content = content[:start_idx] + config_block + "\n" + content[end_idx:]
        print(" [+] Successfully spliced config block!")

with open(INDEX_PATH, 'w', encoding='utf-8') as f:
    f.write(content)

print("Saved clean index.html for Tornado!")
