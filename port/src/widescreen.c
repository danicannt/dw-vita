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

#include <SDL3/SDL.h>

#include <libetc.h>
#include <libgpu.h>
#include <psyz/audio.h>
#include <psyz/gpu.h>
#include <psyz/video.h>

#include "dw1_native.h"

#define BG_OPCODE 0xF7 /* GP0 opcode no PS1 command uses */
#define BG_SLOT 0

int dw1_wide_extra;

#define TITLE_OPCODE 0xF5 /* HD title screens, below */
static int title_command(const u_long *words, int available, void *userdata);

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

/* ---- HD object sprites: <hd dir>/<MAP>.hdo (fondos/objetos/FORMATO_HDO.md) ----
 * The map's overlay sprites (doors, animated objects, snow) drawn from HD
 * index pages, one per VRAM page of the map. The overlay code queues each
 * sprite as a GP0 command in its ordering-table place, and the renderer draws
 * it from the page (Psyz_VideoDrawSpritePage). Option hd_sprites. */
#define SPR_OPCODE 0xF6 /* GP0 opcode no PS1 command uses */
#define HDO_MAX_PAGES 32
#define HDO_NONE 0xFFFF

typedef struct {
    unsigned short page; /* HDO_NONE: no HD sprite for this object */
    unsigned short u, v, w, h; /* 1x rectangle in its page */
    unsigned char transparency; /* as obj->transparency: 4 opaque, else blend mode */
} HdoTemplate;

static HdoTemplate *hdo_tmpl;
static int hdo_ntmpl;
static int hdo_npages; /* pages uploaded for this map */
static int hdo_scale;
static int hdo_active;

/* the sprite command: tag, then cmd, xy, wh, clut, page (page | abr << 16 |
 * semi << 20), uv and size of the texels in HD pixels */
typedef struct {
    u_long addr, len;
    u_long cmd, xy, wh, clut, pg, uv, sz;
} SpritePacket;

static int sprite_command(const u_long *words, int available, void *userdata)
{
    unsigned pg;

    (void)userdata;
    if (available < 7)
        return 0;
    pg = (unsigned)words[4];
    Psyz_VideoDrawSpritePage(pg & 0xFFFF, (short)(words[1] & 0xFFFF), (short)(words[1] >> 16),
                             (int)(words[2] & 0xFFFF), (int)(words[2] >> 16),
                             (unsigned)(words[3] & 0xFFFF),
                             (int)(words[5] & 0xFFFF), (int)(words[5] >> 16),
                             (int)(words[6] & 0xFFFF), (int)(words[6] >> 16),
                             (pg >> 20) & 1, (pg >> 16) & 3);
    return 7;
}

void dw1_widescreen_init(const char *dir)
{
    snprintf(data_dir, sizeof(data_dir), "%s", dir[0] ? dir : ".");
    Psyz_GpuRegisterCommandHandler(BG_OPCODE, bg_command, NULL);
    Psyz_GpuRegisterCommandHandler(SPR_OPCODE, sprite_command, NULL);
    Psyz_GpuRegisterCommandHandler(TITLE_OPCODE, title_command, NULL);
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

/* <MAP>.hdm, NULL when there is none or it does not fit the map.
 * The indices come in with one fread from an unbuffered stream, which newlib
 * reads straight into the buffer in one call: through its 1 KB stdio buffer
 * the Vita made a card read per KB (595 ms for a 2.3 MB pack, the whole game
 * stopped on every map change) */
static unsigned char *load_pack(const char *name, int w, int h, int *pw, int *ph, int *ext)
{
    char path[sizeof(data_dir) + 32];
    unsigned char head[20];
    unsigned char *rgba = NULL;
    FILE *f;
    int scale, i;
    size_t n, k;
    Uint64 t0 = SDL_GetTicksNS(), t1;

    {
        char d[sizeof(data_dir) + 4]; /* <data dir>/hd */

        snprintf(path, sizeof(path), "%s/%s.hdm", hd_dir(d, sizeof(d)), name);
    }
    if (!dw1_option(DW1_OPT_HD_BACKGROUNDS) || (f = fopen(path, "rb")) == NULL)
        return NULL;
    setvbuf(f, NULL, _IONBF, 0);
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
    n = (size_t)*pw * *ph;
    rgba = malloc(n * 4);
    if (rgba == NULL)
        goto bad;
    /* the indices go in the last quarter and spread forwards in place:
     * pixel k is written at 4k..4k+3, below index k+1 at 3n+k+1 */
    if (fread(rgba + n * 3, 1, n, f) != n)
        goto bad;
    t1 = SDL_GetTicksNS();
    for (k = 0; k < n; k++) {
        unsigned char *p = rgba + k * 4;

        p[0] = rgba[n * 3 + k];
        p[1] = p[2] = 0;
        p[3] = 255;
    }
    fclose(f);
    fprintf(stderr, "dw1: background %s: HD pack x%d, margins %d %d %d %d (read %.1f ms, convert %.1f ms)\n",
            name, scale, ext[0], ext[1], ext[2], ext[3], (double)(t1 - t0) / 1e6,
            (double)(SDL_GetTicksNS() - t1) / 1e6);
    return rgba;
bad:
    fprintf(stderr, "dw1: background %s: ignoring %s (not a pack for this map)\n", name, path);
    free(rgba);
    fclose(f);
    return NULL;
}

static void hdo_clear(void)
{
    int i;

    for (i = 0; i < hdo_npages; i++)
        Psyz_VideoSetSpritePage((unsigned)i, 0, 0, NULL);
    free(hdo_tmpl);
    hdo_tmpl = NULL;
    hdo_ntmpl = hdo_npages = hdo_scale = hdo_active = 0;
}

static unsigned rd32(const unsigned char *p)
{
    return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

/* <MAP>.hdo for the map's overlay sprites, replacing the previous map's.
 * Returns 1 when there is one (and the option is on). */
int dw1_hdo_load(const char *name)
{
    char path[sizeof(data_dir) + 32], clean[16], d[sizeof(data_dir) + 4];
    unsigned char *file = NULL, *rgba = NULL;
    const unsigned char *dir, *tab;
    long size;
    FILE *f;
    int i, k, npage, ntmpl, scale;

    hdo_clear();
    if (!dw1_option(DW1_OPT_HD_SPRITES))
        return 0;
    for (i = 0; i < 10 && name[i] != '\0' && name[i] != ' '; i++)
        clean[i] = name[i];
    clean[i] = '\0';
    snprintf(path, sizeof(path), "%s/%s.hdo", hd_dir(d, sizeof(d)), clean);
    if ((f = fopen(path, "rb")) == NULL)
        return 0;
    if (fseek(f, 0, SEEK_END) != 0 || (size = ftell(f)) < 12 || fseek(f, 0, SEEK_SET) != 0 ||
        (file = malloc((size_t)size)) == NULL || fread(file, 1, (size_t)size, f) != (size_t)size)
        goto bad;
    fclose(f);
    f = NULL;
    if (memcmp(file, "DWHO", 4) != 0 || rd16(file + 4) != 1)
        goto bad;
    scale = rd16(file + 6);
    npage = rd16(file + 8);
    ntmpl = rd16(file + 10);

    if ((scale != 2 && scale != 4) || npage > HDO_MAX_PAGES ||
        12 + 20 * (long)npage + 12 * (long)ntmpl > size)
        goto bad;
    hdo_tmpl = malloc(sizeof(*hdo_tmpl) * (size_t)(ntmpl > 0 ? ntmpl : 1));
    if (hdo_tmpl == NULL)
        goto bad;
    dir = file + 12;
    tab = dir + 20 * npage;
    for (i = 0; i < npage; i++) {
        const unsigned char *e = dir + 20 * i;
        int w = scale * rd16(e + 8), h = scale * rd16(e + 10);
        unsigned off = rd32(e + 16);
        long n = (long)w * h;

        if (w <= 0 || h <= 0 || (long)off + 2 * n > size)
            goto bad;
        rgba = malloc((size_t)n * 4);
        if (rgba == NULL)
            goto bad;
        for (k = 0; k < n; k++) {
            rgba[4 * k] = file[off + k];           /* index */
            rgba[4 * k + 1] = rgba[4 * k + 2] = 0;
            rgba[4 * k + 3] = file[off + n + k];   /* 0 or 255 */
        }
        if (Psyz_VideoSetSpritePage((unsigned)i, (unsigned)w, (unsigned)h, rgba) != 0) {
            free(rgba);
            rgba = NULL;
            goto bad;
        }
        free(rgba);
        rgba = NULL;
        hdo_npages = i + 1;
    }
    for (k = 0; k < ntmpl; k++) {
        const unsigned char *t = tab + 12 * k;

        hdo_tmpl[k].page = rd16(t);
        hdo_tmpl[k].u = rd16(t + 2);
        hdo_tmpl[k].v = rd16(t + 4);
        hdo_tmpl[k].w = rd16(t + 6);
        hdo_tmpl[k].h = rd16(t + 8);
        hdo_tmpl[k].transparency = t[10];
        if (hdo_tmpl[k].page != HDO_NONE && hdo_tmpl[k].page >= npage)
            goto bad;
    }
    hdo_ntmpl = ntmpl;
    hdo_scale = scale;
    hdo_active = 1;
    fprintf(stderr, "dw1: object sprites %s: HD x%d, %d pages, %d objects\n", clean, scale, npage, ntmpl);
    free(file);
    return 1;
bad:
    fprintf(stderr, "dw1: object sprites %s: ignoring %s (not a pack for this map)\n", clean, path);
    if (f != NULL)
        fclose(f);
    free(rgba);
    free(file);
    hdo_clear();
    return 0;
}

/* queue the sprite of object `tmpl` (its HD texels) at x,y, w x h in primitive
 * coordinates, colours from `clut`; NULL when there is no HD sprite. The
 * caller continues from the returned work pointer. */
void *dw1_hdo_sprite_queue(void *ot_entry, void *work, int tmpl, int x, int y, int w, int h,
                           unsigned clut)
{
    const HdoTemplate *t;
    SpritePacket *p = work;

    if (!hdo_active || tmpl < 0 || tmpl >= hdo_ntmpl)
        return NULL;
    t = &hdo_tmpl[tmpl];
    if (t->page == HDO_NONE)
        return NULL;
    setlen(p, 7);
    p->cmd = (u_long)SPR_OPCODE << 24;
    p->xy = (u_long)((x & 0xFFFF) | ((y & 0xFFFF) << 16));
    p->wh = (u_long)((w & 0xFFFF) | ((h & 0xFFFF) << 16));
    p->clut = clut;
    p->pg = (u_long)t->page | ((u_long)(t->transparency & 3) << 16) |
            ((u_long)(t->transparency != 4) << 20);
    p->uv = (u_long)(t->u * hdo_scale) | ((u_long)(t->v * hdo_scale) << 16);
    p->sz = (u_long)(t->w * hdo_scale) | ((u_long)(t->h * hdo_scale) << 16);
    AddPrim(ot_entry, p);
    return p + 1;
}

int dw1_bg_set(const char *name, int tiles_w, int tiles_h, unsigned char *const *tile_images)
{
    int w = tiles_w * 128, h = tiles_h * 128;
    int pw = w, ph = h, ext[4] = {0, 0, 0, 0};
    unsigned char *rgba;
    char clean[16];
    int i;

    bg_active = 0;
    dw1_title_clear(); /* the title screens are over */
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

/* Start (as MOV_playMovie checks), Cross or Circle, pressed during the movie:
 * a button still held from the menu that started it does not count */
#define MOVIE_SKIP_BUTTONS (PADstart | PADRdown | PADRright)
static int movie_skip_held;

void Psyz_PadsPoll(void); /* PSY-Z platform layer */

static int skip_on_button(void)
{
    int held, pressed;

    /* PSY-Z samples the pads once per VSync, and nothing calls VSync while a
     * movie plays: without a fresh poll PadRead keeps the state from before */
    Psyz_PadsPoll();
    held = (PadRead(1) & MOVIE_SKIP_BUTTONS) != 0;
    pressed = held && !movie_skip_held;

    movie_skip_held = held;
    return pressed;
}

int dw1_play_hd_movie(int movie_id, int skippable)
{
    static const char *const names[] = {"OP1", "OP2", "ED2", "EDR"};
    char dir[sizeof(data_dir) + 4], path[sizeof(data_dir) + 32];

    if (movie_id < 0 || movie_id > 3 || !dw1_option(DW1_OPT_HD_MOVIES))
        return -1;
    snprintf(path, sizeof(path), "%s/movies/%s.mp4", hd_dir(dir, sizeof(dir)), names[movie_id]);
    /* PSY-Z starts its audio when a CdlSetmode asks for XA, which the original
     * movie does at boot: an HD movie in its place has to start it instead */
    Psyz_AudioInit();
    movie_skip_held = 1; /* until a poll sees every skip button released */
    return Psyz_VideoPlayMovie(path, skippable ? skip_on_button : NULL);
}

int dw1_slots_original(void)
{
    return !dw1_option(DW1_OPT_FAIR_SLOTS);
}

/* ---- HD title screens: <hd dir>/<TIM>.hdt ----------------------------------
 * The Press Start screen (TITLE256) and the main menu (TITLE2) backgrounds,
 * redrawn in true colour and widened (dw1-tools/titulo_hd.py):
 *
 *   <TIM>.hdt  "DWHT", u16 version 1, u16 scale, u16 extLeft, extRight,
 *              extTop, extBottom (original pixels added around the 320x240
 *              screen), u16 width, height (bitmap pixels), then RGB bytes
 *              rows top to bottom
 *
 * The game queues it where it drew the two quads of the TIM; the Press Start
 * text keeps coming from the TIM in VRAM. Only in widescreen wide enough for
 * the pack's margins (the art moved into them), with the HD backgrounds
 * option. Loaded on the first frame that draws it, freed when a map loads. */
#define TITLE_SLOT 1 /* indexed-image slot (BG_SLOT is the map) */

static char title_name[12]; /* what title_slot holds or failed to load */
static int title_ok;
static int title_ext[4];

static int title_command(const u_long *words, int available, void *userdata)
{
    (void)userdata;
    if (available < 3)
        return 0;
    Psyz_VideoDrawIndexedImage(TITLE_SLOT, (short)(words[1] & 0xFFFF), (short)(words[1] >> 16),
                               (int)(words[2] & 0xFFFF), (int)(words[2] >> 16), 0);
    return 3;
}

void dw1_title_clear(void)
{
    if (title_ok)
        Psyz_VideoSetDirectImage(TITLE_SLOT, 0, 0, NULL);
    title_ok = 0;
    title_name[0] = '\0';
}

static void title_load(const char *name)
{
    char path[sizeof(data_dir) + 32], d[sizeof(data_dir) + 4];
    unsigned char head[20], *rgba = NULL;
    int scale, w, h, i;
    size_t n, k;
    FILE *f;
    Uint64 t0 = SDL_GetTicksNS();

    dw1_title_clear();
    snprintf(title_name, sizeof(title_name), "%s", name);
    snprintf(path, sizeof(path), "%s/%s.hdt", hd_dir(d, sizeof(d)), name);
    if ((f = fopen(path, "rb")) == NULL)
        return;
    setvbuf(f, NULL, _IONBF, 0); /* one read straight into the buffer (load_pack) */
    if (fread(head, 1, sizeof(head), f) != sizeof(head) || memcmp(head, "DWHT", 4) != 0 ||
        rd16(head + 4) != 1)
        goto bad;
    scale = rd16(head + 6);
    for (i = 0; i < 4; i++)
        title_ext[i] = rd16(head + 8 + 2 * i);
    w = rd16(head + 16);
    h = rd16(head + 18);
    if (scale < 1 || w != (320 + title_ext[0] + title_ext[1]) * scale ||
        h != (240 + title_ext[2] + title_ext[3]) * scale)
        goto bad;
    n = (size_t)w * h;
    rgba = malloc(n * 4);
    if (rgba == NULL)
        goto bad;
    /* RGB in the last three quarters, spread forwards in place: pixel k is
     * written at 4k..4k+3, below pixel k+1 at n+3k+3 */
    if (fread(rgba + n, 1, n * 3, f) != n * 3)
        goto bad;
    for (k = 0; k < n; k++) {
        unsigned char *p = rgba + k * 4, *s = rgba + n + k * 3;
        unsigned char r = s[0], g = s[1], b = s[2];

        p[0] = r;
        p[1] = g;
        p[2] = b;
        p[3] = 255;
    }
    fclose(f);
    if (Psyz_VideoSetDirectImage(TITLE_SLOT, (unsigned)w, (unsigned)h, rgba) == 0)
        title_ok = 1;
    free(rgba);
    fprintf(stderr, "dw1: title %s: HD x%d %dx%d, margins %d %d (%.1f ms)%s\n", name, scale, w, h,
            title_ext[0], title_ext[1], (double)(SDL_GetTicksNS() - t0) / 1e6,
            title_ok ? "" : ", no renderer support");
    return;
bad:
    fprintf(stderr, "dw1: title %s: ignoring %s (not a title pack)\n", name, path);
    free(rgba);
    fclose(f);
}

typedef struct {
    u_long addr, len;
    u_long cmd, xy, wh;
} TitlePacket;

void *dw1_title_queue(void *ot_entry, void *work, const char *name)
{
    TitlePacket *p = work;

    if (!dw1_option(DW1_OPT_HD_BACKGROUNDS)) {
        dw1_title_clear();
        return NULL;
    }
    if (strcmp(name, title_name) != 0)
        title_load(name);
    if (!title_ok || dw1_wide_extra < title_ext[0] || dw1_wide_extra < title_ext[1])
        return NULL;
    setlen(p, 3);
    p->cmd = (u_long)TITLE_OPCODE << 24;
    p->xy = (u_long)(((-160 - title_ext[0]) & 0xFFFF) | (((-120 - title_ext[2]) & 0xFFFF) << 16));
    p->wh = (u_long)((320 + title_ext[0] + title_ext[1]) | ((240 + title_ext[2] + title_ext[3]) << 16));
    AddPrim(ot_entry, p);
    return p + 1;
}
