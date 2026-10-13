# -*- coding: utf-8 -*-
"""直接用句柄恢复 Bochs 窗口并置前"""
import ctypes, ctypes.wintypes as w, sys

h = int(sys.argv[1]) if len(sys.argv) > 1 else 1443264
u32 = ctypes.WinDLL("user32", use_last_error=True)
u32.ShowWindow(h, 9)          # SW_RESTORE
u32.ShowWindow(h, 5)          # SW_SHOW
r = w.RECT()
u32.GetWindowRect(h, ctypes.byref(r))
u32.MoveWindow(h, 60, 40, 800, 560, True)
u32.BringWindowToTop(h)
u32.SetForegroundWindow(h)
u32.SetFocus(h)
u32.GetWindowRect(h, ctypes.byref(r))
print("h=%d rect=%d,%d,%d,%d" % (h, r.left, r.top, r.right, r.bottom))
