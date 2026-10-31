#!/usr/bin/env python3
"""Unlocks the ready-made HD packs (*.dw1hd) with YOUR OWN disc.

  python unlock_hd_pack.py "Digimon World (USA).cue" [pack.dw1hd ...]

Without packs it uses every .dw1hd next to this script. The files go to the
"hd" folder next to this script (or --out): copy that folder to ux0:data/dw1/
on the Vita. Only needs Python 3.8+, nothing else to install."""
import argparse
import glob
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('disc', help='Digimon World (USA) .cue or .bin (raw 2352-byte sectors)')
    ap.add_argument('packs', nargs='*', help='.dw1hd files (default: all next to this script)')
    ap.add_argument('--out', default=os.path.join(HERE, 'hd'), help='output folder (default: hd next to this script)')
    a = ap.parse_args()
    from dw1hd import disc as disc_mod, sealed
    packs = a.packs or sorted(glob.glob(os.path.join(HERE, '*.dw1hd')))
    if not packs:
        sys.exit('No .dw1hd packs found: download them from the release page and put them next to this script.')
    d = disc_mod.Disc(a.disc)
    print(f'Disc: {d.path}')
    t0, bad = time.time(), 0
    for p in packs:
        print(f'== {os.path.basename(p)}')
        done, skipped, failed = sealed.unseal(d, p, a.out)
        print(f'   {done} unlocked, {skipped} already there, {failed} failed')
        bad += failed
    print(f'Done in {time.time() - t0:.0f} s: {a.out}')
    print('Copy that "hd" folder to ux0:data/dw1/ on your Vita.')
    if bad:
        print(f'{bad} files did not match your disc. It must be Digimon World (USA), SLUS-01032,'
              ' as a raw .bin/.cue dump (Redump: "Digimon World (USA)").')
        sys.exit(1)


if __name__ == '__main__':
    main()
