# WinPP OS 构建脚本（Windows / PowerShell）
# 依赖：zig（D:\WinPP\tools\zig\zig.exe）+ NASM（可选）

$ErrorActionPreference = "Stop"
$root = "D:\WinPP\winpp-os"
Set-Location $root

# 定位 zig
$zig = $null
foreach ($p in @("C:\Program Files\zig\zig.exe",
                 "C:\Program Files (x86)\zig\zig.exe")) {
    if (Test-Path $p) { $zig = $p; break }
}
if (-not $zig) {
    $zig = Get-ChildItem "D:\WinPP\tools\zig" -Recurse -Filter "zig.exe" -ErrorAction SilentlyContinue |
           Select-Object -First 1 -ExpandProperty FullName
}
if (-not $zig) {
    $link = "$env:LOCALAPPDATA\Microsoft\WinGet\Links\zig.exe"
    if (Test-Path $link) {
        $t = (Get-Item $link).Target
        if ($t) { $zig = $t }
    }
}
if (-not $zig) { $zig = (Get-Command zig.exe -ErrorAction SilentlyContinue).Source }
if (-not $zig) { throw "zig.exe not found" }
Write-Host "zig: $zig"

$srcC  = (Get-ChildItem "$root\src" -Filter *.c | Select-Object -ExpandProperty FullName)
$srcAS = @("$root\src\boot.S", "$root\src\isr.S")

$args = @(
  "cc",
  "-target", "x86-freestanding",   # Zig 0.17：32 位 x86 = "x86"
  "-ffreestanding", "-fno-builtin", "-fno-stack-protector",
  "-fno-pic", "-fno-pie", "-g0",
  "-O2", "-Wall", "-Wextra",
  "-mno-sse", "-mno-80387",
  "-nostdlib",
  "-Wl,-T,$root\src\linker.ld",
  "-Wl,--build-id=none",
  "-o", "$root\winpp-kernel.elf"
) + $srcAS + $srcC   # 汇编在前，保证 .multiboot 排在最前

& $zig @args
if ($LASTEXITCODE -ne 0) { throw "zig cc failed: $LASTEXITCODE" }

$elf = Get-Item "$root\winpp-kernel.elf"
Write-Host "OK -> $($elf.FullName)  ($($elf.Length) bytes)"
