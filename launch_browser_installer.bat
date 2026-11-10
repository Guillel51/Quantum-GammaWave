@echo off
setlocal enabledelayedexpansion
title QUANTUM GAMMAWAVE - M-VAVE FM-1 installer
cd /d "%~dp0"

echo ============================================================================
echo   QUANTUM GAMMAWAVE (QG-1) - M-VAVE FM-1 installer
echo ============================================================================
echo.
echo   This window opens the installer. No Python, Node, Rust or compiler is
echo   needed to install the firmware: the page does the checks itself.
echo.
echo   STEP 0 - PLAN BEFORE YOU WRITE (the page this window opens):
echo     Load your M-Flasher dump (the 1 MiB .bin) under
echo     "Before you write anything". The page then runs M-Flasher's own
echo     checks on the release package and shows the exact sectors it will
echo     erase: 0x04000-0x92FFF, and only the ones that differ from your unit.
echo     Nothing is sent to the FM-1 while you read that plan.
echo.
echo   STEP 1 - open INSTALL (the button at the top of that page) and press
echo     "INSTALL FIRMWARE". On a stock unit the page stages this package's
echo     loader, which checks the flash chip before erasing anything; it writes
echo     only what differs inside 0x04000-0x92FFF and keeps the boot area and
echo     the factory data.
echo   STEP 2 - after that the unit runs QG, and this same page is its updater.
echo.
echo   Allow Web MIDI when the browser asks: that is the page talking to the FM-1.
echo   Use a data USB cable, plugged straight into the computer, no hub.
echo.

rem STEP 0 opens the plan page: it reads the site's package, runs M-Flasher's own
rem checks on it, and prints the exact sectors M-Flasher will erase, using this PC's dump.
rem STEP 1 is the site's installer.
set "SITE=https://guillel51.github.io/Quantum-GammaWave/plan/"
set "INSTALL=https://guillel51.github.io/Quantum-GammaWave/webapp/installer/"
set "LOCAL_PAGE="
if exist "%~dp0webapp\installer\index.html" set "LOCAL_PAGE=%~dp0"
if not defined LOCAL_PAGE if exist "%~dp0build\site\webapp\installer\index.html" set "LOCAL_PAGE=%~dp0build\site"

set "PY_CMD="
for %%C in (python py python3) do (
    if not defined PY_CMD (
        %%C --version >nul 2>&1
        if !errorlevel! equ 0 set "PY_CMD=%%C"
    )
)

if defined LOCAL_PAGE if defined PY_CMD (
    echo   Offline copy found in this folder: http://localhost:8765/webapp/installer/
    echo   (Web MIDI needs HTTPS or localhost, so a local server is used.)
    echo.
    start "" "http://localhost:8765/webapp/installer/"
    pushd "!LOCAL_PAGE!"
    %PY_CMD% -m http.server 8765
    popd
    goto :eof
)

echo   Opening the plan page over HTTPS. Nothing has to be installed on this PC.
echo   Plan:    %SITE%
echo   Install: %INSTALL%
echo.
start "" "%SITE%"
echo   The browser is opening the plan page. Load your dump there, read the sectors,
echo   then press "Continue to the installer".
echo.
pause
