# WinPP OS — 一个能开机、能交互的迷你内核

WinPP OS 是一个从零写的 i386 迷你操作系统内核（软盘引导 + 32 位保护模式，
另带 Multiboot v1 头），自带引导扇区、GDT/IDT、8259 PIC、PIT 定时器、
PS/2 键盘驱动、CMOS RTC、COM1 串口调试输出、80x25 VGA 文本驱动，
上面跑一个 Unix 风格的交互 Shell，外加一个可切换窗口的"桌面 UI"演示。

本内核已在 **Bochs 2.8 模拟器中实机开机并完成全程键盘交互验证**：
开机 → Shell 提示符 → 逐键输入 7 条命令（help/time/uptime/sysinfo/ls/about/demo）
→ 全部执行并输出到屏幕 → `demo` 打开 3 个重叠窗口（Tab 切焦点、方向键移动、
Esc 返回 Shell）。机器侧真值由 COM1 串口记录（14 次全屏转储 + 39 次按键日志 +
0 故障），见 `shots/` 实测截图。

## 功能

- **开机**：自写 512B 引导扇区（bootsec.S，CHS 读盘 → A20 → LGDT → 保护模式跳 1MB），
  另一条路径是 Multiboot v1 头（boot.S），GRUB/QEMU `-kernel` 可直接加载
- **中断**：完整 IDT（0-31 异常 + 32-47 IRQ 重映射），异常有寄存器转储 + Panic 屏
- **驱动**：PIT 100Hz 时钟、PS/2 键盘（Shift/Caps/E0 方向键）、CMOS 实时时钟、COM1 串口
- **交互 Shell**（Unix 风格提示符 `winpp@kernel:~$`）：
  - `help` 命令列表、`about` 版本信息、`echo` 回显
  - `time` 读 CMOS 时钟、`uptime`、`sysinfo`（CPU 厂商/RAM/时钟节拍）
  - `calc a op b` 内核内整数运算、`color` VGA 调色板
  - `ls` 虚拟文件系统、`demo` 窗口 UI、`beep` 蜂鸣
  - `reboot` 8042 复位、`poweroff` ACPI 关机（QEMU）
- **窗口 UI 演示**：`demo` 命令打开 3 个重叠窗口（Terminal/Files/Settings），
  `Tab` 切换焦点、方向键移动窗口、`Esc` 返回 Shell
- **状态栏**：顶部实时显示系统时钟、运行时间、内存（每秒刷新）

## 目录结构

```
winpp-os/
├── src/
│   ├── bootsec.S      # 512B 引导扇区（软盘 CHS 引导）
│   ├── bootsec.ld     # 引导扇区链接脚本
│   ├── boot.S         # Multiboot v1 头 + 32 位入口（SSE/A20/LGDT）
│   ├── isr.S          # 32 个异常 + 16 个 IRQ 存根（GAS）
│   ├── kernel.c       # kmain、异常/IRQ 分发、Panic、状态栏、重启/关机
│   ├── gdt.c/.h       # 平坦内存模型 GDT
│   ├── idt.c/.h       # IDT 与 PIC 重映射
│   ├── vga.c/.h       # 80x25 文本驱动 + 自带 printf/画框
│   ├── keyboard.c/.h  # PS/2 键盘 + 环形缓冲（Enter=0x1C→'\n'）
│   ├── timer.c/.h     # PIT 100Hz
│   ├── rtc.c/.h       # CMOS 实时时钟
│   ├── serial.c/.h    # COM1 调试输出（机器侧取证通道）
│   ├── shell.c/.h     # 交互命令 Shell（每键串口日志 + 命令后全屏转储）
│   ├── demo.c/.h      # 窗口 UI 演示
│   └── linker_flat.ld / linker.ld   # 链接脚本（加载到 1MB）
├── build_flat.ps1     # 构建脚本：bootsec + kernel → floppy.img
├── build.ps1          # 构建脚本：Multiboot ELF
├── bochsrc.txt        # Bochs 配置（软盘引导 + COM1 串口文件）
├── floppy.img         # 1.44MB 启动盘镜像（扇区0=引导扇区，扇区1起=内核）
├── kernel.bin         # 扁平内核（20,496B，入口 0x100000）
├── winpp-kernel.elf   # Multiboot 内核（24,304B）
├── serial.out         # COM1 串口记录（按键日志 + 屏幕转储，机器侧真值）
├── bochs.log          # Bochs 运行日志（0 故障）
└── shots/             # 实测截图（01-10 系列）
```

## 构建

依赖：`zig` 0.17.0（winget portable，本机：
`C:\Users\Administrator\AppData\Local\Microsoft\WinGet\Packages\zig.zig_...\zig.exe`）。
构建关键点：32 位目标是 `x86-freestanding`；zig 0.17 不支持 `--oformat=binary`，
先出 ELF 再 `zig objcopy -O binary`。

```powershell
powershell -ExecutionPolicy Bypass -File D:\WinPP\winpp-os\build_flat.ps1
```

产物：`floppy.img`（1,474,560B，41 个内核扇区）、`kernel.bin`、`bootsec.bin`（0xAA55 校验通过）。

## 在 Bochs 2.8 中开机实测（本机已验证）

依赖：Bochs 2.8（本机 `C:\Bochs-2.8\bochs.exe`，配置文件 `D:\WinPP\winpp-os\bochsrc.txt`）。
配置：软盘 `floppya: 1_44=floppy.img`、`boot: a`、COM1 串口输出到 `serial.out`。

```powershell
# 无头自动化验证：跑 6-12 秒后强杀，检查 bochs.log（0 故障）与 serial.out（启动轨迹）
Start-Process "C:\Bochs-2.8\bochs.exe" -ArgumentList "-q","-f","D:\WinPP\winpp-os\bochsrc.txt"
Start-Sleep 8; Get-Process bochs | Stop-Process -Force
Get-Content D:\WinPP\winpp-os\bochs.log   # 应无 exception/panic/fault
Get-Content D:\WinPP\winpp-os\serial.out  # 应有 "[1] gdt ... [7] all init ok"
```

GUI 交互实测：打开 Bochs → Load 配置 `bochsrc.txt` → Start，用真实键盘输入命令。
屏幕与串口记录均为真实机器取证（非脚本模拟）。

## 实测证据（2026-10-12，Bochs 2.8 实机）

机器侧真值通道 = COM1 串口（`serial.out`）：内核启动轨迹、逐键日志、每次命令
执行前后的 25 行全屏转储，全部由内核自己写入，非截图猜测。

- 串口启动轨迹：`=== WinPP kernel boot ===` → `[1] gdt ... [7] all init ok, drawing UI ...`，0 故障
- 按键日志：`[key 104][key 101][key 108][key 112]...` 逐键确认送达
- 屏幕转储：7 条命令 × before/after = 14 次全屏转储，行文本完整记录命令与输出
- Bochs 日志：`bochs.log` 无任何异常/故障/重启

实测截图（`shots/`）：

1. `01-kernel-shell.png` 开机进入 Shell（早期版本）
2. `05-fixed-boot.png` 修复 Enter 后全新开机（状态栏/时钟/内存/提示符就位）
3. `06-help-time.png` `help` 命令表 + `time` CMOS 时钟输出
4. `07-uptime-sysinfo-ls.png` `uptime`（真实节拍 1661509）`sysinfo`（GenuineIntel/65152KB）`ls`（虚拟 FS）
5. `08-demo-ui.png` `demo` 三个重叠窗口（Terminal `ps` 进程表 / Files / Settings）
6. `09-demo-interact.png` Tab 切换焦点 + 方向键移动窗口后
7. `10-esc-back.png` Esc 返回 Shell，提示符就位

## 排障记录（修复的关键 Bug）

1. **引导扇区 LBA 偏移**：CHS 计算误用 si 本身，读到 LBA 0（引导扇区自身）覆盖 1MB 代码，
   三重故障。修复：`LBA = si + 1`。
2. **SSE 未开启**：编译器生成 `pxor xmm0,xmm0` 而 CR4 无 OSFXSR，异常复位。
   修复：boot.S 入口设置 CR4.OSFXSR|OSXMMEXCPT(0x600)。
3. **magic=600**：CR4 代码误放在 push magic/info 之前破坏 EAX。修复：先 push 再开 CR4。
4. **扁平路径 g_mem_kb=0**：加 CMOS 0x70/0x31 读取扩展内存，最终 65152 KB 正确显示。
5. **vga_printf `%04u` 补零死循环**：满屏 '0' 卡死。修复：丢弃逐个 putc 补零，
   改为内部 buf 补零后统一输出。
6. **Enter 永不分发（交互闭环的最后一块拼图）**：键盘驱动把 Enter 扫描码 0x1C 映射为
   `'\n'`(10)，但 keyboard.h 里 `KEY_ENTER` 误定义为 `13`(CR)，`shell_readline` 判断
   `k == KEY_ENTER` 永不命中 → 命令永远不执行、提示符文本不断累积。修复：
   `#define KEY_ENTER '\n'`。修复后 help/time/uptime/sysinfo/ls/about/demo 全部实机执行成功。
7. **Mode13h 寄存器组错 + 属性控制器/调色板没编程（GUI 完全看不见画面）**：CRTC 表把 CR08 写成
   0x28（bit5-6 = 垂直像素平移）、CR0C/0D 起始地址写成 0x0028、CR13 偏移写成 0x40，且 0x3C0
   属性控制器与 0x3C8/0x3C9 调色板从未写过 → Bochs 报 `vertical pel panning not supported` /
   `byte panning not implemented`，窗口几何算成 640x271、画面不刷新。修复：改用 VGA BIOS 官方
   模式表（vgabios vgatables.h）的 Mode13h 组（CR08=0x00、CR13=0x28、SR4=0x0E…）+ 补 21 个 AC
   寄存器与 16 色 DAC。修复后平移告警 0 条、几何 640x400、4 个窗口正常画出。
8. **退出图形模式满屏竖条纹（文本字体被写坏）**：Mode13h 的 64KB 帧缓冲在 chain4 下写穿 plane 0-3
   各 16KB，plane 2 偏移 0-8K 正是文本 8x16 字体；BIOS 切模式会重装字体，内核自己切就得自己存/装。
   修复：进图形模式前保存整套文本寄存器组（Misc/Sequencer/CRTC/GC/AC/DAC）+ 8KB 字体，退出写回。
   注意字体访问必须用经典配方 `SR2=04 / SR4=06 / GC4=02 / GC5=00 / GC6=00（A0000 窗口）`，否则
   文本模式的字母数字模式会把奇/偶地址分流到 plane 0/1，字体只写回一半（字形带条纹）。
   校验：8KB 字体 saved=restored=`c8dbddd1`，mismatch=0。
9. **0xB9000 页错误**：分页只映射了 0xB8000 一页，而字体在 0xB8000-0xB9FFF，保存字体时读
   `0xB9000` 立刻 #PF 挂死（串口 `[pfdbg] cr2=b9000`）。修复：整段映射 0xB8000-0xBFFFF（8 页）；
   #PF 演示仍打地址 0，不受影响。
10. **Bochs 退出图形模式后残留图形像素**：win32 前端只重绘"字符有变化"的单元。修复：退出时先用
   可见字符铺满整屏强制全部变化一次再清屏重绘（实机 CRT 持续扫描显存，无此问题）。

## 在真实机器上启动

1. 用软盘镜像：把 `floppy.img` 写到真实软盘/USB（Legacy BIOS）直接启动。
2. 或用 Multiboot ELF：`winpp-kernel.elf` 拷到装有 GRUB（Legacy BIOS）的机器上：

   ```
   menuentry "WinPP OS" {
       multiboot /boot/winpp-kernel.elf
   }
   ```

> 注意：内核目前只支持 32 位保护模式 + VGA 文本模式，未包含存储/网络/分页驱动，
> 属于教学级迷你内核。UEFI 机器需以 CSM/Legacy 模式启动。
