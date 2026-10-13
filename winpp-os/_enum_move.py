# -*- coding: utf-8 -*-
import ctypes, ctypes.wintypes as wt, re

user32 = ctypes.windll.user32
hwnds = []
def cb(h, _):
    if user32.IsWindowVisible(h):
        n = ctypes.create_unicode_buffer(256)
        user32.GetWindowTextW(h, n, 256)
        if 'Bochs' in n.value:
            hwnds.append((h, n.value))
    return True
PROC = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
user32.EnumWindows(PROC(cb), 0)
print('windows:', hwnds)
disp = [h for h, t in hwnds if 'Display' in t]
if not disp:
    print('NO display window found')
else:
    h = disp[0]
    open(r'D:\WinPP\winpp-os\_dbg.txt', 'w').write(str(h))
    s = open(r'D:\WinPP\winpp-os\_sendkeys.py', encoding='utf-8').read()
    s = re.sub(r'H = \d+', 'H = ' + str(h), s)
    open(r'D:\WinPP\winpp-os\_sendkeys.py', 'w', encoding='utf-8').write(s)
    r = wt.RECT()
    user32.GetWindowRect(h, ctypes.byref(r))
    print('H:', h, 'rect:', r.left, r.top, r.right, r.bottom)
    # 虚拟屏幕范围
    vs = wt.RECT()
    user32.SystemParametersInfoW(0x0070, 0, ctypes.byref(vs), 0)  # SPI_GETWORKAREA? 用虚拟屏
    print('work area:', vs.left, vs.top, vs.right, vs.bottom)
    # 如果窗口不在虚拟屏内，移到主屏
    if not (r.left >= -50 and r.left < 1920 and r.top >= -50 and r.top < 1080):
        user32.ShowWindow(h, 9)
        user32.MoveWindow(h, 120, 90, 740, 509, True)
        user32.GetWindowRect(h, ctypes.byref(r))
        print('moved to', r.left, r.top, r.right, r.bottom)
