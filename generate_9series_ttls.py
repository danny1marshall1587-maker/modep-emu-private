import os

def generate_modguis():
    with open("plugins/cyber-organ9.lv2/modgui/icon-cyber-organ9.html", "r", encoding="utf-8") as f:
        org_html_lines = f.readlines()
    with open("plugins/cyber-organ9.lv2/modgui/stylesheet.css", "r", encoding="utf-8") as f:
        org_css = f.read()
    with open("plugins/cyber-organ9.lv2/modgui/script.js", "r", encoding="utf-8") as f:
        org_js = f.read()

    sec_a = "".join(org_html_lines[:186])
    sec_dev_rack = "".join(org_html_lines[290:357])
    sec_d_and_tail = "".join(org_html_lines[428:])

    configs = {
        'cyber-mel9.lv2': {
            'prefix': 'mel9',
            'title': 'MEL 9',
            'subtitle': 'VINTAGE TAPE INSTRUMENTS &bull; MELLOTRON M400 EMULATION &bull; CHORD HARMONY',
            'inst_label': 'TAPE INSTRUMENT',
            'inst_symbol': 'preset',
            'inst_options': [
                (0, '1: Orchestra'), (1, '2: Cello'), (2, '3: Strings'),
                (3, '4: Flutes'), (4, '5: Clarinet'), (5, '6: Saxophone'),
                (6, '7: Brass'), (7, '8: Low Choir'), (8, '9: High Choir')
            ],
            'knobs': [
                ('flutter', 'FLUTTER', 0, 10, 3, 'Tape Wow and Flutter'),
                ('warmth', 'WARMTH', 0, 10, 4, 'Tape Warmth and Saturation'),
                ('attack', 'ATTACK', 0, 10, 2, 'Tape Attack Time'),
                ('sustain', 'SUSTAIN', 0, 10, 5, 'Chord Sustain'),
                ('release', 'RELEASE', 0, 10, 5, 'Release Decay'),
                ('gate_sens', 'GATE', 0, 10, 5, 'Gate Sensitivity'),
                ('dry', 'DRY', 0, 10, 5, 'Dry Guitar Level'),
                ('wet', 'MEL VOL', 0, 10, 8, 'Mellotron Volume'),
                ('output', 'OUTPUT', 0, 10, 7, 'Master Output Level')
            ],
            'theme_bg': 'linear-gradient(180deg, #181512 0%, #231e1a 100%)',
            'accent': '#d4b483'
        },
        'cyber-synth9.lv2': {
            'prefix': 'syn9',
            'title': 'SYNTH 9',
            'subtitle': 'VINTAGE ANALOG POLYSYNTH &bull; MOOG LADDER &bull; CHORD HARMONY',
            'inst_label': 'SYNTH MODEL',
            'inst_symbol': 'preset',
            'inst_options': [
                (0, '1: OBX (Warm Brass)'), (1, '2: Profit 5 (Poly Strings)'),
                (2, '3: Moog Lead (Fat Solo)'), (3, '4: Moodys (Square Pad)'),
                (4, '5: String Synth (Solina)'), (5, '6: Poly VI (Korg Chorus Synth)'),
                (6, '7: Mini Mood (Sub Lead)'), (7, '8: Laser (Sweep Filter)'),
                (8, '9: Micro (MicroMoog PWM)')
            ],
            'knobs': [
                ('cutoff', 'CUTOFF', 0, 10, 5, '4-Pole Filter Cutoff'),
                ('resonance', 'RESO', 0, 10, 4, 'Filter Resonance'),
                ('env_mod', 'ENV MOD', 0, 10, 5, 'Filter Envelope Sweep'),
                ('attack', 'ATTACK', 0, 10, 2, 'Attack Time'),
                ('sustain', 'SUSTAIN', 0, 10, 5, 'Chord Sustain'),
                ('gate_sens', 'GATE', 0, 10, 5, 'Gate Sensitivity'),
                ('dry', 'DRY', 0, 10, 5, 'Dry Guitar Level'),
                ('wet', 'SYNTH VOL', 0, 10, 8, 'Synth Volume'),
                ('output', 'OUTPUT', 0, 10, 7, 'Master Output Level')
            ],
            'theme_bg': 'linear-gradient(180deg, #10141a 0%, #171d26 100%)',
            'accent': '#4ea8de'
        },
        'cyber-string9.lv2': {
            'prefix': 'str9',
            'title': 'STRING 9',
            'subtitle': 'SOLINA STRINGS &bull; SYMPHONIC STRINGS &bull; TRIPLE BBD CHORUS &bull; CHORD HARMONY',
            'inst_label': 'ENSEMBLE MODE',
            'inst_symbol': 'preset',
            'inst_options': [
                (0, '1: Symphonic'), (1, '2: Orchestra'), (2, '3: Choir'),
                (3, '4: Freeze / Sustain'), (4, '5: Rotary String')
            ],
            'knobs': [
                ('ensemble_depth', 'CHORUS', 0, 10, 7, 'Triple BBD Chorus Depth'),
                ('brightness', 'TONE', 0, 10, 6, 'String High Sheen & Tone'),
                ('attack', 'ATTACK', 0, 10, 4, 'String Attack Time'),
                ('sustain', 'SUSTAIN', 0, 10, 5, 'Chord Sustain'),
                ('release', 'RELEASE', 0, 10, 5, 'Release Decay'),
                ('gate_sens', 'GATE', 0, 10, 5, 'Gate Sensitivity'),
                ('dry', 'DRY', 0, 10, 5, 'Dry Guitar Level'),
                ('wet', 'STR VOL', 0, 10, 8, 'String Ensemble Volume'),
                ('output', 'OUTPUT', 0, 10, 7, 'Master Output Level')
            ],
            'theme_bg': 'linear-gradient(180deg, #0d1821 0%, #14212d 100%)',
            'accent': '#5bc0be'
        },
        'cyber-bass9.lv2': {
            'prefix': 'bas9',
            'title': 'BASS 9',
            'subtitle': 'VINTAGE BASS MACHINE &bull; P-BASS, FRETLESS, SYNTH &bull; CHORD HARMONY',
            'inst_label': 'BASS MACHINE PRESET',
            'inst_symbol': 'preset',
            'inst_options': [
                (0, '1: Precision (P-Bass)'), (1, '2: Longhorn (Baritone)'),
                (2, '3: Fretless (Smooth Mwah)'), (3, '4: Synth (Taurus Sub)'),
                (4, '5: Virtual (Dynamic Contour)'), (5, '6: Bowed (Upright Contrabass)'),
                (6, '7: Split Bass (Low Split Poly)'), (7, '8: 3:03 (Acid Bass Synth)'),
                (8, '9: Flip-Flop (Dual Sub-Octave)')
            ],
            'knobs': [
                ('ctrl1', 'CTRL 1', 0, 10, 5, 'Tone / Attack / Characteristic 1'),
                ('ctrl2', 'CTRL 2', 0, 10, 5, 'Filter / Resonance / Sub Level 2'),
                ('attack', 'ATTACK', 0, 10, 2, 'Attack Speed'),
                ('sustain', 'SUSTAIN', 0, 10, 5, 'Chord & Note Sustain'),
                ('release', 'RELEASE', 0, 10, 5, 'Release Decay'),
                ('gate_sens', 'GATE', 0, 10, 5, 'Gate Sensitivity'),
                ('dry', 'DRY', 0, 10, 5, 'Dry Guitar Level'),
                ('wet', 'BASS VOL', 0, 10, 8, 'Bass Machine Volume'),
                ('output', 'OUTPUT', 0, 10, 7, 'Master Output Level')
            ],
            'theme_bg': 'linear-gradient(180deg, #181c14 0%, #22291d 100%)',
            'accent': '#aacc00'
        }
    }

    for p_name, cfg in configs.items():
        pfx = cfg['prefix']
        m_dir = os.path.join('plugins', p_name, 'modgui')
        os.makedirs(m_dir, exist_ok=True)

        opt_lines = ""
        for v, lbl in cfg['inst_options']:
            opt_lines += f'                    <option value="{v}">{lbl}</option>\n'

        inst_sec = f"""        <div class="{pfx}-section {pfx}-inst-section">
            <div class="{pfx}-section-header">
                <div class="{pfx}-section-title">{cfg['inst_label']}</div>
            </div>
            <div class="{pfx}-inst-box">
                <select class="{pfx}-select {pfx}-inst-select" id="{pfx}-select-inst" title="{cfg['inst_label']}">
{opt_lines}                </select>
                <div class="mod-knob-image" mod-role="input-control-port" mod-port-symbol="{cfg['inst_symbol']}" style="display:none;"></div>
                <div class="{pfx}-inst-display" id="{pfx}-inst-display">{cfg['inst_options'][0][1]}</div>
            </div>
        </div>
    </div>

"""

        knob_lines = f'    <div class="{pfx}-perf-bar">\n'
        for sym, lbl, vmin, vmax, vdef, title in cfg['knobs']:
            d_cls = f"{pfx}-dial"
            if sym in ('wet', 'output'):
                d_cls += f" {pfx}-dial-out"
            elif sym == 'gate_sens':
                d_cls += f" {pfx}-dial-gate"
            elif sym in ('sustain', 'release', 'flutter', 'cutoff', 'ensemble_depth'):
                d_cls += f" {pfx}-dial-highlight"

            knob_lines += f"""        <div class="{pfx}-perf-unit" title="{title}">
            <div class="{d_cls}" data-sym="{sym}" data-min="{vmin}" data-max="{vmax}" data-def="{vdef}"><div class="{pfx}-dial-rotor"></div></div>
            <div class="mod-knob-image" mod-role="input-control-port" mod-port-symbol="{sym}" style="display:none;"></div>
            <span class="{pfx}-perf-lbl">{lbl}</span>
        </div>\n"""
        knob_lines += "    </div>\n\n"

        # Combine HTML
        h_part = sec_a.replace('org9-', f'{pfx}-').replace('ORGAN 9', cfg['title']).replace(
            'HAMMOND B3 TONEWHEEL PAD &bull; LESLIE 122 ROTARY &bull; CHORD HARMONY', cfg['subtitle']
        )
        dev_part = sec_dev_rack.replace('org9-', f'{pfx}-')
        t_part = sec_d_and_tail.replace('org9-', f'{pfx}-')
        full_html = h_part + inst_sec + dev_part + knob_lines + t_part

        html_f = os.path.join(m_dir, f"icon-{p_name[:-4]}.html")
        with open(html_f, "w", encoding="utf-8") as f:
            f.write(full_html)

        # CSS
        c = org_css.replace('org9-', f'{pfx}-')
        c = c.replace('linear-gradient(180deg, #110e0c 0%, #1a1512 100%)', cfg['theme_bg'])
        c = c.replace('#c48a37', cfg['accent'])
        c += f"""
.{pfx}-inst-section {{ width: 260px; display: flex; flex-direction: column; }}
.{pfx}-inst-box {{ display: flex; flex-direction: column; gap: 8px; padding: 16px 8px; align-items: center; justify-content: center; flex: 1; }}
.{pfx}-inst-select {{ width: 90%; padding: 8px; font-size: 11px; border: 1px solid {cfg['accent']}88; background: #181411; }}
.{pfx}-inst-display {{ font-size: 12px; font-weight: 800; color: {cfg['accent']}; letter-spacing: 1px; text-align: center; }}
"""
        with open(os.path.join(m_dir, "stylesheet.css"), "w", encoding="utf-8") as f:
            f.write(c)

        # JS
        j = org_js.replace('org9-', f'{pfx}-')
        # Wire instrument select and apply port
        inst_wire = f"""
    pedal.find('#{pfx}-select-inst').off('change.org').on('change.org', function () {{
        var val = parseFloat($(this).val()) || 0;
        pedal.find('#{pfx}-inst-display').text($(this).find('option:selected').text());
        sendPortValue('{cfg['inst_symbol']}', val);
    }});
"""
        inst_apply = f"""
        if (symbol === '{cfg['inst_symbol']}') {{
            var sel = pedal.find('#{pfx}-select-inst');
            if (sel.length) {{
                sel.val(Math.round(fVal));
                pedal.find('#{pfx}-inst-display').text(sel.find('option:selected').text());
            }}
            return;
        }}
"""
        j = j.replace("if (symbol === 'rotary') {", inst_apply + "if (symbol === 'rotary') {")
        j = j.replace("// Rotary switch click", inst_wire + "// Rotary switch click")

        with open(os.path.join(m_dir, "script.js"), "w", encoding="utf-8") as f:
            f.write(j)

        print(f"[+] Successfully generated complete modgui for {p_name}")

generate_modguis()

# ── CYBER MEL 9 ─────────────────────────────────────────────────────────────
mel9_ttl = """@prefix lv2:   <http://lv2plug.in/ns/lv2core#> .
@prefix rdf:   <http://www.w3.org/1999/02/22-rdf-syntax-ns#> .
@prefix rdfs:  <http://www.w3.org/2000/01/rdf-schema#> .
@prefix doap:  <http://usefulinc.com/ns/doap#> .
@prefix mod:   <http://moddevices.com/ns#> .

<http://cyber-audio.co.uk/plugins/cyber-mel9>
    a lv2:Plugin , lv2:InstrumentPlugin ;
    doap:name "Cyber Mel 9" ;
    doap:license <http://opensource.org/licenses/mit> ;
    mod:brand "CyberAudio" ;
    mod:label "Mel 9" ;
    rdfs:comment "Vintage Mellotron tape replay sound machine with 9 instruments, tape flutter and saturation, Note Selector chord harmonies, and rhythm bass player." ;

    lv2:port [
        a lv2:AudioPort , lv2:InputPort ;
        lv2:index 0 ; lv2:symbol "in_l" ; lv2:name "Guitar In L/Mono" ;
    ] , [
        a lv2:AudioPort , lv2:InputPort ;
        lv2:index 1 ; lv2:symbol "in_r" ; lv2:name "Guitar In R" ;
    ] , [
        a lv2:AudioPort , lv2:OutputPort ;
        lv2:index 2 ; lv2:symbol "out_l" ; lv2:name "Mel Out L" ;
    ] , [
        a lv2:AudioPort , lv2:OutputPort ;
        lv2:index 3 ; lv2:symbol "out_r" ; lv2:name "Mel Out R" ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 4 ; lv2:symbol "bypass" ; lv2:name "Bypass" ;
        lv2:default 1.0 ; lv2:minimum 0.0 ; lv2:maximum 1.0 ;
        lv2:portProperty lv2:toggled , lv2:integer ;
        lv2:designation lv2:enabled ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 5 ; lv2:symbol "third_mode" ; lv2:name "3rd Quality" ;
        lv2:default 0.0 ; lv2:minimum 0.0 ; lv2:maximum 2.0 ;
        lv2:portProperty lv2:enumeration , lv2:integer ;
        lv2:scalePoint [ rdfs:label "Auto (detect from guitar)" ; rdf:value 0 ] ;
        lv2:scalePoint [ rdfs:label "Force Minor 3rd" ; rdf:value 1 ] ;
        lv2:scalePoint [ rdfs:label "Force Major 3rd" ; rdf:value 2 ] ;

    # ── NOTE SELECTOR ─────────────────────────────────────────────────────

    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 6 ; lv2:symbol "ns_sub_oct" ; lv2:name "Sub Octave" ;
        lv2:default 0.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 7 ; lv2:symbol "ns_sub_fifth" ; lv2:name "Sub Fifth" ;
        lv2:default 0.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 8 ; lv2:symbol "ns_sub_third" ; lv2:name "Sub Third" ;
        lv2:default 0.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 9 ; lv2:symbol "ns_root" ; lv2:name "Root" ;
        lv2:default 8.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 10 ; lv2:symbol "ns_third" ; lv2:name "+3rd" ;
        lv2:default 7.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 11 ; lv2:symbol "ns_fifth" ; lv2:name "+5th" ;
        lv2:default 6.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 12 ; lv2:symbol "ns_oct" ; lv2:name "+Oct" ;
        lv2:default 5.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 13 ; lv2:symbol "ns_oct_third" ; lv2:name "+Oct+3rd" ;
        lv2:default 0.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 14 ; lv2:symbol "ns_oct_fifth" ; lv2:name "+Oct+5th" ;
        lv2:default 0.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 15 ; lv2:symbol "ns_dbl_oct" ; lv2:name "+2 Oct" ;
        lv2:default 0.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;

    # ── MELLOTRON INSTRUMENTS & TAPE PARAMS ───────────────────────────────

    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 16 ; lv2:symbol "preset" ; lv2:name "Mellotron Instrument" ;
        lv2:default 0.0 ; lv2:minimum 0.0 ; lv2:maximum 8.0 ;
        lv2:portProperty lv2:enumeration , lv2:integer ;
        lv2:scalePoint [ rdfs:label "1: Orchestra" ; rdf:value 0 ] ;
        lv2:scalePoint [ rdfs:label "2: Cello" ; rdf:value 1 ] ;
        lv2:scalePoint [ rdfs:label "3: Strings" ; rdf:value 2 ] ;
        lv2:scalePoint [ rdfs:label "4: Flutes" ; rdf:value 3 ] ;
        lv2:scalePoint [ rdfs:label "5: Clarinet" ; rdf:value 4 ] ;
        lv2:scalePoint [ rdfs:label "6: Saxophone" ; rdf:value 5 ] ;
        lv2:scalePoint [ rdfs:label "7: Brass" ; rdf:value 6 ] ;
        lv2:scalePoint [ rdfs:label "8: Low Choir" ; rdf:value 7 ] ;
        lv2:scalePoint [ rdfs:label "9: High Choir" ; rdf:value 8 ] ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 17 ; lv2:symbol "attack" ; lv2:name "Attack Time" ;
        lv2:default 2.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 18 ; lv2:symbol "sustain" ; lv2:name "Sustain" ;
        lv2:default 5.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 19 ; lv2:symbol "release" ; lv2:name "Release Slope" ;
        lv2:default 5.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 20 ; lv2:symbol "flutter" ; lv2:name "Tape Wow and Flutter" ;
        lv2:default 3.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 21 ; lv2:symbol "warmth" ; lv2:name "Tape Warmth / Saturation" ;
        lv2:default 4.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 22 ; lv2:symbol "gate_sens" ; lv2:name "Gate Sens" ;
        lv2:default 5.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 23 ; lv2:symbol "dry" ; lv2:name "Dry Level" ;
        lv2:default 5.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 24 ; lv2:symbol "wet" ; lv2:name "Mel Level" ;
        lv2:default 8.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 25 ; lv2:symbol "output" ; lv2:name "Output Level" ;
        lv2:default 7.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] ,

    # ── KEY & SCALE DIATONIC LOCK ────────────────────────────────────────

    [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 26 ; lv2:symbol "key_root" ; lv2:name "Diatonic Key Lock" ;
        lv2:default 0 ; lv2:minimum 0 ; lv2:maximum 12 ;
        lv2:portProperty lv2:enumeration , lv2:integer ;
        lv2:scalePoint [ rdfs:label "Auto (Full Chromatic)" ; rdf:value 0 ] ;
        lv2:scalePoint [ rdfs:label "C" ; rdf:value 1 ] ;
        lv2:scalePoint [ rdfs:label "C#" ; rdf:value 2 ] ;
        lv2:scalePoint [ rdfs:label "D" ; rdf:value 3 ] ;
        lv2:scalePoint [ rdfs:label "D#" ; rdf:value 4 ] ;
        lv2:scalePoint [ rdfs:label "E" ; rdf:value 5 ] ;
        lv2:scalePoint [ rdfs:label "F" ; rdf:value 6 ] ;
        lv2:scalePoint [ rdfs:label "F#" ; rdf:value 7 ] ;
        lv2:scalePoint [ rdfs:label "G" ; rdf:value 8 ] ;
        lv2:scalePoint [ rdfs:label "G#" ; rdf:value 9 ] ;
        lv2:scalePoint [ rdfs:label "A" ; rdf:value 10 ] ;
        lv2:scalePoint [ rdfs:label "A#" ; rdf:value 11 ] ;
        lv2:scalePoint [ rdfs:label "B" ; rdf:value 12 ] ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 27 ; lv2:symbol "scale_mode" ; lv2:name "Scale Mode" ;
        lv2:default 0 ; lv2:minimum 0 ; lv2:maximum 2 ;
        lv2:portProperty lv2:enumeration , lv2:integer ;
        lv2:scalePoint [ rdfs:label "Auto" ; rdf:value 0 ] ;
        lv2:scalePoint [ rdfs:label "Major / Ionian" ; rdf:value 1 ] ;
        lv2:scalePoint [ rdfs:label "Minor / Aeolian" ; rdf:value 2 ] ;
    ] ,

    # ── DUAL PITCH DETECTOR CALIBRATION ──────────────────────────────────

    [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 28 ; lv2:symbol "track_sens" ; lv2:name "Detector Sensitivity" ;
        lv2:default 8.0 ; lv2:minimum 0.1 ; lv2:maximum 20.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 29 ; lv2:symbol "track_stab" ; lv2:name "Chord Stability / Hysteresis" ;
        lv2:default 6.0 ; lv2:minimum 0.1 ; lv2:maximum 20.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 30 ; lv2:symbol "debounce_speed" ; lv2:name "Debounce / Confirmation" ;
        lv2:default 2.0 ; lv2:minimum 1.0 ; lv2:maximum 16.0 ;
        lv2:portProperty lv2:integer ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 31 ; lv2:symbol "filter_q" ; lv2:name "Resonator Bandpass Q" ;
        lv2:default 7.0 ; lv2:minimum 1.5 ; lv2:maximum 30.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 32 ; lv2:symbol "bass_boost" ; lv2:name "Bass Weighting" ;
        lv2:default 6.0 ; lv2:minimum 0.0 ; lv2:maximum 20.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 33 ; lv2:symbol "agc_speed" ; lv2:name "Auto Gain Speed" ;
        lv2:default 6.0 ; lv2:minimum 0.0 ; lv2:maximum 20.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 34 ; lv2:symbol "ac_weight" ; lv2:name "Autocorrelation 2nd-Detector Mix" ;
        lv2:default 8.0 ; lv2:minimum 0.0 ; lv2:maximum 20.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 35 ; lv2:symbol "ac_clip" ; lv2:name "Time-Domain Center Clip" ;
        lv2:default 4.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;

    # ── AUTO LOWEST-NOTE RHYTHM BASS PLAYER ──────────────────────────────

    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 36 ; lv2:symbol "bass_level" ; lv2:name "Bass Level" ;
        lv2:default 0.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 37 ; lv2:symbol "bass_pattern" ; lv2:name "Bass Pattern" ;
        lv2:default 1.0 ; lv2:minimum 0.0 ; lv2:maximum 6.0 ;
        lv2:portProperty lv2:enumeration , lv2:integer ;
        lv2:scalePoint [ rdfs:label "0: Off" ; rdf:value 0 ] ;
        lv2:scalePoint [ rdfs:label "1: Drone Sustain" ; rdf:value 1 ] ;
        lv2:scalePoint [ rdfs:label "2: 8th Note Rock Pulse" ; rdf:value 2 ] ;
        lv2:scalePoint [ rdfs:label "3: Root-Octave Groove" ; rdf:value 3 ] ;
        lv2:scalePoint [ rdfs:label "4: Root-5th Country/Blues" ; rdf:value 4 ] ;
        lv2:scalePoint [ rdfs:label "5: Walking Bass" ; rdf:value 5 ] ;
        lv2:scalePoint [ rdfs:label "6: Dub / Reggae Drop" ; rdf:value 6 ] ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 38 ; lv2:symbol "bass_bpm" ; lv2:name "Bass Tempo (BPM)" ;
        lv2:default 120.0 ; lv2:minimum 40.0 ; lv2:maximum 240.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 39 ; lv2:symbol "bass_tap" ; lv2:name "Bass Tap Tempo" ;
        lv2:default 0.0 ; lv2:minimum 0.0 ; lv2:maximum 1.0 ;
        lv2:portProperty lv2:toggled ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 40 ; lv2:symbol "bass_tone" ; lv2:name "Bass Tone / Lowpass" ;
        lv2:default 5.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] .
"""

# ── CYBER SYNTH 9 ───────────────────────────────────────────────────────────
synth9_ttl = """@prefix lv2:   <http://lv2plug.in/ns/lv2core#> .
@prefix rdf:   <http://www.w3.org/1999/02/22-rdf-syntax-ns#> .
@prefix rdfs:  <http://www.w3.org/2000/01/rdf-schema#> .
@prefix doap:  <http://usefulinc.com/ns/doap#> .
@prefix mod:   <http://moddevices.com/ns#> .

<http://cyber-audio.co.uk/plugins/cyber-synth9>
    a lv2:Plugin , lv2:InstrumentPlugin ;
    doap:name "Cyber Synth 9" ;
    doap:license <http://opensource.org/licenses/mit> ;
    mod:brand "CyberAudio" ;
    mod:label "Synth 9" ;
    rdfs:comment "Vintage analog synthesizer suite with 9 synth circuits, Moog-style 4-pole resonant ladder filter, envelope sweeps, Note Selector harmonies, and rhythm bass player." ;

    lv2:port [
        a lv2:AudioPort , lv2:InputPort ;
        lv2:index 0 ; lv2:symbol "in_l" ; lv2:name "Guitar In L/Mono" ;
    ] , [
        a lv2:AudioPort , lv2:InputPort ;
        lv2:index 1 ; lv2:symbol "in_r" ; lv2:name "Guitar In R" ;
    ] , [
        a lv2:AudioPort , lv2:OutputPort ;
        lv2:index 2 ; lv2:symbol "out_l" ; lv2:name "Synth Out L" ;
    ] , [
        a lv2:AudioPort , lv2:OutputPort ;
        lv2:index 3 ; lv2:symbol "out_r" ; lv2:name "Synth Out R" ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 4 ; lv2:symbol "bypass" ; lv2:name "Bypass" ;
        lv2:default 1.0 ; lv2:minimum 0.0 ; lv2:maximum 1.0 ;
        lv2:portProperty lv2:toggled , lv2:integer ;
        lv2:designation lv2:enabled ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 5 ; lv2:symbol "third_mode" ; lv2:name "3rd Quality" ;
        lv2:default 0.0 ; lv2:minimum 0.0 ; lv2:maximum 2.0 ;
        lv2:portProperty lv2:enumeration , lv2:integer ;
        lv2:scalePoint [ rdfs:label "Auto (detect from guitar)" ; rdf:value 0 ] ;
        lv2:scalePoint [ rdfs:label "Force Minor 3rd" ; rdf:value 1 ] ;
        lv2:scalePoint [ rdfs:label "Force Major 3rd" ; rdf:value 2 ] ;

    # ── NOTE SELECTOR ─────────────────────────────────────────────────────

    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 6 ; lv2:symbol "ns_sub_oct" ; lv2:name "Sub Octave" ;
        lv2:default 0.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 7 ; lv2:symbol "ns_sub_fifth" ; lv2:name "Sub Fifth" ;
        lv2:default 0.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 8 ; lv2:symbol "ns_sub_third" ; lv2:name "Sub Third" ;
        lv2:default 0.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 9 ; lv2:symbol "ns_root" ; lv2:name "Root" ;
        lv2:default 8.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 10 ; lv2:symbol "ns_third" ; lv2:name "+3rd" ;
        lv2:default 7.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 11 ; lv2:symbol "ns_fifth" ; lv2:name "+5th" ;
        lv2:default 6.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 12 ; lv2:symbol "ns_oct" ; lv2:name "+Oct" ;
        lv2:default 5.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 13 ; lv2:symbol "ns_oct_third" ; lv2:name "+Oct+3rd" ;
        lv2:default 0.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 14 ; lv2:symbol "ns_oct_fifth" ; lv2:name "+Oct+5th" ;
        lv2:default 0.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 15 ; lv2:symbol "ns_dbl_oct" ; lv2:name "+2 Oct" ;
        lv2:default 0.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;

    # ── SYNTH CIRCUITS & FILTER PARAMS ───────────────────────────────────

    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 16 ; lv2:symbol "preset" ; lv2:name "Synth Model" ;
        lv2:default 0.0 ; lv2:minimum 0.0 ; lv2:maximum 8.0 ;
        lv2:portProperty lv2:enumeration , lv2:integer ;
        lv2:scalePoint [ rdfs:label "1: OBX Poly" ; rdf:value 0 ] ;
        lv2:scalePoint [ rdfs:label "2: Profit 5" ; rdf:value 1 ] ;
        lv2:scalePoint [ rdfs:label "3: Moog Lead" ; rdf:value 2 ] ;
        lv2:scalePoint [ rdfs:label "4: Moodys" ; rdf:value 3 ] ;
        lv2:scalePoint [ rdfs:label "5: String Synth" ; rdf:value 4 ] ;
        lv2:scalePoint [ rdfs:label "6: Poly VI" ; rdf:value 5 ] ;
        lv2:scalePoint [ rdfs:label "7: Mini Mood" ; rdf:value 6 ] ;
        lv2:scalePoint [ rdfs:label "8: Laser" ; rdf:value 7 ] ;
        lv2:scalePoint [ rdfs:label "9: Micro" ; rdf:value 8 ] ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 17 ; lv2:symbol "cutoff" ; lv2:name "Filter Cutoff" ;
        lv2:default 5.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 18 ; lv2:symbol "resonance" ; lv2:name "Filter Resonance" ;
        lv2:default 4.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 19 ; lv2:symbol "env_mod" ; lv2:name "Envelope Sweep" ;
        lv2:default 5.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 20 ; lv2:symbol "attack" ; lv2:name "Attack Time" ;
        lv2:default 2.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 21 ; lv2:symbol "sustain" ; lv2:name "Sustain" ;
        lv2:default 5.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 22 ; lv2:symbol "gate_sens" ; lv2:name "Gate Sens" ;
        lv2:default 5.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 23 ; lv2:symbol "dry" ; lv2:name "Dry Level" ;
        lv2:default 5.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 24 ; lv2:symbol "wet" ; lv2:name "Synth Level" ;
        lv2:default 8.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 25 ; lv2:symbol "output" ; lv2:name "Output Level" ;
        lv2:default 7.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] ,

    # ── KEY & SCALE DIATONIC LOCK ────────────────────────────────────────

    [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 26 ; lv2:symbol "key_root" ; lv2:name "Diatonic Key Lock" ;
        lv2:default 0 ; lv2:minimum 0 ; lv2:maximum 12 ;
        lv2:portProperty lv2:enumeration , lv2:integer ;
        lv2:scalePoint [ rdfs:label "Auto (Full Chromatic)" ; rdf:value 0 ] ;
        lv2:scalePoint [ rdfs:label "C" ; rdf:value 1 ] ;
        lv2:scalePoint [ rdfs:label "C#" ; rdf:value 2 ] ;
        lv2:scalePoint [ rdfs:label "D" ; rdf:value 3 ] ;
        lv2:scalePoint [ rdfs:label "D#" ; rdf:value 4 ] ;
        lv2:scalePoint [ rdfs:label "E" ; rdf:value 5 ] ;
        lv2:scalePoint [ rdfs:label "F" ; rdf:value 6 ] ;
        lv2:scalePoint [ rdfs:label "F#" ; rdf:value 7 ] ;
        lv2:scalePoint [ rdfs:label "G" ; rdf:value 8 ] ;
        lv2:scalePoint [ rdfs:label "G#" ; rdf:value 9 ] ;
        lv2:scalePoint [ rdfs:label "A" ; rdf:value 10 ] ;
        lv2:scalePoint [ rdfs:label "A#" ; rdf:value 11 ] ;
        lv2:scalePoint [ rdfs:label "B" ; rdf:value 12 ] ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 27 ; lv2:symbol "scale_mode" ; lv2:name "Scale Mode" ;
        lv2:default 0 ; lv2:minimum 0 ; lv2:maximum 2 ;
        lv2:portProperty lv2:enumeration , lv2:integer ;
        lv2:scalePoint [ rdfs:label "Auto" ; rdf:value 0 ] ;
        lv2:scalePoint [ rdfs:label "Major / Ionian" ; rdf:value 1 ] ;
        lv2:scalePoint [ rdfs:label "Minor / Aeolian" ; rdf:value 2 ] ;
    ] ,

    # ── DUAL PITCH DETECTOR CALIBRATION ──────────────────────────────────

    [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 28 ; lv2:symbol "track_sens" ; lv2:name "Detector Sensitivity" ;
        lv2:default 8.0 ; lv2:minimum 0.1 ; lv2:maximum 20.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 29 ; lv2:symbol "track_stab" ; lv2:name "Chord Stability / Hysteresis" ;
        lv2:default 6.0 ; lv2:minimum 0.1 ; lv2:maximum 20.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 30 ; lv2:symbol "debounce_speed" ; lv2:name "Debounce / Confirmation" ;
        lv2:default 2.0 ; lv2:minimum 1.0 ; lv2:maximum 16.0 ;
        lv2:portProperty lv2:integer ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 31 ; lv2:symbol "filter_q" ; lv2:name "Resonator Bandpass Q" ;
        lv2:default 7.0 ; lv2:minimum 1.5 ; lv2:maximum 30.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 32 ; lv2:symbol "bass_boost" ; lv2:name "Bass Weighting" ;
        lv2:default 6.0 ; lv2:minimum 0.0 ; lv2:maximum 20.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 33 ; lv2:symbol "agc_speed" ; lv2:name "Auto Gain Speed" ;
        lv2:default 6.0 ; lv2:minimum 0.0 ; lv2:maximum 20.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 34 ; lv2:symbol "ac_weight" ; lv2:name "Autocorrelation 2nd-Detector Mix" ;
        lv2:default 8.0 ; lv2:minimum 0.0 ; lv2:maximum 20.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 35 ; lv2:symbol "ac_clip" ; lv2:name "Time-Domain Center Clip" ;
        lv2:default 4.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;

    # ── AUTO LOWEST-NOTE RHYTHM BASS PLAYER ──────────────────────────────

    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 36 ; lv2:symbol "bass_level" ; lv2:name "Bass Level" ;
        lv2:default 0.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 37 ; lv2:symbol "bass_pattern" ; lv2:name "Bass Pattern" ;
        lv2:default 1.0 ; lv2:minimum 0.0 ; lv2:maximum 6.0 ;
        lv2:portProperty lv2:enumeration , lv2:integer ;
        lv2:scalePoint [ rdfs:label "0: Off" ; rdf:value 0 ] ;
        lv2:scalePoint [ rdfs:label "1: Drone Sustain" ; rdf:value 1 ] ;
        lv2:scalePoint [ rdfs:label "2: 8th Note Rock Pulse" ; rdf:value 2 ] ;
        lv2:scalePoint [ rdfs:label "3: Root-Octave Groove" ; rdf:value 3 ] ;
        lv2:scalePoint [ rdfs:label "4: Root-5th Country/Blues" ; rdf:value 4 ] ;
        lv2:scalePoint [ rdfs:label "5: Walking Bass" ; rdf:value 5 ] ;
        lv2:scalePoint [ rdfs:label "6: Dub / Reggae Drop" ; rdf:value 6 ] ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 38 ; lv2:symbol "bass_bpm" ; lv2:name "Bass Tempo (BPM)" ;
        lv2:default 120.0 ; lv2:minimum 40.0 ; lv2:maximum 240.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 39 ; lv2:symbol "bass_tap" ; lv2:name "Bass Tap Tempo" ;
        lv2:default 0.0 ; lv2:minimum 0.0 ; lv2:maximum 1.0 ;
        lv2:portProperty lv2:toggled ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 40 ; lv2:symbol "bass_tone" ; lv2:name "Bass Tone / Lowpass" ;
        lv2:default 5.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] .
"""

# ── CYBER STRING 9 ──────────────────────────────────────────────────────────
string9_ttl = """@prefix lv2:   <http://lv2plug.in/ns/lv2core#> .
@prefix rdf:   <http://www.w3.org/1999/02/22-rdf-syntax-ns#> .
@prefix rdfs:  <http://www.w3.org/2000/01/rdf-schema#> .
@prefix doap:  <http://usefulinc.com/ns/doap#> .
@prefix mod:   <http://moddevices.com/ns#> .

<http://cyber-audio.co.uk/plugins/cyber-string9>
    a lv2:Plugin , lv2:InstrumentPlugin ;
    doap:name "Cyber String 9" ;
    doap:license <http://opensource.org/licenses/mit> ;
    mod:brand "CyberAudio" ;
    mod:label "String 9" ;
    rdfs:comment "Vintage Solina string ensemble and symphonic string machine with triple BBD chorus, Note Selector harmonies, and rhythm bass player." ;

    lv2:port [
        a lv2:AudioPort , lv2:InputPort ;
        lv2:index 0 ; lv2:symbol "in_l" ; lv2:name "Guitar In L/Mono" ;
    ] , [
        a lv2:AudioPort , lv2:InputPort ;
        lv2:index 1 ; lv2:symbol "in_r" ; lv2:name "Guitar In R" ;
    ] , [
        a lv2:AudioPort , lv2:OutputPort ;
        lv2:index 2 ; lv2:symbol "out_l" ; lv2:name "String Out L" ;
    ] , [
        a lv2:AudioPort , lv2:OutputPort ;
        lv2:index 3 ; lv2:symbol "out_r" ; lv2:name "String Out R" ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 4 ; lv2:symbol "bypass" ; lv2:name "Bypass" ;
        lv2:default 1.0 ; lv2:minimum 0.0 ; lv2:maximum 1.0 ;
        lv2:portProperty lv2:toggled , lv2:integer ;
        lv2:designation lv2:enabled ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 5 ; lv2:symbol "third_mode" ; lv2:name "3rd Quality" ;
        lv2:default 0.0 ; lv2:minimum 0.0 ; lv2:maximum 2.0 ;
        lv2:portProperty lv2:enumeration , lv2:integer ;
        lv2:scalePoint [ rdfs:label "Auto (detect from guitar)" ; rdf:value 0 ] ;
        lv2:scalePoint [ rdfs:label "Force Minor 3rd" ; rdf:value 1 ] ;
        lv2:scalePoint [ rdfs:label "Force Major 3rd" ; rdf:value 2 ] ;

    # ── NOTE SELECTOR ─────────────────────────────────────────────────────

    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 6 ; lv2:symbol "ns_sub_oct" ; lv2:name "Sub Octave" ;
        lv2:default 0.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 7 ; lv2:symbol "ns_sub_fifth" ; lv2:name "Sub Fifth" ;
        lv2:default 0.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 8 ; lv2:symbol "ns_sub_third" ; lv2:name "Sub Third" ;
        lv2:default 0.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 9 ; lv2:symbol "ns_root" ; lv2:name "Root" ;
        lv2:default 8.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 10 ; lv2:symbol "ns_third" ; lv2:name "+3rd" ;
        lv2:default 7.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 11 ; lv2:symbol "ns_fifth" ; lv2:name "+5th" ;
        lv2:default 6.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 12 ; lv2:symbol "ns_oct" ; lv2:name "+Oct" ;
        lv2:default 5.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 13 ; lv2:symbol "ns_oct_third" ; lv2:name "+Oct+3rd" ;
        lv2:default 0.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 14 ; lv2:symbol "ns_oct_fifth" ; lv2:name "+Oct+5th" ;
        lv2:default 0.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 15 ; lv2:symbol "ns_dbl_oct" ; lv2:name "+2 Oct" ;
        lv2:default 0.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;

    # ── STRING ENSEMBLE & BBD CHORUS PARAMS ──────────────────────────────

    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 16 ; lv2:symbol "preset" ; lv2:name "String Ensemble Mode" ;
        lv2:default 0.0 ; lv2:minimum 0.0 ; lv2:maximum 4.0 ;
        lv2:portProperty lv2:enumeration , lv2:integer ;
        lv2:scalePoint [ rdfs:label "1: Solina String Ensemble" ; rdf:value 0 ] ;
        lv2:scalePoint [ rdfs:label "2: Symphonic Strings" ; rdf:value 1 ] ;
        lv2:scalePoint [ rdfs:label "3: Orchestral Pad" ; rdf:value 2 ] ;
        lv2:scalePoint [ rdfs:label "4: Freeze / Sustain" ; rdf:value 3 ] ;
        lv2:scalePoint [ rdfs:label "5: Rotary String" ; rdf:value 4 ] ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 17 ; lv2:symbol "ensemble_depth" ; lv2:name "Ensemble Chorus Depth" ;
        lv2:default 7.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 18 ; lv2:symbol "brightness" ; lv2:name "String Brightness / Tone" ;
        lv2:default 6.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 19 ; lv2:symbol "attack" ; lv2:name "Attack Time" ;
        lv2:default 4.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 20 ; lv2:symbol "sustain" ; lv2:name "Sustain" ;
        lv2:default 5.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 21 ; lv2:symbol "release" ; lv2:name "Release Slope" ;
        lv2:default 5.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 22 ; lv2:symbol "gate_sens" ; lv2:name "Gate Sens" ;
        lv2:default 5.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 23 ; lv2:symbol "dry" ; lv2:name "Dry Level" ;
        lv2:default 5.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 24 ; lv2:symbol "wet" ; lv2:name "String Level" ;
        lv2:default 8.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 25 ; lv2:symbol "output" ; lv2:name "Output Level" ;
        lv2:default 7.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] ,

    # ── KEY & SCALE DIATONIC LOCK ────────────────────────────────────────

    [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 26 ; lv2:symbol "key_root" ; lv2:name "Diatonic Key Lock" ;
        lv2:default 0 ; lv2:minimum 0 ; lv2:maximum 12 ;
        lv2:portProperty lv2:enumeration , lv2:integer ;
        lv2:scalePoint [ rdfs:label "Auto (Full Chromatic)" ; rdf:value 0 ] ;
        lv2:scalePoint [ rdfs:label "C" ; rdf:value 1 ] ;
        lv2:scalePoint [ rdfs:label "C#" ; rdf:value 2 ] ;
        lv2:scalePoint [ rdfs:label "D" ; rdf:value 3 ] ;
        lv2:scalePoint [ rdfs:label "D#" ; rdf:value 4 ] ;
        lv2:scalePoint [ rdfs:label "E" ; rdf:value 5 ] ;
        lv2:scalePoint [ rdfs:label "F" ; rdf:value 6 ] ;
        lv2:scalePoint [ rdfs:label "F#" ; rdf:value 7 ] ;
        lv2:scalePoint [ rdfs:label "G" ; rdf:value 8 ] ;
        lv2:scalePoint [ rdfs:label "G#" ; rdf:value 9 ] ;
        lv2:scalePoint [ rdfs:label "A" ; rdf:value 10 ] ;
        lv2:scalePoint [ rdfs:label "A#" ; rdf:value 11 ] ;
        lv2:scalePoint [ rdfs:label "B" ; rdf:value 12 ] ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 27 ; lv2:symbol "scale_mode" ; lv2:name "Scale Mode" ;
        lv2:default 0 ; lv2:minimum 0 ; lv2:maximum 2 ;
        lv2:portProperty lv2:enumeration , lv2:integer ;
        lv2:scalePoint [ rdfs:label "Auto" ; rdf:value 0 ] ;
        lv2:scalePoint [ rdfs:label "Major / Ionian" ; rdf:value 1 ] ;
        lv2:scalePoint [ rdfs:label "Minor / Aeolian" ; rdf:value 2 ] ;
    ] ,

    # ── DUAL PITCH DETECTOR CALIBRATION ──────────────────────────────────

    [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 28 ; lv2:symbol "track_sens" ; lv2:name "Detector Sensitivity" ;
        lv2:default 8.0 ; lv2:minimum 0.1 ; lv2:maximum 20.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 29 ; lv2:symbol "track_stab" ; lv2:name "Chord Stability / Hysteresis" ;
        lv2:default 6.0 ; lv2:minimum 0.1 ; lv2:maximum 20.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 30 ; lv2:symbol "debounce_speed" ; lv2:name "Debounce / Confirmation" ;
        lv2:default 2.0 ; lv2:minimum 1.0 ; lv2:maximum 16.0 ;
        lv2:portProperty lv2:integer ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 31 ; lv2:symbol "filter_q" ; lv2:name "Resonator Bandpass Q" ;
        lv2:default 7.0 ; lv2:minimum 1.5 ; lv2:maximum 30.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 32 ; lv2:symbol "bass_boost" ; lv2:name "Bass Weighting" ;
        lv2:default 6.0 ; lv2:minimum 0.0 ; lv2:maximum 20.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 33 ; lv2:symbol "agc_speed" ; lv2:name "Auto Gain Speed" ;
        lv2:default 6.0 ; lv2:minimum 0.0 ; lv2:maximum 20.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 34 ; lv2:symbol "ac_weight" ; lv2:name "Autocorrelation 2nd-Detector Mix" ;
        lv2:default 8.0 ; lv2:minimum 0.0 ; lv2:maximum 20.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 35 ; lv2:symbol "ac_clip" ; lv2:name "Time-Domain Center Clip" ;
        lv2:default 4.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;

    # ── AUTO LOWEST-NOTE RHYTHM BASS PLAYER ──────────────────────────────

    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 36 ; lv2:symbol "bass_level" ; lv2:name "Bass Level" ;
        lv2:default 0.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 37 ; lv2:symbol "bass_pattern" ; lv2:name "Bass Pattern" ;
        lv2:default 1.0 ; lv2:minimum 0.0 ; lv2:maximum 6.0 ;
        lv2:portProperty lv2:enumeration , lv2:integer ;
        lv2:scalePoint [ rdfs:label "0: Off" ; rdf:value 0 ] ;
        lv2:scalePoint [ rdfs:label "1: Drone Sustain" ; rdf:value 1 ] ;
        lv2:scalePoint [ rdfs:label "2: 8th Note Rock Pulse" ; rdf:value 2 ] ;
        lv2:scalePoint [ rdfs:label "3: Root-Octave Groove" ; rdf:value 3 ] ;
        lv2:scalePoint [ rdfs:label "4: Root-5th Country/Blues" ; rdf:value 4 ] ;
        lv2:scalePoint [ rdfs:label "5: Walking Bass" ; rdf:value 5 ] ;
        lv2:scalePoint [ rdfs:label "6: Dub / Reggae Drop" ; rdf:value 6 ] ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 38 ; lv2:symbol "bass_bpm" ; lv2:name "Bass Tempo (BPM)" ;
        lv2:default 120.0 ; lv2:minimum 40.0 ; lv2:maximum 240.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 39 ; lv2:symbol "bass_tap" ; lv2:name "Bass Tap Tempo" ;
        lv2:default 0.0 ; lv2:minimum 0.0 ; lv2:maximum 1.0 ;
        lv2:portProperty lv2:toggled ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 40 ; lv2:symbol "bass_tone" ; lv2:name "Bass Tone / Lowpass" ;
        lv2:default 5.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] .
"""

def generate_modgui_ttl(plugin_uri, pedal_label, icon_file, control_ports):
    port_items = []
    for idx, sym in enumerate(control_ports):
        if idx == 0:
            port_items.append(f"""        modgui:port [
            lv2:index {idx:2d} ; lv2:symbol "{sym}" ;
        ]""")
        else:
            port_items.append(f"""        [
            lv2:index {idx:2d} ; lv2:symbol "{sym}" ;
        ]""")
    ports_ttl = " , \n".join(port_items)
    
    return f"""@prefix modgui: <http://moddevices.com/ns/modgui#> .
@prefix lv2:    <http://lv2plug.in/ns/lv2core#> .

<{plugin_uri}>
    modgui:gui [
        modgui:resourcesDirectory <modgui> ;
        modgui:iconTemplate        <{icon_file}> ;
        modgui:stylesheet          <modgui/stylesheet.css> ;
        modgui:javascript          <modgui/script.js> ;
        modgui:screenshot          <modgui/screenshot.png> ;
        modgui:thumbnail           <modgui/thumbnail.png> ;
        modgui:brand "CyberAudio" ;
        modgui:label "{pedal_label}" ;
        modgui:model "boxy" ;
        modgui:panel "custom" ;
        modgui:color "black" ;
        modgui:knob "black" ;

{ports_ttl}
    ] .
"""

# Control port symbols for modgui (indices 0 to 35 for 36 control ports)
mel9_controls = [
    "third_mode",
    "ns_sub_oct", "ns_sub_fifth", "ns_sub_third", "ns_root", "ns_third", "ns_fifth", "ns_oct", "ns_oct_third", "ns_oct_fifth", "ns_dbl_oct",
    "preset", "attack", "sustain", "release", "flutter", "warmth", "gate_sens", "dry", "wet", "output",
    "key_root", "scale_mode",
    "track_sens", "track_stab", "debounce_speed", "filter_q", "bass_boost", "agc_speed", "ac_weight", "ac_clip",
    "bass_level", "bass_pattern", "bass_bpm", "bass_tap", "bass_tone"
]

synth9_controls = [
    "third_mode",
    "ns_sub_oct", "ns_sub_fifth", "ns_sub_third", "ns_root", "ns_third", "ns_fifth", "ns_oct", "ns_oct_third", "ns_oct_fifth", "ns_dbl_oct",
    "preset", "cutoff", "resonance", "env_mod", "attack", "sustain", "gate_sens", "dry", "wet", "output",
    "key_root", "scale_mode",
    "track_sens", "track_stab", "debounce_speed", "filter_q", "bass_boost", "agc_speed", "ac_weight", "ac_clip",
    "bass_level", "bass_pattern", "bass_bpm", "bass_tap", "bass_tone"
]

string9_controls = [
    "third_mode",
    "ns_sub_oct", "ns_sub_fifth", "ns_sub_third", "ns_root", "ns_third", "ns_fifth", "ns_oct", "ns_oct_third", "ns_oct_fifth", "ns_dbl_oct",
    "preset", "ensemble_depth", "brightness", "attack", "sustain", "release", "gate_sens", "dry", "wet", "output",
    "key_root", "scale_mode",
    "track_sens", "track_stab", "debounce_speed", "filter_q", "bass_boost", "agc_speed", "ac_weight", "ac_clip",
    "bass_level", "bass_pattern", "bass_bpm", "bass_tap", "bass_tone"
]

bass9_ttl = """@prefix lv2:   <http://lv2plug.in/ns/lv2core#> .
@prefix rdf:   <http://www.w3.org/1999/02/22-rdf-syntax-ns#> .
@prefix rdfs:  <http://www.w3.org/2000/01/rdf-schema#> .
@prefix doap:  <http://usefulinc.com/ns/doap#> .
@prefix mod:   <http://moddevices.com/ns#> .

<http://cyber-audio.co.uk/plugins/cyber-bass9>
    a lv2:Plugin , lv2:InstrumentPlugin ;
    doap:name "Cyber Bass 9" ;
    doap:license <http://opensource.org/licenses/mit> ;
    mod:brand "CyberAudio" ;
    mod:label "Bass 9" ;
    rdfs:comment "Vintage Bass Machine with 9 iconic bass emulations (Precision, Longhorn, Fretless, Synth, Virtual, Bowed, Split Bass, 3:03, Flip-Flop), Note Selector chord harmony, and rhythm bass player." ;

    lv2:port [
        a lv2:AudioPort , lv2:InputPort ;
        lv2:index 0 ; lv2:symbol "in_l" ; lv2:name "Guitar In L/Mono" ;
    ] , [
        a lv2:AudioPort , lv2:InputPort ;
        lv2:index 1 ; lv2:symbol "in_r" ; lv2:name "Guitar In R" ;
    ] , [
        a lv2:AudioPort , lv2:OutputPort ;
        lv2:index 2 ; lv2:symbol "out_l" ; lv2:name "Out L" ;
    ] , [
        a lv2:AudioPort , lv2:OutputPort ;
        lv2:index 3 ; lv2:symbol "out_r" ; lv2:name "Out R" ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 4 ; lv2:symbol "bypass" ; lv2:name "Bypass" ;
        lv2:default 1.0 ; lv2:minimum 0.0 ; lv2:maximum 1.0 ;
        lv2:portProperty lv2:toggled , lv2:integer ;
        lv2:designation lv2:enabled ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 5 ; lv2:symbol "third_mode" ; lv2:name "3rd Quality" ;
        lv2:default 0.0 ; lv2:minimum 0.0 ; lv2:maximum 2.0 ;
        lv2:portProperty lv2:enumeration , lv2:integer ;
        lv2:scalePoint [ rdfs:label "Auto (from Input)" ; rdf:value 0 ] ;
        lv2:scalePoint [ rdfs:label "Force Minor 3rd" ; rdf:value 1 ] ;
        lv2:scalePoint [ rdfs:label "Force Major 3rd" ; rdf:value 2 ] ;
    ] ,

    # ── NOTE SELECTOR HARMONIC VOICING (10 SLIDERS) ──────────────────────

    [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 6 ; lv2:symbol "ns_sub_oct" ; lv2:name "-Oct (-12st)" ;
        lv2:default 0.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 7 ; lv2:symbol "ns_sub_fifth" ; lv2:name "-5th (-7st)" ;
        lv2:default 0.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 8 ; lv2:symbol "ns_sub_third" ; lv2:name "-3rd (auto M/m)" ;
        lv2:default 0.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 9 ; lv2:symbol "ns_root" ; lv2:name "Root (1st)" ;
        lv2:default 10.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 10 ; lv2:symbol "ns_third" ; lv2:name "+3rd (auto M/m)" ;
        lv2:default 0.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 11 ; lv2:symbol "ns_fifth" ; lv2:name "+5th (+7st)" ;
        lv2:default 0.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 12 ; lv2:symbol "ns_oct" ; lv2:name "+Oct (+12st)" ;
        lv2:default 0.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 13 ; lv2:symbol "ns_oct_third" ; lv2:name "+Oct+3rd" ;
        lv2:default 0.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 14 ; lv2:symbol "ns_oct_fifth" ; lv2:name "+Oct+5th" ;
        lv2:default 0.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 15 ; lv2:symbol "ns_dbl_oct" ; lv2:name "+2 Oct" ;
        lv2:default 0.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] ,

    # ── BASS 9 CIRCUIT & EMULATION PARAMS ─────────────────────────────────

    [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 16 ; lv2:symbol "preset" ; lv2:name "Bass Preset" ;
        lv2:default 0.0 ; lv2:minimum 0.0 ; lv2:maximum 8.0 ;
        lv2:portProperty lv2:enumeration , lv2:integer ;
        lv2:scalePoint [ rdfs:label "1: Precision" ; rdf:value 0 ] ;
        lv2:scalePoint [ rdfs:label "2: Longhorn" ; rdf:value 1 ] ;
        lv2:scalePoint [ rdfs:label "3: Fretless" ; rdf:value 2 ] ;
        lv2:scalePoint [ rdfs:label "4: Synth (Taurus)" ; rdf:value 3 ] ;
        lv2:scalePoint [ rdfs:label "5: Virtual" ; rdf:value 4 ] ;
        lv2:scalePoint [ rdfs:label "6: Bowed" ; rdf:value 5 ] ;
        lv2:scalePoint [ rdfs:label "7: Split Bass" ; rdf:value 6 ] ;
        lv2:scalePoint [ rdfs:label "8: 3:03" ; rdf:value 7 ] ;
        lv2:scalePoint [ rdfs:label "9: Flip-Flop" ; rdf:value 8 ] ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 17 ; lv2:symbol "ctrl1" ; lv2:name "Tone / Attack / Param 1" ;
        lv2:default 5.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 18 ; lv2:symbol "ctrl2" ; lv2:name "Filter / Sub / Param 2" ;
        lv2:default 5.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 19 ; lv2:symbol "attack" ; lv2:name "Attack Time" ;
        lv2:default 2.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 20 ; lv2:symbol "sustain" ; lv2:name "Sustain" ;
        lv2:default 5.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 21 ; lv2:symbol "release" ; lv2:name "Release Slope" ;
        lv2:default 5.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 22 ; lv2:symbol "gate_sens" ; lv2:name "Gate Sens" ;
        lv2:default 5.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 23 ; lv2:symbol "dry" ; lv2:name "Dry Level" ;
        lv2:default 5.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 24 ; lv2:symbol "wet" ; lv2:name "Bass Vol" ;
        lv2:default 8.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 25 ; lv2:symbol "output" ; lv2:name "Output Level" ;
        lv2:default 7.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] ,

    # ── KEY & SCALE DIATONIC LOCK ────────────────────────────────────────

    [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 26 ; lv2:symbol "key_root" ; lv2:name "Diatonic Key Lock" ;
        lv2:default 0 ; lv2:minimum 0 ; lv2:maximum 12 ;
        lv2:portProperty lv2:enumeration , lv2:integer ;
        lv2:scalePoint [ rdfs:label "Auto (Full Chromatic)" ; rdf:value 0 ] ;
        lv2:scalePoint [ rdfs:label "C" ; rdf:value 1 ] ;
        lv2:scalePoint [ rdfs:label "C#" ; rdf:value 2 ] ;
        lv2:scalePoint [ rdfs:label "D" ; rdf:value 3 ] ;
        lv2:scalePoint [ rdfs:label "D#" ; rdf:value 4 ] ;
        lv2:scalePoint [ rdfs:label "E" ; rdf:value 5 ] ;
        lv2:scalePoint [ rdfs:label "F" ; rdf:value 6 ] ;
        lv2:scalePoint [ rdfs:label "F#" ; rdf:value 7 ] ;
        lv2:scalePoint [ rdfs:label "G" ; rdf:value 8 ] ;
        lv2:scalePoint [ rdfs:label "G#" ; rdf:value 9 ] ;
        lv2:scalePoint [ rdfs:label "A" ; rdf:value 10 ] ;
        lv2:scalePoint [ rdfs:label "A#" ; rdf:value 11 ] ;
        lv2:scalePoint [ rdfs:label "B" ; rdf:value 12 ] ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 27 ; lv2:symbol "scale_mode" ; lv2:name "Scale Mode" ;
        lv2:default 0 ; lv2:minimum 0 ; lv2:maximum 2 ;
        lv2:portProperty lv2:enumeration , lv2:integer ;
        lv2:scalePoint [ rdfs:label "Auto" ; rdf:value 0 ] ;
        lv2:scalePoint [ rdfs:label "Major / Ionian" ; rdf:value 1 ] ;
        lv2:scalePoint [ rdfs:label "Minor / Aeolian" ; rdf:value 2 ] ;
    ] ,

    # ── DUAL PITCH DETECTOR CALIBRATION ──────────────────────────────────

    [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 28 ; lv2:symbol "track_sens" ; lv2:name "Detector Sensitivity" ;
        lv2:default 8.0 ; lv2:minimum 0.1 ; lv2:maximum 20.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 29 ; lv2:symbol "track_stab" ; lv2:name "Chord Stability / Hysteresis" ;
        lv2:default 6.0 ; lv2:minimum 0.1 ; lv2:maximum 20.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 30 ; lv2:symbol "debounce_speed" ; lv2:name "Debounce / Confirmation" ;
        lv2:default 2.0 ; lv2:minimum 1.0 ; lv2:maximum 16.0 ;
        lv2:portProperty lv2:integer ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 31 ; lv2:symbol "filter_q" ; lv2:name "Resonator Bandpass Q" ;
        lv2:default 7.0 ; lv2:minimum 1.5 ; lv2:maximum 30.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 32 ; lv2:symbol "bass_boost" ; lv2:name "Bass Weighting" ;
        lv2:default 6.0 ; lv2:minimum 0.0 ; lv2:maximum 20.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 33 ; lv2:symbol "agc_speed" ; lv2:name "Auto Gain Speed" ;
        lv2:default 6.0 ; lv2:minimum 0.0 ; lv2:maximum 20.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 34 ; lv2:symbol "ac_weight" ; lv2:name "Autocorrelation 2nd-Detector Mix" ;
        lv2:default 8.0 ; lv2:minimum 0.0 ; lv2:maximum 20.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 35 ; lv2:symbol "ac_clip" ; lv2:name "Time-Domain Center Clip" ;
        lv2:default 4.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] ,

    # ── AUTO LOWEST-NOTE RHYTHM BASS PLAYER ──────────────────────────────

    [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 36 ; lv2:symbol "bass_level" ; lv2:name "Bass Level" ;
        lv2:default 0.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 37 ; lv2:symbol "bass_pattern" ; lv2:name "Bass Pattern" ;
        lv2:default 1.0 ; lv2:minimum 0.0 ; lv2:maximum 6.0 ;
        lv2:portProperty lv2:enumeration , lv2:integer ;
        lv2:scalePoint [ rdfs:label "0: Off" ; rdf:value 0 ] ;
        lv2:scalePoint [ rdfs:label "1: Drone / Pedal Sustain" ; rdf:value 1 ] ;
        lv2:scalePoint [ rdfs:label "2: 8th Note Rock Pulse" ; rdf:value 2 ] ;
        lv2:scalePoint [ rdfs:label "3: Root-Octave Groove" ; rdf:value 3 ] ;
        lv2:scalePoint [ rdfs:label "4: Root-5th Country / Blues" ; rdf:value 4 ] ;
        lv2:scalePoint [ rdfs:label "5: Walking Bass" ; rdf:value 5 ] ;
        lv2:scalePoint [ rdfs:label "6: Dub / Reggae Drop" ; rdf:value 6 ] ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 38 ; lv2:symbol "bass_bpm" ; lv2:name "Bass Sequencer BPM" ;
        lv2:default 120.0 ; lv2:minimum 40.0 ; lv2:maximum 240.0 ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 39 ; lv2:symbol "bass_tap" ; lv2:name "Tap Tempo Trigger" ;
        lv2:default 0.0 ; lv2:minimum 0.0 ; lv2:maximum 1.0 ;
        lv2:portProperty lv2:toggled , lv2:integer ;
    ] , [
        a lv2:ControlPort , lv2:InputPort ;
        lv2:index 40 ; lv2:symbol "bass_tone" ; lv2:name "Bass Tone" ;
        lv2:default 5.0 ; lv2:minimum 0.0 ; lv2:maximum 10.0 ;
    ] .
"""

bass9_controls = [
    "third_mode",
    "ns_sub_oct", "ns_sub_fifth", "ns_sub_third", "ns_root", "ns_third", "ns_fifth", "ns_oct", "ns_oct_third", "ns_oct_fifth", "ns_dbl_oct",
    "preset", "ctrl1", "ctrl2", "attack", "sustain", "release", "gate_sens", "dry", "wet", "output",
    "key_root", "scale_mode",
    "track_sens", "track_stab", "debounce_speed", "filter_q", "bass_boost", "agc_speed", "ac_weight", "ac_clip",
    "bass_level", "bass_pattern", "bass_bpm", "bass_tap", "bass_tone"
]

with open('plugins/cyber-mel9.lv2/cyber_mel9.ttl', 'w', encoding='utf-8') as f:
    f.write(mel9_ttl)

with open('plugins/cyber-mel9.lv2/modgui.ttl', 'w', encoding='utf-8') as f:
    f.write(generate_modgui_ttl("http://cyber-audio.co.uk/plugins/cyber-mel9", "Mel 9", "modgui/icon-cyber-mel9.html", mel9_controls))

with open('plugins/cyber-synth9.lv2/cyber_synth9.ttl', 'w', encoding='utf-8') as f:
    f.write(synth9_ttl)

with open('plugins/cyber-synth9.lv2/modgui.ttl', 'w', encoding='utf-8') as f:
    f.write(generate_modgui_ttl("http://cyber-audio.co.uk/plugins/cyber-synth9", "Synth 9", "modgui/icon-cyber-synth9.html", synth9_controls))

with open('plugins/cyber-string9.lv2/cyber_string9.ttl', 'w', encoding='utf-8') as f:
    f.write(string9_ttl)

with open('plugins/cyber-string9.lv2/modgui.ttl', 'w', encoding='utf-8') as f:
    f.write(generate_modgui_ttl("http://cyber-audio.co.uk/plugins/cyber-string9", "String 9", "modgui/icon-cyber-string9.html", string9_controls))

with open('plugins/cyber-bass9.lv2/cyber_bass9.ttl', 'w', encoding='utf-8') as f:
    f.write(bass9_ttl)

with open('plugins/cyber-bass9.lv2/modgui.ttl', 'w', encoding='utf-8') as f:
    f.write(generate_modgui_ttl("http://cyber-audio.co.uk/plugins/cyber-bass9", "Bass 9", "modgui/icon-cyber-bass9.html", bass9_controls))

print('All 9-series TTL and modgui.ttl files created successfully!')
