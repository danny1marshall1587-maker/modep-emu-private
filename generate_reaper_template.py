#!/usr/bin/env python3
import os
import sys

OUTPUT_FILE = "/var/modep/recordings/Patchbox_Prime_Live.RPP"
RECORDING_DIR = "/var/modep/recordings"

def generate_rpp():
    os.makedirs(RECORDING_DIR, exist_ok=True)

    tracks_rpp = []

    # Tracks 1–18: "Dry HW 1" through "Dry HW 18"
    for i in range(1, 19):
        # REC: 1 (armed)
        # RECINPUT: i - 1 (maps to input channel index, 0-indexed mono: in1=0, in2=1, ...)
        # RECMON: 0 (monitoring off/prevent feedback)
        # TRACKHEIGHT: default, VOLPAN: 1 0 -1 -1 1
        track_chunk = f"""  <TRACK
    NAME "Dry HW {i}"
    PEAKCOL 16576
    BEAT -1
    VOLPAN 1 0 -1 -1 1
    MUTESOLO 0 0 0
    IPHASE 0
    ISBUS 0 0
    BUSCOMP 0 0 0 0 0
    SHOWINMIX 1 0.6667 0.5 1 0.5 0 0 0
    REC 1 {i-1} 0 0 0 0 0 0
    TRACKHEIGHT 0 0 0 0 0 0
    INQ 0 0 0 0.5 100 0 0 100
    NCHAN 2
    FX 1
    TRACKID {{{os.urandom(16).hex().upper()}}}
  >"""
        tracks_rpp.append(track_chunk)

    # Tracks 19–22: "Wet Rig 1" through "Wet Rig 4"
    for j in range(1, 5):
        track_idx = 18 + j
        input_idx = 18 + (j - 1) # in19 = 18, in20 = 19, in21 = 20, in22 = 21
        track_chunk = f"""  <TRACK
    NAME "Wet Rig {j}"
    PEAKCOL 3342591
    BEAT -1
    VOLPAN 1 0 -1 -1 1
    MUTESOLO 0 0 0
    IPHASE 0
    ISBUS 0 0
    BUSCOMP 0 0 0 0 0
    SHOWINMIX 1 0.6667 0.5 1 0.5 0 0 0
    REC 1 {input_idx} 0 0 0 0 0 0
    TRACKHEIGHT 0 0 0 0 0 0
    INQ 0 0 0 0.5 100 0 0 100
    NCHAN 2
    FX 1
    TRACKID {{{os.urandom(16).hex().upper()}}}
  >"""
        tracks_rpp.append(track_chunk)

    all_tracks = "\n".join(tracks_rpp)

    rpp_content = f"""<REAPER_PROJECT 0.1 "7.80/linux-aarch64" 1759068000
  RIPPLE 0
  GROUPOVERRIDE 0 0 0
  AUTOXFADE 1
  ENVATTACH 1
  POOLEDENVATTACH 0
  MIXERSLIDER 1
  RECORD_PATH "{RECORDING_DIR}" ""
  RECORD_CFG 1987013481 24 0 0 0
  DEFRECMODE 1
  AUTOSAVE 300 0 0
  SAMPLERATE 48000 0 0
  <METRONOME 6 2
    VOL 0.25 0.125
    FREQ 800 1600 1
    BEATLEN 4
    SAMPLES "" ""
    PATTERN 2863311530 0
    MULT 1
  >
  MASTERAUTOMODE 0
  MASTERTRACKHEIGHT 0 0
  MASTERTRACKVIEW 0 0 -.5 0 0 0 0 0 0 0 0 0 0
  MASTERHWOUT 0 0 1 0 0 0 0 -1
  MASTERVOLPAN 1 0 -1 -1 1
  MASTERPEAKCOL 16576
  MASTERMUTESOLO 0 0 0
  MASTERNCHAN 2
  MASTERFX 1
  MASTERTRACKID {{{os.urandom(16).hex().upper()}}}
{all_tracks}
>
"""
    with open(OUTPUT_FILE, "w", encoding="utf-8") as f:
        f.write(rpp_content)

    print(f"Generated REAPER multitrack project at {OUTPUT_FILE} (22 armed tracks, 48kHz 24-bit, auto-save 300s).")

if __name__ == "__main__":
    generate_rpp()
