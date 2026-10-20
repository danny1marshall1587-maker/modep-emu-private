import re
import sys

# 1. Update HTML files: mixer_index.html and index.html
html_path = "/home/patch/modep-live-console-release/web/mixer_index.html"
with open(html_path, "r", encoding="utf-8") as f:
    html = f.read()

# Define default plugins for the 6 linear slots
default_slots = [
    "DE-NOISER (GATE)",
    "ReaEQ (PARAMETRIC)",
    "FETISH 76 (PEAK COMP)",
    "LALA (OPTO LEVELER)",
    "LOADES (DE-ESSER)",
    "MASTER BUS THRU"
]

def make_channel_dsp_block(ch):
    rack_port = "8881" if ch in [6, 7] else ("8882" if ch in [0, 1] else ("8883" if ch in [2, 3] else "8884"))
    
    slots_html = []
    for idx in range(6):
        s_name = default_slots[idx]
        slots_html.append(f"""        <div class="insert-slot" id="slot-{ch}-{idx}" onclick="openPluginPickerModal({ch}, {idx})">
          <span class="slot-num">{idx + 1}</span>
          <span class="slot-name" id="slot-name-{ch}-{idx}">{s_name}</span>
          <button class="slot-bypass active" id="slot-byp-{ch}-{idx}" onclick="event.stopPropagation(); toggleSlotBypass({ch}, {idx})">ACT</button>
        </div>""")
    slots_joined = "\n".join(slots_html)

    return f"""                    <!-- MODE SELECTOR BAR -->
                    <div class="dsp-mode-bar" id="dsp-mode-bar-{ch}">
                      <button class="btn-dsp-mode active" id="btn-mode-rack-{ch}" onclick="setChannelDspMode({ch}, 'rack')">LINEAR RACK</button>
                      <button class="btn-dsp-mode" id="btn-mode-canvas-{ch}" onclick="setChannelDspMode({ch}, 'canvas')">CANVAS</button>
                    </div>

                    <!-- CONTAINER A: 6-SLOT LINEAR INSERT RACK (Visible when mode === 'rack') -->
                    <div class="dsp-container linear-slot-container" id="dsp-rack-container-{ch}">
                      <div class="insert-slots-grid">
{slots_joined}
                      </div>
                      <div class="btn-fx-quick-open" onclick="showDspRackPage({ch})" title="Open Detailed Channel {ch + 1} Linear DSP Rack">
                        <div class="fx-quick-title">
                          <span>🎛️ FULL RACK VIEW</span>
                        </div>
                      </div>
                    </div>

                    <!-- CONTAINER B: MODULAR CANVAS ROUTER (Visible when mode === 'canvas') -->
                    <div class="dsp-container canvas-router-container" id="dsp-canvas-container-{ch}" style="display: none;">
                      <div class="canvas-rack-select-row">
                        <label>ASSIGN RACK:</label>
                        <select class="canvas-rack-dropdown" id="canvas-rack-select-{ch}" onchange="setChannelCanvasRack({ch}, this.value)">
                          <option value="8881" {'selected' if rack_port=='8881' else ''}>Rack 1 (:8881) - Guitar/Amp (NAM)</option>
                          <option value="8882" {'selected' if rack_port=='8882' else ''}>Rack 2 (:8882) - Acoustic Multi-FX</option>
                          <option value="8883" {'selected' if rack_port=='8883' else ''}>Rack 3 (:8883) - Vocals / Mod / Delay</option>
                          <option value="8884" {'selected' if rack_port=='8884' else ''}>Rack 4 (:8884) - Synth / Aux FX</option>
                        </select>
                      </div>
                      <div class="canvas-actions-row">
                        <button class="btn-open-canvas" onclick="openModepCanvasModal({ch})">OPEN CANVAS</button>
                        <button class="btn-canvas-hard-bypass" id="btn-canvas-bypass-{ch}" onclick="toggleCanvasHardBypass({ch})">CANVAS BYPASS: OFF</button>
                      </div>
                    </div>"""

# Replace each channel's DSP section
for ch in range(8):
    # Pattern to match old DSP section for channel ch:
    # From btn-fx-quick-open up to before <div class="fader-section">
    # Note: ch=0..7
    insert_num = ch + 1
    pattern = rf'(<!-- Clean Single FX Rack Button.*?onclick="showDspRackPage\({ch}\)".*?<!-- Dedicated MODEP Board Insert per Stereo Pair with Isolated Stereo Canvas -->\s*<div class="btn-super-insert modep-insert-btn" onclick="openModepInsert\({insert_num}\)".*?</div>\s*)'
    
    match = re.search(pattern, html, flags=re.DOTALL)
    if match:
        html = html[:match.start()] + make_channel_dsp_block(ch) + "\n\n                    " + html[match.end():]
        print(f"Replaced DSP block for channel {ch}")
    else:
        print(f"Could not find regex match for channel {ch}, trying alternate pattern...")

# Also add the Plugin Picker Modal and Canvas Modal if not present
picker_modal_html = """
    <!-- Plugin Picker Modal -->
    <div class="modal-overlay" id="plugin-picker-overlay" onclick="if(event.target===this)closePluginPickerModal()" style="display:none; position:fixed; inset:0; background:rgba(0,0,0,0.75); z-index:9999; align-items:center; justify-content:center;">
        <div class="plugin-picker-card">
            <div class="modal-header" style="display:flex; justify-content:space-between; align-items:center; border-bottom:1px solid #374151; padding-bottom:8px;">
                <div class="modal-title" id="plugin-picker-title" style="font-size:0.95rem; font-weight:800; color:#38bdf8;">Select Insert Plugin</div>
                <button class="btn-close-modal" onclick="closePluginPickerModal()" style="background:transparent; border:none; color:#94a3b8; font-size:1.4rem; cursor:pointer;">&times;</button>
            </div>
            <div class="plugin-picker-list" id="plugin-picker-list">
                <!-- Dynamically rendered plugins -->
            </div>
        </div>
    </div>

    <!-- MODEP Visual Canvas Modal (Iframe / Remote Window) -->
    <div class="modal-overlay" id="canvas-modal-overlay" onclick="if(event.target===this)closeCanvasModal()" style="display:none; position:fixed; inset:0; background:rgba(0,0,0,0.85); z-index:9999; align-items:center; justify-content:center; padding:12px;">
        <div style="background:#111827; border:1.5px solid #10b981; border-radius:10px; width:95%; height:92%; display:flex; flex-direction:column; overflow:hidden; box-shadow:0 12px 35px rgba(0,0,0,0.9);">
            <div style="display:flex; justify-content:space-between; align-items:center; background:#0f172a; padding:8px 14px; border-bottom:1px solid #1e293b;">
                <div id="canvas-modal-title" style="font-size:0.9rem; font-weight:800; color:#34d399;">MODEP Modular Canvas</div>
                <div style="display:flex; gap:8px; align-items:center;">
                    <a id="canvas-modal-newtab" href="#" target="_blank" style="color:#60a5fa; font-size:0.75rem; text-decoration:none; font-weight:700;">[ POP OUT ↗ ]</a>
                    <button onclick="closeCanvasModal()" style="background:#1e293b; border:1px solid #374151; color:#fff; border-radius:4px; font-weight:800; cursor:pointer; padding:2px 8px;">✕ CLOSE</button>
                </div>
            </div>
            <iframe id="canvas-modal-iframe" src="about:blank" style="flex:1; width:100%; height:100%; border:none; background:#0f141c;"></iframe>
        </div>
    </div>
"""

if 'id="plugin-picker-overlay"' not in html:
    idx_end_body = html.find("</body>")
    if idx_end_body != -1:
        html = html[:idx_end_body] + picker_modal_html + "\n" + html[idx_end_body:]
        print("Added plugin-picker-overlay and canvas-modal-overlay to HTML.")

with open(html_path, "w", encoding="utf-8") as f:
    f.write(html)

with open("/home/patch/modep-live-console-release/web/index.html", "w", encoding="utf-8") as f:
    f.write(html)

print("Saved mixer_index.html and synchronized index.html.")
