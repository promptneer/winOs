# -*- coding: utf-8 -*-
# 在 WinPP内核详解.html 追加 S5 章节（插到 S4 章节之后、proof 之前），更新导航
path = r'D:\WinPP\WinPP内核详解.html'
d = open(path, encoding='utf-8').read()

s5 = u'''<!-- ================= 第五阶段：进程间通信 IPC ================= -->
<section class="sec" id="s5">
  <div class="wrap">
    <h2>第五阶段：进程间通信 IPC（管道 / 命名管道 / 消息队列）</h2>
    <p class="sub">一句话意义：第四阶段每个进程有了独立地址空间；第五阶段让进程之间能互相传数据——管道（水管流水）、命名管道（挂牌水管）、消息队列（信箱传信）。</p>

    <h3>新系统调用（int 0x80，eax 编号）</h3>
    <table class="tbl">
      <colgroup><col style="width:9%"><col style="width:22%"><col style="width:26%"><col style="width:43%"></colgroup>
      <tr><th>编号</th><th>调用</th><th>参数</th><th>说明</th></tr>
      <tr><td>9</td><td><b>pipe</b></td><td>—</td><td>创建匿名管道，返回读端 fd（写端 = fd+1）</td></tr>
      <tr><td>10</td><td><b>fifo_create</b></td><td>name</td><td>创建/复用命名管道，返回读端 fd（写端 = fd+1）</td></tr>
      <tr><td>11</td><td><b>msgsend</b></td><td>src, len</td><td>投递一条消息（≤32B）到全局队列，满则丢最旧</td></tr>
      <tr><td>12</td><td><b>msgrecv</b></td><td>dst</td><td>取出一条消息返回长度；空返回 -1</td></tr>
    </table>

    <h3>三个子任务（全部 Bochs 实机 + 串口 + 截图取证）</h3>
    <table class="tbl">
      <colgroup><col style="width:14%"><col style="width:46%"><col style="width:40%"></colgroup>
      <tr><th>子任务</th><th>做了什么</th><th>实测铁证</th></tr>
      <tr><td>S5-1 管道</td><td>ipc.c 环形缓冲 4KB；fd 槽复用（type 1/2 读/写端）；syscall 9 pipe() 返回读端 fd</td><td>read-fd=0 write-fd=1；[pid1] Hello Pipe! 循环 461 次，0 页错误</td></tr>
      <tr><td>S5-2 命名管道</td><td>名字表 8 个 FIFO（各 4KB），同名复用；syscall 10 fifo_create(name)</td><td>'fifo0' created: read-fd=2 write-fd=3；[pid1] Hello FIFO! 循环 470 次，0 页错误</td></tr>
      <tr><td>S5-3 消息队列</td><td>全局环形队列 32 条 x 32B；msgsend/msgrecv，满丢最旧、空返 -1</td><td>send len=2/5/7 -> recv 一一对应；AB/msg12/msg-789 各 375 条，0 页错误</td></tr>
    </table>

    <h3>新增/修改文件</h3>
    <table class="tbl">
      <colgroup><col style="width:34%"><col style="width:66%"></colgroup>
      <tr><th>文件</th><th>职责</th></tr>
      <tr><td>src/ipc.c + ipc.h</td><td>管道/命名管道/消息队列内核实现（环形缓冲 + 消息队列）</td></tr>
      <tr><td>src/fs.c + fs.h</td><td>fd 槽加 type/obj 字段；read/write 按类型分发到 IPC</td></tr>
      <tr><td>src/user7.S</td><td>管道演示（pipe 一次创建，循环写读回）</td></tr>
      <tr><td>src/user8.S</td><td>命名管道演示（fifo_create "fifo0"）</td></tr>
      <tr><td>src/user9.S</td><td>消息队列演示（发 3 条不同长度、收 3 条打印）</td></tr>
    </table>
  </div>
</section>

'''

# 插到 proof 章节前（S4 章节之后紧邻 proof）
d = d.replace('<!-- ================= 实测证据', s5 + '<!-- ================= 实测证据', 1)

# 导航加一项
d = d.replace('<a href="#s4">第四阶段</a>', '<a href="#s4">第四阶段</a>\n    <a href="#s5">第五阶段</a>', 1)

open(path, 'w', encoding='utf-8').write(d)
print('s5 inserted:', 'id="s5"' in d, '| nav:', '>第五阶段<' in d)
