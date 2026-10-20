import sys

with open('bricasti_options.html', 'r', encoding='utf-8') as f:
    opts = f.read().strip()

with open('mixer_pro_index.html', 'r', encoding='utf-8') as f:
    mixer = f.read()

target = '''                            <select onchange="reverbDspState.atlantis.irModel=this.value; drawIrWaveform();" style="background:#0b0d11; color:#f1f5f9; border:1px solid #374151; border-radius:4px; padding:4px 8px; font-size:0.75rem; font-weight:700;">
                                <option value="cathedral" ${a.irModel === 'cathedral' ? 'selected' : ''}>Gothic Stone Cathedral (4.8s)</option>
                                <option value="concert_hall" ${a.irModel === 'concert_hall' ? 'selected' : ''}>Symphony Concert Hall (2.4s)</option>
                                <option value="emt140" ${a.irModel === 'emt140' ? 'selected' : ''}>EMT-140 Classic Plate (3.1s)</option>
                                <option value="drum_room" ${a.irModel === 'drum_room' ? 'selected' : ''}>Live Oak Drum Room (0.8s)</option>
                                <option value="ambient_space" ${a.irModel === 'ambient_space' ? 'selected' : ''}>Cosmic Void Ambience (7.5s)</option>
                            </select>'''

replacement = '''                            <select onchange="reverbDspState.atlantis.irModel=this.value; drawIrWaveform();" style="background:#0b0d11; color:#f1f5f9; border:1px solid #374151; border-radius:4px; padding:4px 8px; font-size:0.75rem; font-weight:700; max-width:280px;">
''' + opts + '''
                            </select>'''

if target not in mixer:
    print('ERROR: target not found in mixer')
    sys.exit(1)

new_mixer = mixer.replace(target, replacement, 1)
with open('mixer_pro_index.html', 'w', encoding='utf-8') as f:
    f.write(new_mixer)

print('SUCCESS: spliced Bricasti IR options into mixer_pro_index.html')
