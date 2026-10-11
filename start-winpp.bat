@echo off
rem ============================================
rem  WinPP OS one-click launcher (Bochs 2.8)
rem  Double-click to boot the kernel in Bochs.
rem  Type in the window: help / time / demo ...
rem ============================================
set "BOCHS=C:\Bochs-2.8\bochs.exe"
set "CFG=D:\WinPP\winpp-os\bochsrc.txt"
if not exist "%BOCHS%" (echo [ERROR] Bochs not found: %BOCHS% & pause & exit /b 1)
if not exist "%CFG%" (echo [ERROR] config not found: %CFG% & pause & exit /b 1)
taskkill /IM bochs.exe /F >nul 2>&1
ping -n 2 127.0.0.1 >nul
start "" "%BOCHS%" -q -f "%CFG%"
exit /b 0