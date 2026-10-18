/* Native entry point: replaces the PS1 boot code (game/src/main/_psstart.c).
 *
 * Usage: dw1 [path/to/Digimon World (USA).cue]
 *
 * The game reads its files through PSY-Z's emulated CD drive from the user's
 * own disc image; nothing from the game ships with the port. Memory cards are
 * card1.mcd / card2.mcd (raw 128 KB images) in the data directory. */
#include <stdio.h>
#include <string.h>

#include <psyz.h>

#include "dw1_memcard.h"
#include "dw1_native.h"

int dw1_game_main(void);

#if defined(__vita__)
#define DW1_DATA_DIR "ux0:data/dw1"
#else
#define DW1_DATA_DIR ""
#endif

#ifndef DW1_DEFAULT_DISC
#define DW1_DEFAULT_DISC "Digimon World (USA).cue"
#endif

int main(int argc, char **argv)
{
    char disc[512];

    if (argc > 1) {
        snprintf(disc, sizeof(disc), "%s", argv[1]);
    } else if (DW1_DATA_DIR[0] != '\0') {
        snprintf(disc, sizeof(disc), "%s/%s", DW1_DATA_DIR, DW1_DEFAULT_DISC);
    } else {
        snprintf(disc, sizeof(disc), "%s", DW1_DEFAULT_DISC);
    }

    if (Psyz_CdSetDiskPath(disc) < 0) {
        fprintf(stderr, "dw1: cannot open the disc image '%s'\n", disc);
        fprintf(stderr, "usage: %s [\"Digimon World (USA).cue\"]\n", argv[0]);
        return 1;
    }

    dw1_disc_set_path(disc);
    dw1_memcard_set_dir(DW1_DATA_DIR);
    dw1_overlay_init();
    return dw1_game_main();
}
