# -*- coding: utf-8 -*-
# 抓取 Bochs Display 窗口内容（PrintWindow），参数: <h> <out>
import ctypes, ctypes.wintypes, struct, sys
u = ctypes.windll.user32
g = ctypes.windll.gdi32
h = int(sys.argv[1])
out = sys.argv[2]
r = ctypes.wintypes.RECT()
u.GetWindowRect(h, ctypes.byref(r))
w = r.right - r.left
hh = r.bottom - r.top
hdc = u.GetWindowDC(h)
mdc = g.CreateCompatibleDC(hdc)
bmp = g.CreateCompatibleBitmap(hdc, w, hh)
g.SelectObject(mdc, bmp)
ok = u.PrintWindow(h, mdc, 0x2)
class BI(ctypes.Structure):
    _fields_ = [('biSize', ctypes.c_uint), ('biWidth', ctypes.c_int), ('biHeight', ctypes.c_int),
                ('biPlanes', ctypes.c_ushort), ('biBitCount', ctypes.c_ushort),
                ('biCompression', ctypes.c_uint), ('biSizeImage', ctypes.c_uint),
                ('biXPels', ctypes.c_int), ('biYPels', ctypes.c_int),
                ('biClrUsed', ctypes.c_uint), ('biClrImportant', ctypes.c_uint)]
bi = BI()
bi.biSize = ctypes.sizeof(BI)
bi.biWidth = w
bi.biHeight = -hh
bi.biPlanes = 1
bi.biBitCount = 24
bi.biCompression = 0
buf = ctypes.create_string_buffer(w * hh * 3)
g.GetDIBits(hdc, bmp, 0, hh, buf, ctypes.byref(bi), 0)
hdr = b'BM' + struct.pack('<IHHI', 54 + len(buf), 0, 0, 54) + struct.pack('<IiiHHIIiiII', 40, w, hh, 1, 24, 0, len(buf), 0, 0, 0, 0)
open(out, 'wb').write(hdr + buf)
g.DeleteObject(bmp); g.DeleteDC(mdc); u.ReleaseDC(h, hdc)
print('ok', w, hh, ok)
