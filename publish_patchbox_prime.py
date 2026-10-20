import os
import json
import base64
import tarfile
import zipfile
import urllib.request
import urllib.error
import time
from concurrent.futures import ThreadPoolExecutor, as_completed

BASE_DIR = r"c:\Users\danny\Documents\modep emu"
GITHUB_USER = "danny1marshall1587-maker"

TOKENS = [
    "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",
    "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",
    "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"
]

def find_working_token():
    for tok in TOKENS:
        req = urllib.request.Request("https://api.github.com/user", headers={
            "User-Agent": "PatchboxPrime-Publisher",
            "Authorization": f"token {tok}",
            "Accept": "application/vnd.github.v3+json"
        })
        try:
            with urllib.request.urlopen(req) as resp:
                data = json.loads(resp.read().decode("utf-8"))
                scopes = resp.headers.get("X-OAuth-Scopes", "")
                print(f"[+] Token valid for {data.get('login')} (scopes: {scopes})")
                return tok
        except Exception as e:
            print(f"[-] Token check failed: {e}")
    raise RuntimeError("No valid GitHub token found")

ACTIVE_TOKEN = find_working_token()

def api(url, data=None, method=None, content_type="application/json"):
    hdrs = {
        "Authorization": f"token {ACTIVE_TOKEN}",
        "User-Agent": "PatchboxPrime-Publisher",
        "Accept": "application/vnd.github.v3+json"
    }
    if content_type:
        hdrs["Content-Type"] = content_type

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

# 1. Build Release Image Archives (.tar.gz and .zip)
tar_path = os.path.join(BASE_DIR, "patchbox_prime_v3.0_release.tar.gz")
zip_path = os.path.join(BASE_DIR, "patchbox_prime_v3.0_release.zip")

def collect_project_files():
    """Returns list of (abs_path, repo_rel_path) for Patchbox Prime."""
    items = []
    # Root docs & service files
    root_files = [
        "PROJECT_INDEX.md",
        "AGENT_RULES.md",
        "README.md",
        "CMakeLists.txt",
        "Makefile",
        "mixer_meter_daemon.c",
        "modep-meter-daemon.service",
        "patchbox-reaper.service",
        "install_reaper.sh",
        "install_to_modep.sh",
        "build_patchbox_prime.py",
        "verify_patchbox_prime.py"
    ]
    for rf in root_files:
        fp = os.path.join(BASE_DIR, rf)
        if os.path.exists(fp):
            items.append((fp, rf))

    # C++ JACK DSP sources in src/
    src_dir = os.path.join(BASE_DIR, "src")
    if os.path.isdir(src_dir):
        for root, _, files in os.walk(src_dir):
            for f in files:
                fp = os.path.join(root, f)
                rel = os.path.relpath(fp, BASE_DIR).replace("\\", "/")
                items.append((fp, rel))

    # Live Web Console in web/web/ mapped to web/
    web_root = os.path.join(BASE_DIR, "web", "web")
    if os.path.isdir(web_root):
        for root, _, files in os.walk(web_root):
            for f in files:
                if f.endswith((".bak", ".orig", ".tmp")) or ".bak." in f:
                    continue
                fp = os.path.join(root, f)
                rel_inside_web = os.path.relpath(fp, web_root).replace("\\", "/")
                items.append((fp, f"web/{rel_inside_web}"))

    return items

project_files = collect_project_files()
print(f"[*] Collected {len(project_files)} source & console files for Patchbox Prime.")

with tarfile.open(tar_path, "w:gz") as tar:
    for fp, rel in project_files:
        tar.add(fp, arcname=f"patchbox-prime/{rel}")
print(f"[+] Built release image tarball: {tar_path} ({os.path.getsize(tar_path)} bytes)")

with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
    for fp, rel in project_files:
        zf.write(fp, arcname=f"patchbox-prime/{rel}")
print(f"[+] Built release image zip: {zip_path} ({os.path.getsize(zip_path)} bytes)")

# 2. Update README.md so it links directly to PROJECT_INDEX.md and AGENT_RULES.md
readme_path = os.path.join(BASE_DIR, "README.md")
readme_content = """# Patchbox Prime — Pro Live Digital Mixing Console, 1176 Preamp & Dual-Mode MODEP LV2 Workstation

**Patchbox Prime (v3.0)** is an ultra-low-latency 8-Channel / 18-Input Live Digital Mixing Console, Studio Desk DSP Strip, 4-Slot Auto-Routed MODEP LV2 Insert Rack, and Embedded REAPER Multitrack Workstation for Raspberry Pi 4 + Behringer UMC1820.

## 📚 Quick Reference & Architectural Index
- **[`PROJECT_INDEX.md`](PROJECT_INDEX.md)**: Complete 325-line architectural reference covering the "whys and wherefores" of the system, Hardware/JACK topology, 9-stage per-channel signal flow (`Stage 1: 1176 FET Preamp` ➔ `Stage 2: Digital Gain + REGAIN` ➔ `Core Desk DSP` ➔ `4 Auto-Routed LV2 Slots / Modular Canvas`), DOM ID lookup table, and API reference.
- **[`AGENT_RULES.md`](AGENT_RULES.md)**: Strict engineering invariants (zero simulated meters, `TouchGovernor` slew limiting, `< 450` lines per JS module, DPI-aware canvas rendering).

## ⚡ Key Highlights in v3.0 (Patchbox Prime)
1. **Stage 1: 1176 Class-A FET Preamp Emulation (`Before REGAIN`)**:
   - Dedicated **1176 Input Drive (`0` to `+36 dB`)**, selectable **Soft FET Saturation / Hard Diode Clipping**, real-time telemetry-driven **`CLEAN / FET SAT / CLIP!` indicator**, and **Post-FET Output Trim (`-24` to `+12 dB`)** feeding directly into **Stage 2: Digital Gain + Automatic REGAIN + Phase Invert (`Ø`)**.
2. **4 Auto-Routed Linear MODEP LV2 / MOD-GUI Insert Slots (`RACK SLOTS` Mode)**:
   - Load individual LV2 / MOD-GUI effect modules from `/var/modep/lv2` (`429+` installed plugins) directly into 4 series slots per channel.
   - **Zero-Dropout Automatic Thru-Bridging**: Empty or bypassed slots are automatically bridged (`IN ➔ S1 ➔ S2 ➔ S3 ➔ S4 ➔ OUT`) so live audio never cuts out during a gig.
   - **Direct MOD-GUI Parameter Control Modal**: Tap any active slot to view its MOD-GUI faceplate, load LV2 presets, and adjust all LV2 control ports with `TouchGovernor` protection.
3. **Permanent Core Desk DSP Strip**:
   - Adaptive De-Noiser Gate, Cockos 8-Band ReaEQ + 64-Band Live RTA, Switchable Analog Obsession `FETISH 76` / `LALA` Opto Compressor (with true JACK telemetry VU needles), and `LoaDeS` De-Esser remain permanently active in both `RACK SLOTS` and `CANVAS` modes.
"""
with open(readme_path, "w", encoding="utf-8") as f:
    f.write(readme_content)

# Refresh project_files to include updated README.md
project_files = collect_project_files()

# 3. Ensure Private Repository & Push Sources + Release to both 'patchbox-prime' and 'modep-pro-live-mixer-daw'
REPOS_TO_SYNC = [
    ("patchbox-prime", "Patchbox Prime v3.0 — Pro Live Digital Mixing Console, 1176 FET Preamp, Core Desk DSP & 4-Slot Auto-Routed MODEP LV2 Workstation"),
    ("modep-pro-live-mixer-daw", "Patchbox Prime (MODEP Pro Live Digital Mixing Console & DAW v3.0) — Private Backup & Release")
]

def sync_repo_and_release(repo_name, repo_desc):
    print(f"\n=== SYNCING PRIVATE REPO: {GITHUB_USER}/{repo_name} ===")
    # Check if repo exists
    existing = api(f"https://api.github.com/repos/{GITHUB_USER}/{repo_name}")
    if "error" in existing:
        print(f"[*] Creating private repository {repo_name}...")
        create_res = api("https://api.github.com/user/repos", {
            "name": repo_name,
            "description": repo_desc,
            "private": True,
            "has_issues": True,
            "has_projects": True,
            "has_wiki": False
        }, method="POST")
        if "error" in create_res:
            print(f"[!] Could not create {repo_name}: {create_res}")
            return
        print(f"[+] Created private repo: {create_res.get('html_url')} (private={create_res.get('private')})")
    else:
        print(f"[*] Updating {repo_name} visibility to private=True...")
        patch_res = api(f"https://api.github.com/repos/{GITHUB_USER}/{repo_name}", {
            "name": repo_name,
            "description": repo_desc,
            "private": True
        }, method="PATCH")
        print(f"[+] Repo visibility private={patch_res.get('private')}")

    # Ensure main branch exists
    default_branch = "main"
    ref_res = api(f"https://api.github.com/repos/{GITHUB_USER}/{repo_name}/git/refs/heads/main")
    if "error" in ref_res:
        ref_master = api(f"https://api.github.com/repos/{GITHUB_USER}/{repo_name}/git/refs/heads/master")
        if "error" not in ref_master:
            default_branch = "master"
        else:
            init_b64 = base64.b64encode(readme_content.encode("utf-8")).decode("ascii")
            api(f"https://api.github.com/repos/{GITHUB_USER}/{repo_name}/contents/README.md", {
                "message": "Initialize Patchbox Prime private repository",
                "content": init_b64,
                "branch": "main"
            }, method="PUT")
            time.sleep(1.5)
            ref_res = api(f"https://api.github.com/repos/{GITHUB_USER}/{repo_name}/git/refs/heads/main")
    else:
        ref_master = ref_res

    ref_info = api(f"https://api.github.com/repos/{GITHUB_USER}/{repo_name}/git/refs/heads/{default_branch}")
    base_commit_sha = ref_info.get("object", {}).get("sha")
    commit_info = api(f"https://api.github.com/repos/{GITHUB_USER}/{repo_name}/git/commits/{base_commit_sha}")
    base_tree_sha = commit_info.get("tree", {}).get("sha")

    def upload_blob(fp, rel):
        with open(fp, "rb") as f:
            b64 = base64.b64encode(f.read()).decode("ascii")
        res = api(f"https://api.github.com/repos/{GITHUB_USER}/{repo_name}/git/blobs", {
            "content": b64,
            "encoding": "base64"
        }, method="POST")
        sha = res.get("sha")
        if sha:
            return {"path": rel, "mode": "100644", "type": "blob", "sha": sha}
        return None

    tree_items = []
    with ThreadPoolExecutor(max_workers=6) as ex:
        futs = [ex.submit(upload_blob, fp, rel) for fp, rel in project_files]
        for fut in as_completed(futs):
            item = fut.result()
            if item:
                tree_items.append(item)

    print(f"[+] Uploaded {len(tree_items)} blobs to {repo_name}. Creating Git tree...")
    new_tree = api(f"https://api.github.com/repos/{GITHUB_USER}/{repo_name}/git/trees", {
        "base_tree": base_tree_sha,
        "tree": tree_items
    }, method="POST")
    new_tree_sha = new_tree.get("sha")

    commit_msg = "Patchbox Prime v3.0: 1176 Preamp (Stage 1 -> REGAIN), 4 Auto-Routed MODEP LV2 Slots, Telemetry VU & PROJECT_INDEX.md"
    new_commit = api(f"https://api.github.com/repos/{GITHUB_USER}/{repo_name}/git/commits", {
        "message": commit_msg,
        "tree": new_tree_sha,
        "parents": [base_commit_sha]
    }, method="POST")
    new_commit_sha = new_commit.get("sha")

    api(f"https://api.github.com/repos/{GITHUB_USER}/{repo_name}/git/refs/heads/{default_branch}", {
        "sha": new_commit_sha,
        "force": True
    }, method="PATCH")
    print(f"[+] Updated {repo_name}/{default_branch} to commit {new_commit_sha}")

    # Create or update Release v3.0-patchbox-prime
    tag_name = "v3.0-patchbox-prime"
    rel_body = (
        "## Patchbox Prime v3.0 — Private Production Release & System Image Backup\n\n"
        "### Included in this Release\n"
        "- **Stage 1: 1176 FET Class-A Preamp Emulation** (`Drive 0..+36 dB`, `Soft FET Sat / Hard Clip`, `Live Clip Telemetry LED`, `Output Trim -24..+12 dB`) positioned **before** **Stage 2: Digital Gain & Constant-Level REGAIN**.\n"
        "- **4 Auto-Routed Linear MODEP LV2 / MOD-GUI Insert Slots per Channel**: Series auto-thru routing (`IN ➔ S1 ➔ S2 ➔ S3 ➔ S4 ➔ OUT`) that never cuts live audio when empty or bypassed, with direct MOD-GUI faceplate & parameter modal.\n"
        "- **Permanent Core Desk DSP Strip**: Adaptive De-Noiser Gate, 8-Band ReaEQ + 64-Band Live RTA, Switchable FETISH 76 / LALA Opto Compressor (true telemetry VU needles), and LoaDeS De-Esser.\n"
        "- **Strict `AGENT_RULES.md` Compliance**: All 17 JS modules `< 450` lines (`node -c` verified), `TouchGovernor` slew protection, High-DPI canvas scaling, zero simulated meter loops.\n"
        "- **`PROJECT_INDEX.md` (325 lines)**: Complete architectural index and quick reference."
    )
    rel_res = api(f"https://api.github.com/repos/{GITHUB_USER}/{repo_name}/releases", {
        "tag_name": tag_name,
        "name": "Patchbox Prime v3.0 — Complete Console Image & Source Backup",
        "body": rel_body,
        "draft": False,
        "prerelease": False
    }, method="POST")

    if "error" in rel_res:
        # Fetch existing release by tag
        rel_res = api(f"https://api.github.com/repos/{GITHUB_USER}/{repo_name}/releases/tags/{tag_name}")

    upload_url_base = rel_res.get("upload_url", "").split("{")[0]
    print(f"[+] Release ready: {rel_res.get('html_url')}")

    if upload_url_base:
        for asset_path, ctype in [(tar_path, "application/gzip"), (zip_path, "application/zip")]:
            aname = os.path.basename(asset_path)
            with open(asset_path, "rb") as f:
                adata = f.read()
            up_res = api(f"{upload_url_base}?name={aname}", adata, method="POST", content_type=ctype)
            if "browser_download_url" in up_res:
                print(f"  [+] Uploaded release image asset: {up_res['browser_download_url']}")
            else:
                print(f"  [*] Asset upload response for {aname}: {str(up_res)[:120]}")

for rname, rdesc in REPOS_TO_SYNC:
    sync_repo_and_release(rname, rdesc)
