"""Disassemble a DW1 (US) function from the original disc files.

usage: psxdis.py DECOMP_DIR DISC_DIR name [name...]
Symbols come from DECOMP_DIR/config/us/symbols*.txt; the binary is
SLUS_010.32 for symbols.txt and <OVL>_REL.BIN for symbols_<ovl>.txt, loaded at
the vram of the first segment in config/us/<ovl>.yaml.
"""
import glob
import os
import re
import struct
import sys

import capstone

SYM_RE = re.compile(r"^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(0x[0-9A-Fa-f]+)\s*;")


def load_symbols(decomp):
    tables = {}
    for path in glob.glob(os.path.join(decomp, "config/us/symbols*.txt")):
        base = os.path.basename(path)
        ovl = "main" if base == "symbols.txt" else base[len("symbols_"):-4]
        syms = {}
        with open(path) as f:
            for line in f:
                m = SYM_RE.match(line)
                if m:
                    syms[m.group(1)] = int(m.group(2), 16)
        tables[ovl] = syms
    return tables


def overlay_base(decomp, ovl):
    with open(os.path.join(decomp, "config/us", ovl + ".yaml")) as f:
        text = f.read()
    m = re.search(r"start:\s*0x0\s*\n\s*vram:\s*(0x[0-9A-Fa-f]+)", text)
    return int(m.group(1), 16)


def load_binary(decomp, disc, ovl):
    if ovl == "main":
        data = open(os.path.join(disc, "SLUS_010.32"), "rb").read()
        t_addr = struct.unpack_from("<I", data, 0x18)[0]
        return data[0x800:], t_addr
    name = ovl.upper() + "_REL.BIN"
    return open(os.path.join(disc, name), "rb").read(), overlay_base(decomp, ovl)


def main():
    decomp, disc = sys.argv[1], sys.argv[2]
    tables = load_symbols(decomp)
    md = capstone.Cs(capstone.CS_ARCH_MIPS, capstone.CS_MODE_MIPS32 | capstone.CS_MODE_LITTLE_ENDIAN)
    for name in sys.argv[3:]:
        hits = [(ovl, syms[name]) for ovl, syms in tables.items() if name in syms]
        if not hits:
            print("## %s: not in the symbol maps" % name)
            continue
        for ovl, addr in hits:
            data, base = load_binary(decomp, disc, ovl)
            # end: the next symbol above it in the same table
            above = sorted(a for a in tables[ovl].values() if a > addr)
            end = above[0] if above else addr + 0x400
            end = min(end, addr + 0x2000)
            names = {a: n for n, a in tables[ovl].items()}
            names.update({a: n for n, a in tables["main"].items()})
            print("## %s @ 0x%08X (%s, %d bytes)" % (name, addr, ovl, end - addr))
            code = data[addr - base:end - base]
            for ins in md.disasm(code, addr):
                op = ins.op_str
                m = re.search(r"0x([0-9a-f]{8})", op)
                if m and int(m.group(1), 16) in names:
                    op += "   <%s>" % names[int(m.group(1), 16)]
                print("  %08x: %-8s %s" % (ins.address, ins.mnemonic, op))


if __name__ == "__main__":
    main()
