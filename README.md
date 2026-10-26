# Digimon World native port (PS Vita / PC)

**PS Vita test build:** [`release/dw1-vita.vpk`](release/dw1-vita.vpk) — how to install and play: [`release/README.md`](release/README.md).

A native port of *Digimon World* (PlayStation, 1999, USA `SLUS-01032`) built from
the [Digimon World decompilation](https://github.com/jype0/dw_decomp). The game's
own C code is compiled for the target machine and the PlayStation libraries
are replaced by [PSY-Z](https://github.com/Xeeynamo/psyz) plus a port layer.
It is not an emulator.

Target: **PS Vita** (ARMv7, 32-bit). Development happens on 32-bit Linux
first, where debugging is easy, and on 32-bit ARM under qemu to catch
ARM-specific problems.

> **Status: early.** Everything compiles and links, and the game boots through
> its whole initialisation to the title-screen loop on x86 and ARM. It has not
> been run against a real disc yet, and there is no Vita build yet. See
> [doc/PORTING.md](doc/PORTING.md) for the state of every piece and the plan.

**You need your own copy of the game.** This repository contains no game
data: the port reads everything from your disc image at run time.

## Layout

```
external/dw_decomp   the decompilation (git submodule, upstream, untouched)
external/psyz        PSY-Z: PSY-Q replacement + SDL3 platform (git submodule)
game/                the US game sources, copied from the decomp by
                     tools/import_decomp.py; native fixes are marked DW1_NATIVE
port/                what the port adds: libgs, memory card, overlays, PS1 RAM,
                     GTE/libsnd/libpress pieces PSY-Z lacks
host/                entry point
tests/               unit tests that run without the disc
cmake/               generated source lists, toolchains
doc/PORTING.md       architecture, status, backlog
```

## Building (Linux, 32-bit)

Requirements: CMake 3.21+, Ninja, a multilib GCC (`gcc-multilib g++-multilib`),
Python 3 with PyYAML, and SDL3's build dependencies (X11 or Wayland, audio).

```sh
git clone --recursive <this repo>
cd dw1-port
git -C external/psyz submodule update --init external/SDL

tools/configure_linux32.sh          # HEADLESS=1 for a build without video/audio drivers
ninja -C build/linux32 dw1
(cd build/linux32 && ctest)         # unit tests
```

PSY-Z draws with SDL_GPU (Vulkan) by default; add `-DPSYZ_RENDERER=sdl3_gl` to
the configure line for OpenGL.

## Running

```sh
build/linux32/dw1 "path/to/Digimon World (USA).cue"
```

Use a raw dump (`.cue` + `.bin`, 2352-byte sectors). Memory cards are
`card1.mcd` / `card2.mcd` in the working directory, in the raw 128 KB format
that DuckStation, PCSX-Redux and ePSXe use, so saves move both ways.

## ARM check (no Vita needed)

On Ubuntu the ARM cross compilers and `gcc-multilib` cannot be installed
together (apt removes one for the other); use separate machines or containers,
as the CI does.

```sh
sudo apt install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf qemu-user
cmake -S . -B build/armhf -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-linux-gnueabihf.cmake \
  -DSDL_UNIX_CONSOLE_BUILD=ON
ninja -C build/armhf && (cd build/armhf && ctest)
```

## Updating the game sources

`tools/import_decomp.py` overwrites `game/` with the decomp's US sources.
Commit first, re-import, then re-apply the port's changes with git: they are
all marked `DW1_NATIVE`.

## Credits

- [jype0/dw_decomp](https://github.com/jype0/dw_decomp) (MIT) and the reverse
  engineering by [SydMontague](https://github.com/SydMontague/DW1-Code) it builds on
- [PSY-Z](https://github.com/Xeeynamo/psyz) (MIT) by Xeeynamo and contributors
- [DW2-PC](https://github.com/Wyrelade/DW2-PC) (CC0), whose overlay and
  memory-layout approach this port follows

*Digimon World* and its data belong to their owners.
