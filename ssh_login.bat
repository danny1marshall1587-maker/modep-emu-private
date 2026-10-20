@echo off
echo Connecting to Patchbox OS via SSH on port 2222...
echo Default Password: blokaslabs
echo.
ssh -o StrictHostKeyChecking=no -p 2222 patch@localhost
pause
