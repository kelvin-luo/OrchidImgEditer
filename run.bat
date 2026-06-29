@echo off
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0run.ps1" %*
set "RC=%ERRORLEVEL%"
endlocal & exit /b %RC%
