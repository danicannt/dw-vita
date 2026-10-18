#!/usr/bin/env python3
"""Check a DW1_READLOG log against the executable's symbols.

usage: tools/check_reads.py build/linux32/dw1 readlog.txt [more logs...]

Each CdRead destination is mapped to the symbol that contains it (`nm -S`).
Reads into dw1_ps1_ram (the PS1 RAM arena) follow the PS1 layout and are only
reported when they leave the arena; reads into any other symbol are reported
when they run past its end, because natively the next symbol is not the one
the game expects.
"""
import bisect
import subprocess
import sys


def load_symbols(exe):
    out = subprocess.run(["nm", "-S", "--defined-only", exe], capture_output=True, text=True, check=True).stdout
    syms = []
    main = None
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 4:
            addr, size, kind, name = int(parts[0], 16), int(parts[1], 16), parts[2], parts[3]
            if kind in "bBdDrR":
                syms.append((addr, size, name))
        if len(parts) >= 3 and parts[-1] == "main":
            main = int(parts[0], 16)
    syms.sort()
    return syms, main


def main():
    exe, logs = sys.argv[1], sys.argv[2:]
    syms, main_nm = load_symbols(exe)
    starts = [s[0] for s in syms]
    seen = {}
    for path in logs:
        base = None
        for line in open(path):
            parts = line.split()
            if parts[0] == "main":
                base = int(parts[1], 16) - main_nm
                continue
            if parts[0] != "read" or base is None:
                continue
            addr = int(parts[1], 16) - base
            size = int(parts[2])
            i = bisect.bisect_right(starts, addr) - 1
            if i < 0:
                continue
            s_addr, s_size, name = syms[i]
            if addr >= s_addr + s_size:
                key = ("outside", name, addr - s_addr, size)
            elif addr + size > s_addr + s_size:
                key = ("OVERFLOW", name, addr - s_addr, size, s_size)
            else:
                continue
            seen[key] = seen.get(key, 0) + 1
    if not seen:
        print("no read runs past its destination")
    for key, n in sorted(seen.items(), key=lambda kv: kv[0][1]):
        if key[0] == "OVERFLOW":
            print("OVERFLOW x%d: %s+0x%x, %d bytes, symbol is %d bytes (over by %d)"
                  % (n, key[1], key[2], key[3], key[4], key[2] + key[3] - key[4]))
        else:
            print("outside x%d: just past %s (+0x%x), %d bytes" % (n, key[1], key[2], key[3]))


main()
