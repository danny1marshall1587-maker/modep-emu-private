# Patchbox Prime / MODEP Live Console — 7-Step Implementation Plan

This document outlines the 7-step architectural and UI/DSP implementation plan agreed during the `/grill-me` design review.

---

## Step 1: 4 True Independent Stereo Aux Sends & Returns (2 Reverbs + 2 Delays)
* **Goal**: Replace the shared 1-Reverb / 1-Delay Aux bus with **4 true independent Stereo Sends (`Send 1–4`)** feeding **4 dedicated Stereo FX Engines** into **4 true Stereo Returns (`Return 1–4`)**:
  1. **Send 1 (Stereo) → Reverb 1 → Return 1 (Stereo)**: **Instrument Room / Plate Reverb** (4×4 Hadamard FDN + 8-Tap Early Reflections IR Hybrid tuned for guitars, drums, keys, and acoustic instruments).
  2. **Send 2 (Stereo) → Reverb 2 → Return 2 (Stereo)**: **Vocal-Focused Lush Hall / Abbey Plate Reverb** (tuned vocal pre-delay, vocal presence contour, auto-ducking, and lush modulated tail).
  3. **Send 3 (Stereo) → Delay 1 → Return 3 (Stereo)**: **Tape Slapback Engine** (authentic 15/30 IPS slapback echo, tape oxide saturation, wow/flutter, and stereo slap spread).
  4. **Send 4 (Stereo) → Delay 2 → Return 4 (Stereo)**: **Modulated Vocal Delay Engine** (Hermite 4-point fractional chorus-modulated stereo / ping-pong delay with dynamic vocal auto-ducking).
* **Files**: `src/mixer_meter_daemon.c`, `web/web/js/tal-reverb.js`, `web/web/js/tal-delay.js`, `web/web/js/dsp-modals.js`.

---

## Step 2: Tape-Machine Aesthetic Reverb & Delay Fullscreen GUIs with Input + Output Filters
* **Goal**: Redesign both the **Reverb Fullscreen GUI** (switching seamlessly between `REVERB 1: INSTRUMENT` and `REVERB 2: VOCAL`) and **Delay Fullscreen GUI** (switching between `DELAY 1: SLAPBACK TAPE` and `DELAY 2: MODULATED VOCAL`) in the warm vintage **2" Tape Machine aesthetic** (brushed dark steel, warm amber/copper meters, tactile hardware faders/rotaries).
* **100% Live Controls + Dual Filtering**:
  * Remove any non-functional legacy controls so every slider/knob maps 1:1 to the C audio engine.
  * Add dedicated **INPUT FILTERS (`Input Low-Cut HPF` + `Input High-Cut LPF`)** before the tank/delay line AND **OUTPUT RETURN FILTERS (`Output Low-Cut HPF` + `Output High-Cut LPF`)** on the stereo return.
* **Files**: `src/mixer_meter_daemon.c`, `web/web/js/tal-reverb.js`, `web/web/js/tal-delay.js`, `web/web/css/mixer.css`.

---

## Step 3: 20-Track Tape Machine True DAW Overdubbing & Single-Button Fullscreen File/Project Browser
* **Goal**: Make the 20-Track Tape Machine operate as a true standalone live DAW / multitrack tape recorder:
  * **True Multitrack Overdub**: When returning to `00:00` (`RTZ`) and pressing `RECORD`, **only Record-Armed tracks (`REC` = ON)** overwrite their RAM buffer and `.wav` file on disk. **All Un-Armed tracks (`REC` = OFF)** preserve their recorded audio and play back into the monitor mix in real time so you can record Take 2 / Guitar 2 while hearing Guitar 1 and the rest of the band.
  * **Smart Overdub Workflow**: Adding a new track (`+ ADD TRACK`) or arming a track in Overdub mode automatically keeps previously recorded tracks safe in playback mode.
  * **Single-Button Fullscreen Session & File Browser (`📁 PROJECTS & FILES`)**: Opens a dedicated fullscreen DAW File Manager modal to:
    * Browse, Load, Create, Rename, or Delete Song/Project folders (`<tape_base_dir>/<song_name>/`).
    * Inspect, Audition, or Delete individual recorded `.wav` stems inside a Song folder.
    * Browse and select the active recording drive/directory (Internal SD Card, External USB SSD, or custom directory path).
* **Files**: `src/mixer_meter_daemon.c`, `web/web/js/dsp-modals.js`, `web/web/js/app.js`.

---

## Step 4: Single-Knob Auto-Compensated 1176 Preamp (Stage 1)
* **Goal**: Upgrade the Stage 1 `1176 Class-A FET Preamp` on all 8 input channels to a world-class UTC input transformer + FET harmonic saturation model controlled by a **Single Auto-Compensated `DRIVE` Knob (`0.0 to +24.0 dB`)**:
  * Automatically applies mathematical inverse loudness compensation (`1.0 / calibrated_tanh_gain(drive)`) so sweeping `DRIVE` from `0 dB` to `+24 dB` Dial-in adds rich analog warmth, transient weight, and harmonic saturation **without jumping in volume or clipping downstream stages**.
  * Includes a `SOFT FET / HARD CLIP` character switch and live `CLEAN / FET WARMTH / SATURATED` LED telemetry.
* **Files**: `src/mixer_meter_daemon.c`, `web/web/js/dsp-rack.js`, `web/web/js/dsp-modals.js`.

---

## Step 5: Per-Channel World-Class Compressors + Dual-Slot Compressors/Limiters on Master & Monitors 1–6
* **Goal**:
  * Upgrade the C dynamics engine (`FETISH 76`, `LALA Optical`, `DUAL 76→LA`, `BUSTER VCA/Brickwall Limiter`) to ultra-lightweight, zero-bloat studio-grade ballistics with strictly independent state per Input Channel (`CH 1–8`), Send (`SEND 1–4`), and Return (`RET 1–4`).
  * **Dual-Slot Speaker Dynamics on Master L/R & Monitor Buses 1–6**: Equip `MASTER` and `MON 1–6` with **two independent series Dynamics Slots (`SLOT A: BUS COMP` → `SLOT B: SPEAKER COMP / LIMITER`)**, each offering the full model choices (`FET 76`, `LALA`, `DUAL`, `BUSTER LIMITER`) so you can glue/compress the bus in Slot A and brickwall-limit speaker outputs in Slot B simultaneously.
* **Files**: `src/mixer_meter_daemon.c`, `web/web/js/compressor.js`, `web/web/js/dsp-rack.js`, `web/web/js/dsp-modals.js`.

---

## Step 6: Cyber Tuner 60FPS Live Pitch Engine + MOD-GUI List/File Selectors (NAM, IRs, Evans Bass Pro)
* **Goal**:
  * **Cyber Tuner / Strobe Tuner**: Add a fast zero-crossing + autocorrelation pitch detector per channel in `src/mixer_meter_daemon.c` streaming live Frequency (`Hz`), Note Name (`E2`, `A2`, `D3`, etc.), and Cent deviation (`-50..+50 cents`) at 60FPS to `modgui-host.html`, complete with a fullscreen Strobe/Needle Tuner display and a 1-tap `🔇 MUTE CHANNEL WHILE TUNING` toggle whenever a tuner plugin is loaded in an Effect Slot.
  * **MOD-GUI List & File Selectors**: Upgrade `web/web/modgui-host.html` to inspect LV2 `scalePoints` enumerations (e.g., `cyber-evans-bass-pro` articulation/voicing lists) AND `atom:Path` / `mod:fileTypes` parameters (e.g., `.nam` Neural Amp Modeler profiles, `.wav` Cabinet/Reverb Impulse Responses), rendering large touch-friendly selector buttons/dropdowns and syncing selections to the DSP engine.
* **Files**: `src/mixer_meter_daemon.c`, `web/web/modgui-host.html`, `web/web/js/dsp-rack.js`.

---

## Step 7: Fader-Track Integrated 60FPS Channel Meter & Top-Bar APK Refresh Button
* **Goal**:
  * **Fader-Track Integrated Background Meter**: Embed each channel's 60FPS LED/gradient level meter (`#meter-ch-0`..`7`, Sends, Returns, Monitors, Master) directly **behind the full vertical height of the `.fader-track`**, allowing the fader thumb to glide over the live glowing signal meter while freeing horizontal strip width for a sleeker vertical console layout.
  * **Top-Bar APK Refresh Button**: Add a 1-tap `🔄 REFRESH` button in the top console header bar so the Android tablet APK or browser view can be reloaded instantly without leaving fullscreen kiosk mode.
* **Files**: `web/web/index.html`, `web/web/css/mixer.css`, `web/web/js/mixer-ui.js`, `web/web/js/telemetry.js`.

---

## Work Log & Glitch-Free DSP Fixes (Implemented 2026-10-08 — Ready for Pi Deploy)

1. ✅ **Instrument Reverb (`REVERB 1: INSTRUMENT`) — Added 3 Studio Hall Reverb Presets ([`tal-reverb.js`](file:///c:/Users/danny/Documents/modep%20emu/web/web/js/tal-reverb.js#L56-L61))**:
   * Added **`LEXICON 224 INSTRUMENT HALL`**, **`WARM ACOUSTIC CONCERT HALL`**, and **`LUSH SYMPHONY STRINGS HALL`** directly into the Instrument Reverb (`Send 1 → Return 1`) preset bank (now 19 presets total).

2. ✅ **Fixed Low-CPU / 0-XRUN Audio Glitching ([`mixer_meter_daemon.c`](file:///c:/Users/danny/Documents/modep%20emu/src/mixer_meter_daemon.c#L1367-L1386), [#L1457-L1525](file:///c:/Users/danny/Documents/modep%20emu/src/mixer_meter_daemon.c#L1457-L1525), [#L2992-L2999](file:///c:/Users/danny/Documents/modep%20emu/src/mixer_meter_daemon.c#L2992-L2999))**:
   * **Gated 300ms RMS Auto-Gain Staging (`apply_module_autogain`)**: Fixed the 13.8ms fast square-law rider that was tracking bass half-cycles and snapping `target_g` to `1.0f` during note tails/silence. Envelopes and gain now hold steady below `-44 dBFS`, use a ~300ms RMS integration window, and never fight compressor/denoiser gain reduction.
   * **Eliminated Delay LFO Harmonic Phase-Wrap Clicks (`mod_phase2`)**: Replaced `sinf(dl->mod_phase * 2.7f)` (which jumped discontinuously by up to ~9 samples every $2\pi$ wrap) with an independent `mod_phase2` accumulator.
   * **Fractional Hermite Reads in Channel Slot Delays & Chorus (`process_channel_effect_slots_range`)**: Replaced integer `(int)` LFO delay-line indexing in Slot Delay (`case 3`) and Slot Chorus/Flanger (`case 5`) with `read_frac_hermite` fractional interpolation.
   * **Thread-Safe Audio-Owned EQ Coefficient Updates (`param_dirty` + `ramp_svf_band_params`)**: WebSocket thread now only updates target `freq`/`gain`/`q` and sets `param_dirty = 1`; the JACK audio thread smoothly ramps `sm_freq` (log-domain), `sm_q`, and `sm_gain` at 3 kHz control rate so the audio thread never reads a half-written coefficient set.

3. ✅ **Fixed Fader-Track Meter Lag ([`mixer.css`](file:///c:/Users/danny/Documents/modep%20emu/web/web/css/mixer.css#L3470-L3519))**:
   * Removed `filter: saturate(1.35) brightness(1.12)` and `filter: saturate(1.15)` from `.fader-section > .meter-container .meter-bar` and `::before` (which forced Android WebView to re-rasterize 20 full-height filtered layers on every 60FPS `clip-path` frame) and baked the vivid colors directly into the gradient.

4. ✅ **Zipper-Free Parameter Smoothing & Dual Read-Head Tap Tempo Crossfade ([`mixer_meter_daemon.c`](file:///c:/Users/danny/Documents/modep%20emu/src/mixer_meter_daemon.c#L2789-L3030))**:
   * **Delay Time / Tap Tempo Dual-Head Equal-Power Crossfade (~45 ms)**: Changing Delay Time or tapping Tempo on Delay 1 & Delay 2 now crossfades smoothly (`cos`/`sin` equal-power over ~45 ms) to a second read head (`xf_target`) instead of sliding a single read head across thousands of samples (eliminating the pitch-warp "dive-bomb" and feedback runaway).
   * **Reverb & Delay Per-Sample ~20ms Smoothing + Fractional Pre-Delay**: `rev_size_scale`, `rev_fb`, `rev_damp`, `rev_wet`, `rev_ap_g`, `rev_er_mix`, `rev_width`, `dly_fb`, `dly_wet`, `dly_width`, `dly_spread`, and all Input/Output HPF/LPF filter coefficients now ramp per sample (`~20 ms`), and Reverb Pre-Delay uses fractional linear interpolation (`sm_predly`).


