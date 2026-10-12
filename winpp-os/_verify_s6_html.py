# -*- coding: utf-8 -*-
d = open(r'D:\WinPP\WinPP内核详解.html', encoding='utf-8').read()
print('s6', d.find('id="s6"') > 0)
print('nav6', d.find('>第六阶段<') > 0)
print('mkdir', d.find('mkdir') > 0)
