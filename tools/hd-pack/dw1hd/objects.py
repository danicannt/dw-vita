"""HD map object sprites: <MAP>.hdo packs (Options > HD object sprites, experimental).

The objects drawn over a background (doors, water, torches...) are TIMs in the .MAP.
This rebuilds the VRAM the game sets up in loadMap() (loadMapImage1/2), cuts each
texture page the objects use, scales it x4 with 4x-UltraSharpV2, brings it down to
the pack scale and maps it back to the page's CLUT, keeping the semi-transparency bit.

.hdo v1 (port/src/widescreen.c): "DWHO", u16 version, scale, pages, templates;
per page u16 col, u8 bpp, u8 flags, s16 clutX, clutY, u16 w, h, u, v, u32 offset;
per template u16 page, u, v, w, h, u8 abr, u8 wrap; then per page the HD indices,
the HD alpha and the PS1 CLUT."""
import os
import struct
import time

import numpy as np

from . import ai
from .backgrounds import unique_maps
from .maptable import MAPS

VRAM_W, VRAM_H = 1024, 512
DAY_CLUT_Y = 480
CLUT4_Y = 486
CLUT8_BASE_Y = 468


def read_tim(buf, off):
    magic, flags = struct.unpack_from('<II', buf, off)
    assert magic == 0x10, f'bad TIM at {off:#x}'
    bpp = {0: 4, 1: 8, 2: 16}[flags & 3]
    p = off + 8
    clut = None
    if flags & 8:
        size = struct.unpack_from('<I', buf, p)[0]
        x, y, w, h = struct.unpack_from('<4H', buf, p + 4)
        clut = dict(x=x, y=y, w=w, h=h,
                    data=np.frombuffer(buf, '<u2', w * h, p + 12).reshape(h, w).copy())
        p += size
    x, y, w, h = struct.unpack_from('<4H', buf, p + 4)
    img = dict(bpp=bpp, x=x, y=y, w=w, h=h,
               data=np.frombuffer(buf, '<u2', w * h, p + 12).reshape(h, w).copy())
    return img, clut


def build_vram(mp, n8, n4, tfs_day):
    """What loadMap() puts in VRAM from the .MAP's TIMs; offset of the object table."""
    vram = np.zeros((VRAM_H, VRAM_W), np.uint16)
    vram[DAY_CLUT_Y, 0:256] = tfs_day
    offs = np.frombuffer(mp, '<u4', len(mp) // 4)
    if not (n8 or n4):
        return vram, None
    k = 1
    for _ in range(n8):
        img, clut = read_tim(mp, offs[k]); k += 1
        vram[img['y']:img['y'] + img['h'], img['x']:img['x'] + img['w']] = img['data']
        if clut is not None and clut['y'] != DAY_CLUT_Y:
            vram[clut['y']:clut['y'] + clut['h'], clut['x']:clut['x'] + clut['w']] = clut['data']
    for i in range(n4):
        img, clut = read_tim(mp, offs[k]); k += 1
        vram[img['y']:img['y'] + img['h'], img['x']:img['x'] + img['w']] = img['data']
        if i == 0 and clut is not None:
            for r in range(clut['h']):  # loadMapImage2: one 16-colour row per CLUT
                vram[CLUT4_Y, r * 16:(r + 1) * 16] = clut['data'][r, :16]
    return vram, offs[k]


def read_templates(mp, off):
    count = struct.unpack_from('<h', mp, off)[0]
    out = []
    for i in range(count):
        v = struct.unpack_from('<9h', mp, off + 2 + 18 * i)
        out.append(dict(texX=v[0], texY=v[1], width=v[2], height=v[3], clut=v[7], transparency=v[8]))
    return out


def clut_mode(o):
    c = o['clut']
    if c == -1:
        return 8, 0, DAY_CLUT_Y
    if c < 16:
        return 4, c * 16, CLUT4_Y
    return 8, 0, c + CLUT8_BASE_Y


def page_x(o):
    bpp = clut_mode(o)[0]
    return 384 + (o['texX'] // 256) * (128 if bpp == 8 else 64)


def fill_transparent(rgb, opaque):
    """Transparent texels take the mean colour of their nearest opaque ones (layer by
    layer), so the upscaler does not bleed black into the edges."""
    out = rgb.copy().astype(np.float32)
    free = ~opaque
    known = opaque.copy()
    while free.any():
        acc = np.zeros(out.shape, np.float32)
        cnt = np.zeros(free.shape, np.float32)
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            sh = np.roll(known, (dy, dx), (0, 1))
            col = np.roll(out, (dy, dx), (0, 1))
            m = sh & free
            acc += col * m[..., None]
            cnt += m
        pick = (cnt > 0) & free
        if not pick.any():
            break
        out[pick] = acc[pick] / cnt[pick][:, None]
        known |= pick
        free &= ~pick
    return np.clip(out + .5, 0, 255).astype(np.uint8)


def pages_of(disc, mid):
    """Pages and templates of one map, as the game samples them, or None."""
    f, n8, n4, _, _ = MAPS[mid]
    d = f'MAP/MAP{mid // 15 + 1}/'
    mp = disc.read(d + f + '.MAP')
    tfs = disc.read(d + f + '.TFS')
    vram, obj_off = build_vram(mp, n8, n4, np.frombuffer(tfs, '<u2', 256, 8))
    if obj_off is None:
        return None
    templates = read_templates(mp, obj_off)
    groups = {}
    for i, o in enumerate(templates):
        bpp, cx, cy = clut_mode(o)
        groups.setdefault((page_x(o), bpp, cx, cy), []).append(i)
    pages, tmpl = [], {}
    for (hx0, bpp, cx, cy), ids in sorted(groups.items()):
        col = (hx0 - 384) // (128 if bpp == 8 else 64)
        bbox = [256, 256, 0, 0]
        for i in ids:
            o = templates[i]
            u0, v0 = o['texX'] % 256, o['texY'] % 256
            bbox = [min(bbox[0], u0), min(bbox[1], v0),
                    max(bbox[2], u0 + o['width']), max(bbox[3], v0 + o['height'])]
        full = np.zeros((256, 256), np.uint8)
        for v in range(256):
            row = vram[v, hx0:hx0 + (128 if bpp == 8 else 64)]
            if bpp == 8:
                full[v, 0::2] = row & 0xff
                full[v, 1::2] = row >> 8
            else:
                for k in range(4):
                    full[v, k::4] = (row >> (4 * k)) & 0xf
        x0, y0, x1, y1 = bbox
        crop = full[y0:y1, x0:x1]
        if crop.size == 0:
            continue
        clut = np.asarray(vram[cy, cx:cx + (256 if bpp == 8 else 16)], np.uint16)
        pages.append(dict(col=col, bpp=bpp, cx=cx, cy=cy, u=x0, v=y0, idx=crop, clut=clut))
        for i in ids:
            o = templates[i]
            u0, v0 = o['texX'] % 256, o['texY'] % 256
            tmpl[i] = dict(page=len(pages) - 1, u=u0 - x0, v=v0 - y0, w=o['width'], h=o['height'],
                           wrap=(u0 + o['width'] > 256) or (v0 + o['height'] > 256),
                           transparency=o['transparency'])
    return pages, tmpl, len(templates)


def scale_page(pg, scales):
    clut = pg['clut']
    n = len(clut)
    pal = np.zeros((256, 3), np.uint8)
    pal[:n] = np.stack([(clut & 31) << 3, ((clut >> 5) & 31) << 3, ((clut >> 10) & 31) << 3],
                       -1).astype(np.uint8)
    stp = np.zeros(256, bool)
    stp[:n] = (clut & 0x8000 != 0) & (clut != 0)
    opaque = clut[pg['idx']] != 0
    hd4 = ai.upscale4(fill_transparent(pal[pg['idx']], opaque))
    h, w = pg['idx'].shape
    out = {}
    for s in scales:
        if s == 4:
            hd = hd4
        else:
            from PIL import Image
            hd = np.asarray(Image.fromarray(hd4).resize((w * s, h * s), Image.LANCZOS))
        idx = ai.reindex(hd, pg['idx'], pal, ~opaque, s, stp=stp)
        alpha = np.repeat(np.repeat(opaque, s, 0), s, 1).astype(np.uint8) * 255
        out[s] = (idx, alpha)
    return out


def write_hdo(path, pages, scaled, tmpl, ntmpl, s):
    head = 12 + 20 * len(pages) + 12 * ntmpl
    offs, p = [], head
    for i, pg in enumerate(pages):
        idx, alpha = scaled[i][s]
        offs.append(p)
        p += idx.size + alpha.size + len(pg['clut']) * 2
    b = bytearray(b'DWHO' + struct.pack('<HHHH', 1, s, len(pages), ntmpl))
    for pg, off in zip(pages, offs):
        cy = pg['cy']
        flags = (1 if cy == 480 else 0) | (2 if cy == 486 else 0) | (4 if cy >= 468 and cy not in (480, 486) else 0)
        h1, w1 = pg['idx'].shape
        b += struct.pack('<HBBhhHHHHI', pg['col'], pg['bpp'], flags, pg['cx'], cy, w1, h1,
                         pg['u'], pg['v'], off)
    for i in range(ntmpl):
        t = tmpl.get(i)
        if t is None:
            b += struct.pack('<HHHHHBB', 0xFFFF, 0, 0, 0, 0, 0, 0)
            continue
        # as the game's GetTPage/SetSemiTrans: abr = transparency & 3, 4 = opaque
        abr = 4 if t['transparency'] == 4 else t['transparency'] & 3
        b += struct.pack('<HHHHHBB', t['page'], t['u'], t['v'], t['w'], t['h'], abr, 1 if t['wrap'] else 0)
    assert len(b) == head
    for i, pg in enumerate(pages):
        idx, alpha = scaled[i][s]
        b += np.ascontiguousarray(idx, np.uint8).tobytes()
        b += np.ascontiguousarray(alpha, np.uint8).tobytes()
        b += pg['clut'].astype('<u2').tobytes()
    tmp = path + '.tmp'
    with open(tmp, 'wb') as f:
        f.write(b)
    os.replace(tmp, path)


def run(disc, out, scales, only=None, log=print):
    os.makedirs(out, exist_ok=True)
    dst = lambda f: {s: os.path.join(out if len(scales) == 1 else os.path.join(out, f'x{s}'),
                                     f'{f}.hdo') for s in scales}
    ids = [i for i in unique_maps(disc) if not only or MAPS[i][0] in only]
    todo = [i for i in ids if not all(os.path.exists(p) for p in dst(MAPS[i][0]).values())]
    t_all, made = time.time(), 0
    for n, mid in enumerate(todo, 1):
        f = MAPS[mid][0]
        r = pages_of(disc, mid)
        if r is None or not r[0]:
            continue
        pages, tmpl, ntmpl = r
        t0 = time.time()
        scaled = [scale_page(pg, scales) for pg in pages]
        for s, path in dst(f).items():
            os.makedirs(os.path.dirname(path), exist_ok=True)
            write_hdo(path, pages, scaled, tmpl, ntmpl, s)
        made += 1
        log(f'[objects] {n}/{len(todo)} {f}: {len(pages)} pages in {time.time() - t0:.0f}s,'
            f' ~{(time.time() - t_all) / n * (len(todo) - n) / 60:.0f} min left')
    return made
