# PATCHBOX PRIME — MASTER ARCHITECTURAL INDEX & QUICK REFERENCE
**Version:** 3.0 (`Patchbox Prime`)  
**Target Hardware:** Raspberry Pi 4B (4GB/8GB) + Behringer UMC1820 (18-In / 20-Out USB Audio Interface)  
**Target Host & Directory:** `patch@192.168.1.148:/home/patch/modep-live-console-release`  
**Memory & Context Budget:** Strictly maintained between 300 and 400 lines as the canonical single-file engineering reference.

---

## 1. EXECUTIVE SUMMARY: THE "WHYS AND WHEREFORES" OF PATCHBOX PRIME

**Patchbox Prime** is a purpose-built, ultra-low-latency live digital mixing console, multi-FX rack, and DAW workstation running on Patchbox OS (Raspberry Pi 4). It unifies four systems that traditionally require separate hardware or incompatible software workflows:
1. **An 8-Channel / 18-Input Touch-First Live Digital FOH & Monitor Console** with Sends-on-Fader (SOF), 6 Stage Monitor mixes, 4 Stereo FX Send/Return buses, and Dugan-style Automatic Mic Mixing (AMM).
2. **A Permanent Studio-Grade Core Desk DSP Strip on Every Channel** powered by a native C/C++ JACK DSP engine with **Headless `ysfx` (JSFX) Hosting** (`@init/@slider/@sample` compiled via ARM64 JIT, controlled 100% by the custom Touch UI + `TouchGovernor` without desktop `@gfx` windows): 1176 FET Preamp, Constant-Level REGAIN, Adaptive De-Noiser Gate, 8-Band ReaEQ + 64-Band RTA, Unified Switchable `FETISH 76 | LALA | DUAL (76 ➔ LA)` Compressor with Digital GR LED meters, and LoaDeS De-Esser.
3. **Dual-Mode MODEP Effect Insertion per Channel** giving the live engineer an instant choice between:
   - **Mode A — 4 Auto-Routed Linear LV2 / MOD-GUI Slots (`RACK SLOTS`)**: 4 series insert slots that load individual LV2 modules from `/var/modep/lv2` (`429+` plugins) with automatic thru-bridging so live audio **never** drops out during mid-song effect changes.
   - **Mode B — Full Visual Pedalboard Canvas (`CANVAS`)**: Routes the channel through a full multi-pedalboard MODEP canvas (`:8881`–`:8884`) when complex parallel amp/cab/multi-FX chains are needed.
4. **Custom C/JACK 22-Track USB-SSD Tape Recorder (`modep-tape-deck`) & 10" Android Tablet Kiosk APK**: Replaces the heavy Headless REAPER (`Xvfb`/`x11vnc`) stack with a lock-free `libsndfile` multitrack stem recorder (8 Dry Inputs + 8 Post-Strip Wet Stems + 4 FX Returns + 2 Master L/R) with 1-Click Virtual Soundcheck, paired with an immersive fullscreen Android APK that auto-discovers Studio Wi-Fi (`192.168.1.148`) and Pi Hotspot (`10.42.0.1`).

### Why These Specific Architectural Decisions Were Made

#### A. Why Stage 1 (1176 FET Preamp Emulation) Precedes Stage 2 (Digital Gain & REGAIN)
In earlier iterations, the console's `REGAIN` feature (which automatically counter-trims the channel fader when input gain is adjusted) replaced the input preamp stage. In **Patchbox Prime**, they are separated into two distinct, sequential stages:
- **Stage 1: 1176 Class-A FET Preamp Emulation** (`Input Drive 0 to +36 dB` ➔ `Simulated FET Soft Saturation / Hard Diode Clipping` ➔ `Output Trim -24 to +12 dB`). This stage is purely for **tone, harmonic saturation, transient rounding, and deliberate FET drive**. Its `CLEAN / FET SAT / CLIP!` indicator is driven strictly by real-time JACK input telemetry plus applied drive.
- **Stage 2: Digital Gain + Automatic REGAIN + Polarity Invert (`Ø`)**. Positioned immediately after the 1176 Preamp Output Trim, Stage 2 lets the FOH engineer adjust the clean digital operating level entering the rest of the channel strip while `REGAIN` automatically adjusts the main fader in the opposite direction (`-deltaGain dB`) so FOH loudness stays constant during gain staging.

#### B. Why the Core Desk DSP Strip Is Permanent & Independent of MODEP Mode
Live sound engineers cannot afford to lose their channel high-pass filter, 8-band parametric EQ, noise gate, or vocal compressor simply because they switched a channel from insert slots to a guitar pedalboard canvas. Therefore:
- The **Core Desk DSP Strip (Stages 1 through 7)** runs **permanently on every channel at all times**.
- Switching between `RACK SLOTS` and `CANVAS` on a channel strip **only** changes **Stage 8 (the Post-Core MODEP Insert Router)**.

#### C. Why the 4 Linear MODEP LV2 Slots Exist Alongside the Visual Pedalboard Canvas
The native MODEP visual pedalboard (`mod-ui` on `:8881`) is powerful for pre-built guitar/synth rigs, but it requires manual virtual cable dragging. Unplugging or rewiring a node on the visual canvas in the middle of a live gig causes an immediate audio dropout and is error-prone on a touchscreen.
- **The 4 Auto-Routed Linear LV2 Slots (`#slot-${ch}-0` to `#slot-${ch}-3`) solve this live-gig hazard**:
  1. Audio automatically flows in series: `Core Strip Out ➔ Slot 1 ➔ Slot 2 ➔ Slot 3 ➔ Slot 4 ➔ Channel Fader`.
  2. Any slot set to `EMPTY (THRU)` or `BYPASSED (THRU)` is **automatically bridged in software** with zero signal interruption.
  3. Clicking an empty slot (or tapping `⇄`) opens the categorized **LV2 Module Picker** (populated directly from `/effect/list` across `AMP / NAM`, `DRIVE`, `MODULATION`, `DELAY`, `REVERB`, `DYNAMICS`, `EQ / FILTER`).
  4. Clicking an occupied slot opens the **Direct MOD-GUI / LV2 Parameter Modal** (`#slot-modgui-overlay`), which fetches `/effect/get?uri=<URI>`, displays the authentic MOD-GUI faceplate (`/effect/image/screenshot.png`), preset selector, and touch-governed rotary sliders for every `ports.control.input` parameter—all without leaving the mixer screen.

---

## 2. HARDWARE, OS & NETWORK SERVICE TOPOLOGY

### Hardware & JACK2 Real-Time Configuration
- **Host SBC:** Raspberry Pi 4 Model B (`ARM64 / aarch64`, Patchbox OS / Debian Bookworm)
- **Audio Interface:** Behringer UMC1820 USB 2.0 (`18 Capture Channels`, `20 Playback Channels`)
  - `system:capture_1..8`: Analog XLR/TRS Mic/Line Inputs 1–8 (Default mapped to Console Channels 1–8; soft-patchable across `HW 1..18`).
  - `system:capture_9..10`: Coaxial S/PDIF Stereo Digital Input.
  - `system:capture_11..18`: Optical ADAT Inputs (8 channels).
  - `system:playback_1..2`: Main FOH PA Left / Right (`MASTER L/R`).
  - `system:playback_3..8`: Stage Monitors 1–6 (`MON 1` Wedge L, `MON 2` Wedge R, `MON 3` IEM 1, `MON 4` IEM 2, `MON 5` Drum Fill, `MON 6` Side Fill).
  - `system:playback_9..12`: External Hardware FX Send/Return Loops.
- **JACK2 Engine Parameters:** `48,000 Hz` Sample Rate | `128 frames/period` | `2 periods/buffer` (`5.33 ms` round-trip latency).

### Network Ports & Active Services (`192.168.1.148` / `10.42.0.1`)
| Port / Endpoint | Service / Daemon | Purpose in Patchbox Prime |
| :--- | :--- | :--- |
| `http://192.168.1.148:8090/` | `jack_patch_router.py` HTTP/WS | Serves `/home/patch/modep-live-console-release/web/` & bridges JACK/MODEP APIs |
| `ws://192.168.1.148:8081/meters` | Primary Telemetry WebSocket | Streams 30 Hz JSON frames (`ch`, `master`, `aux`, `gr`, `master_gr`, `mon_gr`) & DSP commands |
| `ws://192.168.1.148:8089/` | Native C JACK Daemon (`modep-meter-daemon`) | Real-time C JACK audio processor (`src/mixer_meter_daemon.c`) & Peak/GR telemetry source |
| `http://192.168.1.148:8881/` | Primary MODEP `mod-ui` Canvas | Active visual pedalboard canvas (`Rack 1 / Rack 2` consolidated on `:8881`) |
| `http://192.168.1.148:8883..8884/` | Secondary MODEP Canvases | Optional Aux/Vocal & Synth multi-FX instances |

---

## 3. COMPLETE AUDIO SIGNAL FLOW (INPUT TO OUTPUT)

```
[UMC1820 Physical Input (HW 1..18 via Soft-Patch) OR Virtual Soundcheck Dry Track 1..8]
       │
       ▼
┌─────────────────────────────────────────────────────────────────────────┐
│ PERMANENT CORE DESK DSP STRIP (Active in both RACK SLOTS & CANVAS mode) │
│                                                                         │
│  STAGE 1: 1176 FET Class-A Preamp Emulation                             │
│           ├─ Input Drive (0.0 to +36.0 dB)                              │
│           ├─ Saturation Curve (Soft FET Tanh Sat vs. Hard Diode Clip)   │
│           ├─ Live Clip Telemetry LED (CLEAN / FET SAT / CLIP!)          │
│           └─ Output Trim (-24.0 to +12.0 dB)                            │
│                    │                                                    │
│  STAGE 2: Digital Gain, Polarity Invert (Ø), 80Hz HPF & REGAIN          │
│           ├─ Digital Pre-Gain (-18.0 to +24.0 dB) + 80Hz Biquad HPF     │
│           ├─ Phase Invert (0° Normal / 180° Inverted) + L/C/R Pan       │
│           └─ REGAIN Compensator (Auto-trims Fader by -ΔGain dB)         │
│                    │                                                    │
│  STAGE 3: Adaptive Input De-Noiser & Expander Gate                      │
│           ├─ Threshold (-70 to -10 dB), Max Reduction (-60 to 0 dB)     │
│           ├─ Sidechain HPF (20–400 Hz) & Automated Noise Floor Learn    │
│                    │                                                    │
│  STAGE 4: Cockos ReaEQ 8-Band Parametric EQ + 64-Band Live RTA          │
│           ├─ B1 Low-Cut, B2 Low-Shelf, B3–B6 Peaking, B7 Hi-Shelf, B8   │
│           └─ Cytomic SVF zero-delay feedback math + hardware DPI canvas │
│                    │                                                    │
│  STAGE 5/6: Unified Compressor [FETISH 76 | LALA | DUAL (76 ➔ LA)]      │
│           ├─ Single unified engine (compressor.js) across all strips    │
│           └─ 20-Segment Digital Gain Reduction (GR) LED Bar Meters      │
│                    │                                                    │
│  STAGE 7: LoaDeS High-Frequency De-Esser (3–12 kHz Split-Band)          │
│           └─ Selectable PRE / POST Direct Send Tap                      │
└─────────────────────────────────────────────────────────────────────────┘
       │
       ▼
┌─────────────────────────────────────────────────────────────────────────┐
│ STAGE 8: DUAL-MODE MODEP INSERT ROUTER (Selectable per Channel)         │
│                                                                         │
│  [MODE A: RACK SLOTS (Default)]                                         │
│    Auto-Routed Linear Series Chain (Never cuts audio when empty/byp):   │
│    Core Out ➔ [Slot 1 LV2] ➔ [Slot 2 LV2] ➔ [Slot 3 LV2] ➔ [Slot 4 LV2] │
│                                                                         │
│  [MODE B: MODULAR CANVAS]                                               │
│    Routes through assigned MODEP Visual Pedalboard (:8881/:8883/:8884)  │
│    with instant "CANVAS BYPASS" hard-bypass safety switch.              │
└─────────────────────────────────────────────────────────────────────────┘
       │
       ▼
┌─────────────────────────────────────────────────────────────────────────┐
│ STAGE 9: CHANNEL FADER, SENDS-ON-FADER (SOF), AMM DUGAN & BUS ROUTING   │
│  ├─ Aux Sends 1–4 (with Ducking Comp) ➔ [TAL-Reverb-4 / Atlantis IR /   │
│  │    Ping-Pong & Tape Delays] ➔ Stereo FX Returns 1–4 (with Glue Comp) │
│  ├─ Stage Monitor Sends 1–6 ➔ [ReaEQ + -1.0 dBFS Buster Safety Limiter] │
│  └─ Main Fader + Pan + Solo/Mute + Dugan AMM ➔ MASTER L/R BUS           │
│       └─ [Analog Console Summing ➔ Master ReaEQ ➔ 12-Notch AFS          │
│           ➔ RTA Auto-EQ Room Calibrator ➔ LALA + Buster Limiter]        │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 4. COMPLETE CODEBASE & MODULE INDEX (`< 450` LINES PER JS FILE)

Every JavaScript file under `web/js/` is strictly capped below **450 lines** in compliance with `AGENT_RULES.md` Constraint 1, and validated with `node -c` and `test_all_dsp_views.js` (exit code `0`).

### Frontend HTML & CSS (`web/web/`)
| File Path | Lines | Architectural Role |
| :--- | :--- | :--- |
| `web/web/index.html` | ~1,855 | Primary console DOM: `#dsp-rack-page`, 8 Input Strips (`#ch-0..7`), 4 FX Sends/Returns, 6 Monitors, Master Strip, Modals |
| `web/web/mixer_index.html` | ~1,855 | Synchronized mirror of `index.html` (Rule 9: always updated alongside `index.html`) |
| `web/web/css/mixer.css` | ~2,710 | Complete dark-console stylesheet, hardware fader grooves, 1176 preamp strip, 4-slot LV2 cards, and modal layouts |

### Modular JavaScript Engine (`web/web/js/` — 22 Modules, All `< 450` Lines)
| Module File | Lines | Key Exports (`window.*`) & Responsibilities |
| :--- | :--- | :--- |
| `touch-governor.js` | 30 | `window.touchGovernor.process(id, val)` — Slew-rate limiter preventing accidental touchscreen fader/knob jumps |
| `reaper-transport.js` | 30 | Transport bar bindings (`REC`, `STOP`, `PLAY`) transitioning to `modep-tape-deck` 22-track recorder |
| `state.js` | 88 | Initializes `PatchboxState`, `channelInputGains`, `channelRegainActive`, `faderValues`, `stereoLinkState`, `reaeqBands` |
| `config.js` | 99 | `window.channelNames`, hardware channel defaults, 12 console color swatches, default EQ band frequencies |
| `app.js` | 123 | DOM bootstrapper, `openRootPedalboard()`, `openSuperInsert(ch)`, keyboard shortcuts, and initial view setup |
| `room-suite.js` | 132 | `startRoomCal`, `commitRoomCalToBus`, RTA pink-noise room calibration wizard |
| `strip-controls.js` | 133 | `toggleChHpf`, `handlePanChange`, `resetPanCenter`, `togglePrePostTap`, `triggerNoiseLearn` |
| `room-afd.js` | 145 | `getAfdChannel`, `renderAfdSpectrum`, `toggleLockFixedNotch`, `panicClearAfdFilters` (12-Notch AFS) |
| `dsp-rack-eq.js` | 164 | `buildRackRowsHtml`, `renderMiniReEq`, `selectRackEqBand`, `updateRackEqParam` |
| `tal-reverb.js` | 183 | `renderReverbControls`, `switchReverbEngine`, `initTalRotaries`, `drawIrWaveform` (`TAL-Reverb-4` + `Atlantis IR`) |
| `dsp-modals.js` | 190 | `openDspModal(type, chId)`, `closeDspModal()` — Modal dispatcher routing compressor/limiter types to `compressor.js` |
| `dsp-modals-ao.js` | 217 | Deep modal views for `denoiser`, `loades`, and Analog Console Summing |
| `reaeq-touch.js` | 222 | `bindReaEQTouchCanvas(ch)` — Multi-touch pointer drag & pinch-Q handler for the 8 ReaEQ parametric nodes |
| `ao-vu-knobs.js` | 252 | Telemetry-driven rotary knob renderer (`bindAoRotary`, `drawAoKnob`) |
| `reaeq-canvas.js` | 256 | `renderReaEQGraph(ch)` — Cytomic SVF biquad magnitude response plotter + 64-band live RTA spectrum underlay |
| `dsp-rack-views.js` | 266 | `updateFocusedChannelStrip(ch)`, `startFocusedFaderDrag(evt, ch)` |
| `tal-delay.js` | 272 | `renderDelayControls`, `initTalDelayRotaries` (Send 3 Ping-Pong + Send 4 Smooth Tape Delay) |
| `touch-physics.js` | 283 | `setFaderPosition`, `updateFaderSpeed`, `applySlowModeToAllSliders`, pointer capture physics for all vertical faders |
| `telemetry.js` | 290 | `connectMeterWs()` — Connects to `:8081/meters` & `:8089`, drives peak bars, 20-segment digital GR LEDs, and 64-band RTA |
| `dsp-rack.js` | 309 | `showDspRackPage`, `setChannelCompModel`, `handle1176PreampDrive/Trim`, `toggle1176ClipMode` |
| `compressor.js` | 387 | **Unified Compressor Engine**: `getCompState`, `setCompParam`, `renderDigitalGrMeterHtml`, `updateDigitalGrMeter`, `buildPreamp1176CardHtml`, `buildCompSlotHtml`, `renderCompModal` |
| `channel-strip.js` | 415 | `openRenameModal`, `openColorPalette`, `handleTapTempo`, `switchMixerView`, `toggleSofMode`, `handleInputGainChange`, `toggleStereoLink` |
| `modep-slots.js` | 415 | `setChannelDspMode`, `handleSlotClick`, `openPluginPickerModal`, `assignSlotPlugin`, `openSlotModGuiModal`, `syncChannelLinearChain` |


### Backend C/C++ Real-Time JACK DSP & Telemetry Engine (`src/`)
| File Path | Architectural Role |
| :--- | :--- |
| `src/mixer_meter_daemon.c` | Active real-time C JACK audio processor (`/usr/local/bin/modep-meter-daemon`, port `:8089`) computing Pre-Gain, Phase, 80Hz Biquad HPF, Faders, Pan, Mute/Solo, 6 Monitor Buses + `-1.0 dBFS` Limiters, and 30 Hz Peak/GR LED telemetry |
| `src/main.cpp` & `src/Biquad.h` | C++ JACK2 / Cytomic SVF filter library and `libwebsockets` reference implementation |
| `src/jack_tape_deck.c` *(Planned)* | Custom 22-track `libsndfile` USB-SSD multitrack WAV recorder & 1-click Virtual Soundcheck player replacing Headless REAPER |

---

## 5. PROTOCOL & API QUICK REFERENCE

### A. MODEP LV2 / MOD-GUI REST Endpoints (Port `80` / `8090` -> `mod-ui`)
1. **List Installed LV2 Modules (`429+` plugins in `/var/modep/lv2`)**:
   - `GET /effect/list` ➔ Returns JSON array of plugin descriptors (`uri`, `name`, `brand`, `label`, `category`, `ports`, `presets`).
2. **Inspect Single LV2 Module & MOD-GUI Ports**:
   - `GET /effect/get?uri=<ENCODED_URI>` ➔ Returns full `ports.control.input` array (`symbol`, `name`, `ranges.minimum`, `ranges.maximum`, `ranges.default`, `units.symbol`) and `presets`.
3. **Fetch Authentic MOD-GUI Faceplate Graphics**:
   - `GET /effect/image/screenshot.png?uri=<ENCODED_URI>`
   - `GET /effect/image/thumbnail.png?uri=<ENCODED_URI>`
4. **Instantiate / Remove / Parameter-Set for 4-Slot Auto-Routed Chain**:
   - Instantiate slot module: `GET /effect/add/graph/ch_<1..8>_slot_<1..4>?uri=<ENCODED_URI>&x=<X>&y=<Y>`
   - Remove bypassed/empty module: `GET /effect/remove/graph/ch_<1..8>_slot_<1..4>`
   - Set LV2 control port value: `POST /effect/parameter/set//graph/ch_<1..8>_slot_<1..4>/:<symbol>` (`body: value=<float>`)
   - Load LV2 preset: `GET /effect/preset/load/graph/ch_<1..8>_slot_<1..4>?uri=<PRESET_URI>`

### B. WebSocket Real-Time Telemetry & Control (`ws://<host>:8081/meters` & `:8089`)
- **Incoming Telemetry Frame (Server ➔ Browser at 30 Hz)**:
  ```json
  {
    "type": "meters",
    "ch": [0.12, 0.45, 0.0, 0.08, 0.22, 0.0, 0.0, 0.01],
    "master": [0.38, 0.36],
    "aux": [0.15, 0.14, 0.20, 0.18, 0.10, 0.05],
    "gr": [0.0, 4.2, 0.0, 1.5, 6.8, 0.0, 0.0, 0.0],
    "master_gr": 1.2,
    "mon_gr": [0.0, 0.0, 0.5, 0.0, 0.0, 0.0],
    "rta": [-72.0, -68.4, -55.1, "... 64 floats in dBFS ..."],
    "gate": [1, 1, 0, 1, 1, 0, 0, 0]
  }
  ```
- **Outgoing Control Frame (Browser ➔ Server)**:
  ```json
  {
    "cmd": "comp",
    "id": "0",
    "model": "dual",
    "param": "fetInput",
    "val": 6.5
  }
  ```

---

## 6. MANDATORY ENGINEERING INVARIANTS (`AGENT_RULES.md`)

1. **Rule 1 — Zero Tolerance for Simulated Meter Physics**:
   - Never use `Math.sin()`, `Math.random()`, or synthetic `setInterval` oscillators to animate audio meters, RTA bars, or compressor GR bars.
   - All meters (`#meter-0..7`, `#gr-meter-0..7`, `#gr-leds-*`, `#strip-1176-clip-0..7`) are driven strictly by WebSocket telemetry or rest at `0%` / `0.0 dB` when offline.
2. **Rule 2 — Refactor & Replace (No Ghost Markup)**:
   - Never hide obsolete UI components with `display: none` while appending duplicate structures. Replace obsolete DOM cleanly.
3. **Rule 3 — Hardware Touch Governor Enforcement**:
   - Every continuous parameter handler (Faders, 1176 Drive/Trim, REGAIN, ReaEQ nodes, Compressor rotaries, LV2 slot sliders) must pass through `window.touchGovernor.process(controlId, rawValue)` before mutating state or dispatching WebSocket messages.
4. **Rule 4 — High-DPI Canvas Scaling**:
   - All `<canvas>` renderers (`renderReaEQGraph`, `renderMiniReEq`, `drawAoKnob`, `renderAfdSpectrum`, `drawRoomCalCurve`, `drawIrWaveform`) must scale by `const dpr = window.devicePixelRatio || 1`.
5. **Rules 7, 8 & 9 — Feature-Boundary Modules (`< 450` Lines), Runtime Verification & Entrypoint Sync**:
   - Every file in `web/web/js/*.js` must remain strictly `< 450` lines, pass `node -c`, and pass `test_all_dsp_views.js` (`0` errors).
   - Always keep `index.html` and `mixer_index.html` identical locally and on `/home/patch/modep-live-console-release/web/`.

---

## 7. COMPLETE DOM ID & EVENT HANDLER QUICK-LOOKUP MAP

### A. Global Header & Navigation Bar (`index.html`)
| DOM ID | Element Type | Bound Event Handler | Description |
| :--- | :--- | :--- | :--- |
| `#live-indicator` | `<span class="badge-live">` | Driven by `telemetry.js` | Displays `LIVE JACK SYNC` (green) or `OFFLINE` (grey) |
| `#btn-tap-tempo` | `<button>` | `handleTapTempo()` | Global master tap tempo (`30.0`–`300.0 BPM`, syncs TAL-Delay-Pro) |
| `#tempo-bpm-val` | `<span>` | `setGlobalBpm(bpm)` | Numeric BPM readout |
| `#btn-amm-master` | `<button>` | `toggleGlobalAmm()` | Master Dugan-style Automatic Mic Mixer enable/bypass |
| `#btn-fb-master` | `<button>` | `openDspModal('feedback', 'master')` | Opens Master 12-Notch AFS Feedback Suppressor modal |
| `#btn-room-cal-header` | `<button>` | `openDspModal('roomcal', 'master')` | Opens RTA Auto-EQ Room Calibrator modal |
| `#btn-analog-summing` | `<button>` | `openDspModal('summing', 'master')` | Opens Neve 8048 Analog Console Summing & Transformer Drive modal |
| `#btn-reaper-rec/stop/play`| `<button>` | `reaper-transport.js` | Transport bar (`REC`, `STOP`, `PLAY`) for Multitrack Tape Deck |
| `#tab-btn-inputs/sends/monitors`| `<button>` | `switchMixerView(view)` | Switches main mixer deck between Inputs 1–8, FX Sends/Returns, and Monitors 1–6 |
| `#sof-btn-mon1..6 / fx1..4`| `<button>` | `toggleSofMode(target, title)` | Flips Channels 1–8 faders into Monitor 1–6 or FX Send 1–4 levels |
| `#fader-damping-slider` | `<input type="range">` | `updateFaderSpeed(this.value)` | Adjusts `TouchGovernor` anti-jump damping (`1` Raw to `10` Heavy) |
| `#btn-fx-mute-momentary`| `<button>` | `startMomentaryFxMute` / `stopMomentaryFxMute` | Hold-to-mute panic switch for between-song FX tails |

### B. Per-Channel Strip DOM Structure (`ch = 0..7` corresponding to `CH 1..8`)
| DOM ID Pattern | Stage / Section | Bound Event Handler | Purpose |
| :--- | :--- | :--- | :--- |
| `#ch-badge-${ch}` / `#ch-name-${ch}` | Header | `openColorPalette` / `openRenameModal` | 12-swatch scribble-strip color picker & channel rename modal |
| `#ch-input-source-${ch}`| Soft-Patch | `handleSoftPatchChange(${ch}, hw)` | Routes physical UMC1820 input `system:capture_1..18` to channel |
| `#strip-1176-drive/trim-${ch}`| Stage 1 (1176 Pre) | `handle1176PreampDrive/Trim` | 1176 Class-A FET input drive (`0..+36 dB`) & output trim (`-24..+12 dB`) |
| `#phase-ch-${ch}` / `#hpf-ch-${ch}` | Stage 2 | `toggleChPhase` / `toggleChHpf` | 180° polarity inversion (`Ø`) & 80Hz 12dB/oct High-Pass Filter |
| `#gain-slider-${ch}` / `#regain-ch-${ch}` | Stage 2 (Gain) | `handleGainChange` / `toggleChRegain` | Digital input gain (`-18..+24 dB`) coupled to automatic `REGAIN` |
| `#pan-slider-${ch}` | Stage 2 (Pan) | `handlePanChange` / `resetPanCenter` | Constant-power L/C/R stereo pan slider (double-tap for Center) |
| `#btn-mode-rack/canvas-${ch}`| Stage 8 Mode Bar | `setChannelDspMode(${ch}, mode)` | Switches Stage 8 between 4 Linear LV2 Slots (`rack`) & Visual Canvas (`canvas`) |
| `#slot-${ch}-${0..3}` | Stage 8 Rack Mode | `handleSlotClick(${ch}, ${idx})` | Opens LV2 Picker if empty, or MOD-GUI Parameter Modal if occupied |
| `#meter-${ch}` / `#gr-meter-${ch}` | Fader Section | Driven by `telemetry.js` | True JACK peak bar (`-60..+6 dBFS`) & compressor GR bar (`0..-24 dB`) |
| `#fader-track-${ch}` | Fader Section | `touch-physics.js` pointer capture | High-resolution logarithmic channel fader track (cap-only drag) |
| `#amm-ch-${ch}` / `#solo-ch-${ch}` / `#mute-ch-${ch}` | Strip Footer | `toggleChAmm` / `toggleChSolo` / `toggleChMute` | Dugan AMM group, PFL/AFL Solo, and hard Mute |

### C. Full-Screen Core Desk DSP View (`#dsp-rack-page`) & Overlay Modals
| Container ID | Open Function | Close Function | Contents |
| :--- | :--- | :--- | :--- |
| `#dsp-rack-page` | `showDspRackPage(ch)` | `closeDspRackPage()` | Full-page Core Desk Strip (Stages 1–7) + focused left fader strip |
| `#dsp-modal-overlay` | `openDspModal(type, chId)` | `closeDspModal()` | Deep editor for `preamp1176`, `denoiser`, `reaeq`, `fet1176`, `la2a`, `dual`, `comp`, `limiter`, `loades`, `delay`, `reverb`, `feedback`, `roomcal`, `summing` |
| `#plugin-picker-overlay`| `openPluginPickerModal(ch, idx)`| `closePluginPickerModal()`| Searchable, categorized browser for all 429+ MODEP LV2 modules |
| `#slot-modgui-overlay` | `openSlotModGuiModal(ch, idx)` | `closeSlotModGuiModal()` | Direct MOD-GUI faceplate, preset loader, and LV2 parameter controls |
| `#modep-canvas-overlay` | `openModepCanvasModal(ch)` | `closeModepCanvasModal()` | Embedded `:8881` MODEP pedalboard canvas iframe |

---

## 8. DEFAULT 8-CHANNEL CONSOLE PATCHBAY & BUS MATRIX

| Console Strip | Default Name | Default HW Patch | Default Comp Model | Primary Live Role |
| :--- | :--- | :--- | :--- | :--- |
| **CH 1 (`ch-0`)** | `Vocal 1` | `HW 1` (`capture_1`) | `FETISH 76` | Lead Vocal (1176 Preamp warmth + FET 76 peak control + LoaDeS) |
| **CH 2 (`ch-1`)** | `Guitar` | `HW 2` (`capture_2`) | `LALA OPTO` | Electric/Acoustic Guitar (4 LV2 Slots or `:8881` NAM Canvas) |
| **CH 3 (`ch-2`)** | `Bass DI` | `HW 3` (`capture_3`) | `FETISH 76` | Bass Guitar DI (1176 FET drive + optical/FET leveling) |
| **CH 4 (`ch-3`)** | `Snare / Kit`| `HW 4` (`capture_4`) | `FETISH 76` | Snare / Percussion (Fast FET transient limiting + Gate) |
| **CH 5 (`ch-4`)** | `Keys L` | `HW 5` (`capture_5`) | `LALA OPTO` | Stereo Keys Left (Linkable with CH 6 via `[ LINK ]`) |
| **CH 6 (`ch-5`)** | `Keys R` | `HW 6` (`capture_6`) | `LALA OPTO` | Stereo Keys Right (Linked Stereo Pair with CH 5) |
| **CH 7 (`ch-6`)** | `Synth / Backing`| `HW 7` (`capture_7`)| `LALA OPTO` | Backing Tracks / Synth Stem |
| **CH 8 (`ch-7`)** | `Vocal 2 / RTA` | `HW 8` (`capture_8`)| `FETISH 76` | Backing Vocal or RTA Reference Measurement Mic |

---

## 9. 8-STAGE MASTER ROADMAP (`LIVE_MIXER_ROADMAP.md`) & PI DEPLOYMENT RUNBOOK

### Current Status Across the Master Roadmap (JSFX On Hold — Native C/JACK DSP Active)
- **✅ Stage 1 (100% Complete)**: Channel Strip Live Controls (`L/C/R Pan`, `80Hz HPF`, Sticky `CLIP` hold, `CLEAR SOLO`, `MUTE STAGE MICS`) + Real C/JACK Audio Matrix (`src/mixer_meter_daemon.c`) for Pre-Gain, Phase, HPF, Faders, Pan, Mute/Solo, 6 Monitor Buses, and `-1.0 dBFS` Limiters.
- **✅ Stage 2 (100% Complete)**: Stage 1 `1176 FET Preamp` (`ON/BYP`, `Drive 0..+36 dB`, `Soft FET Asymmetric Tanh Sat / Hard Diode Clip`, `Output Trim -24..+12 dB`, live `preamp_in` & `preamp_status` LED) feeding Stage 2 `Digital Pre-Gain + REGAIN + Phase Invert Ø` in `src/mixer_meter_daemon.c`.
- **✅ Stage 3 (100% Complete)**: Unified Compressor Engine (`web/web/js/compressor.js` & `DynamicsSlot` in `src/mixer_meter_daemon.c`) across all Channels, 1176 Preamp, `FET 76 | LALA | DUAL | BUSTER`, Sends, Returns, Monitors, and Master + 20-Segment Digital GR LED Meters + Real-Time C JACK Expander Gate (`⏺ LEARN NOISE`) & `LoaDeS` 4.5 kHz Split-Band De-Esser.
- **✅ Stage 4 (100% Complete)**: 8-Band Cytomic SVF Parametric EQ (`ReaEQ` JSFX parity: all 10 filter shapes, `12/24/36/48 dB/oct` slopes, interactive Freq/Gain/Q sliders, 1:1 unthrottled touch dragging without `TouchGovernor` slowing, no oversampling button) + Live 64-Band RTA filter bank in `src/mixer_meter_daemon.c`.
- **✅ Stage 5 (100% Complete)**: 4 Auto-Routed Linear MODEP LV2 Slots with Dual `🎛️ MOD-GUI VIEW` ↔ `⚙️ MODEP CONTROL VIEW` (`modgui-host.html`, `modep-slots.js`) bridged to `mod-host` (`ModHostSlotEngine` in `jack_patch_router.py`) + **Auto Mono-to-Stereo (`M ➔ S`) & True Stereo (`S ➔ S`) Channel & Slot Routing** (`modep-meter-daemon:in_1..8` & `in_1_r..8_r`).
- **🔧 Stage 5B (Next — Aux Send/Return Reverb & Delay Rework)**: Remove `MODEP SEND 1–4 INSERT` buttons from Aux Send/Return strips so they only host dedicated Reverb/Delay + ReEQ + Ducking Comp; fix all faders & rotary knobs in `tal-reverb.js`, `ir-convolver.js`, and `tal-delay.js` for 1:1 touch dragging (remove `18px` cap lockout, `TouchGovernor` clamping, undefined `delayDspState` capture, and `maxSpeedDamping` errors); port proven open-source/JSFX Reverb & Delay DSP to `src/mixer_meter_daemon.c` for `Send 1–4 ➔ Return 1–4`.
- **🔧 Stage 6 (Main Bus Adaptive Room-Curve Dynamic EQ, Per-Channel + Bus `-∞ dB` Surgical AFD, Auto-Q Bass Tracker, Room Cal & Scene Recall)**:
  - **Main Bus Adaptive Room-Curve Dynamic EQ (`MAIN L/R`)**: Ported directly from an open-source / JSFX Dynamic EQ script and locked to the Room Calibration Target Curve to keep the room's tonal curve flat across **both quiet and loud volumes** (dynamically ducking room resonances that bloom at high SPL and supporting low-end/air contour at low SPL).
  - **Hybrid AFD (`CH 1–8` Live Tracker + `MON 1–6` & `MAIN L/R` Soundcheck Lock)**: 64-bit (`double`) surgical SVF notches (`Q` up to `100+` for transparent neighboring tone) with **True `-∞ dB` (`-Infinity` single-filter spectral null)** so buses never need to be stacked like on the X32.
  - **Acoustic Guitar Moving-Bass Tracker (`60 Hz – 280 Hz`)**: Sub-bin centroid frequency tracking (`±15 Hz` auto-follow) + dynamic **Auto-Width (`Auto-Q`)** that automatically widens/narrows based on bass frequency drift during a gig.
  - **Master Analog Summing (`Neve 8048`), Pink-Noise Room Calibrator, `MON 1–6` 8-Band EQ + `-1.0 dBFS` Safety Limiters & 1-Tap Scene Snapshot Save/Recall**.
- **🆕 Stage 7 (Native C/JACK USB-SSD Tape Recorder & Virtual Soundcheck)**: Custom C/JACK multitrack WAV recorder (`src/jack_tape_deck.c`) & 1-Click Virtual Soundcheck replacing Headless REAPER (`Xvfb`/`x11vnc`).
- **🆕 Stage 8 (Dedicated 10" Android Tablet Kiosk APK)**: Immersive fullscreen Android APK wrapper with edge-swipe protection, local UI cache, and Wi-Fi `192.168.1.148` & Hotspot `10.42.0.1` auto-discovery.

### Deployment & Verification Commands
```powershell
# 1. Sync index.html & mixer_index.html locally and deploy to Pi (Rule 9)
Copy-Item "c:\Users\danny\Documents\modep emu\web\web\index.html" "c:\Users\danny\Documents\modep emu\web\web\mixer_index.html" -Force
scp "c:\Users\danny\Documents\modep emu\web\web\index.html" "c:\Users\danny\Documents\modep emu\web\web\mixer_index.html" patch@192.168.1.148:/home/patch/modep-live-console-release/web/
scp "c:\Users\danny\Documents\modep emu\web\web\css\mixer.css" patch@192.168.1.148:/home/patch/modep-live-console-release/web/css/
scp "c:\Users\danny\Documents\modep emu\web\web\js\*.js" patch@192.168.1.148:/home/patch/modep-live-console-release/web/js/

# 2. Verify syntax (<450 lines) and run headless runtime view test on Pi (Rules 2, 7, 8)
ssh patch@192.168.1.148 "node /tmp/test_all_dsp_views.js"
```


