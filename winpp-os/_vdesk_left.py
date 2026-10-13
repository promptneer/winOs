# -*- coding: utf-8 -*-
import ctypes, time
k = ctypes.windll.kernel32

def combo(*keys):
    # keys: (vk, scan) list, down then up in order
    for vk, sc in keys:
        k.keybd_event(vk, sc, 0, 0)
    time.sleep(0.08)
    for vk, sc in reversed(keys):
        k.keybd_event(vk, sc, 2, 0)
    time.sleep(0.3)

# Win+Ctrl+Left 切回左侧虚拟桌面（最多 3 次）
for i in range(3):
    combo((0x5B, 0x5B), (0x11, 0x1D), (0x25, 0x4B))
    time.sleep(0.6)
print('switched left x3')
