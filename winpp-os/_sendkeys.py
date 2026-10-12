# -*- coding: utf-8 -*-
# 强制置前 Bochs Display 窗口并注入按键序列
import ctypes, time, sys

u = ctypes.windll.user32
k32 = ctypes.windll.kernel32

H = 1443642  # Bochs for Windows - Display

def get_fg():
    return u.GetForegroundWindow()

def force_foreground(h):
    fg = get_fg()
    fg_tid = u.GetWindowThreadProcessId(fg, None)
    h_tid = u.GetWindowThreadProcessId(h, None)
    u.AttachThreadInput(h_tid, fg_tid, True)
    u.BringWindowToTop(h)
    u.ShowWindow(h, 9)  # SW_RESTORE
    u.SetForegroundWindow(h)
    u.SetFocus(h)
    u.AttachThreadInput(h_tid, fg_tid, False)
    time.sleep(0.4)
    return get_fg() == h

class KEYBDINPUT(ctypes.Structure):
    _fields_ = [('wVk', ctypes.c_ushort), ('wScan', ctypes.c_ushort),
                ('dwFlags', ctypes.c_ulong), ('time', ctypes.c_ulong),
                ('dwExtraInfo', ctypes.POINTER(ctypes.c_ulong))]

class MOUSEINPUT(ctypes.Structure):
    _fields_ = [('dx', ctypes.c_long), ('dy', ctypes.c_long), ('mouseData', ctypes.c_ulong),
                ('dwFlags', ctypes.c_ulong), ('time', ctypes.c_ulong),
                ('dwExtraInfo', ctypes.POINTER(ctypes.c_ulong))]

class HARDWAREINPUT(ctypes.Structure):
    _fields_ = [('uMsg', ctypes.c_ulong), ('wParamL', ctypes.c_short), ('wParamH', ctypes.c_short)]

class _INPUTUNION(ctypes.Union):
    _fields_ = [('ki', KEYBDINPUT), ('mi', MOUSEINPUT), ('hi', HARDWAREINPUT)]

class INPUT(ctypes.Structure):
    _fields_ = [('type', ctypes.c_ulong), ('u', _INPUTUNION)]

INPUT_KEYBOARD = 1
KEYEVENTF_KEYUP = 0x0002

def sendkey(vk, scan, up=False):
    inp = INPUT()
    inp.type = INPUT_KEYBOARD
    inp.u.ki.wVk = vk
    inp.u.ki.wScan = scan
    inp.u.ki.dwFlags = KEYEVENTF_KEYUP if up else 0
    inp.u.ki.time = 0
    inp.u.ki.dwExtraInfo = ctypes.pointer(ctypes.c_ulong(0))
    u.SendInput(1, ctypes.byref(inp), ctypes.sizeof(INPUT))

def type_seq(chars):
    # vk, scan (PS/2 set 1)
    m = {'r': (0x52, 0x13), 'u': (0x55, 0x16), 'n': (0x4E, 0x31),
         'p': (0x50, 0x19), 's': (0x53, 0x1F), 'k': (0x4B, 0x25),
         'i': (0x49, 0x17), 'l': (0x4C, 0x26), 't': (0x54, 0x14),
         'o': (0x4F, 0x18), '\n': (0x0D, 0x1C), '0': (0x30, 0x0B),
         '1': (0x31, 0x02), '2': (0x32, 0x03), '3': (0x33, 0x04),
         '4': (0x34, 0x05), '5': (0x35, 0x06), '6': (0x36, 0x07),
         '7': (0x37, 0x08), '8': (0x38, 0x09), '9': (0x39, 0x0A),
         ' ': (0x20, 0x39)}
    for ch in chars:
        if ch not in m:
            print('skip', repr(ch))
            continue
        vk, scan = m[ch]
        sendkey(vk, scan, False)
        time.sleep(0.18)
        sendkey(vk, scan, True)
        time.sleep(0.18)

if __name__ == '__main__':
    cmd = sys.argv[1]
    ok = force_foreground(H)
    print('foreground ok:', ok, 'fg=', get_fg())
    if not ok:
        time.sleep(0.5)
        ok = force_foreground(H)
        print('retry foreground ok:', ok)
    time.sleep(0.3)
    if cmd == 'ENTER':
        vk, scan = 0x0D, 0x1C
        sendkey(vk, scan, False); time.sleep(0.18)
        sendkey(vk, scan, True); time.sleep(0.18)
        print('typed ENTER')
    else:
        type_seq(cmd)
        print('typed:', cmd)
