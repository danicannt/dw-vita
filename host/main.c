/* Native entry point: replaces the PS1 boot code (game/src/main/_psstart.c).
 *
 * Usage: dw1 [path/to/Digimon World (USA).cue]
 * The game reads its files through PSY-Z's emulated CD drive from the user's
 * own disc image; nothing from the game ships with the port. */
#include <stdio.h>
#include <psyz.h>

int dw1_game_main(void);

#ifndef DW1_DEFAULT_DISC
#define DW1_DEFAULT_DISC "Digimon World (USA).cue"
#endif

int main(int argc, char **argv)
{
    const char *disc = argc > 1 ? argv[1] : DW1_DEFAULT_DISC;

    if (Psyz_CdSetDiskPath(disc) < 0) {
        fprintf(stderr, "dw1: cannot open disc image '%s'\n", disc);
        fprintf(stderr, "usage: %s [disc.cue]\n", argv[0]);
        return 1;
    }
    return dw1_game_main();
}
