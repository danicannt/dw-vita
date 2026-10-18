/* Test drivers that put the game into situations directly instead of playing
 * there. They run once gameplay has started (dw1_smoke_frame() is called at
 * the top of every gameLoop) and, when a DW1_REPLAY is set, only after it has
 * finished, so a short recording that just continues a saved game gets them
 * into the world.
 *
 *   DW1_WARP=map[:exit]       change to that map once (the game's changeMap)
 *   DW1_SMOKE=maps[:a[-b]]    visit every map with a file name, ids a..b
 *                             (default 0..254), DW1_SMOKE_FRAMES frames each
 *                             (default 300), pressing X now and then to get
 *                             through dialogue; logs "dw1: smoke: map N NAME"
 *                             before each one and exits when done
 *
 * Anything a map breaks (crash stack, division by zero, unsupported GPU
 * command) lands in the log after its "smoke: map" line; dw1-run/smoke.sh
 * restarts after a crash from the next map. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void changeMap(short mapId, short exitId);
int tamerGetState(void);
void buildMapPath(char *out, char *name, signed char *suffix, int mapId);
int lookupFileTable(void *lookup, char *path);
extern signed char MAP_FILE_EXT_MAP[];
unsigned char *getScript(unsigned short id);
unsigned char *getScriptSection(unsigned char *script, unsigned short section);

/* game/include/dw/map.h */
typedef struct {
    char filename[10];
    signed char num8bppImages;
    signed char num4bppImages;
    signed char flags;
    signed char doorsId;
    signed char toiletId;
    signed char loadingName;
} SmokeMapEntry;
extern SmokeMapEntry MAP_ENTRIES[255];

int dw1_replay_running(void);

enum { SMOKE_OFF, SMOKE_WARP, SMOKE_MAPS };

static int mode = -1;
static int warp_map, warp_exit;
static int map_cur, map_end, frames_per_map, frame;
static int active;   /* a driver owns the pad */
static int visited;  /* the battery has entered its first map */
static unsigned long pad_tick;

static void parse(void)
{
    const char *s;

    mode = SMOKE_OFF;
    if ((s = getenv("DW1_WARP")) != NULL) {
        mode = SMOKE_WARP;
        warp_map = atoi(s);
        s = strchr(s, ':');
        warp_exit = s ? atoi(s + 1) : 0;
    } else if ((s = getenv("DW1_SMOKE")) != NULL && strncmp(s, "maps", 4) == 0) {
        mode = SMOKE_MAPS;
        map_cur = 0;
        map_end = 254;
        if (s[4] == ':') {
            map_cur = atoi(s + 5);
            s = strchr(s + 5, '-');
            if (s)
                map_end = atoi(s + 1);
        }
        s = getenv("DW1_SMOKE_FRAMES");
        frames_per_map = s ? atoi(s) : 300;
        map_cur--; /* advanced before the first visit */
        frame = frames_per_map;
    }
}

static int next_map(void)
{
    while (++map_cur <= map_end) {
        char path[32], lookup[32];

        if (MAP_ENTRIES[map_cur].filename[0] == '\0')
            continue;
        /* unused entries (YAKA01, YAKA21) have no file on the disc */
        buildMapPath(path, MAP_ENTRIES[map_cur].filename, MAP_FILE_EXT_MAP, map_cur);
        if (!lookupFileTable(lookup, path)) {
            fprintf(stderr, "dw1: smoke: skip %d (no %s)\n", map_cur, path);
            continue;
        }
        /* maps the game only shows from an event have no head script, or one
         * that returns into the event's script stack (TOPN01, 238): entered
         * with changeMap they run from address 0 */
        if (getScriptSection(getScript(0), (unsigned short)map_cur) == NULL || map_cur == 238) {
            fprintf(stderr, "dw1: smoke: skip %d (no head script)\n", map_cur);
            continue;
        }
        return 1;
    }
    return 0;
}

/* pad state a driver wants, or -1 to leave the pad alone */
int dw1_smoke_pad(void)
{
    if (!active)
        return -1;
    pad_tick++;
    return (pad_tick % 40) < 3 ? 0x40 : 0; /* tap X */
}

void dw1_smoke_frame(void)
{
    if (mode < 0)
        parse();
    if (mode == SMOKE_OFF || dw1_replay_running())
        return;

    switch (mode) {
    case SMOKE_WARP:
        fprintf(stderr, "dw1: warp: map %d exit %d\n", warp_map, warp_exit);
        mode = SMOKE_OFF;
        changeMap((short)warp_map, (short)warp_exit);
        break;
    case SMOKE_MAPS:
        active = 1;
        if (++frame < frames_per_map)
            break;
        /* change only while the tamer is free: not mid-battle, event or script
         * (map_cur is still first - 1 before the first visit: nothing to wait for) */
        if (visited && tamerGetState() != 0 && frame < frames_per_map + 1200)
            break;
        if (visited && tamerGetState() != 0)
            fprintf(stderr, "dw1: smoke: map %d still busy, changing anyway\n", map_cur);
        visited = 1;
        frame = 0;
        if (!next_map()) {
            fprintf(stderr, "dw1: smoke: done\n");
            exit(0);
        }
        fprintf(stderr, "dw1: smoke: map %d %.10s\n", map_cur, MAP_ENTRIES[map_cur].filename);
        fflush(stderr);
        changeMap((short)map_cur, 0);
        break;
    }
}
