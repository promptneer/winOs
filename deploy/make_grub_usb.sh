#!/usr/bin/env bash
# ============================================================
# WinPP OS - GRUB 真机 U 盘启动盘一键制作脚本
# 适用环境：Linux 或 Ubuntu Live USB 环境（root / sudo）
# 用法：   sudo bash make_grub_usb.sh /dev/sdX
# 警告：   /dev/sdX 整盘会被覆写！先用 lsblk 确认盘符，输错会毁盘。
# 依赖：   parted, dosfstools, grub-pc-bin（缺失会自动 apt 安装）
# ============================================================
set -euo pipefail

DEV="${1:-}"
if [ -z "$DEV" ]; then
  echo "用法: sudo bash make_grub_usb.sh /dev/sdX"
  echo "     sdX 是你的 U 盘设备名，先用 lsblk 查看确认"
  exit 1
fi

echo ">>> 目标设备: $DEV"
lsblk "$DEV" || { echo "设备不存在，请检查盘符"; exit 1; }
read -r -p ">>> 即将完全覆写 $DEV 全部数据，确认无误请输入 yes 继续: " ans
[ "$ans" = "yes" ] || { echo "已取消"; exit 1; }

# 0. 卸载可能挂载的分区
umount "$DEV"?* 2>/dev/null || true

# 1. 分区：MBR + 单个 FAT32 主分区，设启动标志
echo ">>> [1/5] 分区（MBR + FAT32）..."
wipefs -a "$DEV"
parted -s "$DEV" mklabel msdos
parted -s "$DEV" mkpart primary fat32 1MiB 100%
parted -s "$DEV" set 1 boot on
sleep 2

PART="${DEV}1"
[ -b "$PART" ] || PART="$(lsblk -lnp -o NAME "$DEV" | sed -n '2p')"
echo ">>> 分区: $PART"

# 2. 格式化 FAT32
echo ">>> [2/5] 格式化 FAT32..."
mkfs.vfat -F 32 "$PART"

# 3. 挂载
mkdir -p /mnt/usb
mount "$PART" /mnt/usb

# 4. 安装 GRUB（Legacy BIOS / i386-pc）到 MBR
echo ">>> [3/5] 安装 GRUB i386-pc..."
if ! command -v grub-install >/dev/null 2>&1; then
  echo "    未找到 grub-install，尝试安装 grub-pc-bin ..."
  apt-get update >/dev/null 2>&1 || true
  apt-get install -y grub-pc-bin >/dev/null 2>&1 || { echo "安装失败，请手动: apt install grub-pc-bin"; exit 1; }
fi
grub-install --target=i386-pc --boot-directory=/mnt/usb/boot "$DEV"

# 5. 拷贝内核 + 写 GRUB 菜单
echo ">>> [4/5] 拷贝 WinPP 内核与菜单..."
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cp "$SCRIPT_DIR/winpp-kernel.elf" /mnt/usb/boot/
mkdir -p /mnt/usb/boot/grub
cp "$SCRIPT_DIR/grub.cfg" /mnt/usb/boot/grub/grub.cfg

sync
umount /mnt/usb
echo ">>> [5/5] 完成！启动盘已做好。"
echo ">>> 下一步：拔下 U 盘 -> 插目标机器 -> BIOS 关 Secure Boot / 开 CSM"
echo ">>>         启动顺序 U 盘第一 -> 出现 GRUB 菜单选 'WinPP OS'"
