# -*- coding: utf-8 -*-
# 点击 Bochs Display 窗口中心（激活 + 键盘捕获），然后可选注入按键
import ctypes, ctypes.wintypes, time, sys

u = ctypes.windll.user32

def click_center(h):
    r = ctypes.wintypes.RECT()
    u.GetWindowRect(h, ctypes.byref(r))
    cx = (r.left + r.right) // 2
    cy = (r.top + r.bottom) // 2
    # 先 SetForegroundWindow + SetFocus
    u.SetForegroundWindow(h)
    u.SetFocus(h)
    time.sleep(0.3)
    # 鼠标移到中心并左键单击（按下+抬起）
    u.SetCursorPos(cx, cy)
    u.mouse_event(0x0002, 0, 0, 0, 0)   # LEFTDOWN
    time.sleep(0.1)
    u.mouse_event(0x0004, 0, 0, 0, 0)   # LEFTUP
    time.sleep(0.4)
    print('clicked', h, (cx, cy))

if __name__ == '__main__':
    h = int(sys.argv[1])
    click_center(h)
