# -*- coding: utf-8 -*-
d = open(r'D:\WinPP\WinPP内核详解.html', encoding='utf-8').read()
print('s7', d.find('id="s7"') > 0)
print('nav7', d.find('>第七阶段<') > 0)
print('devtable', d.find('DEV_IOCTL_TICKS') > 0)
