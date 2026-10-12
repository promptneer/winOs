# WinPP OS -- 扁平内核 + 引导扇区 + 软盘镜像构建（VBox/真机软盘实测路径）
# 产物:
#   winpp-os\kernel.bin      扁平内核（链接于 1MB，入口 0x100000）
#   winpp-os\bootsec.bin     512 字节引导扇区（链接于 0x7C00）
#   winpp-os\floppy.img      1.44MB 可引导软盘镜像（VBox 软驱 / 真机 USB 均可）
# 依赖: zig（与 build.ps1 同一查找逻辑）

$ErrorActionPreference = "Stop"
$root = "D:\WinPP\winpp-os"
Set-Location $root

# 定位 zig
$zig = $null
foreach ($p in @("C:\Program Files\zig\zig.exe", "C:\Program Files (x86)\zig\zig.exe")) {
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

# ---------- 0. 用户程序 user1..10.bin（第三~六阶段：用户态进程） ----------
$users = @(
  @{ name="user1"; ld="user1.ld" },
  @{ name="user2"; ld="user2.ld" },
  @{ name="user3"; ld="user3.ld" },
  @{ name="user4"; ld="user4.ld" },
  @{ name="user5"; ld="user5.ld" },
  @{ name="user6"; ld="user6.ld" },
  @{ name="user7"; ld="user7.ld" },
  @{ name="user8"; ld="user8.ld" },
  @{ name="user9"; ld="user9.ld" },
  @{ name="user10"; ld="user10.ld" },
  @{ name="user11"; ld="user11.ld" }
)
foreach ($u in $users) {
  $uargs = @(
    "cc",
    "-target", "x86-freestanding",
    "-ffreestanding", "-fno-builtin", "-fno-stack-protector",
    "-fno-pic", "-fno-pie", "-g0",
    "-O2", "-Wall",
    "-mno-sse", "-mno-80387",
    "-nostdlib",
    "-Wl,-T,$root\src\$($u.ld)",
    "-Wl,--build-id=none",
    "-o", "$root\src\$($u.name).elf",
    "$root\src\$($u.name).S"
  )
  & $zig @uargs
  if ($LASTEXITCODE -ne 0) { throw "$($u.name).elf build failed: $LASTEXITCODE" }
  & $zig objcopy -O binary "$root\src\$($u.name).elf" "$root\src\$($u.name).bin"
  if ($LASTEXITCODE -ne 0) { throw "$($u.name).bin objcopy failed: $LASTEXITCODE" }
  $ub = Get-Item "$root\src\$($u.name).bin"
  Write-Host "$($u.name).bin: $($ub.Length) bytes"
  if ($ub.Length -gt 4096) { throw "$($u.name).bin too big for one page (4096)" }
}

# 生成 users_inc.c/.h（C 数组内嵌用户程序；zig cc 的 lld 不支持 .incbin，改用数组最稳）
python -c "
names=['user1','user2','user3','user4','user5','user6','user7','user8','user9','user10','user11']
lines_c=[]; lines_h=['#pragma once']
for n in names:
    d=open(r'$root\src\%s.bin' % n,'rb').read()
    lines_c.append('const unsigned char _%s_code[%d] = { %s };' % (n, len(d), ', '.join('0x%02x'%b for b in d)))
    lines_h.append('#define %s_LEN %d' % (n.upper(), len(d)))
    lines_h.append('extern const unsigned char _%s_code[%s_LEN];' % (n, n.upper()))
open(r'$root\src\users_inc.c','w',encoding='utf-8').write('\n'.join(lines_c)+'\n')
open(r'$root\src\users_inc.h','w',encoding='utf-8').write('\n'.join(lines_h)+'\n')
print('users_inc.c/h generated')
"
if ($LASTEXITCODE -ne 0) { throw "users_inc generation failed" }

# ---------- 1. 扁平内核 kernel.bin ----------
$args = @(
  "cc",
  "-target", "x86-freestanding",
  "-ffreestanding", "-fno-builtin", "-fno-stack-protector",
  "-fno-pic", "-fno-pie", "-g0",
  "-O2", "-Wall", "-Wextra",
  "-mno-sse", "-mno-80387",
  "-nostdlib",
  "-DNO_MULTIBOOT=1",
  "-Wl,-T,$root\src\linker_flat.ld",
  "-Wl,--build-id=none",
  "-o", "$root\kernel-flat.elf"
) + $srcAS + $srcC
& $zig @args
if ($LASTEXITCODE -ne 0) { throw "kernel-flat.elf build failed: $LASTEXITCODE" }
& $zig objcopy -O binary "$root\kernel-flat.elf" "$root\kernel.bin"
if ($LASTEXITCODE -ne 0) { throw "kernel.bin objcopy failed: $LASTEXITCODE" }

$kb = Get-Item "$root\kernel.bin"
$sectors = [int][Math]::Ceiling($kb.Length / 512.0)
Write-Host "kernel.bin: $($kb.Length) bytes -> $sectors sectors"
if ($sectors -gt 2880 - 1) { throw "kernel too big for floppy" }

# ---------- 2. 引导扇区 bootsec.bin ----------
$args = @(
  "cc",
  "-target", "x86-freestanding",
  "-ffreestanding", "-fno-builtin", "-fno-stack-protector",
  "-fno-pic", "-fno-pie", "-g0",
  "-nostdlib",
  "-DKERNEL_SECTORS=$sectors",
  "-Wl,-T,$root\src\bootsec.ld",
  "-Wl,--build-id=none",
  "-o", "$root\bootsec.elf",
  "$root\src\bootsec.S"
)
& $zig @args
if ($LASTEXITCODE -ne 0) { throw "bootsec.elf build failed: $LASTEXITCODE" }
& $zig objcopy -O binary "$root\bootsec.elf" "$root\bootsec.bin"
if ($LASTEXITCODE -ne 0) { throw "bootsec.bin objcopy failed: $LASTEXITCODE" }

$bs = Get-Item "$root\bootsec.bin"
Write-Host "bootsec.bin: $($bs.Length) bytes (expect exactly 512)"
if ($bs.Length -ne 512) { throw "boot sector must be 512 bytes, got $($bs.Length)" }
# 校验引导标记
$bytes = [IO.File]::ReadAllBytes($bs.FullName)
if ($bytes[510] -ne 0x55 -or $bytes[511] -ne 0xAA) { throw "boot magic 0xAA55 missing" }
Write-Host "boot magic 0xAA55 OK"

# ---------- 3. 软盘镜像 floppy.img ----------
$img = "$root\floppy.img"
$fs = [IO.File]::Open($img, [IO.FileMode]::Create)
try {
    $fs.SetLength(1474560)          # 1.44MB 全零
    $boot = [IO.File]::ReadAllBytes($bs.FullName)
    $fs.Position = 0
    $fs.Write($boot, 0, $boot.Length)
    $ker = [IO.File]::ReadAllBytes($kb.FullName)
    $fs.Position = 512
    $fs.Write($ker, 0, $ker.Length)
} finally {
    $fs.Close()
}
Write-Host "OK -> $img  ($((Get-Item $img).Length) bytes, kernel at sector 1, $sectors sectors)"
