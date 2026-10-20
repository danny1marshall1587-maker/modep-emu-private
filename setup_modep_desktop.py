#!/usr/bin/env python3
"""
setup_modep_desktop.py
Initializes and configures MODEP Desktop directories, user libraries, and plugin paths on Windows.
"""

import os
import sys
import shutil
import json

def get_mod_desktop_dir():
    user_docs = os.path.expanduser(r'~\Documents')
    return os.path.join(user_docs, 'MOD Desktop')

def setup_environment():
    base_dir = get_mod_desktop_dir()
    print("================================================================")
    print("           Setting Up MODEP Desktop User Environment            ")
    print("================================================================")
    print(f"Target Directory: {base_dir}\n")

    # 1. Base Directories
    subdirs = [
        'lv2',
        'pedalboards',
        'keys',
        '.cache',
        'user-files',
        os.path.join('user-files', 'Speaker Cabinets IRs'),
        os.path.join('user-files', 'Reverb IRs'),
        os.path.join('user-files', 'NAM Models'),
        os.path.join('user-files', 'AIDA-X Models'),
        os.path.join('user-files', 'SoundFonts'),
        os.path.join('user-files', 'SFZ Instruments'),
        os.path.join('user-files', 'Audio Loops'),
        os.path.join('user-files', 'MIDI Files')
    ]

    for sd in subdirs:
        full_path = os.path.join(base_dir, sd)
        os.makedirs(full_path, exist_ok=True)
        print(f" [+] Verified directory: {sd}")

    # 2. Add Readme notes to user-files directories
    readme_contents = {
        os.path.join('user-files', 'Speaker Cabinets IRs', 'README.txt'):
            "Drop your .wav impulse response files here for use with Cabinet Loader and Convolution plugins.\n",
        os.path.join('user-files', 'Reverb IRs', 'README.txt'):
            "Drop your reverb impulse response (.wav) files here.\n",
        os.path.join('user-files', 'NAM Models', 'README.txt'):
            "Drop your Neural Amp Modeler (.nam) amp/pedal capture files here.\n",
        os.path.join('user-files', 'AIDA-X Models', 'README.txt'):
            "Drop your AIDA-X (.json / .aidax) neural model files here.\n",
        os.path.join('user-files', 'SoundFonts', 'README.txt'):
            "Drop your SoundFont (.sf2 / .sf3) files here for use with FluidSynth plugins.\n",
        os.path.join('user-files', 'SFZ Instruments', 'README.txt'):
            "Drop your .sfz multisample instruments here.\n",
        os.path.join('user-files', 'Audio Loops', 'README.txt'):
            "Drop your backing tracks and loop sample files (.wav, .ogg, .flac) here.\n",
    }

    for rel_path, content in readme_contents.items():
        fp = os.path.join(base_dir, rel_path)
        if not os.path.exists(fp):
            with open(fp, 'w', encoding='utf-8') as f:
                f.write(content)

    # 3. Default JSON config files
    json_defaults = {
        'banks.json': [],
        'favorites.json': [],
    }

    for fname, default_data in json_defaults.items():
        fp = os.path.join(base_dir, fname)
        if not os.path.exists(fp):
            with open(fp, 'w', encoding='utf-8') as f:
                json.dump(default_data, f, indent=2)
            print(f" [+] Initialized {fname}")

    print("\nEnvironment initialization complete!")

if __name__ == '__main__':
    setup_environment()
