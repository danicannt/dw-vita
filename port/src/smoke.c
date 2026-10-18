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
 *   DW1_SMOKE=evos[:a-b]      evolve the partner to every EVOLUTION_STATS_GAINS
 *                             target a..b (default 0..65) the way the game
 *                             does (tamer state 6, partner state 13): loads each
 *                             model and plays the whole sequence
 *   DW1_SMOKE=battles[:a-b]   visit maps a..b like "maps" and fight every
 *                             Digimon on each one (startBattle per entity slot)
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
void tamerSetState(int state);
void partnerSetState(int state);
signed char startBattle(short instanceId);
extern short EVOLUTION_TARGET;
int dw1_file_queue_idle(void);
extern void *ENTITY_TABLE[];

enum { SMOKE_OFF, SMOKE_WARP, SMOKE_MAPS, SMOKE_EVOS, SMOKE_BATTLES };

static int mode = -1;
static int warp_map, warp_exit;
static int map_cur, map_end, frames_per_map, frame;
static int active;   /* a driver owns the pad */
static int visited;  /* the battery has entered its first map */
static int settle;   /* idle frames since the last evolution finished */
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
    } else if ((s = getenv("DW1_SMOKE")) != NULL) {
        size_t n;

        if (strncmp(s, "maps", 4) == 0) {
            mode = SMOKE_MAPS;
            n = 4;
            map_end = 254;
        } else if (strncmp(s, "battles", 7) == 0) {
            mode = SMOKE_BATTLES;
            n = 7;
            map_end = 254;
        } else if (strncmp(s, "evos", 4) == 0) {
            mode = SMOKE_EVOS;
            n = 4;
            map_end = 65;
        } else {
            return;
        }
        map_cur = 0;
        if (s[n] == ':') {
            map_cur = atoi(s + n + 1);
            s = strchr(s + n + 1, '-');
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
    case SMOKE_EVOS:
        active = 1;
        /* wait for the previous evolution (EVOLUTION_TARGET back to -1), then
         * for the loads it queued on its way out (map tiles) and a short
         * breather: back-to-back evolutions never happen in play, and a queued
         * read landing after the next one's synchronous TIM read overwrites it */
        if (visited) {
            if (EVOLUTION_TARGET != -1 || tamerGetState() != 0 || !dw1_file_queue_idle()) {
                settle = 0;
                if (++frame < 6000)
                    break;
                fprintf(stderr, "dw1: smoke: model %d still busy, next anyway\n", map_cur);
            } else if (++settle < 90) {
                break;
            }
        }
        settle = 0;
        /* target 62 is Digimon 62 ("wEAG"), which has no model on the disc:
         * no evolution in the game leads there, and waiting for its MMD
         * never ends (on the PS1 too) */
        if (++map_cur == 62)
            ++map_cur;
        if (map_cur > map_end) {
            fprintf(stderr, "dw1: smoke: done\n");
            exit(0);
        }
        visited = 1;
        frame = 0;
        fprintf(stderr, "dw1: smoke: model %d evolution target\n", map_cur);
        fflush(stderr);
        EVOLUTION_TARGET = (short)map_cur;
        tamerSetState(6);
        partnerSetState(13);
        break;
    case SMOKE_BATTLES:
        active = 1;
        if (++frame < frames_per_map / 2)
            break;
        if (visited && tamerGetState() != 0 && frame < frames_per_map + 1200)
            break;
        if (visited) {
            int slot;

            /* everyone the map put in the entity table besides tamer and partner */
            for (slot = 2; slot < 10; slot++) {
                if (ENTITY_TABLE[slot] == NULL)
                    continue;
                fprintf(stderr, "dw1: smoke: fight map %d slot %d\n", map_cur, slot);
                fflush(stderr);
                startBattle((short)slot);
            }
        }
        visited = 1;
        frame = 0;
        if (!next_map()) {
            fprintf(stderr, "dw1: smoke: done\n");
            exit(0);
        }
        fprintf(stderr, "dw1: smoke: battle %d %.10s\n", map_cur, MAP_ENTRIES[map_cur].filename);
        fflush(stderr);
        changeMap((short)map_cur, 0);
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
