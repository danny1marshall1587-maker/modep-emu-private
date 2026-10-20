#!/usr/bin/env python3
"""
patchstorage_manager.py
A comprehensive Cloud and Patchstorage Manager for MODEP Desktop on Windows.
Allows searching, browsing, and 1-click downloading/installing of community
pedalboards, LV2 plugins, NAM/AIDA-X neural models, and impulse responses.
"""

import os
import sys
import json
import shutil
import tarfile
import zipfile
import urllib.request
import urllib.parse
import threading
import http.server
import socketserver
import webbrowser

USER_DOCS = os.path.expanduser(r'~\Documents')
MOD_DATA_DIR = os.path.join(USER_DOCS, 'MOD Desktop')
PEDALBOARDS_DIR = os.path.join(MOD_DATA_DIR, 'pedalboards')
LV2_DIR = os.path.join(MOD_DATA_DIR, 'lv2')
USER_FILES_DIR = os.path.join(MOD_DATA_DIR, 'user-files')
NAM_DIR = os.path.join(USER_FILES_DIR, 'NAM Models')
AIDAX_DIR = os.path.join(USER_FILES_DIR, 'AIDA-X Models')
CAB_IR_DIR = os.path.join(USER_FILES_DIR, 'Speaker Cabinets IRs')
REVERB_IR_DIR = os.path.join(USER_FILES_DIR, 'Reverb IRs')
SOUNDFONTS_DIR = os.path.join(USER_FILES_DIR, 'SoundFonts')

PORT = 18189

# Ensure all destination directories exist
for d in [PEDALBOARDS_DIR, LV2_DIR, USER_FILES_DIR, NAM_DIR, AIDAX_DIR, CAB_IR_DIR, REVERB_IR_DIR, SOUNDFONTS_DIR]:
    os.makedirs(d, exist_ok=True)

HEADERS = {'User-Agent': 'MODEP-Desktop-Manager/1.0'}

def fetch_json(url):
    req = urllib.request.Request(url, headers=HEADERS)
    with urllib.request.urlopen(req, timeout=12) as resp:
        return json.loads(resp.read().decode('utf-8'))

def search_patchstorage(query='', platform_id=None, page=1):
    base_url = 'https://patchstorage.com/api/beta/patches'
    params = {'page': page, 'per_page': 20}
    if query:
        params['q'] = query
    if platform_id:
        params['platforms'] = platform_id
    
    url = f"{base_url}?{urllib.parse.urlencode(params)}"
    try:
        patches = fetch_json(url)
        return {"status": "ok", "data": patches}
    except Exception as e:
        return {"status": "error", "message": str(e)}

def get_patch_details(patch_id):
    url = f"https://patchstorage.com/api/beta/patches/{patch_id}"
    try:
        details = fetch_json(url)
        return {"status": "ok", "data": details}
    except Exception as e:
        return {"status": "error", "message": str(e)}

def install_patch_file(file_url, filename, patch_title=""):
    print(f"[*] Downloading: {filename} from {file_url}...")
    req = urllib.request.Request(file_url, headers=HEADERS)
    temp_path = os.path.join(MOD_DATA_DIR, '.cache', filename)
    os.makedirs(os.path.dirname(temp_path), exist_ok=True)
    
    with urllib.request.urlopen(req, timeout=30) as resp, open(temp_path, 'wb') as out_f:
        shutil.copyfileobj(resp, out_f)
    
    installed_type = "file"
    destination = USER_FILES_DIR
    lower_name = filename.lower()

    # 1. Neural Amp Modeler (.nam)
    if lower_name.endswith('.nam'):
        dest_file = os.path.join(NAM_DIR, filename)
        shutil.move(temp_path, dest_file)
        installed_type = "NAM Model"
        destination = NAM_DIR

    # 2. AIDA-X Model (.aidax / json with aidax)
    elif lower_name.endswith('.aidax'):
        dest_file = os.path.join(AIDAX_DIR, filename)
        shutil.move(temp_path, dest_file)
        installed_type = "AIDA-X Model"
        destination = AIDAX_DIR

    # 3. SoundFont (.sf2 / .sf3)
    elif lower_name.endswith('.sf2') or lower_name.endswith('.sf3'):
        dest_file = os.path.join(SOUNDFONTS_DIR, filename)
        shutil.move(temp_path, dest_file)
        installed_type = "SoundFont"
        destination = SOUNDFONTS_DIR

    # 4. Impulse Response WAV (.wav)
    elif lower_name.endswith('.wav'):
        if 'reverb' in lower_name or 'hall' in lower_name or 'room' in lower_name:
            dest_file = os.path.join(REVERB_IR_DIR, filename)
            destination = REVERB_IR_DIR
            installed_type = "Reverb IR"
        else:
            dest_file = os.path.join(CAB_IR_DIR, filename)
            destination = CAB_IR_DIR
            installed_type = "Speaker Cab IR"
        shutil.move(temp_path, dest_file)

    # 5. Archive (.zip, .tar.gz, .tgz, .tar)
    elif lower_name.endswith('.zip') or lower_name.endswith('.tar.gz') or lower_name.endswith('.tgz') or lower_name.endswith('.tar'):
        # Extract and analyze contents
        extract_dir = os.path.join(MOD_DATA_DIR, '.cache', '_unpack_' + os.path.splitext(filename)[0])
        os.makedirs(extract_dir, exist_ok=True)

        if lower_name.endswith('.zip'):
            with zipfile.ZipFile(temp_path, 'r') as zf:
                zf.extractall(extract_dir)
        else:
            with tarfile.open(temp_path, 'r:*') as tf:
                tf.extractall(extract_dir)

        # Look for .pedalboard, .lv2, or individual assets
        items = os.listdir(extract_dir)
        for item in items:
            item_path = os.path.join(extract_dir, item)
            item_lower = item.lower()
            if item_lower.endswith('.pedalboard') or os.path.exists(os.path.join(item_path, 'manifest.ttl')):
                if item_lower.endswith('.pedalboard'):
                    target = os.path.join(PEDALBOARDS_DIR, item)
                    if os.path.exists(target):
                        shutil.rmtree(target)
                    shutil.move(item_path, target)
                    installed_type = "Pedalboard"
                    destination = PEDALBOARDS_DIR
                else: # LV2 plugin
                    target = os.path.join(LV2_DIR, item if item_lower.endswith('.lv2') else item + '.lv2')
                    if os.path.exists(target):
                        shutil.rmtree(target)
                    shutil.move(item_path, target)
                    installed_type = "LV2 Plugin"
                    destination = LV2_DIR
            elif item_lower.endswith('.nam'):
                shutil.move(item_path, os.path.join(NAM_DIR, item))
                installed_type = "NAM Model"
                destination = NAM_DIR
            elif item_lower.endswith('.aidax'):
                shutil.move(item_path, os.path.join(AIDAX_DIR, item))
                installed_type = "AIDA-X Model"
                destination = AIDAX_DIR
            elif item_lower.endswith('.wav'):
                shutil.move(item_path, os.path.join(CAB_IR_DIR, item))
                installed_type = "Speaker Cab IR"
                destination = CAB_IR_DIR

        # Cleanup
        shutil.rmtree(extract_dir, ignore_errors=True)
        if os.path.exists(temp_path):
            os.remove(temp_path)
    else:
        # Generic user file
        dest_file = os.path.join(USER_FILES_DIR, filename)
        shutil.move(temp_path, dest_file)
        installed_type = "User File"

    print(f"[+] Installed as {installed_type} into: {destination}")
    return {"status": "ok", "type": installed_type, "destination": destination, "filename": filename}

def list_installed():
    return {
        "pedalboards": os.listdir(PEDALBOARDS_DIR) if os.path.exists(PEDALBOARDS_DIR) else [],
        "plugins": os.listdir(LV2_DIR) if os.path.exists(LV2_DIR) else [],
        "nam_models": os.listdir(NAM_DIR) if os.path.exists(NAM_DIR) else [],
        "aidax_models": os.listdir(AIDAX_DIR) if os.path.exists(AIDAX_DIR) else [],
        "speaker_irs": os.listdir(CAB_IR_DIR) if os.path.exists(CAB_IR_DIR) else [],
        "soundfonts": os.listdir(SOUNDFONTS_DIR) if os.path.exists(SOUNDFONTS_DIR) else []
    }

HTML_TEMPLATE = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>MODEP Desktop Cloud & Patchstorage Manager</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
  :root {
    --bg: #121418;
    --surface: #1e222b;
    --surface-hover: #282e3a;
    --accent: #00e5ff;
    --accent-gold: #ffb300;
    --text: #e6edf3;
    --text-muted: #8b949e;
    --border: #30363d;
    --success: #2ea043;
  }
  * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
  body { background: var(--bg); color: var(--text); padding: 20px; }
  header { display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid var(--border); padding-bottom: 16px; margin-bottom: 20px; }
  h1 { font-size: 22px; color: var(--text); display: flex; align-items: center; gap: 10px; }
  .badge { background: #0056b3; color: #fff; padding: 4px 8px; border-radius: 6px; font-size: 12px; font-weight: bold; }
  .tabs { display: flex; gap: 10px; margin-bottom: 20px; }
  .tab-btn { background: var(--surface); color: var(--text-muted); border: 1px solid var(--border); padding: 10px 18px; border-radius: 8px; cursor: pointer; font-weight: 600; }
  .tab-btn.active { background: var(--accent); color: #000; border-color: var(--accent); }
  .search-bar { display: flex; gap: 10px; margin-bottom: 20px; }
  input[type="text"] { flex: 1; background: var(--surface); border: 1px solid var(--border); color: #fff; padding: 12px 16px; border-radius: 8px; font-size: 15px; outline: none; }
  input[type="text"]:focus { border-color: var(--accent); }
  button.action-btn { background: var(--accent); color: #000; border: none; padding: 12px 20px; border-radius: 8px; cursor: pointer; font-weight: bold; }
  .platforms { display: flex; gap: 8px; flex-wrap: wrap; margin-bottom: 20px; }
  .chip { background: var(--surface); border: 1px solid var(--border); color: var(--text-muted); padding: 6px 12px; border-radius: 16px; font-size: 13px; cursor: pointer; }
  .chip.active { background: #004d61; color: var(--accent); border-color: var(--accent); }
  .grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(320px, 1fr)); gap: 16px; }
  .card { background: var(--surface); border: 1px solid var(--border); border-radius: 10px; padding: 16px; display: flex; flex-direction: column; justify-content: space-between; transition: 0.2s; }
  .card:hover { border-color: var(--accent); background: var(--surface-hover); }
  .card h3 { font-size: 16px; margin-bottom: 6px; color: #fff; }
  .card p { font-size: 13px; color: var(--text-muted); line-height: 1.4; margin-bottom: 12px; flex: 1; }
  .card-meta { display: flex; justify-content: space-between; font-size: 12px; color: var(--text-muted); margin-bottom: 12px; }
  .btn-install { background: var(--success); color: #fff; border: none; padding: 8px 14px; border-radius: 6px; cursor: pointer; font-weight: bold; font-size: 13px; width: 100%; transition: 0.2s; }
  .btn-install:hover { background: #3fb950; }
  .toast { position: fixed; bottom: 20px; right: 20px; background: var(--success); color: #fff; padding: 12px 20px; border-radius: 8px; font-weight: bold; box-shadow: 0 4px 12px rgba(0,0,0,0.4); display: none; z-index: 1000; }
  .installed-list { background: var(--surface); border: 1px solid var(--border); border-radius: 8px; padding: 16px; margin-bottom: 16px; }
  .installed-list h4 { margin-bottom: 10px; color: var(--accent); }
  .tag { background: #21262d; border: 1px solid var(--border); padding: 4px 8px; border-radius: 4px; font-size: 12px; margin-right: 6px; }
</style>
</head>
<body>

<header>
  <h1><span>⚡</span> MODEP Desktop Cloud & Patchstorage Manager <span class="badge">Windows</span></h1>
  <div>
    <a href="http://127.0.0.1:18181" target="_blank" style="color: var(--accent); text-decoration: none; font-weight: bold; margin-right: 15px;">Open MODEP Web GUI &rarr;</a>
    <button onclick="openFolder()" class="tab-btn" style="padding: 6px 12px; font-size: 12px;">📂 Open Documents Folder</button>
  </div>
</header>

<div class="tabs">
  <button class="tab-btn active" onclick="switchTab('patchstorage')">☁️ Patchstorage Community</button>
  <button class="tab-btn" onclick="switchTab('installed')">📦 Installed Library</button>
</div>

<div id="tab-patchstorage">
  <div class="search-bar">
    <input type="text" id="searchInput" placeholder="Search patches, pedals, NAM amps, guitar presets, synths..." onkeydown="if(event.key==='Enter') search();">
    <button class="action-btn" onclick="search()">Search</button>
  </div>

  <div class="platforms">
    <span class="chip active" onclick="setPlatform('', this)">All Categories</span>
    <span class="chip" onclick="setPlatform('8046', this)">LV2 Plugins</span>
    <span class="chip" onclick="setPlatform('10361', this)">PiPedal / Guitar Presets</span>
    <span class="chip" onclick="setPlatform('1558', this)">Pisound / Patchbox</span>
    <span class="chip" onclick="setPlatform('7834', this)">Cardinal Modular</span>
    <span class="chip" onclick="setPlatform('5760', this)">Zynthian Rigs</span>
  </div>

  <div class="grid" id="resultsGrid">
    <p style="color: var(--text-muted);">Loading patches from Patchstorage...</p>
  </div>
</div>

<div id="tab-installed" style="display: none;">
  <div id="installedContent">Loading installed library...</div>
</div>

<div class="toast" id="toast">Installed successfully!</div>

<script>
let currentPlatform = '';

function switchTab(tab) {
  document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
  event.target.classList.add('active');
  if (tab === 'patchstorage') {
    document.getElementById('tab-patchstorage').style.display = 'block';
    document.getElementById('tab-installed').style.display = 'none';
  } else {
    document.getElementById('tab-patchstorage').style.display = 'none';
    document.getElementById('tab-installed').style.display = 'block';
    loadInstalled();
  }
}

function setPlatform(pid, el) {
  currentPlatform = pid;
  document.querySelectorAll('.chip').forEach(c => c.classList.remove('active'));
  el.classList.add('active');
  search();
}

async function search() {
  const q = document.getElementById('searchInput').value;
  const grid = document.getElementById('resultsGrid');
  grid.innerHTML = '<p style="color: var(--text-muted);">Searching Patchstorage...</p>';
  
  try {
    const res = await fetch(`/api/search?q=${encodeURIComponent(q)}&platform=${currentPlatform}`);
    const data = await res.json();
    if (data.status === 'ok' && data.data && data.data.length > 0) {
      grid.innerHTML = data.data.map(p => `
        <div class="card">
          <div>
            <h3>${p.title || 'Untitled Patch'}</h3>
            <div class="card-meta">
              <span>👤 ${p.author ? p.author.name : 'Community'}</span>
              <span>💾 ${p.download_count || 0} downloads</span>
            </div>
            <p>${(p.description || 'No description provided.').replace(/<[^>]*>?/gm, '').slice(0, 140)}...</p>
          </div>
          <button class="btn-install" onclick="installPatch(${p.id}, this)">📥 1-Click Install to MODEP</button>
        </div>
      `).join('');
    } else {
      grid.innerHTML = '<p style="color: var(--text-muted);">No patches found matching your query.</p>';
    }
  } catch (err) {
    grid.innerHTML = `<p style="color: red;">Error loading from Patchstorage: ${err.message}</p>`;
  }
}

async function installPatch(patchId, btn) {
  const originalText = btn.innerText;
  btn.innerText = '⏳ Downloading & Installing...';
  btn.disabled = true;
  
  try {
    const res = await fetch(`/api/install?patch_id=${patchId}`, { method: 'POST' });
    const data = await res.json();
    if (data.status === 'ok') {
      showToast(`✅ Installed: ${data.filename} (${data.type})`);
      btn.innerText = '✅ Installed!';
      btn.style.background = '#238636';
    } else {
      alert('Installation failed: ' + data.message);
      btn.innerText = originalText;
      btn.disabled = false;
    }
  } catch (e) {
    alert('Error: ' + e.message);
    btn.innerText = originalText;
    btn.disabled = false;
  }
}

async function loadInstalled() {
  const content = document.getElementById('installedContent');
  try {
    const res = await fetch('/api/installed');
    const data = await res.json();
    content.innerHTML = `
      <div class="installed-list">
        <h4>🎛️ Installed Pedalboards (${data.pedalboards.length})</h4>
        <div>${data.pedalboards.map(p => `<span class="tag">${p}</span>`).join(' ') || '<span style="color:var(--text-muted)">No custom pedalboards installed yet</span>'}</div>
      </div>
      <div class="installed-list">
        <h4>🔌 Installed LV2 Plugins (${data.plugins.length})</h4>
        <div>${data.plugins.map(p => `<span class="tag">${p}</span>`).join(' ') || '<span style="color:var(--text-muted)">No custom LV2 plugins installed yet</span>'}</div>
      </div>
      <div class="installed-list">
        <h4>🎸 NAM Neural Amp Models (${data.nam_models.length})</h4>
        <div>${data.nam_models.map(p => `<span class="tag">${p}</span>`).join(' ') || '<span style="color:var(--text-muted)">Drop .nam files in NAM Models folder</span>'}</div>
      </div>
      <div class="installed-list">
        <h4>🔊 Speaker Cab IRs (${data.speaker_irs.length})</h4>
        <div>${data.speaker_irs.map(p => `<span class="tag">${p}</span>`).join(' ') || '<span style="color:var(--text-muted)">Drop .wav files in Speaker Cabinets IRs</span>'}</div>
      </div>
    `;
  } catch (e) {
    content.innerHTML = `<p style="color:red;">Error: ${e.message}</p>`;
  }
}

function showToast(msg) {
  const t = document.getElementById('toast');
  t.innerText = msg;
  t.style.display = 'block';
  setTimeout(() => { t.style.display = 'none'; }, 4000);
}

function openFolder() {
  fetch('/api/open_folder', { method: 'POST' });
}

// Initial load
search();
</script>

</body>
</html>
"""

class RequestHandler(http.server.BaseHTTPRequestHandler):
    def log_message(self, format, *args):
        pass # Quiet logging

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        if parsed.path == '/' or parsed.path == '/index.html':
            self.send_response(200)
            self.send_header('Content-Type', 'text/html; charset=utf-8')
            self.end_headers()
            self.wfile.write(HTML_TEMPLATE.encode('utf-8'))
        elif parsed.path == '/api/search':
            qs = urllib.parse.parse_qs(parsed.query)
            q = qs.get('q', [''])[0]
            platform = qs.get('platform', [''])[0]
            res = search_patchstorage(query=q, platform_id=platform if platform else None)
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(json.dumps(res).encode('utf-8'))
        elif parsed.path == '/api/installed':
            res = list_installed()
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(json.dumps(res).encode('utf-8'))
        else:
            self.send_response(404)
            self.end_headers()

    def do_POST(self):
        parsed = urllib.parse.urlparse(self.path)
        if parsed.path == '/api/install':
            qs = urllib.parse.parse_qs(parsed.query)
            patch_id = qs.get('patch_id', [None])[0]
            if not patch_id:
                self.send_response(400)
                self.end_headers()
                return

            details_res = get_patch_details(patch_id)
            if details_res.get('status') != 'ok':
                self.send_response(500)
                self.send_header('Content-Type', 'application/json')
                self.end_headers()
                self.wfile.write(json.dumps(details_res).encode('utf-8'))
                return

            patch_data = details_res['data']
            files = patch_data.get('files', [])
            if not files:
                self.send_response(400)
                self.send_header('Content-Type', 'application/json')
                self.end_headers()
                self.wfile.write(json.dumps({"status": "error", "message": "No downloadable files attached to this patch."}).encode('utf-8'))
                return

            file_info = files[0]
            install_res = install_patch_file(file_info['url'], file_info['filename'], patch_title=patch_data.get('title', ''))
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(json.dumps(install_res).encode('utf-8'))

        elif parsed.path == '/api/open_folder':
            os.startfile(MOD_DATA_DIR)
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(json.dumps({"status": "ok"}).encode('utf-8'))
        else:
            self.send_response(404)
            self.end_headers()

def run_server():
    socketserver.TCPServer.allow_reuse_address = True
    server = socketserver.TCPServer(('127.0.0.1', PORT), RequestHandler)
    print(f"[*] MODEP Cloud & Patchstorage Manager running at http://127.0.0.1:{PORT}")
    server.serve_forever()

if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == '--install':
        if len(sys.argv) > 2:
            patch_id = sys.argv[2]
            det = get_patch_details(patch_id)
            if det.get('status') == 'ok' and det['data'].get('files'):
                f = det['data']['files'][0]
                install_patch_file(f['url'], f['filename'])
            else:
                print("Failed to find patch files.")
        sys.exit(0)

    t = threading.Thread(target=run_server, daemon=True)
    t.start()
    webbrowser.open(f'http://127.0.0.1:{PORT}')
    print("Press Ctrl+C to stop.")
    try:
        while True:
            import time
            time.sleep(1)
    except KeyboardInterrupt:
        print("\nStopping Patchstorage Manager.")
