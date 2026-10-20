// telemetry.js - Real-time WebSocket telemetry, Peak & GR meters, RTA update
(function(window) {
    'use strict';

        // 1. Real-time JACK WebSocket Peak & GR Telemetry Engine
        // Connects to ws://${host}:8081/meters (telemetry server) with fallback to ws://${host}:8089
        // Strictly compliant with AGENT_RULES.md: Zero synthetic meters or fake physics.
        let meterWs;
        window.liveRtaBins = new Float32Array(64); // 64-band RTA spectrum underlay
        window.channelGrValues = new Float32Array(8); // Real GR in dB

        function getPeakColor(db) {
            if (db < -18.0) return "#10b981"; // Below -18 dBFS: Green
            if (db < -6.0)  return "#eab308"; // -18 to -6 dBFS: Yellow
            if (db < 0.0)   return "#f97316"; // -6 to 0 dBFS: Orange
            return "#ef4444";                 // 0 dBFS and above: Red / Clipping
        }

        function getGrColor(grDb) {
            // grDb is positive reduction (e.g. 3 dB reduction)
            if (grDb <= 6.0)  return "#10b981"; // 0 to -6 dB reduction: Green
            if (grDb <= 12.0) return "#f59e0b"; // -6 to -12 dB reduction: Orange
            return "#ef4444";                   // Beyond -12 dB reduction: Red
        }

        // Stage 8: Parallel Multi-Host & Subnet Auto-Discovery for Lenovo Tab M11 Kiosk APK & Web Console
        const DISCOVERY_HOSTS = ["192.168.1.148", "10.42.0.1", "172.24.1.1", "patchbox.local"];
        let _hostCandidateIdx = 0;
        let _useSavedHost = true;
        let _wsConnectTimer = null;

        window.getActivePiHost = function() {
            if (window._nativeDiscoveredHost) return window._nativeDiscoveredHost;
            const loc = window.location.hostname;
            if (loc && loc !== "localhost" && loc !== "127.0.0.1" && !loc.startsWith("file")) {
                return loc;
            }
            if (_useSavedHost) {
                try {
                    const saved = localStorage.getItem("modep_pi_host");
                    if (saved && saved.trim()) return saved.trim();
                } catch(e) {}
            }
            return DISCOVERY_HOSTS[_hostCandidateIdx % DISCOVERY_HOSTS.length];
        };

        window.onNativePiDiscovered = function(ip) {
            if (!ip) return;
            window._nativeDiscoveredHost = ip.trim();
            try { localStorage.setItem("modep_pi_host", window._nativeDiscoveredHost); } catch(e) {}
            if (!meterWs || meterWs.readyState !== WebSocket.OPEN) {
                if (meterWs) { try { meterWs.close(); } catch(e) {} }
                connectMeterWs();
            }
        };

        function probeParallelHosts() {
            if (meterWs && meterWs.readyState === WebSocket.OPEN) return;
            DISCOVERY_HOSTS.forEach(cand => {
                try {
                    const pws = new WebSocket("ws://" + cand + ":8089", "ws-meters");
                    const tmr = setTimeout(() => { try { pws.close(); } catch(e) {} }, 1200);
                    pws.onopen = () => {
                        clearTimeout(tmr);
                        try { pws.close(); } catch(e) {}
                        window.onNativePiDiscovered(cand);
                    };
                    pws.onerror = () => clearTimeout(tmr);
                } catch(e) {}
            });
        }

        window.promptPiHostOverride = function() {
            const cur = window.getActivePiHost();
            const next = window.prompt(
                "📡 PATCHBOX PI CONNECTION HOST / IP\n" +
                "• Leave blank for Auto-Scan (Wi-Fi Subnet + 192.168.1.148 ↔ 10.42.0.1 ↔ 172.24.1.1)\n" +
                "• Or enter custom Pi IP address:",
                cur
            );
            if (next === null) return;
            try {
                if (next.trim() === "" || next.trim().toLowerCase() === "auto") {
                    localStorage.removeItem("modep_pi_host");
                    window._nativeDiscoveredHost = null;
                    _useSavedHost = false;
                } else {
                    localStorage.setItem("modep_pi_host", next.trim());
                    window._nativeDiscoveredHost = next.trim();
                    _useSavedHost = true;
                }
            } catch(e) {}
            if (meterWs) { try { meterWs.close(); } catch(e) {} }
            connectMeterWs();
        };

        function connectMeterWs() {
            if (_wsConnectTimer) { clearTimeout(_wsConnectTimer); _wsConnectTimer = null; }
            const host = window.getActivePiHost();
            const fallbackUri = "ws://" + host + ":8089";
            const ind = document.getElementById("live-indicator");
            if (ind && (!meterWs || meterWs.readyState !== WebSocket.OPEN)) {
                ind.innerText = "SCANNING (" + host + ")...";
                ind.style.background = "#d97706";
                ind.style.cursor = "pointer";
                ind.onclick = window.promptPiHostOverride;
            }

            try {
                meterWs = new WebSocket(fallbackUri, "ws-meters");
            } catch(e) {
                try { meterWs = new WebSocket(fallbackUri); } catch(err) {}
            }
            window.meterWs = meterWs;
            if (!meterWs) {
                setTimeout(connectMeterWs, 1000);
                return;
            }
            const currentWs = meterWs;
            _wsConnectTimer = setTimeout(() => {
                if (currentWs && currentWs.readyState !== WebSocket.OPEN) {
                    _useSavedHost = false;
                    window._nativeDiscoveredHost = null;
                    _hostCandidateIdx = (_hostCandidateIdx + 1) % DISCOVERY_HOSTS.length;
                    probeParallelHosts();
                    try { currentWs.close(); } catch(e) {}
                }
            }, 1500);

            const _rawSend = meterWs.send.bind(meterWs);
            window.sendRawWs = _rawSend;
            meterWs.send = function(payload) {
                if (window._applyingRemoteSync) return;
                _rawSend(payload);
            };

            meterWs.onopen = () => {
                if (_wsConnectTimer) { clearTimeout(_wsConnectTimer); _wsConnectTimer = null; }
                window.meterWs = meterWs;
                _useSavedHost = true;
                try { localStorage.setItem("modep_pi_host", host); } catch(e) {}
                const indEl = document.getElementById("live-indicator");
                if (indEl) {
                    indEl.innerText = "LIVE SYNC (" + host + ")";
                    indEl.style.background = "#238636";
                    indEl.style.cursor = "pointer";
                    indEl.title = "Connected to " + host + ":8089 (Tap to configure Pi IP)";
                    indEl.onclick = window.promptPiHostOverride;
                }
                if (typeof window.onMultiDeviceWsOpen === "function") {
                    window.onMultiDeviceWsOpen();
                }
                if (typeof window.syncAllChannelSlotChains === "function") {
                    window.syncAllChannelSlotChains();
                }
            };

            meterWs.onerror = () => {
                _useSavedHost = false;
                _hostCandidateIdx = (_hostCandidateIdx + 1) % DISCOVERY_HOSTS.length;
            };

            const _meterDomCache = {};
            function getFastEl(id) {
                let el = _meterDomCache[id];
                if (!el || !el.isConnected) {
                    el = document.getElementById(id);
                    if (el) _meterDomCache[id] = el;
                }
                return el;
            }
            function setFastMeter(id, level) {
                const bar = getFastEl(id);
                if (!bar) return;
                if (level > 0.00001) {
                    const db = 20.0 * Math.log10(level);
                    const pct = Math.max(0, Math.min(100, ((db + 60.0) / 66.0) * 100));
                    const hStr = pct.toFixed(1) + "%";
                    if (bar._lastH !== hStr) {
                        bar._lastH = hStr;
                        bar.style.height = hStr;
                        bar.style.background = getPeakColor(db);
                    }
                } else if (bar._lastH !== "0%") {
                    bar._lastH = "0%";
                    bar.style.height = "0%";
                }
            }
            function pctToLinGain(pct) {
                if (!pct || pct <= 0.5) return 0.0;
                const db = (typeof window.percentToDb === "function") ? window.percentToDb(pct) : ((pct / 75.0) * 60.0 - 60.0);
                return db <= -59.5 ? 0.0 : Math.pow(10.0, db / 20.0);
            }

            let _latestMeterPacket = null;
            let _meterRafId = 0;
            const _lastChPreLevels = [0, 0, 0, 0, 0, 0, 0, 0];

            window.refreshLocalChannelMeterImmediate = function(ch) {
                if (ch < 0 || ch >= 8) return;
                const sofMode = window.currentSofMode;
                const preSig = _lastChPreLevels[ch] || 0.0;
                if (sofMode && window.faderValues && window.faderValues.sof && window.faderValues.sof[sofMode]) {
                    const sofPct = window.faderValues.sof[sofMode][ch] ?? 0;
                    setFastMeter("meter-" + ch, preSig * pctToLinGain(sofPct));
                } else {
                    const isMuted = !!(window.muteState && window.muteState[ch]);
                    const fPct = (window.faderValues && window.faderValues.main) ? (window.faderValues.main[ch] ?? 75) : 75;
                    const level = isMuted ? 0.0 : (preSig * pctToLinGain(fPct));
                    setFastMeter("meter-" + ch, level);
                    if (window.dspRackCurrentChannel === ch) {
                        setFastMeter("meter-focused-rack", level);
                    }
                }
            };

            function renderLatestMeterFrame() {
                _meterRafId = 0;
                const data = _latestMeterPacket;
                if (!data) return;
                _latestMeterPacket = null;

                try {
                    // 0. Universal Audio Interface Hardware Telemetry Badge & Dynamic I/O Map
                    if (data.hw && typeof window.updateAudioHardwareTelemetry === "function") {
                        window.updateAudioHardwareTelemetry(data.hw);
                    }

                    // 1. Channel Peak & Master Meters (with Sends on Fader SOF Live Metering)
                    if (data.type === "meters" || data.ch) {
                        const chs = data.ch || [];
                        const chPre = data.ch_pre || chs;
                        const preIn = data.preamp_in || [];
                        const preSt = data.preamp_status || [];
                        const sofMode = window.currentSofMode;
                        const sofArr = (sofMode && window.faderValues && window.faderValues.sof) ? window.faderValues.sof[sofMode] : null;

                        for (let i = 0; i < 8; i++) {
                            const mainLevel = chs[i] || 0.0;
                            const rawPreLevel = (preIn[i] !== undefined) ? preIn[i] : mainLevel;
                            _lastChPreLevels[i] = (chPre[i] !== undefined) ? chPre[i] : rawPreLevel;
                            if (typeof window.updatePreamp1176ClipTelemetry === "function") {
                                window.updatePreamp1176ClipTelemetry(i, rawPreLevel, preSt[i]);
                            }
                            if (mainLevel >= 0.944 && typeof window.latchClipHold === "function") {
                                window.latchClipHold(i);
                            }
                            // In Sends-on-Fader (SOF) mode, display each channel's post-SOF send contribution!
                            let displayLevel = mainLevel;
                            if (sofMode && sofArr) {
                                const sofPct = sofArr[i] ?? 0;
                                const preSig = (chPre[i] !== undefined && chPre[i] > 0) ? chPre[i] : rawPreLevel;
                                displayLevel = preSig * pctToLinGain(sofPct);
                            }
                            setFastMeter("meter-" + i, displayLevel);
                        }

                        // Master L/R Peak Meters (or active SOF Bus Meter when SOF mode is engaged)
                        const m = data.master || [0.0, 0.0];
                        if ((m[0] >= 0.944 || m[1] >= 0.944) && typeof window.latchClipHold === "function") {
                            window.latchClipHold("master");
                        }
                        let masterDisplayL = m[0] || 0.0;
                        let masterDisplayR = m[1] || 0.0;
                        if (sofMode) {
                            let sofBusLevel = 0.0;
                            if (sofMode.startsWith("fx") && data.send) {
                                const fxIdx = parseInt(sofMode.replace(/\D+/g, ""), 10) - 1;
                                sofBusLevel = (data.send[fxIdx] || 0.0);
                            } else if (sofMode.startsWith("mon") && data.aux) {
                                const monIdx = parseInt(sofMode.replace(/\D+/g, ""), 10) - 1;
                                sofBusLevel = (data.aux[monIdx] || 0.0);
                            }
                            masterDisplayL = sofBusLevel;
                            masterDisplayR = sofBusLevel;
                            const sofFill = getFastEl("sof-banner-meter-fill");
                            const sofDbEl = getFastEl("sof-banner-meter-db");
                            if (sofFill) {
                                const sDb = sofBusLevel > 0.00001 ? 20.0 * Math.log10(sofBusLevel) : -100;
                                const sPct = Math.max(0, Math.min(100, ((sDb + 60.0) / 66.0) * 100));
                                sofFill.style.width = sPct.toFixed(1) + "%";
                                sofFill.style.background = getPeakColor(sDb);
                                if (sofDbEl) sofDbEl.innerText = sDb > -59.5 ? (sDb >= 0 ? "+" : "") + sDb.toFixed(1) + "dB" : "-inf";
                            }
                        }
                        setFastMeter("meter-master-l", masterDisplayL);
                        setFastMeter("meter-master-r", masterDisplayR);

                        // Focused rack meter
                        if (typeof window.dspRackCurrentChannel === 'number') {
                            const fCh = window.dspRackCurrentChannel;
                            const fLevel = (chs[fCh] > 0.0001) ? chs[fCh] : (chPre[fCh] || 0.0);
                            setFastMeter("meter-focused-rack", fLevel);
                        }

                        // Aux 1-6 Stage Monitor Meters (Safety Limiters capped at -1.0 dBFS)
                        if (data.aux) {
                            for (let a = 1; a <= 6; a++) {
                                setFastMeter(`meter-mon-${a}`, Math.min(0.891, data.aux[a - 1] || 0.0));
                            }
                        }

                        // FX Sends 1-4 & FX Returns 1-4 Meters
                        if (data.send) {
                            for (let s = 1; s <= 4; s++) {
                                setFastMeter(`meter-send-${s}`, data.send[s - 1] || 0.0);
                            }
                        }
                        if (data.ret) {
                            for (let r = 1; r <= 4; r++) {
                                const rLev = data.ret[r - 1] || 0.0;
                                setFastMeter(`meter-ret-${r}-l`, rLev);
                                setFastMeter(`meter-ret-${r}-r`, rLev);
                            }
                        }

                        // 2. Gain Reduction (GR) Multi-Color Meters beside each fader
                        const grs = data.gr || [];
                        for (let i = 0; i < 8; i++) {
                            const grVal = Math.abs(grs[i] || 0.0); // dB reduction
                            window.channelGrValues[i] = grVal;
                            const grBar = getFastEl("gr-meter-" + i);
                            if (grBar) {
                                if (grVal > 0.05) {
                                    const grPct = Math.min(100, (grVal / 24.0) * 100);
                                    const hStr = grPct.toFixed(1) + "%";
                                    if (grBar._lastH !== hStr) {
                                        grBar._lastH = hStr;
                                        grBar.style.height = hStr;
                                        grBar.style.background = getGrColor(grVal);
                                    }
                                } else if (grBar._lastH !== "0%") {
                                    grBar._lastH = "0%";
                                    grBar.style.height = "0%";
                                }
                            }
                        }

                        // Bus GR values (Master,Monitors,Sends,Returns)
                        const masterGr = Math.abs(data.master_gr || 0.0);
                        if (window.busGrValues) {
                            window.busGrValues.master = masterGr;
                            if (Array.isArray(data.mon_gr)) {
                                for (let m = 1; m <= 6; m++) window.busGrValues[`mon-${m}`] = Math.abs(data.mon_gr[m - 1] || 0.0);
                            }
                            if (Array.isArray(data.send_gr)) {
                                for (let s = 1; s <= 4; s++) window.busGrValues[`send-${s}`] = Math.abs(data.send_gr[s - 1] || 0.0);
                            }
                            if (Array.isArray(data.ret_gr)) {
                                for (let r = 1; r <= 4; r++) window.busGrValues[`ret-${r}`] = Math.abs(data.ret_gr[r - 1] || 0.0);
                            }
                        }
                        const masterGrBar = getFastEl("gr-meter-master");
                        if (masterGrBar) {
                            const hStr = masterGr > 0.05 ? Math.min(100, (masterGr / 24.0) * 100).toFixed(1) + "%" : "0%";
                            if (masterGrBar._lastH !== hStr) {
                                masterGrBar._lastH = hStr;
                                masterGrBar.style.height = hStr;
                                if (masterGr > 0.05) masterGrBar.style.background = getGrColor(masterGr);
                            }
                        }

                        // Focused rack GR bar
                        if (typeof window.dspRackCurrentChannel === 'number') {
                            const fGrBar = getFastEl("gr-meter-focused-rack");
                            if (fGrBar) {
                                const fGr = window.channelGrValues[window.dspRackCurrentChannel] || 0;
                                const hStr = fGr > 0.05 ? Math.min(100, (fGr / 24.0) * 100).toFixed(1) + "%" : "0%";
                                if (fGrBar._lastH !== hStr) {
                                    fGrBar._lastH = hStr;
                                    fGrBar.style.height = hStr;
                                    if (fGr > 0.05) fGrBar.style.background = getGrColor(fGr);
                                }
                            }
                        }

                        // 3. Compressor Modal Live VU Needle & Digital GR LED Bar Updates
                        const activeTarget = window.currentCompModalTarget !== undefined ? window.currentCompModalTarget : (window.currentModalChannel ?? 0);
                        const targetGrPos = (typeof window.getTargetLiveGr === 'function') ? window.getTargetLiveGr(activeTarget) : (window.channelGrValues[activeTarget] || masterGr || 0.0);
                        if (typeof window.updateDigitalGrBar === 'function' && getFastEl('comp-digital-gr-bar-readout')) {
                            window.updateDigitalGrBar('comp-digital-gr-bar', targetGrPos);
                        }
                        if (typeof window.renderAoVuMeter === 'function') {
                            const fetEl = getFastEl('fet-vu-meter');
                            if (fetEl && fetEl.offsetParent !== null) window.renderAoVuMeter('fet-vu-meter', -targetGrPos, 'fetish');
                            const lalaEl = getFastEl('lala-vu-meter');
                            if (lalaEl && lalaEl.offsetParent !== null) window.renderAoVuMeter('lala-vu-meter', -targetGrPos, 'lala');
                            const busterEl = getFastEl('buster-vu-meter');
                            if (busterEl && busterEl.offsetParent !== null) window.renderAoVuMeter('buster-vu-meter', -targetGrPos, 'lala');
                        }

                        // 4. Live 64-Band RTA Spectrum Underlay & 8-Band Tukan Dynamic EQ Telemetry
                        if (data.dyn_eq && Array.isArray(data.dyn_eq) && typeof window.getChannelReEq === "function") {
                            const curSt = window.getChannelReEq(window.reEqRuntime ? window.reEqRuntime.currentTarget : 0);
                            if (curSt && curSt.bands) {
                                for (let b = 0; b < Math.min(8, data.dyn_eq.length); b++) {
                                    curSt.bands[b].dynGainDb = parseFloat(data.dyn_eq[b]) || 0.0;
                                    const dynBtn = getFastEl("reeq-dyn-btn-" + b);
                                    if (dynBtn && curSt.bands[b].dynEnabled) {
                                        const dg = curSt.bands[b].dynGainDb;
                                        dynBtn.innerText = Math.abs(dg) > 0.05 ? `DYN ${(dg >= 0 ? '+' : '') + dg.toFixed(1)}dB` : "DYN ON";
                                    }
                                }
                            }
                        }
                        if (data.rta && Array.isArray(data.rta)) {
                            for (let b = 0; b < Math.min(64, data.rta.length); b++) {
                                window.liveRtaBins[b] = data.rta[b];
                            }
                            const eqModalCanvas = getFastEl("reeq-canvas");
                            if (eqModalCanvas && eqModalCanvas.offsetParent !== null && typeof window.drawReaEQCurve === "function") {
                                const eqTarget = (window.reEqRuntime && window.reEqRuntime.currentTarget !== undefined) ? window.reEqRuntime.currentTarget : (window.reEqState ? window.reEqState.activeChannel : 0);
                                window.drawReaEQCurve(eqModalCanvas, window.getChannelReEq(eqTarget));
                            }
                            const rackPageEl = getFastEl("dsp-rack-page");
                            if (rackPageEl && rackPageEl.classList.contains("open") && typeof window.renderMiniReEq === "function") {
                                window.renderMiniReEq(window.dspRackCurrentChannel || 0);
                            }
                        }

                        // 5. Cyber-Denoiser PRO 10-Band Spectral Telemetry & Gate LED states
                        if (Array.isArray(data.denoise_l)) {
                            window.liveDenoiseBands = data.denoise_l;
                        }
                        if (data.gate && Array.isArray(data.gate)) {
                            for (let i = 0; i < 8; i++) {
                                const isOpen = !!data.gate[i];
                                const led1 = getFastEl('gate-led-' + i);
                                if (led1 && led1._lastOpen !== isOpen) {
                                    led1._lastOpen = isOpen;
                                    led1.classList.toggle("open", isOpen);
                                    led1.classList.toggle("closed", !isOpen);
                                }
                                const led2 = getFastEl('modal-gate-led-' + i);
                                if (led2 && led2._lastOpen !== isOpen) {
                                    led2._lastOpen = isOpen;
                                    led2.classList.toggle("open", isOpen);
                                    led2.classList.toggle("closed", !isOpen);
                                }
                            }
                        }

                        // 6. Dual-Band De-Esser (Stage 7) 60FPS Live GR & Detector Telemetry
                        if (Array.isArray(data.ds_gr1)) window.liveDeesserGr1 = data.ds_gr1;
                        if (Array.isArray(data.ds_gr2)) window.liveDeesserGr2 = data.ds_gr2;
                        if (Array.isArray(data.ds_det1)) window.liveDeesserDet1 = data.ds_det1;
                        if (Array.isArray(data.ds_det2)) window.liveDeesserDet2 = data.ds_det2;
                        if (typeof window.updateDeesserLiveMeters === "function") {
                            window.updateDeesserLiveMeters();
                        }

                        // 7. Raspberry Pi Real-Time System Telemetry Pill (JACK DSP %, CPU %, Core Temp, XRUN Counter)
                        if (data.sys) {
                            window.lastPiSysTelemetry = data.sys;
                            const dspEl = getFastEl("pi-tel-dsp");
                            const cpuEl = getFastEl("pi-tel-cpu");
                            const tmpEl = getFastEl("pi-tel-temp");
                            const xrEl  = getFastEl("pi-tel-xrun");
                            const dspVal = Number(data.sys.dsp || 0);
                            const cpuVal = Number(data.sys.cpu || 0);
                            const tmpVal = Number(data.sys.temp || 0);
                            const xrVal  = parseInt(data.sys.xruns || 0, 10);
                            if (dspEl) {
                                const s = "DSP " + dspVal.toFixed(1) + "%";
                                if (dspEl._lastTxt !== s) {
                                    dspEl._lastTxt = s;
                                    dspEl.innerText = s;
                                    dspEl.style.color = dspVal > 70 ? "#ef4444" : (dspVal > 45 ? "#fbbf24" : "#38bdf8");
                                }
                            }
                            if (cpuEl) {
                                const s = "CPU " + Math.round(cpuVal) + "%";
                                if (cpuEl._lastTxt !== s) {
                                    cpuEl._lastTxt = s;
                                    cpuEl.innerText = s;
                                    cpuEl.style.color = cpuVal > 80 ? "#ef4444" : (cpuVal > 55 ? "#fbbf24" : "#e2e8f0");
                                }
                            }
                            if (tmpEl) {
                                const s = Math.round(tmpVal) + "°C";
                                if (tmpEl._lastTxt !== s) {
                                    tmpEl._lastTxt = s;
                                    tmpEl.innerText = s;
                                    tmpEl.style.color = tmpVal >= 78 ? "#ef4444" : (tmpVal >= 68 ? "#fbbf24" : "#a7f3d0");
                                }
                            }
                            if (xrEl) {
                                const s = "XRUN " + xrVal;
                                if (xrEl._lastTxt !== s) {
                                    xrEl._lastTxt = s;
                                    xrEl.innerText = s;
                                    xrEl.classList.toggle("ok", xrVal === 0);
                                    xrEl.classList.toggle("warn", xrVal > 0);
                                }
                            }
                        }
                    }

                    // Stage 7: 2" Analog Multitrack Tape Machine & 0 VU = -18 dBFS Telemetry
                    if (data.tape && typeof window.updateTapeTelemetry === "function") {
                        window.updateTapeTelemetry(data.tape);
                    }
                } catch(e) {}
            }

            meterWs.onmessage = (evt) => {
                try {
                    const data = JSON.parse(evt.data);
                    if ((data.type === "sync_event" || data.type === "full_state_sync") && typeof window.handleMultiDeviceWsMessage === "function") {
                        window.handleMultiDeviceWsMessage(data);
                        return;
                    }

                    // Handle Bidirectional 10-Band Cyber-Denoiser Learn Result immediately
                    if (data.action === "denoise_learned") {
                        if (typeof window.handleDenoiseLearned === "function") {
                            const lch = (data.channel !== undefined) ? data.channel : data.ch;
                            window.handleDenoiseLearned(lch, data.threshold ?? data.thresh, data.reduction ?? data.reduc, data.thresholds);
                        }
                        return;
                    }

                    // Handle Room Calibration Response & Progress immediately
                    if (data.action === "room_cal_complete") {
                        if (typeof window.handleRoomCalComplete === "function") {
                            window.handleRoomCalComplete(data.bus, data.bands);
                        }
                        return;
                    }
                    if (data.action === "cal_progress") {
                        if (typeof window.handleRoomCalProgress === "function") {
                            window.handleRoomCalProgress(data.seconds_left, data.bus);
                        }
                        return;
                    }

                    // Latest-Frame Coalescing for 60FPS Meters: discard any unrendered older meter packet in the same VSync!
                    if (_latestMeterPacket && _latestMeterPacket.hw && !data.hw) {
                        data.hw = _latestMeterPacket.hw;
                    }
                    _latestMeterPacket = data;
                    if (_meterRafId === 0) {
                        _meterRafId = requestAnimationFrame(renderLatestMeterFrame);
                    }
                } catch(e) {}
            };

            meterWs.onclose = () => {
                _hostCandidateIdx = (_hostCandidateIdx + 1) % DISCOVERY_HOSTS.length;
                const ind = document.getElementById("live-indicator");
                if (ind) {
                    ind.innerText = "OFFLINE (TAP IP)";
                    ind.style.background = "#8b949e";
                    ind.style.cursor = "pointer";
                    ind.onclick = window.promptPiHostOverride;
                }
                for (let i = 0; i < 8; i++) {
                    const bar = document.getElementById("meter-" + i);
                    if (bar) bar.style.height = "0%";
                    const grBar = document.getElementById("gr-meter-" + i);
                    if (grBar) grBar.style.height = "0%";
                }
                const bL = document.getElementById("meter-master-l");
                const bR = document.getElementById("meter-master-r");
                if (bL) bL.style.height = "0%";
                if (bR) bR.style.height = "0%";
                if (typeof window.renderAoVuMeter === 'function') window.renderAoVuMeter('buster-vu-meter', 0, 'lala');
                setTimeout(connectMeterWs, 1200);
            };
        }

        function resetAllGrMeters() {
            for (let i = 0; i < 8; i++) {
                const el = document.getElementById('gr-meter-' + i);
                if (el) el.style.height = '0%';
            }
            const focusEl = document.getElementById('gr-meter-focused-rack');
            if (focusEl) focusEl.style.height = '0%';
        }
        resetAllGrMeters();

        window.resetPiXruns = function() {
            if (meterWs && meterWs.readyState === WebSocket.OPEN) {
                meterWs.send(JSON.stringify({ action: "reset_xruns" }));
            }
            const xrEl = document.getElementById("pi-tel-xrun");
            if (xrEl) {
                xrEl._lastTxt = "XRUN 0";
                xrEl.innerText = "XRUN 0";
                xrEl.classList.add("ok");
                xrEl.classList.remove("warn");
            }
        };

        window.connectMeterWs = connectMeterWs;
        window.resetAllGrMeters = resetAllGrMeters;
})(window);
