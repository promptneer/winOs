@echo off
title WinPP OS v0.1 - S10 GUI Launcher
echo ==============================================
echo   WinPP OS v0.1  (Stage 10 GUI edition)
echo   After boot, type:  gui  then press Enter
echo   In GUI: 1-5 switch win / n new / x close / q quit
echo ==============================================
echo.
"C:\Bochs-2.8\bochs.exe" -q -f "D:\WinPP\winpp-os\bochsrc.txt"
echo.
echo Bochs exited. Press any key to close this window.
pause >nul
