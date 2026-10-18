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
| Boot to title loop | works (x86, ARM/qemu) | with a blank disc image; **not yet run with the real disc** |
| libgs: display buffers, OT, sprites, box fill | done | PSY-Q reversed OTs; `GsSortClear` uses a fill primitive |
| libgs: coordinates, view, lights | done, untested on game data | `GsSetRefView2` and light sign need a check against the PS1 |
| libgs: TMD objects (`GsSortObject4J`, 48 drivers) | done, unit-tested | one generic renderer behind the `GsFCALL4` table; no subdivision, no fog |
| libmcrd (memory card) | done, unit-tested | raw 128 KB images, emulator-compatible |
| heap3 (`InitHeap3`/`malloc3`/`free3`) | done | first-fit, retail size |
| GTE extras | done | `ApplyMatrixLV/SV`, `ApplyTransposeMatrixLV`, `MulMatrix0`, `NormalColorCol3`, `RotNclip3/4` |
| Overlays | done | data reset on load; SHOP_REL is resident (see below) |
| Sound (libsnd/libspu) | PSY-Z | `SsUtAutoVol` applies the end volume at once (no ramp) |
| Movies (libpress) | partial | `DecDCTvlc2` maps to PSY-Z's `DecDCTvlc`; table build is a no-op |
| Renderer | PSY-Z SDL_GPU / GL | desktop only; no Vita backend |
| Vita build | not started | VitaSDK toolchain, PSY-Z Vita platform, `.vpk` |

## Backlog, in order

1. **Run with the real disc (Linux).** Boot to the title screen, start a new
   game, reach Jijimon's house, walk, open the menu, save and load. Fix what
   breaks. Expect: wrong lighting sign, camera details in `GsSetRefView2`,
   gaps in libgs 2D (`GsSortSprite` attribute bits), sound volume ramps.
2. **Battle and the overlays.** A battle (BTL), training (TRN/TRN2), a fishing
   trip (FISH), a tournament (STD/VS), an evolution (EVL), the intro movie
   (MOV). Overlay data resets are the thing to watch.
3. **Movies.** Check MOV against the retail intro; finish `DecDCTvlc2` if needed.
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
7. **Extras**, only once the game is complete: widescreen, higher internal
   resolution, no-wobble vertices.

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
