# -*- coding: utf-8 -*-
d = open(r'D:\WinPP\winpp-os\serial.out', 'rb').read().decode('utf-8', 'replace')
lines = d.splitlines()
o = ['FAULT %d' % sum(1 for l in lines if 'FAULT' in l)]
for l in lines:
    if '[fs] chmod' in l or 'sec-read' in l or '[fs] open' in l or 'chmod ok' in l:
        o.append(repr(l))
open(r'D:\WinPP\winpp-os\_v8.txt', 'w', encoding='utf-8').write('\n'.join(o))
print('\n'.join(o))
