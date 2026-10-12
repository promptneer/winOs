# -*- coding: utf-8 -*-
# 内核详解追加 S9 章节 + 路线图 S9 done / S10 now
path = r'D:\WinPP\WinPP内核详解.html'
d = open(path, encoding='utf-8').read()

s9 = u'''<!-- ================= 第九阶段：信号与同步 ================= -->
<section class="sec" id="s9">
  <div class="wrap">
    <h2>第九阶段：信号与同步（sleep / 信号量 / 互斥锁）</h2>
    <p class="sub">一句话意义：程序学会"排队和睡觉"——sleep 主动让出 CPU，信号量/互斥锁保证多个进程抢资源时一个进、一个等。16 次互斥进出零违规。</p>

    <h3>进程状态扩展（sched.h/sched.c）</h3>
    <table class="tbl">
      <colgroup><col style="width:22%"><col style="width:78%"></colgroup>
      <tr><th>项</th><th>说明</th></tr>
      <tr><td>PROC_SLEEPING</td><td>第 4 种状态：睡眠/等待信号量中，不参与调度</td></tr>
      <tr><td>pcb.wake_tick</td><td>sleep 唤醒时刻（timer_ticks 值）</td></tr>
      <tr><td>pcb.blk_sem</td><td>阻塞等待的信号量 id；-1 = 纯 sleep</td></tr>
      <tr><td>sched_wakeup_scan()</td><td>每 tick 由 irq_handler 调用，到期 sleep 进程置 READY</td></tr>
      <tr><td>sched_wake_by_sem()</td><td>V 操作调用：唤醒队首等待者（置 READY + syscall 返回 0）</td></tr>
      <tr><td>sched_yield_now()</td><td>阻塞 syscall 内立即让出（保存帧 + 切换，不返回）</td></tr>
    </table>

    <h3>信号量实现（sem.h/sem.c）</h3>
    <table class="tbl">
      <colgroup><col style="width:24%"><col style="width:76%"></colgroup>
      <tr><th>元素</th><th>说明</th></tr>
      <tr><td>sem_t</td><td>id / value / inuse / waiters 等待 pid 队列（FIFO）/ nwait；8 槽</td></tr>
      <tr><td>sem_wait（P）</td><td>value&gt;0 减一返回；否则入队 + PROC_SLEEPING + 立即让出</td></tr>
      <tr><td>sem_signal（V）</td><td>无等待者 value++；有等待者"直接交接"（handoff）：不增 v，唤醒队首并让其返回 0</td></tr>
      <tr><td>互斥锁</td><td>mutex_create = sem_create(1)；lock/unlock = wait/signal（二值信号量）</td></tr>
    </table>

    <h3>系统调用（总数 0-23 共 24 个）</h3>
    <table class="tbl">
      <colgroup><col style="width:26%"><col style="width:74%"></colgroup>
      <tr><th>调用</th><th>语义</th></tr>
      <tr><td>17 sleep(ticks)</td><td>仅用户进程：wake_tick=now+ticks，SLEEPING + 让出；到期唤醒</td></tr>
      <tr><td>18 sem_create(value)</td><td>创建信号量 → id（8 槽）</td></tr>
      <tr><td>19 sem_wait(id)</td><td>P：拿通行证或阻塞排队</td></tr>
      <tr><td>20 sem_signal(id)</td><td>V：放回通行证 / 直接交接给队首</td></tr>
      <tr><td>21-23 mutex_*</td><td>互斥锁三件套（= 二值信号量封装）</td></tr>
    </table>

    <h3>实测铁证（Bochs + 串口 + 截图）</h3>
    <table class="tbl">
      <colgroup><col style="width:34%"><col style="width:66%"></colgroup>
      <tr><th>项目</th><th>结果</th></tr>
      <tr><td>sleep 让出</td><td>C1 sleep 60（tick 1704→1764 准时醒），期间 C2 独占 3 轮；ps 显示 sleep 状态</td></tr>
      <tr><td>信号量阻塞</td><td>[sem] wait sem0 pid2 block (q=1) → C1 排队等锁</td></tr>
      <tr><td>handoff 交接</td><td>[sem] signal sem0 pid3 handoff wake pid2 → 锁直接交给 C1</td></tr>
      <tr><td>互斥零违规</td><td>C1/C2 各 8 轮 IN/OUT 全配对，16 次进出无重叠（脚本校验 violations=0）</td></tr>
      <tr><td>稳定性</td><td>PAGE_FAULT=0</td></tr>
    </table>

    <h3>新增/修改文件</h3>
    <table class="tbl">
      <colgroup><col style="width:34%"><col style="width:66%"></colgroup>
      <tr><th>文件</th><th>职责</th></tr>
      <tr><td>src/sem.h + sem.c（新）</td><td>信号量/互斥锁实现（8 槽 FIFO 等待队列）</td></tr>
      <tr><td>src/sched.h + sched.c</td><td>PROC_SLEEPING、wake_tick/blk_sem、唤醒扫描、sched_yield_now、syscall 17-23</td></tr>
      <tr><td>src/user12.S + user12.ld（新）</td><td>同步演示：mutex + fork 双子 + 互斥循环 + sleep（274B，入口 0x690000）</td></tr>
      <tr><td>src/shell.c</td><td>syncdemo 命令</td></tr>
      <tr><td>src/kernel.c</td><td>sem_init（[11c]）、irq_handler 每 tick 唤醒扫描</td></tr>
      <tr><td>build_flat.ps1</td><td>user12 加入构建</td></tr>
    </table>

    <h3>设计修正</h3>
    <table class="tbl">
      <colgroup><col style="width:22%"><col style="width:78%"></colgroup>
      <tr><th>编号</th><th>问题与修复</th></tr>
      <tr><td>修正 1</td><td>V 操作先 value++ 再唤醒：第三方抢先 take，被唤醒者误判拿到锁 → 双进程进临界区（实测 1 次违规）。修复：有等待者时直接交接（handoff），不增 v</td></tr>
    </table>
  </div>
</section>

'''

d = d.replace('<!-- ================= 实测证据', s9 + '<!-- ================= 实测证据', 1)
d = d.replace('<a href="#s8">第八阶段</a>', '<a href="#s8">第八阶段</a>\n    <a href="#s9">第九阶段</a>', 1)
open(path, 'w', encoding='utf-8').write(d)

# ---------- 路线图：S9 done、S10 now ----------
rp = r'D:\WinPP\WinPP内核路线图.html'
r = open(rp, encoding='utf-8').read()
r = r.replace('S1–S8 全部完成并实机取证', 'S1–S9 全部完成并实机取证')
r = r.replace('下一战场 <b>S9 信号与同步</b>（sleep/wait/信号量/互斥锁）。GUI 为 S10、网络为 S11。',
              '下一战场 <b>S10 图形界面窗口系统</b>（呼应最初"干净窗口"的愿望）。网络为 S11。')
r = r.replace('<div class="node done">\n        <div class="sn">S8</div>\n        <div class="nm">用户与权限</div>\n        <div class="ds">root/UID · 文件属主 · rwx 权限位 · 提示符 #/$</div>\n        <div class="st">已完成</div>\n      </div>\n      <div class="arrow">→</div>\n      <div class="node now">\n        <div class="sn">S9 · 现在</div>\n        <div class="nm">信号与同步</div>\n        <div class="ds">sleep 睡眠 · wait 等待 · 信号量 · 互斥锁</div>\n        <div class="st">下一步 · 我们在这里</div>\n      </div>',
              '<div class="node done">\n        <div class="sn">S8</div>\n        <div class="nm">用户与权限</div>\n        <div class="ds">root/UID · 文件属主 · rwx 权限位 · 提示符 #/$</div>\n        <div class="st">已完成</div>\n      </div>\n      <div class="arrow">→</div>\n      <div class="node done">\n        <div class="sn">S9</div>\n        <div class="nm">信号与同步</div>\n        <div class="ds">sleep 睡眠 · wait 等待 · 信号量 · 互斥锁</div>\n        <div class="st">已完成</div>\n      </div>')
r = r.replace('<div class="node plan">\n        <div class="sn">S10</div>\n        <div class="nm">图形界面窗口系统</div>\n        <div class="ds">呼应最初"干净窗口"的愿望 · GUI 登录</div>\n        <div class="st">规划</div>\n      </div>',
              '<div class="node now">\n        <div class="sn">S10 · 现在</div>\n        <div class="nm">图形界面窗口系统</div>\n        <div class="ds">呼应最初"干净窗口"的愿望 · GUI 登录</div>\n        <div class="st">下一步 · 我们在这里</div>\n      </div>')
open(rp, 'w', encoding='utf-8').write(r)
print('s9 inserted:', d.find('id="s9"') > 0, '| nav:', d.find('>第九阶段<') > 0)
print('roadmap S9done:', 'node done">\n        <div class="sn">S9' in r, '| S10now:', 'S10 · 现在' in r)
