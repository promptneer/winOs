# -*- coding: utf-8 -*-
# bochsdbg 交互定位 fault：跑 20 秒，若停在异常/重置则打印 CPU 状态
import subprocess, time, sys, os, re

cfg = r"D:\WinPP\winpp-os\bochsrc.txt"
p = subprocess.Popen(
    [r"C:\Bochs-2.8\bochsdbg.exe", "-q", "-f", cfg],
    stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
    text=True, bufsize=1, encoding="utf-8", errors="replace"
)

def cmd(s, wait=0.4):
    try:
        p.stdin.write(s + "\n"); p.stdin.flush()
    except Exception as e:
        print("WRITE FAIL:", e)
    time.sleep(wait)

out = []
def drain(t=1.0):
    import select
    end = time.time() + t
    while time.time() < end:
        try:
            line = p.stdout.readline()
            if not line: break
            out.append(line.rstrip())
            if len(out) > 300: out.pop(0)
        except Exception:
            break

# 让 BIOS 引导跑起来
cmd("c", 1.0)
drain(2.0)
cmd("c", 6.0)
drain(3.0)
# 看当前状态
cmd("info cpu", 0.8)
cmd("regs", 0.8)
drain(2.0)
cmd("q", 0.5)
drain(1.0)
try:
    p.kill()
except Exception:
    pass

# 输出最后 120 行
print("\n".join(out[-120:]))
