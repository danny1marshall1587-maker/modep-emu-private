// ============================================================================
// STAGE 7 & 8: FULLSCREEN VINTAGE TASCAM / 2" ANALOG MULTITRACK TAPE MACHINE
// 8-Track Bank Paging (Lenovo Tab M11 Fit), Multi-Finger Red Faders & Rec Lock
// ============================================================================
(function(window) {
    'use strict';

    const DEFAULT_TRACKS = [];
    const chNames = ['Lead Vox', 'Ac. Gtr', 'Snare/El', 'Kick', 'Bass DI', 'Keys L', 'Keys R', 'Tracks'];
    const fxNames = ['Plate Rev L', 'Plate Rev R', 'Tape Dly L', 'Tape Dly R'];

    // Tracks 0..7: Ch 1-8 Processed Outs | Tracks 8..11: FX 1-4 Returns | Tracks 12..19: Ch 1-8 Post-Preamp (1176+HPF) Direct Safety Tracks
    for (let i = 0; i < 8; i++) {
        DEFAULT_TRACKS.push({ idx: i, srcCh: i, isDirect: false, isFx: false, name: `CH ${i + 1} ${chNames[i]}`, badge: `POST-DSP ${i + 1}`, vol: 100, pan: 0, drive: 1.5, mute: false, solo: false, rec: true, vu: -24.0, clip: 0 });
    }
    for (let f = 0; f < 4; f++) {
        DEFAULT_TRACKS.push({ idx: 8 + f, srcCh: f, isDirect: false, isFx: true, name: `FX ${f + 1} ${fxNames[f]}`, badge: `FX RET ${f + 1}`, vol: 100, pan: (f % 2 === 0 ? -45 : 45), drive: 1.0, mute: false, solo: false, rec: true, vu: -24.0, clip: 0 });
    }
    for (let i = 0; i < 8; i++) {
        DEFAULT_TRACKS.push({ idx: 12 + i, srcCh: i, isDirect: true, isFx: false, name: `CH ${i + 1} ${chNames[i]} (1176+HPF)`, badge: `POST-PRE ${i + 1}`, vol: 100, pan: 0, drive: 0.0, mute: true, solo: false, rec: true, vu: -24.0, clip: 0 });
    }

    window.tapeState = {
        transport: 0, sec: 0.0, ips: 15, emuEnabled: true, printWav: false,
        songName: 'Song_01_Live_Take',
        baseDir: '/var/modep/recordings', freeGb: 16.0,
        drives: [{ label: 'SD Card (/var/modep/recordings)', path: '/var/modep/recordings', free: 16.0 }],
        savedSongs: ['Song_01_Live_Take', 'Song_02_Acoustic_Set', 'Song_03_Full_Band_Jam', 'Song_04_Encore'],
        subPage: 'main', // 'main' (Ch 1-8) | 'fx' (FX 9-12 & Dubs) | 'direct' (Direct 1-8) | 'outputs'
        bankPage: 0,
        recStopConfirmUntil: 0,
        addSourceCh: 0, masterVol: 100, monSends: [0, 0, 0, 0, 0, 0], tracks: DEFAULT_TRACKS
    };

    function wsSend(payload) {
        if (window.meterWs && window.meterWs.readyState === WebSocket.OPEN) window.meterWs.send(JSON.stringify(payload));
    }
    function formatTapeTime(sec) {
        const s = Math.max(0, Number(sec) || 0);
        const hrs = Math.floor(s / 3600);
        const mins = Math.floor((s % 3600) / 60);
        const secs = Math.floor(s % 60);
        const tenths = Math.floor((s - Math.floor(s)) * 10);
        return `${String(hrs).padStart(2, '0')}:${String(mins).padStart(2, '0')}:${String(secs).padStart(2, '0')}.${tenths}`;
    }

    let _lastTapeSyncPerf = performance.now();
    let _lastTapeTransportPerf = 0;

    // 60FPS smooth tape timecode counter loop
    function tickTapeCounter60Fps() {
        const ts = window.tapeState;
        if (ts) {
            const now = performance.now();
            const dt = Math.max(0, (now - _lastTapeSyncPerf) / 1000.0);
            const displaySec = (ts.transport === 1 || ts.transport === 2)
                ? (ts.sec + Math.min(dt, 0.25))
                : ts.sec;
            const tcEl = document.getElementById('tape-timecode-val');
            if (tcEl) tcEl.textContent = formatTapeTime(displaySec);
        }
        requestAnimationFrame(tickTapeCounter60Fps);
    }
    requestAnimationFrame(tickTapeCounter60Fps);

    function updateHeaderTransportButtons() {
        const st = window.tapeState.transport;
        const bRec = document.getElementById('btn-reaper-rec'), bStop = document.getElementById('btn-reaper-stop'), bPlay = document.getElementById('btn-reaper-play');
        if (bRec) { bRec.style.background = (st === 2) ? '#dc2626' : ''; bRec.style.color = (st === 2) ? '#fff' : ''; bRec.style.boxShadow = (st === 2) ? '0 0 12px rgba(239,68,68,0.85)' : ''; }
        if (bPlay) { bPlay.style.background = (st === 1) ? '#059669' : ''; bPlay.style.color = (st === 1) ? '#fff' : ''; bPlay.style.boxShadow = (st === 1) ? '0 0 12px rgba(16,185,129,0.85)' : ''; }
        if (bStop) bStop.style.borderColor = (st === 0) ? '#94a3b8' : '';
    }

    // Stage 8 Recording Safety Lock: Require 2-tap confirmation when stopping active recording
    window.sendTapeTransport = function(cmd) {
        const ts = window.tapeState, now = performance.now();
        const statusEl = document.getElementById('tape-deck-status-badge'), stopBtn = document.getElementById('tape-modal-stop-btn');
        if (ts.transport === 2 && (cmd === 'stop' || cmd === 'rtz') && now > ts.recStopConfirmUntil) {
            ts.recStopConfirmUntil = now + 2600;
            if (statusEl) { statusEl.textContent = '⚠️ REC LOCK: TAP STOP AGAIN TO CONFIRM!'; statusEl.style.color = '#fbbf24'; }
            if (stopBtn) { stopBtn.style.background = '#d97706'; stopBtn.style.color = '#000'; stopBtn.textContent = '⚠️ CONFIRM STOP'; }
            return;
        }
        ts.recStopConfirmUntil = 0;
        _lastTapeTransportPerf = now;
        _lastTapeSyncPerf = now;
        if (cmd === 'rec') ts.transport = 2;
        else if (cmd === 'play') ts.transport = 1;
        else if (cmd === 'stop') ts.transport = 0;
        else if (cmd === 'rtz') ts.sec = 0.0;
        wsSend({ action: 'tape_transport', command: cmd });
        updateHeaderTransportButtons();
        window.refreshTapeModalView();
        if (stopBtn) { stopBtn.style.background = ''; stopBtn.style.color = ''; stopBtn.textContent = '■ STOP'; }
        if (statusEl) {
            statusEl.textContent = cmd === 'rec' ? '● STREAMING WAV STEMS TO DISK' : (cmd === 'play' ? '▶ PLAY MONITOR MIX' : '■ STOPPED / STEMS SAVED');
            statusEl.style.color = cmd === 'rec' ? '#f87171' : (cmd === 'play' ? '#34d399' : '#fbbf24');
        }
    };

    window.reaperTransport = {
        record: () => window.sendTapeTransport('rec'),
        stop:   () => window.sendTapeTransport('stop'),
        play:   () => window.sendTapeTransport('play'),
        marker: () => window.sendTapeTransport('rtz')
    };
    window.reaperPlay = window.reaperTransport.play;
    window.reaperStop = window.reaperTransport.stop;
    window.reaperRecord = window.reaperTransport.record;

    window.setTapeStoragePath = function(modeOrPath) {
        const ts = window.tapeState;
        if (modeOrPath === 'auto') { wsSend({ action: 'tape_set_storage', auto: 1 }); return; }
        const inp = document.getElementById('tape-storage-path-input');
        const chosen = (typeof modeOrPath === 'string' && modeOrPath.startsWith('/')) ? modeOrPath : (inp ? inp.value.trim() : ts.baseDir);
        if (!chosen || !chosen.startsWith('/')) return;
        ts.baseDir = chosen;
        if (inp) inp.value = chosen;
        wsSend({ action: 'tape_set_storage', path: chosen });
        const badge = document.getElementById('tape-deck-status-badge');
        if (badge) { badge.textContent = `💾 PATH → ${chosen}/${ts.songName}`; badge.style.color = '#38bdf8'; }
    };

    window.setTapeMachineParam = function(key, val, monIdx) {
        const ts = window.tapeState;
        if (key === 'ips') { ts.ips = (Number(val) === 30) ? 30 : 15; wsSend({ action: 'tape_set_machine', ips: ts.ips }); }
        else if (key === 'emuEnabled') { ts.emuEnabled = !!val; wsSend({ action: 'tape_set_machine', emuEnabled: ts.emuEnabled ? 1 : 0 }); }
        else if (key === 'printWav') { ts.printWav = !!val; wsSend({ action: 'tape_set_machine', printWav: ts.printWav ? 1 : 0 }); }
        else if (key === 'masterVol') {
            ts.masterVol = Math.max(0, Math.min(150, Math.round(Number(val))));
            wsSend({ action: 'tape_set_machine', masterVol: ts.masterVol / 100.0 });
            const el = document.getElementById('tape-master-vol-val'), cap = document.getElementById('tape-out-cap-master');
            if (el) el.textContent = `${ts.masterVol}%`;
            if (cap) cap.style.bottom = `${(ts.masterVol / 150.0) * 82 + 9}%`;
            return;
        } else if (key === 'monSend') {
            const m = Number(monIdx) || 0;
            ts.monSends[m] = Math.max(0, Math.min(150, Math.round(Number(val))));
            wsSend({ action: 'tape_set_machine', monIdx: m, monSend: ts.monSends[m] / 100.0 });
            const el = document.getElementById(`tape-mon-send-val-${m}`), cap = document.getElementById(`tape-out-cap-mon-${m}`);
            if (el) el.textContent = `${ts.monSends[m]}%`;
            if (cap) cap.style.bottom = `${(ts.monSends[m] / 150.0) * 82 + 9}%`;
            return;
        }
        window.refreshTapeModalView();
    };

    window.setTapeTrackParam = function(tIdx, key, val) {
        const trk = window.tapeState.tracks[tIdx];
        if (!trk) return;
        if (key === 'vol') {
            trk.vol = Math.max(0, Math.min(150, Math.round(Number(val))));
            wsSend({ action: 'tape_set_track', track: tIdx, vol: trk.vol / 100.0 });
            const lbl = document.getElementById(`tape-trk-vol-val-${tIdx}`), cap = document.getElementById(`tape-red-cap-${tIdx}`);
            if (lbl) lbl.textContent = `${trk.vol}%`;
            if (cap) cap.style.bottom = `${(trk.vol / 150.0) * 82 + 9}%`;
        } else if (key === 'pan') {
            trk.pan = Math.max(-100, Math.min(100, Math.round(Number(val) / 5) * 5));
            wsSend({ action: 'tape_set_track', track: tIdx, pan: trk.pan / 100.0 });
            const lbl = document.getElementById(`tape-trk-pan-val-${tIdx}`);
            if (lbl) lbl.textContent = trk.pan === 0 ? 'C' : (trk.pan < 0 ? `L${Math.abs(trk.pan)}` : `R${trk.pan}`);
        } else if (key === 'drive') {
            trk.drive = Math.max(-6, Math.min(18, Math.round(Number(val) * 2) / 2));
            wsSend({ action: 'tape_set_track', track: tIdx, drive: trk.drive });
            const lbl = document.getElementById(`tape-trk-drv-val-${tIdx}`);
            if (lbl) lbl.textContent = `${trk.drive >= 0 ? '+' : ''}${trk.drive.toFixed(1)}dB`;
        } else if (key === 'mute' || key === 'solo' || key === 'rec') {
            trk[key] = !trk[key];
            wsSend({ action: 'tape_set_track', track: tIdx, [key]: trk[key] ? 1 : 0 });
            window.refreshTapeModalView();
        }
    };

    // Multi-finger simultaneous Red TASCAM Faders + Double-Tap to 100% Unity
    const _tapeTapMap = {};
    window.startTapeFaderDrag = function(ev, targetType, idx) {
        if (ev && ev.preventDefault) ev.preventDefault();
        const well = ev.currentTarget;
        if (!well) return;
        const key = `${targetType}-${idx}`, now = performance.now();
        if (now - (_tapeTapMap[key] || 0) < 320) {
            _tapeTapMap[key] = 0;
            if (targetType === 'track') window.setTapeTrackParam(idx, 'vol', 100);
            else if (targetType === 'master') window.setTapeMachineParam('masterVol', 100);
            else if (targetType === 'mon') window.setTapeMachineParam('monSend', 100, idx);
            return;
        }
        _tapeTapMap[key] = now;
        const pId = ev.pointerId;
        if (well.setPointerCapture && pId !== undefined) { try { well.setPointerCapture(pId); } catch(e) {} }
        const updateFromY = (clientY) => {
            const rect = well.getBoundingClientRect();
            const val = Math.round((1.0 - Math.max(0, Math.min(1, (clientY - rect.top - 10) / Math.max(20, rect.height - 20)))) * 150);
            if (targetType === 'track') window.setTapeTrackParam(idx, 'vol', val);
            else if (targetType === 'master') window.setTapeMachineParam('masterVol', val);
            else if (targetType === 'mon') window.setTapeMachineParam('monSend', val, idx);
        };
        updateFromY(ev.clientY);
        const onMove = (me) => { if (pId === undefined || me.pointerId === pId) updateFromY(me.clientY); };
        const onUp = (ue) => {
            if (pId === undefined || ue.pointerId === pId) {
                window.removeEventListener('pointermove', onMove);
                window.removeEventListener('pointerup', onUp);
                window.removeEventListener('pointercancel', onUp);
            }
        };
        window.addEventListener('pointermove', onMove);
        window.addEventListener('pointerup', onUp);
        window.addEventListener('pointercancel', onUp);
    };

    window.switchTapeSubPage = function(page, bankIdx) {
        window.tapeState.subPage = page;
        window.tapeState.bankPage = bankIdx || 0;
        window.refreshTapeModalView();
    };

    window.addPairedTapeTrack = function() {
        const ts = window.tapeState, srcEl = document.getElementById('tape-add-src-select');
        const srcCh = srcEl ? Number(srcEl.value) : ts.addSourceCh;
        ts.addSourceCh = srcCh;
        if (ts.tracks.length + 2 > 32) return;
        const pIdx = ts.tracks.length, dIdx = ts.tracks.length + 1, takeNum = Math.floor((pIdx - 20) / 2) + 2;
        ts.tracks.push({ idx: pIdx, srcCh: srcCh, isDirect: false, isFx: false, isDub: true, name: `CH ${srcCh + 1} Dub #${takeNum}`, badge: `POST-DSP ${srcCh + 1}`, vol: 100, pan: 0, drive: 1.5, mute: false, solo: false, rec: true, vu: -24.0, clip: 0 });
        ts.tracks.push({ idx: dIdx, srcCh: srcCh, isDirect: true, isFx: false, isDub: true, name: `CH ${srcCh + 1} Dub #${takeNum} (1176+HPF)`, badge: `POST-PRE ${srcCh + 1}`, vol: 100, pan: 0, drive: 0.0, mute: true, solo: false, rec: true, vu: -24.0, clip: 0 });
        wsSend({ action: 'tape_add_track', sourceCh: srcCh });
        ts.subPage = 'fx';
        window.refreshTapeModalView();
    };

    window.handleTapeSongOp = function(op) {
        const ts = window.tapeState, inp = document.getElementById('tape-song-name-input');
        if (ts.transport === 2 && op === 'new') { window.sendTapeTransport('stop'); return; }
        if (inp && inp.value.trim()) {
            ts.songName = inp.value.trim().replace(/[^a-zA-Z0-9_-]/g, '_');
            if (!ts.savedSongs.includes(ts.songName)) ts.savedSongs.push(ts.songName);
        }
        if (op === 'new') {
            ts.songName = `Song_0${ts.savedSongs.length + 1}_Live_Take`;
            if (!ts.savedSongs.includes(ts.songName)) ts.savedSongs.push(ts.songName);
            ts.sec = 0.0; ts.transport = 0;
            _lastTapeSyncPerf = performance.now();
        }
        wsSend({ action: 'tape_song_op', op: op, song: ts.songName });
        window.refreshTapeModalView();
        const badge = document.getElementById('tape-deck-status-badge');
        if (badge) {
            badge.textContent = op === 'bounce' ? `🔥 BOUNCED ${ts.tracks.length} STEMS → ${ts.baseDir}/${ts.songName}` : `📁 ${ts.baseDir}/${ts.songName}`;
            badge.style.color = '#34d399';
        }
    };

    window.updateTapeTelemetry = function(tapeData) {
        if (!tapeData) return;
        const ts = window.tapeState;
        const now = performance.now();
        // Prevent stale WS packets within 120ms of a local transport tap from reverting state
        if (now - _lastTapeTransportPerf > 120) {
            const prevSt = ts.transport;
            ts.transport = Number(tapeData.st) || 0;
            if (prevSt !== ts.transport) {
                updateHeaderTransportButtons();
                window.refreshTapeModalView();
            }
        }
        const incomingSec = Number(tapeData.sec) || 0.0;
        // Avoid backward micro-jitter when interpolating at 60fps during active transport
        if (ts.transport === 0 || incomingSec === 0.0 || Math.abs(incomingSec - ts.sec) > 0.01) {
            ts.sec = incomingSec;
            _lastTapeSyncPerf = now;
        }
        if (tapeData.dir) ts.baseDir = tapeData.dir;
        if (typeof tapeData.free === 'number') ts.freeGb = tapeData.free;
        if (Array.isArray(tapeData.drives) && tapeData.drives.length > 0) ts.drives = tapeData.drives;

        const modalBody = document.getElementById('modal-body');
        const isTapeModalOpen = !!(modalBody && modalBody.dataset.modalType === 'tape');
        if (isTapeModalOpen) {
            const freeEl = document.getElementById('tape-free-gb-badge');
            if (freeEl) freeEl.textContent = `${ts.freeGb.toFixed(1)} GB FREE`;
        }

        if (Array.isArray(tapeData.vu)) {
            for (let i = 0; i < tapeData.vu.length && i < ts.tracks.length; i++) {
                const vuDb = Number(tapeData.vu[i]);
                const isClip = Array.isArray(tapeData.clip) ? !!tapeData.clip[i] : false;
                ts.tracks[i].vu = vuDb;
                ts.tracks[i].clip = isClip ? 1 : 0;
                if (!isTapeModalOpen) continue;
                const fillEl = document.getElementById(`tape-vu-fill-${i}`);
                const txtEl = document.getElementById(`tape-vu-txt-${i}`);
                const clipEl = document.getElementById(`tape-clip-led-${i}`);
                if (fillEl) {
                    const pct = Math.max(3, Math.min(100, ((vuDb + 20.0) / 26.0) * 100.0));
                    fillEl.style.width = `${pct.toFixed(1)}%`;
                    fillEl.style.background = vuDb > 3.0
                        ? 'linear-gradient(90deg, #22c55e 0%, #f59e0b 75%, #ef4444 100%)'
                        : (vuDb >= -0.2 ? 'linear-gradient(90deg, #22c55e 0%, #f59e0b 100%)' : '#22c55e');
                }
                if (txtEl) {
                    txtEl.textContent = vuDb <= -22 ? '-∞ VU' : `${vuDb >= 0 ? '+' : ''}${vuDb.toFixed(1)} VU`;
                    txtEl.style.color = vuDb > 3.0 ? '#fca5a5' : '#fde68a';
                }
                if (clipEl) {
                    clipEl.style.background = isClip ? '#ef4444' : '#291515';
                    clipEl.style.boxShadow = isClip ? '0 0 8px #ef4444' : 'none';
                }
            }
        }
    };

    window.refreshTapeModalView = function() {
        const body = document.getElementById('modal-body');
        const title = document.getElementById('modal-title');
        const modal = document.getElementById('modal-container');
        if (body && body.dataset.modalType === 'tape') window.renderTapeMachineModal(title, body, modal);
    };

    function renderRedFaderWell(targetType, idx, val) {
        const capBottom = ((val / 150.0) * 82 + 9).toFixed(1);
        const capId = targetType === 'track' ? `tape-red-cap-${idx}` : (targetType === 'master' ? 'tape-out-cap-master' : `tape-out-cap-mon-${idx}`);
        return `
            <div class="tascam-fader-area" onpointerdown="startTapeFaderDrag(event, '${targetType}', ${idx})" title="Multi-Finger Drag Red TASCAM Fader | Double-Tap = 100% (0 dB)">
                <div style="display:flex; flex-direction:column; justify-content:space-between; height:100%; font-size:0.5rem; font-family:monospace; color:#b8ae9c; padding:4px 3px 4px 0; text-align:right; pointer-events:none;">
                    <span style="color:#f87171;">+6</span><span style="color:#fde68a; font-weight:900;">0</span><span>-5</span><span>-10</span><span>-20</span><span>∞</span>
                </div>
                <div class="tascam-fader-slot">
                    <div style="position:absolute; bottom:63.6%; left:-8px; right:-8px; border-top:1.5px solid rgba(253,230,138,0.55); pointer-events:none;"></div>
                    <div id="${capId}" class="tascam-red-cap" style="bottom:${capBottom}%;"></div>
                </div>
                <div style="display:flex; flex-direction:column; justify-content:space-between; height:100%; font-size:0.46rem; font-family:monospace; color:#8c8270; padding:4px 0 4px 3px; pointer-events:none;">
                    <span>━</span><span>━</span><span>━</span><span>━</span><span>━</span><span>━</span>
                </div>
            </div>`;
    }

    window.renderTapeMachineModal = function(title, body, modal) {
        const ts = window.tapeState;
        if (modal) modal.classList.add('fullscreen', 'tape-fullscreen');
        body.dataset.modalType = 'tape';
        if (title) {
            title.innerHTML = `<span style="font-family:monospace; letter-spacing:1px; color:#f5e6c8; text-shadow:0 1px 2px #000;">📼 TASCAM / STUDER MULTITRACK PORTASTUDIO</span> <span style="font-size:0.66rem; color:#fde68a; margin-left:8px; padding:2px 7px; background:#3b2a14; border:1px solid #b45309; border-radius:3px;">0 VU = -18.0 dBFS • STUDIO TIME COUNTER</span>`;
        }

        // Split tracks into 8-track tablet banks so never > 8 strips appear at once
        const chTracks = ts.tracks.filter(t => !t.isDirect && !t.isFx && !t.isDub);
        const fxAndDubTracks = ts.tracks.filter(t => !t.isDirect && (t.isFx || t.isDub));
        const directTracks = ts.tracks.filter(t => t.isDirect);

        let centerContentHtml = '';
        if (ts.subPage === 'outputs') {
            centerContentHtml = `
                <div style="flex:1; display:flex; flex-direction:column; justify-content:space-between; background:rgba(20,19,17,0.86); border:1.5px solid #575044; border-radius:6px; padding:12px 14px; overflow:hidden;">
                    <div style="display:flex; justify-content:space-between; align-items:center; border-bottom:1px solid #4a4338; padding-bottom:6px;">
                        <div>
                            <div style="font-size:0.88rem; font-weight:900; color:#fde68a; font-family:monospace;">🎧 STEREO TAPE MONITOR OUTPUT ROUTING &amp; ANALOG HEAD CALIBRATION</div>
                            <div style="font-size:0.68rem; color:#d6cfc2;">Non-destructive monitor balance. Raw WAV stems in <code style="color:#fbbf24;">${ts.baseDir}/${ts.songName}/</code> remain untouched.</div>
                        </div>
                        <button class="tascam-transport-btn" style="background:#b45309; border-color:#f59e0b; color:#fff;" onclick="switchTapeSubPage('main', 0)">← BACK TO CH 1-8</button>
                    </div>
                    <div style="display:flex; gap:8px; align-items:stretch; justify-content:space-between; flex:1; padding:8px 0;">
                        <div class="tascam-strip" style="flex:1; min-width:0; border-color:#f59e0b;">
                            <div style="font-size:0.68rem; font-weight:900; color:#fde68a; font-family:monospace;">MAIN L/R</div>
                            <div style="font-size:0.54rem; color:#d6cfc2;">STEREO BUS</div>
                            ${renderRedFaderWell('master', 0, ts.masterVol)}
                            <div style="font-size:0.64rem; font-family:monospace; color:#fde68a; font-weight:900;" id="tape-master-vol-val">${ts.masterVol}%</div>
                        </div>
                        ${[0, 1, 2, 3, 4, 5].map(m => `
                            <div class="tascam-strip" style="flex:1; min-width:0;">
                                <div style="font-size:0.64rem; font-weight:900; color:#e7e2d5; font-family:monospace;">MON ${m + 1}</div>
                                <div style="font-size:0.52rem; color:#a8a090;">AUX ${m + 1}</div>
                                ${renderRedFaderWell('mon', m, ts.monSends[m])}
                                <div style="font-size:0.62rem; font-family:monospace; color:#38bdf8; font-weight:900;" id="tape-mon-send-val-${m}">${ts.monSends[m]}%</div>
                            </div>
                        `).join('')}
                    </div>
                </div>`;
        } else {
            const pool = ts.subPage === 'direct' ? directTracks : (ts.subPage === 'fx' ? fxAndDubTracks : chTracks);
            const bankStart = (ts.bankPage || 0) * 8;
            const visibleTracks = pool.slice(bankStart, bankStart + 8);
            const stripsHtml = visibleTracks.map(t => {
                const pct = Math.max(3, Math.min(100, ((t.vu + 20.0) / 26.0) * 100.0));
                const panLbl = t.pan === 0 ? 'C' : (t.pan < 0 ? `L${Math.abs(t.pan)}` : `R${t.pan}`);
                return `
                    <div class="tascam-strip" style="flex:1 1 0; min-width:0;">
                        <div style="width:100%; display:flex; justify-content:space-between; align-items:center;">
                            <span style="font-size:0.6rem; font-weight:900; font-family:monospace; color:#fde68a; background:#1c1915; padding:1px 5px; border-radius:2px; border:1px solid #4a4236;">TRK ${t.idx + 1}</span>
                            <span id="tape-clip-led-${t.idx}" title="-1.0 dBFS Peak LED" style="width:9px; height:9px; border-radius:50%; background:#291515; border:1px solid #573a32;"></span>
                        </div>
                        <div style="font-size:0.62rem; font-weight:900; color:#f5efe6; text-align:center; white-space:nowrap; overflow:hidden; text-overflow:ellipsis; width:100%; text-shadow:0 1px 1px #000;">${t.name}</div>
                        <div class="tascam-vu-box" title="0 VU = -18.0 dBFS | 0..+3 VU Golden Tape Sweet Spot">
                            <div style="display:flex; justify-content:space-between; font-size:0.44rem; font-family:monospace; color:#fcd34d; opacity:0.9;">
                                <span>-20</span><span>-10</span><span style="color:#fef08a; font-weight:900;">0VU</span><span style="color:#fb923c;">+3</span><span style="color:#f87171;">+6</span>
                            </div>
                            <div style="position:relative; width:100%; height:11px; background:#0c0702; border:1px solid #5c3a14; border-radius:2px; overflow:hidden;">
                                <div style="position:absolute; left:76.9%; top:0; bottom:0; border-left:1.5px solid #fef08a; z-index:3;"></div>
                                <div style="position:absolute; left:88.5%; top:0; bottom:0; border-left:1px dashed #ef4444; z-index:3;"></div>
                                <div id="tape-vu-fill-${t.idx}" style="height:100%; width:${pct.toFixed(1)}%; background:#22c55e;"></div>
                            </div>
                            <div style="display:flex; justify-content:space-between; align-items:center;">
                                <span style="font-size:0.44rem; color:#d6cfc2; font-family:monospace;">${t.badge}</span>
                                <span id="tape-vu-txt-${t.idx}" style="font-size:0.5rem; font-family:monospace; font-weight:900; color:#fde68a;">-20.0 VU</span>
                            </div>
                        </div>
                        <div style="width:100%; background:rgba(18,17,15,0.82); padding:2px 4px; border-radius:3px; border:1px solid #3d372e;">
                            <div style="display:flex; justify-content:space-between; font-size:0.5rem; font-family:monospace; color:#fbbf24;">
                                <span>DRIVE</span><span id="tape-trk-drv-val-${t.idx}">${t.drive >= 0 ? '+' : ''}${t.drive.toFixed(1)}dB</span>
                            </div>
                            <input type="range" min="-6" max="18" step="0.5" value="${t.drive}" data-default="1.5" style="width:100%; height:8px; accent-color:#d97706;" oninput="setTapeTrackParam(${t.idx}, 'drive', this.value)">
                            <div style="display:flex; justify-content:space-between; font-size:0.5rem; font-family:monospace; color:#d6cfc2; margin-top:1px;">
                                <span>PAN</span><span id="tape-trk-pan-val-${t.idx}">${panLbl}</span>
                            </div>
                            <input type="range" min="-100" max="100" step="5" value="${t.pan}" data-default="0" style="width:100%; height:8px; accent-color:#a8a29e;" oninput="setTapeTrackParam(${t.idx}, 'pan', this.value)">
                        </div>
                        ${renderRedFaderWell('track', t.idx, t.vol)}
                        <div style="font-size:0.54rem; font-family:monospace; color:#e7e2d5;">LVL <b id="tape-trk-vol-val-${t.idx}" style="color:#fde68a;">${t.vol}%</b></div>
                        <div style="display:flex; gap:3px; width:100%;">
                            <button class="tascam-transport-btn" style="flex:1; min-height:24px; padding:1px 0; font-size:0.58rem; background:${t.mute ? '#b91c1c' : '#2b2925'}; color:#fff;" onclick="setTapeTrackParam(${t.idx}, 'mute')">M</button>
                            <button class="tascam-transport-btn" style="flex:1; min-height:24px; padding:1px 0; font-size:0.58rem; background:${t.solo ? '#ca8a04' : '#2b2925'}; color:#fff;" onclick="setTapeTrackParam(${t.idx}, 'solo')">S</button>
                            <button class="tascam-transport-btn" style="flex:1; min-height:24px; padding:1px 0; font-size:0.58rem; background:${t.rec ? '#991b1b' : '#2b2925'}; color:#fca5a5;" onclick="setTapeTrackParam(${t.idx}, 'rec')">●</button>
                        </div>
                    </div>`;
            }).join('');

            centerContentHtml = `
                <div style="flex:1; display:flex; gap:6px; overflow:hidden; padding:6px 8px; background:rgba(14,13,11,0.78); border:1.5px solid #4a4338; border-radius:6px; align-items:stretch;">
                    ${stripsHtml}
                </div>`;
        }

        body.innerHTML = `
            <div style="display:flex; flex-direction:column; gap:5px; height:100%; width:100%; color:#f5efe6; overflow:hidden;">
                <!-- TOP HARDWARE TRANSPORT, STORAGE DRIVE SELECTOR (SD / USB SSD) & SONG FOLDER BAR -->
                <div class="tascam-deck-bar">
                    <div style="display:flex; align-items:center; gap:5px; flex-wrap:wrap;">
                        <div style="background:#090806; border:2px solid #574c3a; border-radius:4px; padding:2px 8px; font-family:monospace; box-shadow:inset 0 2px 6px #000;">
                            <div style="font-size:0.46rem; color:#a8a090;">TAPE TIME (HH:MM:SS.d)</div>
                            <div id="tape-timecode-val" style="font-size:1.05rem; font-weight:900; color:#fbbf24; letter-spacing:1px;">${formatTapeTime(ts.sec)}</div>
                        </div>
                        <button class="tascam-transport-btn" onclick="sendTapeTransport('rtz')">⏮ RTZ</button>
                        <button class="tascam-transport-btn" style="background:${ts.transport === 2 ? '#dc2626' : '#5c1919'}; color:#fff;" onclick="sendTapeTransport('rec')">● REC</button>
                        <button id="tape-modal-stop-btn" class="tascam-transport-btn" onclick="sendTapeTransport('stop')">■ STOP</button>
                        <button class="tascam-transport-btn" style="background:${ts.transport === 1 ? '#15803d' : '#1b3b28'}; color:#fff;" onclick="sendTapeTransport('play')">▶ PLAY</button>
                        <span id="tape-deck-status-badge" style="font-size:0.62rem; font-family:monospace; font-weight:800; color:#fde68a;">📁 ${ts.songName}</span>
                    </div>

                    <div style="display:flex; align-items:center; gap:4px; flex-wrap:wrap;">
                        <select id="tape-drive-select" onchange="setTapeStoragePath(this.value)" title="Select SD Card or External USB SSD Drive" style="background:#1c1915; color:#38bdf8; border:1px solid #6e6250; border-radius:3px; padding:4px 5px; font-size:0.64rem; font-family:monospace;">
                            ${ts.drives.map(d => `<option value="${d.path}" ${d.path === ts.baseDir ? 'selected' : ''}>💾 ${d.label} (${Number(d.free || 0).toFixed(1)}GB)</option>`).join('')}
                        </select>
                        <input id="tape-storage-path-input" type="text" value="${ts.baseDir}" title="Recording Storage Folder Path" style="background:#12100e; color:#38bdf8; border:1px solid #6e6250; border-radius:3px; padding:4px 6px; font-family:monospace; font-size:0.64rem; width:138px;">
                        <button class="tascam-transport-btn" style="min-height:26px; padding:2px 6px; font-size:0.6rem;" onclick="setTapeStoragePath()">SET PATH</button>
                        <span id="tape-free-gb-badge" style="font-size:0.58rem; font-family:monospace; color:#34d399; background:#14261c; padding:3px 5px; border:1px solid #15803d; border-radius:3px;">${ts.freeGb.toFixed(1)} GB FREE</span>
                        <input id="tape-song-name-input" type="text" value="${ts.songName}" style="background:#12100e; color:#fde68a; border:1px solid #6e6250; border-radius:3px; padding:4px 6px; font-family:monospace; font-size:0.66rem; width:118px;">
                        <select onchange="document.getElementById('tape-song-name-input').value=this.value; handleTapeSongOp('load')" style="background:#26231f; color:#f5efe6; border:1px solid #6e6250; border-radius:3px; padding:4px 5px; font-size:0.64rem;">
                            ${ts.savedSongs.map(s => `<option value="${s}" ${s === ts.songName ? 'selected' : ''}>📂 ${s}</option>`).join('')}
                        </select>
                        <button class="tascam-transport-btn" style="min-height:26px; padding:2px 6px;" onclick="handleTapeSongOp('new')">📁 NEW</button>
                        <button class="tascam-transport-btn" style="min-height:26px; padding:2px 6px; background:#9a3412; color:#fef3c7;" onclick="handleTapeSongOp('bounce')">🔥 BOUNCE</button>
                    </div>
                </div>

                <!-- SECOND BAR: 8-TRACK BANK SELECTOR (CH 1-8 | FX 9-12 & DUBS | DIRECT 1-8 | OUTPUTS) -->
                <div class="tascam-deck-bar" style="padding:4px 10px;">
                    <div style="display:flex; align-items:center; gap:5px; flex-wrap:wrap;">
                        <button class="tascam-transport-btn" style="background:${ts.subPage === 'main' ? '#b45309' : '#2b2925'}; color:#fff;" onclick="switchTapeSubPage('main', 0)">🎛️ CH 1-8 (${chTracks.length})</button>
                        <button class="tascam-transport-btn" style="background:${ts.subPage === 'fx' ? '#7c2d12' : '#2b2925'}; color:#fff;" onclick="switchTapeSubPage('fx', 0)">✨ FX 9-12 &amp; DUBS (${fxAndDubTracks.length})</button>
                        <button class="tascam-transport-btn" style="background:${ts.subPage === 'direct' && !ts.bankPage ? '#0369a1' : '#2b2925'}; color:#fff;" onclick="switchTapeSubPage('direct', 0)">🔌 DIRECT 1-8 (1176+HPF)</button>
                        ${directTracks.length > 8 ? `<button class="tascam-transport-btn" style="background:${ts.subPage === 'direct' && ts.bankPage === 1 ? '#0369a1' : '#2b2925'}; color:#fff;" onclick="switchTapeSubPage('direct', 1)">🔌 DIRECT DUBS</button>` : ''}
                        <button class="tascam-transport-btn" style="background:${ts.subPage === 'outputs' ? '#4f46e5' : '#2b2925'}; color:#fde68a;" onclick="switchTapeSubPage('outputs', 0)">🎧 OUTPUTS &amp; MON</button>
                        <div style="display:flex; align-items:center; gap:4px; background:#141210; padding:2px 6px; border-radius:4px; border:1px solid #575044;">
                            <select id="tape-add-src-select" style="background:#26231f; color:#fde68a; border:1px solid #575044; font-size:0.64rem; padding:2px 4px;">
                                ${chNames.map((nm, idx) => `<option value="${idx}" ${idx === ts.addSourceCh ? 'selected' : ''}>CH ${idx + 1} (${nm})</option>`).join('')}
                            </select>
                            <button class="tascam-transport-btn" style="min-height:26px; padding:2px 7px; background:#15803d; font-size:0.64rem;" onclick="addPairedTapeTrack()">+ ADD DUB PAIR</button>
                        </div>
                    </div>
                    <div style="display:flex; align-items:center; gap:5px; flex-wrap:wrap;">
                        <button class="tascam-transport-btn" style="min-height:26px; padding:2px 7px; font-size:0.64rem; background:${ts.ips === 15 ? '#78350f' : '#2b2925'};" onclick="setTapeMachineParam('ips', 15)">WARM HEAD</button>
                        <button class="tascam-transport-btn" style="min-height:26px; padding:2px 7px; font-size:0.64rem; background:${ts.ips === 30 ? '#78350f' : '#2b2925'};" onclick="setTapeMachineParam('ips', 30)">HI-FI HEAD</button>
                        <button class="tascam-transport-btn" style="min-height:26px; padding:2px 7px; font-size:0.64rem; background:${ts.emuEnabled ? '#166534' : '#2b2925'};" onclick="setTapeMachineParam('emuEnabled', !window.tapeState.emuEnabled)">🧲 ANALOG HEADS: ${ts.emuEnabled ? 'ON' : 'BYP'}</button>
                        <button class="tascam-transport-btn" style="min-height:26px; padding:2px 7px; font-size:0.64rem; background:${ts.printWav ? '#991b1b' : '#2b2925'};" onclick="setTapeMachineParam('printWav', !window.tapeState.printWav)">🔥 PRINT WAV: ${ts.printWav ? 'ON' : 'OFF'}</button>
                    </div>
                </div>
                ${centerContentHtml}
            </div>`;
    };
})(window);
