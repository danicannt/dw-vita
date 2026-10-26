/* VRAM dumps for debugging rendering without a screenshot tool.
 *
 *   DW1_DUMP=300,301[,...]   frame numbers (counted in GsSwapDispBuff calls)
 *   DW1_DUMP_VSYNC=600,...   the same counted in VSync calls (movies never
 *                            swap through libgs)
 *   DW1_DUMP_DIR=dir         where vram_<frame>.ppm goes (default: current dir)
 *   DW1_SHOT_EVERY=n         every n swaps, the frame as presented (the
 *                            renderer's capture: the wide canvas in
 *                            widescreen) as shot_<frame>.ppm; DW1_SHOT_MAX
 *                            caps how many (default 30), DW1_SHOT_FROM
 *                            skips the frames before it
 *
 * At each listed frame the whole 1024x512 VRAM is read back just before the
 * buffers swap, so both display buffers and the texture pages are visible.
 * Hooked with the linker's --wrap=GsSwapDispBuff and --wrap=VSync. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libgpu.h>
#include <psyz/audio.h>
#include <psyz/video.h>

void __real_GsSwapDispBuff(void);
int __real_VSync(int mode);

struct trigger {
    int parsed;
    long frames[64];
    int nframes;
    long count;
};

static struct trigger by_swap, by_vsync;

static int due(struct trigger *t, const char *var)
{
    int i;

    if (!t->parsed) {
        const char *s = getenv(var);
        char *end;

        t->parsed = 1;
        while (s != NULL && *s != '\0' && t->nframes < (int)(sizeof(t->frames) / sizeof(t->frames[0]))) {
            t->frames[t->nframes++] = strtol(s, &end, 10);
            if (end == s)
                break;
            s = (*end == ',') ? end + 1 : end;
        }
    }
    t->count++;
    for (i = 0; i < t->nframes; i++)
        if (t->frames[i] == t->count)
            return 1;
    return 0;
}

/* a VRAM rectangle from (0,0) as DW1_DUMP_DIR/<name>.ppm; vram and rgb hold
 * w * h pixels */
static void dump_vram(int w, int h, u_short *vram, unsigned char *rgb, const char *name, int log)
{
    const char *dir = getenv("DW1_DUMP_DIR");
    char path[512];
    RECT r;
    FILE *f;
    int i;

    setRECT(&r, 0, 0, w, h);
    DrawSync(0);
    StoreImage(&r, (u_long *)vram);
    DrawSync(0);
    for (i = 0; i < w * h; i++) {
        rgb[i * 3 + 0] = (unsigned char)((vram[i] & 31) << 3);
        rgb[i * 3 + 1] = (unsigned char)(((vram[i] >> 5) & 31) << 3);
        rgb[i * 3 + 2] = (unsigned char)(((vram[i] >> 10) & 31) << 3);
    }
    snprintf(path, sizeof(path), "%s%s%s.ppm", dir ? dir : "", dir ? "/" : "", name);
    f = fopen(path, "wb");
    if (f == NULL) {
        fprintf(stderr, "dw1: cannot write %s\n", path);
        return;
    }
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    fwrite(rgb, 1, (size_t)w * h * 3, f);
    fclose(f);
    if (log)
        fprintf(stderr, "dw1: dumped %s\n", path);
}

static void dump(long frame)
{
    static u_short vram[1024 * 512];
    static unsigned char rgb[1024 * 512 * 3];
    char name[32];

    snprintf(name, sizeof(name), "vram_%05ld", frame);
    dump_vram(1024, 512, vram, rgb, name, 1);
}

/* port/src/smoke.c's walk battery: both display buffers (320x480) to
 * DW1_DUMP_DIR/<name>.ppm at the next swap */
static char named_request[64];

void dw1_dump_request(const char *name)
{
    snprintf(named_request, sizeof(named_request), "%s", name);
}

static void dump_named(const char *name)
{
    static u_short vram[320 * 480];
    static unsigned char rgb[320 * 480 * 3];

    dump_vram(320, 480, vram, rgb, name, 0);
}

static void shot(long frame)
{
    static int every = -1, max = 30, taken;
    static long from;
    const char *dir = getenv("DW1_DUMP_DIR");
    char path[512];
    unsigned char *rgb;
    int w, h;
    FILE *f;

    if (every < 0) {
        const char *s = getenv("DW1_SHOT_EVERY");

        every = s ? atoi(s) : 0;
        if ((s = getenv("DW1_SHOT_MAX")) != NULL)
            max = atoi(s);
        if ((s = getenv("DW1_SHOT_FROM")) != NULL)
            from = atol(s);
    }
    if (every <= 0 || frame < from || frame % every != 0 || taken >= max)
        return;
    rgb = Psyz_VideoAllocCapturedFrame(&w, &h);
    if (rgb == NULL)
        return;
    taken++;
    snprintf(path, sizeof(path), "%s%sshot_%05ld.ppm", dir ? dir : "", dir ? "/" : "", frame);
    f = fopen(path, "wb");
    if (f != NULL) {
        fprintf(f, "P6\n%d %d\n255\n", w, h);
        fwrite(rgb, 1, (size_t)w * h * 3, f);
        fclose(f);
        fprintf(stderr, "dw1: shot %s (%dx%d)\n", path, w, h);
    }
    free(rgb);
}

void __wrap_GsSwapDispBuff(void)
{
    shot(by_swap.count + 1);
    if (named_request[0] != '\0') {
        dump_named(named_request);
        named_request[0] = '\0';
    }
    if (due(&by_swap, "DW1_DUMP"))
        dump(by_swap.count);
    __real_GsSwapDispBuff();
}

int __wrap_VSync(int mode)
{
    if (mode >= 0) {
        if (due(&by_vsync, "DW1_DUMP_VSYNC"))
            dump(-by_vsync.count); /* negative: numbered by VSync */
        /* frame-locked audio (recordings / replays): one frame per VSync, or
         * the n VBlanks VSync(n) waits for */
        Psyz_AudioFrame(mode > 1 ? mode : 1);
    }
    return __real_VSync(mode);
}
