@echo off
rem Drag "Digimon World (USA).cue" onto this file (the .dw1hd packs go next to it).
cd /d "%~dp0"
if "%~1"=="" (
  echo Drag your "Digimon World (USA).cue" onto this file.
  pause
  exit /b 1
)
set PY=python
where py >nul 2>nul && set PY=py -3
%PY% --version >nul 2>nul || (
  echo Python not found: install it from python.org and tick "Add python.exe to PATH".
  pause
  exit /b 1
)
%PY% unlock_hd_pack.py %*
pause
