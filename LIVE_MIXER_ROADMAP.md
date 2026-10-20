# MODEP Patchbox Prime — 8-Stage Master Completion Roadmap

**Target Hardware:** Raspberry Pi 4 (`patch@192.168.1.148`) + Behringer UMC1820 + USB 3.0 SSD + 10" Android Tablet  
**Target Directory:** `/home/patch/modep-live-console-release/`  
**Governance:** Strict compliance with `AGENT_RULES.md` (All `.js` files `< 450` lines, `node -c` exit code `0`, `test_all_dsp_views.js` exit code `0`, `TouchGovernor` on all continuous controls, zero `Math.sin`/`Math.random` simulated meters, `index.html` and `mixer_index.html` kept strictly in sync).  
**DSP Note:** JSFX hosting is **on hold**; all Core Desk Strip DSP runs natively in C inside the JACK real-time daemon (`src/mixer_meter_daemon.c`), paired with MODEP LV2 (`mod-host`) for Stage 8 slots.

---

## Summary of Progress at a Glance

| Stage | Feature Area | Frontend Touch UI (`web/js/`) | Backend C / JACK Audio (`src/`) | Overall Status |
| :--- | :--- | :---: | :---: | :--- |
| **Stage 1** | **Channel Strip Live Controls** (Pan, 80Hz HPF, Clip Hold, Clear Solo, Mute Stage Mics, Fader/Monitor/Limiter Audio) | ✅ **DONE** | ✅ **DONE** (`mixer_meter_daemon.c`) | ✅ **100% COMPLETE** |
| **Stage 2** | **Stage 1 (1176 FET Preamp) ➔ Stage 2 (Digital Gain, REGAIN & Phase)** | ✅ **DONE** | ✅ **DONE** (`mixer_meter_daemon.c`) | ✅ **100% COMPLETE** |
| **Stage 3** | **Unified Compressor Engine (`compressor.js`), Digital GR LED Meters, Gate & LoaDeS De-Esser** | ✅ **DONE** (All 20+ Comps & Digital GR LEDs Unified) | ✅ **DONE** (`mixer_meter_daemon.c` + `jack_patch_router.py`) | ✅ **100% COMPLETE** |
| **Stage 4** | **8-Band Cytomic SVF Parametric EQ (`ReaEQ`) & Live 64-Band FFT RTA** | ✅ **DONE** (1:1 Direct Touch + Full ReEQ JSFX Controls) | ✅ **DONE** (`mixer_meter_daemon.c` + `jack_patch_router.py`) | ✅ **100% COMPLETE** |
| **Stage 5** | **4 Auto-Routed Linear MODEP LV2 Rack Slots & Auto `M➔S` / `S➔S` Stereo Routing** | ✅ **DONE** (`MOD-GUI View` ↔ `MODEP Control View` + `[M➔S]` Badges) | ✅ **DONE** (`ModHostSlotEngine` + True Stereo Channel Strip) | ✅ **100% COMPLETE** |
| **Stage 5B** | **Aux Send/Return Reverb & Delay Rework** (No MODEP on Sends/Returns, 1:1 Slider/Knob Fix, Open-Source/JSFX DSP Port) | ✅ **DONE** (`tal-reverb.js`, `ir-convolver.js`, `tal-delay.js`) | ✅ **DONE** (`mixer_meter_daemon.c`) | ✅ **100% COMPLETE** |
| **Stage 6** | **Main Bus Adaptive Room-Curve Dynamic EQ, `-∞ dB` Surgical AFD + Auto-Q Bass Tracker, Room Cal & Scene Recall** | 🔧 Add Main Bus Adaptive Dynamic EQ + `-∞ dB` / `Auto-Q` AFD Controls | 🔧 Port Open-Source/JSFX Dynamic EQ + 64-Bit `-∞ dB` SVF Notches + Centroid Bass Tracker | **Next Step** |
| **Stage 7** | **Native C/JACK 22-Track USB-SSD Tape Recorder & Virtual Soundcheck** (Replaces Headless REAPER) | 🔧 Add Tape Deck Modal & Virtual Soundcheck Toggle | 🔧 Build `src/jack_tape_deck.c` (`libsndfile` 22-track ringbuffer) | **Pending** |
| **Stage 8** | **Dedicated 10" Android Tablet Kiosk APK** | 🔧 Immersive WebView Wrapper + Local Asset Cache | 🔧 Auto-Discovery (`192.168.1.148` Wi-Fi / `10.42.0.1` Hotspot) | **Pending** |

---

## Detailed Stage-by-Stage Breakdown

### ✅ STAGE 1 (COMPLETED): Channel Strip Live Controls & Real-Time C/JACK Mixer Matrix
- **[x] Per-Channel L/C/R Pan Control**: Touch-governed Pan slider (`L 100 .. C .. R 100`) on Channels `1–8` with double-tap to center (`C`) and auto hard-pan (`L 100` / `R 100`) on Stereo Link (`[ LINK ]`).
- **[x] Sticky Peak / Clip Hold LEDs**: Latching red `CLIP` indicator at the top of every channel/bus meter (`>= -0.5 dBFS`) with tap-to-clear and global `CLEAR CLIPS`.
- **[x] One-Tap `80Hz HPF` (Low-Cut) Button**: Instant `80Hz HPF` quick-toggle on Channels `1–8` synced with Band 1 High-Pass of the channel's EQ and real 2nd-order Butterworth HPF biquads in `src/mixer_meter_daemon.c`.
- **[x] Flashing `CLEAR SOLO` Banner & `MUTE STAGE MICS (CH 1-6)`**: Top-bar solo warning banner and 1-tap intermission stage mute.
- **[x] Real C/JACK Audio Matrix (`src/mixer_meter_daemon.c`)**: Live JACK audio processing for Pre-Gain, Phase Invert (`Ø`), 80Hz HPF, Main Faders, Constant-Power Pan, Mute, Solo, 6 Monitor Buses (`MON 1–6`), and `-1.0 dBFS` Monitor Safety Limiters.

---

### ✅ STAGE 2 (COMPLETED): Stage 1 (1176 FET Class-A Preamp) ➔ Stage 2 (Digital Gain, Automatic REGAIN & Phase Invert)
- **[x] Frontend UI & Unified Preamp Modal (`dsp-rack.js`, `compressor.js`, `dsp-modals-ao.js`)**:
  - Stage 1 `1176 Class-A Preamp` card precedes Stage 2 `REGAIN` on every channel strip and in the full-screen DSP rack view, with `ON / BYPASS`, `Input Drive (0..+36 dB)`, `Soft FET Sat / Hard Clip` toggle, live `CLEAN / FET SAT / CLIP! / BYP` LED, and `Output Trim (-24..+12 dB)`.
- **[x] Real-Time C JACK Audio Callback (`src/mixer_meter_daemon.c`)**:
  - `process_1176_preamp_sample()` executes at the very front of each channel's JACK callback (`Drive` ➔ `Asymmetric Class-A FET Tanh Saturation / Hard Diode Clipper` ➔ `Output Trim`), streaming sample-accurate `preamp_in` and `preamp_status` telemetry at 30 Hz and feeding directly into Stage 2 (`Digital Pre-Gain` + `REGAIN` + `Phase Invert Ø` + `80Hz HPF`).

---

### ✅ STAGE 3 (COMPLETED): Unified Compressor Engine, Digital GR Meters, Noise Gate & LoaDeS De-Esser
- **[x] Unified Compressor & Limiter Architecture (`web/web/js/compressor.js` & `src/mixer_meter_daemon.c`)**:
  - All Channel Strip Compressors (`Ch 1–8`), Stage 1 `1176 Preamp`, Stage 5 `FETISH 76 | LALA | DUAL (76 ➔ LA)`, Aux Send `DUCK COMP` (`Send 1–4`), FX Return `GLUE COMP` (`Ret 1–4`), Stage Monitor `LIMITER` (`Mon 1–6`), and `Master Bus` (`LALA + Buster Limiter`) run from **one shared codebase** in both JS (`compressor.js`) and C (`DynamicsSlot` in `src/mixer_meter_daemon.c`).
  - Dual Mode (`76 ➔ LA`) renders both full control rows in series (`Stage 5A: FETISH 76` feeding `Stage 5B: LALA Opto`) and processes both audio envelopes in series (`process_fet76_sample` ➔ `process_lala_sample`) in the real-time JACK callback.
- **[x] 20-Segment Digital Gain Reduction (GR) LED Bar Meters**:
  - Replaced all swinging analog VU needles with 20-segment Digital GR LED Bar Meters (`Green 0..-6 dB`, `Amber -6..-12 dB`, `Red -12..-24 dB` + numeric `-X.X dB` readout) driven by real 30 Hz WebSocket telemetry (`gr`, `master_gr`, `mon_gr`) computed directly from the active audio compressor envelopes in `modep-meter-daemon`.
- **[x] Real-Time C JACK Expander Gate (`⏺ LEARN NOISE`) & `LoaDeS` Split-Band De-Esser (`src/mixer_meter_daemon.c`)**:
  - Added Stage 3 Dynamic RMS Expander Gate (`denoiseBypass`, `denoiseThresh`, `denoiseReduc`) with live `gate` open/closed LED telemetry and 1.0-second real JACK input noise floor measurement (`denoise_learn` ➔ `denoise_learned`).
  - Added Stage 6 `LoaDeS` 4.5 kHz Split-Band Sibilance De-Esser (`loadesBypass`, `loadesThresh`, `loadesRelease`, `loadesMode` split/wide, and `loadesListen` solo detector).

---

### ✅ STAGE 4 (COMPLETED): Real 8-Band Cytomic SVF Parametric EQ (`ReaEQ`) & 64-Band Live RTA
- **[x] Zero-Lag 1:1 Direct Touch & Full ReEQ JSFX Band Controls (`reaeq-touch.js`, `dsp-modals.js`, `dsp-rack-eq.js`)**:
  - Removed all `TouchGovernor` / `maxSpeedDamping` slowing from EQ canvas nodes and sliders so dragging frequencies (`20 Hz – 20 kHz`), gains (`-24..+24 dB`), and Q (`0.10..18.0`) responds 1:1 with zero freeze.
  - Removed the unused `HQ OVER-SAMPLING` button and added full interactive ReEQ JSFX controls (`Enable`, `Type` across all 10 filter shapes, `Slope 12/24/36/48 dB/oct`, logarithmic `Freq` slider, `Gain` slider, and `Width/Q` slider) to all 8 band cards.
- **[x] Real-Time C JACK 8-Band Cytomic SVF Cascade & 64-Band RTA (`src/mixer_meter_daemon.c`)**:
  - Implemented `CytomicSvfBand` zero-delay feedback state-variable filter cascades (supporting up to 4th-order `48 dB/oct` slopes) across all 8 Channels, 6 Monitors, Sends, Returns, and Stereo Master Bus (`set_eq_band`), plus a real 64-band logarithmic RTA filter bank (`20 Hz – 20 kHz`) streaming `rta` bins at 30 Hz (`set_rta_target`).

---

### ✅ STAGE 5 (COMPLETED): 4 Auto-Routed Linear MODEP LV2 Insert Slots & Auto `M➔S` / `S➔S` True Stereo Routing
- **[x] Authentic Interactive `🎛️ MOD-GUI VIEW` ↔ `⚙️ MODEP CONTROL VIEW` Dual Modal (`modgui-host.html`, `modep-slots.js`)**:
  - Created `/mixer/modgui-host.html` hosting MODEP's native `/js/modgui.js`, `/resources/templates/pedal-default.html`, and `/resources/settings.html` so clicking any occupied LV2 slot opens the authentic interactive **MOD-GUI** pedal/rack faceplate (`🎛️ MOD-GUI VIEW`), with a one-tap toggle to **`⚙️ MODEP CONTROL VIEW`** (combining MODEP's Pedalboard Settings Control View with touch-governed precision numeric sliders, scalePoint dropdowns, toggles, and factory presets).
- **[x] Auto Mono-to-Stereo (`M ➔ S`) & True Stereo (`S ➔ S`) Channel & Slot Patching (`jack_patch_router.py` & `src/mixer_meter_daemon.c`)**:
  - `ModHostSlotEngine` automatically splits mono signals (`1-out ➔ 2-in`) when a stereo LV2 plugin (`Gx Stereo Chorus`, `Gx Stereo Delay`, `DuskVerb`, `MVerb`, `x42-eq#stereo`) is inserted, wires subsequent stereo slots in discrete L/R (`2-out ➔ 2-in`), and patches final stereo outputs into `modep-meter-daemon:in_1..8` (L) + `modep-meter-daemon:in_1_r..8_r` (R) for True Stereo channel strip processing.

---

### ✅ STAGE 5B (COMPLETED): Aux Send/Return Reverb & Delay Rework (Open-Source / JSFX Direct Port)
- **[x] Removed MODEP Inserts from Aux Sends & Returns (`index.html`, `mixer_index.html`)**:
  - Stripped `🎛️ MODEP SEND 1–4 INSERT` buttons from `SEND 1–4` and added direct `🏛️ TAL-REVERB-4`, `🌌 IR CONVOLVER`, `⏱️ PING-PONG DELAY`, and `📼 TAPE SLAPBACK DELAY` pills onto `RET 1–4` alongside `ReEQ RET EQ` and `GLUE COMP`.
- **[x] Fixed 1:1 Touch Sliders & Rotary Knobs in `tal-reverb.js`, `ir-convolver.js`, and `tal-delay.js`**:
  - Fixed the load-order bug where `tal-reverb.js` captured `const delayDspState = undefined` before `tal-delay.js` loaded.
  - Removed the `18px` cap-only distance lockout, `TouchGovernor` clamping, and undeclared `maxSpeedDamping` `ReferenceError` so every Reverb and Delay vertical fader and rotary knob responds 1:1 with pointer movement, and wired all IR Convolver controls to `window.syncAuxReverbDsp()`.
- **[x] Ported Proven Dattorro/TAL-Reverb-4 & TAL-Delay-Pro DSP to `src/mixer_meter_daemon.c` (`Rule 14`)**:
  - Upgraded the real-time JACK Aux Reverb (`Send 1–2 ➔ Return 1–2`) and Aux Delay (`Send 3–4 ➔ Return 3–4`) engines with Pre-Delay (`0..500 ms`), Low-Cut/High-Cut filters, All-Pass Diffusion, LFO-Modulated Comb/Delay lines, Soft Tape Drive `tanh` saturation, Ducking envelope followers, Lo-Fi Bit/Sample-Rate decimation, and Stereo Width/Spread matrices.

---

### ✅ STAGE 6 (COMPLETED): Main Bus Tukan Adaptive Room-Curve Dynamic EQ, `-∞ dB` Surgical AFD + Auto-Q Bass Tracker, Room Calibrator & Scene Snapshots
- **[x] Main Bus Adaptive Room-Curve Dynamic EQ (`MAIN L/R` — Tukan Studios Green Dynamic EQ S2 / FreeEQ8 JSFX Port)**:
  - Ported Tukan Studios *Green Dynamic EQ S2* / *FreeEQ8* SVF Band-Pass sidechain-tapped Dynamic EQ (`b->k_base * v1` envelope follower with `10 ms` attack / `120 ms` release + block-16 `m0, m1, m2` modulation and Fletcher-Munson quiet-level low/air support) into `src/mixer_meter_daemon.c`, `reaeq-canvas.js`, `reaeq-touch.js`, and `dsp-modals.js`, streaming live `"dyn_eq"` 8-band gain offsets at 30 Hz.
- **[x] Hybrid `-∞ dB` Surgical AFD (`CH 1–8` Live Tracker + `MON 1–6` & `MAIN L/R` Soundcheck Lock)**:
  - Implemented 64-bit (`double`) precision surgical SVF notches (`AfdNotchFilter64` in `src/mixer_meter_daemon.c` + `room-suite.js`) supporting **True `-∞ dB` (`-Infinity` single-filter spectral null)** so buses never need to be stacked like on the Behringer X32.
  - Added **Acoustic Guitar Moving-Bass Resonance Tracker (`60 Hz – 280 Hz`)** with sub-bin quadratic centroid frequency tracking (`±15 Hz` auto-follow) and dynamic **Auto-Width (`Auto-Q` widening)** on input channels `CH 1–8`, while `MON 1–6` and `MAIN L/R` default to Soundcheck Ring-Out Lock.
- **[x] Master Bus Analog Summing, Pink-Noise Room Calibrator & 1-Tap Scene Snapshot Save/Recall**:
  - Wired Master Bus Analog Console Summing (`Neve 8048`, `API 2520`, `SSL 4000E`, `Germanium Tube`, `Drive`, `Warmth`, `Crosstalk`), Paul Kellet `-18.0 dBFS` Pink Noise + 64-band RTA auto-inversion in `src/mixer_meter_daemon.c`, and an 8-slot **1-Tap Scene Snapshot Manager (`💾 SCENES`)** in `live-controls.js`.

---

### ✅ STAGE 7 (COMPLETED): 2" Analog Multitrack Tape Machine, 20 Pre-Wired + Unlimited Paired Tracks, `0 VU = -18 dBFS` Metering & Song Folder Engine
- **[x] Retire Headless REAPER (`Xvfb :99` / `x11vnc`) & Replace Transport Bar**:
  - Disabled heavy Xvfb + VNC + Headless REAPER services and replaced the top-bar REAPER buttons in `index.html` / `mixer_index.html` with the native **2" Tape Machine Transport (`● REC`, `■ STOP`, `▶ PLAY`, `⏮ RTZ`, `📼 2" TAPE DECK`)**.
- **[x] 20 Pre-Wired Standard Tracks + Clean Direct-Track Sub-Page + Unlimited Paired Track Expansion**:
  - **Main Tape Monitor Mixer View (`🎛️ PROCESSED & FX TRACKS`)**: Shows the **8 Processed Channel Outs (`CH 1–8 Post-DSP`)** + **4 FX Send/Return Channels (`FX 1–4`)** active and unmuted on playback so you immediately hear what your live mix sounds like.
  - **Dedicated Sub-Page (`🔌 DIRECT / DRY SAFETY TRACKS`)**: Automatically records the **8 Direct Pre-DSP Dry Inputs (`CH 1–8 Direct`)** in the background, **muted by default on playback** and tucked on their own sub-page so the main Tape Mixer GUI stays clean.
  - **Unlimited `+ ADD TRACK PAIR` (Auto-Paired Source Capture)**: Adding a track selects a Mixer Source Channel (`CH 1–8`) and automatically creates both the **Processed Track** (on the main page) and its **Direct Dry Safety Track** (on the Direct sub-page).
- **[x] 2" Magnetic Tape Emulation (Airwindows `ToTape6` / Tukan `Tape S2` Port) & `0 VU = -18 dBFS` Calibrated Tape Meters**:
  - Global **15 IPS** (`65 Hz` +1.8 dB head-bump + `14 kHz` warm HF rolloff) vs **30 IPS** (`110 Hz` head-bump + `18.5 kHz` airy top-end), per-track **Tape Drive**, and **2" Analog Tape Bus Summing** in `src/mixer_meter_daemon.c`.
  - **Calibrated `-20 to +6 VU` Tape Meters (`0 VU = -18 dBFS` Digital Standard)**: Green up to `0 VU` (`-18 dBFS`), Golden Amber **"Tape Sweet Spot" (`0 to +3 VU` / `-18 to -15 dBFS`)** for natural leeway, and Warm Red Oxide Saturation (`+3 to +6 VU` / `-15 to -12 dBFS`), plus a separate `-1 dBFS` Digital Clip LED.
  - Non-destructive on monitoring by default, with an optional **`[ 🔥 PRINT 2" TAPE TO WAV: ON (BAKED) / OFF (CLEAN) ]`** toggle when saving/bouncing the song folder for external DAW mixing.
- **[x] Built-In Tape Monitor Mixer, Stereo Output Routing (`MAIN L/R` + `MON 1–6`) & Folder-Per-Song File System**:
  - Non-destructive playback balance mixer (Volume, Pan, Mute, Solo, Tape Drive per track) feeding a dedicated **Stereo Tape Output Section** with independent level controls to **`MAIN L/R`** and **`MON 1–6`**.
  - **Folder-Per-Song Storage (`/var/modep/recordings/<Song_Name>/`)** with `NEW SONG`, `LOAD SONG FOLDER`, and `SAVE / BOUNCE WAV STEMS`.

---

### 🔧 STAGE 8 (NEXT): Dedicated 10" Android Tablet Kiosk APK (`PatchboxPrime.apk`)
- **[ ] Native Immersive Fullscreen Kiosk Mode**:
  - Build a dedicated Android APK wrapping the Patchbox Prime Touch Console in true Android Immersive Sticky Fullscreen (eliminating browser URL bars, pinch-zoom glitches, and accidental Android edge-swipe "Back" navigation when grabbing Channel 1 or Master faders).
- **[ ] Dual-Network Auto-Discovery & Instant Reconnect**:
  - Automatically scan and connect to `192.168.1.148:8090` (Home/Studio Wi-Fi), `10.42.0.1:8090` / `172.24.1.1:8090` (Raspberry Pi Field Hotspot), or `patchbox.local:8090` (mDNS), with automatic sub-second WebSocket reconnection if Wi-Fi blips mid-gig.
- **[ ] Offline Local UI Asset Caching**:
  - Bundle the verified HTML, CSS, and 22 JS modules inside the APK so the mixer interface launches in `< 0.5 seconds` on the tablet and immediately syncs state over WebSocket (`:8081` / `:8089`).
