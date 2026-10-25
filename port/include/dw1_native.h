/* Force-included in every game translation unit (see CMakeLists.txt).
 *
 * Keep it small: native fixes belong next to the code they fix, marked
 * DW1_NATIVE. This header only carries what the whole game needs. */
#ifndef DW1_NATIVE_H
#define DW1_NATIVE_H

#include <psyz.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- PS1 main RAM ----------------------------------------------------------
 * The game uses a few fixed PS1 addresses as buffers (GENERAL_BUFFER at
 * 0x80010000, TEXTURE_BUFFER, the battle model area, two ordering tables, ...),
 * all in the RAM the retail executable leaves free. Natively they live in one
 * 2 MB arena laid out like PS1 RAM, so offsets between them stay as on the PS1.
 * The heap (InitHeap3) is placed in the same arena at its retail address. */
#define DW1_PS1_RAM_SIZE 0x200000
extern unsigned char dw1_ps1_ram[DW1_PS1_RAM_SIZE];
#define DW1_PS1_ADDR(addr) ((void *)(dw1_ps1_ram + ((unsigned long)(addr) & (DW1_PS1_RAM_SIZE - 1))))

/* ---- scratchpad ------------------------------------------------------------
 * 1 KB of fast RAM at 0x1F800000 on the PS1. */
#define DW1_SCRATCHPAD_SIZE 0x400
extern unsigned char dw1_scratchpad[DW1_SCRATCHPAD_SIZE];
#ifndef getScratchAddr
#define getScratchAddr(offset) ((unsigned long *)(dw1_scratchpad + (offset) * 4))
#endif

/* ---- overlays --------------------------------------------------------------
 * The PS1 loads one *_REL.BIN overlay at a time over the same memory. Natively
 * every overlay is linked into the executable; loading one restores its data
 * to the initial state, as a fresh load from disc did (port/src/overlay.c). */
void dw1_overlay_init(void);
void dw1_overlay_reset(int lib);

/* ---- movies ----------------------------------------------------------------
 * STR video is streamed straight from the disc image (port/src/movie_stream.c);
 * host/main.c passes it the same .cue PSY-Z mounts. */
void dw1_disc_set_path(const char *cue);

/* ---- division ---------------------------------------------------------------
 * DW1 was built without zero-division checks and relies on what the R3000
 * gives: quotient -1 (1 for a negative dividend, all ones unsigned) and the
 * dividend as remainder; INT_MIN / -1 stays INT_MIN. x86 traps and ARM returns
 * 0, so sites that divide by zero use these (crash_handler.c logs new ones).
 * The Vita's libgcc gives INT_MAX / INT_MIN (0 for 0 / 0) and remainder 0. */
static inline int dw1_div(int a, int b)
{
    if (b == 0)
        return a >= 0 ? -1 : 1;
    if (b == -1 && a == (int)0x80000000u)
        return a;
    return a / b;
}
static inline unsigned int dw1_divu(unsigned int a, unsigned int b)
{
    return b == 0 ? 0xFFFFFFFFu : a / b;
}
static inline int dw1_mod(int a, int b)
{
    if (b == 0)
        return a;
    if (b == -1)
        return 0;
    return a % b;
}
static inline unsigned int dw1_modu(unsigned int a, unsigned int b)
{
    return b == 0 ? a : a % b;
}

/* ---- crashes ---------------------------------------------------------------
 * Linux: print the call stack on SIGSEGV and friends (port/src/crash_handler.c). */
void dw1_crash_handler_install(void);

/* ---- test drivers ----------------------------------------------------------
 * DW1_WARP / DW1_SMOKE (port/src/smoke.c), stepped from the top of gameLoop. */
void dw1_smoke_frame(void);

/* ---- widescreen and native backgrounds --------------------------------------
 * port/src/widescreen.c. dw1_wide_extra: game pixels added on each side of
 * the 320-wide screen (0 in 4:3). The background functions take the map's
 * tiles (NULL image: black tile) and queue it in the ordering table; with
 * no renderer support dw1_bg_set returns 0 and dw1_bg_queue NULL. */
extern int dw1_wide_extra;
void dw1_widescreen_init(const char *data_dir);
int dw1_bg_set(const char *name, int tiles_w, int tiles_h, unsigned char *const *tile_images);
void dw1_bg_clear(void);
void *dw1_bg_queue(void *ot_entry, void *work, int x, int y, unsigned clut);
/* margins an HD pack adds around the map (original pixels), 0 without one;
 * returns whether a native background is active */
int dw1_wide_margins(int *left, int *right);
/* an upscaled FMV from <hd dir>/movies when there is one: 1 played to the
 * end, 0 skipped with Start, -1 none (play the original) */
int dw1_play_hd_movie(int movie_id, int skippable);

/* ---- file queue ------------------------------------------------------------
 * game/src/main/file_queue.c: let synchronous reads wait for queued ones. */
void dw1_file_queue_drain(void);

#ifdef __cplusplus
}
#endif

#endif
