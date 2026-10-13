# -*- coding: utf-8 -*-
# S11 自动化实测：查 Bochs 句柄 → 更新 _sendkeys → 逐条注入网络命令
import subprocess, re, sys, time, os

BASE = r"D:\WinPP\winpp-os"
PY = sys.executable

# 1. 查 Bochs 窗口句柄
out = subprocess.run(["powershell","-NoProfile","-Command",
    "Get-Process -Name bochs -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty MainWindowHandle"],
    capture_output=True, text=True, timeout=30).stdout.strip()
print("handle:", out)
h = int(out)

# 2. 更新 _sendkeys.py 的 H
sk = os.path.join(BASE, "_sendkeys.py")
src = open(sk, encoding="utf-8").read()
src = re.sub(r"H = \d+", "H = %d" % h, src)
open(sk, "w", encoding="utf-8").write(src)
print("updated H =", h)

# 3. 逐条注入
cmds = [
    "net",
    "ip 192.168.0.10",
    "udptest",
    "ping 192.168.0.10",
    "chatd 7777",
    "udpsend 192.168.0.10 7777 hello",
    "udpsend 192.168.0.10 7777 world",
    "chatd",
    "netstat",
    "arp",
]
for c in cmds:
    r = subprocess.run([PY, os.path.join(BASE, "_sendkeys.py"), c + "\n"],
                       capture_output=True, text=True, timeout=60)
    print("cmd:", c, "| rc:", r.returncode, "|", (r.stdout or "")[-50:].strip())
    time.sleep(1.6)
print("DONE")
