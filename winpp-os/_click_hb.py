# -*- coding: utf-8 -*-
"""点击 Bochs header bar 按钮（PostMessage WM_LBUTTONDOWN/UP）
用法: python _click_hb.py <x> <y>   (客户区像素坐标)
同时设置剪贴板内容（Bochs paste 按钮读取）。"""
import ctypes, ctypes.wintypes as w, sys, time

u32 = ctypes.WinDLL("user32", use_last_error=True)
k32 = ctypes.WinDLL("kernel32", use_last_error=True)

WM_LBUTTONDOWN = 0x0201
WM_LBUTTONUP = 0x0202
MK_LBUTTON = 0x0001

def set_clipboard(text):
    u32.OpenClipboard(0)
    u32.EmptyClipboard()
    data = text.encode("utf-16-le") + b"\0\0"
    hmem = k32.GlobalAlloc(0x0042, len(data))   # GMEM_MOVEABLE|GMEM_ZEROINIT
    ptr = k32.GlobalLock(hmem)
    ctypes.memmove(ptr, data, len(data))
    k32.GlobalUnlock(hmem)
    u32.SetClipboardData(13, hmem)   # CF_UNICODETEXT
    u32.CloseClipboard()

def click(h, x, y):
    lp = (y << 16) | (x & 0xFFFF)
    u32.PostMessageW(h, WM_LBUTTONDOWN, MK_LBUTTON, lp)
    u32.PostMessageW(h, WM_LBUTTONUP, 0, lp)
    time.sleep(1.0)

if __name__ == "__main__":
    h = int(sys.argv[1])
    x = int(sys.argv[2])
    y = int(sys.argv[3])
    txt = sys.argv[4] if len(sys.argv) > 4 else "ps\r"
    set_clipboard(txt)
    print("clipboard set:", repr(txt), flush=True)
    click(h, x, y)
    print("clicked", x, y, flush=True)
