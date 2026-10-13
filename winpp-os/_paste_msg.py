# -*- coding: utf-8 -*-
# 设置剪贴板 + PostMessage 发 Ctrl+V 到 Bochs 窗口（wx 加速键 paste）
import ctypes, ctypes.wintypes as wt, time, sys

u = ctypes.windll.user32
k = ctypes.windll.kernel32
CF_UNICODETEXT = 13
WM_KEYDOWN = 0x0100
WM_KEYUP = 0x0101

def set_clipboard(text):
    u.OpenClipboard(0)
    u.EmptyClipboard()
    data = (text + '\x00').encode('utf-16-le')
    h = k.GlobalAlloc(0x0042, len(data))
    p = k.GlobalLock(h)
    ctypes.memmove(p, data, len(data))
    k.GlobalUnlock(h)
    u.SetClipboardData(CF_UNICODETEXT, h)
    u.CloseClipboard()

def post_ctrl_v(h):
    # VK_CONTROL=0x11 scan=0x1D, V=0x56 scan=0x2F
    u.PostMessageW(h, WM_KEYDOWN, 0x11, 0x1D0001)
    time.sleep(0.05)
    u.PostMessageW(h, WM_KEYDOWN, 0x56, 0x2F0001)
    time.sleep(0.05)
    u.PostMessageW(h, WM_KEYUP, 0x56, 0x2F0001 | 0xC0000000)
    time.sleep(0.05)
    u.PostMessageW(h, WM_KEYUP, 0x11, 0x1D0001 | 0xC0000000)
    time.sleep(0.1)

if __name__ == '__main__':
    h = int(sys.argv[1])
    text = sys.argv[2]
    set_clipboard(text)
    time.sleep(0.3)
    # 发给父窗口和 SIMWINDOW 各一份
    kids = []
    def cb(w, _):
        cls = ctypes.create_unicode_buffer(128)
        u.GetClassNameW(w, cls, 128)
        if cls.value == 'SIMWINDOW':
            kids.append(w)
        return True
    PROC = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
    u.EnumChildWindows(h, PROC(cb), 0)
    targets = [h] + kids
    for t in targets:
        post_ctrl_v(t)
        time.sleep(0.2)
    time.sleep(1.5)
    print('pasted', repr(text), '->', targets)
