# -*- coding: utf-8 -*-
# 在 WinPP内核详解.html 追加 S8 章节 + 更新路线图 HTML（S8 done、S9 now）
path = r'D:\WinPP\WinPP内核详解.html'
d = open(path, encoding='utf-8').read()

s8 = u'''<!-- ================= 第八阶段：用户与权限（root/UID/文件 rwx） ================= -->
<section class="sec" id="s8">
  <div class="wrap">
    <h2>第八阶段：用户与权限（root / UID / 文件 rwx）</h2>
    <p class="sub">一句话意义：系统第一次有"主人和客人"——root(uid=0) 管理员、guest(uid=1) 普通用户；每个文件标了属主和 rwx 权限，读写/建目录/进目录都先查权限。guest 读 root 私有文件被 EACCES 拒绝。</p>

    <h3>用户表（user.c / user.h）</h3>
    <table class="tbl">
      <colgroup><col style="width:12%"><col style="width:16%"><col style="width:72%"></colgroup>
      <tr><th>UID</th><th>用户名</th><th>权限语义</th></tr>
      <tr><td>0</td><td><b>root</b></td><td>管理员：绕过一切权限检查（Linux root 惯例）</td></tr>
      <tr><td>1</td><td><b>guest</b></td><td>普通用户：按文件 mode 的 owner/other 位判断</td></tr>
    </table>

    <h3>文件权限模型（fs.c）</h3>
    <table class="tbl">
      <colgroup><col style="width:24%"><col style="width:76%"></colgroup>
      <tr><th>元素</th><th>说明</th></tr>
      <tr><td>owner_uid</td><td>每槽属主（创建者即属主）</td></tr>
      <tr><td>mode（6 位）</td><td>bit5-3 = 属主 rwx；bit2-0 = 他人 rwx。0644=属主读写+他人读；0600=仅属主；0700=仅属主全权</td></tr>
      <tr><td>fs_can(uid,owner,mode,want)</td><td>want:4=读/2=写/1=可进入；uid==owner 看 owner 位，否则看 other 位；uid==0 恒通过</td></tr>
      <tr><td>检查点</td><td>fs_open（读/写模式）、fs_write、fs_read、fs_mkdir（父目录写）、fs_chdir（目录 x 位）</td></tr>
      <tr><td>预置演示文件</td><td>/secret.txt 0600、/public.txt 0644、/rootonly 0700 目录</td></tr>
    </table>

    <h3>子任务与实测（全部 Bochs 实机 + 串口 + 截图）</h3>
    <table class="tbl">
      <colgroup><col style="width:16%"><col style="width:44%"><col style="width:40%"></colgroup>
      <tr><th>子任务</th><th>做了什么</th><th>实测铁证</th></tr>
      <tr><td>S8-1 用户与身份</td><td>user.c 账户表；login/whoami/id；pcb.uid；syscall15 getuid；提示符 user@kernel:path #/$</td><td>login guest → whoami guest(uid=1)；提示符 guest@kernel:/ $</td></tr>
      <tr><td>S8-2 文件权限</td><td>fs_ent 加 owner/mode；fs_can 检查；chmod；ls 显示 mode+属主；预置三文件</td><td>guest 读 /secret.txt EACCES；root 放行</td></tr>
      <tr><td>S8-3 演示进程</td><td>user11.S：getuid + 读公开/私有 + 写文件；fs11 命令</td><td>guest：sec-read-DENIED；root：sec-read-OK；chmod 644→guest 可读、600→被拒</td></tr>
    </table>

    <h3>新增/修改文件</h3>
    <table class="tbl">
      <colgroup><col style="width:34%"><col style="width:66%"></colgroup>
      <tr><th>文件</th><th>职责</th></tr>
      <tr><td>src/user.c + user.h（新）</td><td>用户表、user_init/user_login/user_name_of/user_current_uid</td></tr>
      <tr><td>src/fs.h + fs.c</td><td>owner_uid+mode 字段、fs_can 检查、chmod、预置演示文件、ls 权限列</td></tr>
      <tr><td>src/sched.h + sched.c</td><td>pcb.uid、创建/fork 继承身份、syscall15 getuid、syscall16 chmod、ps 加 UID 列</td></tr>
      <tr><td>src/shell.c</td><td>login/whoami/id/chmod/fs11 命令、提示符用户名 + #/$</td></tr>
      <tr><td>src/user11.S + user11.ld（新）</td><td>权限演示进程（入口 0x680000，栈 0x7A1000）</td></tr>
      <tr><td>src/kernel.c</td><td>kmain 加 [10b] user_init</td></tr>
    </table>

    <h3>设计修正</h3>
    <table class="tbl">
      <colgroup><col style="width:22%"><col style="width:78%"></colgroup>
      <tr><th>编号</th><th>问题与修复</th></tr>
      <tr><td>修正 1</td><td>chmod 初版用十进制解析（644=六百四十四），与 Linux 八进制不符。修复：k_octal 八进制解析</td></tr>
      <tr><td>修正 2</td><td>测试工具 _sendkeys.py 缺 '/' '.' 键映射，路径参数被敲成 "secrettxt"。修复：补 VK 0xBF/0xBE</td></tr>
    </table>
  </div>
</section>

'''

d = d.replace('<!-- ================= 实测证据', s8 + '<!-- ================= 实测证据', 1)
d = d.replace('<a href="#s7">第七阶段</a>', '<a href="#s7">第七阶段</a>\n    <a href="#s8">第八阶段</a>', 1)
open(path, 'w', encoding='utf-8').write(d)

# ---------- 路线图更新：S8 done、S9 now ----------
rp = r'D:\WinPP\WinPP内核路线图.html'
r = open(rp, encoding='utf-8').read()
import re as _re
# S8 节点 -> done，S9 节点 -> now（"S9 信号与同步"原为下一步）
r = _re.sub(r'class="node now"><b>S9</b><span>信号与同步（下一步）', 'class="node now"><b>S9</b><span>信号与同步（下一步）', r)
r = _re.sub(r'class="node done"><b>S8</b><span>用户与权限（插入）', 'class="node done"><b>S8</b><span>用户与权限', r)
r = _re.sub(r'class="node now"><b>S9</b><span>信号与同步</span>', 'class="node now"><b>S9</b><span>信号与同步</span>', r)
# S8 若原为 now（黄色）改为 done
r = r.replace('class="node now"><b>S8</b>', 'class="node done"><b>S8</b>')
r = r.replace('class="node"><b>S9</b><span>信号与同步</span>', 'class="node now"><b>S9</b><span>信号与同步（下一步）</span>')
open(rp, 'w', encoding='utf-8').write(r)
print('s8 inserted:', d.find('id="s8"') > 0, '| nav:', d.find('>第八阶段<') > 0)
print('roadmap S8done:', r.find('class="node done"><b>S8</b>') > 0, '| S9now:', r.find('class="node now"><b>S9</b>') > 0)
