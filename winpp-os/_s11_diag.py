# -*- coding: utf-8 -*-
import subprocess, os, io
# Bochs 进程
out = subprocess.run(["powershell","-NoProfile","-Command",
  "Get-Process -Name 'bochs*' -ErrorAction SilentlyContinue | Select-Object Id,StartTime,MainWindowTitle | Format-List | Out-String"],
  capture_output=True, text=True, timeout=30)
print("PROC:", out.stdout.strip() or "(no bochs process)")
print("ERR:", out.stderr.strip()[:300])
# bochs.log 尾部
p = r"D:\WinPP\winpp-os\bochs.log"
if os.path.exists(p):
    s = io.open(p, encoding="utf-8", errors="replace").read()
    print("LOG TAIL:", s[-1200:])
# serial 是否有 PANIC / 异常
sp = r"D:\WinPP\winpp-os\serial.out"
if os.path.exists(sp):
    ss = io.open(sp, encoding="utf-8", errors="replace").read()
    print("serial len:", len(ss))
    for kw in ["PANIC","exception","page fault","PAGE FAULT"]:
        i = ss.find(kw)
        if i >= 0:
            print("SERIAL HIT", kw, ":", ss[max(0,i-100):i+300])
