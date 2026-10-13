# -*- coding: utf-8 -*-
import io
s = io.open(r"D:\WinPP\winpp-os\serial.out", encoding="utf-8", errors="replace").read()
print("LEN:", len(s))
print(s[:6000])
