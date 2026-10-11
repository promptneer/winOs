# WinPP OS -- 在 QEMU 窗口中交互式启动
# 用法: powershell -ExecutionPolicy Bypass -File run_qemu_gui.ps1
$root = "D:\WinPP\winpp-os"
$kernel = "$root\winpp-kernel.elf"
if (-not (Test-Path $kernel)) { throw "kernel not built: $kernel" }

$qemu = $null
foreach ($p in @("C:\Program Files\qemu\qemu-system-i386.exe",
                 "C:\msys64\mingw64\bin\qemu-system-i386.exe",
                 "C:\msys64\ucrt64\bin\qemu-system-i386.exe")) {
    if (Test-Path $p) { $qemu = $p; break }
}
if (-not $qemu) { throw "qemu-system-i386.exe not found" }

Write-Host "Booting WinPP OS in QEMU window..."
& $qemu -m 64 -vga std -kernel $kernel
