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

    # Read seconds 126 to 142
    f.seek(126 * sr * ch * 4, 1)
    raw = f.read(16 * sr * ch * 4)
    floats = struct.unpack("<" + str(len(raw)//4) + "f", raw)
    count = len(floats) // ch
    print("Floats count:", len(floats), "Frames count:", count)
    for s in range(16):
        sub_d = [abs(floats[(s*sr + i)*ch + 1]) for i in range(sr)]
        print("Chunk sec", 126+s, "Drum Max:", max(sub_d))

    hp_x1 = 0.0
    fastEnv = 0.0
    slowEnv = 0.0
    refractory = 0
    prevHit = False
    
    strumWindow = 0
    strumHoldSteps = 0
    singleStrumFired = False
    ringingSustain = False
    strumCountInBar = 0
    lastHitStep = -1
    currentStep = 0
    currentBar = 0
    clockPhase = 0.0
    clockInc = (120.0 / (60.0 * sr)) * 4.0
    
    notes_triggered = 0
    
    for i in range(count):
        sample = floats[i * ch]
        absS = abs(sample)
        
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
            strumWindow = int(0.350 * sr)
            if currentStep <= 1 and absS * 3.5 > 0.45:
                strumHoldSteps = 16
                singleStrumFired = False
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
                currentBar += 1
                strumCountInBar = 0
                lastHitStep = -1
                ringingSustain = False
                
            isStrumActive = (strumWindow > 0)
            isRinging = (strumHoldSteps > 0)
            # Default mode (strum_only = 0):
            shouldPlay = isStrumActive or isRinging
            
            # Check ring out branch:
            if ringingSustain or ((not isStrumActive) and isRinging):
                pass
            elif shouldPlay:
                notes_triggered += 1
                
        if i % sr == 0:
            sec = 126 + (i // sr)
            print("Sec: %3d | Notes: %2d | Ringing: %5s | StrumCount: %d | Window: %5d" % (
                sec, notes_triggered, str(ringingSustain), strumCountInBar, strumWindow
            ))
            notes_triggered = 0
