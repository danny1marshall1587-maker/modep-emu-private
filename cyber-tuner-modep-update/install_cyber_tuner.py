#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Cyber Audio
# SPDX-License-Identifier: MIT
# Automated Installer & Backup Tool for Cyber Strobe & Peak Tuner System

import os
import sys
import shutil
import time
import datetime
import subprocess

def find_target_dir():
    candidates = [
        r'C:\Program Files\MOD Desktop',
        '/usr/share/mod',
        '/usr/local/share/mod',
        '/var/modep'
    ]
    for c in candidates:
        if os.path.exists(c) and os.path.exists(os.path.join(c, 'html')) and os.path.exists(os.path.join(c, 'mod')):
            return c
    return None

def main():
    print("================================================================")
    print("  CYBER STROBE & PEAK TUNER -- INSTALLER & BACKUP TOOL")
    print("================================================================")

    target_dir = find_target_dir()
    if not target_dir:
        print("ERROR: Could not automatically locate MOD Desktop / MODEP root directory.")
        sys.exit(1)

    print(f"Target directory: {target_dir}")

    # Stop running processes
    print("\nStopping running MOD instances...")
    if os.name == 'nt':
        subprocess.run(['powershell', '-Command', 'Stop-Process -Name mod-desktop, mod-ui, jackd -Force -ErrorAction SilentlyContinue'])
    else:
        subprocess.run(['systemctl', 'stop', 'modep-mod-ui'], stderr=subprocess.DEVNULL)
    time.sleep(1)

    # Determine Backup location
    timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    if os.name == 'nt':
        user_docs = os.path.expanduser('~')
        backup_base = os.path.join(user_docs, 'Documents', 'MOD Desktop', 'backups')
        backup_dir = os.path.join(backup_base, f"backup_cyber_tuner_{timestamp}")
    else:
        backup_base = '/var/backups'
        backup_dir = os.path.join(backup_base, f"backup_cyber_tuner_{timestamp}")

    os.makedirs(backup_dir, exist_ok=True)
    print(f"Creating timestamped backup at: {backup_dir}")

    files_to_backup = [
        os.path.join(target_dir, 'html', 'index.html'),
        os.path.join(target_dir, 'html', 'js', 'desktop.js'),
        os.path.join(target_dir, 'html', 'js', 'host.js'),
        os.path.join(target_dir, 'mod', 'settings.py'),
        os.path.join(target_dir, 'mod', 'webserver.py'),
        os.path.join(target_dir, 'mod', 'session.py'),
        os.path.join(target_dir, 'mod', 'host.py'),
    ]

    for f in files_to_backup:
        if os.path.exists(f):
            rel = os.path.relpath(f, target_dir)
            dst = os.path.join(backup_dir, rel)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copy2(f, dst)
            print(f"  Backed up: {rel}")

    src_root = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'src')
    if not os.path.exists(src_root):
        src_root = os.path.join(r'c:\Users\danny\Documents\modep emu\cyber-tuner-modep-update\src')

    # 1. Copy Frontend Assets (tuner.css, tuner.js, tuner.png)
    print("\n1. Installing frontend assets...")
    shutil.copy2(os.path.join(src_root, 'html', 'css', 'tuner.css'), os.path.join(target_dir, 'html', 'css', 'tuner.css'))
    shutil.copy2(os.path.join(src_root, 'html', 'js', 'tuner.js'), os.path.join(target_dir, 'html', 'js', 'tuner.js'))

    icon_src = os.path.join(src_root, 'html', 'img', 'icons', '25', 'tuner.png')
    icon_dst = os.path.join(target_dir, 'html', 'img', 'icons', '25', 'tuner.png')
    if os.path.exists(icon_src):
        os.makedirs(os.path.dirname(icon_dst), exist_ok=True)
        shutil.copy2(icon_src, icon_dst)
        print("  Copied tuner.png icon")

    print("  Copied tuner.css and tuner.js")

    # 2. Patch index.html
    print("\n2. Patching html/index.html...")
    index_path = os.path.join(target_dir, 'html', 'index.html')
    with open(index_path, 'r', encoding='utf-8') as f:
        html_content = f.read()

    # Clean out old misplaced or legacy tuner elements
    clean_targets = [
        '<div id="mod-tuner-icon" class="icon" data-message="Open Cyber Strobe Tuner">TUNER</div>\n        ',
        '<div id="mod-tuner-icon" class="icon" data-message="Open Cyber Strobe Tuner">TUNER</div>\n',
        '<div id="mod-tuner-icon" class="icon" data-message="Open Cyber Strobe Tuner">TUNER</div>',
        '<button class="js-tuner menu-trigger" id="mod-tuner-top-btn" title="Open Cyber Strobe & Peak Tuner (Right-click to assign MIDI)">TUNER</button>\n                    ',
        '<button class="js-tuner" id="mod-tuner-top-btn" title="Open Cyber Strobe Tuner">Tuner</button>\n                    ',
    ]
    for ct in clean_targets:
        html_content = html_content.replace(ct, '')

    # Remove any old misplaced <a> tuner links
    cleaned_lines = []
    for line in html_content.splitlines():
        if 'id="mod-tuner-top-btn"' in line and '<a ' in line:
            continue
        cleaned_lines.append(line)
    html_content = '\n'.join(cleaned_lines)

    # Add CSS link
    if 'tuner.css' not in html_content:
        if 'css/main.css?v={{version}}' in html_content:
            html_content = html_content.replace(
                '<link rel="stylesheet" type="text/css" href="css/main.css?v={{version}}"/>',
                '<link rel="stylesheet" type="text/css" href="css/main.css?v={{version}}"/>\n<link rel="stylesheet" type="text/css" href="css/tuner.css?v={{version}}"/>'
            )
        else:
            html_content = html_content.replace(
                '<link rel="stylesheet" href="css/main.css">',
                '<link rel="stylesheet" href="css/main.css">\n<link rel="stylesheet" href="css/tuner.css">'
            )

    # Add JS script
    if 'tuner.js' not in html_content:
        if '<script type="text/javascript" src="js/desktop.js?v={{version}}"></script>' in html_content:
            html_content = html_content.replace(
                '<script type="text/javascript" src="js/desktop.js?v={{version}}"></script>',
                '<script type="text/javascript" src="js/tuner.js?v={{version}}"></script>\n<script type="text/javascript" src="js/desktop.js?v={{version}}"></script>'
            )
        else:
            html_content = html_content.replace(
                '<script src="js/desktop.js"></script>',
                '<script src="js/tuner.js"></script>\n<script src="js/desktop.js"></script>'
            )

    # 1. Add Top Header Bar Tuner button in #pedalboard-info .actions (next to snapshots)
    top_btn_html = '<button class="js-tuner" id="mod-tuner-top-btn" title="Open Cyber Strobe Tuner (Right-click to assign MIDI)">Tuner</button>'
    if 'id="mod-tuner-top-btn"' not in html_content:
        if '<div id="js-preset-menu"' in html_content:
            html_content = html_content.replace(
                '<div id="js-preset-menu"',
                top_btn_html + '\n                    <div id="js-preset-menu"'
            )
        elif '<div class="actions">' in html_content:
            html_content = html_content.replace(
                '<div class="actions">',
                '<div class="actions">\n                    ' + top_btn_html
            )
        elif '<header id="pedalboard-actions">' in html_content:
            html_content = html_content.replace(
                '<header id="pedalboard-actions">',
                '<header id="pedalboard-actions">\n        ' + top_btn_html
            )

    # 2. Add Main Menu icon in #main-menu (next to transport icon)
    menu_icon_html = '<div id="mod-tuner-icon" class="icon" data-message="Open Cyber Strobe Tuner">TUNER</div>'
    if 'id="mod-tuner-icon"' not in html_content:
        if '<div id="mod-transport-icon"' in html_content:
            html_content = html_content.replace(
                '<div id="mod-transport-icon"',
                menu_icon_html + '\n        <div id="mod-transport-icon"'
            )
        elif '<a id="mod-transport-icon"' in html_content:
            html_content = html_content.replace(
                '<a id="mod-transport-icon"',
                '<a id="mod-tuner-icon" class="icon" data-message="Open Cyber Strobe Tuner">TUNER</a>\n        <a id="mod-transport-icon"'
            )

    # Add Tuner Window Modal container
    modal_html = """
    <!-- CYBER STROBE TUNER MODAL WINDOW -->
    <div id="cyber-tuner-backdrop" class="cyber-tuner-backdrop" style="display:none;"></div>
    <div id="mod-tuner-window" class="cyber-tuner-window" style="display:none;"></div>
"""
    if 'id="mod-tuner-window"' not in html_content:
        html_content = html_content.replace('</body>', modal_html + '\n</body>')

    with open(index_path, 'w', encoding='utf-8') as f:
        f.write(html_content)
    print("  Patched html/index.html successfully")

    # 3. Patch html/js/desktop.js
    print("\n3. Patching html/js/desktop.js...")
    desktop_path = os.path.join(target_dir, 'html', 'js', 'desktop.js')
    with open(desktop_path, 'r', encoding='utf-8') as f:
        desktop_content = f.read()

    # Instantiate CyberTuner
    init_code = """
        if (window.CyberTuner && $("#mod-tuner-window").length) {
            self.cyberTuner = new CyberTuner({
                topButton: $("#mod-tuner-top-btn"),
                windowModal: $("#mod-tuner-window")
            });
            window.CyberTunerInstance = self.cyberTuner;
        }
"""
    if 'self.cyberTuner = new CyberTuner' not in desktop_content:
        desktop_content = desktop_content.replace(
            'this.transportControls = new TransportControls({',
            init_code + '\n        this.transportControls = new TransportControls({'
        )

    # Ignore :tuner in hardwareManager.setEnabled for transportControls
    if 'portSymbol == ":tuner"' not in desktop_content:
        desktop_content = desktop_content.replace(
            'if (instance == "/pedalboard") {\n                self.transportControls.setControlEnabled(portSymbol, enabled, feedback, forceAddress, momentaryMode)\n                return\n            }',
            'if (instance == "/pedalboard") {\n                if (portSymbol == ":tuner") {\n                    return;\n                }\n                self.transportControls.setControlEnabled(portSymbol, enabled, feedback, forceAddress, momentaryMode)\n                return\n            }'
        )

    with open(desktop_path, 'w', encoding='utf-8') as f:
        f.write(desktop_content)
    print("  Patched html/js/desktop.js successfully")

    # 4. Patch html/js/host.js
    print("\n4. Patching html/js/host.js...")
    host_js_path = os.path.join(target_dir, 'html', 'js', 'host.js')
    with open(host_js_path, 'r', encoding='utf-8') as f:
        host_js_content = f.read()

    # Clean any old bug lines in midi_map
    host_js_content = host_js_content.replace(
        'if (window.CyberTunerInstance && window.CyberTunerInstance.isLearningMidi) {\n                window.CyberTunerInstance.handleNativeMidi(channel + 1, control, 127);\n            }',
        ''
    )

    # Param set listener for /pedalboard :tuner
    param_set_tuner = """
            if (instance == "/pedalboard" && symbol == ":tuner") {
                var tuner = window.CyberTunerInstance || (desktop && desktop.cyberTuner);
                if (tuner && typeof tuner.handleNativeParamSet === 'function') {
                    tuner.handleNativeParamSet(value);
                } else if (tuner) {
                    if (value > 0.5) {
                        if (!tuner.isOpen) tuner.open();
                    } else {
                        if (tuner.isOpen) tuner.close();
                    }
                }
                return;
            }
"""
    if 'instance == "/pedalboard" && symbol == ":tuner"' not in host_js_content:
        host_js_content = host_js_content.replace(
            'desktop.pedalboard.pedalboard("setPortWidgetsValue", instance, symbol, value);',
            param_set_tuner + '\n            desktop.pedalboard.pedalboard("setPortWidgetsValue", instance, symbol, value);'
        )

    # Midi map listener for /pedalboard :tuner
    midi_map_tuner = """
            if (instance === "/pedalboard" && symbol === ":tuner") {
                var tuner = window.CyberTunerInstance || (desktop && desktop.cyberTuner);
                if (tuner && typeof tuner.updateMidiMappingLabel === 'function') {
                    tuner.updateMidiMappingLabel(channel, control);
                }
            }
"""
    if 'instance === "/pedalboard" && symbol === ":tuner"' not in host_js_content:
        host_js_content = host_js_content.replace(
            'desktop.hardwareManager.addMidiMapping(instance, symbol, channel, control, minimum, maximum)',
            midi_map_tuner + '            desktop.hardwareManager.addMidiMapping(instance, symbol, channel, control, minimum, maximum)'
        )

    with open(host_js_path, 'w', encoding='utf-8') as f:
        f.write(host_js_content)
    print("  Patched html/js/host.js successfully")

    # 5. Patch mod/settings.py
    print("\n5. Patching mod/settings.py...")
    settings_path = os.path.join(target_dir, 'mod', 'settings.py')
    if os.path.exists(settings_path):
        with open(settings_path, 'r', encoding='utf-8') as f:
            settings_content = f.read()

        settings_content = settings_content.replace(
            'TUNER = os.environ.get(\'MOD_TUNER_PLUGIN\', "gxtuner")',
            'TUNER = os.environ.get(\'MOD_TUNER_PLUGIN\', "tuna")'
        )
        settings_content = settings_content.replace(
            'if TUNER == "tuna":\n    TUNER_URI = "urn:mod:tuna"',
            'if TUNER == "tuna":\n    TUNER_URI = "http://gareus.org/oss/lv2/tuna#mod"'
        )
        with open(settings_path, 'w', encoding='utf-8') as f:
            f.write(settings_content)
        print("  Patched mod/settings.py: configured tuna.lv2 as native tuner")
    else:
        print("  mod/settings.py not present, skipping")

    # 6. Patch mod/session.py
    print("\n6. Patching mod/session.py...")
    session_path = os.path.join(target_dir, 'mod', 'session.py')
    with open(session_path, 'r', encoding='utf-8') as f:
        session_lines = f.readlines()

    clean_lines = []
    skip = False
    for line in session_lines:
        if 'Cyber Strobe Tuner hooks' in line:
            skip = True
            continue
        if skip:
            if line.startswith('SESSION = Session()'):
                skip = False
            elif line.startswith('class ') or (line.startswith('def ') and not line.startswith('def tuner_')):
                skip = False
            else:
                continue
        clean_lines.append(line)

    session_content = "".join(clean_lines)

    session_patch = """
    # Cyber Strobe Tuner hooks
    def tuner_enable(self, mute=True):
        self.tuner_active = True
        if hasattr(self, 'host') and self.host:
            def _cb(ok=True):
                pass
            try:
                self.host.hmi_tuner_on(_cb)
            except:
                pass
            try:
                self.host.hmi_menu_set_tuner_mute(mute, _cb)
            except:
                pass

    def tuner_disable(self):
        self.tuner_active = False
        if hasattr(self, 'host') and self.host:
            def _cb(ok=True):
                pass
            try:
                self.host.hmi_tuner_off(_cb)
            except:
                pass

    def tuner_set_ref_freq(self, freq):
        if hasattr(self, 'host') and self.host:
            def _cb(ok=True):
                pass
            try:
                self.host.hmi_tuner_ref_freq(int(freq), _cb)
            except:
                pass

    def tuner_set_mute(self, mute):
        if hasattr(self, 'host') and self.host:
            def _cb(ok=True):
                pass
            try:
                self.host.hmi_menu_set_tuner_mute(bool(mute), _cb)
            except:
                pass

    def tuner_pitch_update(self, freq, note, cents):
        import json
        payload = json.dumps({
            'freq': float(freq),
            'note': str(note),
            'cents': float(cents),
            'in_tune': abs(float(cents)) < 1.5
        })
        msg = "tuner-pitch " + payload
        for ws in list(self.websockets):
            try:
                ws.write_message(msg)
            except:
                pass
"""
    if 'self.tuner_active = False' not in session_content:
        session_content = session_content.replace(
            'self.websockets = []\n',
            'self.websockets = []\n        self.tuner_active = False\n'
        )

    if 'def tuner_enable' not in session_content:
        session_content = session_content.replace('SESSION = Session()', session_patch + '\nSESSION = Session()')

    with open(session_path, 'w', encoding='utf-8') as f:
        f.write(session_content)
    print("  Patched mod/session.py successfully")

    # 7. Patch mod/host.py
    print("\n7. Patching mod/host.py...")
    host_path = os.path.join(target_dir, 'mod', 'host.py')
    with open(host_path, 'r', encoding='utf-8') as f:
        host_content = f.read()

    # Add :tuner to PEDALBOARD_INSTANCE_ID midiCCs
    if '":tuner"' not in host_content:
        host_content = host_content.replace(
            '":rolling": (-1,-1,0.0,1.0),',
            '":rolling": (-1,-1,0.0,1.0),\n                    ":tuner"  : (-1,-1,0.0,1.0),'
        )

    # Add :tuner to addr_task_get_port_value
    if 'portsymbol == ":tuner"' not in host_content:
        host_content = host_content.replace(
            'if portsymbol == ":rolling":\n                return 1.0 if self.transport_rolling else 0.0',
            'if portsymbol == ":rolling":\n                return 1.0 if self.transport_rolling else 0.0\n            if portsymbol == ":tuner":\n                from mod.session import SESSION\n                return 1.0 if getattr(SESSION, "tuner_active", False) else 0.0'
        )

    # Add :tuner to process_read_message_body under param_set
    param_set_tuner_hook = """        if cmd == "param_set":
            msg_data    = data.split(" ",3)
            instance_id = int(msg_data[0])
            portsymbol  = msg_data[1]
            value       = float(msg_data[2])

            if instance_id == TUNER_INSTANCE_ID and (portsymbol == "mode" or portsymbol == ":tuner"):
                tuner_on = bool(value > 0.5)
                from mod.session import SESSION
                if tuner_on:
                    SESSION.tuner_enable(self.current_tuner_mute)
                else:
                    SESSION.tuner_disable()
                self.msg_callback("param_set /pedalboard :tuner %f" % (1.0 if tuner_on else 0.0))
                return
"""
    if 'if instance_id == TUNER_INSTANCE_ID and (portsymbol == "mode" or portsymbol == ":tuner"):' not in host_content:
        host_content = host_content.replace(
            '        if cmd == "param_set":\n            msg_data    = data.split(" ",3)\n            instance_id = int(msg_data[0])\n            portsymbol  = msg_data[1]\n            value       = float(msg_data[2])',
            param_set_tuner_hook.rstrip()
        )

    # Add :tuner to process_read_message_body under midi_mapped
    midi_mapped_tuner_hook = """        elif cmd == "midi_mapped":
            msg_data    = data.split(" ",7)
            instance_id = int(msg_data[0])
            portsymbol  = msg_data[1]
            channel     = int(msg_data[2])
            controller  = int(msg_data[3])
            value       = float(msg_data[4])
            minimum     = float(msg_data[5])
            maximum     = float(msg_data[6])

            if instance_id == TUNER_INSTANCE_ID and (portsymbol == "mode" or portsymbol == ":tuner"):
                instance_id = PEDALBOARD_INSTANCE_ID
                portsymbol = ":tuner"
"""
    if 'if instance_id == TUNER_INSTANCE_ID and (portsymbol == "mode" or portsymbol == ":tuner"):\n                instance_id = PEDALBOARD_INSTANCE_ID' not in host_content:
        host_content = host_content.replace(
            '        elif cmd == "midi_mapped":\n            msg_data    = data.split(" ",7)\n            instance_id = int(msg_data[0])\n            portsymbol  = msg_data[1]\n            channel     = int(msg_data[2])\n            controller  = int(msg_data[3])\n            value       = float(msg_data[4])\n            minimum     = float(msg_data[5])\n            maximum     = float(msg_data[6])',
            midi_mapped_tuner_hook.rstrip()
        )

    # Map midi_learn for :tuner
    if 'instance_id == PEDALBOARD_INSTANCE_ID and portsymbol == ":tuner":\n                self.send_notmodified("midi_learn %d mode' not in host_content:
        host_content = host_content.replace(
            '        if actuator_uri == kMidiLearnURI:\n            self.send_notmodified("midi_learn %d %s %f %f" % (instance_id,\n                                                              portsymbol,\n                                                              minimum,\n                                                              maximum), callback, datatype=\'boolean\')\n            return',
            '        if actuator_uri == kMidiLearnURI:\n            if instance_id == PEDALBOARD_INSTANCE_ID and portsymbol == ":tuner":\n                self.send_notmodified("midi_learn %d mode %f %f" % (TUNER_INSTANCE_ID, minimum, maximum), callback, datatype=\'boolean\')\n            else:\n                self.send_notmodified("midi_learn %d %s %f %f" % (instance_id, portsymbol, minimum, maximum), callback, datatype=\'boolean\')\n            return'
        )

    # Map midi_unmap for :tuner
    if 'instance_id == PEDALBOARD_INSTANCE_ID and portsymbol == ":tuner":\n                self.send_modified("midi_unmap %d mode' not in host_content:
        host_content = host_content.replace(
            '        if actuator_uri == kMidiUnlearnURI:\n            self.send_modified("midi_unmap %d %s" % (instance_id, portsymbol), callback, datatype=\'boolean\')\n            return',
            '        if actuator_uri == kMidiUnlearnURI:\n            if instance_id == PEDALBOARD_INSTANCE_ID and portsymbol == ":tuner":\n                self.send_modified("midi_unmap %d mode" % TUNER_INSTANCE_ID, callback, datatype=\'boolean\')\n            else:\n                self.send_modified("midi_unmap %d %s" % (instance_id, portsymbol), callback, datatype=\'boolean\')\n            return'
        )

    # Map explicit midi_map for :tuner
    if 'instance_id == PEDALBOARD_INSTANCE_ID and portsymbol == ":tuner":\n                            self.send_modified("midi_map %d mode' not in host_content:
        host_content = host_content.replace(
            '                        self.send_modified("midi_map %d %s %i %i %f %f" % (instance_id,\n                                                                           portsymbol,\n                                                                           channel,\n                                                                           controller,\n                                                                           minimum,\n                                                                           maximum), callback, datatype=\'boolean\')',
            '                        if instance_id == PEDALBOARD_INSTANCE_ID and portsymbol == ":tuner":\n                            self.send_modified("midi_map %d mode %i %i %f %f" % (TUNER_INSTANCE_ID, channel, controller, minimum, maximum), callback, datatype=\'boolean\')\n                        else:\n                            self.send_modified("midi_map %d %s %i %i %f %f" % (instance_id, portsymbol, channel, controller, minimum, maximum), callback, datatype=\'boolean\')'
        )

    # Add :tuner to hmi_or_cc_parameter_set
    if 'elif portsymbol == ":tuner":' not in host_content:
        host_content = host_content.replace(
            'if portsymbol in (":bpb", ":bpm", ":rolling"):',
            'if portsymbol in (":bpb", ":bpm", ":rolling", ":tuner"):'
        )
        host_content = host_content.replace(
            'elif portsymbol == ":rolling":\n                        rolling = bool(value > 0.5)\n                        self.set_transport_rolling(rolling, True, True, True, False, callback)',
            'elif portsymbol == ":rolling":\n                        rolling = bool(value > 0.5)\n                        self.set_transport_rolling(rolling, True, True, True, False, callback)\n                    elif portsymbol == ":tuner":\n                        tuner_on = bool(value > 0.5)\n                        from mod.session import SESSION\n                        if tuner_on:\n                            SESSION.tuner_enable(self.current_tuner_mute)\n                        else:\n                            SESSION.tuner_disable()\n                        self.msg_callback("param_set /pedalboard :tuner %f" % (1.0 if tuner_on else 0.0))\n                        if callback is not None:\n                            callback(True)'
        )

    # Fix hmi_tuner_ref_freq for tuna
    host_content = host_content.replace(
        'self.send_notmodified("param_set %d REFFREQ %d" % (TUNER_INSTANCE_ID, freq), callback)',
        'ref_port = "tuning" if str(TUNER_URI).startswith("http://gareus.org") or str(TUNER_URI) == "urn:mod:tuna" else "REFFREQ"\n        self.send_notmodified("param_set %d %s %f" % (TUNER_INSTANCE_ID, ref_port, float(freq)), callback)'
    )

    # Add :tuner to portsyms in save_pedalboard
    if '":tuner"' not in host_content:
        host_content = host_content.replace(
            'portsyms = [":bpb",":bpm",":rolling",',
            'portsyms = [":bpb",":bpm",":rolling",":tuner",'
        )

    # Pitch update in set_tuner_value
    clean_try = """        try:
            yield gen.Task(self.hmi.tuner, freq, note, cents)
            from mod.session import SESSION
            SESSION.tuner_pitch_update(freq, note, cents)
        except Exception as e:
            logging.exception(e)
            return"""
    orig_block = """        try:
            yield gen.Task(self.hmi.tuner, freq, note, cents)
        except Exception as e:"""
    if orig_block in host_content and 'SESSION.tuner_pitch_update' not in host_content:
        host_content = host_content.replace(orig_block, clean_try)

    with open(host_path, 'w', encoding='utf-8') as f:
        f.write(host_content)
    print("  Patched mod/host.py successfully with native :tuner pedalboard port")

    # 8. Patch mod/webserver.py
    print("\n8. Patching mod/webserver.py...")
    webserver_path = os.path.join(target_dir, 'mod', 'webserver.py')
    with open(webserver_path, 'r', encoding='utf-8') as f:
        webserver_content = f.read()

    if 'elif cmd == "tuner-enable":' not in webserver_content:
        ws_patch = """
        elif cmd == "tuner-enable":
            mute = True if len(data) < 2 or data[1] == "1" else False
            SESSION.tuner_enable(mute)

        elif cmd == "tuner-disable":
            SESSION.tuner_disable()

        elif cmd == "tuner-ref-freq":
            try:
                freq = float(data[1])
                SESSION.tuner_set_ref_freq(freq)
            except:
                pass

        elif cmd == "tuner-mute":
            try:
                mute = bool(int(data[1]))
                SESSION.tuner_set_mute(mute)
            except:
                pass
"""
        webserver_content = webserver_content.replace(
            'elif cmd == "show_external_ui":',
            ws_patch + '        elif cmd == "show_external_ui":'
        )

        webserver_content = webserver_content.replace(
            'yield gen.Task(SESSION.websocket_closed, self)',
            'yield gen.Task(SESSION.websocket_closed, self)\n        if getattr(SESSION, "tuner_active", False):\n            SESSION.tuner_disable()'
        )

        with open(webserver_path, 'w', encoding='utf-8') as f:
            f.write(webserver_content)
        print("  Patched mod/webserver.py successfully")


    # 9. Patch mod/host.py to NOT remove the tuner, allowing MIDI assignments to work when it's closed!
    print("\n9. Patching mod/host.py for MIDI stability...")
    host_py_path = os.path.join(target_dir, 'mod', 'host.py')
    with open(host_py_path, 'r', encoding='utf-8') as f:
        host_py_content = f.read()

    # 1. Patch kMidiLearnURI
    old_learn = '''        # MIDI learn is not an actual addressing
        if actuator_uri == kMidiLearnURI:
            if instance_id == PEDALBOARD_INSTANCE_ID and portsymbol == ":tuner":
                self.send_notmodified("midi_learn %d mode %f %f" % (TUNER_INSTANCE_ID, minimum, maximum), callback, datatype='boolean')'''

    new_learn = '''        # MIDI learn is not an actual addressing
        if actuator_uri == kMidiLearnURI:
            if instance_id == PEDALBOARD_INSTANCE_ID and portsymbol == ":tuner":
                def after_load(_):
                    self.send_notmodified("midi_learn %d mode %f %f" % (TUNER_INSTANCE_ID, minimum, maximum), callback, datatype='boolean')
                self.send_notmodified("add %s %d" % (TUNER_URI, TUNER_INSTANCE_ID), after_load)'''

    if old_learn in host_py_content:
        host_py_content = host_py_content.replace(old_learn, new_learn)

    # 2. Patch kMidiUnlearnURI
    old_unlearn = '''        # So we need special casing for unlearn.
        if actuator_uri == kMidiUnlearnURI:
            if instance_id == PEDALBOARD_INSTANCE_ID and portsymbol == ":tuner":
                self.send_modified("midi_unmap %d mode" % TUNER_INSTANCE_ID, callback, datatype='boolean')'''

    new_unlearn = '''        # So we need special casing for unlearn.
        if actuator_uri == kMidiUnlearnURI:
            if instance_id == PEDALBOARD_INSTANCE_ID and portsymbol == ":tuner":
                def after_load(_):
                    self.send_modified("midi_unmap %d mode" % TUNER_INSTANCE_ID, callback, datatype='boolean')
                self.send_notmodified("add %s %d" % (TUNER_URI, TUNER_INSTANCE_ID), after_load)'''

    if old_unlearn in host_py_content:
        host_py_content = host_py_content.replace(old_unlearn, new_unlearn)

    # 3. Patch midi_map
    old_map = '''                        if instance_id == PEDALBOARD_INSTANCE_ID and portsymbol == ":tuner":
                            self.send_modified("midi_map %d mode %i %i %f %f" % (TUNER_INSTANCE_ID, channel, controller, minimum, maximum), callback, datatype='boolean')'''

    new_map = '''                        if instance_id == PEDALBOARD_INSTANCE_ID and portsymbol == ":tuner":
                            def after_load(_):
                                self.send_modified("midi_map %d mode %i %i %f %f" % (TUNER_INSTANCE_ID, channel, controller, minimum, maximum), callback, datatype='boolean')
                            self.send_notmodified("add %s %d" % (TUNER_URI, TUNER_INSTANCE_ID), after_load)'''

    if old_map in host_py_content:
        host_py_content = host_py_content.replace(old_map, new_map)

    # 4. Patch hmi_tuner_off
    old_off = '''    def hmi_tuner_off(self, callback):
        logging.debug("hmi tuner off")

        def tuner_removed(_):
            if self.current_tuner_mute:
                self.unmute()
            callback(True)

        self.send_notmodified("remove %d" % TUNER_INSTANCE_ID, tuner_removed)'''

    new_off = '''    def hmi_tuner_off(self, callback):
        logging.debug("hmi tuner off")

        try:
            hw_old_port = self.hw_tuner_input_port(self.current_tuner_port)
            from mod.utils import disconnect_jack_ports
            disconnect_jack_ports("system:capture_%s" % hw_old_port, "effect_%d:%s" % (TUNER_INSTANCE_ID, TUNER_INPUT_PORT))
        except:
            pass

        def tuner_disabled(_):
            if self.current_tuner_mute:
                self.unmute()
            callback(True)

        self.send_notmodified("param_set %d mode 0.0" % TUNER_INSTANCE_ID, tuner_disabled)'''

    if old_off in host_py_content:
        host_py_content = host_py_content.replace(old_off, new_off)

    with open(host_py_path, 'w', encoding='utf-8') as f:
        f.write(host_py_content)
    print("  Patched mod/host.py successfully")


    # 10. Patch mod/host.py to restore Tuner MIDI map on pedalboard load
    print("\n10. Patching mod/host.py to restore Tuner MIDI on load...")
    
    load_patch = '''
        if bundlepath:
            # PATCH: Restore Tuner MIDI mapping
            try:
                import json, os, re
                addr_path = os.path.join(bundlepath, "addressings.json")
                if os.path.exists(addr_path):
                    with open(addr_path, 'r') as f:
                        addr_data = json.load(f)
                    for addr in addr_data:
                        if addr.get('instance') == '/pedalboard' and addr.get('port') == ':tuner' and addr.get('uri', '').startswith('http://moddevices.com/ns/mod#midi-custom_'):
                            m = re.match(r'.*_Ch\\.([^a-zA-Z]+)_CC#(\\d+)', addr['uri'])
                            if m:
                                ch = int(m.group(1)) - 1
                                cc = int(m.group(2))
                                def map_tuner_now(_):
                                    self.send_notmodified("midi_map %d mode %i %i 0.0 1.0" % (TUNER_INSTANCE_ID, ch, cc))
                                self.send_notmodified("add %s %d" % (TUNER_URI, TUNER_INSTANCE_ID), map_tuner_now)
            except Exception as e:
                pass
            
            self.load_pb_snapshots(bundlepath)'''

    if 'self.load_pb_snapshots(bundlepath)' in host_py_content and '# PATCH: Restore Tuner' not in host_py_content:
        host_py_content = host_py_content.replace('        if bundlepath:\n            self.load_pb_snapshots(bundlepath)', load_patch)
        with open(host_py_path, 'w', encoding='utf-8') as f:
            f.write(host_py_content)
        print("  Patched mod/host.py (load) successfully")


        # 11. Patch webserver.py for Auto-Restore Tuner MIDI
    print("\n11. Patching mod/webserver.py for MIDI persistence...")
    webserver_py_path = os.path.join(target_dir, "mod", "webserver.py")
    if os.path.exists(webserver_py_path):
        with open(webserver_py_path, 'r', encoding='utf-8') as f:
            ws_content = f.read()
            
        old_ws_code = '''        elif cmd == "tuner_midi_map":\n            ch, cc = data[1].split(" ")\n            SESSION.host.send_notmodified("midi_map 9994 mode %d %d 0.0 1.0" % (int(ch), int(cc)))\n            return'''
        
        ws_patch = '''        elif cmd == "tuner_midi_map":\n            ch, cc = data[1].split(" ")\n            def map_tuner_now(_):\n                SESSION.host.send_notmodified("midi_map 9994 mode %d %d 0.0 1.0" % (int(ch), int(cc)))\n            from mod.settings import TUNER_URI\n            SESSION.host.send_notmodified("add %s 9994" % TUNER_URI, map_tuner_now)\n            return\n\n'''
        

        
        # REPAIR: If previous installer used the wrong TUNER_URI, fix it
        wrong_uri_code = 'add http://moddevices.com/plugins/mod-devel/tuna 9994'
        if wrong_uri_code in ws_content:
            ws_content = ws_content.replace('SESSION.host.send_notmodified("add http://moddevices.com/plugins/mod-devel/tuna 9994", map_tuner_now)', 'from mod.settings import TUNER_URI\n            SESSION.host.send_notmodified("add %s 9994" % TUNER_URI, map_tuner_now)')
            with open(webserver_py_path, 'w', encoding='utf-8') as f:
                f.write(ws_content)
            print("  Repaired wrong TUNER_URI in mod/webserver.py")

        # REPAIR: If previous installer broke indentation, fix it
        broken_code = 'elif cmd == "tuner_midi_map":'
        if broken_code in ws_content and '        elif cmd == "tuner_midi_map":' not in ws_content:
            ws_content = ws_content.replace('elif cmd == "tuner_midi_map":', '        elif cmd == "tuner_midi_map":')
            with open(webserver_py_path, 'w', encoding='utf-8') as f:
                f.write(ws_content)
            print("  Repaired broken indentation in mod/webserver.py")

        if old_ws_code in ws_content:
            ws_content = ws_content.replace(old_ws_code, ws_patch)
            with open(webserver_py_path, 'w', encoding='utf-8') as f:
                f.write(ws_content)
            print("  Upgraded mod/webserver.py patch successfully")
        elif 'map_tuner_now' not in ws_content:
            ws_content = ws_content.replace('        elif cmd == "param_set":', ws_patch + '        elif cmd == "param_set":')
            with open(webserver_py_path, 'w', encoding='utf-8') as f:
                f.write(ws_content)
            print("  Patched mod/webserver.py successfully")
        else:
            print("  mod/webserver.py is already up to date")

    # 12. Deploy Latest LV2 Plugin Bundles (Cyber 9-Series, Tuner, Amps, Pedals)
    print("\n12. Deploying latest LV2 plugin bundles...")
    base_dist = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "dist")
    if not os.path.exists(base_dist):
        base_dist = r"c:\Users\danny\Documents\modep emu\dist"

    lv2_targets = []
    if os.name == 'nt':
        lv2_targets.append(os.path.join(os.path.expanduser('~'), 'Documents', 'MOD Desktop', 'lv2'))
    else:
        lv2_targets.append('/var/modep/lv2')
        lv2_targets.append(os.path.expanduser('~/.lv2'))

    if os.path.exists(base_dist):
        for lv2_target in lv2_targets:
            try:
                os.makedirs(lv2_target, exist_ok=True)
                count = 0
                for item in os.listdir(base_dist):
                    if item.endswith('.lv2'):
                        src_b = os.path.join(base_dist, item)
                        dst_b = os.path.join(lv2_target, item)
                        if os.path.exists(dst_b):
                            shutil.rmtree(dst_b)
                        shutil.copytree(src_b, dst_b)
                        count += 1
                print(f"  Successfully deployed {count} LV2 bundles to {lv2_target}")
            except Exception as e:
                print(f"  Warning deploying to {lv2_target}: {e}")

    print("\n================================================================")
    print("  CYBER STROBE & PEAK TUNER + PLUGINS UPDATE COMPLETE!")
    print("================================================================")
    print(f"A clean backup was preserved at: {backup_dir}")

if __name__ == '__main__':
    main()
