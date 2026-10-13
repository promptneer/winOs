# -*- coding: utf-8 -*-
import io, re
p = r"D:\WinPP\winpp-os\bochs.log"
s = io.open(p, encoding="utf-8", errors="replace").read()
lines = s.splitlines()
# 找 ne2k 相关行
hits = [l for l in lines if "NE2K" in l or "ne2k" in l.lower() or "slirp" in l.lower()]
print("NE2K/slirp lines:", len(hits))
# 打印前 40 行 ne2k 相关
for l in hits[:40]:
    print(l)
print("---- first Sending occurrence context ----")
for i, l in enumerate(lines):
    if "Sending buffer" in l:
        print("first Sending at line", i)
        for j in range(max(0, i-6), i+3):
            print(" ", lines[j])
        break
# 统计发送次数
print("Sending count:", len([l for l in lines if "Sending buffer" in l]))
