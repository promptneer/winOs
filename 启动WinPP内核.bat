@echo off
chcp 65001 >nul
title WinPP OS v0.1 - S10 图形界面版
echo ==============================================
echo   WinPP OS v0.1 (S10 图形界面版) 一键启动
echo   启动后输入 gui 回车 进入图形界面
echo   GUI 内按键: 1-5 切窗口 / n 新建 / x 关闭 / q 退出
echo ==============================================
echo.
"C:\Bochs-2.8\bochs.exe" -q -f "D:\WinPP\winpp-os\bochsrc.txt"
echo.
echo 已退出。按任意键关闭窗口...
pause >nul
