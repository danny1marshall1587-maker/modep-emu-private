import struct
import math

with open("/tmp/guitar_and_drums_session.wav", "rb") as f:
    header = f.read(12)
    # read fmt chunk
    sr = 48000
    ch = 3
    while True:
        chunk_hdr = f.read(8)
        if len(chunk_hdr) < 8:
            break
        chunk_id, chunk_sz = struct.unpack("<4sI", chunk_hdr)
        if chunk_id == b'fmt ':
            fmt_data = f.read(chunk_sz)
            wFormatTag, nChannels, nSamplesPerSec, nAvgBytesPerSec, nBlockAlign, wBitsPerSample = struct.unpack("<HHIIHH", fmt_data[:16])
            sr = nSamplesPerSec
            ch = nChannels
            print(f"Format: tag={wFormatTag}, ch={ch}, sr={sr}, bits={wBitsPerSample}")
        elif chunk_id == b'data':
            break
        else:
            f.seek(chunk_sz, 1)

    bytes_per_sec = sr * ch * 4
    sec = 0
    print(f"{'SEC':>3} | {'G_Peak':>6} {'G_RMS':>6} {'GUITAR':<15} | {'D_Peak':>6} {'D_RMS':>6} {'DRUMS':<15} | STATE")
    print("-" * 75)
    
    while True:
        raw = f.read(bytes_per_sec)
        if not raw or len(raw) < ch * 4:
            break
        num_floats = len(raw) // 4
        num_frames = num_floats // ch
        floats = struct.unpack(f"<{num_frames * ch}f", raw[:num_frames * ch * 4])
        
        g_max = 0.0
        g_sum = 0.0
        d_max = 0.0
        d_sum = 0.0
        for i in range(num_frames):
            g = abs(floats[i * ch])
            d = max(abs(floats[i * ch + 1]), abs(floats[i * ch + 2]))
            if g > g_max: g_max = g
            if d > d_max: d_max = d
            g_sum += g * g
            d_sum += d * d
        g_rms = math.sqrt(g_sum / num_frames)
        d_rms = math.sqrt(d_sum / num_frames)
        
        bar_g = "#" * int(min(15, g_max * 30))
        bar_d = "*" * int(min(15, d_max * 30))
        note = "IDLE"
        if g_max > 0.04 and d_rms < 0.002:
            note = "MISS: Guitar playing, Drums SILENT"
        elif g_max < 0.01 and d_rms > 0.01:
            note = "MISS: Drums runaway on silence"
        elif g_max > 0.04 and d_rms > 0.01:
            note = "GROOVING TOGETHER"
        elif g_max > 0.02 and d_max > 0.02 and d_rms < 0.01:
            note = "RING OUT / ACCENT HIT"
        elif g_max > 0.01:
            note = "Guitar sparse"
            
        if g_max > 0.005 or d_max > 0.005 or sec % 10 == 0:
            print(f"{sec:3d} | {g_max:6.3f} {g_rms:6.4f} [{bar_g:<15}] | {d_max:6.3f} {d_rms:6.4f} [{bar_d:<15}] | {note}")
        sec += 1
