/* Start and Select during play (in place of the PS1's "PAUSE" box, from
 * handlePause in game/src/main/utils2.c).
 *
 * Start opens the pause menu:
 *   Continue         back to the game (Start again does the same)
 *   Save             the save machine's "Do you want to save?" (without its
 *                    battle registration), anywhere on the field
 *   Options          the Options menu (port/game/options_menu.c)
 *   Return to title  after a confirmation, without saving; main() in
 *                    game/src/main/main.c then cleans up as after the ending
 *
 * Select opens the debug menu when its option is on (Options > Sound and extras):
 *   Freeze time      the clock stops (game/src/main/clock.c)
 *   No aging         lifetime and age stop (dw1_cheats_tick)
 *   Heal partner     HP and MP full, no injury, sickness or tiredness
 *   Happy, obedient  happiness and discipline 100
 *   +10000 bits
 *
 * While a menu is open the game is frozen: each frame redraws the scene
 * (renderObjects, without ticking) under the menu. One PadRead per frame,
 * like the PS1's pause loop, so recorded pad scripts keep their timing. */
#include <libetc.h>
#include <libgpu.h>
#include <libgs.h>

#include <dw/entity.h>
#include <dw/input.h>
#include <dw/params.h>
#include <dw/script.h>
#include <dw/sound.h>
#include <dw/tamer.h>
#include <dw/types.h>
#include <dw/world_object.h>

#include "port_menu.h"

extern int32_t ACTIVE_FRAMEBUFFER;
extern GsOT GS_ORDERING_TABLE[];
extern PACKET GS_WORK_BASES[2][0x14000];
extern DR_OFFSET DR_OFFSETS[2];
extern GsOT *ACTIVE_ORDERING_TABLE;
extern GsRVIEW2 GS_VIEWPOINT;
extern int32_t VIEWPORT_DISTANCE;
extern int32_t DRAWING_OFFSET_X;
extern int32_t DRAWING_OFFSET_Y;
extern int32_t CHANGED_INPUT;
extern int32_t POLLED_INPUT;
extern int8_t GAME_STATE;
extern int32_t CURRENT_MENU;
extern int32_t TARGET_MENU;
void pollInputMenu(void);
void tickMainMenu(void);
void renderMainMenu(void);
void openSaveMachine(void);

int dw1_cheat_freeze_time, dw1_cheat_no_aging;

static int quit_requested;

/* No aging: the partner's remaining lifetime never goes down and its age
 * never up, whatever takes them (the clock, sleep, training, resting...);
 * rises in lifetime (items) and a new life (age 0) are kept */
static int16_t held_lifetime, held_age;

void dw1_cheats_tick(void)
{
    if (!dw1_cheat_no_aging) {
        return;
    }
    if (PARTNER_PARA.remainingLifetime < held_lifetime)
        PARTNER_PARA.remainingLifetime = held_lifetime;
    if (PARTNER_PARA.age > held_age)
        PARTNER_PARA.age = held_age;
    held_lifetime = PARTNER_PARA.remainingLifetime;
    held_age = PARTNER_PARA.age;
}

int dw1_quit_to_title(void)
{
    int q = quit_requested;

    quit_requested = 0;
    if (q) /* the cheats belong to this game, not the next one loaded */
        dw1_cheat_freeze_time = dw1_cheat_no_aging = 0;
    return q;
}

/* one frame of a menu over the frozen scene */
static void frame(void (*draw)(void))
{
    ACTIVE_FRAMEBUFFER = GsGetActiveBuff();
    ACTIVE_ORDERING_TABLE = &GS_ORDERING_TABLE[ACTIVE_FRAMEBUFFER];
    GsSetWorkBase(GS_WORK_BASES[ACTIVE_FRAMEBUFFER]);
    GsClearOt(0, 0, ACTIVE_ORDERING_TABLE);
    GsSetOrign(DRAWING_OFFSET_X, DRAWING_OFFSET_Y);
    GsSetRefView2(&GS_VIEWPOINT);
    GsSetProjection(VIEWPORT_DISTANCE);
    renderObjects();
    draw();
    AddPrim(ACTIVE_ORDERING_TABLE->org + 0x20, &DR_OFFSETS[ACTIVE_FRAMEBUFFER]);
    DrawSync(0);
    VSync(2);
    GsSwapDispBuff();
    GsSortClear(0, 0, 0, ACTIVE_ORDERING_TABLE);
    GsDrawOt(ACTIVE_ORDERING_TABLE);
}

/* the screen darkened by level / 31, as renderFade (game/src/main/fade.c)
 * does: a flat subtractive quad over everything */
static int fade_level;

static void draw_fade(void)
{
    POLY_F4 *flat = (POLY_F4 *)GsGetWorkBase();
    DR_TPAGE *mode;
    int c = fade_level > 31 ? 31 : fade_level;

    setPolyF4(flat);
    setSemiTrans(flat, 1);
    setRGB0(flat, c << 3, c << 3, c << 3);
    setXYWH(flat, -160 - dw1_wide_extra, -120, 320 + 2 * dw1_wide_extra, 240);
    addPrim(ACTIVE_ORDERING_TABLE->org, flat);
    mode = (DR_TPAGE *)&flat[1]; /* added after the quad so it runs first */
    setDrawTPage(mode, 1, 1, GetTPage(0, 2, 896, 448));
    addPrim(ACTIVE_ORDERING_TABLE->org, mode);
    GsSetWorkBase((PACKET *)&mode[1]);
}

/* "Return to title": fade out with the game still frozen (running the game
 * loop to fade would let the tamer walk into a battle or a door meanwhile);
 * main() cleans up right after this frame */
static void fade_out(void)
{
    for (fade_level = 4; fade_level <= 32; fade_level += 4)
        frame(draw_fade);
}

/* ---- the text area ---------------------------------------------------------
 * The menus write their strings over the game's (a textbox, the map name);
 * it is put back when they close. */
#define TEXT_RECT_X 704
#define TEXT_RECT_Y 256
static u_long saved_text[64 * 256 / 2];

static void save_text(void)
{
    RECT r;

    setRECT(&r, TEXT_RECT_X, TEXT_RECT_Y, 64, 256);
    DrawSync(0);
    StoreImage(&r, saved_text);
    DrawSync(0);
    dw1_menu_invalidate();
}

static void restore_text(void)
{
    RECT r;

    setRECT(&r, TEXT_RECT_X, TEXT_RECT_Y, 64, 256);
    LoadImage(&r, saved_text);
    DrawSync(0);
    dw1_menu_invalidate();
}

/* ---- menus ------------------------------------------------------------------ */
static Dw1Menu menu;

static void draw_menu(void)
{
    dw1_menu_render(&menu);
}

static void draw_options(void)
{
    dw1_options_menu_render();
}

static void draw_save(void)
{
    renderMainMenu();
}

static void set_row(int i, const char *label, const char *value, int enabled)
{
    Dw1MenuRow *r = &menu.rows[i];

    r->label = label;
    r->value = value;
    r->header = 0;
    r->color = enabled ? DW1_MENU_WHITE : DW1_MENU_GREY;
    r->value_color = enabled && value != NULL && value[1] == 'n' ? DW1_MENU_YELLOW : r->color;
}

/* saving and leaving only on the field, with the tamer free */
static int on_field(void)
{
    return GAME_STATE == 0 && tamerGetState() == 0;
}

static void run_options(void)
{
    dw1_options_menu_open();
    do {
        pollInputMenu();
        frame(draw_options);
    } while (dw1_options_menu_tick());
}

static void run_save(void)
{
    openSaveMachine();
    /* straight to "Do you want to save?", past the save machine's battle
     * registration (0x3C) or its "too young to register" (0x46); 0x44, no
     * card chosen yet, stays */
    if (TARGET_MENU == 0x3C || TARGET_MENU == 0x46)
        TARGET_MENU = 0x45;
    do {
        pollInputMenu();
        tickMainMenu();
        frame(draw_save);
    } while (CURRENT_MENU != -1);
    dw1_menu_invalidate();
}

enum { P_CONTINUE, P_SAVE, P_OPTIONS, P_QUIT };

static void build_pause(int confirm)
{
    int field = on_field();

    if (confirm) {
        menu.title = "Return to title?";
        menu.count = 2;
        set_row(0, "No", NULL, 1);
        set_row(1, "Yes, without saving", NULL, 1);
    } else {
        menu.title = "Pause";
        menu.count = 4;
        set_row(P_CONTINUE, "Continue", NULL, 1);
        set_row(P_SAVE, "Save", NULL, field);
        set_row(P_OPTIONS, "Options", NULL, 1);
        set_row(P_QUIT, "Return to title", NULL, field);
    }
    menu.width = 0xA8;
    menu.value_x = 0xA0;
}

static void run_pause(void)
{
    int confirm = 0;

    build_pause(0);
    menu.cursor = 0;
    for (;;) {
        int input;

        dw1_menu_text(&menu);
        pollInputMenu();
        input = CHANGED_INPUT & 0xFFFF;
        if (dw1_menu_move(&menu, input)) {
        } else if (input == DW1_PAD_START && !confirm) {
            playSound(0, 4);
            return;
        } else if (input == CANCEL_BUTTON) {
            playSound(0, 4);
            if (!confirm)
                return;
            confirm = 0;
            build_pause(0);
            menu.cursor = P_QUIT;
        } else if (input == CONFIRM_BUTTON) {
            if (confirm) {
                playSound(0, 3);
                if (menu.cursor == 1) {
                    quit_requested = 1;
                    fade_out();
                    return;
                }
                confirm = 0;
                build_pause(0);
                menu.cursor = P_QUIT;
            } else if (menu.rows[menu.cursor].color == DW1_MENU_GREY) {
                playSound(0, 4);
            } else {
                playSound(0, 3);
                switch (menu.cursor) {
                case P_CONTINUE:
                    return;
                case P_SAVE:
                    run_save();
                    return;
                case P_OPTIONS:
                    run_options();
                    build_pause(0);
                    menu.cursor = P_OPTIONS;
                    break;
                case P_QUIT:
                    confirm = 1;
                    build_pause(1);
                    menu.cursor = 0;
                    break;
                }
            }
        }
        frame(draw_menu);
    }
}

enum { D_FREEZE, D_AGING, D_HEAL, D_HAPPY, D_MONEY, D_BACK };

static void build_debug(void)
{
    menu.title = "Debug";
    menu.count = 6;
    set_row(D_FREEZE, "Freeze time", dw1_cheat_freeze_time ? "On" : "Off", 1);
    set_row(D_AGING, "No aging", dw1_cheat_no_aging ? "On" : "Off", 1);
    set_row(D_HEAL, "Heal partner", NULL, 1);
    set_row(D_HAPPY, "Happy, obedient", NULL, 1);
    set_row(D_MONEY, "+10000 bits", NULL, 1);
    set_row(D_BACK, "Back", NULL, 1);
    menu.width = 0xC8;
    menu.value_x = 0x88;
}

static void heal_partner(void)
{
    Stats *st = &PARTNER_ENTITY.digimonEntity.stats;

    st->current.currentHP = st->base.hp;
    st->current.currentMP = st->base.mp;
    PARTNER_PARA.condition &= ~(CONDITION_TIRED | CONDITION_INJURED | CONDITION_SICK);
    PARTNER_PARA.tiredness = 0;
    PARTNER_PARA.subTiredness = 0;
    PARTNER_PARA.sicknessTimer = 0;
    PARTNER_PARA.injuryTimer = 0;
}

static void run_debug(void)
{
    build_debug();
    menu.cursor = 0;
    for (;;) {
        int input;

        dw1_menu_text(&menu);
        pollInputMenu();
        input = CHANGED_INPUT & 0xFFFF;
        if (dw1_menu_move(&menu, input)) {
        } else if (input == CANCEL_BUTTON || input == DW1_PAD_SELECT ||
                   (input == CONFIRM_BUTTON && menu.cursor == D_BACK)) {
            playSound(0, 4);
            return;
        } else if (input == CONFIRM_BUTTON || input == DW1_PAD_LEFT || input == DW1_PAD_RIGHT) {
            int toggle = menu.cursor == D_FREEZE || menu.cursor == D_AGING;

            if (input != CONFIRM_BUTTON && !toggle) {
                frame(draw_menu);
                continue;
            }
            playSound(0, 3);
            switch (menu.cursor) {
            case D_FREEZE:
                dw1_cheat_freeze_time = !dw1_cheat_freeze_time;
                break;
            case D_AGING:
                dw1_cheat_no_aging = !dw1_cheat_no_aging;
                held_lifetime = PARTNER_PARA.remainingLifetime;
                held_age = PARTNER_PARA.age;
                break;
            case D_HEAL:
                heal_partner();
                break;
            case D_HAPPY:
                PARTNER_PARA.happiness = 100;
                PARTNER_PARA.discipline = 100;
                break;
            case D_MONEY:
                MONEY += 10000;
                if (MONEY > 999999)
                    MONEY = 999999;
                break;
            }
            build_debug();
        }
        frame(draw_menu);
    }
}

unsigned dw1_pause_menu_poll(unsigned pressed, unsigned held)
{
    /* the menus poll with pollInputMenu, whose edges are against
     * POLLED_INPUT: read at the start of this frame, it may not have the
     * press that opened the menu yet, which would then close it at once */
    if (pressed & (DW1_PAD_START | DW1_PAD_SELECT))
        POLLED_INPUT = (int32_t)held;
    if (pressed & DW1_PAD_START) {
        save_text();
        playSound(0, 3);
        run_pause();
        restore_text();
    } else if ((pressed & DW1_PAD_SELECT) && dw1_option(DW1_OPT_DEBUG_MENU)) {
        save_text();
        playSound(0, 3);
        run_debug();
        restore_text();
    } else {
        return 0;
    }
    return (unsigned)POLLED_INPUT & 0xFFFF;
}
