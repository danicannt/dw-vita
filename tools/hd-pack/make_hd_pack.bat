@echo off
rem Drag "Digimon World (USA).cue" onto this file, or run:
rem   make_hd_pack.bat "path\to\Digimon World (USA).cue" [options]
cd /d "%~dp0"
if "%~1"=="" (
  echo Drag your "Digimon World (USA).cue" onto this file.
  pause
  exit /b 1
)
if not exist venv\Scripts\python.exe (
  echo Run setup_windows.bat first.
  pause
  exit /b 1
)
venv\Scripts\python make_hd_pack.py %*
pause
