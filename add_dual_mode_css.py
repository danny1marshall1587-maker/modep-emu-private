import re
import sys

# 1. Update CSS in /home/patch/modep-live-console-release/web/css/mixer.css
css_path = "/home/patch/modep-live-console-release/web/css/mixer.css"
with open(css_path, "r", encoding="utf-8") as f:
    css = f.read()

new_css_rules = """
/* ==========================================================================
   DUAL-MODE CHANNEL DSP (LINEAR RACK VS. MODULAR CANVAS)
   ========================================================================== */
.dsp-mode-bar {
    display: flex;
    gap: 3px;
    margin-bottom: 6px;
    background: #0f141c;
    padding: 2px;
    border-radius: 5px;
    border: 1px solid #1f2937;
}

.btn-dsp-mode {
    flex: 1;
    background: transparent;
    border: 1px solid transparent;
    color: #64748b;
    font-size: 0.62rem;
    font-weight: 800;
    padding: 3px 2px;
    border-radius: 4px;
    cursor: pointer;
    text-align: center;
    transition: all 0.15s ease;
    white-space: nowrap;
}

.btn-dsp-mode:hover {
    color: #cbd5e1;
}

.btn-dsp-mode.active {
    background: #1e293b;
    border-color: #38bdf8;
    color: #38bdf8;
    box-shadow: 0 0 6px rgba(56, 189, 248, 0.35);
}

.dsp-container {
    width: 100%;
    margin-bottom: 6px;
}

/* Linear Insert Rack Container */
.linear-slot-container .insert-slots-grid {
    display: flex;
    flex-direction: column;
    gap: 3px;
    margin-bottom: 5px;
}

.insert-slot {
    display: flex;
    align-items: center;
    justify-content: space-between;
    background: #111827;
    border: 1px solid #1f2937;
    border-radius: 4px;
    padding: 2px 4px;
    cursor: pointer;
    transition: all 0.12s ease;
    min-height: 22px;
}

.insert-slot:hover {
    border-color: #38bdf8;
    background: #1e293b;
}

.insert-slot.bypassed {
    opacity: 0.55;
    border-color: #374151;
}

.insert-slot.bypassed .slot-name {
    text-decoration: line-through;
    color: #64748b;
}

.slot-num {
    font-size: 0.58rem;
    font-weight: 900;
    color: #38bdf8;
    width: 12px;
}

.slot-name {
    flex: 1;
    font-size: 0.62rem;
    font-weight: 700;
    color: #e2e8f0;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
    padding: 0 4px;
}

.slot-bypass {
    background: #1f242d;
    border: 1px solid #374151;
    color: #94a3b8;
    font-size: 0.55rem;
    font-weight: 800;
    padding: 1px 4px;
    border-radius: 3px;
    cursor: pointer;
    transition: all 0.12s ease;
}

.slot-bypass.active {
    background: #065f46;
    border-color: #10b981;
    color: #a7f3d0;
}

.slot-bypass:hover {
    filter: brightness(1.2);
}

/* Canvas Router Container */
.canvas-router-container {
    background: #0b111a;
    border: 1px solid #1e293b;
    border-radius: 6px;
    padding: 5px;
    display: flex;
    flex-direction: column;
    gap: 4px;
}

.canvas-rack-select-row {
    display: flex;
    flex-direction: column;
    gap: 2px;
}

.canvas-rack-select-row label {
    font-size: 0.58rem;
    font-weight: 800;
    color: #94a3b8;
    letter-spacing: 0.5px;
}

.canvas-rack-dropdown {
    width: 100%;
    background: #0f172a;
    border: 1px solid #334155;
    color: #38bdf8;
    font-size: 0.62rem;
    font-weight: 700;
    border-radius: 4px;
    padding: 3px 2px;
    cursor: pointer;
}

.canvas-rack-dropdown:focus {
    outline: none;
    border-color: #38bdf8;
    box-shadow: 0 0 6px rgba(56, 189, 248, 0.4);
}

.canvas-actions-row {
    display: flex;
    flex-direction: column;
    gap: 3px;
    margin-top: 2px;
}

.btn-open-canvas {
    width: 100%;
    background: linear-gradient(180deg, #1e293b 0%, #0f172a 100%);
    border: 1.5px solid #10b981;
    color: #34d399;
    border-radius: 5px;
    font-size: 0.68rem;
    font-weight: 800;
    min-height: 28px;
    cursor: pointer;
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 4px;
    transition: all 0.15s ease;
}

.btn-open-canvas:hover {
    background: #065f46;
    color: #fff;
}

.btn-open-canvas:active {
    transform: scale(0.97);
}

.btn-canvas-hard-bypass {
    width: 100%;
    background: #181f2a;
    border: 1px solid #334155;
    color: #94a3b8;
    border-radius: 4px;
    font-size: 0.58rem;
    font-weight: 800;
    padding: 3px;
    cursor: pointer;
    transition: all 0.12s ease;
    text-align: center;
}

.btn-canvas-hard-bypass.bypassed {
    background: #7f1d1d;
    border-color: #ef4444;
    color: #fecaca;
    box-shadow: 0 0 6px rgba(239, 68, 68, 0.4);
}

/* Plugin Picker Modal Card */
.plugin-picker-card {
    background: #111827;
    border: 1.5px solid #38bdf8;
    border-radius: 10px;
    width: 90%;
    max-width: 440px;
    padding: 16px;
    box-shadow: 0 10px 30px rgba(0, 0, 0, 0.8);
    display: flex;
    flex-direction: column;
    gap: 12px;
}

.plugin-picker-list {
    display: grid;
    grid-template-columns: 1fr;
    gap: 6px;
    max-height: 320px;
    overflow-y: auto;
}

.plugin-picker-item {
    display: flex;
    align-items: center;
    justify-content: space-between;
    background: #1f2937;
    border: 1px solid #374151;
    border-radius: 6px;
    padding: 8px 10px;
    cursor: pointer;
    transition: all 0.12s ease;
}

.plugin-picker-item:hover {
    background: #2563eb;
    border-color: #60a5fa;
    color: #fff;
}

.plugin-picker-item .item-title {
    font-size: 0.82rem;
    font-weight: 800;
}

.plugin-picker-item .item-cat {
    font-size: 0.65rem;
    color: #94a3b8;
    text-transform: uppercase;
}
"""

if ".dsp-mode-bar" not in css:
    css += "\n" + new_css_rules
    with open(css_path, "w", encoding="utf-8") as f:
        f.write(css)
    print("Updated mixer.css with Dual-Mode DSP styles.")
else:
    print("mixer.css already contains Dual-Mode styles.")
