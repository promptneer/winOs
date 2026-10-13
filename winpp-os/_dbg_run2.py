# -*- coding: utf-8 -*-
# bochsdbg 交互定位 fault（健壮版：非阻塞读，10 秒总时限）
import subprocess, time, sys, os, threading

cfg = r"D:\WinPP\winpp-os\bochsrc.txt"
p = subprocess.Popen(
    [r"C:\Bochs-2.8\bochsdbg.exe", "-q", "-f", cfg],
    stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
    text=True, bufsize=1, encoding="utf-8", errors="replace"
)

out = []
stop = threading.Event()
def reader():
    while not stop.is_set():
        line = p.stdout.readline()
        if not line:
            break
        out.append(line.rstrip())
        if len(out) > 500:
            out.pop(0)
t = threading.Thread(target=reader, daemon=True)
t.start()

def cmd(s, wait=0.3):
    try:
        p.stdin.write(s + "\n"); p.stdin.flush()
    except Exception:
        pass
    time.sleep(wait)

try:
    cmd("c", 1.0)      # 第一段：BIOS 引导
    cmd("c", 8.0)      # 第二段：跑内核，若 fault 会停
    cmd("info cpu", 0.5)
    cmd("regs", 0.5)
    time.sleep(1.5)
finally:
    try:
        cmd("q", 0.2)
    except Exception:
        pass
    stop.set()
    time.sleep(0.5)
    try:
        p.kill()
    except Exception:
        pass

# 打印关键行：fault/exception/reset/registers/EIP/CR3
keys = ["fault", "exception", "reset", "EIP", "CR0", "CR3", "CPU0", "Halt", "interrupt"]
tail = out[-200:]
for l in tail:
    low = l.lower()
    if any(k.lower() in low for k in keys) or "<bochs" in low or "next" in low:
        print(l)
print("===TAIL===")
print("\n".join(tail[-30:]))
