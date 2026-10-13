# -*- coding: utf-8 -*-
# 强制恢复 Bochs Display 窗口到可见位置并激活，点击后注入按键
import ctypes, ctypes.wintypes, time, sys, subprocess

u = ctypes.windll.user32
h = int(sys.argv[1])

# 1. 显示 + 移动到固定位置
u.ShowWindow(h, 5)                      # SW_SHOW
u.ShowWindow(h, 9)                      # SW_RESTORE
u.MoveWindow(h, 120, 90, 740, 509, True)
time.sleep(0.5)

# 2. 激活（双保险）
fg = u.GetForegroundWindow()
u.AttachThreadInput(u.GetWindowThreadProcessId(fg, None),
                    u.GetWindowThreadProcessId(h, None), True)
u.BringWindowToTop(h)
u.SetForegroundWindow(h)
u.SetFocus(h)
u.AttachThreadInput(u.GetWindowThreadProcessId(fg, None),
                    u.GetWindowThreadProcessId(h, None), False)
time.sleep(0.5)

# 3. 点击窗口内容区中心
r = ctypes.wintypes.RECT()
u.GetWindowRect(h, ctypes.byref(r))
cx = (r.left + r.right) // 2
cy = (r.top + r.bottom) // 2
u.SetCursorPos(cx, cy)
u.mouse_event(0x0002, 0, 0, 0, 0)
time.sleep(0.1)
u.mouse_event(0x0004, 0, 0, 0, 0)
time.sleep(0.4)

# 4. 注入按键
if len(sys.argv) > 2:
    subprocess.run([sys.executable, r'D:\WinPP\winpp-os\_sendkeys.py', sys.argv[2]],
                   capture_output=True)
    print('typed', sys.argv[2])
print('activated', h, 'rect', r.left, r.top, r.right, r.bottom)
