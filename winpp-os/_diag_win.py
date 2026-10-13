# -*- coding: utf-8 -*-
"""诊断 Bochs 窗口状态：可见性/前台/窗口站/桌面"""
import ctypes, ctypes.wintypes as w, sys

h = int(sys.argv[1])
u32 = ctypes.WinDLL("user32", use_last_error=True)
k32 = ctypes.WinDLL("kernel32", use_last_error=True)

print("IsWindow:", bool(u32.IsWindow(h)))
print("IsWindowVisible:", bool(u32.IsWindowVisible(h)))
print("IsIconic:", bool(u32.IsIconic(h)))
print("IsZoomed:", bool(u32.IsZoomed(h)))
fg = u32.GetForegroundWindow()
print("ForegroundWindow:", int(fg), "== target:", int(fg) == h)

# 前台窗口信息
b = ctypes.create_unicode_buffer(256)
u32.GetWindowTextW(fg, b, 256)
print("FG title:", b.value)

# 窗口站/桌面
ws = k32.GetProcessWindowStation()
print("WindowStation handle:", int(ws))
name = ctypes.create_unicode_buffer(256)
n = w.DWORD()
k32.GetUserObjectInformationW(ws, 2, name, 512, ctypes.byref(n))  # UOI_NAME
print("WindowStation name:", name.value)

desk = u32.GetThreadDesktop(ctypes.windll.kernel32.GetCurrentThreadId())
k32.GetUserObjectInformationW(desk, 2, name, 512, ctypes.byref(n))
print("Desktop name:", name.value)

# 目标窗口所在线程的桌面
tid = u32.GetWindowThreadProcessId(h, None)
desk2 = u32.GetThreadDesktop(tid)
name2 = ctypes.create_unicode_buffer(256)
k32.GetUserObjectInformationW(desk2, 2, name2, 512, ctypes.byref(n))
print("Target thread desktop:", name2.value)

# 目标窗口 rect
r = w.RECT()
u32.GetWindowRect(h, ctypes.byref(r))
print("Rect:", r.left, r.top, r.right, r.bottom)
