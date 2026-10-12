# -*- coding: utf-8 -*-
# 在 WinPP内核详解.html 追加 S7 章节（插到 S6 章节之后、实测证据之前），更新导航
path = r'D:\WinPP\WinPP内核详解.html'
d = open(path, encoding='utf-8').read()

s7 = u'''<!-- ================= 第七阶段：设备驱动（设备表+统一接口） ================= -->
<section class="sec" id="s7">
  <div class="wrap">
    <h2>第七阶段：设备驱动（设备表 + 统一接口）</h2>
    <p class="sub">一句话意义：键盘/定时器/串口从"散落的外设代码"收编成一张设备表，统一 read/write/ioctl/irq 四个接口，中断按设备分发——外设第一次有了正式编制。</p>

    <h3>设备表（device.c / device.h）</h3>
    <table class="tbl">
      <colgroup><col style="width:9%"><col style="width:16%"><col style="width:14%"><col style="width:12%"><col style="width:49%"></colgroup>
      <tr><th>major</th><th>名称</th><th>类型</th><th>IRQ</th><th>统一接口能力</th></tr>
      <tr><td>1</td><td><b>kbd</b></td><td>char</td><td>1</td><td>read（阻塞读一键）</td></tr>
      <tr><td>2</td><td><b>timer</b></td><td>clock</td><td>0</td><td>ioctl（DEV_IOCTL_TICKS / UPTIME）+ irq=timer_tick</td></tr>
      <tr><td>3</td><td><b>com1</b></td><td>char</td><td>4(未开)</td><td>write（串口输出）</td></tr>
    </table>

    <h3>三个子任务（全部 Bochs 实机 + 串口 + 截图取证）</h3>
    <table class="tbl">
      <colgroup><col style="width:14%"><col style="width:46%"><col style="width:40%"></colgroup>
      <tr><th>子任务</th><th>做了什么</th><th>实测铁证</th></tr>
      <tr><td>S7-1 设备框架</td><td>device_t 表（name/type/major/irqno/init/read/write/ioctl/irq）；device_register / device_get / device_by_irq / device_count；DEV_MAX=8</td><td>[dev] registered 3 devices 注册表打印正确</td></tr>
      <tr><td>S7-2 三大设备驱动化</td><td>keyboard_device()/timer_device()/serial_device() 各自暴露设备描述，kmain [3b] device_init_all 统一注册</td><td>kbd major=1 irq=1 r=1 / timer major=2 irq=0 io=1 / com1 major=3 irq=4 w=1</td></tr>
      <tr><td>S7-3 中断按设备分发</td><td>irq_handler 改查 device_by_irq(irq)->irq()；内核只留策略层（调度/状态栏）</td><td>dev 命令：kbd.read='z'、timer.ioctl ticks=2150、com1.write=43B；0 页错误</td></tr>
    </table>

    <h3>新增/修改文件</h3>
    <table class="tbl">
      <colgroup><col style="width:34%"><col style="width:66%"></colgroup>
      <tr><th>文件</th><th>职责</th></tr>
      <tr><td>src/device.c + device.h</td><td>设备表、注册/查询/中断分发、device_init_all 统一注册三个设备</td></tr>
      <tr><td>src/keyboard.c / timer.c / serial.c</td><td>各加 keyboard_device()/timer_device()/serial_device() 设备描述</td></tr>
      <tr><td>src/kernel.c</td><td>irq_handler 改设备表分发；kmain 加 [3b] device_init_all</td></tr>
      <tr><td>src/shell.c</td><td>新命令 dev：打印设备表 + 实测 read/ioctl/write</td></tr>
      <tr><td>bochsrc.txt</td><td>clock sync=none → sync=realtime（修复虚拟时钟加速 326 倍）</td></tr>
    </table>

    <h3>Bug 卡</h3>
    <table class="tbl">
      <colgroup><col style="width:22%"><col style="width:78%"></colgroup>
      <tr><th>编号</th><th>问题与修复</th></tr>
      <tr><td>BUG-S7-01</td><td>serial_printf 不支持 %- 左对齐 → 设备表打印错乱（major 变指针值）。修复：格式化跳过 '-' 标记</td></tr>
      <tr><td>BUG-S7-02</td><td>serial_printf 不支持 %c → 读键显示假象 'c'（实际读到 'z'）。修复：增加 %c 分支</td></tr>
      <tr><td>环境</td><td>Bochs sync=none 虚拟时钟加速 ~326 倍（PIT 32.6kHz、uptime 失真）。修复：sync=realtime，PIT 恢复 100Hz（串口核验 29s/2910 ticks）</td></tr>
    </table>
  </div>
</section>

'''

d = d.replace('<!-- ================= 实测证据', s7 + '<!-- ================= 实测证据', 1)
d = d.replace('<a href="#s6">第六阶段</a>', '<a href="#s6">第六阶段</a>\n    <a href="#s7">第七阶段</a>', 1)
open(path, 'w', encoding='utf-8').write(d)
print('s7 inserted:', d.find('id="s7"') > 0, '| nav:', d.find('>第七阶段<') > 0)
