import json

CYBER_PLUGINS = [
    {
        "key": "36000",
        "id": 9101,
        "name": "Cyber Organ 9",
        "index": 0,
        "desc": "Cyber Audio polyphonic chord-tracking organ with 9 authentic classic organ voicings and single-note protection.",
        "library": "cyber_organ9.so"
    },
    {
        "key": "36100",
        "id": 9102,
        "name": "Cyber Mel9",
        "index": 0,
        "desc": "Cyber Audio vintage tape replay / mellotron emulator featuring strings, flutes, choir and orchestra sounds.",
        "library": "cyber_mel9.so"
    },
    {
        "key": "36200",
        "id": 9103,
        "name": "Cyber Synth9",
        "index": 0,
        "desc": "Cyber Audio analog synthesizer guitar pedal emulator with fat polyphonic synth leads and pads.",
        "library": "cyber_synth9.so"
    },
    {
        "key": "36300",
        "id": 9104,
        "name": "Cyber String9",
        "index": 0,
        "desc": "Cyber Audio symphonic string ensemble machine delivering rich orchestral washes and lush string pads.",
        "library": "cyber_string9.so"
    },
    {
        "key": "36400",
        "id": 9105,
        "name": "Cyber Bass9",
        "index": 0,
        "desc": "Cyber Audio polyphonic bass synthesizer with sub-octaves, synths and deep upright bass tones.",
        "library": "cyber_bass9.so"
    },
    {
        "key": "36500",
        "id": 9106,
        "name": "Cyber Hum Killer PRO",
        "index": 0,
        "desc": "Cyber Audio high-precision 8-harmonic mains hum (50Hz/60Hz) & DC suppressor with adaptive notch filtering.",
        "library": "cyber_hum_killer.so"
    },
    {
        "key": "36600",
        "id": 9107,
        "name": "Cyber Denoiser PRO",
        "index": 0,
        "desc": "Cyber Audio 10-band forensic spectral subtractive denoiser with real-time noise profile learning.",
        "library": "cyber_denoiser.so"
    },
    {
        "key": "36700",
        "id": 9108,
        "name": "Smart Fizz Killer",
        "index": 0,
        "desc": "Cyber Audio warm boutique harmonic saturator, phase smearer and high-frequency fizz suppressor.",
        "library": "smart_fizz_killer.so"
    }
]

# Update lv2_plugins.json
lv2_file = r"amp-rack-android\app\src\main\assets\lv2_plugins.json"
with open(lv2_file, "r", encoding="utf-8") as f:
    lv2_data = json.load(f)

for p in CYBER_PLUGINS:
    k = p["key"]
    lv2_data[k] = {
        "id": p["id"],
        "name": p["name"],
        "index": p["index"],
        "desc": p["desc"],
        "library": p["library"]
    }

with open(lv2_file, "w", encoding="utf-8") as f:
    json.dump(lv2_data, f, indent=2)

print("Updated lv2_plugins.json with Cyber Audio plugins.")

# Update all_plugins.json
all_file = r"amp-rack-android\app\src\main\assets\all_plugins.json"
with open(all_file, "r", encoding="utf-8") as f:
    all_data = json.load(f)

for p in CYBER_PLUGINS:
    k = p["key"]
    all_data[k] = {
        "name": p["name"],
        "id": p["id"],
        "plugin": p["index"],
        "desc": p["desc"],
        "library": p["library"]
    }

with open(all_file, "w", encoding="utf-8") as f:
    json.dump(all_data, f, indent=2)

print("Updated all_plugins.json with Cyber Audio plugins.")
