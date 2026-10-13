# -*- coding: utf-8 -*-
# S11 完整双向实测：ip对齐网段 → chatd → 主机发7777 → 内核收 → 内核发8888 → 主机收
import socket, threading, time, subprocess, sys, os, re

BASE = r"D:\WinPP\winpp-os"
PY = sys.executable

# 前置窗口
out = subprocess.run(["powershell","-NoProfile","-Command",
    "Get-Process -Name bochs -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty MainWindowHandle"],
    capture_output=True, text=True, timeout=30).stdout.strip()
h = int(out); print("handle:", h)
subprocess.run(["python", os.path.join(BASE, "_restore_h.py"), str(h)],
               capture_output=True, text=True, timeout=30)
time.sleep(1)
sk = os.path.join(BASE, "_sendkeys.py")
src = open(sk, encoding="utf-8").read()
src = re.sub(r"H = \d+", "H = %d" % h, src)
open(sk, "w", encoding="utf-8").write(src)

def inject(cmd):
    r = subprocess.run([PY, os.path.join(BASE, "_sendkeys.py"), cmd + "\n"],
                       capture_output=True, text=True, timeout=60)
    print("inject:", cmd, "rc:", r.returncode)
    time.sleep(1.5)

# 主机 UDP 监听 8888
recv_evt = threading.Event(); got = []
def listen8888():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind(("0.0.0.0", 8888)); s.settimeout(15)
    try:
        while True:
            try:
                data, addr = s.recvfrom(2048); got.append((data, addr))
                print("HOST RECV 8888:", data, "from", addr)
            except socket.timeout: break
    finally: s.close()
th = threading.Thread(target=listen8888, daemon=True); th.start()

inject("ip 10.0.2.15")
inject("chatd 7777")
time.sleep(1)

# 主机 → 内核（hostfwd 7777）
c = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
c.sendto(b"hello-from-host-1", ("127.0.0.1", 7777))
print("HOST SENT 127.0.0.1:7777")
time.sleep(3)

# 内核发 → 主机（此时 ARP 应已学到 10.0.2.2）
inject("udpsend 10.0.2.2 8888 hello4")
time.sleep(3)

# 查看内核 chatd 收件箱
inject("chatd")
time.sleep(2)
th.join(timeout=12)
print("DONE host-recv8888:", got)
