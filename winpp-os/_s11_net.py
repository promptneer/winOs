# -*- coding: utf-8 -*-
import subprocess, sys, os, re, time, io
BASE = r"D:\WinPP\winpp-os"
out = subprocess.run(["powershell","-NoProfile","-Command",
    "Get-Process -Name bochs -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty MainWindowHandle"],
    capture_output=True, text=True, timeout=30).stdout.strip()
h = int(out); print("handle", h)
subprocess.run([sys.executable, os.path.join(BASE, "_restore_h.py"), str(h)], capture_output=True, text=True, timeout=30)
time.sleep(1)
sk = os.path.join(BASE, "_sendkeys.py")
src = open(sk, encoding="utf-8").read()
src = re.sub(r"H = \d+", "H = %d" % h, src)
open(sk, "w", encoding="utf-8").write(src)
subprocess.run([sys.executable, os.path.join(BASE, "_sendkeys.py"), "net\n"], capture_output=True, text=True, timeout=60)
time.sleep(2)
s = io.open(os.path.join(BASE, "serial.out"), encoding="utf-8", errors="replace").read()
print(s[-1500:])
