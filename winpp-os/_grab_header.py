# -*- coding: utf-8 -*-
"""裁剪窗口抓图的底部 header bar 区域，放大保存"""
import ctypes, ctypes.wintypes as w, sys, struct

h = int(sys.argv[1])
out = sys.argv[2]
u32 = ctypes.WinDLL("user32", use_last_error=True)
gdi32 = ctypes.WinDLL("gdi32", use_last_error=True)

r = w.RECT()
u32.GetWindowRect(h, ctypes.byref(r))
W = r.right - r.left; H = r.bottom - r.top

hdc = u32.GetWindowDC(h)
memdc = gdi32.CreateCompatibleDC(hdc)
bmp = gdi32.CreateCompatibleBitmap(hdc, W, H)
old = gdi32.SelectObject(memdc, bmp)
u32.PrintWindow(h, memdc, 0)

class BITMAPINFOHEADER(ctypes.Structure):
    _fields_ = [("biSize", w.DWORD), ("biWidth", w.LONG), ("biHeight", w.LONG),
                ("biPlanes", w.WORD), ("biBitCount", w.WORD), ("biCompression", w.DWORD),
                ("biSizeImage", w.DWORD), ("biXPelsPerMeter", w.LONG),
                ("biYPelsPerMeter", w.LONG), ("biClrUsed", w.DWORD), ("biClrImportant", w.DWORD)]

bi = BITMAPINFOHEADER()
bi.biSize = ctypes.sizeof(BITMAPINFOHEADER)
bi.biWidth = W; bi.biHeight = -H; bi.biPlanes = 1; bi.biBitCount = 24; bi.biCompression = 0
buf = ctypes.create_string_buffer(W * H * 3)
gdi32.GetDIBits(memdc, bmp, 0, H, buf, ctypes.byref(bi), 0)
gdi32.SelectObject(memdc, old)
gdi32.DeleteObject(bmp); gdi32.DeleteDC(memdc); u32.ReleaseDC(h, hdc)

# 底部 160 像素
crop_h = 160
start = H - crop_h
crop = bytearray()
row = (W * 3 + 3) & ~3
for y in range(crop_h):
    sy = start + y
    src = buf.raw[sy * W * 3:(sy + 1) * W * 3]
    crop.extend(src)
    pad = row - W * 3
    if pad: crop.extend(b"\0" * pad)

size = 14 + 40 + row * crop_h
fh = struct.pack("<2sIHHI", b"BM", size, 0, 0, 54)
ih = struct.pack("<IiiHHIIiiII", 40, W, crop_h, 1, 24, 0, row * crop_h, 2835, 2835, 0, 0)
with open(out, "wb") as f:
    f.write(fh); f.write(ih); f.write(bytes(crop))
print("saved", out, W, "x", crop_h, flush=True)
