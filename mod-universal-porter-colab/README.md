# 🎸 MOD Universal Plugin Porter & Cloud Cross-Compiler

[![Open In Colab](https://colab.research.google.com/assets/colab-badge.svg)](https://colab.research.google.com/github/danny1marshall1587-maker/mod-universal-plugin-porter/blob/main/MOD_Universal_Plugin_Porter.ipynb)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![MOD Compatible](https://img.shields.io/badge/MOD-Desktop%20%26%20MODEP%20Ready-00ff66.svg)]()

An automated cloud compiler and repackager that converts **any LV2 audio plugin repository or ZIP** into a **Universal Multi-Architecture FAT bundle** ready for **MOD Desktop (Windows/Linux/macOS)** and **MODEP (Raspberry Pi / Patchbox OS)**.

---

## 🚀 1-Click Cloud Usage (Google Colab)

Click the badge below to open the tool directly in your web browser with zero local installation required:

[![Open In Colab](https://colab.research.google.com/assets/colab-badge.svg)](https://colab.research.google.com/github/danny1marshall1587-maker/mod-universal-plugin-porter/blob/main/MOD_Universal_Plugin_Porter.ipynb)

### How to use:
1. **Run Cell 1**: Sets up the Ubuntu cloud cross-compilation toolchains (~25 seconds).
2. **Fill in the Form in Cell 2**:
   - **`PLUGIN_SOURCE_URL`**: Paste any GitHub repository URL or ZIP download link.
   - **Target Architectures**: Select the platforms you want to compile for:
     - 🪟 **Windows 64-bit** (`.dll` for MOD Desktop Windows)
     - 🐧 **Linux x86_64** (`.so` for MOD Desktop Linux)
     - 🍓 **Raspberry Pi 3/4 (ARMv7 32-bit)** (`.so` for Blokas MODEP)
     - ⚡ **Raspberry Pi 4/5 (AArch64 64-bit)** (`.so` for 64-bit MODEP / MOD Dwarf / Duo X)
   - **`AUTO_GENERATE_MODGUI`**: Automatically creates an authentic HTML5/CSS3 pedal GUI if the plugin has no web interface.
3. **Click Play (Run)**: Compiles all targets in parallel, packages a multi-architecture `manifest.ttl`, and **automatically downloads the finished `.zip` bundle to your PC**!

---

## 🌟 How the Universal FAT LV2 Bundle Works

A single generated `.lv2` bundle contains binaries for all operating systems and CPU architectures. When dropped into either MOD Desktop or MODEP, the host automatically loads the appropriate binary:

```
my-plugin.lv2/
├── manifest.ttl           <-- Multi-architecture OS & CPU mappings
├── my_plugin.ttl          <-- LV2 port & parameter descriptors
├── modgui.ttl             <-- Web interface descriptor
├── modgui/                <-- HTML5 / CSS3 / JS pedal interface
├── my_plugin.dll          <-- Windows 64-bit
├── my_plugin_linux_x86_64.so <-- Linux x86_64
├── my_plugin_armv7.so     <-- Raspberry Pi 3/4 (32-bit ARM)
└── my_plugin_arm64.so     <-- Raspberry Pi 4/5 (64-bit AArch64)
```

---

## 🌐 Embedding in Blokas / MOD Community Websites

To add an **"Auto-Compile for MOD"** button to any website, forum post, or documentation:

### HTML:
```html
<a href="https://colab.research.google.com/github/danny1marshall1587-maker/mod-universal-plugin-porter/blob/main/MOD_Universal_Plugin_Porter.ipynb" target="_blank">
  <img src="https://colab.research.google.com/assets/colab-badge.svg" alt="Open In Colab"/>
</a>
```

### Markdown:
```markdown
[![Open In Colab](https://colab.research.google.com/assets/colab-badge.svg)](https://colab.research.google.com/github/danny1marshall1587-maker/mod-universal-plugin-porter/blob/main/MOD_Universal_Plugin_Porter.ipynb)
```

---

## 📄 License
MIT License. Copyright © 2026 Cyber Audio & Community Contributors.
