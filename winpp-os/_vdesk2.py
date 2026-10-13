# -*- coding: utf-8 -*-
import ctypes, ctypes.wintypes as wt, re, sys

user32 = ctypes.windll.user32
ole32 = ctypes.windll.ole32

# CLSID_VirtualDesktopManager
CLSID = ctypes.create_string_buffer(16)
# {AA509086-5CA9-4C25-8F95-589D3C07B48A}
b = bytes.fromhex('AA509086-5CA9-4C25-8F95-589D3C07B48A')
# GUID 二进制顺序：data1(LE) data2(LE) data3(LE) data4(原序)
CLSID = (ctypes.c_ubyte * 16)()
CLSID[0], CLSID[1], CLSID[2], CLSID[3] = 0x86, 0x90, 0x50, 0xAA
CLSID[4], CLSID[5] = 0xA9, 0x5C
CLSID[6], CLSID[7] = 0x25, 0x4C
CLSID[8:] = bytes.fromhex('8F95589D3C07B48A')

IID = (ctypes.c_ubyte * 16)()
# {A5CD92FF-29BE-4BC7-9D2F-4F878E7151E3}
IID[0], IID[1], IID[2], IID[3] = 0xFF, 0x92, 0xCD, 0xA5
IID[4], IID[5] = 0xBE, 0x29
IID[6], IID[7] = 0xC7, 0x4B
IID[8:] = bytes.fromhex('9D2F4F878E7151E3')

def co_create(clsid, iid):
    p = ctypes.c_void_p()
    hr = ole32.CoCreateInstance(ctypes.byref(clsid), None, 1,  # CLSCTX_INPROC_SERVER
                                ctypes.byref(iid), ctypes.byref(p))
    if hr != 0:
        return None
    return ctypes.cast(p, ctypes.POINTER(ctypes.c_void_p))

h = int(sys.argv[1])
ole32.CoInitialize(None)
v = co_create(CLSID, IID)
if not v:
    print('CoCreateInstance failed')
    sys.exit(1)

# IVirtualDesktopManager vtable: QueryInterface(0) AddRef(1) Release(2)
# IsWindowOnCurrentVirtualDesktop(3) GetWindowDesktopId(4)
vtable = ctypes.cast(v, ctypes.POINTER(ctypes.POINTER(ctypes.c_void_p)))
fn = ctypes.CFUNCTYPE(ctypes.c_long, ctypes.c_void_p, ctypes.c_void_p)
is_on = ctypes.cast(vtable[0][3], fn)
res = is_on(v, h)
print('IsWindowOnCurrentVirtualDesktop:', res)
ole32.CoUninitialize()
