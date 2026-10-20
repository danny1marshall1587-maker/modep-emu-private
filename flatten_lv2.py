import os
import shutil
import glob

USER_LV2_DIR = os.path.expanduser(r"~\Documents\MOD Desktop\lv2")

print("=== Flattening subdirectories in User LV2 ===")
for root, dirs, files in os.walk(USER_LV2_DIR):
    if root == USER_LV2_DIR:
        continue
    for d in dirs:
        if d.endswith('.lv2'):
            src_dir = os.path.join(root, d)
            dst_dir = os.path.join(USER_LV2_DIR, d)
            if src_dir != dst_dir:
                print(f"Moving {src_dir} -> {dst_dir}")
                if os.path.exists(dst_dir):
                    shutil.rmtree(dst_dir, ignore_errors=True)
                shutil.move(src_dir, dst_dir)

# Clean up empty parent directories
for item in os.listdir(USER_LV2_DIR):
    p = os.path.join(USER_LV2_DIR, item)
    if os.path.isdir(p) and not item.endswith('.lv2'):
        shutil.rmtree(p, ignore_errors=True)

print("Flattening complete! Direct LV2 bundles in User LV2:")
bundles = [d for d in os.listdir(USER_LV2_DIR) if os.path.isdir(os.path.join(USER_LV2_DIR, d))]
print(f"Total: {len(bundles)} bundles")
for b in sorted(bundles)[:20]:
    print("  *", b)
