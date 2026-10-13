# -*- coding: utf-8 -*-
# 检查/切换到 Bochs 窗口所在桌面，再激活+点击+注入
import ctypes, ctypes.wintypes as wt, time, subprocess, sys

u = ctypes.windll.user32
k = ctypes.windll.kernel32
par = int(sys.argv[1])

# 1. 检查桌面归属
my_tid = k.GetCurrentThreadId()
my_desk = u.GetThreadDesktop(my_tid)
w_tid = u.GetWindowThreadProcessId(par, None)
w_desk = u.GetThreadDesktop(w_tid)
print('my desk:', my_desk, 'bochs desk:', w_desk, 'same:', my_desk == w_desk)

# 2. 如果不同，切换线程桌面
if my_desk != w_desk:
    r = u.SetThreadDesktop(w_desk)
    print('SetThreadDesktop:', r)
    time.sleep(0.2)

# 3. 激活 + 聚焦 SIMWINDOW
kids = []
def cb(w, _):
    cls = ctypes.create_unicode_buffer(128)
    u.GetClassNameW(w, cls, 128)
    if cls.value == 'SIMWINDOW':
        kids.append(w)
    return True
PROC = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
u.EnumChildWindows(par, PROC(cb), 0)
sim = kids[0] if kids else par
print('sim:', sim)

u.ShowWindow(par, 9)
u.SetForegroundWindow(par)
time.sleep(0.4)
u.SetFocus(sim)

# 4. 点击 SIMWINDOW
r = wt.RECT()
u.GetWindowRect(sim, ctypes.byref(r))
cx = (r.left + r.right) // 2
cy = (r.top + r.bottom) // 2
u.SetCursorPos(cx, cy)
u.mouse_event(0x0002, 0, 0, 0, 0)
time.sleep(0.12)
u.mouse_event(0x0004, 0, 0, 0, 0)
time.sleep(0.4)

# 5. 注入
if len(sys.argv) > 2:
    subprocess.run([sys.executable, r'D:\WinPP\winpp-os\_sendkeys.py', sys.argv[2]],
                   capture_output=True)
    print('typed', sys.argv[2])
print('done')
