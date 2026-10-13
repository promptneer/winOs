# -*- coding: utf-8 -*-
import ctypes, ctypes.wintypes as wt

u = ctypes.windll.user32
k = ctypes.windll.kernel32

# 窗口站名
ws = u.GetProcessWindowStation()
name = ctypes.create_unicode_buffer(256)
u.GetUserObjectInformationW(ws, 2, name, 512, None)  # UOI_NAME=2
print('agent window station:', name.value)

# 桌面名
desk = u.GetThreadDesktop(k.GetCurrentThreadId())
dname = ctypes.create_unicode_buffer(256)
u.GetUserObjectInformationW(desk, 2, dname, 512, None)
print('agent desktop:', dname.value)

# 会话
sess = ctypes.c_uint32()
k.ProcessIdToSessionId(k.GetCurrentProcessId(), ctypes.byref(sess))
print('agent session:', sess.value)

# Bochs 窗口的窗口站/桌面（用有效句柄）
import subprocess
out = subprocess.run(['powershell', '-Command',
    'Get-Process -Name bochs* -ErrorAction SilentlyContinue | Select-Object -First 1 Id'],
    capture_output=True, text=True)
print('bochs pid:', out.stdout.strip())
