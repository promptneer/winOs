# -*- coding: utf-8 -*-
"""PrintWindow 抓取指定窗口内容到 PNG（不依赖窗口可见性）"""
import ctypes, ctypes.wintypes as w, sys

h = int(sys.argv[1])
out = sys.argv[2]
u32 = ctypes.WinDLL("user32", use_last_error=True)
k32 = ctypes.WinDLL("kernel32", use_last_error=True)

r = w.RECT()
u32.GetWindowRect(h, ctypes.byref(r))
W = r.right - r.left; H = r.bottom - r.top
print("win rect:", r.left, r.top, r.right, r.bottom, "size:", W, "x", H, flush=True)

# GDI 位图
gdi32 = ctypes.WinDLL("gdi32", use_last_error=True)
hdc = u32.GetWindowDC(h)
memdc = gdi32.CreateCompatibleDC(hdc)
bmp = gdi32.CreateCompatibleBitmap(hdc, W, H)
old = gdi32.SelectObject(memdc, bmp)
ok = u32.PrintWindow(h, memdc, 0)   # 0=全窗口
print("PrintWindow ok:", ok, flush=True)

# 保存 BMP
class BITMAPINFOHEADER(ctypes.Structure):
    _fields_ = [("biSize", w.DWORD), ("biWidth", w.LONG), ("biHeight", w.LONG),
                ("biPlanes", w.WORD), ("biBitCount", w.WORD), ("biCompression", w.DWORD),
                ("biSizeImage", w.DWORD), ("biXPelsPerMeter", w.LONG),
                ("biYPelsPerMeter", w.LONG), ("biClrUsed", w.DWORD), ("biClrImportant", w.DWORD)]

bi = BITMAPINFOHEADER()
bi.biSize = ctypes.sizeof(BITMAPINFOHEADER)
bi.biWidth = W; bi.biHeight = -H; bi.biPlanes = 1; bi.biBitCount = 24; bi.biCompression = 0
buf = ctypes.create_string_buffer(W * H * 3)
got = gdi32.GetDIBits(memdc, bmp, 0, H, buf, ctypes.byref(bi), 0)
gdi32.SelectObject(memdc, old)
gdi32.DeleteObject(bmp); gdi32.DeleteDC(memdc); u32.ReleaseDC(h, hdc)
print("GetDIBits:", got, flush=True)

# 写 BMP 文件
bmp_header = b"BM"
import struct
row = (W * 3 + 3) & ~3
size = 14 + 40 + row * H
fh = struct.pack("<2sIHHI", b"BM", size, 0, 0, 54)
ih = struct.pack("<IiiHHIIiiII", 40, W, H, 1, 24, 0, row * H, 2835, 2835, 0, 0)
with open(out, "wb") as f:
    f.write(fh); f.write(ih)
    for y in range(H):
        f.write(buf.raw[y * W * 3:(y + 1) * W * 3])
        pad = row - W * 3
        if pad: f.write(b"\0" * pad)
print("saved:", out, flush=True)
