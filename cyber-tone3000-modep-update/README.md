# Cyber TONE3000 System Addon for MODEP & MOD Desktop

Adds a permanent **`[TONE3000]`** (or `☁ TONE3000`) button directly into the main top navigation bar of MOD-UI (next to the Tuner and Transport controls), allowing you to open the full-screen TONE3000 Cloud & NAM Player overlay from anywhere on your pedalboard!

---

## What It Installs

1. **Top Navigation Bar Button**:
   - Displays a sleek cloud icon + `TONE3000` text in the main header bar.
   - Highlights with a glowing neon cyan line on hover / active.
2. **Full-Screen Interactive Overlay Modal**:
   - Opens the official TONE3000 dark-mode interface across the screen.
   - 4-Block Signal Chain: `Noise Gate` ──► `NAM Neural Core` ──► `IR Convolver` ──► `3-Band Tone Stack`.
   - TONE3000 Cloud Tone Explorer: Live search across community captures, category filters (*Acoustic*, *Clean*, *Crunch*, *High Gain*, *Bass*, *Cab IRs*).
   - In-browser chord audition audio clips.
   - Single-click `[LOAD]` button that updates the active pedal on your board in real time.
   - Local drag-and-drop zone for `.nam` and `.wav` IR files.
3. **Automatic Backup & Restore**:
   - Automatically creates timestamped backups in `Documents/MOD Desktop/backups/` before applying any changes.
   - Clean 1-click uninstaller restores everything to its exact previous state.

---

## How to Install

### On Windows (MOD Desktop):
1. Right-click **`install_cyber_tone3000.bat`** and select **Run as Administrator** (or double-click it and accept the prompt).
2. Start MOD Desktop. You will see the new **`TONE3000`** button in the top navigation bar!

### On Raspberry Pi 5 / 4 (MODEP):
```bash
cd cyber-tone3000-modep-update
sudo python3 install_cyber_tone3000.py
```

---

## How to Uninstall

### On Windows (MOD Desktop):
1. Right-click **`uninstall_cyber_tone3000.bat`** and select **Run as Administrator**.
2. Restores your previous MOD Desktop files from backup.

### On Raspberry Pi (MODEP):
```bash
cd cyber-tone3000-modep-update
sudo python3 uninstall_cyber_tone3000.py
```
