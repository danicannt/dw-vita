/* VRAM dumps for debugging rendering without a screenshot tool.
 *
 *   DW1_DUMP=300,301[,...]   frame numbers (counted in GsSwapDispBuff calls)
 *   DW1_DUMP_VSYNC=600,...   the same counted in VSync calls (movies never
 *                            swap through libgs)
 *   DW1_DUMP_DIR=dir         where vram_<frame>.ppm goes (default: current dir)
 *
 * At each listed frame the whole 1024x512 VRAM is read back just before the
 * buffers swap, so both display buffers and the texture pages are visible.
 * Hooked with the linker's --wrap=GsSwapDispBuff and --wrap=VSync. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libgpu.h>

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

static void dump(long frame)
{
    static u_short vram[1024 * 512];
    static unsigned char rgb[1024 * 512 * 3];
    const char *dir = getenv("DW1_DUMP_DIR");
    char path[512];
    RECT r;
    FILE *f;
    int i;

    setRECT(&r, 0, 0, 1024, 512);
    DrawSync(0);
    StoreImage(&r, (u_long *)vram);
    DrawSync(0);
    for (i = 0; i < 1024 * 512; i++) {
        rgb[i * 3 + 0] = (unsigned char)((vram[i] & 31) << 3);
        rgb[i * 3 + 1] = (unsigned char)(((vram[i] >> 5) & 31) << 3);
        rgb[i * 3 + 2] = (unsigned char)(((vram[i] >> 10) & 31) << 3);
    }
    snprintf(path, sizeof(path), "%s%svram_%05ld.ppm", dir ? dir : "", dir ? "/" : "", frame);
    f = fopen(path, "wb");
    if (f == NULL) {
        fprintf(stderr, "dw1: cannot write %s\n", path);
        return;
    }
    fprintf(f, "P6\n1024 512\n255\n");
    fwrite(rgb, 1, sizeof(rgb), f);
    fclose(f);
    fprintf(stderr, "dw1: dumped %s\n", path);
}

void __wrap_GsSwapDispBuff(void)
{
    if (due(&by_swap, "DW1_DUMP"))
        dump(by_swap.count);
    __real_GsSwapDispBuff();
}

int __wrap_VSync(int mode)
{
    if (mode >= 0 && due(&by_vsync, "DW1_DUMP_VSYNC"))
        dump(-by_vsync.count); /* negative: numbered by VSync */
    return __real_VSync(mode);
}
