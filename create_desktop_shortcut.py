import os
import subprocess

ps_script = """
$WshShell = New-Object -comObject WScript.Shell
$DesktopPath = [System.Environment]::GetFolderPath('Desktop')
$Shortcut = $WshShell.CreateShortcut("$DesktopPath\\MOD Desktop.lnk")
$Shortcut.TargetPath = "C:\\Program Files\\MOD Desktop\\mod-desktop.exe"
$Shortcut.WorkingDirectory = "C:\\Program Files\\MOD Desktop"
$Shortcut.IconLocation = "C:\\Program Files\\MOD Desktop\\mod-logo.ico, 0"
$Shortcut.Save()
Write-Host "Created shortcut successfully!"
"""

with open("create_shortcut.ps1", "w", encoding="utf-8") as f:
    f.write(ps_script)

res = subprocess.run(["powershell", "-ExecutionPolicy", "Bypass", "-File", "create_shortcut.ps1"], capture_output=True, text=True)
print(res.stdout)
if os.path.exists("create_shortcut.ps1"):
    os.remove("create_shortcut.ps1")
