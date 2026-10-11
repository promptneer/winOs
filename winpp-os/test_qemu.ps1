# WinPP OS -- 自动化机器实测脚本
# 用 QEMU 开机，经监视器自动按键交互，逐阶段截图作为证据
# 用法: powershell -ExecutionPolicy Bypass -File test_qemu.ps1

$ErrorActionPreference = "Stop"
$root   = "D:\WinPP\winpp-os"
$shots  = "$root\shots"
$kernel = "$root\winpp-kernel.elf"

if (-not (Test-Path $kernel)) { throw "kernel not built: $kernel" }
New-Item -ItemType Directory -Force $shots | Out-Null
Get-ChildItem $shots -Filter *.png -ErrorAction SilentlyContinue | Remove-Item -Force

# 定位 QEMU
$qemu = $null
foreach ($p in @("C:\Program Files\qemu\qemu-system-i386.exe",
                 "C:\Program Files (x86)\qemu\qemu-system-i386.exe",
                 "C:\msys64\mingw64\bin\qemu-system-i386.exe",
                 "C:\msys64\ucrt64\bin\qemu-system-i386.exe")) {
    if (Test-Path $p) { $qemu = $p; break }
}
if (-not $qemu) { $qemu = (Get-Command qemu-system-i386.exe -ErrorAction SilentlyContinue).Source }
if (-not $qemu) { throw "qemu-system-i386.exe not found" }
Write-Host "qemu: $qemu"

# 启动 QEMU（无窗口、监视器走 TCP）
$monPort = 4444
$qemuArgs = @("-m","64","-vga","std","-display","none",
              "-monitor","tcp:127.0.0.1:$monPort,server,nowait",
              "-no-reboot",
              "-kernel",$kernel)
$proc = Start-Process -FilePath $qemu -ArgumentList $qemuArgs -PassThru -WindowStyle Hidden

try {
    $client = New-Object System.Net.Sockets.TcpClient
    $ok = $false
    for ($i = 0; $i -lt 150; $i++) {
        try { $client.Connect("127.0.0.1", $monPort); $ok = $true; break } catch { }
        Start-Sleep -Milliseconds 200
    }
    if (-not $ok) { throw "cannot connect to QEMU monitor" }

    $stream = $client.GetStream()
    $writer = New-Object System.IO.StreamWriter($stream)
    $writer.AutoFlush = $true
    $reader = New-Object System.IO.StreamReader($stream)

    function M([string]$cmd) { $writer.WriteLine($cmd) | Out-Null }
    function Shot([string]$name) { M("screendump $($shots.Replace('\','/'))/$name") }

    Start-Sleep -Seconds 3
    Shot "01-boot.png";                Start-Sleep -Milliseconds 800

    # 1) 输入 help 回车 -> 展示命令表
    M "sendkey h e l p ret";           Start-Sleep -Milliseconds 1200
    Shot "02-help.png";                Start-Sleep -Milliseconds 600

    # 2) echo hello winpp
    M "sendkey e c h o sp h e l l o sp w i n p p ret"; Start-Sleep -Milliseconds 1000
    Shot "03-echo.png";                Start-Sleep -Milliseconds 600

    # 3) calc 81 / 9 -> 内核里算出来 = 9
    M "sendkey c a l c sp 8 1 sp slash sp 9 ret"; Start-Sleep -Milliseconds 1000
    Shot "04-calc.png";                Start-Sleep -Milliseconds 600

    # 4) time（读 CMOS 时钟）
    M "sendkey t i m e ret";           Start-Sleep -Milliseconds 1000
    Shot "05-time.png";                Start-Sleep -Milliseconds 600

    # 5) uptime
    M "sendkey u p t i m e ret";       Start-Sleep -Milliseconds 1000
    Shot "06-uptime.png";              Start-Sleep -Milliseconds 600

    # 6) sysinfo（CPU/RAM）
    M "sendkey s y s i n f o ret";     Start-Sleep -Milliseconds 1000
    Shot "07-sysinfo.png";             Start-Sleep -Milliseconds 600

    # 7) demo 窗口 UI
    M "sendkey d e m o ret";           Start-Sleep -Milliseconds 1500
    Shot "08-demo.png";                Start-Sleep -Milliseconds 600
    M "sendkey tab";                   Start-Sleep -Milliseconds 900
    Shot "09-demo-tab.png";            Start-Sleep -Milliseconds 600
    M "sendkey left";                  Start-Sleep -Milliseconds 900
    Shot "10-demo-move.png";           Start-Sleep -Milliseconds 600
    M "sendkey esc";                   Start-Sleep -Milliseconds 1200
    Shot "11-back-to-shell.png"

    # 8) 关机（ACPI）
    M "sendkey p o w e r o f f ret"
    Start-Sleep -Seconds 3

    M "quit"
    $proc.WaitForExit(10000) | Out-Null

    $list = Get-ChildItem $shots -Filter *.png | Sort-Object Name
    Write-Host "OK: $($list.Count) screenshots captured:"
    $list | ForEach-Object { Write-Host "  $($_.Name)  $($_.Length) bytes" }
}
finally {
    if (-not $proc.HasExited) { $proc.Kill() | Out-Null }
}
