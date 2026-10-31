"""Reads files straight from the user's Digimon World (USA) disc image.

Accepts the .cue (the first data track's .bin is used) or the .bin itself.
The image must be a raw dump: 2352-byte sectors, MODE2/2352."""
import os
import re
import struct

SECTOR = 2352
DATA_OFF = 24  # sync (12) + header (4) + subheader (8)


def find_bin(path):
    if not path.lower().endswith('.cue'):
        return path
    text = open(path, encoding='latin-1').read()
    m = re.search(r'FILE\s+"([^"]+)"', text) or re.search(r'FILE\s+(\S+)', text)
    if not m:
        raise SystemExit(f'{path}: no FILE line in the .cue')
    b = os.path.join(os.path.dirname(os.path.abspath(path)), m.group(1))
    if not os.path.exists(b):
        raise SystemExit(f'{path} points to "{m.group(1)}", which is not next to it.\n'
                         'Fix the FILE line in the .cue or pass the .bin directly.')
    return b


class Disc:
    def __init__(self, path):
        self.path = find_bin(path)
        self.f = open(self.path, 'rb')
        if os.path.getsize(self.path) % SECTOR:
            raise SystemExit(f'{self.path}: not a raw 2352-byte-sector image')
        pvd = self.read_sectors(16, 1)
        if pvd[1:6] != b'CD001':
            raise SystemExit(f'{self.path}: no ISO9660 file system (is this a raw .bin?)')
        self.root = self._record(pvd[156:156 + 34])
        try:
            self.find('SLUS_010.32')
        except FileNotFoundError:
            raise SystemExit(f'{self.path}: this is not Digimon World (USA, SLUS-01032)')

    def read_sectors(self, lba, n):
        out = bytearray()
        for i in range(n):
            self.f.seek((lba + i) * SECTOR + DATA_OFF)
            out += self.f.read(2048)
        return bytes(out)

    def read_raw(self, lba, n):
        """Whole 2352-byte sectors (STR movies use Form 2 sectors)."""
        self.f.seek(lba * SECTOR)
        return self.f.read(n * SECTOR)

    @staticmethod
    def _record(rec):
        lba = struct.unpack_from('<I', rec, 2)[0]
        size = struct.unpack_from('<I', rec, 10)[0]
        nl = rec[32]
        name = rec[33:33 + nl].decode('ascii', 'replace').split(';')[0]
        return {'lba': lba, 'size': size, 'dir': bool(rec[25] & 2), 'name': name}

    def listdir(self, d):
        data = self.read_sectors(d['lba'], (d['size'] + 2047) // 2048)[:d['size']]
        out, i = [], 0
        while i < len(data):
            ln = data[i]
            if ln == 0:
                i = (i // 2048 + 1) * 2048
                continue
            r = self._record(data[i:i + ln])
            if r['name'] not in ('\x00', '\x01'):
                out.append(r)
            i += ln
        return out

    def find(self, path):
        cur = self.root
        for part in [p for p in path.replace('\\', '/').split('/') if p]:
            for r in self.listdir(cur):
                if r['name'].upper() == part.upper():
                    cur = r
                    break
            else:
                raise FileNotFoundError(path)
        return cur

    def read(self, path):
        r = self.find(path)
        return self.read_sectors(r['lba'], (r['size'] + 2047) // 2048)[:r['size']]
