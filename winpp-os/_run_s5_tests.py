# -*- coding: utf-8 -*-
# S5 全量实测：pipe -> stop -> fifo -> stop -> mq -> stop + 关键节点抓屏
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

key('pipe', 4.5)
key('ps', 1.5);   grab('s5-01-ps-pipe.png')
key('stop', 2.0)
key('fifo', 4.5)
key('ps', 1.5);   grab('s5-02-ps-fifo.png')
key('stop', 2.0)
key('mq', 4.5)
key('ps', 1.5);   grab('s5-03-ps-mq.png')
key('stop', 2.0)
print('ALL DONE')
