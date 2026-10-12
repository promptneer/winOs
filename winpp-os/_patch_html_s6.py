# -*- coding: utf-8 -*-
# 在 WinPP内核详解.html 追加 S6 章节（插到 S5 章节之后、实测证据之前），更新导航
path = r'D:\WinPP\WinPP内核详解.html'
d = open(path, encoding='utf-8').read()

s6 = u'''<!-- ================= 第六阶段：文件系统增强（目录/路径） ================= -->
<section class="sec" id="s6">
  <div class="wrap">
    <h2>第六阶段：文件系统增强（目录 / 路径）</h2>
    <p class="sub">一句话意义：第四阶段文件系统是"平铺的文件表"；第六阶段加了目录与路径——mkdir 建目录、dir/file.txt 定位文件、cd/ls/pwd 在目录间走动，向"迷你 Linux"的文件观又近一步。</p>

    <h3>新系统调用（int 0x80，eax 编号）</h3>
    <table class="tbl">
      <colgroup><col style="width:9%"><col style="width:22%"><col style="width:26%"><col style="width:43%"></colgroup>
      <tr><th>编号</th><th>调用</th><th>参数</th><th>说明</th></tr>
      <tr><td>13</td><td><b>mkdir</b></td><td>path</td><td>创建目录（相对/绝对路径），父目录必须是目录，已存在返回 0</td></tr>
      <tr><td>14</td><td><b>chdir</b></td><td>path</td><td>切换内核当前目录 fs_cwd，目标必须是目录</td></tr>
    </table>

    <h3>三个子任务（全部 Bochs 实机 + 串口 + 截图取证）</h3>
    <table class="tbl">
      <colgroup><col style="width:14%"><col style="width:46%"><col style="width:40%"></colgroup>
      <tr><th>子任务</th><th>做了什么</th><th>实测铁证</th></tr>
      <tr><td>S6-1 目录</td><td>fstab 槽 type=5 目录项；syscall 13 mkdir；父目录存在性检查；幂等返回 0</td><td>[fs] mkdir '/docs' fd=0；mkdir '/docs/docs2' fd=1；(exists) 幂等</td></tr>
      <tr><td>S6-2 路径解析</td><td>fs_resolve 相对->绝对（cwd 拼接 + 去尾斜杠）；fs_parent 取父；open 按绝对路径精确匹配，创建时校验父目录</td><td>open created '/docs/a.txt' fd=1；相对 "b.txt" -> '/docs/b.txt' fd=2；Sub File! 循环 800 次</td></tr>
      <tr><td>S6-3 shell 增强</td><td>mkdir/cd/pwd/ls 命令；提示符显示 cwd（winpp@kernel:/docs$）；fs_ls 前缀匹配列目录</td><td>[ls] /: docs/ 1 entries；fslist 4 entries (cwd=/docs)；0 页错误</td></tr>
    </table>

    <h3>新增/修改文件</h3>
    <table class="tbl">
      <colgroup><col style="width:34%"><col style="width:66%"></colgroup>
      <tr><th>文件</th><th>职责</th></tr>
      <tr><td>src/fs.c + fs.h</td><td>fs_cwd、fs_resolve/fs_parent/fs_is_dir、fs_mkdir、fs_chdir、fs_ls（目录列），open 路径化</td></tr>
      <tr><td>src/sched.c</td><td>syscall_handler 加 13=mkdir / 14=chdir（共 15 个系统调用）</td></tr>
      <tr><td>src/shell.c</td><td>cmd_mkdir/cmd_cd/cmd_pwd、cmd_ls 改真目录列、提示符显示 cwd、fs2 命令</td></tr>
      <tr><td>src/user10.S + user10.ld</td><td>目录/路径演示（mkdir /docs + 绝对写 a.txt + chdir + 相对写 b.txt）</td></tr>
    </table>
  </div>
</section>

'''

d = d.replace('<!-- ================= 实测证据', s6 + '<!-- ================= 实测证据', 1)
d = d.replace('<a href="#s5">第五阶段</a>', '<a href="#s5">第五阶段</a>\n    <a href="#s6">第六阶段</a>', 1)
open(path, 'w', encoding='utf-8').write(d)
print('s6 inserted:', 'id="s6"' in d, '| nav:', '>第六阶段<' in d)
