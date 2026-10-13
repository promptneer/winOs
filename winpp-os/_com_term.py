# -*- coding: utf-8 -*-
"""S10 串口注入终端：连接 Bochs com1 socket (127.0.0.1:40000)
用法:
  python _com_term.py "ps\r" "gui" "x" "1" "q"  --hold 8
每个参数作为一个发送串（\r 为回车），间隔 0.6s；--hold 指定保持接收秒数。
串口输出实时追加写入 serial.out（证据链）。"""
import socket, sys, time, threading

HOST, PORT = "127.0.0.1", 40000
OUT = r"D:\WinPP\winpp-os\serial.out"

def main():
    cmds = []
    hold = 12
    server_mode = False
    i = 1
    while i < len(sys.argv):
        a = sys.argv[i]
        if a == "--hold":
            hold = float(sys.argv[i + 1]); i += 2
        elif a == "--port":
            global PORT
            PORT = int(sys.argv[i + 1]); i += 2
        elif a == "--server":
            server_mode = True; i += 1
        else:
            cmds.append(a); i += 1

    if server_mode:
        # 先监听，等 Bochs(client) 连上来
        srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        srv.bind((HOST, PORT))
        srv.listen(1)
        print("LISTENING on %s:%s ..." % (HOST, PORT), flush=True)
        s, addr = srv.accept()
        srv.close()
        print("BOCHS CONNECTED from %s" % repr(addr), flush=True)
    else:
        # 反复尝试连接（Bochs 先监听）
        s = None
        for _ in range(30):
            try:
                s = socket.create_connection((HOST, PORT), timeout=3)
                break
            except OSError:
                time.sleep(0.5)
        if s is None:
            print("CONNECT FAILED: no bochs com1 socket on %s:%s" % (HOST, PORT))
            sys.exit(1)
        print("CONNECTED to bochs com1 %s:%s" % (HOST, PORT))
    s.settimeout(0.2)

    stop = threading.Event()
    out = open(OUT, "ab")
    nbytes = [0]

    def reader():
        while not stop.is_set():
            try:
                data = s.recv(8192)
                if not data:
                    break
                out.write(data); out.flush()
                nbytes[0] += len(data)
                sys.stdout.write(data.decode("latin1", "replace"))
                sys.stdout.flush()
            except socket.timeout:
                continue
            except OSError:
                break

    t = threading.Thread(target=reader, daemon=True)
    t.start()

    for cmd in cmds:
        b = cmd.encode("latin1")
        s.sendall(b)
        print(">> SENT: %r" % cmd, flush=True)
        time.sleep(0.8)

    # 保持接收
    end = time.time() + hold
    while time.time() < end and not stop.is_set():
        time.sleep(0.5)

    stop.set()
    out.close()
    print("\n== serial.out bytes: %d ==" % nbytes[0])
    s.close()

if __name__ == "__main__":
    main()
