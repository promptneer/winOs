import struct, sys

path = r"D:\WinPP\winpp-os\winpp-kernel.elf"
data = open(path, "rb").read()
print("elf size:", len(data))

found = None
for off in range(0, 8192, 4):
    if off + 12 <= len(data):
        magic, flags, chksum = struct.unpack_from("<III", data, off)
        if magic == 0x1BADB002 and (magic + flags + chksum) & 0xFFFFFFFF == 0:
            found = (off, magic, flags, chksum)
            break
print("multiboot header:", found)
if found:
    off, magic, flags, chksum = found
    print("  flags=0x%08x -> " % flags, end="")
    parts = []
    if flags & 1: parts.append("ALIGN_MODULES")
    if flags & 2: parts.append("MEMINFO")
    if flags & 4: parts.append("VIDEO")
    if flags & 0x10000: parts.append("AOUT_KLUDGE")
    print("|".join(parts) if parts else "(none)")
    # check entry point in ELF header
    e_entry = struct.unpack_from("<I", data, 24)[0]
    print("  ELF entry point: 0x%08x" % e_entry)
