@echo off
chcp 65001 >nul
title WinPP OS - Host <-> Kernel console (UDP over Bochs slirp)
echo ============================================================
echo   WinPP 主机通讯台（Windows ^<-^> 内核）
echo.
echo   先在内核里执行：
echo       chatd 7777 echo      （消息口，收到自动回 ACK）
echo       chatd 7778 cmd       （命令口，远程执行命令 + 回传屏幕）
echo.
echo   本窗口：直接敲一行 = 发给内核；/cmd ^<命令^> = 远程执行内核命令；/quit 退出
echo ============================================================
echo.
python "D:\WinPP\winpp-os\winpp-host.py" %*
echo.
echo 通讯台已退出。按任意键关闭窗口。
pause >nul
