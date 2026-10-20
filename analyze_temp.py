import numpy as np

with open('/var/modep/user-files/Audio Recordings/lv2record/mod_session003.wav', 'rb') as f:
    buf = f.read()

data_idx = buf.find(b'data')
size = int.from_bytes(buf[data_idx+4:data_idx+8], 'little')
audio = np.frombuffer(buf[data_idx+8:data_idx+8+size], dtype=np.float32)
sr = 48000

# Goertzel or FFT chroma
# Compute 12-semitone chroma profile every 0.25s
def get_chroma(chunk, sr):
    # compute FFT
    win = np.hanning(len(chunk))
    fft = np.abs(np.fft.rfft(chunk * win))
    freqs = np.fft.rfftfreq(len(chunk), 1.0/sr)
    
    chroma = np.zeros(12)
    # guitar range 80Hz - 800Hz
    for i, f in enumerate(freqs):
        if 80.0 <= f <= 800.0:
            midi = 69.0 + 12.0 * np.log2(f / 440.0)
            semi = int(round(midi)) % 12
            chroma[semi] += fft[i]
    return chroma

NOTES = ["C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"]

# Window = 0.5s, step = 0.5s
step = 0.5
win_len = int(0.5 * sr)
results = []
for t in np.arange(3.0, 50.0, step):
    idx = int(t * sr)
    chunk = audio[idx:idx+win_len]
    c = get_chroma(chunk, sr)
    if np.sum(c) > 0:
        c = c / np.max(c)
        # Root score: root + fifth
        scores = [c[s]*1.5 + c[(s+7)%12]*1.0 + max(c[(s+4)%12], c[(s+3)%12])*0.8 for s in range(12)]
        best = np.argmax(scores)
        results.append((t, NOTES[best], scores[best]))
    else:
        results.append((t, "-", 0))

# Print summary
for t, note, sc in results:
    if int(t * 10) % 10 in (0, 5): # every 0.5s
        print(f"t={t:5.1f}s: {note:>3} (sc={sc:.2f})")
