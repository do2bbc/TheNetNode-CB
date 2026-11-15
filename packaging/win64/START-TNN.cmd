@echo off
cd /d "%~dp0"
tnn64.exe -v
if errorlevel 1 (
  echo TNN exited with an error. Read the output above.
  pause
)
