"""Extract every file of a Mode 2/2352 PS1 disc image (ISO9660) to a folder."""
import os
import struct
import sys

SECTOR = 2352
DATA = 24  # Mode 2 Form 1 user data offset


def read_sector(f, lba):
    f.seek(lba * SECTOR + DATA)
    return f.read(2048)


def read_extent(f, lba, size):
    out = bytearray()
    n = (size + 2047) // 2048
    for i in range(n):
        out += read_sector(f, lba + i)
    return bytes(out[:size])


def walk(f, lba, size, path, dst):
    data = read_extent(f, lba, size)
    off = 0
    while off < len(data):
        ln = data[off]
        if ln == 0:
            off = (off // 2048 + 1) * 2048
            continue
        rec = data[off:off + ln]
        ext = struct.unpack_from("<I", rec, 2)[0]
        sz = struct.unpack_from("<I", rec, 10)[0]
        flags = rec[25]
        nl = rec[32]
        name = rec[33:33 + nl]
        off += ln
        if name in (b"\x00", b"\x01"):
            continue
        name = name.decode("ascii", "replace").split(";")[0]
        full = os.path.join(path, name)
        if flags & 2:
            os.makedirs(os.path.join(dst, full), exist_ok=True)
            walk(f, ext, sz, full, dst)
        else:
            with open(os.path.join(dst, full), "wb") as o:
                o.write(read_extent(f, ext, sz))


def main():
    img, dst = sys.argv[1], sys.argv[2]
    os.makedirs(dst, exist_ok=True)
    with open(img, "rb") as f:
        pvd = read_sector(f, 16)
        root = pvd[156:156 + 34]
        lba = struct.unpack_from("<I", root, 2)[0]
        size = struct.unpack_from("<I", root, 10)[0]
        walk(f, lba, size, "", dst)


if __name__ == "__main__":
    main()
