#!/usr/bin/env python3
"""Builds the optional HD packs of the Digimon World port from YOUR OWN disc.

  python make_hd_pack.py "Digimon World (USA).cue"

Output (default: the dw1-hd/hd folder next to this script), copy the whole "hd" folder to ux0:data/dw1/ on the Vita:
  hd/<MAP>.hdm         HD widescreen backgrounds   (Options > HD backgrounds)
  hd/<MAP>.hdo         HD map object sprites       (Options > HD object sprites)
  hd/movies/<V>.mp4    HD movies                   (Options > HD movies)

Everything can be stopped (Ctrl+C) and resumed: run the same command again.
Nothing from the game is downloaded or uploaded: only the AI models are downloaded."""
import argparse
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

PARTS = ('backgrounds', 'objects', 'movies')


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('disc', help='Digimon World (USA) .cue or .bin (raw 2352-byte sectors)')
    ap.add_argument('--out', default=os.path.join(HERE, 'dw1-hd'),
                    help='work and output folder (default: dw1-hd next to this script)')
    ap.add_argument('--only', default=','.join(PARTS),
                    help='comma-separated parts to build: backgrounds,objects,movies (default: all)')
    ap.add_argument('--scale', type=int, choices=(2, 4), default=2,
                    help='background/object scale: 2 for the Vita (default), 4 for PC')
    ap.add_argument('--extend', choices=('sd', 'lama', 'none'),
                    help='how the widescreen sides of the backgrounds are made: '
                         'sd = LaMa + Stable Diffusion (best, what the port uses; default with an NVIDIA GPU), '
                         'lama = LaMa only (much faster; default without one), '
                         'none = no sides (black bars in widescreen)')
    ap.add_argument('--movies', default='OP1,OP2,ED2,EDR', help='which movies (default: all four)')
    ap.add_argument('--maps', help='only these map files, e.g. MAYO01,START00 (for a quick test)')
    ap.add_argument('--keep', action='store_true', help='keep the intermediate movie frames')
    a = ap.parse_args()

    parts = [p.strip() for p in a.only.split(',') if p.strip()]
    for p in parts:
        if p not in PARTS:
            ap.error(f'unknown part "{p}" (choose from {", ".join(PARTS)})')

    try:
        import numpy, PIL, torch, spandrel  # noqa: F401
    except ImportError as e:
        sys.exit(f'Missing Python package ({e.name}). Run the setup script first '
                 '(setup_windows.bat or setup_linux.sh), see README.md.')
    from dw1hd import ai, backgrounds, disc as disc_mod, movies, objects

    work = os.path.abspath(os.path.join(a.out, 'work'))
    out = os.path.abspath(os.path.join(a.out, 'hd'))
    ai.model_dir = os.path.join(os.path.abspath(a.out), 'models')
    d = disc_mod.Disc(a.disc)
    only = {m.strip().upper() for m in a.maps.split(',')} if a.maps else None
    extend = a.extend or ('sd' if ai.device() == 'cuda' else 'lama')
    if extend == 'sd':
        try:
            import diffusers, transformers  # noqa: F401
        except ImportError:
            sys.exit('--extend sd needs diffusers and transformers (pip install -r requirements.txt)')

    os.makedirs(os.path.abspath(a.out), exist_ok=True)
    logf = open(os.path.join(os.path.abspath(a.out), 'log.txt'), 'a', encoding='utf-8')

    def log(msg):
        print(msg, flush=True)
        logf.write(time.strftime('%H:%M:%S ') + msg + '\n')
        logf.flush()

    log(f'Disc: {d.path}')
    log(f'Device: {ai.describe_device()}')
    if ai.device() == 'cpu':
        log('WARNING: no GPU found, this will be very slow (days for everything). '
            'Try --only movies or --maps MAYO01 first.')
    log(f'Output: {out}')
    t0 = time.time()
    if 'backgrounds' in parts:
        log(f'== HD backgrounds (sides: {extend}, x{a.scale})')
        n = backgrounds.run(d, work, out, [a.scale], extend, only, log)
        log(f'== {n} backgrounds ready')
    if 'objects' in parts:
        log(f'== HD object sprites (x{a.scale})')
        n = objects.run(d, out, [a.scale], only, log)
        log(f'== object sprites ready ({n} new maps)')
    if 'movies' in parts and not only:
        which = [m.strip().upper() for m in a.movies.split(',') if m.strip()]
        log(f'== HD movies: {" ".join(which)}')
        movies.run(d, work, out, which, keep=a.keep, log=log)
    log(f'All done in {(time.time() - t0) / 3600:.1f} h.')
    log(f'Copy the folder "{out}" to ux0:data/dw1/ on your Vita (it becomes ux0:data/dw1/hd/).')


if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print('\nStopped. Run the same command again to continue where it left off.')
        sys.exit(130)
