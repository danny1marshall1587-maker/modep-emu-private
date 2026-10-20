# Patchbox Prime (v8.0) — Pro Live Digital Mixing Console, 2" 20-Track Tape Machine & Lenovo Tab M11 Kiosk APK

**Patchbox Prime (v8.0)** is an ultra-low-latency 8-Channel / 18-Input Live Digital Mixing Console, Studio Desk DSP Strip, 4-Slot Auto-Routed MODEP LV2 Insert Rack (Inputs 1–8), C++ 20-Track 2" Analog Tape Machine (`15/30 IPS` + `-18 dBFS = 0 VU`), and Dedicated Lenovo Tab M11 Android Kiosk App (`PatchboxPrime.apk`) for Raspberry Pi 4 + Behringer UMC1820.

## 📚 Quick Reference & Architectural Index
- **[`PROJECT_INDEX.md`](PROJECT_INDEX.md)**: Architectural reference covering Hardware/JACK topology, per-channel signal flow (`Stage 1: 1176 FET Preamp + HPF` ➔ `Direct Out Tape Tap` ➔ `Stage 2: Digital Gain + REGAIN` ➔ `Core Desk DSP` ➔ `4 Auto-Routed LV2 Slots / Modular Canvas`), C++ Tape Engine (`tape_engine.hpp`), DOM ID lookup table, and API reference.
- **[`AGENT_RULES.md`](AGENT_RULES.md)**: Strict engineering invariants (zero simulated meters, `TouchGovernor` slew limiting, `< 450` lines per JS module, DPI-aware canvas rendering, REAPER background service permanently disabled).
- **[`LIVE_MIXER_ROADMAP.md`](LIVE_MIXER_ROADMAP.md)**: Complete Stage 1–8 implementation roadmap and verification log.

## ⚡ Key Highlights in v8.0 (Patchbox Prime + Lenovo Tab M11 Kiosk Release)
1. **Lenovo Tab M11 Dedicated Android Kiosk App (`PatchboxPrime.apk`)**:
   - **Full-Screen Immersive Kiosk + Hardware WakeLock**: Runs in true edge-to-edge immersive mode (`1920×1200` `16:10` Landscape & `1200×1920` `10:16` Portrait) with hardware screen WakeLock and persistent top-bar IP Connection Gear (`⚙️`).
   - **Zero-Scroll 9-Strip Console Layout**: Fits all 8 Input Channels + 4 Stereo Link Columns + 1 Master L/R Strip (`9 strips total`) on a single screen with zero horizontal or vertical scrolling in both Landscape and Portrait. Portrait mode automatically collapses the 4 LV2 slots into a floating `[🎛️ 4× LV2 RACK ▾]` popover drawer to maximize fader throw height.
   - **Pro Multi-Touch Gesture Suite**:
     - **Multi-Finger Simultaneous Faders** (`activeFadersMap` per `Touch.identifier`), 2-finger `0.25x` fine-tune mode, and double-tap `0.0 dB` unity reset.
     - **Omni-Axis Rotary Knobs**: Drag `Up` or `Right` to increase (`Δ = (startY - curY) + (curX - startX)`), `Down` or `Left` to decrease, plus double-tap default reset and 2-finger fine-tune.
     - **2-Finger Pinch-to-Q**: Pinch anywhere on the ReaEQ / Bus EQ canvas to widen or narrow the active band's Q bandwidth (`0.1` to `18.0`).
2. **20+ Track 2" Analog Tape Machine (`tape_engine.hpp` + Tascam Vintage GUI)**:
   - Replaces background REAPER (`reaper-headless.service` stopped & disabled) with a zero-allocation C++ JACK multitrack recorder and 2" analog tape emulator (`15 IPS` warm bump / `30 IPS` sheen, transformer saturation, and `-18 dBFS = 0 VU` calibration).
   - **Post-1176 + HPF Direct Outs (Tracks 11–18)**: Captures the warm Class-A 1176 FET saturation and rumble filter directly into the Direct Out tape tracks before channel EQ/Comp/Fader.
   - **8-Track Bank Paging & 2-Tap Record Safety Lock**: Switches cleanly between `🎛️ CH 1-8`, `✨ FX 9-12 & DUBS`, `🔌 DIRECT 1-8 (1176+HPF)`, and `🎧 OUTPUTS & MON` with 8 large touch faders per bank and a 2-tap confirmation lock while recording.
   - **Storage Drive & Session Folder Picker**: Select SD Card or external USB SSDs (`/media`, `/mnt`, `/run/media`) with real-time free disk space and recording minutes remaining.
3. **Input-Only MODEP LV2 Inserts (Buses & Mains Streamlined)**:
   - All 4-slot MODEP LV2 inserts and canvas views are dedicated exclusively to **Input Channels 1–8**; Monitor Buses 1–6 and Main L/R use dedicated zero-latency C++ Bus EQ, Wedge Protection Limiters, and Analog Obsession Master Bus Comp (`BUSTERse`) + Brickwall Limiter.
