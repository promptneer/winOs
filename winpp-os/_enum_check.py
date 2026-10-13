# -*- coding: utf-8 -*-
import ctypes, ctypes.wintypes as wt, re, os
user32 = ctypes.windll.user32
kernel32 = ctypes.windll.kernel32

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
disp = [h for h, t in hwnds if 'Display' in t]
open(r'D:\WinPP\winpp-os\_dbg.txt', 'w').write(str(disp[0]) if disp else '')
s = open(r'D:\WinPP\winpp-os\_sendkeys.py', encoding='utf-8').read()
s = re.sub(r'H = \d+', 'H = ' + str(disp[0]), s)
open(r'D:\WinPP\winpp-os\_sendkeys.py', 'w', encoding='utf-8').write(s)
print('H ->', disp[0] if disp else 'NONE')

pid = kernel32.GetCurrentProcessId()
sess = ctypes.c_uint32()
kernel32.ProcessIdToSessionId(pid, ctypes.byref(sess))
print('agent session:', sess.value)

# Bochs 进程会话
out = os.popen('tasklist /FI "IMAGENAME eq bochs.exe" /FO CSV').read()
print('bochs tasklist:')
print(out)
