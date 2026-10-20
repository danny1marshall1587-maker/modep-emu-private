# STRICT AGENT CONTRACT & DEVELOPMENT RULES

You are an audio systems engineer working on a production live digital mixer running on Patchbox OS, Linux RT, JACK, and a Raspberry Pi 4. 

Failure to follow these rules will corrupt the mixer's stability and result in immediate code rejection.

---

### RULE 1: ZERO TOLERANCE FOR SIMULATED OR DUMMY CODE
- NEVER implement fake physics, `Math.sin()`, `Math.random()`, or synthetic decay loops to animate meters (Peak, RMS, VU needles, or Gain Reduction bars)[cite: 2].
- If live telemetry (WebSocket, OSC, SharedArrayBuffer) is not yet active, set meter elements statically to `-inf` / `0` and write a single comment: `// TELEMETRY_PENDING: Needs backend socket link`.
- Never give the user the visual illusion of working audio DSP when backend plumbing is missing[cite: 2].

---

### RULE 2: REFACTOR AND REPLACE — NEVER STACK OR LEAVE GHOST MARKUP
- When replacing an audio processor (e.g., swapping the VCA compressor for Analog Obsession FETISH 76 or LALA), you must COMPLETELY DELETE the obsolete processor's HTML, CSS, and JavaScript event listeners.
- Do NOT hide old modules with `display: none` or stack them on top of one another[cite: 2].
- Channel DSP rack order is strictly: 
  `Input Gain -> ReaEQ Parametric -> [Switchable Comp: FETISH 76 | LALA] -> LOADES De-Esser -> MODEP Insert Loop`.

---

### RULE 3: TOUCH SAFETY GUARDRAILS ARE INVIOLABLE
- NEVER bypass, disable, or shortcut the rate-of-change limiter (`skipSpeedLimit = true` is strictly banned) on level faders, pre-gain, or send levels[cite: 2].
- Continuous level controls (Channel Faders, Input Gain, REGAIN, Send Knobs) must route through `TouchGovernor.process(id, value)` to prevent velocity snaps and accidental acoustic shock on live PA systems.
- **EXCEPTION — EQ NODES & EQ CONTROLS (`ReaEQ`)**: Never apply `TouchGovernor` or `maxSpeedDamping` slowing to EQ frequency, gain, or Q controls (`reaeq-touch.js`, `dsp-rack-eq.js`, `dsp-modals.js`). Because frequency is logarithmic (`10 Hz – 22,050 Hz`), rate-limiting delta by a few units freezes frequency dragging. All EQ canvas nodes and band sliders must respond 1:1 with finger/pointer position.
- If a fader or knob moves erratically during fast drags, solve the underlying calculation delta; do not remove the speed limiter[cite: 2].

---

### RULE 4: TRUE HARDWARE CANVAS RENDERING
- Sizing an HTML5 `<canvas>` via CSS containers alone (`width: 100%`, `height: 100%`, or flexbox) without updating underlying canvas buffer properties is strictly banned[cite: 2].
- Every canvas resize (including fullscreen ReaEQ toggles) must explicitly set:
  ```javascript
  const dpr = window.devicePixelRatio || 1;
  canvas.width = Math.floor(container.clientWidth * dpr);
  canvas.height = Math.floor(container.clientHeight * dpr);
  const ctx = canvas.getContext('2d');
  ctx.scale(dpr, dpr);
  ## ARCHITECTURAL CONSTRAINTS & CODE QUALITY GUARDS

1. ABSOLUTE BAN ON MONOLITHIC INLINE CODE:
   - NEVER inject JavaScript `<script>` blocks or CSS `<style>` blocks into `index.html`.
   - HTML files must contain semantic markup only.
   - All logic must reside in dedicated modules under `web/js/`.
   - All styling must reside in dedicated stylesheets under `web/css/`.
   - No individual JavaScript file may exceed 450 lines.

2. MANDATORY LOCAL AST / SYNTAX VALIDATION:
   - Before reporting ANY task complete, you must run Node syntax verification:
     node -c web/js/*.js
   - If any file fails with a syntax error, unexpected token, or parse error, you must fix it immediately. You are forbidden from returning a completion message if `node -c` exits with a non-zero code.

3. IDENTIFIER DUPLICATION GUARD:
   - Before defining any top-level variable or function (`let`, `const`, `var`, `function`), grep `web/js/` to confirm the identifier is not already defined.
   - Cross-module state must use explicit properties on the global `window.PatchboxState` object, never loose global variables.

4. SAFE DOM RENDERING (NEVER WIPE EVENT LISTENERS):
   - Never use `container.innerHTML = ...` on elements that already have event listeners attached.
   - Use `document.createElement`, `.textContent`, `.value`, or dataset attribute updates for dynamic rendering.

5. VERIFICATION HONESTY:
   - Do not state that UI interactions or audio engines are "verified" unless you have run a terminal command with exit code 0 that proves it.

6. NON-HANGING SSH COMMANDS & LIVE WEB ROOT PATH:
   - The live Patchbox Prime web root on `patch@192.168.1.148` is `/home/patch/modep-live-console-release/web/` (served on port `8090`).
   - NEVER launch a background daemon over SSH without closing `stdin` (`</dev/null`), or SSH will hang indefinitely waiting for file descriptors to close. Always use:
     `nohup /usr/local/bin/modep-meter-daemon </dev/null >/tmp/meter.log 2>&1 &`

7. FEATURE-BOUNDARY MODULE SPLITTING & SCRIPT LOAD ORDER:
   - Never chop a `.js` file at an arbitrary line number or delete multi-row UI templates (such as Dual Compressor views) to stay under 450 lines. Extract logical feature modules (e.g., `web/js/compressor.js`) and share a single unified implementation across Channels, Sends, Returns, Monitors, and Master.
   - Maintain strict dependency-first `<script>` ordering in `index.html`: utility/canvas modules (`ao-vu-knobs.js`, `compressor.js`, `reaeq-canvas.js`, `reaeq-touch.js`, `tal-reverb.js`) MUST load before orchestrators (`dsp-rack-eq.js`, `dsp-rack.js`, `tal-delay.js`, `dsp-modals-ao.js`, `dsp-modals.js`).

8. MANDATORY RUNTIME VIEW TEST AFTER MODULE EDITS:
   - In addition to `node -c`, execute a headless runtime check verifying that `showDspRackPage(0..7)`, `setChannelCompModel(ch, 'dual')`, and `openDspModal()` for `preamp1176`, `fet1176`, `la2a`, `dual`, `comp`, and `limiter` execute with zero `ReferenceError` or missing DOM elements.

9. SYNCHRONIZED WEB ENTRYPOINTS (`index.html` & `mixer_index.html`):
   - On the Raspberry Pi (`/home/patch/modep-live-console-release/web/`), both `index.html` and `mixer_index.html` are used by different launch paths.
   - Whenever `web/web/index.html` is modified or deployed, you MUST copy it to both `index.html` and `mixer_index.html` locally and on the Pi so neither entrypoint ever goes stale.

10. NEXT-GEN ARCHITECTURE (NATIVE C/JACK DSP + CUSTOM JACK TAPE DECK + ANDROID APK):
   - **Native C/JACK DSP Engine (`src/mixer_meter_daemon.c`)**: JSFX hosting is on hold. All Core Desk Strip stages (1176 Preamp, Regain/HPF/Pan, Gate/Denoiser, 8-Band Cytomic SVF ReaEQ, Unified `FET 76 | LALA | DUAL` Compressor, LoaDeS De-Esser, Master AFS/Summing) run natively in C inside `modep-meter-daemon`, while Stage 8 uses `mod-host` LV2 slots.
   - **Custom C/JACK 22-Track Tape Recorder (`modep-tape-deck`)**: Replaces Headless REAPER (`Xvfb`/`x11vnc`). Streams 8 Dry + 8 Wet + 4 FX + 2 Master WAV stems directly to USB 3.0 SSD via `libsndfile` ringbuffers and provides 1-click **Virtual Soundcheck** playback into Channels 1–8.
   - **Android Tablet Kiosk APK**: Immersive fullscreen Android wrapper with edge-swipe protection, local UI asset caching, and automatic failover between Studio Wi-Fi (`192.168.1.148`) and Pi Hotspot (`10.42.0.1`).

11. REAEQ JSFX PARITY & ZERO-LAG EQ INTERACTION:
   - Never apply `TouchGovernor` or `maxSpeedDamping` slowing to EQ canvas node drags, pinch-to-Q, or band parameter sliders (`reaeq-touch.js`, `dsp-rack-eq.js`, `dsp-modals.js`).
   - Do not include an `HQ OVER-SAMPLING` button in the EQ toolbar.
   - Every ReaEQ band (`Bands 1–8`) must expose full ReEQ JSFX controls: **Enable/Bypass**, **Filter Type** (`Peak`, `Low Cut`, `Low Cut (Butterworth)`, `Low Shelf`, `High Shelf`, `High Cut`, `High Cut (Butterworth)`, `Notch`, `Band Pass`, `Tilt Shelf`), **Slope / Order** (`12`, `24`, `36`, `48 dB/oct` for Cut/Butterworth filters), **Frequency (`20 Hz – 20 kHz`)**, **Gain (`-24 to +24 dB`)**, and **Q (`0.10 to 18.0`)**, synchronized in real time with the C/JACK 8-band Cytomic SVF engine and 64-band RTA.

12. DEDICATED AUX SEND/RETURN FX & 1:1 PLUGIN CONTROLS:
   - Aux Send (`SEND 1–4`) and FX Return (`RET 1–4`) strips must NEVER include MODEP pedalboard inserts or LV2 slot racks (`btn-super-insert`). They are dedicated exclusively to the built-in Reverb (`Send 1–2`) and Delay (`Send 3–4`) engines, plus per-bus ReEQ and Ducking Compression.
   - All plugin modal faders and rotary knobs (`TAL-Reverb-4`, `IR Convolver`, `TAL-Delay-Pro` in `tal-reverb.js`, `ir-convolver.js`, `tal-delay.js`) must respond 1:1 with pointer/touch movement (no `TouchGovernor` clamping, no `maxSpeedDamping` slowing, and full-track touch capture without an 18px cap lockout).
   - Every Reverb and Delay GUI control (`Size`, `Damp`, `Pre-Delay`, `Diffuse`, `Stereo Width`, `Dry/Wet`, `Low Cut`, `High Cut`, `Delay Time / Tap Tempo Subdivision`, `Feedback`, `Ping-Pong`, `Modulation`, `Ducking`) must be wired end-to-end to the C JACK DSP engine (`src/mixer_meter_daemon.c`).

13. TRUE -INFINITY dB SURGICAL NOTCHES & AUTO-TRACKING ACOUSTIC BASS RESONANCE (AFD):
   - AFD is provided on all 8 Input Channels (`CH 1–8`), all 6 Stage Monitors (`MON 1–6`), and `MAIN L/R` (with Soundcheck Ring-Out Lock on buses + Live Tracking on channels so they never conflict).
   - Every notch must support continuous depth down to **True `-∞ dB` (`-Infinity` complete spectral null)** in a single filter—eliminating the Behringer X32 limitation where `-18 dB` cuts had to be stacked across multiple buses.
   - Use 64-bit (`double`) precision ultra-high-Q (`Q` up to `100+`) surgical SVF filters for maximum musical transparency on neighboring notes.
   - Include **Auto-Width (`Auto-Q`) & Centroid Pitch-Tracking** for low-frequency acoustic guitar/body resonances (`60 Hz – 280 Hz`): the filter automatically tracks drifting bass resonances (`±15 Hz` window) and dynamically adjusts its `Q` width based on frequency drift so the player never has to manually chase moving bass feedback during a gig.

14. WORK SMARTER — COPY / PORT OPEN-SOURCE PLUGIN OR JSFX DSP CODE DIRECTLY:
   - Never reinvent complex DSP algorithms from scratch when proven open-source plugin code or JSFX script code exists online.
   - Before implementing or reworking any effect (Reverb, Delay, Main Bus Adaptive Room-Curve Dynamic EQ, Surgical `-∞ dB` AFD Notches, Pitch/Resonance Trackers, Analog Summing), search online (`search_web` / `read_url_content`) for open-source C/C++ plugins (TAL, Airwindows, x42, LSP, Guitarix, ChowDSP) or ReaPack/Cockos JSFX scripts and port their exact DSP math directly into `src/mixer_meter_daemon.c`.

15. RETIRING BACKGROUND DAEMONS ON RASPBERRY PI (`systemctl disable --now`):
   - Never rely on `pkill` alone when retiring a background daemon on the Pi (such as `patchbox-reaper.service` / `Xvfb :99`). Always check `systemctl list-unit-files` and run `sudo systemctl stop <unit> && sudo systemctl disable <unit>` so `Restart=always` does not respawn the process in the background or on reboot.

16. ANDROID KIOSK APK COMPILATION & WEBVIEW ASSET RULES:
   - **Never compile Android Java sources against hand-written stub classes**: Guessed parameter types (e.g., `Object` instead of `android.graphics.Paint` in `View.setLayerType`) emit mismatched bytecode descriptors that immediately crash Android ART with `java.lang.NoSuchMethodError`. Always compile `javac` and `d8 --lib` against the official Android SDK `android.jar` and verify bytecode descriptors with `javap -c`.
   - **Window Features Before `super.onCreate()`**: Always call `requestWindowFeature(Window.FEATURE_NO_TITLE)` *before* `super.onCreate(savedInstanceState)` (wrapped in `try/catch`), and sign APKs with `apksigner` using v1 + v2 + v3 signatures (`--v1-signing-enabled true --v2-signing-enabled true --v3-signing-enabled true`).
   - **No Bare Relative `fetch('/...')` in Hybrid APK Assets**: When bundling Web UI assets under `file:///android_asset/`, resolve HTTP endpoints against `window.getActivePiHost()` whenever `window.location.protocol === 'file:'`.
   - **Auto-Pull Live Pi UI on Startup (`checkAndPullLivePiUi`)**: `MainActivity.java` loads `file:///android_asset/web/index.html` for 0ms offline startup and automatically probes `192.168.1.148`, `172.24.1.1`, and `10.42.0.1` on port `8080` to load `http://<host>:8080/mixer/?v=<timestamp>` when reachable so HTML/JS updates appear immediately on the tablet without reinstalling the APK.

17. ANDROID TABLET DPI-SCALED VIEWPORT (`960×600` / `1280×800` CSS PX) & ZERO-SCROLL FIT:
   - **Verify at DPI-Scaled CSS Resolutions**: A `1920×1200` Android tablet (Lenovo Tab M11) renders WebView at `1280×800` (`1.5x` DPR) or `960×600` (`2.0x` DPR) CSS pixels in Landscape (`800×1280` / `600×960` in Portrait). Always verify headless Chromium screenshots at `960×600`, `1280×800`, and `800×1280` before publishing the APK.
   - **Protect Top Chrome with `flex-shrink: 0`**: `header` and `.mixer-nav-tabs` must always have `flex-shrink: 0 !important; height: auto !important;` and dock `.global-quick-actions` (`SOLO CLEAR`, `CLR CLIP`, `MUTE MICS 1-6`, `FX MUTE`) into the right side of Row 1 (`.nav-row-top`) so they never wrap behind channel strips.
   - **Never Apply `display: flex !important` to `.channel-strip`**: Tabbed views (`switchMixerView`) rely on inline `style="display:none;"` to hide inactive send/return/monitor strips.
   - **100% Zero-Scroll Fit for Fullscreen EQ, DSP Rack & FX Modals**: In `.dsp-modal.reeq-modal.fullscreen`, `#modal-body` must use `display: flex; flex-direction: column; flex: 1 1 0; min-height: 0; overflow: hidden;` with `#reeq-canvas { position: absolute; inset: 0; }` and an 8-column grid `.reeq-node-strip` so all 8 band cards fit on screen without scrolling.

18. MODAL Z-INDEX STACKING HIERARCHY & MULTI-DEVICE LIVE SYNC PARITY:
   - **Strict 3-Tier Z-Index Hierarchy**: Level 0 Main Console (`z-index: 1–100`) < Level 1 Fullscreen Sub-Pages (`#dsp-rack-page`, `#tape-looper-page` at `z-index: 1500`) < Level 2 Detail Modals (`#dsp-modal-overlay` / `.modal-overlay` at `z-index: 3500 !important`). Never set a Level 1 page (`#dsp-rack-page`) to a higher `z-index` than `.modal-overlay`, or Level 2 detail modals (`1176 Preamp`, `Denoiser`, `AFD Pro`, `Compressors`, `De-Esser`) opened from inside the rack will render behind the rack page.
   - **Sync All 3 Web Roots on Raspberry Pi**: Whenever deploying HTML/CSS/JS changes to the Pi, always copy to all 3 web directories (`/home/patch/modep-live-console-release/web/`, `/home/patch/modep-git-backup/web/`, and `/var/modep/web/`) before running `build_apk_on_pi.sh` so both the `:8080` Web Portal and the bundled APK assets stay 100% identical.
   - **Exact WebSocket Payload Key Parity (`multi-device-sync.js`)**: `multi-device-sync.js` must inspect `const act = msg.action || msg.type;` and match the exact JSON keys emitted by `live-controls.js`, `dsp-rack.js`, `compressor.js`, `reaeq-canvas.js`, `tal-reverb.js`, and `tal-delay.js` (`pct`, `sof`, `target`, `type: 'dsp_param'`, `getCompState`), guarded by `window._applyingRemoteSync = true` to prevent feedback loops.

19. NATIVE-APP 60FPS GUI RESPONSIVENESS & SENDS-ON-FADER (SOF) INTEGRITY:
   - **Zero GPU Blur & Zero `setTimeout` Modal Open Delays**: Never use `backdrop-filter: blur(...)` on fullscreen overlays (`.modal-overlay`), and never wrap modal DOM/canvas initialization in `setTimeout(..., 30..50)`. Open `.modal-overlay` synchronously and size canvases on the first `requestAnimationFrame`.
   - **60FPS Cached-DOM Metering & Offscreen VU Dials**: All 60FPS meter updates in `telemetry.js` must use cached DOM references (`_meterDomCache`) with `0.2%` change quantization and `transition: none !important; contain: strict;`. Analog VU meters (`ao-vu-knobs.js`) must cache static dial scales/gradients on an offscreen canvas and animate only the needle via a 60FPS `requestAnimationFrame` loop.
   - **Non-Destructive Sends-on-Fader (SOF) Switching & Pre-Fader Send Metering**: Calling `setFaderPosition()` while toggling or exiting SOF mode must pass `skipBackend = true` so visual fader repositioning never overwrites backend gains. When `window.currentSofMode` is active, channel meters (`meter-0`..`meter-7`) must display pre-fader channel signal × SOF send gain (`ch_pre[i] * sofLinearGain`), and the SOF banner + Master L/R meters must display the active SOF bus level.
   - **Timed DSP Learning Actions Must Have Client-Side Completion Safeguards**: Any timed hardware/DSP learning action (e.g., Cyber-Denoiser `Learn Noise`) must include both matching C/JS JSON keys (`ch` and `channel`) and a client-side fallback timer (~1.15s) so the UI button never hangs in a perpetual learning state if a packet is dropped.
   - **Tape Counter Uses Studio Time (`HH:MM:SS.d`), RTA Uses `PEAKS | BARS | SPECTRAL | OFF`**: Always display Tape Machine position in studio time (`HH:MM:SS.d`) rather than inches/IPS, and keep the EQ RTA modes visually distinct (`📐 PEAKS` sharp triangular spires, `📊 BARS`, `🔬 SPECTRAL`, `OFF`).


20. ZERO-LATENCY (<16ms) REAL-TIME METERING, CYBER-DENOISER DSP EFFICIENCY & DAEMON BINARY PARITY:
    - **Always Stop `modep-meter-daemon.service` and Update `/usr/local/bin/modep-meter-daemon` (with RT Limits)**: `modep-meter-daemon.service` executes `/usr/local/bin/modep-meter-daemon` (with `/usr/local/bin/mixer_meter_daemon` symlinked to it) and requires `LimitRTPRIO=95` and `LimitMEMLOCK=infinity` in `/etc/systemd/system/modep-meter-daemon.service` so JACK's real-time callback never fails `AcquireSelfRealTime`. Always `sudo systemctl stop modep-meter-daemon.service` before copying the compiled binary so Linux never rejects the copy with `Text file busy`.
    - **Zero Per-Sample Transcendentals & Safe `-38 dBFS` Learn Ceiling in Cyber-Denoiser**: Never call `expf`, `powf`, or `log10f` inside the 48kHz × 8ch × 10-band per-sample loop in `process_cyber_denoiser_sample`; cache filter/attack coefficients and linear thresholds, compute `log10f` UI band levels once per JACK block (375Hz), use peak-assisted instantaneous attack (`0.5ms`), and clamp `Learn Noise` thresholds to a `-38.0 dBFS` maximum ceiling (resetting `denoise_current_gains = 1.0f` on completion) so stage bleed during Learn Noise never chokes or delays the channel signal or post-DSP meter.
    - **Instant 0ms Fader/Mute Meter Scaling & Linear-in-dB Peak Ballistics**: Use true block-peak hold with fast linear-in-dB release (`peak * 0.85f` per 60Hz tick, snapping to `0.0f` below `0.0003f`), scale `g_state.peak_levels` immediately inside `handle_ws_control_message` when `set_fader` or `set_mute` arrives, and call `window.refreshLocalChannelMeterImmediate(ch)` locally in `channel-strip.js` so fader/mute movements reflect on channel meters in 0.0ms.
    - **60FPS (`16ms`) Bidirectional WebSocket Pipeline, `SO_SNDBUF = 65536` & 0ms `pointerdown` Touch Engine**: Broadcast `"meters"` every `16ms` (`60 FPS`) in `mixer_meter_daemon.c` with immediate `<1ms` echo on `LWS_CALLBACK_RECEIVE`, reassemble multi-fragment WebSocket messages (`lws_is_first_fragment`/`lws_is_final_fragment`), and set `TCP_NODELAY = 1` and `SO_SNDBUF = 65536` (`64 KB` — small enough to prevent bufferbloat while large enough to write `full_state_sync` + `snapshot` atomically without truncation). Coalesce incoming `"meters"` packets once per `requestAnimationFrame` in `telemetry.js`, cache static EQ grids/curves onto offscreen canvases (`_gridLayer` / `_curveLayer`), coalesce outbound fader/knob drags at `60 FPS` (`16.6ms` `requestAnimationFrame` in `live-controls.js` and `reaper-transport.js`), track per-control `lastTimes = new Map()` in `touch-governor.js`, and fire all non-scrollable console/rack/tape buttons on `pointerdown` (`0ms` latency in `touch-physics.js`, preserving the 2-tap `STOP` confirmation during active recording).
    - **"Pi is King of Sync" & Instant Non-Volatile Auto-Save (`/var/modep/last_live_mixer_state.bin`)**: `modep-meter-daemon` is the single authoritative Master State. Persist `LivePersistentState` (all faders, pans, mutes, pregains, 1176 preamp, denoiser, 8-band EQs, compressors, de-essers, AFD notches, Reverb 1&2, Delay 1&2, Tape Machine config, LV2 slots, and `g_latest_ui_snapshot`) atomically via `.tmp` + `fsync` + `rename` to `/var/modep/last_live_mixer_state.bin` on the background disk thread within `120ms` of any change (and on `SIGINT`/`SIGTERM`), and restore it on startup in `main()`. Connecting clients (`telemetry.js`, `multi-device-sync.js`) must never overwrite Pi state on `onopen`; they must synchronize to the Pi's `full_state_sync` and schedule debounced (`220ms`) `peer_ui_snapshot` saves after local edits.

21. 100% FULLSCREEN TOUCHSCREEN REAL-ESTATE, POST-SLOT DENOISER ORDER & MOD-GUI BRIDGE RULE:
    - **100% Touchscreen Real-Estate Mandate**: Every sub-page, multi-slot view, plugin picker, and effect editor (including the **Full-Screen 4-Slot Channel Rack**, the **Full-Screen Effect Picker with Category Filter Buttons**, and the **MOD-GUI / Control View Editor**) MUST utilize `100vw x 100vh` (`100%` of touchscreen real estate) rather than cramped centered floating popups. All toggle switches (`.custom-toggle-switch`, `.mod-switch`, `.mod-footswitch`, and Control View switches) must have expanded touch hit targets (`>= 60px x 60px` effective tap zone with `touch-action: manipulation` and direct `pointerdown`/`click` handling) so they trigger effortlessly during a live gig.
    - **Post-Insert Cyber-Denoiser Signal Chain Order**: In both `src/mixer_meter_daemon.c` (`jack_process`) and the DSP Rack UI (`dsp-rack.js`), the **4 Channel Effect Slots** MUST run **BEFORE** the **Cyber-Denoiser PRO** (`Input Gain -> Gate -> 4x Effect Slots -> Cyber-Denoiser PRO -> Compressor -> 4-Band EQ -> Fader/Sends`). This ensures any tube hiss, saturation noise, or analog floor introduced by hardware-emulation effects in the slots is cleaned up by the denoiser.
    - **MOD-GUI Host & Real-Time Slot Audio Wiring**: `modep-web-portal.py` on port `8080` must reverse-proxy `/effect/*`, `/resources/*`, and `/mixer/modgui-host.html` from `127.0.0.1:8881` with `Access-Control-Allow-Origin: *` so both browser (`:8080`) and Android APK (`file://`) contexts load all LV2 metadata and custom `cyber-*` MOD-GUI assets reliably. In `modgui-host.html`, `GUI(..., { change: function(portSym, value) })` receives the port symbol string as its 1st argument, and programmatic updates must pass `"from-js"` (never boolean `true`) as the 3rd argument to `setPortValue(sym, val, "from-js")`, plus bind universal touch handlers for `.custom-knob-dial[data-symbol]` and `.custom-toggle-switch[data-symbol]`.
    - **Universal 3D Rotary Knob CSS Fallback & 1:1 Touch Dragging**: When an LV2 pedalboard modgui lacks knob sprite stylesheets (rendering `.mod-knob` invisible), `modgui-host.html` must inject a universal 3D brushed metal rotary knob dial fallback with pointer vertical-drag rotation (`-135°` to `+135°`), double-tap reset to default, and live numeric value tooltips.
    - **Multi-Instance User-Files Linkage & Universal NAM/IR Selectors**: Symlink `/var/modep/instance_*/user-files -> /var/modep/user-files` and `/data -> /var/modep` on Patchbox OS so all MOD-UI instances index all `.nam` models and `.wav` IRs. Both `modgui-host.html` and the 4-slot rack Controls view must render prominent `<optgroup>` folder-grouped file pickers with `◀ PREV` / `NEXT ▶` steppers and dispatch `lv2PatchSet` / `set_lv2_slot_file` to `mod-host`.
    - **End-to-End NAM/IR File Persistence & Voicing in C Daemon**: `mixer_meter_daemon.c` MUST implement `set_lv2_slot_file` to store `file_path` and `file_label` in `SlotDspUnit`, persist them across boots via `PersistentSlotParam` (`/var/modep/last_live_mixer_state.bin`), and dynamically adapt tone-stack/gain/cab voicings to the selected model character (Clean/Blackface, British Crunch, High-Gain Lead, Vintage Silvertone, Bassman, Acoustic Flat Shimmer).
    - **Non-Hanging SSH/SCP Timeout Guardrail**: Always pass `-o ConnectTimeout=5 -o BatchMode=yes -o ServerAliveInterval=3 -o ServerAliveCountMax=2` to `ssh` and `scp` commands (and wrap remote commands in `timeout 15s ...` when appropriate) so transient Wi-Fi drops or remote processes never hang an SSH session.
 
 22. PRINCIPAL SOFTWARE ENGINEER & WORLD-CLASS PRODUCT ARCHITECT MANDATE (4 PILLARS):
     - **1. Architectural Foresight**: Automatically implement industry-standard best practices (lock-free real-time audio thread safety, zero allocations in `jack_process`, proper state management, multi-device sync decoupling, and CPU/GPU resource optimization) without waiting to be asked.
     - **2. The "Slightly Off" Fix**: Proactively engineer solutions to prevent classic audio/UI pitfalls (e.g., zipper noise on parameter changes, phase cancellation in parallel/crossover paths, gain-staging jumps, WebSocket bufferbloat, latency spikes, or UI/DSP state desync).
     - **3. Class-Leading Enhancements**: Include 1–2 high-value, studio-grade sub-features that elevate every component beyond a bare-minimum implementation (e.g., automatic dry/wet or drive gain compensation, musical defaults, clip/headroom protection, and 100% fullscreen gig-ready touch ergonomics).
     - **4. Zero-Bloat Code**: Deliver clean, modular, and clearly commented code explaining *why* specific DSP, real-time systems, and UI architectural choices were made.
 
 23. **JACK Hardware Passthrough Isolation (`mod-monitor`), MOD-GUI `SAMPLERATE` Contract & Studio DSP Standards**:
     - **A. Disconnecting `mod-monitor` Parallel Hardware Bleed in JACK**:
       - On Patchbox OS / MODEP, `mod-host` creates a default `mod-monitor` client wired directly from `system:capture_1/2` to `system:playback_1/2`. If left connected while `modep-meter-daemon` is active, raw dry input bleeds straight to the main outputs/headphones even when mixer channels are muted, and creates severe parallel comb-filtering against the mixer's DSP and Aux Reverb/Delay.
       - Whenever `modep-meter-daemon` attaches to JACK in `try_attach_jack()`, it MUST explicitly disconnect `mod-monitor:out_1 -> system:playback_1`, `mod-monitor:out_2 -> system:playback_2`, `system:capture_1 -> mod-monitor:in_1`, and `system:capture_2 -> mod-monitor:in_2` so `modep-meter-daemon` is the single authoritative master bus driving `system:playback_1/2`.
     - **B. MOD-GUI Host Global `SAMPLERATE` & Zero-Hang `iconTemplate` Rendering**:
       - `/usr/share/mod/html/js/modgui.js` requires a global `SAMPLERATE` variable (`var SAMPLERATE = 48000;`) inside `makePortIndexes` (`port.ranges.minimum *= SAMPLERATE`) and hangs indefinitely if `/effect/file/settingsTemplate` fails.
       - `modgui-host.html` MUST define `window.SAMPLERATE = 48000;` before loading `modgui.js` and enforce a `1000ms` render watchdog that directly fetches `/effect/file/iconTemplate?uri=...` (or renders the interactive cyber-pedal fallback) so no slot ever hangs on `Loading Effect Module...`.
       - All state variables (including `renderedOk`) must be explicitly declared in the top-level host scope to prevent fatal runtime `ReferenceError` crashes, and `modep-web-portal.py` must implement `do_HEAD` for static MOD-UI assets to prevent 404 asset drops.
     - **C. Studio DSP & Touchscreen Ergonomics Standards**:
       - **Aux Reverb (Send 1-2) & Stereo Delay (Send 3-4)**: Aux Reverb uses a modulated **4x4 Hadamard Feedback Delay Network (FDN) + Stereo Allpass Diffuser + Early Reflections IR hybrid** with Abbey Road-style Low/High-Cut damping. Aux Delay uses **Hermite cubic fractional delay interpolation**, switchable **Tape / BBD Analog / Studio Digital** voicings, Ping-Pong width, and **Auto-Ducking** controlled by input envelope. All Aux FX modals (`#auxModal`) MUST occupy **100% Fullscreen (`100vw x 100vh`)** on tablet APK and web.
       - **Stage 7 Dual-Band Metered De-Esser**: Features two independent dynamic suppression bands (**Band 1 Vocal Sibilance `2.0 kHz – 8.5 kHz`** and **Band 2 High/Air Harshness `4.5 kHz – 16.0 kHz`**) with a 100% fullscreen 60FPS split-view interactive GUI: (1) **Compressor Transfer Curve & Gain Reduction History Graph (`#deesser-comp-canvas`)** with draggable `T1`/`T2` knee handles, live operating-point dots, and scrolling GR trace, and (2) **Dynamic Split-Band Attenuation Spectrum (`#deesser-rta-canvas`)** with 2D draggable `B1`/`B2` nodes, twin Gain-Reduction meters (`gr_deesser`, `gr_deesser2`), control-rate SVF smoothing (`ramp_deesser_svf_band`), and a **SOLO SIBILANCE (LISTEN)** audition mode.
       - **Piecewise Studio Mid-Focus EQ Scale (`200 Hz – 2.5 kHz` Expanded)**: Default horizontal frequency mapping places `100 Hz` at `~12%` of width and dedicates `~58%` of horizontal touchscreen space to `200 Hz – 2.5 kHz`, with a 1-tap `MID-FOCUS / STD LOG` scale toggle in the EQ header.
       - **Click-Free Compressor Bypass & Top-Bar Pi Telemetry Pill**: Both the inline DSP Rack Compressor row and Fullscreen Compressor Modal provide a **Master COMP BYPASS** toggle alongside individual `FET 76` and `LALA` power switches with smoothed gain-compensated switching. The top header displays a compact **Pi Telemetry Pill** showing real-time **JACK DSP %**, **Pi CPU %**, **Core Temp**, and a live **XRUN counter** (with tap-to-reset).
 24. **4-Bus Stereo Aux FX, True DAW Overdubbing, Auto-Compensated 1176 Preamp, Dual Speaker Dynamics & Fader-Track Meters**:
     - **4 Independent Stereo Aux Sends/Returns (`Send 1–4` → `Return 1–4`)**:
       - `Send 1 → Return 1`: **Stereo Reverb 1 (Instrument Room / Plate Hybrid)**.
       - `Send 2 → Return 2`: **Stereo Reverb 2 (Vocal Hall / Abbey Plate Hybrid with Auto-Ducking)**.
       - `Send 3 → Return 3`: **Stereo Delay 1 (15/30 IPS Tape Slapback Engine)**.
       - `Send 4 → Return 4`: **Stereo Delay 2 (Chorus-Modulated Vocal Ping-Pong Delay with Auto-Ducking)**.
       - Both Reverb and Delay Fullscreen GUIs use the **2" Tape Machine aesthetic**, expose only 100% live C-bound parameters, and provide dedicated **Input HPF/LPF** and **Output Return HPF/LPF** filters.
     - **20-Track Tape Machine True DAW Overdub & Fullscreen File Manager**:
       - In `RECORD` mode (`tape_transport == 2`), only **Record-Armed tracks (`tape_trk_rec[t] == 1`)** overwrite their buffer/WAV file, while **Un-Armed tracks (`tape_trk_rec[t] == 0`)** play back their existing recorded audio in real time for true multitrack overdubbing.
       - **Unlimited-Length Direct-Disk WAV Streaming (Zero Fixed RAM Loop Buffers)**: Never use a fixed-duration RAM loop buffer (`TAPE_LOOP_FRAMES`) or modulo frame wrapping (`tape_frame_pos % TAPE_LOOP_FRAMES`) in `mixer_meter_daemon.c`, and never overwrite disk WAV stems from a truncated RAM buffer on `SAVE / BOUNCE`. Stream both recording (`tape_disk_ring[t]`) and playback/overdubbing (`tape_play_ring[t]`, with 32,768-frame stop/seek pre-buffering and periodic 2-second WAV header commits) directly to/from 24-bit WAV files on the SD card / USB SSD, and stop cleanly at `tape_max_frames` during `PLAY` instead of looping.
       - A single **`📁 PROJECTS & FILES`** button opens a Fullscreen DAW Session & File Browser to Load/New/Rename/Delete song folders, delete/audition track WAVs, and browse/select recording drives/directories.
     - **Single-Knob Auto-Compensated 1176 Preamp**: Stage 1 `1176 Preamp` operates as a single **`DRIVE` (`0..+24 dB`)** control with automatic mathematical inverse loudness compensation so increasing harmonic drive never causes volume jumps.
     - **Per-Channel Compressors & Dual-Slot Speaker Dynamics (`MASTER` & `MON 1–6`)**: Every channel, send, and return has its own independent Compressor instance; `MASTER` and `MON 1–6` each feature **2 series Dynamics Slots (`SLOT A: COMP` + `SLOT B: COMP / LIMITER`)** for simultaneous bus compression and speaker limiting.
     - **Cyber Tuner 60FPS Pitch Engine & MOD-GUI List/File Selectors**: `modep-meter-daemon` computes live per-channel pitch (`Hz`, note, cents) for the slot Cyber Tuner (with Mute-to-Tune), and `modgui-host.html` renders touch-friendly List/File selectors for LV2 `scalePoints` (Evans Bass Pro) and `atom:Path` files (`.nam` models and `.wav` IRs).
     - **100% Zero-Scroll Viewport Real-Estate for 4-Slot Rack Page**: `renderFullScreenSlotsPage` in `modep-slots.js` must compact headers (`34px` action buttons), slot cards (`padding: 8px 10px`, sliders `20px`), and quick category buttons so all 4 slot columns fit 100% on tablet screens (`960x600`, `1280x800`, and `800x1280`) with zero scroll.
     - **Slot Card Model Badge & 1-Tap Stepper Buttons**: When an effect slot hosts a file-backed plugin (NAM models, IR convolver, Aida DSP), the slot card MUST render an active model pill badge (`📁 [MODEL NAME]`) and 1-tap `◀ PREV` / `NEXT ▶` stepper buttons directly on the card so musicians can swap tones during a performance without opening detail modals.
     - **Fader-Track Integrated 60FPS Channel Meter & APK Refresh**: Channel level meters render directly as the full-height background inside `.fader-track`, and the top header bar includes a 1-tap `🔄 REFRESH` button for the tablet APK.


## 25. Auto Module Gain Staging, Independent '1st in Chain' Routing, `< Preset >` FX Decks & Track-Color Illuminated Fader Meters
- **Automatic Per-Module Unity Gain Staging & Soft-Ceiling Guard**:
  - Every DSP module in the channel and bus chain (`1176 Preamp`, `Insert Slots 1&2`, `MOD-GUI`, `10-Band Denoise`, `8-Band EQ`, `Compressor`, `De-Esser`) must apply automatic unity-gain compensation and a `-0.5 dBFS` (`0.944f`) soft-saturation ceiling guard at the output of each module so driving a module or boosting EQ bands never overloads the next stage.
- **Independent `'1ST IN CHAIN'` Toggles for Insert Slots 1&2 and MOD-GUI**:
  - Keep the main console channel strips (`CH 1–8`) clean with a **single `⚡ DSP` button (`#btn-ch-dsp-{ch}`)** that opens the Channel DSP Rack page (no multi-button `SLOT: POST` / `MOD: POST` / `4 SLOTS` clutter on the channel strip itself). Inside the Channel DSP Rack and 4-Slot Rack, provide independent `1ST` routing buttons for **Insert Slots 1&2** (`slot_first[ch]`) and **MOD-GUI Pedalboard** (`modgui_first[ch]`), while **Denoise** always executes after both Insert Slots and MOD-GUI.
- **Zero-Scroll Reverb & Delay GUIs with `< [PRESET NAME] >` Stepper, 32 Presets per Unit & 0–100% C-Daemon Scaling**:
  - `tal-reverb.js` and `tal-delay.js` must fit 100% inside the `960x600` viewport with zero scrolling, use a compact `< [PRESET NAME] >` prev/next arrow stepper with **32 studio presets per unit** (including Concert Halls, Studio Chambers, and Acoustic Guitar Ambiances on Reverb 1), and send `0..100` (`%`) / `ms` / `Hz` values matching `mixer_meter_daemon.c`.
  - **Zero-Lag Targeted Button DOM Updates**: Never call full-container `innerHTML` re-renders (`renderTapeMachineModal`) on transport, track arm, mute, or solo button presses; mutate only the targeted button classes/attributes (`syncTapeTrackDom`) so touch-up events never get swallowed on tablet WebViews.
- **Track-Color-Synced Multi-Shade Illuminated Fader-Track Meter**:
  - Render the entire `.fader-track` column as a full-width backlit LED meter strip with dark dB numbers & ticks (`-6` to `-60`) flanking the central black fader slit and a 3D dark ribbed fader cap riding in the center.
  - Animate the lit region from `-60 dB` up to the live 60FPS audio level (dimming the unlit upper portion to ~18% brightness) using a multi-shade rainbow gradient synchronized to each channel/bus strip's primary & secondary track color palette, while always keeping the top `-3 dB .. 0 dB` overload/clipping zone bright Red (`#ef4444`).

## 26. MANDATORY COMPLETION & DEPLOY CHECKLIST (NO SKIPPING)
Before claiming any task complete, tick every applicable item with command + exit-code evidence:
1. `node -c` on every edited `web/web/js/*.js` -> exit 0.
2. Headless runtime check (Arch Rule 8) -> zero `ReferenceError`.
3. UI/CSS change: headless screenshots at `960x600`, `1280x800`, `800x1280` viewed before publishing.
4. `index.html` edited -> copy to `mixer_index.html` (local + Pi) and bump the `?v=` cache string.
5. Deploy to ALL 3 Pi web roots: `/home/patch/modep-live-console-release/web/`, `/home/patch/modep-git-backup/web/`, `/var/modep/web/`.
6. Every ssh/scp uses `-o ConnectTimeout=5 -o BatchMode=yes -o ServerAliveInterval=3 -o ServerAliveCountMax=2`; use `;` not `&&` in Windows PowerShell 5.
7. C daemon edited -> `sudo systemctl stop modep-meter-daemon.service`, compile, copy to `/usr/local/bin/modep-meter-daemon`, start, confirm JACK attach in `journalctl`.
8. APK -> `build_apk_on_pi.sh` only after steps 4-5, then sync to `dist/`, repo root and `web/web/`.
9. Real-time audio: no parameter may step inside `jack_process`; coefficients, delay times and gains must be ramped or crossfaded, and WS-thread writes must never tear coefficient sets read by the audio thread (see Rule 27).
10. Pi Offline / Unreachable Protocol: always resolve HTTP/WebSocket hosts via `window.getActivePiHost()` (never hardcode `192.168.1.148` in `config.js` or modal helpers), and include a rolling `/24` subnet batch sweep in `telemetry.js` (`probeParallelHosts`) so router DHCP changes (`192.168.1.2..254`) and hotspot IPs (`10.42.0.1`, `172.24.1.1`) auto-discover. When `192.168.1.148`, `10.42.0.1`, and `172.24.1.1` time out during deployment, run a fast parallel TCP scan across `192.168.1.1..254` (ports `22`, `8080`, `8089`) and check `netsh wlan show interfaces` before reporting whether the Pi moved to a new DHCP IP, switched to `patchbox` hotspot mode, or is powered off.
    - Keep NetworkManager `pb-hotspot` set to `connection.autoconnect no` on the Pi so it never hijacks `wlan0` from Studio Wi-Fi at boot, and maintain the Top-Bar **`📶 WIFI ↔ HOTSPOT`** switcher (`set_network_mode` -> `/usr/local/bin/patchbox-net-switch.sh`) in `telemetry.js` and `index.html`.

## 27. GLITCH-FREE REAL-TIME PARAMETER CHANGES
- **No step changes**: reverb size/decay/damp/wet, delay feedback/wet and filter cutoffs are ramped per-sample (~20 ms one-pole), not recomputed per block from raw values.
- **Delay time / Tap Tempo uses dual read-head crossfade** (~30-60 ms equal-power), never a one-pole slide of a single read pointer (pitch-warp "dive-bomb"). Tape-style glide may be an optional mode only.
- **Pre-delay read pointers** never jump by integer samples; use fractional smoothed reads or a crossfade.
- **LFO phase**: never multiply a wrapped phase (`sinf(phase * 2.7f)`), as it discontinues at the wrap; keep separate phase accumulators per harmonic.
- **Thread safety**: the WebSocket thread writes *target* params only; the audio thread derives SVF/biquad coefficients from smoothed targets (or swaps double-buffered coefficient sets atomically). Never let the audio thread read a half-written coefficient set.
- **Meters**: never put CSS `filter:` (saturate/brightness/blur) on a layer whose `clip-path`/`height` animates at 60FPS; bake brightness into the gradient colors instead.

## 28. EXCLUSIVE CHANNEL INSERT ROUTING (`4 SLOTS ONLY` vs `MOD PEDALBOARD ONLY`) & CLEAN BLEED ELIMINATION
- **Mutually-Exclusive Channel Insert Mode**: Every input channel (`CH 1–8`) must feature a strictly mutually-exclusive 3-way insert mode: **`OFF (DIRECT)` | `4 SLOTS ONLY` | `MOD PEDALBOARD ONLY`**.
- **Zero Cascaded Insert Conflict**: When a channel is set to `MOD PEDALBOARD ONLY`, the 4 internal DSP rack slots are completely bypassed (0% DSP execution in `mixer_meter_daemon.c`) to provide a 100% direct signal loop to the MOD pedalboard. When set to `4 SLOTS ONLY`, the MOD pedalboard insert is disconnected.
- **Hardware Monitor Bleed Disconnection Guard**: `mixer_meter_daemon.c` must periodically (and on every JACK port change callback) enforce the disconnection of `mod-monitor:out_1 -> system:playback_1` and `mod-monitor:out_2 -> system:playback_2` so clean un-effected hardware input can NEVER bleed directly to the PA/monitors in parallel with pedalboard or slot processing.

## 29. EXACT ALGORITHMIC C++ PORTING FOR CYBER AMPS & AUTHENTIC MOD-GUI PARITY
- **Ground Truth from `/home/patch/plugins/`**: When implementing or tuning cyber amp DSP in `mixer_meter_daemon.c` (`cyber-marshall-jtm45`, `cyber-orange-or120`, `cyber-fender-bassman59`, `cyber-vox-ac30tb`, `cyber-dumble-ods`, `cyber-friedman-be100`, etc.), the agent MUST inspect the actual C++ sources in `/home/patch/plugins/<plugin>.lv2/src/` and port their exact tube clipping formulas, tone-stack filter topologies, and gain staging.
- **Authentic MOD-GUI & Control View Parity**: `modgui-host.html` must faithfully render all controls matching the original LV2 `.ttl` port ranges and custom MOD-GUI styles, providing exact 1:1 control response.

## 30. ANDROID KIOSK APK MOD-UI CANVAS AUTO-FIT & MULTI-WINDOW POP-OUT
- **WebView Multi-Window Support**: In `MainActivity.java`, enable `settings.setSupportMultipleWindows(true)` and `settings.setJavaScriptCanOpenWindowsAutomatically(true)`, and implement `WebChromeClient.onCreateWindow` (or route external/popup links through an Android Intent to the browser or dedicated fullscreen activity).
- **100% Zero-Scroll Viewport Canvas Auto-Fit**: `#canvas-modal-overlay` and `#canvas-modal-iframe` must include responsive viewport scaling (CSS zoom/transform scaling) and a header toggle (`🔍 FIT SCREEN` / `100% ZOOM`) so both the input jacks on the left and the output jacks on the right are 100% visible and interactive on Android tablet screens (`960×600` / `1280×800`).

## 31. NATIVE LV2 `.SO` HOSTING IN C DAEMON, EXACT `.TTL` PORT PARITY & TABLET TOUCH HUD
- **Native `.so` Execution First**: Whenever an effect slot in `mixer_meter_daemon.c` loads an installed LV2 plugin (`cyber-*`, `nam-loader`, `duskverb`, etc.), `load_native_lv2_slot()` MUST dynamically load (`dlopen`) the compiled `.so` binary from `/usr/modep/lv2/<bundle>.lv2/`, parse its `.ttl` port indices/symbols/defaults, and execute `lv2_desc->run(handle, nframes)` on the JACK audio block. The internal C approximation runs only as a fallback if no `.so` binary is present.
- **100% `.ttl` Port Symbol & ScalePoints Parity**: Every entry in `window.MODEP_LV2_CATALOG` (`modep-slots.js`) must use the exact LV2 `.ttl` port symbols, min/max/default ranges, and `scalePoints` enumerations (never generic `AMP_PORTS` placeholders). Any port with `scalePoints` (`model`, `ir`, `preset`, `algorithm`) must render a 1-tap `◀ PREV` / `NEXT ▶` stepper badge directly on the 4-slot rack card.
- **Tablet Touch Hit Targets & Live Parameter HUD**: In `modgui-host.html`, all `.mini-dial` controls (`>= 44x44px`), `[class*="-mod-switch-pill"]` switches (`>= 36x58px`), and `.custom-knob-dial` rotors must rotate `.knob-rotor` without double-rotating the outer housing, and display a live top-center parameter readout pill (`#touch-param-hud`) during touch interaction.

## 32. PHYSICAL FLUID SIMULATION STABILITY, FLUID REVERB SPACE & FLUID FENDER HARMONIC TREMOLO
- **Strict Polynomial Boundedness in Non-Linear Fluid Cells**:
  - Never apply raw quadratic or cubic polynomials ($x^2$, $x^3$) directly to unbounded audio or drive signals inside fluid dynamics cells.
  - All asymmetric harmonic expansions and corner eddy vortex stresses must operate on bounded normalized variables ($x = \tanh(\text{signal}) \in (-1, 1)$), guaranteeing mathematical stability ($|\text{eigenvalues}| < 1.0$) across all volume multipliers ($0.1\times \dots 10.0\times$) and drive levels with zero Nyquist blow-up.
- **Fluid Dynamics Reverb Space (`FluidReverbSpace`)**:
  - Reverb must embody fluid cavity acoustics: Chamber Shape sweeps continuously from circular cylinder (pure radial Bessel modes) to cube/box (corner eddy turbulence and modal scattering).
  - Chamber Size scales along the 19-position 432 Hz natural acoustic period ladder ($\div 10 \dots \div 2$, center $1\times$ at 432 Hz, $\times 2 \dots \times 10$).
  - Send filtering must apply Abbey Road Low-Cut and High-Cut conditioning to the signal *before* non-linear fluid drive to prevent low-end mud and pick splash.
- **Vintage Fender Fluid Space Harmonic Tremolo (`FluidHarmonicTremolo`)**:
  - Authentic 1960–1963 6G-series crossover topology: phase-aligned Linkwitz-Riley split (~750 Hz) into Low and High frequency bands.
  - Low and High bands must process through twin Fluid Dynamics Tube Cells modulated in $180^\circ$ anti-phase, reproducing true tube conduction/cutoff, dynamic sag, and authentic Doppler pitch wobble with zero volume loss.
- **Stepped Control Ergonomics & Parameter Range Verification**:
  - All stepped knobs must support vertical-drag stepping with click-to-advance fallback and double-click reset.
  - GUI knob ranges (`0..10`) and DSP engine normalized ranges (`0.0..1.0`) must be strictly aligned with calibrated audio-taper curves.

## 33. NON-DESTRUCTIVE HISS ELIMINATION, DISCRETE CHANNEL TONE STACKS, FLUID STOMPBOXES & HIGH-GAIN CASCADES
- **Non-Destructive High-Frequency Hiss Elimination**:
  - Never apply static low-pass filtering that dulls guitar pick attack, treble chime, or harmonic brilliance.
  - Hiss reduction must use dynamic program-dependent downward expansion and spectral subtraction, opening with instantaneous peak response (<=0.5ms) during notes to preserve 100% of guitar highs (3 kHz – 15 kHz), and clamping only the low-level noise floor as notes decay.
  - The clean lab denoiser must be seeded with calibrated defaults (never inactive -100 dBFS), and post-power/post-cab dynamic hiss tracking must operate strictly below the guitar signal threshold (<= -55 dBFS).
- **Discrete Channel Tone Stacks**:
  - In multi-channel guitar engines (Clean, Crunch, High-Gain Lead), channels must NEVER share tone stack state or GUI knobs.
  - Each channel must expose its own calibrated Bass, Middle, and Treble controls matching its authentic hardware circuit topology (Blackface AB763 plate-driven vs Bassman/Plexi cathode-follower vs BE-100 lead).
- **Pre-Amp Stompbox Drive Staging**:
  - Selectable overdrive pedals (Fluid Klon Centaur vs Tube Screamer TS808) must execute directly after the clean lab and before the tube preamp.
  - All clipping and diode bridges must operate on bounded fluid non-linearities (x = tanh(·)) with authentic tone-stack filtering.
- **High-Gain British Rock Topology (Friedman BE-100 / Marshall JCM 800)**:
  - Must incorporate the authentic 10k Cold Clipper (V1B) asymmetric transfer curve, optional HBE cascade, C45 voicing, and EL34 push-pull power stage.

## 34. ZERO-BLEED FX BYPASS, INDEPENDENT STOMPBOX STACKING & HIGH-HEADROOM CLEAN BOOSTING
- **Zero-Bleed FX Bypass**:
  - Time-based and acoustic cavity effects (Delay, Acoustic Space, Reverb) must have bit-accurate zero-bleed bypass.
  - When an effect is bypassed or mix is at 0.0%, output must equal clean input (`out = in`) with zero delayed, comb-filtered, or cavity-coupled signal bleeding into the dry audio path.
- **Independent Stompbox Stacking in Series**:
  - Analog drive emulations (Klon Centaur, TS808, BD-2 Blues Driver) must be provided as independent series modules with dedicated true-bypass switches rather than mutually exclusive A/B selectors.
- **High-Headroom Clean Boost Execution**:
  - All drive pedal models must provide up to +20 dB .. +24 dB of clean output level headroom so players can set gain low and use the pedal as an authentic tube amplifier driver.
- **Discrete Multi-Stage FET Emulation (BD-2 Blues Driver)**:
  - Blues Driver circuits must emulate discrete cascading JFET stages with full-range bass retention and smooth dynamic pinch-off, avoiding digital fizz or harsh diode clipping.

## 35. TRANSIENT-DECOUPLED HIGH-BAND DE-HISSING & ADAPTIVE PHASE CANCELLATION
- **Steep $\ge 24\text{ dB/octave}$ Detector Decoupling**:
  - Never use shallow 1st-order (6 dB/octave) high-pass filters in high-band de-hiss detectors or downward expanders.
  - High-band hiss detectors must employ steep $\ge 24\text{ dB/octave}$ (4th-order) phase-aligned filtering ($>75\text{ dB}$ sub-harmonic rejection at $90\text{ Hz}$) so heavy low-frequency guitar string thumps ($60\text{ Hz} \dots 250\text{ Hz}$) cannot leak into the high-frequency detector or falsely trigger gate release tails.
- **Adaptive Anti-Phase Noise Subtraction**:
  - Stationary noise floors must be cancelled using $180^\circ$ anti-phase subtraction ($s(t) + n(t) - \hat{n}(t) \equiv s(t)$), eliminating hiss bursts without low-pass dulling.
- **Active Tone Matrix Hiss Shield**:
  - High-shelf EQ stages providing $>+10\text{ dB}$ of high-frequency gain (Glass Air, Treble, Bright Chime) must incorporate dynamic floor decoupling so idle background hiss is never amplified during gate release windows.


