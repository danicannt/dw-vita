# Digimon World for PS Vita — test build

A native PS Vita port of *Digimon World* (PS1, US version), built from the
community decompilation. It is not an emulator: the game code runs natively,
with widescreen, HD backgrounds and other optional improvements.

**This is a fan project, free and non-commercial, not affiliated with Bandai
Namco. No game data is included: you need your own copy of the game.**

## What you need

- A PS Vita / PS TV with HENkaku/Ensō and VitaShell.
- Your own dump of *Digimon World* (USA, SLUS-01032) as `.bin` + `.cue`
  (raw 2352-byte sectors, as made by most dumping tools).

## Install

1. Copy `dw1-vita.vpk` to the Vita and install it with VitaShell.
2. Create the folder `ux0:data/dw1/` and copy your dump there, named exactly:
   - `ux0:data/dw1/Digimon World (USA).bin`
   - `ux0:data/dw1/Digimon World (USA).cue`

   (If your `.cue` refers to the `.bin` by another name, edit the `FILE` line
   inside the `.cue` to match.)
3. Launch the bubble.

Saves go to `ux0:data/dw1/card1.mcd` / `card2.mcd`, in the usual raw 128 KB
memory-card format: you can bring saves from DuckStation/ePSXe and back.

## Options

Pick **OPTIONS** on the title screen (below New Game / Continue). Choices are
saved in `ux0:data/dw1/options.ini`.

| Section | Option | Default | What it does |
|---|---|---|---|
| Video | Widescreen 16:9 | on | HUD at the screen edges |
| Video | No polygon wobble | on | PGXP: no wobbly polygons or warping textures |
| Video | Model depth fix | off | depth buffer for 3D models (slower; shows some overlaps of the original models) |
| Video | 3D texture filter | off | smooth textures on 3D models (slower) |
| HD packs | HD backgrounds | on | uses an HD background pack in `ux0:data/dw1/hd/` if present |
| HD packs | HD object sprites | off | HD map objects (experimental) |
| HD packs | HD movies | on | uses upscaled FMVs in `ux0:data/dw1/hd/movies/` if present |
| Gameplay | Fair Bonus Try | on | gym slots without the hidden rigging, at half speed |
| Gameplay | Better battle gains | on | battles raise stats according to how your Digimon fought |
| Gameplay | Original bug fixes | on | fixes for bugs of the original game |
| Sound and extras | Audio | Improved | smoother sample interpolation, or Original |
| Sound and extras | Select: debug menu | off | |

In game, Start opens a pause menu (save, return to title).

## Controls

PS1 layout on the Vita buttons. Also: left stick moves like the D-pad, touch
the screen to walk / pet your Digimon, L1 for quick feeding.

## Reporting bugs

Please include `ux0:data/dw1/log.txt` (it is rewritten on every launch), what
you were doing, and a screenshot if possible.

## Known issues

- Changing area can freeze the game for about a second while HD graphics
  load.
- "Model depth fix" and "3D texture filter" can drop the frame rate.
- Not every part of the game has been played through on this port: expect
  bugs, and keep backups of your saves.

## Credits

- [jype0/dw_decomp](https://github.com/jype0/dw_decomp) (MIT) and
  [SydMontague](https://github.com/SydMontague/DW1-Code)'s reverse engineering
- [PSY-Z](https://github.com/Xeeynamo/psyz) (MIT) by Xeeynamo and contributors
- [vitaGL](https://github.com/Rinnegatamante/vitaGL) by Rinnegatamante
- [DW2-PC](https://github.com/Wyrelade/DW2-PC) (CC0)

*Digimon World* and its data belong to their owners.
