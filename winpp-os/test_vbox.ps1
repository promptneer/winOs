# WinPP OS -- VirtualBox 机器实测（软盘引导 + 自动按键 + PNG 截图取证）
# 用法: powershell -ExecutionPolicy Bypass -File test_vbox.ps1
# 前提: VirtualBox 已安装; 已运行 build_flat.ps1 生成 floppy.img

$ErrorActionPreference = "Stop"
$root  = "D:\WinPP\winpp-os"
$shots = "$root\shots"
$img   = "$root\floppy.img"
$vm    = "WinPP"

if (-not (Test-Path $img)) { throw "floppy.img missing -- run build_flat.ps1 first" }
New-Item -ItemType Directory -Force -Path $shots | Out-Null

$vbm = "C:\Program Files\Oracle\VirtualBox\VBoxManage.exe"
if (-not (Test-Path $vbm)) { throw "VBoxManage.exe not found -- install VirtualBox" }

function VBox { & $vbm @args; if ($LASTEXITCODE -ne 0) { throw "VBoxManage $($args[0]) failed: $LASTEXITCODE" } }

# ---- 清理旧 VM ----
& $vbm unregistervm $vm --delete 2>$null | Out-Null

# ---- 建 VM（软驱引导，无网卡，64MB 内存） ----
VBox createvm --name $vm --ostype "Other" --register
VBox modifyvm $vm --memory 64 --vram 8 --cpus 1 --acpi on --ioapic off `
     --nic none --audio none --usb off --graphicscontroller vboxvga `
     --firmware bios --boot1 floppy --boot2 none --boot3 none --boot4 none --rtcuseutc on
VBox storagectl  $vm --name FDD --add floppy --controller I82078
VBox storageattach $vm --storagectl FDD --port 0 --device 0 --type fdd --medium $img

# ---- 启动（headless） ----
VBox startvm $vm --type headless
Start-Sleep -Seconds 6

function Shot([string]$name) {
    & $vbm controlvm $vm screenshotpng "$shots\$name.png" 2>$null
    Write-Host "shot: $name"
    Start-Sleep -Milliseconds 400
}

# PS/2 扫描码 set 1: 字符 -> (make,break) 对
$sc = @{
 'a'='1e a3'; 'b'='30 b0'; 'c'='2e ae'; 'd'='20 a0'; 'e'='12 92'; 'f'='21 a1';
 'g'='22 a2'; 'h'='23 a3'; 'i'='17 97'; 'j'='24 a4'; 'k'='25 a5'; 'l'='26 a6';
 'm'='32 b2'; 'n'='31 b1'; 'o'='18 98'; 'p'='19 99'; 'q'='10 90'; 'r'='13 93';
 's'='1f 9f'; 't'='14 94'; 'u'='16 96'; 'v'='2f af'; 'w'='11 91'; 'x'='2d ad';
 'y'='15 95'; 'z'='2c ac'; '0'='0b 8b'; '1'='02 82'; '2'='03 83'; '3'='04 84';
 '4'='05 85'; '5'='06 86'; '6'='07 87'; '7'='08 88'; '8'='09 89'; '9'='0a 8a';
 ' '='39 b9'; '.'='34 b4'; '/'='35 b5'; '-'='0c 8c'; '='='0d 8d'; '*'='37 b7'
}
function Type([string]$text) {
    $codes = @()
    foreach ($ch in $text.ToCharArray()) {
        if (-not $sc.ContainsKey([string]$ch)) { throw "no scancode for '$ch'" }
        $codes += ($sc[[string]$ch] -split ' ')
    }
    & $vbm controlvm $vm keyboardputscancode $codes
    Start-Sleep -Milliseconds 350
}
function Enter { & $vbm controlvm $vm keyboardputscancode 1c 9c; Start-Sleep -Milliseconds 350 }

Write-Host "== boot =="
Shot "01-boot"

Write-Host "== help =="
Type "help"; Enter; Start-Sleep -Milliseconds 800
Shot "02-help"

Write-Host "== calc 100 / 4 =="
Type "calc 100 / 4"; Enter; Start-Sleep -Milliseconds 800
Shot "03-calc"

Write-Host "== time =="
Type "time"; Enter; Start-Sleep -Milliseconds 800
Shot "04-time"

Write-Host "== uptime =="
Type "uptime"; Enter; Start-Sleep -Milliseconds 800
Shot "05-uptime"

Write-Host "== sysinfo =="
Type "sysinfo"; Enter; Start-Sleep -Milliseconds 800
Shot "06-sysinfo"

Write-Host "== demo =="
Type "demo"; Enter; Start-Sleep -Milliseconds 1200
Shot "07-demo"

Write-Host "== demo Tab =="
& $vbm controlvm $vm keyboardputscancode 0f 8f; Start-Sleep -Milliseconds 700
Shot "08-demo-tab"

Write-Host "== demo Left =="
& $vbm controlvm $vm keyboardputscancode e0 4b e0 cb; Start-Sleep -Milliseconds 700
Shot "09-demo-left"

Write-Host "== demo Esc =="
& $vbm controlvm $vm keyboardputscancode 01 81; Start-Sleep -Milliseconds 700
Shot "10-back-to-shell"

Write-Host "== poweroff =="
Type "poweroff"; Enter
Start-Sleep -Milliseconds 1200
& $vbm controlvm $vm screenshotpng "$shots\11-poweroff.png" 2>$null

# 停机（若内核已自行关机则忽略错误）
& $vbm controlvm $vm poweroff 2>$null | Out-Null

Write-Host "DONE. screenshots in $shots"
