# HD packs

The port can use optional HD packs: **widescreen HD backgrounds**, **HD map
object sprites** and **HD movies**. They are made from the game's own images,
so none of them comes with the VPK. There are two ways to get them, and both
need **your own disc image** of Digimon World (USA, SLUS-01032) as `.bin` +
`.cue`, the same one you play with.

| Pack | Files | Port option |
|---|---|---|
| HD backgrounds | `hd/<MAP>.hdm` (240 maps) | HD packs > HD backgrounds (on) |
| HD object sprites | `hd/<MAP>.hdo` (213 maps) | HD packs > HD object sprites (off, experimental) |
| HD movies | `hd/movies/OP1.mp4` (`OP2`, `ED2`, `EDR`) | HD packs > HD movies (on) |

## Option A: unlock the ready-made packs (a few minutes, no GPU)

The release page has the packs used to develop the port as `.dw1hd` files.
Each file inside is locked with data from the matching file on the disc, so
they only work with your copy of the game.

1. Install **Python 3** ([python.org](https://www.python.org/downloads/); on
   Windows tick *Add python.exe to PATH*). Nothing else is needed.
2. Download this folder (`tools/hd-pack`, or the repository as a zip) and put
   the `.dw1hd` files you want next to `unlock_hd_pack.py`.
3. **Windows:** drag your `Digimon World (USA).cue` onto `unlock_hd_pack.bat`.
   **Linux / macOS:** `python3 unlock_hd_pack.py "path/to/Digimon World (USA).cue"`
4. Copy the `hd` folder it creates to **`ux0:data/dw1/`** on the Vita.

If it says that files do not match, your image is not the standard dump:
it has to be the USA release (SLUS-01032) as a raw `.bin` (2352-byte sectors),
as listed on Redump.

## Option B: build them yourself with AI (hours, NVIDIA GPU recommended)

This makes the same packs from scratch on your PC, and it can also make the
movies that are not in the ready-made packs yet. Nothing from the game is
downloaded or uploaded; only the AI models are downloaded, once.

### What you need

- Your dump of **Digimon World (USA, SLUS-01032)** as `.bin` + `.cue` (raw
  2352-byte sectors), the same one you play with.
- **Python 3.10 to 3.13** ([python.org](https://www.python.org/downloads/); on
  Windows tick *Add python.exe to PATH*).
- An **NVIDIA GPU** with 4 GB or more is strongly recommended. It works on
  an AMD/Intel GPU or the CPU too, but much more slowly (see the times below).
- About **15 GB of free disk space** (models ~5 GB, temporary movie frames,
  the packs). The temporary files are deleted as each part finishes.

### Windows

1. Download this folder (`tools/hd-pack`), or the whole repository as a zip.
2. Double-click **`setup_windows.bat`**. It installs everything into a `venv`
   folder here. For an **RTX 50** card run `setup_windows.bat cu128` from a
   terminal instead; without an NVIDIA card, `setup_windows.bat cpu`.
3. **Drag your `Digimon World (USA).cue` onto `make_hd_pack.bat`.**

### Linux / macOS

```sh
./setup_linux.sh                 # cu128 for RTX 50, cpu without NVIDIA; macOS uses the Apple GPU
./make_hd_pack.sh "path/to/Digimon World (USA).cue"
```

You also need `python3-venv` on Debian/Ubuntu.

### Then

When it finishes, everything is in **`dw1-hd/hd/`**. Copy the whole `hd`
folder to **`ux0:data/dw1/`** on the Vita (with VitaShell over USB or FTP), so
you end up with `ux0:data/dw1/hd/MAYO01.hdm`, `ux0:data/dw1/hd/movies/OP1.mp4`
and so on. The options are in the title screen's **OPTIONS** and in the pause
menu.

**It can be stopped at any time** (close the window or press Ctrl+C) and
resumed by running it again: finished maps and movies are not redone. You
can also copy what is done so far to the Vita and play with it; maps without
a pack use the original background.

### How long it takes

Measured on a laptop RTX 3050 (4 GB):

| Part | Time |
|---|---|
| Backgrounds, widened with Stable Diffusion (default with NVIDIA) | ~2 h widening + ~4 h upscaling |
| Backgrounds with `--extend lama` (default without NVIDIA) | ~10 min widening + ~4 h upscaling |
| Object sprites | ~1 h |
| Movies | ~1-2 h each |

A faster card is proportionally faster. On a CPU, expect days rather than
hours: build one part at a time with `--only`.

### Options

```
--only backgrounds,objects,movies   build only some parts (default: all)
--movies OP1,ED2                    only some movies
--maps MAYO01,START00               only some maps: a quick test first is a good idea
--extend sd|lama|none               how the widescreen sides are made: sd = LaMa + Stable
                                    Diffusion (best), lama = LaMa only (fast), none = no sides
--scale 2|4                         2 for the Vita (default), 4 for the PC build
--out FOLDER                        where everything goes (default: dw1-hd next to the script)
```

A quick test that takes a couple of minutes:

```
make_hd_pack.bat "Digimon World (USA).cue" --only backgrounds,objects --maps MAYO01
```

## How it works

- **Backgrounds**: each map's background is one 256-colour image with a day,
  dusk and night palette. It is extended 64 pixels left and right (LaMa, then
  Stable Diffusion 1.5 inpainting at a fixed seed), scaled 4x by
  4x-UltraSharpV2, brought down to 2x, and mapped back to the original
  palette pixel by pixel. That keeps the game's day/night cycle and fades
  working.
- **Object sprites**: the tool rebuilds the VRAM the game sets up for each map,
  cuts out the texture pages the objects use, and upscales them the same way,
  keeping each texel's semi-transparency bit.
- **Movies**: the STR files are read from the disc and decoded with ffmpeg.
  Each frame is scaled 4x by 4x-UltraSharp, with flicker reduction where the
  picture is still, then encoded as H.264 that the Vita's hardware decoder
  plays.

## Credits and licences

- [4x-UltraSharp and 4x-UltraSharpV2](https://huggingface.co/Kim2091) by
  Kim2091 (CC BY-NC-SA 4.0: non-commercial use only).
- [LaMa](https://github.com/advimman/lama) (Apache 2.0).
- [Stable Diffusion 1.5 inpainting](https://huggingface.co/stable-diffusion-v1-5/stable-diffusion-inpainting)
  (CreativeML OpenRAIL-M).
- [spandrel](https://github.com/chaiNNer-org/spandrel),
  [diffusers](https://github.com/huggingface/diffusers),
  [FFmpeg](https://ffmpeg.org).

The packs you make are for your own use with your own copy of the game.
Please don't share them.
