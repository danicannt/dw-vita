/* Widescreen and native map backgrounds.
 *
 *   DW1_WIDE=extra[:scale]   widescreen: extra game pixels on each side of
 *                            the 320-wide screen (52 fills 16:9), drawn at
 *                            scale x resolution (default 2). The Vita build
 *                            turns it on by default (DW1_VITA_WIDE). It is
 *                            the "widescreen" option (port/src/options.c),
 *                            like the HD packs and movies below.
 *   DW1_HD_DIR=dir           where <MAP>.hdm background packs are looked
 *                            for (default: <data dir>/hd)
 *
 * The renderer (Psyz_VideoSetWidescreen) draws the display buffers into
 * wider canvases; the game, under DW1_NATIVE, draws the map background, culls
 * and places its HUD for that width (dw1_wide_extra).
 *
 * The map background is drawn natively instead of as the 128x128 tiles the
 * PS1 streamed through a 4x3 ring in VRAM: the whole map, as palette indices
 * looked up in the map's CLUT (VRAM row 480, so day, dusk and night still
 * apply), queued in the ordering table as an application GP0 command. The
 * indices come from the map's own tiles, or from an HD pack:
 *
 *   <MAP>.hdm  "DWHD", u16 version 1, u16 scale, u16 extLeft, extRight,
 *              extTop, extBottom (original pixels added around the map),
 *              u16 width, height (bitmap pixels), u8 indices[width*height]
 *              rows top to bottom, indices into the map's day palette
 *
 * Without the renderer's support (other renderers) the game keeps drawing
 * the tiles.
 *
 * Movies: <hd dir>/movies/OP1.mp4 (OP2, ED2, EDR) replaces that FMV when it
 * exists (an upscaled version), played by the renderer
 * (Psyz_VideoPlayMovie). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libetc.h>
#include <libgpu.h>
#include <psyz/gpu.h>
#include <psyz/video.h>

#include "dw1_native.h"

#define BG_OPCODE 0xF7 /* GP0 opcode no PS1 command uses */
#define BG_SLOT 0

int dw1_wide_extra;

static char data_dir[200] = ".";
static int bg_active;      /* a background is registered for this map */
static int bg_w, bg_h;     /* map size in original pixels */
static int bg_ext[4];      /* left, right, top, bottom margins (original px) */

static int bg_command(const u_long *words, int available, void *userdata)
{
    (void)userdata;
    if (available < 4)
        return 0;
    Psyz_VideoDrawIndexedImage(BG_SLOT, (short)(words[1] & 0xFFFF), (short)(words[1] >> 16),
                               (int)(words[2] & 0xFFFF), (int)(words[2] >> 16),
                               (unsigned)(words[3] & 0xFFFF));
    return 4;
}

void dw1_widescreen_init(const char *dir)
{
    snprintf(data_dir, sizeof(data_dir), "%s", dir[0] ? dir : ".");
    Psyz_GpuRegisterCommandHandler(BG_OPCODE, bg_command, NULL);
}

/* the widescreen option (port/src/options.c); 0 extra: 4:3 */
void dw1_widescreen_set(unsigned extra, unsigned scale)
{
    if (Psyz_VideoSetWidescreen(extra, scale) == 0)
        dw1_wide_extra = (int)extra;
    else
        dw1_wide_extra = 0;
}

int dw1_wide_margins(int *left, int *right)
{
    *left = bg_active ? bg_ext[0] : 0;
    *right = bg_active ? bg_ext[1] : 0;
    return bg_active;
}

static unsigned short rd16(const unsigned char *p)
{
    return (unsigned short)(p[0] | (p[1] << 8));
}

/* DW1_HD_DIR, or <data dir>/hd */
static const char *hd_dir(char *out, size_t size)
{
    const char *dir = getenv("DW1_HD_DIR");

    if (dir != NULL)
        snprintf(out, size, "%s", dir);
    else
        snprintf(out, size, "%s/hd", data_dir);
    return out;
}

/* <MAP>.hdm, NULL when there is none or it does not fit the map */
static unsigned char *load_pack(const char *name, int w, int h, int *pw, int *ph, int *ext)
{
    char path[sizeof(data_dir) + 32];
    unsigned char head[20];
    unsigned char *rgba = NULL, *row = NULL;
    FILE *f;
    int scale, i, x, y;

    {
        char d[sizeof(data_dir) + 4]; /* <data dir>/hd */

        snprintf(path, sizeof(path), "%s/%s.hdm", hd_dir(d, sizeof(d)), name);
    }
    if (!dw1_option(DW1_OPT_HD_BACKGROUNDS) || (f = fopen(path, "rb")) == NULL)
        return NULL;
    if (fread(head, 1, sizeof(head), f) != sizeof(head) || memcmp(head, "DWHD", 4) != 0 ||
        rd16(head + 4) != 1)
        goto bad;
    scale = rd16(head + 6);
    for (i = 0; i < 4; i++)
        ext[i] = rd16(head + 8 + 2 * i);
    *pw = rd16(head + 16);
    *ph = rd16(head + 18);
    if (scale < 1 || *pw != (w + ext[0] + ext[1]) * scale || *ph != (h + ext[2] + ext[3]) * scale)
        goto bad;
    rgba = malloc((size_t)*pw * *ph * 4);
    row = malloc((size_t)*pw);
    if (rgba == NULL || row == NULL)
        goto bad;
    for (y = 0; y < *ph; y++) {
        if (fread(row, 1, (size_t)*pw, f) != (size_t)*pw)
            goto bad;
        for (x = 0; x < *pw; x++) {
            unsigned char *p = rgba + ((size_t)y * *pw + x) * 4;

            p[0] = row[x];
            p[1] = p[2] = 0;
            p[3] = 255;
        }
    }
    free(row);
    fclose(f);
    fprintf(stderr, "dw1: background %s: HD pack x%d, margins %d %d %d %d\n", name, scale,
            ext[0], ext[1], ext[2], ext[3]);
    return rgba;
bad:
    fprintf(stderr, "dw1: background %s: ignoring %s (not a pack for this map)\n", name, path);
    free(rgba);
    free(row);
    fclose(f);
    return NULL;
}

int dw1_bg_set(const char *name, int tiles_w, int tiles_h, unsigned char *const *tile_images)
{
    int w = tiles_w * 128, h = tiles_h * 128;
    int pw = w, ph = h, ext[4] = {0, 0, 0, 0};
    unsigned char *rgba;
    char clean[16];
    int i;

    bg_active = 0;
    /* the file name field is not always terminated (10 chars) */
    for (i = 0; i < 10 && name[i] != '\0' && name[i] != ' '; i++)
        clean[i] = name[i];
    clean[i] = '\0';
    rgba = load_pack(clean, w, h, &pw, &ph, ext);
    if (rgba == NULL) {
        int tx, ty, x, y;

        rgba = malloc((size_t)w * h * 4);
        if (rgba == NULL)
            return 0;
        for (ty = 0; ty < tiles_h; ty++) {
            for (tx = 0; tx < tiles_w; tx++) {
                const uint8_t *src = tile_images[tx + ty * tiles_w];

                for (y = 0; y < 128; y++) {
                    unsigned char *p = rgba + (((size_t)(ty * 128 + y) * w) + tx * 128) * 4;

                    for (x = 0; x < 128; x++, p += 4) {
                        p[0] = src ? src[y * 128 + x] : 0;
                        p[1] = p[2] = 0;
                        p[3] = src ? 255 : 0; /* no tile: black */
                    }
                }
            }
        }
    }
    if (Psyz_VideoSetIndexedImage(BG_SLOT, (unsigned)pw, (unsigned)ph, rgba) == 0) {
        bg_active = 1;
        bg_w = w;
        bg_h = h;
        memcpy(bg_ext, ext, sizeof(bg_ext));
    }
    free(rgba);
    return bg_active;
}

void dw1_bg_clear(void)
{
    if (bg_active)
        Psyz_VideoSetIndexedImage(BG_SLOT, 0, 0, NULL);
    bg_active = 0;
}

/* the background command and its tag (PSY-Z's tag is two words, address and
 * length, where the PS1 packed both in one); `x`, `y` are where the map's
 * top-left corner lands, in primitive coordinates */
typedef struct {
    u_long addr, len;
    u_long cmd, xy, wh, clut;
} BgPacket;

void *dw1_bg_queue(void *ot_entry, void *work, int x, int y, unsigned clut)
{
    BgPacket *p = work;

    if (!bg_active)
        return NULL;
    x -= bg_ext[0];
    y -= bg_ext[2];
    setlen(p, 4);
    p->cmd = (u_long)BG_OPCODE << 24;
    p->xy = (u_long)((x & 0xFFFF) | ((y & 0xFFFF) << 16));
    p->wh = (u_long)((bg_w + bg_ext[0] + bg_ext[1]) | ((bg_h + bg_ext[2] + bg_ext[3]) << 16));
    p->clut = clut;
    AddPrim(ot_entry, p);
    return p + 1;
}

static int skip_on_start(void)
{
    return (PadRead(1) & 0x800) != 0; /* Start, as MOV_playMovie checks */
}

int dw1_play_hd_movie(int movie_id, int skippable)
{
    static const char *const names[] = {"OP1", "OP2", "ED2", "EDR"};
    char dir[sizeof(data_dir) + 4], path[sizeof(data_dir) + 32];

    if (movie_id < 0 || movie_id > 3 || !dw1_option(DW1_OPT_HD_MOVIES))
        return -1;
    snprintf(path, sizeof(path), "%s/movies/%s.mp4", hd_dir(dir, sizeof(dir)), names[movie_id]);
    return Psyz_VideoPlayMovie(path, skippable ? skip_on_start : NULL);
}

int dw1_slots_original(void)
{
    return !dw1_option(DW1_OPT_FAIR_SLOTS);
}
