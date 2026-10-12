# -*- coding: utf-8 -*-
d = open(r'D:\WinPP\WinPP内核路线图.html', encoding='utf-8').read()
print('S7done:', 'node done' in d[:d.find('S8')])
print('S9now:', 'S9 · 现在' in d)
print('sub ok:', 'S1–S8 全部完成' in d)
