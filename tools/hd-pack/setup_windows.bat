@echo off
rem Installs what make_hd_pack.bat needs into a "venv" folder next to this file.
rem Needs Python 3.10-3.13 from python.org (tick "Add python.exe to PATH").
rem Optional argument: cpu, cu126 (default, GTX 900 to RTX 40) or cu128 (RTX 50).
cd /d "%~dp0"
set CUDA=%1
if "%CUDA%"=="" set CUDA=cu126
python -m venv venv || goto :fail
venv\Scripts\python -m pip install --upgrade pip || goto :fail
venv\Scripts\python -m pip install torch torchvision --index-url https://download.pytorch.org/whl/%CUDA% || goto :fail
venv\Scripts\python -m pip install -r requirements.txt || goto :fail
echo.
echo Done. Now drag your "Digimon World (USA).cue" onto make_hd_pack.bat
pause
exit /b 0
:fail
echo.
echo Setup failed, see the messages above.
pause
exit /b 1
