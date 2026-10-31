"""HD movies: movies/<V>.mp4 for the port (Options > HD movies).

  OP1 intro, OP2 new game, ED2 ending, EDR credits.

1. The STR is copied from the disc with its raw sectors and ffmpeg decodes it
   (320x176 MDEC video at 30 fps, XA audio).
2. Each frame is scaled x4 by 4x-UltraSharp and brought to 960x528. Where the
   picture does not move, frames are blended with the previous one (0.8) to
   hide the flicker GAN upscalers have.
3. H.264 Main 3.1, 960x544 (8 px bars), 30 fps, AAC 48 kHz: what sceAvPlayer on
   the Vita decodes without stutter.
Each step is skipped when its output is there, so it can be stopped and resumed."""
import glob
import os
import shutil
import subprocess
import time

import numpy as np
from PIL import Image

from . import ai

MOVIES = ['OP1', 'OP2', 'ED2', 'EDR']
FPS = 30
SIZE = (960, 528)
MODEL = '4x-UltraSharp.pth'


def ffmpeg_exe():
    exe = shutil.which('ffmpeg')
    if exe:
        return exe
    try:
        import imageio_ffmpeg
        return imageio_ffmpeg.get_ffmpeg_exe()
    except ImportError:
        raise SystemExit('ffmpeg not found: install it, or "pip install imageio-ffmpeg"')


def ff(*args):
    subprocess.run([ffmpeg_exe(), '-hide_banner', '-loglevel', 'fatal', '-y', *args], check=True)


def extract(disc, v, d):
    os.makedirs(d, exist_ok=True)
    raw, wav, src = f'{d}/{v}.STR', f'{d}/{v}.wav', f'{d}/src'
    if not os.path.exists(raw):
        r = disc.find(f'MOVIE/{v}.STR')
        tmp = raw + '.tmp'
        with open(tmp, 'wb') as f:
            f.write(disc.read_raw(r['lba'], (r['size'] + 2047) // 2048))
        os.replace(tmp, raw)
    if not os.path.exists(wav):
        ff('-f', 'psxstr', '-i', raw, '-vn', '-c:a', 'pcm_s16le', wav + '.tmp.wav')
        os.replace(wav + '.tmp.wav', wav)
    if not os.path.exists(f'{src}/.done'):
        shutil.rmtree(src, ignore_errors=True)
        os.makedirs(src)
        ff('-f', 'psxstr', '-i', raw, '-an', '-fps_mode', 'passthrough', '-start_number', '0',
           f'{src}/%08d.png')
        open(f'{src}/.done', 'w').close()
    return sorted(glob.glob(f'{src}/*.png')), wav


def _load(f):
    import torch
    a = np.asarray(Image.open(f).convert('RGB'), np.float32) / 255
    return torch.from_numpy(a).permute(2, 0, 1)[None]


def upscale(frames, out, stabilize, log):
    import torch
    import torch.nn.functional as F
    os.makedirs(out, exist_ok=True)
    dev = ai.device()
    mdl = ai._load_upscaler(MODEL)
    dtype = next(mdl.model.parameters()).dtype
    prev_in = prev_out = None
    t0, made = time.time(), 0
    for i, f in enumerate(frames):
        dst = f'{out}/{i:08d}.png'
        x = _load(f)
        y = None
        if os.path.exists(dst):
            try:
                y = _load(dst).to(dev)
            except OSError:
                y = None
        if y is None:
            with torch.no_grad():
                y = mdl(x.to(dev, dtype)).float().clamp(0, 1)
            y = F.interpolate(y, size=SIZE[::-1], mode='bicubic', antialias=True).clamp(0, 1)
            if stabilize > 0 and prev_out is not None:
                d = (x - prev_in).abs().mean(1, keepdim=True).to(dev) * 255
                w = stabilize * (1 - (d / 6).clamp(0, 1))
                w = F.interpolate(w, size=y.shape[2:], mode='bilinear')
                y = y + w * (prev_out - y)
            a = (y[0].permute(1, 2, 0).cpu().numpy() * 255 + .5).astype(np.uint8)
            Image.fromarray(a).save(dst + '.tmp', format='PNG', compress_level=1)
            os.replace(dst + '.tmp', dst)
            made += 1
            if made % 100 == 0:
                dt = (time.time() - t0) / made
                log(f'  {i + 1}/{len(frames)} frames, ~{dt * (len(frames) - i - 1) / 60:.0f} min left')
        prev_in, prev_out = x, y


def encode(frames_dir, wav, mp4, crf):
    tmp = mp4 + '.tmp.mp4'
    ff('-framerate', str(FPS), '-start_number', '0', '-i', f'{frames_dir}/%08d.png', '-i', wav,
       '-vf', f'scale={SIZE[0]}:{SIZE[1]}:flags=lanczos,pad=960:544:0:8,format=yuv420p',
       '-c:v', 'libx264', '-preset', 'slow', '-crf', str(crf), '-profile:v', 'main',
       '-level', '3.1', '-maxrate', '6M', '-bufsize', '12M', '-r', str(FPS),
       '-c:a', 'aac', '-b:a', '192k', '-ar', '48000', '-ac', '2',
       '-shortest', '-movflags', '+faststart', tmp)
    os.replace(tmp, mp4)


def run(disc, work, out, which=None, stabilize=0.8, crf=20, keep=False, log=print):
    out = os.path.join(out, 'movies')
    os.makedirs(out, exist_ok=True)
    done = 0
    for v in which or MOVIES:
        mp4 = f'{out}/{v}.mp4'
        if os.path.exists(mp4):
            done += 1
            continue
        d = os.path.join(work, 'movies', v)
        t0 = time.time()
        log(f'[movies] {v}: reading from the disc')
        frames, wav = extract(disc, v, d)
        log(f'[movies] {v}: upscaling {len(frames)} frames')
        upscale(frames, f'{d}/hd', stabilize, log)
        log(f'[movies] {v}: encoding')
        encode(f'{d}/hd', wav, mp4, crf)
        if not keep:
            shutil.rmtree(d, ignore_errors=True)
        log(f'[movies] {v} done in {(time.time() - t0) / 60:.0f} min: {mp4}'
            f' ({os.path.getsize(mp4) / 2**20:.0f} MB)')
        done += 1
    return done
