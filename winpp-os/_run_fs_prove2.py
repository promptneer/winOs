# -*- coding: utf-8 -*-
import subprocess, sys, time
SEND = r'D:\WinPP\winpp-os\_sendkeys.py'
GRAB = r'D:\WinPP\winpp-os\_grab_any.py'
SHOTS = r'D:\WinPP\winpp-os\shots'
H = open(r'D:\WinPP\winpp-os\_dbg.txt').read().strip()

def key(cmd, wait):
    subprocess.run([sys.executable, SEND, cmd], capture_output=True)
    time.sleep(0.3)
    subprocess.run([sys.executable, SEND, 'ENTER'], capture_output=True)
    time.sleep(wait)

key('stop', 2.5)
key('fs', 3.0)
key('fslist', 2.0)
key('ps', 1.5)
p = subprocess.run([sys.executable, GRAB, H, SHOTS + '\\s5-fs-prove2.png'], capture_output=True)
print('grab', (p.stdout or b'').decode('utf-8', 'replace').strip()[-40:])
print('DONE')
