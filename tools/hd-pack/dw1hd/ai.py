"""AI models: download, upscaler, LaMa and Stable Diffusion inpainting.

Models are downloaded once into <work>/models and checked by SHA-256. They are
the ones the port's own packs were made with:
  4x-UltraSharpV2  (Kim2091, CC BY-NC-SA 4.0)  backgrounds and object sprites
  4x-UltraSharp    (Kim2091, CC BY-NC-SA 4.0)  movies
  big-lama         (LaMa, Apache 2.0)          widescreen side extension
  SD 1.5 inpainting (CreativeML OpenRAIL-M)    widescreen side extension, refinement"""
import hashlib
import os
import sys
import urllib.request

import numpy as np

MODELS = {
    '4x-UltraSharpV2.safetensors': (
        'https://huggingface.co/Kim2091/UltraSharpV2/resolve/main/4x-UltraSharpV2.safetensors',
        '2b5db674aa8f3864a696dccd1d07d7cc0500fbf220f35044cd30eee9a7f971a7'),
    '4x-UltraSharp.pth': (
        'https://huggingface.co/Kim2091/UltraSharp/resolve/main/4x-UltraSharp.pth',
        'a5812231fc936b42af08a5edba784195495d303d5b3248c24489ef0c4021fe01'),
    'big-lama.pt': (
        'https://huggingface.co/fashn-ai/LaMa/resolve/main/big-lama.pt',
        '7ba7aa7ac37a4d41fdbbeba3a2af7ead18058552997e3a3cd1a3b2210c9e6b4c'),
}
SD_REPO = 'stable-diffusion-v1-5/stable-diffusion-inpainting'

model_dir = 'models'
_dev = None


def device():
    global _dev
    if _dev is None:
        import torch
        if torch.cuda.is_available():
            _dev = 'cuda'
        elif getattr(torch.backends, 'mps', None) and torch.backends.mps.is_available():
            _dev = 'mps'
        else:
            _dev = 'cpu'
    return _dev


def describe_device():
    import torch
    d = device()
    if d == 'cuda':
        p = torch.cuda.get_device_properties(0)
        return f'cuda ({p.name}, {p.total_memory / 2**30:.1f} GB)'
    return d


def free():
    import torch
    if device() == 'cuda':
        torch.cuda.empty_cache()


def _sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for b in iter(lambda: f.read(1 << 20), b''):
            h.update(b)
    return h.hexdigest()


def fetch(name):
    """Path of a model, downloading it first if needed."""
    url, sha = MODELS[name]
    os.makedirs(model_dir, exist_ok=True)
    path = os.path.join(model_dir, name)
    if os.path.exists(path):
        return path
    print(f'Downloading {name} ...', flush=True)
    tmp = path + '.part'

    def hook(n, bs, total):
        if total > 0 and n % 200 == 0:
            sys.stdout.write(f'\r  {min(n * bs, total) / 2**20:.0f} / {total / 2**20:.0f} MB')
            sys.stdout.flush()
    urllib.request.urlretrieve(url, tmp, hook)
    print()
    if _sha256(tmp) != sha:
        os.remove(tmp)
        raise SystemExit(f'{name}: download is corrupt (checksum mismatch), run again')
    os.replace(tmp, path)
    return path


# ---------------------------------------------------------------- upscaler

_up = {}


def _load_upscaler(name):
    import torch
    import spandrel
    if name not in _up:
        _up.clear()
        free()
        m = spandrel.ModelLoader(device=device()).load_from_file(fetch(name)).eval()
        if device() == 'cuda' and m.supports_half:
            m.model.half()
        _up[name] = m
    return _up[name]


def upscale4(rgb, name='4x-UltraSharpV2.safetensors', tile=256, pad=16):
    """x4 of an RGB uint8 image, in overlapping tiles (no seams)."""
    import torch
    mdl = _load_upscaler(name)
    dtype = next(mdl.model.parameters()).dtype
    x = torch.from_numpy(np.ascontiguousarray(rgb)).permute(2, 0, 1)[None].to(device(), dtype) / 255
    s = mdl.scale
    _, _, h, w = x.shape
    out = torch.zeros((1, 3, h * s, w * s))
    for y0 in range(0, h, tile):
        for x0 in range(0, w, tile):
            ya, xa = max(y0 - pad, 0), max(x0 - pad, 0)
            yb, xb = min(y0 + tile + pad, h), min(x0 + tile + pad, w)
            with torch.no_grad():
                o = mdl(x[:, :, ya:yb, xa:xb]).float().cpu()
            oy, ox = (y0 - ya) * s, (x0 - xa) * s
            th, tw = (min(y0 + tile, h) - y0) * s, (min(x0 + tile, w) - x0) * s
            out[:, :, y0 * s:y0 * s + th, x0 * s:x0 * s + tw] = o[:, :, oy:oy + th, ox:ox + tw]
    return (out[0].clamp(0, 1).permute(1, 2, 0).numpy() * 255 + .5).astype(np.uint8)


# ---------------------------------------------------------------- inpainting

_lama = None
_sd = None


def unload_inpainting():
    global _lama, _sd
    _lama = _sd = None
    free()


def lama(rgb, mask, ext, context=256):
    """LaMa (big-lama) on each side separately: the ext columns plus `context` of original."""
    import torch
    global _lama
    if _lama is None:
        _lama = torch.jit.load(fetch('big-lama.pt'), map_location='cpu').eval()
    h, W = mask.shape
    out = rgb.copy()
    cw = min(ext + context, W)
    for xs in (slice(0, cw), slice(W - cw, W)):
        sub, k = rgb[:, xs], mask[:, xs]
        if not k.any():
            continue
        ph, pw = (-h) % 8, (-cw) % 8
        x = np.pad(sub, ((0, ph), (0, pw), (0, 0)), mode='edge')
        kk = np.pad(k, ((0, ph), (0, pw)), mode='edge')
        with torch.no_grad():
            xi = torch.from_numpy(x).permute(2, 0, 1)[None].float() / 255
            mi = torch.from_numpy(kk)[None, None].float()
            y = _lama(xi, mi)[0].permute(1, 2, 0).clamp(0, 1).cpu().numpy()
        y = (y[:h, :cw] * 255 + .5).astype(np.uint8)
        out[:, xs] = np.where(k[..., None], y, sub)
    return out


PROMPT = ('{zone}, empty pre-rendered 3D background of a PlayStation 1 game, '
          'seamless continuation of the scenery, same lighting, detailed textures')
NEG = ('text, letters, watermark, character, person, people, animal, creature, monster, figure, '
       'sprite, frame, border, black bars, vignette, blurry, lowres, seam, split screen')


def _sd_pipe():
    global _sd
    if _sd is None:
        import torch
        from diffusers import StableDiffusionInpaintPipeline, DPMSolverMultistepScheduler
        half = device() == 'cuda'
        kw = dict(torch_dtype=torch.float16, variant='fp16') if half else {}
        p = StableDiffusionInpaintPipeline.from_pretrained(
            SD_REPO, cache_dir=os.path.join(model_dir, 'hf'), safety_checker=None,
            requires_safety_checker=False, **kw)
        p.scheduler = DPMSolverMultistepScheduler.from_config(p.scheduler.config)
        p.set_progress_bar_config(disable=True)
        if device() == 'cuda':
            try:
                import torch
                if torch.cuda.get_device_properties(0).total_memory < 5 * 2**30:
                    p.enable_attention_slicing()
            except Exception:
                pass
        _sd = p.to(device())
    return _sd


def sd_refine(rgb, mask, init, zone='', window=512, steps=25, guidance=7.0, seed=1234,
              strength=0.75):
    """SD 1.5 inpainting in 512x512 windows down each side. Each window sees what the
    previous ones made; starting from `init` (LaMa) at strength < 1 keeps its layout."""
    import torch
    from PIL import Image
    pipe = _sd_pipe()
    h, W = mask.shape
    big = init.copy()
    done = ~mask
    gdev = 'cuda' if device() == 'cuda' else 'cpu'
    g = torch.Generator(gdev).manual_seed(seed)
    prompt = PROMPT.format(zone=zone.replace('_', ' ').lower())
    step_y = window - window // 4
    for x0 in (0, W - window):
        ys = list(range(0, max(h - window, 0) + 1, step_y))
        if ys[-1] != max(h - window, 0):
            ys.append(max(h - window, 0))
        for y0 in ys:
            y1, x1 = min(y0 + window, h), x0 + window
            need = mask[y0:y1, x0:x1] & ~done[y0:y1, x0:x1]
            if not need.any():
                continue
            crop = big[y0:y1, x0:x1]
            ch, cw = crop.shape[:2]
            img = Image.fromarray(crop)
            if (ch, cw) != (window, window):
                img = img.resize((window, window), Image.LANCZOS)
            mimg = Image.fromarray((need * 255).astype(np.uint8)).resize((window, window), Image.NEAREST)
            out = pipe(prompt=prompt, negative_prompt=NEG, image=img, mask_image=mimg,
                       height=window, width=window, num_inference_steps=steps,
                       guidance_scale=guidance, strength=strength, generator=g).images[0]
            out = np.asarray(out.resize((cw, ch), Image.LANCZOS))
            crop[need] = out[need]
            done[y0:y1, x0:x1] |= need
    return np.where(mask[..., None], big, rgb)


# ---------------------------------------------------------------- palette

def reindex(hd, idx1, pal, special, s, stp=None, radius=1, block=256):
    """Maps a truecolor HD image (s times idx1) back to palette indices. Each HD pixel
    takes, among the indices around its 1x pixel, the one whose day colour is closest
    (no dither): no index appears that was not already there, so the night and dusk
    CLUTs still mean the same thing. `special` 1x pixels (empty, transparent) are copied
    and never offered to others. With `stp`, a pixel only takes entries with the same
    semi-transparency bit as its own 1x pixel (object sprites)."""
    h1, w1 = idx1.shape
    out = np.empty((h1 * s, w1 * s), np.uint8)
    pad_i = np.pad(idx1, radius, mode='edge')
    pad_e = np.pad(special, radius, mode='edge')
    pal32 = pal.astype(np.int32)
    offs = [(dy, dx) for dy in range(-radius, radius + 1) for dx in range(-radius, radius + 1)]
    for y0 in range(0, h1, block):
        y1 = min(y0 + block, h1)
        blk = hd[y0 * s:y1 * s].astype(np.int32)
        own_i = np.repeat(np.repeat(idx1[y0:y1], s, 0), s, 1)
        own_e = np.repeat(np.repeat(special[y0:y1], s, 0), s, 1)
        best = np.full(blk.shape[:2], 1 << 30, np.int64)
        bidx = own_i.copy() if stp is not None else np.zeros(blk.shape[:2], np.uint8)
        for dy, dx in offs:
            ni = pad_i[radius + y0 + dy:radius + y1 + dy, radius + dx:radius + dx + w1]
            ne = pad_e[radius + y0 + dy:radius + y1 + dy, radius + dx:radius + dx + w1]
            ni = np.repeat(np.repeat(ni, s, 0), s, 1)
            ne = np.repeat(np.repeat(ne, s, 0), s, 1)
            d = ((blk - pal32[ni]) ** 2).sum(-1)
            bad = ne if stp is None else ne | (stp[ni] != stp[own_i])
            d[bad] = 1 << 30
            better = d < best
            best[better] = d[better]
            bidx[better] = ni[better]
        bidx[own_e] = own_i[own_e]
        out[y0 * s:y1 * s] = bidx
    return out


def psx2rgb(c):
    c = np.asarray(c, dtype=np.uint16)
    r, g, b = c & 31, (c >> 5) & 31, (c >> 10) & 31
    conv = lambda x: ((x << 3) | (x >> 2)).astype(np.uint8)
    return np.stack([conv(r), conv(g), conv(b)], -1)
