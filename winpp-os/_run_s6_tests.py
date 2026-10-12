# -*- coding: utf-8 -*-
# S6 全量实测：mkdir -> ls -> cd -> pwd -> 相对mkdir -> cd / -> fs2 演示 -> ps/fslist/ls -> stop
import subprocess, sys, time

SEND = r'D:\WinPP\winpp-os\_sendkeys.py'
GRAB = r'D:\WinPP\winpp-os\_grab_any.py'
SHOTS = r'D:\WinPP\winpp-os\shots'
H = open(r'D:\WinPP\winpp-os\_dbg.txt').read().strip()

def key(cmd, wait):
    subprocess.run([sys.executable, SEND, cmd], capture_output=True)
    subprocess.run([sys.executable, SEND, 'ENTER'], capture_output=True)
    time.sleep(wait)

key('mkdir /docs', 1.5)
key('ls', 1.2)
key('cd /docs', 1.2)
key('pwd', 1.2)
key('mkdir docs2', 1.2)
key('ls', 1.2)
key('cd /', 1.2)
p = subprocess.run([sys.executable, GRAB, H, SHOTS + '\\s6-01-dirs.png'], capture_output=True)
print('grab1', (p.stdout or b'').decode('utf-8', 'replace').strip()[-30:])
key('fs2', 4.5)
key('ps', 1.5);   p = subprocess.run([sys.executable, GRAB, H, SHOTS + '\\s6-02-ps-fs2.png'], capture_output=True)
print('grab2', (p.stdout or b'').decode('utf-8', 'replace').strip()[-30:])
key('fslist', 1.5)
key('ls /docs', 1.5)
key('stop', 2.0)
p = subprocess.run([sys.executable, GRAB, H, SHOTS + '\\s6-03-final.png'], capture_output=True)
print('grab3', (p.stdout or b'').decode('utf-8', 'replace').strip()[-30:])
print('ALL DONE')
