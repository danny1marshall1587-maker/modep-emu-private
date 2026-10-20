@echo off
title Patchbox OS & MODEP (Emulated Raspberry Pi 3B with USB Audio)
cd /d "%~dp0"

echo ================================================================
echo       Starting Patchbox OS & MODEP on Emulated Raspberry Pi 3B
echo ================================================================
echo.
echo  Audio Configuration:
echo   - Host Audio Backend: Windows DirectSound (routed to default sound card/EVO 4)
echo   - Guest Sound Device: USB-Audio (select "USB" or "hw:1" in Patchbox)
echo.
echo  Port Mappings:
echo   - MODEP Web UI:       http://localhost:8080
echo   - SSH / SFTP Access:  localhost:2222  (User: patch / Pass: blokaslabs)
echo   - Blokas Web:         http://localhost:8889
echo.
echo  Note: Make sure your Audient EVO 4 is set as the Default Playback
echo        and Recording device in Windows Sound Settings!
echo ================================================================
echo.

"%~dp0qemu\qemu-system-aarch64.exe" ^
  -M raspi3b ^
  -cpu cortex-a53 ^
  -m 1G ^
  -smp 4 ^
  -kernel "%~dp0kernel8.img" ^
  -initrd "%~dp0initramfs8" ^
  -dtb "%~dp0bcm2710-rpi-3-b.dtb" ^
  -drive "file=%~dp02024-04-04-Patchbox.img,format=raw,if=sd" ^
  -audiodev "dsound,id=snd0,in.channels=2,in.frequency=48000,out.channels=2,out.frequency=48000" ^
  -device "usb-audio,audiodev=snd0" ^
  -netdev "user,id=net0,hostfwd=tcp::8080-:80,hostfwd=tcp::2222-:22,hostfwd=tcp::8889-:8888" ^
  -device "usb-net,netdev=net0" ^
  -device "usb-kbd" ^
  -device "usb-mouse" ^
  -append "console=tty1 console=ttyAMA0,115200 root=PARTUUID=bd7b9517-02 rw rootwait"

pause
