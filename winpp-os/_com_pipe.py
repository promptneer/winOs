# -*- coding: utf-8 -*-
"""S10 串口注入（命名管道版）：连接 Bochs com1 pipe \\\\.\\pipe\\winpp
用法:
  python _com_pipe.py "ps\r" "gui" --hold 10
先启动 Bochs（CreateNamedPipe 监听），再运行本脚本。
输出实时追加 serial.out（证据链）。"""
import ctypes, ctypes.wintypes as w, sys, time, threading

PIPE = r"\\.\pipe\winpp"
OUT = r"D:\WinPP\winpp-os\serial.out"

k32 = ctypes.WinDLL("kernel32", use_last_error=True)
k32.CreateFileW.restype = w.HANDLE
k32.CreateFileW.argtypes = [w.LPCWSTR, w.DWORD, w.DWORD, ctypes.c_void_p, w.DWORD, w.DWORD, w.HANDLE]
k32.ReadFile.argtypes = [w.HANDLE, ctypes.c_void_p, w.DWORD, ctypes.POINTER(w.DWORD), ctypes.c_void_p]
k32.WriteFile.argtypes = [w.HANDLE, ctypes.c_void_p, w.DWORD, ctypes.POINTER(w.DWORD), ctypes.c_void_p]
GENERIC_READ = 0x80000000
GENERIC_WRITE = 0x40000000
OPEN_EXISTING = 3
FILE_ATTRIBUTE_NORMAL = 0x80
INVALID_HANDLE = w.HANDLE(-1).value

def main():
    cmds = []
    hold = 12
    i = 1
    while i < len(sys.argv):
        a = sys.argv[i]
        if a == "--hold":
            hold = float(sys.argv[i + 1]); i += 2
        else:
            cmds.append(a); i += 1

    h = None
    for _ in range(30):
        h = k32.CreateFileW(
            PIPE, GENERIC_READ | GENERIC_WRITE, 0, None,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, None)
        if h != INVALID_HANDLE:
            break
        time.sleep(0.5)
    if h == INVALID_HANDLE:
        print("PIPE CONNECT FAILED, err=%d" % ctypes.get_last_error())
        sys.exit(1)
    print("PIPE CONNECTED: \\\\.\\pipe\\winpp", flush=True)

    stop = threading.Event()
    out = open(OUT, "ab")
    nbytes = [0]

    def reader():
        buf = ctypes.create_string_buffer(8192)
        nread = w.DWORD(0)
        while not stop.is_set():
            ok = k32.ReadFile(h, buf, 8192, ctypes.byref(nread), None)
            n = nread.value
            if n > 0:
                data = buf.raw[:n]
                out.write(data); out.flush()
                nbytes[0] += n
                sys.stdout.write(data.decode("latin1", "replace"))
                sys.stdout.flush()
            if not ok:
                time.sleep(0.1)
            else:
                time.sleep(0.02)

    t = threading.Thread(target=reader, daemon=True)
    t.start()

    for cmd in cmds:
        b = cmd.encode("latin1")
        nw = w.DWORD(0)
        ok = k32.WriteFile(h, b, len(b), ctypes.byref(nw), None)
        err = ctypes.get_last_error()
        print(">> SENT: %r ok=%d n=%d err=%d" % (cmd, bool(ok), nw.value, err), flush=True)
        time.sleep(0.8)

    end = time.time() + hold
    while time.time() < end:
        time.sleep(0.5)
    stop.set()
    out.close()
    print("\n== serial.out bytes: %d ==" % nbytes[0])
    k32.CloseHandle(h)

if __name__ == "__main__":
    main()
