# -*- coding: utf-8 -*-
# 证明 fs 写入功能：mq -> stop -> fs -> fslist + 抓屏
import subprocess, sys, time

SEND = r'D:\WinPP\winpp-os\_sendkeys.py'
GRAB = r'D:\WinPP\winpp-os\_grab_any.py'
SHOTS = r'D:\WinPP\winpp-os\shots'
H = open(r'D:\WinPP\winpp-os\_dbg.txt').read().strip()

def key(cmd, wait):
    subprocess.run([sys.executable, SEND, cmd], capture_output=True)
    subprocess.run([sys.executable, SEND, 'ENTER'], capture_output=True)
    time.sleep(wait)

key('mq', 2.0)
key('fs', 1.0)          # 调度器还跑着，应被拒绝：scheduler already running
key('stop', 2.0)        # 停掉 mq 演示
key('fs', 3.0)          # 现在应能启动 fs 演示（user6 写 hello.txt）
key('fslist', 2.0)      # 应列出 hello.txt
p = subprocess.run([sys.executable, GRAB, H, SHOTS + '\\s5-fs-prove.png'], capture_output=True)
print('grab', (p.stdout or b'').decode('utf-8', 'replace').strip()[-40:])
print('ALL DONE')
