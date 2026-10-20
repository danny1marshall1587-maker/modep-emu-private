#!/usr/bin/env python3
"""
fix_files_list.py
Enhances FilesList in modep_app/mod/webserver.py to support:
1. filetype == "nam", "nammodel", "neuralmodel" -> scans "NAM Models" & "NAM-Models"
2. filetype == "aidax", "aidadspmodel" -> scans "Aida DSP Models" & "AIDA-X Models"
3. Normalizes Windows paths with forward slashes so mod-host/lilv can load atom file paths.
"""

import os
import re

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
WEBSERVER_PY = os.path.join(BASE_DIR, "modep_app", "mod", "webserver.py")

with open(WEBSERVER_PY, "r", encoding="utf-8") as f:
    code = f.read()

# Replace FilesList implementation with enhanced version
old_files_list_pattern = r"class FilesList\(JsonRequestHandler\):[\s\S]*?(?=settings\s*=\s*\{|\Z)"

new_files_list = '''class FilesList(JsonRequestHandler):
    complete_audiofile_exts = (
        # through libsndfile
        ".aif", ".aifc", ".aiff", ".au", ".bwf", ".flac", ".htk", ".iff", ".mat4", ".mat5", ".oga", ".ogg", ".opus",
        ".paf", ".pvf", ".pvf5", ".sd2", ".sf", ".snd", ".svx", ".vcc", ".w64", ".wav", ".xi",
        # extra through ffmpeg
        ".3g2", ".3gp", ".aac", ".ac3", ".amr", ".ape", ".mp2", ".mp3", ".mpc", ".wma",
    )

    hq_audiofile_exts = (
        ".aif", ".aifc", ".aiff", ".flac", ".w64", ".wav",
    )

    @classmethod
    def _get_dirs_and_extensions_for_filetype(kls, filetype):
        ft = filetype.lower().strip()
        if ft == "audioloop":
            return (("Audio Loops",), kls.complete_audiofile_exts)

        elif ft == "audiorecording":
            return (("Audio Recordings",), kls.complete_audiofile_exts)

        elif ft == "audiosample":
            return (("Audio Samples",), kls.complete_audiofile_exts)

        elif ft == "audiotrack":
            return (("Audio Tracks", "Audio-Tracks"), kls.complete_audiofile_exts)

        elif ft in ("cabsim", "speakercabinet"):
            return (("Speaker Cabinets IRs", "Speaker-Cabinets-IRs", "Speaker Cabinets"), kls.hq_audiofile_exts)

        elif ft == "h2drumkit":
            return (("Hydrogen Drumkits",), (".h2drumkit",))

        elif ft == "ir":
            return (("Reverb IRs", "Reverb-IRs"), kls.hq_audiofile_exts)

        elif ft == "midiclip":
            return (("MIDI Clips", "MIDI-Clips"), (".mid", ".midi"))

        elif ft == "midisong":
            return (("MIDI Songs", "MIDI-Songs", "MIDI Files"), (".mid", ".midi"))

        elif ft == "sf2":
            return (("SF2 Instruments", "SoundFonts"), (".sf2", ".sf3"))

        elif ft == "sfz":
            return (("SFZ Instruments",), (".sfz",))

        elif ft in ("aidadspmodel", "aidax", "aida"):
            return (("Aida DSP Models", "AIDA-X Models", "AIDA-X"), (".aidax", ".json", ".aidadspmodel"))

        elif ft in ("nammodel", "nam", "neuralmodel", "neuralamp"):
            return (("NAM Models", "NAM-Models", "Amplifier Profiles"), (".nam", ".nammodel"))

        else:
            return ((), ())

    def prepare(self):
        filetypes = self.get_argument('types', None)
        if filetypes is None:
            raise web.HTTPError(501, "Missing types")

        self.filetypes = filetypes.split(",")

    def get(self):
        retfiles = {}
        fullnames = []

        for filetype in self.filetypes:
            datadirs, extensions = self._get_dirs_and_extensions_for_filetype(filetype)

            for datadir in datadirs:
                target_path = os.path.join(USER_FILES_DIR, datadir)
                if not os.path.exists(target_path):
                    continue

                for root, dirs, files in os.walk(target_path):
                    for name in sorted(files):
                        if name.lower().endswith(extensions):
                            fullname = os.path.normpath(os.path.join(root, name)).replace(os.sep, '/')
                            if fullname not in retfiles:
                                fullnames.append(fullname)
                                retfiles[fullname] = {
                                    'fullname': fullname,
                                    'basename': name,
                                    'filetype': filetype,
                                }

        fullnames.sort()

        self.write({
            'ok': True,
            'files': tuple(retfiles[fn] for fn in fullnames),
        })

'''

code = re.sub(old_files_list_pattern, new_files_list, code)

with open(WEBSERVER_PY, "w", encoding="utf-8") as f:
    f.write(code)

print(" [+] Successfully enhanced FilesList in webserver.py!")
