# -*- coding: utf-8 -*-
"""枚举 Bochs 窗口 → 恢复显示 → 置前 → 返回窗口 rect"""
import ctypes, ctypes.wintypes as w, json, sys

k32 = ctypes.WinDLL("kernel32", use_last_error=True)
u32 = ctypes.WinDLL("user32", use_last_error=True)
WNDENUMPROC = ctypes.WINFUNCTYPE(w.BOOL, w.HWND, w.LPARAM)
u32.EnumWindows.restype = w.BOOL
u32.EnumWindows.argtypes = [WNDENUMPROC, w.LPARAM]
u32.GetWindowTextW.restype = ctypes.c_int
u32.GetWindowTextW.argtypes = [w.HWND, w.LPWSTR, ctypes.c_int]
u32.GetClassNameW.restype = ctypes.c_int
u32.GetClassNameW.argtypes = [w.HWND, w.LPWSTR, ctypes.c_int]
u32.IsWindowVisible.restype = w.BOOL
u32.IsWindowVisible.argtypes = [w.HWND]
u32.GetWindowRect.restype = w.BOOL
u32.GetWindowRect.argtypes = [w.HWND, ctypes.POINTER(w.RECT)]

def get_title(h):
    b = ctypes.create_unicode_buffer(256)
    u32.GetWindowTextW(h, b, 256)
    return b.value

def get_cls(h):
    b = ctypes.create_unicode_buffer(256)
    u32.GetClassNameW(h, b, 256)
    return b.value

found = []
@WNDENUMPROC
def cb(h, lp):
    t = get_title(h)
    if "bochs" in t.lower():
        cls = get_cls(h)
        vis = bool(u32.IsWindowVisible(h))
        r = w.RECT()
        u32.GetWindowRect(h, ctypes.byref(r))
        found.append({"h": int(h), "title": t, "cls": cls, "vis": vis,
                      "rect": [r.left, r.top, r.right, r.bottom]})
    return True

u32.EnumWindows(cb, 0)
print(json.dumps(found, ensure_ascii=False, indent=1))

# 恢复 + 置前（最匹配的那个：标题含 Display）
target = None
for f in found:
    if "Display" in f["title"]:
        target = f
        break
if target is None and found:
    target = found[0]
if target:
    h = target["h"]
    u32.ShowWindow(h, 9)   # SW_RESTORE
    u32.BringWindowToTop(h)
    u32.SetForegroundWindow(h)
    u32.SetFocus(h)
    print("ACTIVATED h=%d %s rect=%s" % (h, target["title"], target["rect"]))
