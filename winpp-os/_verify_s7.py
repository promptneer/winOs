# -*- coding: utf-8 -*-
d = open(r'D:\WinPP\winpp-os\serial.out', 'rb').read().decode('utf-8', 'replace')
lines = d.splitlines()
o = []
o.append('FAULT %d' % sum(1 for l in lines if 'FAULT' in l))
for l in lines:
    if '[dev]' in l or 'uptime:' in l or '[push' in l or '[sc ' in l or '[timer]' in l:
        o.append(l)
open(r'D:\WinPP\winpp-os\_v7.txt', 'w', encoding='utf-8').write('\n'.join(o))
print('\n'.join(o))
