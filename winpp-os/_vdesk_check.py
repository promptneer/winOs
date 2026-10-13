# -*- coding: utf-8 -*-
import ctypes, ctypes.wintypes as wt, ctypes.wintypes
import pythoncom
from win32com.client import Dispatch, CoClassBase
# 用 comtypes 或 ctypes 调 IVirtualDesktopManager
# CLSID_VirtualDesktopManager = {AA509086-5CA9-4C25-8F95-589D3C07B48A}
# IID_IVirtualDesktopManager = {A5CD92FF-29BE-4BC7-9D2F-4F878E7151E3}

user32 = ctypes.windll.user32
h = int(open(r'D:\WinPP\winpp-os\_dbg.txt').read().strip())

# 直接检查前台窗口与 Bochs 是否同桌面
fg = user32.GetForegroundWindow()
print('H ->', h, 'foreground ->', fg, 'same:', fg == h)

# 检查窗口可见状态
print('visible:', user32.IsWindowVisible(h), 'iconic:', user32.IsIconic(h))

# 尝试 COM VirtualDesktopManager
try:
    import comtypes.client as cc
    vdm = cc.CreateObject('{AA509086-5CA9-4C25-8F95-589D3C07B48A}')
    # IVirtualDesktopManager::IsWindowOnCurrentVirtualDesktop
    on_current = vdm.IsWindowOnCurrentVirtualDesktop(h)
    print('IsWindowOnCurrentVirtualDesktop:', on_current)
except Exception as e:
    print('com error:', e)

# 窗口是否属于"当前输入桌面"（GetThreadDesktop vs GetDesktopWindow 的桌面）
try:
    w_tid = user32.GetWindowThreadProcessId(h, None)
    w_desk = user32.GetThreadDesktop(w_tid)
    cur_desk = user32.GetThreadDesktop(user32.GetCurrentThreadId())
    print('win desk:', w_desk, 'cur desk:', cur_desk)
except Exception as e:
    print('desk err:', e)
