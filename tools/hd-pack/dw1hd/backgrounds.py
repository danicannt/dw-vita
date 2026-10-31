"""HD backgrounds: <MAP>.hdm packs for the port (Options > HD backgrounds).

Each map's pre-rendered background is one 8-bit image with a day, dusk and night
CLUT. For widescreen it is extended EXT pixels left and right (LaMa, then SD 1.5
inpainting), scaled x4 by 4x-UltraSharpV2, brought down to the pack scale and
mapped back to the day palette, so the game's own CLUTs still give dusk and night.

Two passes so that only one model is on the GPU at a time:
  1. extend   -> <work>/extended/<MAP>.png  (indexed, original untouched)
  2. upscale  -> <out>/<MAP>.hdm

.hdm: "DWHD", u16 version 1, u16 scale, u16 extLeft, extRight, extTop, extBottom,
u16 width, u16 height, then width*height palette indices (port/src/widescreen.c)."""
import os
import struct
import time

import numpy as np
from PIL import Image

from . import ai
from .maptable import MAPS

TILE = 128
EXT = 64  # original pixels added on each side (16:9 at 240 lines needs ~54)


class Map:
    def __init__(self, disc, map_id):
        self.id = map_id
        self.file, _, _, self.flags, self.zone = MAPS[map_id]
        d = f'MAP/MAP{map_id // 15 + 1}/'
        mp = disc.read(d + self.file + '.MAP')
        setup = struct.unpack_from('<I', mp, 0)[0]
        w, h = struct.unpack_from('<ii', mp, setup + 36 * 4)
        tiles = struct.unpack_from(f'<{w * h}i', mp, setup + 38 * 4)
        self.tiles = np.array(tiles).reshape(h, w)
        tfs = disc.read(d + self.file + '.TFS')
        ncl = struct.unpack_from('<I', tfs, 4)[0]
        clut = np.frombuffer(tfs, '<u2', 256, 8)
        p = 8 + ncl * 512
        idx = np.zeros((h * TILE, w * TILE), np.uint8)
        for i, t in enumerate(tiles):
            if t == -1:
                continue
            p += 4
            px = np.frombuffer(tfs, np.uint8, TILE * TILE, p).reshape(TILE, TILE)
            p += TILE * TILE
            y, x = divmod(i, w)
            idx[y * TILE:(y + 1) * TILE, x * TILE:(x + 1) * TILE] = px
        self.idx = idx
        self.h, self.w = idx.shape
        self.pal = ai.psx2rgb(clut)
        self.transp = [i for i, c in enumerate(clut) if c == 0]
        self.empty = np.kron(self.tiles == -1, np.ones((TILE, TILE), bool))

    def rgb(self):
        return self.pal[self.idx]


def unique_maps(disc):
    """mapIds with a background on the disc, one per file (several ids share files)."""
    seen, out = set(), []
    for mid, (f, *_rest) in enumerate(MAPS):
        if not f or f in seen:
            continue
        seen.add(f)
        try:
            disc.find(f'MAP/MAP{mid // 15 + 1}/{f}.TFS')
        except FileNotFoundError:
            continue  # YAKA01, YAKA21, YAKA25: in the table, not on the disc
        out.append(mid)
    return out


def canvas(m, ext=EXT):
    """Extended RGB (black sides), mask to generate, mask of empty pixels. The strip next
    to an empty (-1) edge tile is not generated: it stays empty like the tile."""
    W = m.w + 2 * ext
    rgb = np.zeros((m.h, W, 3), np.uint8)
    rgb[:, ext:ext + m.w] = m.rgb()
    gen = np.zeros((m.h, W), bool)
    empty = np.zeros((m.h, W), bool)
    empty[:, ext:ext + m.w] = m.empty
    for r in range(m.tiles.shape[0]):
        ys = slice(r * TILE, (r + 1) * TILE)
        for col, xs in ((0, slice(0, ext)), (-1, slice(ext + m.w, W))):
            if m.tiles[r, col] == -1:
                empty[ys, xs] = True
            else:
                gen[ys, xs] = True
    return rgb, gen, empty


def quantize_sides(m, rgb, gen, ext):
    """1x indices of the extended canvas. The original is kept as is; each generated pixel
    takes the closest day colour among the indices the original uses in its two tiles
    nearest that edge, so a new index means "the same thing" at night too."""
    h, W = gen.shape
    idx = np.zeros((h, W), np.uint8)
    idx[:, ext:ext + m.w] = m.idx
    banned = set(m.transp)
    for xs, zone, ze in ((slice(0, ext), m.idx[:, :2 * TILE], m.empty[:, :2 * TILE]),
                         (slice(W - ext, W), m.idx[:, -2 * TILE:], m.empty[:, -2 * TILE:])):
        cand = np.array(sorted(set(np.unique(zone[~ze]).tolist()) - banned), np.int64)
        if len(cand) == 0:
            cand = np.array(sorted(set(range(256)) - banned))
        pix = rgb[:, xs].reshape(-1, 3).astype(np.int32)
        d = ((pix[:, None, :] - m.pal[cand][None].astype(np.int32)) ** 2).sum(-1)
        sub = cand[d.argmin(1)].reshape(h, -1).astype(np.uint8)
        idx[:, xs] = np.where(gen[:, xs], sub, 0)
    return idx


def write_hdm(path, idx, scale, ext_l, ext_r):
    h, w = idx.shape
    tmp = path + '.tmp'
    with open(tmp, 'wb') as f:
        f.write(b'DWHD' + struct.pack('<HH', 1, scale) + struct.pack('<HHHH', ext_l, ext_r, 0, 0)
                + struct.pack('<HH', w, h))
        f.write(np.ascontiguousarray(idx, np.uint8).tobytes())
    os.replace(tmp, path)


def run(disc, work, out, scales, extend, only=None, log=print):
    """extend: 'sd' (LaMa + SD 1.5, the port's own packs), 'lama' (faster, no SD) or 'none'."""
    ext = 0 if extend == 'none' else EXT
    ext_dir = os.path.join(work, f'extended-{extend}')
    os.makedirs(ext_dir, exist_ok=True)
    os.makedirs(out, exist_ok=True)
    ids = [i for i in unique_maps(disc) if not only or MAPS[i][0] in only]
    dst = lambda m: {s: os.path.join(out if len(scales) == 1 else os.path.join(out, f'x{s}'),
                                     f'{m}.hdm') for s in scales}

    # pass 1: widescreen sides
    todo = [i for i in ids if not all(os.path.exists(p) for p in dst(MAPS[i][0]).values())
            and not os.path.exists(os.path.join(ext_dir, MAPS[i][0] + '.png'))]
    t_all = time.time()
    for n, mid in enumerate(todo, 1):
        m = Map(disc, mid)
        t0 = time.time()
        rgb, gen, empty = canvas(m, ext)
        if ext and gen.any():
            o = ai.lama(rgb, gen, ext)
            if extend == 'sd':
                o = ai.sd_refine(rgb, gen, o, zone=m.zone)
        else:
            o = rgb
        idx = quantize_sides(m, o, gen, ext) if ext else m.idx.copy()
        assert (idx[:, ext:ext + m.w] == m.idx).all()
        im = Image.fromarray(idx, 'P')
        im.putpalette(m.pal.flatten().tolist())
        im.save(os.path.join(ext_dir, m.file + '.png'))
        log(f'[backgrounds 1/2] {n}/{len(todo)} {m.file} ({m.zone}) widened in {time.time() - t0:.0f}s,'
            f' ~{(time.time() - t_all) / n * (len(todo) - n) / 60:.0f} min left')
    ai.unload_inpainting()

    # pass 2: x4, down to the pack scale, back to the palette
    todo = [i for i in ids if not all(os.path.exists(p) for p in dst(MAPS[i][0]).values())]
    t_all = time.time()
    for n, mid in enumerate(todo, 1):
        m = Map(disc, mid)
        t0 = time.time()
        idx1 = np.asarray(Image.open(os.path.join(ext_dir, m.file + '.png'))).copy()
        _, _, empty = canvas(m, ext)
        special = empty.copy()
        for t in m.transp:
            special |= idx1 == t
        hd4 = ai.upscale4(m.pal[idx1])
        h1, w1 = idx1.shape
        for s, path in dst(m.file).items():
            os.makedirs(os.path.dirname(path), exist_ok=True)
            hd = hd4 if s == 4 else np.asarray(
                Image.fromarray(hd4).resize((w1 * s, h1 * s), Image.LANCZOS))
            write_hdm(path, ai.reindex(hd, idx1, m.pal, special, s), s, ext, ext)
        log(f'[backgrounds 2/2] {n}/{len(todo)} {m.file} upscaled in {time.time() - t0:.0f}s,'
            f' ~{(time.time() - t_all) / n * (len(todo) - n) / 60:.0f} min left')
    return len(ids)
