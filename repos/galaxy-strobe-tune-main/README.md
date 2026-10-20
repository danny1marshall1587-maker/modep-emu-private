# Galaxy Strobe Tune 🌌

**Galaxy Strobe Tune** is a professional-grade, high-performance monophonic strobe tuner plugin designed for guitarists who demand world-class accuracy and ultra-low CPU overhead.

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Format](https://img.shields.io/badge/format-VST3%20%7C%20AU%20%7C%20Standalone-orange)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-brightgreen)

## Key Features

- **High-Resolution Strobe Display**: Pure, hardware-style strobe wheel with GPU-accelerated rendering (OpenGL) for 60FPS fluid motion.
- **Professional Sweetened Tunings**: Built-in offsets for James Taylor (Acoustic), Buzz Feiten, Peterson GTR, and Open Tunings (D/G).
- **Smart AGC (Auto Gain Control)**: Massive 74dB internal boost range that automatically finds the perfect signal level for your guitar.
- **Ultra-Low CPU**: Optimized background DSP worker ensures less than 0.18% CPU usage on modern systems.
- **Custom Capture**: Capture your own unique guitar offsets and save them to persistent User Profiles.
- **Stability Control**: Adjustable smoothing algorithm to keep the strobe rock-solid on old strings or unstable pickups.

## Installation & Build

### Prerequisites
- CMake 3.15+
- A C++17 compatible compiler (GCC, Clang, or MSVC)
- JUCE dependencies (ALSA/X11/Freetype on Linux)

### Building from Source
```bash
git clone https://github.com/YOUR_USERNAME/galaxy-strobe-tune.git
cd galaxy-strobe-tune
mkdir build && cd build
cmake .. -G Ninja
ninja
```

## Usage

1. **Reference A4**: Adjust the reference pitch from 400Hz to 480Hz.
2. **Stability**: Turn up the Stability slider (default 0.99) for a smoother read on complex signals.
3. **Profiles**: Select a Sweetened Tuning from the dropdown to automatically apply professional offsets.
4. **Capture**: Strum a note and hit "Capture Offset" to calibrate the tuner to your specific instrument's intonation.

## License

This project is licensed under the MIT License.

---
*Created with precision for the CachyOS audio community.*
