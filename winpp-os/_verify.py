# -*- coding: utf-8 -*-
import json
d = open(r'D:\WinPP\WinPP内核详解.html', encoding='utf-8').read()
print('len', len(d))
print('s4', 'id="s4"' in d)
print('nav', '>第四阶段<' in d)
j = json.load(open(r'D:\WinPP\winpp-os\_shot6.json', encoding='utf-8-sig'))
print('consoleErrors', j.get('consoleErrors'))
print('horizontalOverflow', j.get('horizontalOverflow'))
print('overlap', len(j.get('overlappingText') or []))
