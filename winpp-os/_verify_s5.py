# -*- coding: utf-8 -*-
d = open(r'D:\WinPP\WinPP内核详解.html', encoding='utf-8').read()
print('s5-section', d.find('id="s5"') > 0)
print('nav-s5', d.find('>第五阶段<') > 0)
print('len', len(d))
