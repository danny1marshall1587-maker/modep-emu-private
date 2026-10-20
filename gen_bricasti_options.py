import os
import re

base = r'C:\Users\danny\Downloads\Samplicity - Bricasti IRs version 2023-10\Samplicity - Bricasti IRs version 2023-10, left-right files, 48 Khz'
categories = {'Halls': [], 'Plates': [], 'Rooms': [], 'Chambers': [], 'Ambiences': [], 'Spaces': []}

for root, dirs, files in os.walk(base):
    for f in sorted(files):
        if f.endswith(', 48K L.wav') or f.endswith(' L.wav'):
            name = f
            for suff in [', 48K L.wav', ' L.wav', '.wav']:
                if name.endswith(suff):
                    name = name[:-len(suff)]
            m = re.match(r'^([1-6])\s+([A-Za-z]+)\s+\d+\s+(.*)$', name)
            if m:
                cat_num, cat_name, title = m.groups()
                cat_key = [k for k in categories.keys() if k.lower().startswith(cat_name.lower()[:4])]
                if cat_key:
                    categories[cat_key[0]].append((name, title))

lines = []
lines.append('                                <optgroup label="Factory Acoustic Presets">')
lines.append('                                    <option value="cathedral" ${a.irModel === \'cathedral\' ? \'selected\' : \'\'}>Gothic Stone Cathedral (4.8s)</option>')
lines.append('                                    <option value="concert_hall" ${a.irModel === \'concert_hall\' ? \'selected\' : \'\'}>Symphony Concert Hall (2.4s)</option>')
lines.append('                                    <option value="emt140" ${a.irModel === \'emt140\' ? \'selected\' : \'\'}>EMT-140 Classic Plate (3.1s)</option>')
lines.append('                                    <option value="drum_room" ${a.irModel === \'drum_room\' ? \'selected\' : \'\'}>Live Oak Drum Room (0.8s)</option>')
lines.append('                                    <option value="ambient_space" ${a.irModel === \'ambient_space\' ? \'selected\' : \'\'}>Cosmic Void Ambience (7.5s)</option>')
lines.append('                                </optgroup>')

for cat in ['Halls', 'Plates', 'Rooms', 'Chambers', 'Ambiences', 'Spaces']:
    lines.append(f'                                <optgroup label="Bricasti M7 {cat}">')
    for ir_id, title in categories[cat]:
        safe_id = ir_id.replace('"', '&quot;')
        safe_title = title.replace('"', '&quot;')
        lines.append(f"                                    <option value=\"{safe_id}\" ${{a.irModel === '{safe_id}' ? 'selected' : ''}}>{safe_title}</option>")
    lines.append('                                </optgroup>')

out_str = '\n'.join(lines)
with open('bricasti_options.html', 'w', encoding='utf-8') as out:
    out.write(out_str)
print('Generated bricasti_options.html successfully with', len(lines), 'lines')
