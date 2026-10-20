ini_path = "/home/patch/.config/REAPER/reaper.ini"
with open(ini_path, "r") as f:
    lines = f.readlines()

new_lines = []
for line in lines:
    if line.startswith("csurf_"):
        continue
    new_lines.append(line)

new_lines.append("csurf_cnt=1\n")
new_lines.append('csurf_0=HTTP 8080 "" ""\n')

with open(ini_path, "w") as f:
    f.writelines(new_lines)
print("Updated reaper.ini")
