import ctypes, time
from ctypes import wintypes
user32 = ctypes.windll.user32

hwnd = 723014
# 移到屏幕中央可见位置并置顶
user32.ShowWindow(hwnd, 9)            # SW_RESTORE
user32.MoveWindow(hwnd, 60, 80, 860, 660, True)
user32.SetWindowPos(hwnd, -1, 0, 0, 0, 0, 0x0001 | 0x0002)  # HWND_TOPMOST + NOMOVE/NOSIZE
user32.SetForegroundWindow(hwnd)
user32.SetFocus(hwnd)
user32.BringWindowToTop(hwnd)
time.sleep(1)
rect = wintypes.RECT()
user32.GetWindowRect(hwnd, ctypes.byref(rect))
print("rect:", rect.left, rect.top, rect.right, rect.bottom)
print("visible:", user32.IsWindowVisible(hwnd))
