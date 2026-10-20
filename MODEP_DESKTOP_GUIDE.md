# MODEP Desktop User Guide (Windows Native)

Welcome to **MODEP Desktop**! This setup brings the full power, modularity, and plugin ecosystem of MODEP / MOD Audio to your Windows PC natively with ultra-low latency ASIO audio hardware support, eliminating the CPU overhead, audio dropouts, and latency of running an emulated Raspberry Pi inside QEMU.

---

## 1. Quick Start

1. Double-click **[`start_modep_desktop.bat`](file:///c:/Users/danny/Documents/modep%20emu/start_modep_desktop.bat)**.
2. In the **MOD Desktop** system tray window:
   * **Device:** Select your audio interface (e.g. **Audient EVO 4** / ASIO driver).
   * **Buffer Size:** Choose **128** or **256** samples (for ~2.6ms – 5.3ms ultra-low latency at 48kHz).
   * Click **Start**.
3. Click **Open Web GUI** (or navigate to `http://127.0.0.1:18181` in your browser).

---

## 2. Audio Interface & Hardware Setup

* **Audient EVO 4:**
  * For best performance, make sure the official **EVO 4 ASIO Driver** is installed.
  * In the MOD Desktop configuration window, select `EVO 4 (ASIO)` as your device.
  * Keep sample rate at **48000 Hz** (standard MOD audio rate).
* **ASIO / Low Latency:**
  * If using built-in PC audio, you can use **FlexASIO** or **ASIO4ALL**.

---

## 3. Loading User Files (IRs, NAM Models, SoundFonts)

MODEP Desktop includes dedicated directories inside your Documents folder:
Path: `%USERPROFILE%\Documents\MOD Desktop\user-files\`

| Category | Folder Path | Supported Formats |
| :--- | :--- | :--- |
| **Guitar Cab IRs** | `user-files/Speaker Cabinets IRs/` | `.wav` (Cabinet Loaders, Convolution) |
| **Reverb IRs** | `user-files/Reverb IRs/` | `.wav` (Dragonfly, Room/Hall IRs) |
| **Neural Amp Modeler** | `user-files/NAM Models/` | `.nam` (Neural Amp Modeler plugin) |
| **AIDA-X Neural Models** | `user-files/AIDA-X Models/` | `.json`, `.aidax` (AIDA-X plugin) |
| **SoundFonts** | `user-files/SoundFonts/` | `.sf2`, `.sf3` (FluidSynth plugins) |
| **SFZ Instruments** | `user-files/SFZ Instruments/` | `.sfz` |
| **Backing Tracks / Loops** | `user-files/Audio Loops/` | `.wav`, `.ogg`, `.flac` (SooperLooper / Players) |

---

## 4. Cloud Sharing & Patchstorage Community Manager

MODEP Desktop includes a built-in **Cloud & Patchstorage Community Manager** for 1-click downloading and installation of community assets:

* **Launch the Manager:** Double-click **[`open_patchstorage.bat`](file:///c:/Users/danny/Documents/modep%20emu/open_patchstorage.bat)** (or open `http://127.0.0.1:18189`).
* **What you can download:**
  * **Pedalboards & Patches:** Community pedalboard rigs and presets from Patchstorage & MOD Cloud.
  * **LV2 Audio Plugins:** Extra community-built LV2 effect and instrument plugins.
  * **NAM & AIDA-X Neural Amps:** Guitar amp captures and tone models.
  * **Speaker & Reverb IRs:** Custom impulse response `.wav` files.
* **1-Click Auto-Install:**
  * Click **"1-Click Install to MODEP"** on any patch.
  * The tool automatically downloads, unpacks, and places files into the correct directory (`pedalboards/`, `lv2/`, or `user-files/`) so they appear immediately in your MODEP Web GUI!
* **MOD Cloud Live Feed:**
  * Inside the MODEP Web GUI (`http://127.0.0.1:18181`), click the cloud / feed icon in the bottom bar to browse and load pedalboards directly onto your live canvas.

---

## 5. Custom Plugins & LV2 Development

Your custom plugins and modgui assets are automatically packaged and deployed:
* **Deployment Script:** [`deploy_plugins_to_desktop.py`](file:///c:/Users/danny/Documents/modep%20emu/deploy_plugins_to_desktop.py)
* **Custom Plugins Directory:** `%USERPROFILE%\Documents\MOD Desktop\lv2\`
* **Included Custom Plugins:**
  1. **Harmonic Tremolo:** Vintage Fender Tri-Verb SVF crossover tremolo.
  2. **Cyber-Denoiser PRO:** One-touch noise learn suppressor.
  3. **Galaxy Strobe Tuner:** Precision strobe tuner with stage mute.

---

## 6. Comparison: Native MODEP Desktop vs QEMU Emulator

| Feature | Native MODEP Desktop | QEMU Raspberry Pi Emulation |
| :--- | :--- | :--- |
| **Audio Latency** | **~2.6 ms – 5.3 ms (Direct ASIO)** | ~40 ms – 150 ms (DirectSound emulated) |
| **CPU Overhead** | **< 2% (Native x86_64 DSP)** | High (Emulating ARM64 on x86_64) |
| **Audio Stability** | **Rock solid (No buffer underruns)** | Audio dropouts & xruns |
| **File Management** | **Instant Windows Explorer folders** | Slow SSH / SFTP copy required |
| **Plugins Available** | **120+ pre-installed 64-bit LV2s** | ARM-only binaries |
