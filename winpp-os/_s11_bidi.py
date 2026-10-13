# -*- coding: utf-8 -*-
# 主机↔内核 双向 UDP 实测
import socket, threading, time, subprocess, sys, os, re

BASE = r"D:\WinPP\winpp-os"
PY = sys.executable

# 1) 主机 UDP 监听 8888（接收内核 udpsend）
recv_evt = threading.Event()
got = []
def listen8888():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind(("0.0.0.0", 8888))
    s.settimeout(15)
    try:
        while True:
            try:
                data, addr = s.recvfrom(2048)
                got.append((data, addr))
                print("HOST RECV 8888:", data, "from", addr)
            except socket.timeout:
                break
    finally:
        s.close()
th = threading.Thread(target=listen8888, daemon=True); th.start()

# 2) 注入：内核再发 udpsend（此时 ARP 表仍空，观察行为）
out = subprocess.run(["powershell","-NoProfile","-Command",
    "Get-Process -Name bochs -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty MainWindowHandle"],
    capture_output=True, text=True, timeout=30).stdout.strip()
h = int(out); print("handle:", h)
sk = os.path.join(BASE, "_sendkeys.py")
src = open(sk, encoding="utf-8").read()
src = re.sub(r"H = \d+", "H = %d" % h, src)
open(sk, "w", encoding="utf-8").write(src)

def inject(cmd):
    r = subprocess.run([PY, os.path.join(BASE, "_sendkeys.py"), cmd + "\n"],
                       capture_output=True, text=True, timeout=60)
    print("inject:", cmd, "rc:", r.returncode)

inject("udpsend 10.0.2.2 8888 hello3")
time.sleep(3)

# 3) 主机 → 内核：UDP 到 127.0.0.1:7777（slirp hostfwd → 内核 chatd 7777）
c = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
c.sendto(b"hello-from-host!", ("127.0.0.1", 7777))
print("HOST SENT to 127.0.0.1:7777")
time.sleep(1)

# 4) 内核 chatd 查看收件箱
inject("chatd")
time.sleep(2)

th.join(timeout=18)
print("DONE host-recv:", got)
