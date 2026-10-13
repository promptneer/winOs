# -*- coding: utf-8 -*-
"""WinPP OS host console -- Windows <-> WinPP kernel communication (UDP over Bochs slirp)

Links (see winpp-os/slirp.conf):
    Windows -> 127.0.0.1:7777  --(slirp hostfwd)--> kernel 10.0.2.15:7777   (chatd 7777)
    Windows -> 127.0.0.1:7778  --(slirp hostfwd)--> kernel 10.0.2.15:7778   (chatd 7778 cmd)
    kernel  -> 10.0.2.2:40001  --(slirp NAT)------> Windows 0.0.0.0:40001    (this tool)

Kernel side commands:
    chatd 7777 echo      message endpoint with auto-ACK
    chatd 7778 cmd       remote command endpoint (executes the line, sends screen back)
    host <text>          kernel sends a message to this console (port 40001)
    netstat              addresses / ARP / endpoints / counters

Usage:
    python winpp-host.py                          interactive chat
    python winpp-host.py --send "hello"           one-shot message, waits for ACK
    python winpp-host.py --cmd "ping 10.0.2.2"    run a kernel command remotely
    python winpp-host.py --cmd "help" --cmd "netstat"
    python winpp-host.py --monitor                listen only
Interactive extras:  /cmd <kernel command>   /quit
"""
import argparse
import socket
import sys
import threading
import time

DEFAULT_GUEST_PORT = 7777      # host -> kernel : message channel
DEFAULT_CMD_PORT = 7778        # host -> kernel : remote command channel
DEFAULT_HOST_PORT = 40001      # kernel -> host : this tool listens here
GUEST_IP = "127.0.0.1"         # slirp hostfwd listens on the host loopback

stop = threading.Event()


def ts():
    return time.strftime("%H:%M:%S")


def receiver(sock):
    while not stop.is_set():
        try:
            data, addr = sock.recvfrom(4096)
        except socket.timeout:
            continue
        except OSError:
            break
        text = data.decode("latin-1", "replace").rstrip("\r\n")
        print("\n[%s] <- kernel %s:%d (%d bytes): %s"
              % (ts(), addr[0], addr[1], len(data), text), flush=True)
        print("> ", end="", flush=True)


def main():
    ap = argparse.ArgumentParser(description="WinPP OS host console")
    ap.add_argument("--guest-port", type=int, default=DEFAULT_GUEST_PORT,
                    help="host->kernel message port (default 7777)")
    ap.add_argument("--cmd-port", type=int, default=DEFAULT_CMD_PORT,
                    help="host->kernel remote command port (default 7778)")
    ap.add_argument("--host-port", type=int, default=DEFAULT_HOST_PORT,
                    help="local port this tool listens on (default 40001)")
    ap.add_argument("--bind", default="0.0.0.0", help="local bind address")
    ap.add_argument("--send", action="append", default=[], help="send a message (repeatable)")
    ap.add_argument("--cmd", action="append", default=[],
                    help="run a kernel command remotely (repeatable)")
    ap.add_argument("--monitor", action="store_true", help="listen only")
    ap.add_argument("--timeout", type=float, default=5.0, help="wait seconds (default 5)")
    args = ap.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    # Windows 上用独占绑定：端口被别的进程（例如上一次没退干净的实例）占着时
    # 直接报错，而不是两个 socket 抢同一个端口、谁都收不全。
    if hasattr(socket, "SO_EXCLUSIVEADDRUSE"):
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
    else:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        sock.bind((args.bind, args.host_port))
    except OSError as e:
        print("!! bind %s:%d failed: %s" % (args.bind, args.host_port, e))
        print("   another instance may still hold the port "
              "(check: netstat -ano -p UDP | findstr %d)" % args.host_port)
        print("   or use another --host-port and run 'hostport <n>' in the kernel")
        return 1
    sock.settimeout(0.3)

    print("=" * 68)
    print(" WinPP host console")
    print("   listen  (kernel -> Windows) : %s:%d" % (args.bind, args.host_port))
    print("   message (Windows -> kernel) : %s:%d -> kernel 10.0.2.15:%d"
          % (GUEST_IP, args.guest_port, args.guest_port))
    print("   command (Windows -> kernel) : %s:%d -> kernel 10.0.2.15:%d"
          % (GUEST_IP, args.cmd_port, args.cmd_port))
    print("   kernel side:  chatd %d echo   chatd %d cmd" % (args.guest_port, args.cmd_port))
    print("=" * 68)

    def send(port, text):
        try:
            sock.sendto(text.encode("latin-1", "replace"), (GUEST_IP, port))
            print("[%s] -> kernel :%d (%d bytes): %s" % (ts(), port, len(text), text))
            return True
        except OSError as e:
            print("!! send failed: %s" % e)
            return False

    # ---- non-interactive: remote commands ----
    if args.cmd:
        for c in args.cmd:
            send(args.cmd_port, c)
            end = time.time() + args.timeout
            got = False
            while time.time() < end:
                try:
                    data, addr = sock.recvfrom(4096)
                except socket.timeout:
                    continue
                if not got:
                    print("---- screen from kernel ----")
                    got = True
                sys.stdout.write(data.decode("latin-1", "replace"))
            if not got:
                print("---- no screen received (bind 7778 cmd on the kernel side?) ----")
            print()
        sock.close()
        return 0

    # ---- non-interactive: messages ----
    if args.send:
        for s in args.send:
            send(args.guest_port, s)
            time.sleep(0.4)
        end = time.time() + args.timeout
        while time.time() < end and not stop.is_set():
            try:
                data, addr = sock.recvfrom(4096)
            except socket.timeout:
                continue
            text = data.decode("latin-1", "replace").rstrip("\r\n")
            print("[%s] <- kernel %s:%d (%d bytes): %s"
                  % (ts(), addr[0], addr[1], len(data), text))
        stop.set()
        sock.close()
        return 0

    if args.monitor:
        print("(monitor only, Ctrl+C to quit)")
        threading.Thread(target=receiver, args=(sock,), daemon=True).start()
        try:
            while True:
                time.sleep(0.5)
        except KeyboardInterrupt:
            pass
        stop.set()
        sock.close()
        return 0

    # ---- interactive ----
    print("(type a line + Enter to send;  /cmd <kernel command>;  /quit)")
    threading.Thread(target=receiver, args=(sock,), daemon=True).start()
    try:
        while True:
            try:
                line = input("> ")
            except EOFError:
                break
            cmd = line.strip()
            if cmd in ("/quit", "/exit", "/q"):
                break
            if not cmd:
                continue
            if cmd.startswith("/cmd"):
                send(args.cmd_port, cmd[4:].strip())
            else:
                send(args.guest_port, line)
    except KeyboardInterrupt:
        pass
    finally:
        stop.set()
        sock.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
