# -*- coding: utf-8 -*-
# 第四阶段干净全量回归 v2：注入 + subprocess 抓屏
import subprocess, sys, time

SEND = r'D:\WinPP\winpp-os\_sendkeys.py'
GRAB = r'D:\WinPP\winpp-os\_grab_any.py'
SHOTS = r'D:\WinPP\winpp-os\shots'
H = open(r'D:\WinPP\winpp-os\_dbg.txt').read().strip()

def key(cmd, wait):
    subprocess.run([sys.executable, SEND, cmd], capture_output=True)
    subprocess.run([sys.executable, SEND, 'ENTER'], capture_output=True)
    time.sleep(wait)

def grab(name):
    p = subprocess.run([sys.executable, GRAB, H, SHOTS + '\\' + name], capture_output=True)
    print('grab', name, (p.stdout or b'').decode('utf-8', 'replace').strip()[-40:])

# 清掉任何残留
for _ in range(2):
    subprocess.run([sys.executable, SEND, 'ENTER'], capture_output=True)
    time.sleep(0.8)

key('run', 4.5)
key('ps', 1.5);   grab('r4-04-ps-run.png')
key('stop', 2.0)
key('fork', 4.5)
key('ps', 1.5);   grab('r4-07-ps-fork.png')
key('stop', 2.0)
key('exec', 4.5)
key('ps', 1.5);   grab('r4-09-ps-exec.png')
key('stop', 2.0)
key('fs', 4.5)
key('fslist', 1.5); grab('r4-11-fslist.png')
key('kill 1', 2.0)
key('ps', 1.5);   grab('r4-12-ps-kill.png')
key('stop', 2.0)
print('ALL DONE')
