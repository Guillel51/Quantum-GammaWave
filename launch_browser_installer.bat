@echo off
title GAMMAWAVE // WRAITH — M-VAVE FM-1 Browser Uploader & Web Studio
cd /d "%~dp0"
echo ============================================================================
echo   GAMMAWAVE // WRAITH — M-VAVE FM-1 Firmware Uploader ^& Web Studio (v1.0-GW)
echo ============================================================================
echo.
echo   Firmware binary : build\site\firmware\felucca-v1.0-GW.fwsc (610,113 B)
echo   WebMIDI URL     : http://localhost:8765/webapp/installer/
echo   Web Studio URL  : http://localhost:8765/webapp/editor/
echo   Simulator URL   : http://localhost:8765/webapp/editor/?mock=1
echo.
echo   Opening browser (use Chrome, Edge, or Brave for WebMIDI SysEx)...
start "" "http://localhost:8765/webapp/installer/"
python -m http.server 8765 --directory build/site
