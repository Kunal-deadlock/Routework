@echo off
setlocal
cd /d "%~dp0"
if exist "..\w64devkit\bin\node.exe" set "PATH=%~dp0..\w64devkit\bin;%PATH%"
if exist "..\w64devkit\bin\make.exe" set "PATH=%~dp0..\w64devkit\bin;%PATH%"
where node >nul 2>nul
if errorlevel 1 (
  echo Install Node.js 22 or newer, then run this file again.
  pause
  exit /b 1
)
where make >nul 2>nul
if not errorlevel 1 (
  make
  if errorlevel 1 (
    pause
    exit /b 1
  )
)
if not exist vrp.exe (
  echo Build the C++ planner first: make
  pause
  exit /b 1
)
node -e "fetch('http://127.0.0.1:3000/api/health',{signal:AbortSignal.timeout(800)}).then(r=>r.json()).then(d=>process.exit(d.engine==='C++17'?0:1)).catch(()=>process.exit(1))"
if not errorlevel 1 (
  start "" "http://127.0.0.1:3000"
  exit /b 0
)
start "" "http://127.0.0.1:3000"
node web/server.mjs
pause
