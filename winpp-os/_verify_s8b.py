# -*- coding: utf-8 -*-
d = open(r'D:\WinPP\winpp-os\serial.out', 'rb').read().decode('utf-8', 'replace')
lines = d.splitlines()
keys = ['[user]', '[fs] chdir', '[fs] chmod', 'sec-read', 'EACCES', 'uid=', '[login]', '[whoami]']
o = ['FAULT %d' % sum(1 for l in lines if 'FAULT' in l)]
for l in lines:
    for k in keys:
        if k in l:
            o.append(repr(l))
            break
open(r'D:\WinPP\winpp-os\_v8.txt', 'w', encoding='utf-8').write('\n'.join(o))
print('\n'.join(o))
