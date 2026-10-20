import struct

with open("/tmp/guitar_and_drums_session.wav", "rb") as f:
    f.seek(12)
    sr = 48000
    ch = 3
    while True:
        hdr = f.read(8)
        if not hdr:
            break
        cid, csz = struct.unpack("<4sI", hdr)
        if cid == b"fmt ":
            data = f.read(csz)
            _, nch, nsr, _, _, _ = struct.unpack("<HHIIHH", data[:16])
            sr = nsr
            ch = nch
        elif cid == b"data":
            break
        else:
            f.seek(csz, 1)

    # Seek to sec 128
    data_start = f.tell()
    f.seek(data_start + 128 * sr * ch * 4)
    # Read 4 seconds
    raw = f.read(4 * sr * ch * 4)
    floats = struct.unpack("<" + str(len(raw)//4) + "f", raw)
    count = len(floats) // ch

    # Test BroadbandTransientExtractor
    hp_x1 = 0.0
    fastEnv = 0.0
    slowEnv = 0.0
    refractory = 0
    prevHit = False
    
    # Also track strumWindowSamplesRemaining
    strumWindow = 0
    strumWindowLen = int(0.350 * sr)
    strumHoldSteps = 0
    ringingSustain = False
    currentStep = 0
    strumCountInBar = 0
    lastHitStep = -1
    
    clockPhase = 0.0
    clockInc = (120.0 / (60.0 * sr)) * 4.0

    print("Time   | G_Max  | HitTrig | StrumWin | Ringing | StrumOnlyPlay")
    print("-" * 65)

    for i in range(count):
        sample = floats[i * ch]
        absS = abs(sample)
        
        # Transient
        click = sample - hp_x1
        hp_x1 = sample
        absC = abs(click)
        fastEnv = fastEnv * 0.40 + absC * 0.60
        slowEnv = slowEnv * 0.990 + absC * 0.010
        sensFactor = 1.35 + (10.0 - 5.0) * 0.25
        isHitRaw = (fastEnv > slowEnv * sensFactor) and (fastEnv > 0.0008)
        
        isHitTrigger = False
        if refractory > 0:
            refractory -= 1
        elif isHitRaw and not prevHit:
            isHitTrigger = True
            refractory = int(0.040 * sr)
        prevHit = isHitRaw
        
        if isHitTrigger:
            if currentStep != lastHitStep:
                strumCountInBar += 1
                lastHitStep = currentStep
            strumWindow = strumWindowLen
            if currentStep <= 1 and absS * 3.5 > 0.45:
                strumHoldSteps = 16
                ringingSustain = False
                
        if strumWindow > 0:
            strumWindow -= 1
            
        clockPhase += clockInc
        if clockPhase >= 1.0:
            clockPhase -= 1.0
            prevStep = currentStep
            currentStep = (currentStep + 1) % 16
            if strumHoldSteps > 0:
                strumHoldSteps -= 1
            if strumCountInBar <= 2 and currentStep >= 4 and strumWindow <= 0:
                ringingSustain = True
            if currentStep == 0 and prevStep == 15:
                strumCountInBar = 0
                lastHitStep = -1
                ringingSustain = False
                
        # Sample every 100ms
        if i % (sr // 10) == 0:
            t = 128.0 + (i / sr)
            sub = floats[max(0, i-sr//10)*ch : (i+1)*ch : ch]
            g_max_100ms = max([abs(x) for x in sub]) if sub else 0.0
            isStrumActive = (strumWindow > 0)
            shouldPlay = isStrumActive
            print("%6.2fs | %6.3f | %-7s | %8d | %-7s | %s" % (
                t, g_max_100ms, str(isHitTrigger), strumWindow, str(ringingSustain), str(shouldPlay)
            ))
