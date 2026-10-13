# -*- coding: utf-8 -*-
# S11 主机联调：注入 slirp 网络命令
import subprocess, re, sys, time, os

BASE = r"D:\WinPP\winpp-os"
PY = sys.executable

out = subprocess.run(["powershell","-NoProfile","-Command",
    "Get-Process -Name bochs -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty MainWindowHandle"],
    capture_output=True, text=True, timeout=30).stdout.strip()
h = int(out)
print("handle:", h)

sk = os.path.join(BASE, "_sendkeys.py")
src = open(sk, encoding="utf-8").read()
src = re.sub(r"H = \d+", "H = %d" % h, src)
open(sk, "w", encoding="utf-8").write(src)

cmds = [
    "ip 10.0.2.15",
    "chatd 7777",
    "udpsend 10.0.2.2 8888 hello",
    "udpsend 10.0.2.2 8888 hello2",
]
for c in cmds:
    r = subprocess.run([PY, os.path.join(BASE, "_sendkeys.py"), c + "\n"],
                       capture_output=True, text=True, timeout=60)
    print("cmd:", c, "| rc:", r.returncode)
    time.sleep(2.0)
print("DONE")
