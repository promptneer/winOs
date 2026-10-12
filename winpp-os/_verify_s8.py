# -*- coding: utf-8 -*-
d = open(r'D:\WinPP\winpp-os\serial.out', 'rb').read().decode('utf-8', 'replace')
lines = d.splitlines()
o = []
o.append('FAULT %d' % sum(1 for l in lines if 'FAULT' in l))
for l in lines:
    if '[user]' in l or '[login]' in l or '[whoami]' in l or '[id]' in l \
       or '[fs11]' in l or '[fs] seed' in l or '[fs] open' in l or '[fs] read' in l \
       or '[fs] write' in l or '[fs] chdir' in l or '[ls]' in l:
        o.append(l)
open(r'D:\WinPP\winpp-os\_v8.txt', 'w', encoding='utf-8').write('\n'.join(o))
print('\n'.join(o))
