# -*- coding: utf-8 -*-
d = open(r'D:\WinPP\winpp-os\serial.out', 'rb').read().decode('utf-8', 'replace')
lines = d.splitlines()
o = []
o.append('PAGE_FAULT %d' % sum(1 for l in lines if 'FAULT' in l))
o.append('--- mkdir ---')
for l in lines:
    if '[fs] mkdir' in l: o.append(l)
o.append('--- chdir ---')
for l in lines:
    if '[fs] chdir' in l: o.append(l)
o.append('--- open(前4) ---')
n = 0
for l in lines:
    if '[fs] open' in l:
        o.append(l); n += 1
        if n >= 4: break
o.append('--- ls 段 ---')
for l in lines:
    if '[ls]' in l: o.append(l)
o.append('--- fs2 ---')
for l in lines:
    if '[fs2]' in l: o.append(l)
o.append('--- SubFile 计数 ---')
o.append('subfile %d' % sum(1 for l in lines if 'Sub File' in l))
o.append('--- fslist ---')
for l in lines:
    if '[fs] list' in l: o.append(l)
o.append('--- stop ---')
for l in lines:
    if '[stop]' in l: o.append(l)
open(r'D:\WinPP\winpp-os\_dbg.txt', 'w', encoding='utf-8').write('\n'.join(o))
print('\n'.join(o))
