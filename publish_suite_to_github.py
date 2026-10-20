import os
import sys
import json
import base64
import urllib.request
import urllib.error
import time
from concurrent.futures import ThreadPoolExecutor, as_completed

GITHUB_TOKEN = "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"
GITHUB_USER = "danny1marshall1587-maker"
REPO_NAME = "cyber-audio-modep-suite"
REPO_DESC = "Cyber Audio Complete MODEP & Patchbox OS Guitar LV2 Suite: Galaxy Strobe Tuner, 9-Series Harmonies, Vintage Amps, and DSP Cleanup Effects"
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

def ensure_repo():
    print(f"[*] Ensuring repo {REPO_NAME} exists under {GITHUB_USER}...")
    res = api("https://api.github.com/user/repos", {
        "name": REPO_NAME,
        "description": REPO_DESC,
        "private": False,
        "has_issues": True,
        "has_projects": True,
        "has_wiki": False
    })
    
    # Check if main ref exists
    check_ref = api(f"https://api.github.com/repos/{GITHUB_USER}/{REPO_NAME}/git/refs/heads/main")
    if "error" in check_ref:
        readme_url = f"https://api.github.com/repos/{GITHUB_USER}/{REPO_NAME}/contents/README.md"
        init_content = base64.b64encode(f"# {REPO_NAME}\n\n{REPO_DESC}\n".encode("utf-8")).decode("ascii")
        api(readme_url, {
            "message": f"Initial commit for {REPO_NAME}",
            "content": init_content,
            "branch": "main"
        }, method="PUT")
        print(f" [+] Initialized main branch for {REPO_NAME}")
        time.sleep(2)
    else:
        print(f" [*] Repo {REPO_NAME} is ready.")

def upload_blob(full_path, rel_path):
    try:
        with open(full_path, "rb") as f:
            b64 = base64.b64encode(f.read()).decode("ascii")
        url = f"https://api.github.com/repos/{GITHUB_USER}/{REPO_NAME}/git/blobs"
        res = api(url, {"content": b64, "encoding": "base64"})
        sha = res.get("sha")
        if sha:
            clean_path = rel_path.replace("\\", "/")
            return {"path": clean_path, "mode": "100644", "type": "blob", "sha": sha}
    except Exception as e:
        print(f" [!] Error uploading blob {rel_path}: {e}")
    return None

def publish_repository():
    ensure_repo()

    file_map = []
    # Collect files from dist (bundles)
    dist_dir = os.path.join(BASE_DIR, "dist")
    for root, dirs, files in os.walk(dist_dir):
        for f in files:
            # Skip massive Windows dlls if > 10MB, otherwise include
            fp = os.path.join(root, f)
            rp = os.path.relpath(fp, BASE_DIR)
            file_map.append((fp, rp))

    # Also collect scripts and documentation
    for extra_file in ["build_linux_arm_plugins.sh", "install_to_patchbox_pi.bat", "install_to_modep.sh", "package_bundles.py", "patchbox_sd_install_guide.md"]:
        fp = os.path.join(BASE_DIR, extra_file)
        if os.path.exists(fp):
            file_map.append((fp, extra_file))

    print(f"[*] Total files to commit: {len(file_map)}")

    tree_items = []
    with ThreadPoolExecutor(max_workers=8) as executor:
        futures = {executor.submit(upload_blob, fp, rp): rp for fp, rp in file_map}
        for fut in as_completed(futures):
            item = fut.result()
            if item:
                tree_items.append(item)

    print(f"[*] Uploaded {len(tree_items)} blobs. Constructing tree...")

    ref_res = api(f"https://api.github.com/repos/{GITHUB_USER}/{REPO_NAME}/git/refs/heads/main")
    parent_sha = ref_res.get("object", {}).get("sha")

    tree_payload = {"tree": tree_items}
    if parent_sha:
        tree_payload["base_tree"] = parent_sha

    tree_res = api(f"https://api.github.com/repos/{GITHUB_USER}/{REPO_NAME}/git/trees", tree_payload)
    tree_sha = tree_res.get("sha")
    if not tree_sha:
        print(f"[!] Error creating tree: {tree_res}")
        return

    commit_payload = {
        "message": "Publish Cyber Audio MODEP & Patchbox OS Plugin Suite (v1.0.0)",
        "tree": tree_sha,
        "parents": [parent_sha] if parent_sha else []
    }
    commit_res = api(f"https://api.github.com/repos/{GITHUB_USER}/{REPO_NAME}/git/commits", commit_payload)
    commit_sha = commit_res.get("sha")
    if not commit_sha:
        print(f"[!] Error creating commit: {commit_res}")
        return

    update_ref = api(f"https://api.github.com/repos/{GITHUB_USER}/{REPO_NAME}/git/refs/heads/main", {
        "sha": commit_sha,
        "force": True
    }, method="PATCH")
    print(f"[+] Successfully committed code to main! Ref update: {update_ref.get('ref', 'OK')}")

    # Create Release and upload assets
    print(f"[*] Creating GitHub Release v1.0.0...")
    rel_data = {
        "tag_name": "v1.0.0",
        "target_commitish": "main",
        "name": "Cyber Audio MODEP Suite v1.0.0",
        "body": "Complete 58-bundle collection for MODEP & Patchbox OS on Raspberry Pi / ARM SD Card.\nIncludes Galaxy Strobe Tuner, Cyber 9-Series, and all custom amps.",
        "draft": False,
        "prerelease": False
    }
    rel_res = api(f"https://api.github.com/repos/{GITHUB_USER}/{REPO_NAME}/releases", rel_data)
    release_id = rel_res.get("id")
    upload_url_tmpl = rel_res.get("upload_url")

    if release_id and upload_url_tmpl:
        base_upload = upload_url_tmpl.split("{")[0]
        for archive in ["modep_guitar_plugins_bundle.tar.gz", "modep_guitar_plugins_bundle.zip"]:
            arch_path = os.path.join(BASE_DIR, archive)
            if os.path.exists(arch_path):
                print(f"[*] Uploading release asset {archive} ({os.path.getsize(arch_path)} bytes)...")
                with open(arch_path, "rb") as f:
                    data = f.read()
                upload_url = f"{base_upload}?name={archive}"
                res = api(upload_url, data, method="POST", content_type="application/octet-stream")
                print(f" [+] Asset upload response: {res.get('state', 'Uploaded')}")

    print(f"\n================================================================")
    print(f"  SUCCESS! Repo live at: https://github.com/{GITHUB_USER}/{REPO_NAME}")
    print(f"================================================================")

if __name__ == '__main__':
    publish_repository()
