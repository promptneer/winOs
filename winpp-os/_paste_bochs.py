# -*- coding: utf-8 -*-
# Bochs 键盘粘贴：设置剪贴板文本 + 向 Bochs 发 Ctrl+V（Bochs wx GUI 的 paste 功能，内部转 PS/2 扫描码）
import ctypes, ctypes.wintypes as wt, time, sys, subprocess

u = ctypes.windll.user32
k = ctypes.windll.kernel32
CF_UNICODETEXT = 13

def set_clipboard(text):
    u.OpenClipboard(0)
    u.EmptyClipboard()
    # 分配全局内存放 UTF-16 文本
    data = (text + '\x00').encode('utf-16-le')
    h = k.GlobalAlloc(0x0042, len(data))  # GMEM_MOVEABLE
    p = k.GlobalLock(h)
    ctypes.memmove(p, data, len(data))
    k.GlobalUnlock(h)
    u.SetClipboardData(CF_UNICODETEXT, h)
    u.CloseClipboard()

def send_ctrl_v():
    # Ctrl down, V down/up, Ctrl up
    u.keybd_event(0x11, 0x1D, 0, 0)
    time.sleep(0.1)
    u.keybd_event(0x56, 0x2F, 0, 0)
    time.sleep(0.05)
    u.keybd_event(0x56, 0x2F, 0x0002, 0)
    time.sleep(0.1)
    u.keybd_event(0x11, 0x1D, 0x0002, 0)
    time.sleep(0.2)

if __name__ == '__main__':
    h = int(sys.argv[1])
    text = sys.argv[2]
    # 确保 Bochs 窗口前台
    u.ShowWindow(h, 9)
    u.SetForegroundWindow(h)
    time.sleep(0.3)
    set_clipboard(text)
    time.sleep(0.2)
    send_ctrl_v()
    time.sleep(1.5)
    print('pasted', repr(text), '->', h)
