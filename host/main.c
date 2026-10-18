/* Native entry point: replaces the PS1 boot code (game/src/main/_psstart.c).
 *
 * Usage: dw1 [path/to/Digimon World (USA).cue]
 *
 * The game reads its files through PSY-Z's emulated CD drive from the user's
 * own disc image; nothing from the game ships with the port. Memory cards are
 * card1.mcd / card2.mcd (raw 128 KB images) in the data directory. */
#include <stdio.h>
#include <stdlib.h>
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

    dw1_crash_handler_install();
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

#if defined(__linux__)
    /* SDL's HIDAPI retries dlopen("libusb") on every joystick poll when there
     * is no i386 libusb; the kernel hidraw path is enough for pads */
    setenv("SDL_HIDAPI_LIBUSB", "0", 0);
#endif
    /* DW1_TURBO=1: no frame pacing, for fast replays (they count pad reads,
     * not time, so they play the same at any speed) */
    if (getenv("DW1_TURBO") != NULL) {
        Psyz_VideoSetVsyncMode(PSYZ_VSYNC_LIMITLESS);
    }
    /* recordings and replays: the SPU advances per frame, not per audio
     * callback, so voice timing (which game logic polls) is the same live,
     * in turbo and without a device */
    if (getenv("DW1_RECORD") != NULL || getenv("DW1_REPLAY") != NULL) {
        Psyz_AudioSetFrameLocked(1);
    }
    /* picture options: DW1_RES=2..8 internal resolution (default 1, the PS1's
     * 320x240), DW1_DITHER=0 no PS1 dithering, DW1_ASPECT=square 1:1 pixels */
    if (getenv("DW1_RES") != NULL) {
        Psyz_VideoSetInternalResolution((unsigned)atoi(getenv("DW1_RES")));
    }
    if (getenv("DW1_DITHER") != NULL && atoi(getenv("DW1_DITHER")) == 0) {
        Psyz_VideoSetDitheringMode(PSYZ_DITHER_OFF);
    }
    if (getenv("DW1_ASPECT") != NULL && strcmp(getenv("DW1_ASPECT"), "square") == 0) {
        Psyz_VideoSetAspectMode(PSYZ_ASPECT_SQUARE);
    }
    dw1_disc_set_path(disc);
    dw1_memcard_set_dir(DW1_DATA_DIR);
    dw1_overlay_init();
    return dw1_game_main();
}
