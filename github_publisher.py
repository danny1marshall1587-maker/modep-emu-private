#!/usr/bin/env python3
"""
github_publisher.py
Complete pure Python GitHub Publisher for MODEP plugins.
Initializes repos, commits code via Git Data API, and publishes Releases with binaries.
"""

import os
import sys
import json
import base64
import urllib.request
import urllib.error
import shutil
import time
from concurrent.futures import ThreadPoolExecutor, as_completed

sys.stdout.reconfigure(line_buffering=True)

GITHUB_TOKEN = "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"
GITHUB_USER = "danny1marshall1587-maker"
BASE_DIR = os.path.dirname(os.path.abspath(__file__))

HEADERS = {
    "Authorization": f"token {GITHUB_TOKEN}",
    "User-Agent": "MODEP-Publisher",
    "Accept": "application/vnd.github.v3+json",
    "Content-Type": "application/json"
}

def api(url, data=None, method=None, content_type="application/json"):
    hdrs = HEADERS.copy()
    if content_type:
        hdrs["Content-Type"] = content_type
    elif "Content-Type" in hdrs:
        del hdrs["Content-Type"]

    body = None
    if data is not None:
        if isinstance(data, (dict, list)):
            body = json.dumps(data).encode("utf-8")
        elif isinstance(data, (bytes, bytearray)):
            body = data
        else:
            body = str(data).encode("utf-8")

    req = urllib.request.Request(url, data=body, headers=hdrs, method=method)
    try:
        with urllib.request.urlopen(req) as resp:
            resp_b = resp.read()
            if not resp_b:
                return {"status": resp.status}
            try:
                return json.loads(resp_b.decode("utf-8"))
            except Exception:
                return {"raw": resp_b}
    except urllib.error.HTTPError as e:
        err_msg = e.read().decode("utf-8", errors="replace")
        return {"error": e.code, "message": err_msg}
    except Exception as e:
        return {"error": -1, "message": str(e)}

def ensure_repo(repo_name, description):
    print(f"[*] Ensuring repo: {repo_name}...")
    res = api("https://api.github.com/user/repos", {
        "name": repo_name,
        "description": description,
        "private": False,
        "has_issues": True,
        "has_projects": True,
        "has_wiki": False
    })
    
    # Initialize if empty
    check_ref = api(f"https://api.github.com/repos/{GITHUB_USER}/{repo_name}/git/refs/heads/main")
    if "error" in check_ref:
        # Create initial README via Contents API
        readme_url = f"https://api.github.com/repos/{GITHUB_USER}/{repo_name}/contents/README.md"
        init_content = base64.b64encode(f"# {repo_name}\n\n{description}\n".encode("utf-8")).decode("ascii")
        api(readme_url, {
            "message": f"Initial commit for {repo_name}",
            "content": init_content,
            "branch": "main"
        }, method="PUT")
        print(f" [+] Initialized main branch for {repo_name}")
    else:
        print(f" [*] Repo {repo_name} ready.")

def upload_blob(repo_name, full_path, rel_path):
    try:
        with open(full_path, "rb") as f:
            b64 = base64.b64encode(f.read()).decode("ascii")
        url = f"https://api.github.com/repos/{GITHUB_USER}/{repo_name}/git/blobs"
        res = api(url, {"content": b64, "encoding": "base64"})
        sha = res.get("sha")
        if sha:
            clean_path = rel_path.replace("\\", "/")
            return {"path": clean_path, "mode": "100644", "type": "blob", "sha": sha}
    except Exception as e:
        print(f" [!] Error uploading blob {rel_path}: {e}")
    return None

def publish_code(file_map, repo_name, commit_msg="Release v1.0.0"):
    print(f"[*] Uploading {len(file_map)} files to {repo_name} in parallel...")
    tree_items = []
    
    with ThreadPoolExecutor(max_workers=8) as executor:
        futures = {executor.submit(upload_blob, repo_name, fp, rp): rp for fp, rp in file_map}
        for fut in as_completed(futures):
            item = fut.result()
            if item:
                tree_items.append(item)

    ref_res = api(f"https://api.github.com/repos/{GITHUB_USER}/{repo_name}/git/refs/heads/main")
    parent_sha = ref_res.get("object", {}).get("sha")

    tree_payload = {"tree": tree_items}
    if parent_sha:
        tree_payload["base_tree"] = parent_sha

    tree_res = api(f"https://api.github.com/repos/{GITHUB_USER}/{repo_name}/git/trees", tree_payload)
    tree_sha = tree_res.get("sha")
    if not tree_sha:
        print(f" [!] Error creating tree in {repo_name}: {tree_res}")
        return None

    commit_payload = {
        "message": commit_msg,
        "tree": tree_sha,
        "parents": [parent_sha] if parent_sha else []
    }
    commit_res = api(f"https://api.github.com/repos/{GITHUB_USER}/{repo_name}/git/commits", commit_payload)
    commit_sha = commit_res.get("sha")

    api(f"https://api.github.com/repos/{GITHUB_USER}/{repo_name}/git/refs/heads/main", {
        "sha": commit_sha,
        "force": True
    }, method="PATCH")
    print(f" [+] Pushed commit {commit_sha[:7]} to {repo_name}:main")
    return commit_sha

def publish_release(repo_name, tag_name="v1.0.0", title="v1.0.0 Initial Release", body="Release of LV2 plugin for MODEP.", asset_paths=None):
    print(f"[*] Creating Release {tag_name} for {repo_name}...")
    url = f"https://api.github.com/repos/{GITHUB_USER}/{repo_name}/releases"
    res = api(url, {
        "tag_name": tag_name,
        "target_commitish": "main",
        "name": title,
        "body": body,
        "draft": False,
        "prerelease": False
    })
    
    upload_url = None
    if "upload_url" in res:
        upload_url = res["upload_url"].split("{")[0]
    else:
        # Check existing
        r_list = api(f"https://api.github.com/repos/{GITHUB_USER}/{repo_name}/releases")
        if isinstance(r_list, list) and len(r_list) > 0:
            upload_url = r_list[0].get("upload_url", "").split("{")[0]

    if upload_url and asset_paths:
        for ap in asset_paths:
            if not os.path.exists(ap):
                continue
            fname = os.path.basename(ap)
            print(f"   [*] Uploading asset: {fname}...")
            with open(ap, "rb") as f:
                bdata = f.read()
            c_type = "application/gzip" if fname.endswith(".tar.gz") else "application/zip"
            up_res = api(f"{upload_url}?name={fname}", data=bdata, content_type=c_type)
            if "browser_download_url" in up_res:
                print(f"    [+] Uploaded: {up_res['browser_download_url']}")
            elif up_res.get("error") == 422:
                print(f"    [*] Asset {fname} already attached.")
            else:
                print(f"    [!] Asset upload error: {up_res}")

def main():
    print("================================================================")
    print("      Publishing MODEP Plugins & Releases to GitHub             ")
    print("================================================================")

    # 1. Master Collection Repo
    coll_repo = "modep-plugin-collection"
    coll_desc = "Complete MODEP & MOD Desktop LV2 Guitar Plugin Collection for Windows & Patchbox OS"
    ensure_repo(coll_repo, coll_desc)

    coll_files = []
    root_files = ['README.md', 'build_windows_plugins.py', 'deploy_plugins_to_desktop.py', 
                  'package_bundles.py', 'generate_modgui_images.py', '.gitignore',
                  '.github/workflows/build-all.yml']
    for rf in root_files:
        p = os.path.join(BASE_DIR, rf)
        if os.path.exists(p):
            coll_files.append((p, rf.replace('\\', '/')))

    # Include root include headers
    inc_root = os.path.join(BASE_DIR, 'include')
    if os.path.exists(inc_root):
        for root, dirs, files in os.walk(inc_root):
            for f in files:
                fp = os.path.join(root, f)
                rp = os.path.relpath(fp, BASE_DIR).replace('\\', '/')
                coll_files.append((fp, rp))

    plugins_dir = os.path.join(BASE_DIR, 'plugins')
    for root, dirs, files in os.walk(plugins_dir):
        # Exclude shelved or deprecated plugins
        if 'cyber-expression-cv.lv2' in root or 'mains-hum-killer.lv2' in root:
            continue
        for f in files:
            # Exclude compiled binaries from git tree
            if f.endswith('.dll') or f.endswith('.so') or f.endswith('.exe') or f.endswith('.o'):
                continue
            fp = os.path.join(root, f)
            rp = os.path.relpath(fp, BASE_DIR).replace('\\', '/')
            coll_files.append((fp, rp))

    publish_code(coll_files, coll_repo, commit_msg="Update: Full Plugin Suite with Mono/Stereo Counterparts & GitHub Actions CI")

    coll_assets = [
        os.path.join(BASE_DIR, "modep_guitar_plugins_bundle.zip"),
        os.path.join(BASE_DIR, "modep_guitar_plugins_bundle.tar.gz")
    ]
    publish_release(coll_repo, tag_name="v1.0.0", title="MODEP Guitar Plugins Bundle v1.0.0", body="""# MODEP & MOD Desktop Guitar Plugin Collection v1.0.0

A complete collection of 12 pro-grade LV2 audio plugins for MODEP (Patchbox OS / Raspberry Pi) and MOD Desktop (Windows / Linux / macOS):

### Included Plugins:
1. **Smart Fizz Killer PRO** (`smart-fizz-killer.lv2`) - 4-stage digital guitar smoother with phase smear, dynamic fizz cut, tanh soft clipper, and tilt EQ.
2. **Cyber Hum Killer PRO** (`cyber-hum-killer.lv2`) - 8-harmonic mains hum (50Hz/60Hz) and DC power supply rumble suppressor with Learn Hum calibration.
3. **Cyber Denoiser PRO** (`cyber-denoiser.lv2`) - 10-band forensic spectral phase-subtraction denoiser with 5s statistical Learn Noise calibration.
4. **NAM Loader & Tone 3000** (`nam-loader.lv2`) - Neural Amp Modeler profile loader with built-in Tone 3000 library.
5. **Aether Reverb** (`aether.lv2`) - Dougal Stewart / Cloudseed algorithmic stereo reverb.
6. **Bluesbreaker** (`bluesbreaker.lv2`) - Vintage Marshall BB-1 overdrive pedal emulation.
7. **Galaxy Strobe Tuner** (`galaxy-strobe-tune.lv2`) - High-precision real-time strobe guitar tuner.
8. **Dimension-C Chorus** (`dimension-c.lv2`) - Roland/BOSS Dimension-C 4-button spatial chorus.
9. **Dimension IV** (`Dimension_IV.lv2`) - Multi-mode analog spatial chorus.
10. **Dearmondo 610** (`Dearmondo610.lv2`) - Vintage optical tremolo/volume pedal.
11. **Guitar-to-MIDI** (`guitar-midi.lv2`) - Low-latency polyphonic guitar-to-MIDI synth converter.
12. **Harmonic Tremolo** (`harmonic-tremolo.lv2`) - Vintage brownface dual-band harmonic tremolo.

### Installation:
- **MOD Desktop (Windows)**: Extract `modep_guitar_plugins_bundle.zip` into `C:\\Users\\<User>\\Documents\\MOD Desktop\\lv2\\`
- **MODEP (Patchbox OS)**: Extract `modep_guitar_plugins_bundle.tar.gz` into `/var/modep/lv2/` or `~/.lv2/`
""", asset_paths=coll_assets)

    # 2. Individual Repositories
    plugins_to_publish = [
        ("smart-fizz-killer-lv2", "plugins/smart-fizz-killer.lv2", "Smart Fizz Killer PRO: 4-stage digital guitar smoothing LV2 plugin for MODEP & MOD Desktop"),
        ("cyber-hum-killer-lv2", "plugins/cyber-hum-killer.lv2", "Cyber Hum Killer PRO: 8-harmonic mains hum (50Hz/60Hz) & DC suppressor LV2 plugin for MODEP & MOD Desktop"),
        ("cyber-denoiser-lv2", "plugins/cyber-denoiser", "Cyber Denoiser PRO: 10-band forensic spectral subtractive denoiser LV2 plugin for MODEP & MOD Desktop"),
        ("nam-loader-lv2", "plugins/nam-loader.lv2", "NAM Loader: Neural Amp Modeler & Tone 3000 capture browser LV2 plugin for MODEP & MOD Desktop"),
        ("dimension-c-lv2", "plugins/dimension-c", "Dimension-C: 4-mode spatial bucket-brigade chorus LV2 plugin for MODEP & MOD Desktop"),
        ("guitar-midi-lv2", "plugins/guitar-midi", "Guitar-to-MIDI: Low-latency guitar-to-MIDI polyphonic chord converter LV2 plugin for MODEP & MOD Desktop"),
        ("harmonic-tremolo-lv2", "plugins/harmonic-tremolo", "Harmonic Tremolo: Vintage dual-band harmonic optical tremolo LV2 plugin for MODEP & MOD Desktop"),
        ("aether-reverb-lv2", "plugins/aether.lv2", "Aether Reverb: Cloudseed algorithmic stereo ambient reverb LV2 plugin for MODEP & MOD Desktop"),
        ("bluesbreaker-lv2", "plugins/bluesbreaker.lv2", "Bluesbreaker: Vintage BB-1 overdrive pedal emulation LV2 plugin for MODEP & MOD Desktop")
    ]

    for repo_name, rel_dir, desc in plugins_to_publish:
        full_dir = os.path.join(BASE_DIR, rel_dir)
        if not os.path.exists(full_dir):
            continue
        
        ensure_repo(repo_name, desc)
        file_map = []
        for root, dirs, files in os.walk(full_dir):
            for f in files:
                fp = os.path.join(root, f)
                rp = os.path.relpath(fp, full_dir).replace('\\', '/')
                file_map.append((fp, rp))
        
        publish_code(file_map, repo_name, commit_msg=f"Release {repo_name} v1.0.0")
        
        # Build individual zip
        dist_zip = os.path.join(BASE_DIR, "dist", f"{repo_name}.zip")
        shutil.make_archive(os.path.join(BASE_DIR, "dist", repo_name), 'zip', full_dir)

        publish_release(repo_name, tag_name="v1.0.0", title=f"{repo_name} v1.0.0", body=f"# {repo_name}\n\n{desc}\n\nBuilt for MODEP and MOD Desktop.", asset_paths=[dist_zip])

    print("\n================================================================")
    print("   [SUCCESS] All Plugin Repositories & Releases Published!      ")
    print("================================================================")

if __name__ == '__main__':
    main()
