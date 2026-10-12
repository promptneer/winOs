# -*- coding: utf-8 -*-
# 在 WinPP内核详解.html 的 proof 章节前插入第四阶段章节，并更新导航
import re

path = r'D:\WinPP\WinPP内核详解.html'
d = open(path, encoding='utf-8').read()

s4 = u'''<!-- ================= 第四阶段：独立地址空间 + fork/exec + 文件系统 ================= -->
<section class="sec" id="s4">
  <div class="wrap">
    <h2>第四阶段：独立地址空间 + fork / exec + 用户态文件系统</h2>
    <p class="sub">一句话意义：第三阶段是"多个程序能同时跑"；第四阶段是"每个程序有自己的天地（独立地址空间），能繁衍（fork）、能变身（exec）、能读写数据（文件系统）"——到这一步，内核就是真正的"迷你 Linux"。</p>

    <h3>新系统调用（int 0x80，eax 编号）</h3>
    <table class="tbl">
      <colgroup><col style="width:9%"><col style="width:22%"><col style="width:26%"><col style="width:43%"></colgroup>
      <tr><th>编号</th><th>调用</th><th>参数</th><th>说明</th></tr>
      <tr><td>3</td><td><b>fork</b></td><td>—</td><td>复制当前进程为子进程：父进程返回子 pid，子进程返回 0</td></tr>
      <tr><td>4</td><td><b>exec</b></td><td>prog_idx</td><td>同一进程换成另一个程序（内嵌 userN.bin），pid 与地址空间不变</td></tr>
      <tr><td>5</td><td><b>open</b></td><td>name, flags</td><td>打开/创建内存盘文件，返回 fd（flags: 0=读, 1=创建/写）</td></tr>
      <tr><td>6</td><td><b>write</b></td><td>fd, buf, len</td><td>向文件写数据（落入内核内存盘，用户态无法直接触碰）</td></tr>
      <tr><td>7</td><td><b>read</b></td><td>fd, buf, len</td><td>从文件读数据（内核拷回用户内存）</td></tr>
      <tr><td>8</td><td><b>close</b></td><td>fd</td><td>关闭文件，重置偏移</td></tr>
    </table>

    <h3>四个子任务（全部 Bochs 实机 + 串口 + 截图取证）</h3>
    <table class="tbl">
      <colgroup><col style="width:14%"><col style="width:46%"><col style="width:40%"></colgroup>
      <tr><th>子任务</th><th>做了什么</th><th>实测铁证</th></tr>
      <tr><td>S4-1 独立地址空间</td><td>每个用户进程独立页目录 PD + PT1，调度切换换 CR3；新 PD 复制内核恒等映射 + 自映射指向自身；用户态访问其他进程/内核地址空间即 #PF</td><td>3 进程 pd=124000/129000/12e000；切换日志 pd 逐个变化（含回 shell 的 110000）；0 页错误，294 条计数</td></tr>
      <tr><td>S4-2 fork</td><td>sched_fork：复制 pcb、内核栈、PD/PT1、用户代码页/栈页内容；子进程从 fork 调用点返回 eax=0</td><td>[fork] pid 1 -> child pid 2；父打印 parent、子打印 child 独立计数，286 条输出 0 页错误</td></tr>
      <tr><td>S4-3 exec</td><td>sched_exec：解除旧代码/栈映射，拷入目标程序，改入口与栈地址，同一 pid 变身</td><td>[pid1] user5: before exec... -> [exec] pid1 -> user2 -> [pid1] user2: 计数（pd 不变）</td></tr>
      <tr><td>S4-4 文件系统</td><td>fs.c 内存盘：16 文件槽 x 4KB；open/read/write/close 系统调用；数据区在内核内存</td><td>user6 创建 hello.txt、写 "Hello WinPP FS!"、关闭重开读回、串口打印，循环 208 次 0 页错误</td></tr>
    </table>

    <h3>新增文件</h3>
    <table class="tbl">
      <colgroup><col style="width:34%"><col style="width:66%"></colgroup>
      <tr><th>文件</th><th>职责</th></tr>
      <tr><td>src/fs.c + fs.h</td><td>内存盘文件系统：文件表、open/read/write/close、fs_list</td></tr>
      <tr><td>src/user4.S</td><td>fork 演示程序（fork 后父子各自计数）</td></tr>
      <tr><td>src/user5.S</td><td>exec 演示程序（打印后 exec 变身成 user2）</td></tr>
      <tr><td>src/user6.S</td><td>文件系统演示（创建/写/读 hello.txt）</td></tr>
    </table>
  </div>
</section>

'''

# 在 proof 章节前插入
d = d.replace('<!-- ================= 实测证据', s4 + '<!-- ================= 实测证据', 1)

# 导航加一项
d = d.replace('<a href="#proof">实测证据</a>', '<a href="#s4">第四阶段</a>\n    <a href="#proof">实测证据</a>', 1)

open(path, 'w', encoding='utf-8').write(d)
print('inserted at', d.find('id="s4"'))
