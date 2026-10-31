"""Sealed HD packs: ready-made HD files locked with the user's own disc.

Every file is compressed and XORed with a SHAKE-256 keystream whose key is the
hash of the disc data that file was made from:
  <MAP>.hdm, <MAP>.hdo   MAP/MAPn/<MAP>.MAP and .TFS
  movies/<V>.mp4         MOVIE/<V>.STR
so a sealed pack is noise without the disc, and unlocking checks each file
against the SHA-256 of the original.

Pack file (.dw1hd): b"DW1HDPK1", u32 header size, UTF-8 JSON header
{"files": [{name, size, sha256, comp, offset, length}]}, then the blobs.
Standard library only, so unlocking needs nothing installed besides Python."""
import hashlib
import json
import lzma
import os
import struct

from .maptable import MAPS

MAGIC = b'DW1HDPK1'
SALT = b'Digimon World Vita port HD pack v1\0'
CHUNK = 1 << 20


def source_key(disc, name):
    """Key for one pack file, from the disc files it was made from."""
    h = hashlib.sha256(SALT + name.encode() + b'\0')
    base = os.path.basename(name)
    stem, ext = os.path.splitext(base)
    if ext in ('.hdm', '.hdo'):
        mid = next(i for i, m in enumerate(MAPS) if m[0] == stem)
        for e in ('.MAP', '.TFS'):
            h.update(disc.read(f'MAP/MAP{mid // 15 + 1}/{stem}{e}'))
    elif ext == '.mp4':
        r = disc.find(f'MOVIE/{stem}.STR')
        n = (r['size'] + 2047) // 2048
        for i in range(0, n, 256):
            h.update(disc.read_sectors(r['lba'] + i, min(256, n - i)))
    else:
        raise ValueError(f'no disc source for {name}')
    return h.digest()


def xor_stream(data, key, start=0):
    """data XOR SHAKE-256(key || chunk number), chunk by chunk."""
    out = bytearray()
    for off in range(0, len(data), CHUNK):
        part = data[off:off + CHUNK]
        ks = hashlib.shake_256(key + struct.pack('<Q', (start + off) // CHUNK)).digest(len(part))
        x = int.from_bytes(part, 'little') ^ int.from_bytes(ks, 'little')
        out += x.to_bytes(len(part), 'little')
    return bytes(out)


def seal(disc, root, names, path, log=print):
    """Writes the files root/<name> into one sealed pack."""
    entries, blobs, off = [], [], 0
    for n, name in enumerate(names, 1):
        data = open(os.path.join(root, name), 'rb').read()
        comp = 'none' if name.endswith('.mp4') else 'xz'
        body = data if comp == 'none' else lzma.compress(data, preset=6)
        blob = xor_stream(body, source_key(disc, name))
        entries.append(dict(name=name, size=len(data), sha256=hashlib.sha256(data).hexdigest(),
                            comp=comp, offset=off, length=len(blob)))
        blobs.append(blob)
        off += len(blob)
        if n % 20 == 0 or n == len(names):
            log(f'  {n}/{len(names)} sealed')
    head = json.dumps({'files': entries}, separators=(',', ':')).encode()
    tmp = path + '.tmp'
    with open(tmp, 'wb') as f:
        f.write(MAGIC + struct.pack('<I', len(head)) + head)
        for b in blobs:
            f.write(b)
    os.replace(tmp, path)


def read_index(path):
    f = open(path, 'rb')
    if f.read(8) != MAGIC:
        raise SystemExit(f'{path}: not a sealed HD pack')
    hl = struct.unpack('<I', f.read(4))[0]
    files = json.loads(f.read(hl))['files']
    return f, 12 + hl, files


def unseal(disc, path, out, log=print):
    """Unlocks every file of a pack into out/. Returns (written, skipped, failed)."""
    f, base, files = read_index(path)
    done = skipped = bad = 0
    for n, e in enumerate(files, 1):
        dst = os.path.join(out, *e['name'].split('/'))
        if os.path.exists(dst) and os.path.getsize(dst) == e['size']:
            skipped += 1
            continue
        try:
            key = source_key(disc, e['name'])
        except (FileNotFoundError, StopIteration):
            log(f'  {e["name"]}: its source is not on this disc')
            bad += 1
            continue
        f.seek(base + e['offset'])
        body = xor_stream(f.read(e['length']), key)
        try:
            data = body if e['comp'] == 'none' else lzma.decompress(body)
        except lzma.LZMAError:
            data = b''
        if hashlib.sha256(data).hexdigest() != e['sha256']:
            log(f'  {e["name"]}: does not match (a different or modified disc image?)')
            bad += 1
            continue
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        with open(dst + '.tmp', 'wb') as o:
            o.write(data)
        os.replace(dst + '.tmp', dst)
        done += 1
        if n % 20 == 0 or n == len(files):
            log(f'  {n}/{len(files)}')
    return done, skipped, bad
