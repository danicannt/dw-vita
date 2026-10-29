# Porting notes

How the port works, what is done, what is not, and what comes next.

## How it works

```
game/ (decompiled game C, US)          unchanged logic; ~70 changed lines in 16 files, marked DW1_NATIVE
  │ calls PSY-Q
  ├─ port/   libgs, libmcrd, heap3, overlays, PS1 RAM arena, GTE/libsnd/libpress extras
  └─ PSY-Z   libgpu, libgte, libcd (emulated drive on the user's .cue/.bin),
             libspu/libsnd (emulated SPU), libetc (pad, VSync), libpress (MDEC)
               └─ SDL3 platform: window, renderer, audio, input
```

- **32-bit only.** The PS1 and the Vita are 32-bit, and the game keeps pointers
  in 32-bit fields and loads structures with pointers from disc (TMD object
  tables). A 64-bit build is not a goal.
- **Ordering tables.** PSY-Z widens GPU packet tags to a full pointer plus a
  length (`O_TAG`), so packets are bigger than on the PS1 and no 24-bit
  pointer packing is needed. Any game code that hard-codes a packet size must
  use `sizeof` (none found so far).
- **Overlays.** The PS1 loads one `*_REL.BIN` at a time over the same memory.
  Here all 15 overlays are linked in. `CMakeLists.txt` builds each overlay as
  its own object library and generates a linker script that groups each
  overlay's `.data`/`.bss` into a `.dw1ovl_<name>` section; `port/src/overlay.c`
  snapshots those at start and restores one when the game loads it
  (`loadDynamicLibrary` in `game/src/main/utils2.c`). The REL file is still read
  from disc, into a scratch buffer, so the async file queue keeps its order and
  callbacks.
- **Fixed PS1 addresses.** The game uses a few fixed buffers below the
  executable (`GENERAL_BUFFER` at 0x80010000, `TEXTURE_BUFFER`, the battle model
  area, two ordering tables, ...). They map into a 2 MB arena laid out like PS1
  RAM (`DW1_PS1_ADDR`), and the heap (`InitHeap3`) sits in the same arena at
  its retail address with its retail size.
- **Disc access.** The game's own CD code runs unchanged against PSY-Z's
  emulated drive, which reads the user's raw image. No data pack is built.
- **Text.** The decomp's `text/*.yaml` are turned into headers at build time by
  its own `tools/gen_text.py`; the strings stay Shift-JIS (`-fexec-charset=CP932`).

## Status

| Piece | State | Notes |
|---|---|---|
| Game code (US, 120 files, 17 units) | compiles, links | PS1-only prototypes guarded, fixed addresses mapped |
| Boot to title loop | works (x86, ARM/qemu) | real disc on Linux: intro, title, NEW GAME, Jijimon's house, first maps |
| libgs: display buffers, OT, sprites, box fill | done | PSY-Q reversed OTs; `GsSortClear` uses a fill primitive |
| libgs: coordinates, view, lights | done, untested on game data | `GsSetRefView2` and light sign need a check against the PS1 |
| libgs: TMD objects (`GsSortObject4J`, 48 drivers) | done, unit-tested | one generic renderer behind the `GsFCALL4` table; no subdivision, no fog |
| libmcrd (memory card) | done, unit-tested | raw 128 KB images, emulator-compatible |
| heap3 (`InitHeap3`/`malloc3`/`free3`) | done | first-fit, retail size |
| GTE extras | done | `ApplyMatrixLV/SV`, `ApplyTransposeMatrixLV`, `MulMatrix0`, `NormalColorCol3`, `RotNclip3/4` |
| Overlays | done | data reset on load; SHOP_REL is resident (see below) |
| Sound (libsnd/libspu) | PSY-Z, plays on Linux | `SsUtAutoVol` applies the end volume at once (no ramp); i386 SDL needs PulseAudio |
| Movies (STR, libpress) | works on Linux | streaming in `movie_stream.c`, C MDEC from DW2-PC with STR v3; intro checked |
| Renderer | PSY-Z SDL_GPU / GL | desktop only; no Vita backend |
| Vita build | builds, untested on hardware | `PSYZ_RENDERER=sdl3_soft` (CPU rasterizer, SDL3 GXM renderer only scales), `dw1.vpk`, data in `ux0:data/dw1/` |
| Debug aids | done | pad record/replay (`DW1_RECORD`/`DW1_REPLAY`), VRAM dumps (`DW1_DUMP`, `DW1_DUMP_VSYNC`), `DW1_TURBO`, crash stacks |
| Test batteries | maps, battles, evos | `DW1_WARP`, `DW1_SMOKE=maps|battles|evos` (`port/src/smoke.c`), run in parallel by `tools/smoke.py`; disc read audit `DW1_READLOG` + `tools/check_reads.py` |

## Backlog, in order

1. **Run with the real disc (Linux).** Boot to the title screen, start a new
   game, reach Jijimon's house, walk, open the menu, save and load. Fix what
   breaks. Expect: wrong lighting sign, camera details in `GsSetRefView2`,
   gaps in libgs 2D (`GsSortSprite` attribute bits), sound volume ramps.
2. **Battle and the overlays.** A battle (BTL), training (TRN/TRN2), a fishing
   trip (FISH), a tournament (STD/VS), an evolution (EVL), the intro movie
   (MOV). Overlay data resets are the thing to watch.
3. **Movies.** Done for the intro; check the other movies (MOV ids 1-3).
4. **PSY-Z Vita platform.** PSY-Z's PSP backend (`external/psyz/psyz/src/psp`)
   is the model: video, input, audio. Two routes for video:
   - start with PSY-Z's software rasterizer (`sdl3_software.c`) at 320x240
     and present the VRAM through SDL3's Vita renderer (simplest, slowest);
   - then a vitaGL or GXM renderer (like the PSP's `sceGu` one) for speed and
     higher resolution.
   Input: map the Vita buttons to the PS1 pad (L2/R2 on the rear touch or
   L/R). Audio: SDL3 Vita audio.
5. **Vita packaging.** VitaSDK toolchain, `vita_create_self` /
   `vita_create_vpk`, LiveArea assets, data in `ux0:data/dw1/` (disc image and
   cards; `host/main.c` already uses that path on `__vita__`).
6. **Performance on the Vita** (Cortex-A9 at 444 MHz): software GTE and SPU,
   `-O2`/`-O3`, NEON where it pays.
7. **Extras**: widescreen, HD backgrounds, HD movies, music and no-wobble
   vertices are done (see "Enhancements" below); higher internal resolution
   is the canvas scale.

## Enhancements (sdl3_gl renderer)

Each enhancement can be switched on or off in the title screen's **Options**
menu (`port/game/options_menu.c`), which applies it at once and saves it to
`options.ini` in the data directory (`ux0:data/dw1/` on the Vita). The
environment variables below still work and force their option for that run
(greyed out in the menu); see `port/src/options.c`.

During play, Start opens a pause menu (Continue, Save, Options, Return to
title) in place of the PS1's "PAUSE" box, and Select a debug menu (freeze
time, no aging, heal, happiness, bits) when Options > Sound and extras turns it on;
see `port/game/pause_menu.c`.

- **Widescreen** (`DW1_WIDE=extra[:scale]`, on by default on the Vita as
  52:2): drawing into a display buffer goes to a canvas `extra` game pixels
  wider on each side, at `scale` resolution, through a `vramOrigin` uniform
  (`Psyz_VideoSetWidescreen`); VRAM reads of a display buffer copy the
  canvas centre back. The game side (`DW1_NATIVE`) widens culling
  (`isOffScreen`, `isTileOffScreen`), the fade quads, keeps the camera on
  the picture (`dw1_cameraRangeX` in map.c) and anchors the clock and the
  status bars to the edges. Code: `port/src/widescreen.c`.
- **Native backgrounds**: the whole map is one texture of palette indices
  looked up in the map's CLUT (VRAM row 480, so day/dusk/night still work),
  queued in the ordering table as an application GP0 command (0xF7) instead
  of the 4x3 tile ring. Indices come from the map's tiles or from an HD pack,
  `<data>/hd/<MAP>.hdm` (format in widescreen.c; packs made by
  `dw1-tools/pack_fondos_hd.py`, AI-upscaled with AI-extended sides).
- **HD movies**: `<data>/hd/movies/OP1.mp4` (OP2, ED2, EDR) replaces that FMV
  (`MOV_playMovie`); the Vita decodes it with sceAvPlayer, Linux through
  ffmpeg. On the Vita sceAvPlayer has not delivered a frame yet: after 5 s
  the original movie plays.
- **Music**: PSY-Z had no libsnd SEQ interpreter (`_SsSeqPlay` was a stub),
  so no music ever played; `psyz/src/psyz/libsnd_seq.c` implements it.
- **Audio** (`PSYZ_SPU_INTERP=gauss|cubic`, "Improved" = cubic by default):
  the SPU's 4-tap gauss table, which muffles 4-16 kHz, can be swapped for
  Catmull-Rom weights at the same cost (`Psyz_SpuSetInterp`). Output stays at
  44.1 kHz: rendered offline, cubic at 44.1 and 48 kHz match up to 16 kHz.
  `patches/psyz/0015-spu-cubic-interpolation.patch`.
- **PGXP** (`DW1_PGXP=1`, on by default on the Vita): the GTE keeps each
  projected vertex's exact x, y and depth, and the renderer draws 3D
  polygons with them: no wobble, perspective-correct textures.
- **vitaGL pitfalls met here**: `glBlitFramebuffer` reads an FBO texture
  upside down from how shaders sample it (present and MoveImage use the
  quad shader instead); GLSL goes through a Cg translator, where `==` on
  vectors is per component; and a separate draw target kept in sync with
  blits never reached the sampled VRAM (the Vita draws into VRAM directly).

## Known risks and open questions

- **SHOP_REL**: the shop code is linked into the main executable (`src/shop`
  is part of SLUS_010.32), yet the game still loads `SHOP_REL.BIN` to
  0x80080800. Natively that read goes to the scratch buffer. Check what the
  file holds; if it is data the shop reads from that address, map it into the
  arena instead.
- **Overlay areas reused as buffers**: if the game uses an overlay's memory
  as a buffer while no overlay is loaded, the native layout will differ.
  Nothing found yet.
- **Overlay read buffer** is 512 KB; the largest overlay (MOV) is 0x73428 bytes.
- **Warnings are off** for game code (`-w`). Before the Vita work, build once
  with warnings on and review pointer/int casts.
- **Division by zero**: DW1 was built without zero-division checks and relies
  on the R3000's result (quotient -1, or 1 for a negative dividend; remainder =
  dividend), e.g. `tickHealingParticles`. On Linux/i386 the SIGFPE handler in
  `port/src/crash_handler.c` returns that result and logs each site once; the
  Vita's ARM returns 0 instead, so logged sites need a fix in the source.
- **Signed pointer compares**: the game sometimes compares pointers cast to
  `int32_t`; PS1 KSEG0 addresses (0x80...) are negative there, Linux ones
  positive, so a compare against NULL flips (the EFE sub-effect bound broke
  every model-less move). Fixed where found; the Vita (0x81000000+) behaves like
  the PS1, but fixes belong in the source.
- **PS1 primitive layout**: code that writes primitives at raw PS1 offsets
  (4-byte tag) corrupts PSY-Z's 8-byte-tag primitives; fixed so far in
  `addScreenPolyFT3` and the EFE ribbon / radial wave. "unsupported command"
  warnings during play point at more.
- **CD read order**: a synchronous read used to overtake queued ones that on
  the PS1 were already on the busy drive; readFile/readFileSectors now drain
  the file queue first (`dw1_file_queue_drain`).
- **TMD subdivision** ("div" drivers) is not done; large textured polygons near
  the camera will warp more than on the PS1.

## Fixes worth sending upstream (PSY-Z)

- The drawing offset (GP0 E5) is signed 11-bit, but `libgpu.c` decoded it
  unsigned and `Draw_SetOffset` (SDL_GPU and GL) wrapped negatives into VRAM,
  so DW1's buffer 0 (y offset -3) was drawn 509 lines down, outside the drawing
  area: the map flickered every other frame. Fixed by
  `patches/psyz/0001-signed-draw-offset.patch` (sign-extend, no wrap); apply it
  after checking out the submodule:
  `git -C external/psyz apply ../../patches/psyz/0001-signed-draw-offset.patch`.
- `libgte.c`: `NormalColorCol`, `NormalColorDpq` and `DpqColor` took their
  sf/lm bits from byte-swapped GTE command words (`0x1B04084B` for NCCS
  `0x0108041B`, ...), so sf = lm = 0: every lit TMD came out saturated
  white or black. `patches/psyz/0002-gte-command-constants.patch`.
- `sdl3_audio.c`: the SPU only advances in the audio callback, so without
  an audio device voices never end and anything waiting on one (dialogue)
  waits forever. Falls back to SDL's silent dummy driver and logs the driver
  in use. Recordings and replays also lock the SPU to video frames (`Psyz_AudioSetFrameLocked`), so voice timing no longer depends on the real-time audio clock. `patches/psyz/0003-audio-fallback-and-frame-lock.patch`.
- `sdl3_gl.c`: optional "pixel art" presentation pass,
  `Psyz_VideoSetPixelFilter` (`DW1_FILTER=pixel[:N]`): block sampling,
  palette snap and dark outlines in GLSL. Other renderers return -1.
  `patches/psyz/0004-pixel-filter.patch`.

- `libapi.c`: `VSync(n)` with n > 1 waited a single vblank, so the game's
  `VSync(3)` pacing ran it at 60 fps instead of 20. It now presents once and
  waits n vblanks (except in `PSYZ_VSYNC_LIMITLESS`).
  `patches/psyz/0005-vsync-n-vblanks.patch`.

### PS Vita build

```
export VITASDK=/usr/local/vitasdk PATH=$VITASDK/bin:$PATH
cmake -S . -B build/vita -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake -DPSYZ_RENDERER=sdl3_soft
cmake --build build/vita        # build/vita/dw1.vpk
```

GPU renderer (vitaGL; `vdpm install vitaGL`, and `libshacccg.suprx` on the
device): the same with `-B build/vita-gl -DPSYZ_RENDERER=sdl3_gl`. It uses
float-only shaders (`gl_float_shaders.h`, the SGX543 has no integer
arithmetic; `PSYZ_GL_FLOAT_SHADERS=1` tries them on Linux) and copies each
batch into fresh `vglAllocFromScratch` memory instead of rewriting one VBO,
as CTR High Octane does. `PSYZ_GL_PERF` / the Vita log print per-call timings.
All of this is `patches/psyz/0006-vita-soft-and-vitagl-renderers.patch`.

Faster vitaGL: `tools/build_vitagl.sh` builds it from source with the options
CTR High Octane and Lighthouse (Banjo-Kazooie) use, `NO_DEBUG=1` (no GL error
checks) and `HAVE_SHADER_CACHE=1` (compiled shaders kept in
`ux0:data/shader_cache/DGWD00001`, so only the first launch compiles them);
configure with `-DPSYZ_VITAGL_LIB=$PWD/build/vitagl/libvitaGL.a`. Without it
the vdpm library is linked.

vitaGL pitfalls met so far, all handled in `sdl3_gl.c`:
- `vglInitExtended` returns `GL_FALSE` on a normal init (`GL_TRUE` only on a
  resolution fallback).
- `vglAllocFromScratch` hands out unaligned blocks; sizes are rounded to 16.
- `glTexSubImage2D` on a texture the GPU sampled recently moves it to a CPU
  copy, but a framebuffer keeps the old address: VRAM uploads go through a
  fresh texture drawn by a shader instead (`UploadToVram`).
- `vita-elf-create` needs 3-5 KB of free address space right after the code
  segment; the linker script moves the location counter 16 KB between the
  code and data segments (padding inside the code segment does not help).

The Linux builds: `BUILD=build/linux32 tools/configure_linux32.sh
-DPSYZ_RENDERER=sdl3_gpu` (or `sdl3_gl`, `sdl3_soft`). Without the libGL.so
dev symlink, link the GL one through a shim: `mkdir build/linux32-gl/glshim &&
ln -s /usr/lib/i386-linux-gnu/libGL.so.1 build/linux32-gl/glshim/libGL.so` and
`"-DCMAKE_EXE_LINKER_FLAGS=-m32 -Lglshim"` (relative to the build directory,
so paths with spaces work).

Install `dw1.vpk` with VitaShell, then copy `Digimon World (USA).cue` and
`.bin` (and `card1.mcd` / `card2.mcd` if any) to `ux0:data/dw1/`. The log goes
to `ux0:data/dw1/log.txt`. PSY-Z changes: a `VITA` branch in its CMake, the
`sdl3_soft` renderer (also usable on Linux with `-DPSYZ_RENDERER=sdl3_soft`,
about 300 fps in turbo on a desktop) and Vita newlib fixes in `plat_unix.c`
(no `d_type`, no `ioctl`). GCC 15 (VitaSDK) errors on old C the decomp relies
on; game units turn those back into warnings.

Bugs found so far and their causes: [ERRORES.md](ERRORES.md).

- `include/libmcrd.h` defines its own `struct DIRENTRY` behind
  `#ifndef DIRENTRY`, which is always true, so it clashes with `kernel.h`'s
  (the correct PSY-Q layout). Worked around in `port/include/libmcrd.h`.
- `include/libgs.h` differs from the PSY-Q 4.x API in several places
  (`GsSortSprite`/`GsSortBoxFill` without the priority argument,
  `GsInitCoordinate2` with one argument, `GsIMAGE` without the TIM fields).
  The port uses its own `port/include/libgs.h`.
- `libgs`, `libmcrd`, `libds`, `DecDCTvlc2`, `SsUtGetVBaddrInSB`,
  `ApplyMatrixLV` and friends are still assembly in PSY-Z's decomp; the
  port's C versions could be offered back.
