/* Port options: the improvements the native port adds over the PS1, each one
 * on or off, chosen in the title screen's Options menu
 * (port/game/options_menu.c) and kept in <data dir>/options.ini
 * (ux0:data/dw1/options.ini on the Vita):
 *
 *   key             values    environment variable that forces it
 *   widescreen      0 1       DW1_WIDE=extra[:scale] (0: off)
 *   hd_backgrounds  0 1       DW1_HD_BG=0|1
 *   hd_movies       0 1       DW1_HD_MOVIES=0|1
 *   pgxp            0 1       DW1_PGXP=0|1
 *   model_depth     0 1       DW1_PGXP_DEPTH=0|1
 *   texture_filter  0 1 2     DW1_TEX3D=0|1|2
 *   fair_slots      0 1       DW1_SLOTS_ORIGINAL=1 (off) / 0 (on)
 *   battle_gains    0 1       DW1_BATTLE_GAINS=original (off) / anything (on)
 *   original_fixes  0 1       DW1_ORIGINAL_FIXES=0|1
 *   hd_sprites      0 1       DW1_HD_SPRITES=0|1
 *   debug_menu      0 1       DW1_DEBUG_MENU=0|1 (Select: port/game/pause_menu.c)
 *
 * An option whose variable is set takes the variable's value for that run and
 * cannot be changed in the menu; the rest start from the file, or from the
 * defaults below when it has no line for them. DW1_OPTIONS=file reads and
 * writes another file. Recordings and replays (DW1_RECORD / DW1_REPLAY)
 * ignore the file unless DW1_OPTIONS names one, so they play the same on any
 * machine.
 *
 * Every option applies at once: the renderer switches widescreen and PGXP on
 * the fly, the HD packs and movies are looked up when a map or a movie loads,
 * and the slots and battle gains are read when they are used. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <psyz/video.h>

#include "dw1_native.h"

void dw1_widescreen_set(unsigned extra, unsigned scale);

static const char *const keys[DW1_OPT_COUNT] = {
    "widescreen", "hd_backgrounds", "hd_movies",  "pgxp",
    "model_depth", "texture_filter", "fair_slots", "battle_gains",
    "original_fixes", "hd_sprites", "debug_menu",
};

static int value[DW1_OPT_COUNT];
static int locked[DW1_OPT_COUNT];
static int stored[DW1_OPT_COUNT]; /* from the file or the defaults */
static char path[256];
static unsigned wide_extra = 52, wide_scale = 2; /* 16:9 */

int dw1_option(int id)
{
    return value[id];
}

int dw1_option_count(int id)
{
    return id == DW1_OPT_TEXTURE_FILTER ? 3 : 2;
}

int dw1_option_locked(int id)
{
    return locked[id];
}

static void apply(int id)
{
    switch (id) {
    case DW1_OPT_WIDESCREEN:
        /* off: as without widescreen, no scaling either */
        if (value[id])
            dw1_widescreen_set(wide_extra, wide_scale);
        else
            dw1_widescreen_set(0, 1);
        break;
    case DW1_OPT_PGXP:
        Psyz_VideoSetPgxp(value[id]);
        break;
    case DW1_OPT_MODEL_DEPTH:
        Psyz_VideoSetPgxpDepth(value[id]);
        break;
    case DW1_OPT_TEXTURE_FILTER:
        Psyz_VideoSetModelFilter(value[id]);
        break;
    }
}

void dw1_option_set(int id, int v)
{
    if (locked[id] || v < 0 || v >= dw1_option_count(id))
        return;
    value[id] = v;
    apply(id);
}

/* an environment variable read as a number: 1 when set, its value in *v */
static int env_int(const char *name, int *v)
{
    const char *s = getenv(name);

    if (s == NULL)
        return 0;
    *v = atoi(s);
    return 1;
}

static void force(int id, int v)
{
    int n = dw1_option_count(id);

    value[id] = v < 0 ? 0 : v >= n ? n - 1 : v;
    locked[id] = 1;
}

static void load(void)
{
    char line[128];
    FILE *f = fopen(path, "r");
    int i;

    if (f == NULL)
        return;
    while (fgets(line, sizeof(line), f) != NULL) {
        char *eq = strchr(line, '=');

        if (line[0] == '#' || eq == NULL)
            continue;
        *eq = '\0';
        for (i = 0; i < DW1_OPT_COUNT; i++) {
            if (strcmp(line, keys[i]) == 0) {
                int v = atoi(eq + 1);

                if (v >= 0 && v < dw1_option_count(i))
                    value[i] = v;
            }
        }
    }
    fclose(f);
    fprintf(stderr, "dw1: options from %s\n", path);
}

int dw1_options_save(void)
{
    FILE *f;
    int i;

    if (path[0] == '\0')
        return -1;
    if ((f = fopen(path, "w")) == NULL) {
        fprintf(stderr, "dw1: cannot write the options to %s\n", path);
        return -1;
    }
    fprintf(f, "# Digimon World native port options (title screen > Options)\n");
    for (i = 0; i < DW1_OPT_COUNT; i++)
        fprintf(f, "%s=%d\n", keys[i], locked[i] ? stored[i] : value[i]);
    fclose(f);
    fprintf(stderr, "dw1: options saved to %s\n", path);
    return 0;
}

void dw1_options_init(const char *data_dir)
{
    const char *s;
    int v, i;

    /* defaults: everything on, PGXP on the Vita only. The model depth
     * buffer and texture filter start off everywhere: on the Vita they cost
     * frames (36 fps in towns), and the depth shows where the PS1 models'
     * parts were built sunk into each other (the tamer's backpack into his
     * jacket), which the ordering table hid */
    for (i = 0; i < DW1_OPT_COUNT; i++)
        value[i] = 1;
#ifdef DW1_VITA_WIDE
    value[DW1_OPT_WIDESCREEN] = 1;
#else
    value[DW1_OPT_WIDESCREEN] = 0;
#endif
    value[DW1_OPT_MODEL_DEPTH] = 0;
    value[DW1_OPT_TEXTURE_FILTER] = 0;
    /* HD object sprites start off until they are checked on the Vita */
    value[DW1_OPT_HD_SPRITES] = 0;
    value[DW1_OPT_DEBUG_MENU] = 0;
#ifndef __vita__
    value[DW1_OPT_PGXP] = 0;
#endif

    if ((s = getenv("DW1_OPTIONS")) != NULL)
        snprintf(path, sizeof(path), "%s", s);
    else if (getenv("DW1_RECORD") == NULL && getenv("DW1_REPLAY") == NULL)
        snprintf(path, sizeof(path), "%s/options.ini", data_dir[0] ? data_dir : ".");
    if (path[0] != '\0')
        load();
    memcpy(stored, value, sizeof(stored)); /* what a forced option saves */

    if ((s = getenv("DW1_WIDE")) != NULL) {
        const char *n = strchr(s, ':');

        if (atoi(s) > 0)
            wide_extra = (unsigned)atoi(s);
        if (n != NULL)
            wide_scale = (unsigned)atoi(n + 1);
        force(DW1_OPT_WIDESCREEN, atoi(s) > 0);
    }
    if (env_int("DW1_HD_BG", &v))
        force(DW1_OPT_HD_BACKGROUNDS, v != 0);
    if (env_int("DW1_HD_MOVIES", &v))
        force(DW1_OPT_HD_MOVIES, v != 0);
    if (env_int("DW1_PGXP", &v))
        force(DW1_OPT_PGXP, v != 0);
    if (env_int("DW1_PGXP_DEPTH", &v))
        force(DW1_OPT_MODEL_DEPTH, v != 0);
    if (env_int("DW1_TEX3D", &v))
        force(DW1_OPT_TEXTURE_FILTER, v);
    if (env_int("DW1_SLOTS_ORIGINAL", &v))
        force(DW1_OPT_FAIR_SLOTS, v == 0);
    if ((s = getenv("DW1_BATTLE_GAINS")) != NULL)
        force(DW1_OPT_BATTLE_GAINS, strcmp(s, "original") != 0);
    if (env_int("DW1_ORIGINAL_FIXES", &v))
        force(DW1_OPT_ORIGINAL_FIXES, v != 0);
    if (env_int("DW1_HD_SPRITES", &v))
        force(DW1_OPT_HD_SPRITES, v != 0);
    if (env_int("DW1_DEBUG_MENU", &v))
        force(DW1_OPT_DEBUG_MENU, v != 0);

    for (i = 0; i < DW1_OPT_COUNT; i++)
        apply(i);
    /* without the sdl3_gl renderer there is no widescreen */
    if (value[DW1_OPT_WIDESCREEN] && dw1_wide_extra == 0)
        force(DW1_OPT_WIDESCREEN, 0);
}
