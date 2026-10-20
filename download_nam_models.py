import os
import sys
import urllib.request
import json

USER_FILES_DIR = os.path.expanduser(r'~\Documents\MOD Desktop\user-files')
USER_MODELS_DIR = os.path.join(USER_FILES_DIR, 'NAM-Models')
USER_AUDIO_DIR = os.path.join(USER_FILES_DIR, 'Audio-Tracks')
USER_IR_DIR = os.path.join(USER_FILES_DIR, 'Speaker-Cabinets-IRs')

for d in [USER_MODELS_DIR, USER_AUDIO_DIR, USER_IR_DIR]:
    os.makedirs(d, exist_ok=True)

print("Fetching community NAM neural models list from GitHub...")
url = 'https://api.github.com/repos/pelennor2170/NAM_models/git/trees/main?recursive=1'
req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
try:
    res = json.loads(urllib.request.urlopen(req, timeout=10).read())
    all_models = [i['path'] for i in res.get('tree', []) if i['path'].endswith('.nam')]
    print(f"Found {len(all_models)} available models. Downloading top popular models...")
    
    # Download top diverse selection of 20 popular amp models
    selected = all_models[:25]
    for m in selected:
        filename = os.path.basename(m)
        target = os.path.join(USER_MODELS_DIR, filename)
        if os.path.exists(target):
            continue
        raw_url = f"https://raw.githubusercontent.com/pelennor2170/NAM_models/main/{urllib.parse.quote(m)}"
        try:
            m_req = urllib.request.Request(raw_url, headers={'User-Agent': 'Mozilla/5.0'})
            with urllib.request.urlopen(m_req, timeout=15) as r, open(target, 'wb') as f:
                f.write(r.read())
            print(f" [+] Downloaded model: {filename}")
        except Exception as e:
            print(f" [!] Error downloading {filename}: {e}")

    print(f"\nSuccessfully stored community NAM models in {USER_MODELS_DIR}")
except Exception as e:
    print("Error querying repo:", e)
