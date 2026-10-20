# Cyber Strobe & Peak Tuner System for MOD Desktop & MODEP

[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20macOS%20%7C%20Linux%20%7C%20MODEP-blue.svg)]()
[![MOD Compatible](https://img.shields.io/badge/MOD-Desktop%20%26%20MODEP%20Ready-00ff66.svg)]()

A pro-grade, standalone **Studio Strobe & Peak Tuner** upgrade for **MOD Desktop (Windows, macOS, Linux)** and **MODEP (Raspberry Pi / Patchbox OS)**.

---

## 🌟 Key Features

- **Dual Visualization Modes**:
  - **Concentric Strobe Disc**: Dual-ring segmented rotating strobe wheels (coarse outer 16-segment & fine inner 24-segment) that freeze motionless when perfectly in tune.
  - **Peaked 21-Segment LED Bar Meter**: Centered LED peak meter with dynamic sub-cent needle animation.
- **Dynamic 7-Color Neon Palette**: Pitch-black OLED background with customizable glowing neon lighting (*Cyber Green, Cobalt Azure, Electric Amber, Centaur Gold, Electric Violet, Studio Ice White, Crimson Stage*).
- **Massive Pre-Gain on Tap (`0 dB` to `+36 dB`)**: Built-in preamplifier stage with `+18 dB` default gain to cleanly pick up passive single-coils, acoustic piezo pickups, and low line-level inputs.
- **Real-Time Input Signal VU Meter (`SIG`)**: Live gradient meter displaying active input signal and dBFS level.
- **Audio Device Selection**: Direct dropdown selector for audio interfaces (Audient, Focusrite, Line-In, USB audio).
- **1-Click MIDI Learn & Footswitch Modes**:
  - **`MOMENTARY`**: Tap any momentary footswitch to toggle the tuner open/closed.
  - **`HOLD`**: Tuner opens and mutes while holding the pedal down; closes and unmutes upon release.
  - **`LATCHING`**: Traditional state tracking ($CC \ge 64$ open, $CC < 64$ close).
- **Sweetened & Tempered Tunings**: Standard 12-TET, Peterson GTR Sweetened, James Taylor Acoustic, DADGAD Celtic, Drop D, Half-Step Eb, 4/5-String Bass, and Buzz Feiten System (BFTS).
- **Full Persistent State**: All preferences (Gain, Device, Color, View Mode, Reference Freq, Sweetening, MIDI mapping/mode) are saved automatically in `localStorage` across restarts.
- **Tuning Fork Header Icon**: Clean, glowing vector tuning fork icon integrated into the top action bar.

---

## 📦 Installation

### 🪟 Windows (MOD Desktop)
1. Download the latest `cyber-strobe-tuner-v1.0.0.zip` from Releases.
2. Extract the ZIP folder.
3. Right-click **`install_windows.bat`** (or `install_cyber_tuner.py`) and choose **"Run as administrator"**.
4. Open MOD Desktop, navigate to `http://localhost:18181`, and press **Ctrl + F5** to reload.

### 🍎 macOS (MOD Desktop)
1. Download `cyber-strobe-tuner-v1.0.0.tar.gz`.
2. Extract and open Terminal inside the folder:
   ```bash
   chmod +x install_mac.sh
   ./install_mac.sh
   ```
3. Launch MOD Desktop and press **Cmd + Shift + R** in the browser.

### 🐧 Linux & Raspberry Pi (MODEP / Patchbox OS)
1. Download or clone this repository to your device:
   ```bash
   git clone https://github.com/danny1marshall1587-maker/cyber-strobe-tuner.git
   cd cyber-strobe-tuner
   sudo chmod +x install_linux.sh
   sudo ./install_linux.sh
   ```
2. Open MODEP in your browser and refresh.

---

## 🛡️ Non-Destructive Backup & Rollback
Every installation automatically creates a timestamped backup of original files before applying updates. To restore your previous configuration at any time, run:
```bash
python3 install_cyber_tuner.py --restore
```

---

## 📄 License
MIT License. Copyright © 2026 Cyber Audio.
