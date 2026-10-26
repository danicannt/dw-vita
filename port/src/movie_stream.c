/* STR movie streaming (CdRead2 + St* ring functions) for the movie player.
 *
 * PSY-Z's libcd streams only XA audio (CdlReadS with CdlModeRT) and its
 * StCdInterrupt is a stub, so StGetNext never returns a frame and MOV_REL
 * spins forever. Here StGetNext reads the movie's sectors straight from the
 * disc image at the pace of a 2x drive (150 sectors per second), skips the
 * XA audio sectors, joins each frame's video chunks into the ring and hands
 * back the STR header. CdRead2 itself still runs, so PSY-Z plays the movie's
 * XA audio from the same position. (After DW2-PC's psyq/libcd.c, CC0.)
 *
 * Hooked with the linker's --wrap (CdControl, CdRead2, St*); PSY-Z untouched. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <libcd.h>

#include "dw1_native.h"

int __real_CdControl(u_char com, u_char *param, u_char *result);
int __real_CdRead2(long mode);

#define RAW_SECTOR   2352
#define STR_CHUNK    2016   /* video bytes per sector after the 32-byte STR header */
#define STR_SPEED    150    /* sectors per second at 2x */
#define STR_GIVE_UP  64     /* sectors in a row without video: the movie is over */

static char bin_path[512];
static FILE *bin;

static int setloc_lba = -1; /* last CdlSetloc: where CdRead2 starts */
static u_long *ring;
static int ring_bytes;
static int on;              /* a stream runs */
static int lba0, pos;       /* first sector, next sector from it */
static int misses;          /* sectors in a row without video */
static double t0;
static u_long header[8];
static int frames;          /* frames delivered by this stream */
static int turbo = -1;      /* DW1_TURBO: no drive pacing */

/* the first FILE of the .cue; a .bin given directly is used as is */
void dw1_disc_set_path(const char *cue)
{
    const char *dot = strrchr(cue, '.');
    const char *slash = strrchr(cue, '/');
    char line[600];
    FILE *f;

    snprintf(bin_path, sizeof(bin_path), "%s", cue);
    if (dot == NULL || (strcmp(dot, ".cue") != 0 && strcmp(dot, ".CUE") != 0))
        return;
    f = fopen(cue, "r");
    if (f == NULL)
        return;
    while (fgets(line, sizeof(line), f) != NULL) {
        char *q0 = strchr(line, '"');
        char *q1 = q0 ? strchr(q0 + 1, '"') : NULL;

        if (strstr(line, "FILE") == NULL || q1 == NULL)
            continue;
        *q1 = '\0';
        if (q0[1] == '/' || slash == NULL)
            snprintf(bin_path, sizeof(bin_path), "%s", q0 + 1);
        else
            snprintf(bin_path, sizeof(bin_path), "%.*s/%s", (int)(slash - cue), cue, q0 + 1);
        break;
    }
    fclose(f);
}

/* the 2336 bytes after the sync and header: subheader (8) then data */
static int read_sector(int lba, unsigned char *out)
{
    if (bin == NULL && (bin = fopen(bin_path, "rb")) == NULL) {
        fprintf(stderr, "dw1: movie: cannot open '%s'\n", bin_path);
        return -1;
    }
    if (fseek(bin, (long)lba * RAW_SECTOR + 16, SEEK_SET) != 0 || fread(out, 1, 2336, bin) != 2336)
        return -1;
    return 0;
}

static double now(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static int bcd(int v)
{
    return (v >> 4) * 10 + (v & 15);
}

int __wrap_CdControl(u_char com, u_char *param, u_char *result)
{
    if (com == CdlSetloc && param != NULL)
        setloc_lba = (bcd(param[0]) * 60 + bcd(param[1])) * 75 + bcd(param[2]) - 150;
    return __real_CdControl(com, param, result);
}

int __wrap_CdRead2(long mode)
{
    if ((mode & CdlModeStream) && ring != NULL && setloc_lba >= 0) {
        on = 1;
        lba0 = setloc_lba;
        pos = 0;
        misses = 0;
        frames = 0;
        t0 = now();
        fprintf(stderr, "dw1: movie: streaming from sector %d\n", lba0);
    }
    return __real_CdRead2(mode);
}

void __wrap_StSetRing(u_long *ring_addr, u_long ring_size)
{
    ring = ring_addr;
    ring_bytes = (int)ring_size * 2048;
}

void __wrap_StUnSetRing(void)
{
    if (on)
        fprintf(stderr, "dw1: movie: stopped after %d frames, %.1f s\n", frames, now() - t0);
    ring = NULL;
    on = 0;
}

void __wrap_StSetStream(u_long mode, u_long start_frame, u_long end_frame,
                        void (*func1)(), void (*func2)())
{
    (void)mode; (void)start_frame; (void)end_frame; (void)func1; (void)func2;
}

u_long __wrap_StFreeRing(u_long *base)
{
    (void)base; /* each frame is joined at the ring start and decoded at once */
    return 0;
}

int __wrap_StGetBackloc(CdlLOC *loc)
{
    (void)loc;
    return 0;   /* the player then restarts from the file start */
}

/* 0 with the next frame, 1 when the stream is not running */
u_long __wrap_StGetNext(u_long **addr, u_long **hdr)
{
    static unsigned char body[2336];

    if (!on)
        return 1;
    if (turbo < 0)
        turbo = getenv("DW1_TURBO") != NULL;
    for (;;) {
        const unsigned char *d = body + 8;
        double due = t0 + (double)pos / STR_SPEED;
        double wait = due - now();
        int cn, cc;

        if (wait > 0 && !turbo) {
            struct timespec ts;

            ts.tv_sec = (time_t)wait; /* tv_nsec must stay below 1 s */
            ts.tv_nsec = (long)((wait - (double)ts.tv_sec) * 1e9);
            nanosleep(&ts, NULL);
        }
        if (misses >= STR_GIVE_UP || read_sector(lba0 + pos, body) < 0) {
            header[2] = 0x7FFFFFFF; /* a frame number past the end: the player stops */
            *addr = ring;
            *hdr = header;
            return 0;
        }
        pos++;
        if ((body[2] & 0x04) || d[0] != 0x60 || d[1] != 0x01 || d[2] != 0x01 || d[3] != 0x80) {
            misses++; /* XA audio or not an STR video sector */
            continue;
        }
        misses = 0;
        cn = d[4] | d[5] << 8;
        cc = d[6] | d[7] << 8;
        if ((cn + 1) * STR_CHUNK > ring_bytes)
            continue;
        memcpy((unsigned char *)ring + cn * STR_CHUNK, d + 32, STR_CHUNK);
        if (cn == 0)
            memcpy(header, d, sizeof(header));
        if (cn == cc - 1) {
            if (++frames % 50 == 0)
                fprintf(stderr, "dw1: movie: frame %d at %.1f s\n", frames, now() - t0);
            *addr = ring;
            *hdr = header;
            return 0;
        }
    }
}
