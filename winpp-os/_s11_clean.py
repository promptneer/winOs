# -*- coding: utf-8 -*-
# S11 干净双向实测：重启 Bochs → ip → chatd → 主机发7777 → 内核收 → 内核发8888 → 主机收
import socket, threading, time, subprocess, sys, os, re, io

BASE = r"D:\WinPP\winpp-os"
PY = sys.executable

# 0) 干净重启
subprocess.run(["powershell","-NoProfile","-Command",
    "Get-Process -Name 'bochs*' -ErrorAction SilentlyContinue | Stop-Process -Force"],
    capture_output=True, text=True, timeout=30)
time.sleep(1)
for f in ["serial.out","bochs.log","slirp-pktlog.txt"]:
    p = os.path.join(BASE, f)
    if os.path.exists(p): os.remove(p)
subprocess.Popen(["C:\\Bochs-2.8\\bochs.exe","-q","-f",os.path.join(BASE,"bochsrc.txt")])
print("bochs started, waiting boot...")
time.sleep(12)

# 1) 前置窗口 + 更新 sendkeys
out = subprocess.run(["powershell","-NoProfile","-Command",
    "Get-Process -Name bochs -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty MainWindowHandle"],
    capture_output=True, text=True, timeout=30).stdout.strip()
h = int(out); print("handle:", h)
subprocess.run([PY, os.path.join(BASE, "_restore_h.py"), str(h)], capture_output=True, text=True, timeout=30)
time.sleep(1)
sk = os.path.join(BASE, "_sendkeys.py")
src = open(sk, encoding="utf-8").read()
src = re.sub(r"H = \d+", "H = %d" % h, src)
open(sk, "w", encoding="utf-8").write(src)

def inject(cmd, wait=2.0):
    r = subprocess.run([PY, os.path.join(BASE, "_sendkeys.py"), cmd + "\n"],
                       capture_output=True, text=True, timeout=60)
    print("inject:", cmd, "rc:", r.returncode)
    time.sleep(wait)

def serial():
    return io.open(os.path.join(BASE,"serial.out"), encoding="utf-8", errors="replace").read()

# 2) 主机 UDP 监听 8888
got = []
def listen8888():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind(("0.0.0.0", 8888)); s.settimeout(14)
    try:
        while True:
            try:
                data, addr = s.recvfrom(2048); got.append((data, addr)); print("HOST RECV 8888:", data)
            except socket.timeout: break
    finally: s.close()
th = threading.Thread(target=listen8888, daemon=True); th.start()

# 3) 注入 ip + chatd
inject("ip 10.0.2.15", 2.0)
inject("chatd 7777", 2.0)
print("--- serial after ip+chatd ---")
s = serial(); print(s[-1500:])

# 4) 主机 → 内核 7777
c = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
c.sendto(b"hello-from-host-1", ("127.0.0.1", 7777))
print("HOST SENT 127.0.0.1:7777")
time.sleep(4)

# 5) 内核查看 chatd
inject("chatd", 2.0)
print("--- serial after host->guest ---")
s = serial(); print(s[-1800:])

# 6) 内核 → 主机 8888
inject("udpsend 10.0.2.2 8888 hello4", 3.0)
th.join(timeout=12)
print("DONE host-recv8888:", got)
