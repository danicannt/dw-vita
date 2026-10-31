#!/usr/bin/env python3
"""Seals finished HD files into .dw1hd packs for the release (developer side).

  python seal_hd_pack.py DISC.cue HD_DIR OUT_DIR

HD_DIR is laid out as on the Vita (<MAP>.hdm, <MAP>.hdo, movies/*.mp4). Writes
OUT_DIR/dw1-hd-backgrounds.dw1hd, dw1-hd-objects.dw1hd and dw1-hd-movies.dw1hd."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dw1hd import disc as disc_mod, sealed  # noqa: E402

GROUPS = {'backgrounds': '.hdm', 'objects': '.hdo', 'movies': '.mp4'}


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    d = disc_mod.Disc(sys.argv[1])
    root, out = sys.argv[2], sys.argv[3]
    os.makedirs(out, exist_ok=True)
    for group, ext in GROUPS.items():
        names = []
        for dp, _, fs in os.walk(root):
            for f in fs:
                if f.endswith(ext):
                    names.append(os.path.relpath(os.path.join(dp, f), root).replace(os.sep, '/'))
        if not names:
            continue
        path = os.path.join(out, f'dw1-hd-{group}.dw1hd')
        print(f'== {group}: {len(names)} files -> {path}')
        sealed.seal(d, root, sorted(names), path)
        print(f'   {os.path.getsize(path) / 2**20:.0f} MB')


if __name__ == '__main__':
    main()
