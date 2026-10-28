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
/* HD object sprites: <hd dir>/<MAP>.hdo for the map (1 when there is one and
 * the option is on); dw1_hdo_sprite_queue queues object `tmpl` at x,y,w,h with
 * the CLUT of its primitive and returns the next work pointer, or NULL when
 * the object has no HD sprite (draw the primitive then) */
int dw1_hdo_load(const char *name);
void *dw1_hdo_sprite_queue(void *ot_entry, void *work, int tmpl, int x, int y, int w, int h,
                           unsigned clut);
/* HD title screens: queue <hd dir>/<name>.hdt ("TITLE256", "TITLE2") over
 * the whole screen and return the next work pointer, or NULL (draw the TIM);
 * dw1_title_clear frees it (a map loading does too) */
void *dw1_title_queue(void *ot_entry, void *work, const char *name);
/* fade.c: 1 while a fade to black covers the whole screen */
int dw1_fade_black(void);
void dw1_title_clear(void);
/* margins an HD pack adds around the map (original pixels), 0 without one;
 * returns whether a native background is active */
int dw1_wide_margins(int *left, int *right);
/* an upscaled FMV from <hd dir>/movies when there is one: 1 played to the
 * end, 0 skipped with Start, -1 none (play the original) */
int dw1_play_hd_movie(int movie_id, int skippable);
/* the Bonus Try slots cheat as on the PS1: the fair_slots option off, or
 * DW1_SLOTS_ORIGINAL=1 (default: fair, game/src/trn/trn_slots.c) */
int dw1_slots_original(void);

/* ---- port options ----------------------------------------------------------
 * port/src/options.c: each improvement on or off, from the title screen's
 * Options menu (port/game/options_menu.c), saved in <data dir>/options.ini;
 * an environment variable forces an option for one run. */
enum {
    DW1_OPT_WIDESCREEN,     /* 16:9 */
    DW1_OPT_HD_BACKGROUNDS, /* <hd dir>/<MAP>.hdm */
    DW1_OPT_HD_MOVIES,      /* <hd dir>/movies */
    DW1_OPT_PGXP,           /* no polygon wobble */
    DW1_OPT_MODEL_DEPTH,    /* depth buffer for the models (with PGXP) */
    DW1_OPT_TEXTURE_FILTER, /* model textures: 0 nearest, 1 3-point, 2 bilinear */
    DW1_OPT_FAIR_SLOTS,     /* Bonus Try slots do not cheat (dw1_slots_original) */
    DW1_OPT_BATTLE_GAINS,   /* battles raise stats by how they went */
    DW1_OPT_ORIGINAL_FIXES, /* bugs of the original game fixed (dw1_fix) */
    DW1_OPT_HD_SPRITES,     /* <hd dir>/<MAP>.hdo: object sprites in HD */
    DW1_OPT_DEBUG_MENU,     /* Select in the field opens the debug menu */
    DW1_OPT_COUNT
};
void dw1_options_init(const char *data_dir);
int dw1_option(int id);
/* fixes for bugs of the original game, all behind one option (on by
 * default): each site is marked "original bug" in game/src */
#define dw1_fix() dw1_option(DW1_OPT_ORIGINAL_FIXES)
int dw1_option_count(int id);  /* number of values: 2 (off, on) or 3 */
int dw1_option_locked(int id); /* forced by its environment variable */
void dw1_option_set(int id, int value); /* applies it at once */
int dw1_options_save(void);
/* the title screen's Options menu, run from main_menu.c */
void dw1_options_menu_open(void);
int dw1_options_menu_tick(void); /* 0 once it closes */
void dw1_options_menu_render(void);
/* Start / Select during play (port/game/pause_menu.c), from handlePause:
 * opens the pause or debug menu for a new press; returns the pad state when
 * it closes (0: nothing opened) */
unsigned dw1_pause_menu_poll(unsigned pressed, unsigned held);
int dw1_quit_to_title(void); /* "Return to title" was chosen; clears it */
extern int dw1_cheat_freeze_time, dw1_cheat_no_aging; /* debug menu */
void dw1_cheats_tick(void); /* once a frame: No aging holds lifetime and age */

/* ---- file queue ------------------------------------------------------------
 * game/src/main/file_queue.c: let synchronous reads wait for queued ones. */
void dw1_file_queue_drain(void);

/* ---- touch, stick and quick feeding ----------------------------------------
 * port/src/touch_input.c reads the host side (stick, touch panel, mouse);
 * port/game/touch_controls.c acts on it in the overworld.
 *   DW1_STICK=0      the left stick does not move like the D-pad
 *   DW1_TOUCH=0      no touch controls (petting, walking to the touch)
 *   DW1_QUICKFEED=0  L1 does not open the inventory to feed the partner
 * dw1_touch_state: DW1_TOUCH_DOWN while touched, plus DW1_TOUCH_BEGAN on the
 * first read of a touch; *x, *y in game screen pixels, (0, 0) the centre
 * (getEntityScreenPos's space; with widescreen x reaches 160 + extra). */
#define DW1_TOUCH_DOWN 1
#define DW1_TOUCH_BEGAN 2
void dw1_host_input(unsigned int *pad, int id); /* from __wrap_PadRead */
int dw1_touch_state(int *x, int *y);
int dw1_pad_manual_move(void);  /* D-pad or stick held on the real pad */
int dw1_stick_dir(int *x, int *y); /* the stick's exact direction, if it moved */
extern unsigned int dw1_touch_pad; /* bits OR'd into the next PadRead */
int dw1_quick_feed_enabled(void);
void dw1_touch_walk_tick(void);    /* game/src/main/tamer.c, walking state */
int dw1_quick_feed_open(void);     /* tamer.c, on L1: 1 if it opened */
extern int dw1_quick_feed;         /* the inventory was opened with L1 */

#ifdef __cplusplus
}
#endif

#endif
