# -*- coding: utf-8 -*-
# 第四阶段全量回归：一次性注入 13 项测试序列（GUI 键盘注入）
import subprocess, sys, time

SEND = r'D:\WinPP\winpp-os\_sendkeys.py'

def key(cmd, wait):
    r = subprocess.run([sys.executable, SEND, cmd], capture_output=True)
    t = (r.stdout or b'').decode('utf-8', 'replace').strip()
    print('[%s] %s' % (cmd, t[-40:] if t else 'no-out'))
    subprocess.run([sys.executable, SEND, 'ENTER'], capture_output=True)
    time.sleep(wait)

seq = [
    ('run',    4.5),   # T-03 独立地址空间
    ('ps',     2.0),   # T-04
    ('stop',   2.0),   # T-05
    ('fork',   4.5),   # T-06
    ('ps',     2.0),   # T-07
    ('stop',   2.0),
    ('exec',   4.5),   # T-08
    ('ps',     2.0),   # T-09
    ('stop',   2.0),
    ('fs',     4.5),   # T-10
    ('fslist', 2.0),   # T-11
    ('kill 1', 2.0),   # T-12
    ('ps',     2.0),
    ('stop',   2.0),   # T-13
]
for c, w in seq:
    key(c, w)
print('ALL INJECTED')
