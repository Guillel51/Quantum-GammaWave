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
echo   STEP 0 - PLAN BEFORE YOU WRITE (first card on the page):
echo     Load your M-Flasher dump (the 1 MiB .bin) under
echo     "Before you write anything". The page then runs M-Flasher's own
echo     checks on the release package and shows the exact sectors it will
echo     erase: 0x04000-0x92FFF, and only the ones that differ from your unit.
echo     Nothing is sent to the FM-1 while you read that plan.
echo.
echo   STEP 1 - the first install on a stock unit is M-Flasher's
echo     "Install firmware" with the same .fwsc: it keeps the boot area and the
echo     factory data, and verifies every sector it writes.
echo   STEP 2 - after that the unit runs QG, and this page is its updater.
echo.
echo   Allow Web MIDI when the browser asks: that is the page talking to the FM-1.
echo   Use a data USB cable, plugged straight into the computer, no hub.
echo.

set "SITE=https://guillel51.github.io/Quantum-GammaWave/webapp/installer/"
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

echo   Opening the public installer over HTTPS (nothing to install on this PC):
echo   %SITE%
echo.
start "" "%SITE%"
echo   The browser is opening. Keep this window: the steps above are the check
echo   list, and the page's first card is the plan for your own dump.
echo.
pause
