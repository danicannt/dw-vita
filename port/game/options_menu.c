/* The Options menu: the title screen's fifth entry (under New Game,
 * Continue, ...) and the Start pause menu's Options. One row per port option
 * (port/src/options.c), grouped in sections, drawn like the title menu
 * (port/game/port_menu.c).
 *
 *   Up / Down          choose a row
 *   Left / Right / OK  change it (it applies at once)
 *   Cancel, or Back    save the options and close the menu
 *
 * Options forced by their environment variable are greyed out. */
#include <dw/input.h>
#include <dw/sound.h>
#include <dw/types.h>

#include "port_menu.h"

extern int32_t CHANGED_INPUT;

/* the rows: an option, a section title (SECTION) or Back */
#define SECTION(n) (-1 - (n))
#define BACK 100

static const char *const sections[] = {"Video", "HD packs", "Gameplay", "Sound and extras"};
static const int layout[] = {
    SECTION(0),
    DW1_OPT_WIDESCREEN,
    DW1_OPT_PGXP,
    DW1_OPT_MODEL_DEPTH,
    DW1_OPT_TEXTURE_FILTER,
    SECTION(1),
    DW1_OPT_HD_BACKGROUNDS,
    DW1_OPT_HD_SPRITES,
    DW1_OPT_HD_MOVIES,
    SECTION(2),
    DW1_OPT_ORIGINAL_FIXES,
    DW1_OPT_BATTLE_GAINS,
    DW1_OPT_FAIR_SLOTS,
    SECTION(3),
    DW1_OPT_AUDIO,
    DW1_OPT_DEBUG_MENU,
    BACK,
};
#define ROWS ((int)(sizeof(layout) / sizeof(layout[0])))

static const char *const labels[DW1_OPT_COUNT] = {
    [DW1_OPT_WIDESCREEN] = "Widescreen 16:9",
    [DW1_OPT_HD_BACKGROUNDS] = "HD backgrounds",
    [DW1_OPT_HD_MOVIES] = "HD movies",
    [DW1_OPT_PGXP] = "No polygon wobble",
    [DW1_OPT_MODEL_DEPTH] = "Model depth fix",
    [DW1_OPT_TEXTURE_FILTER] = "3D texture filter",
    [DW1_OPT_FAIR_SLOTS] = "Fair Bonus Try",
    [DW1_OPT_BATTLE_GAINS] = "Better battle gains",
    [DW1_OPT_ORIGINAL_FIXES] = "Original bug fixes",
    [DW1_OPT_HD_SPRITES] = "HD object sprites",
    [DW1_OPT_DEBUG_MENU] = "Select: debug menu",
    [DW1_OPT_AUDIO] = "Audio",
};
static const char *const filter_names[3] = {"Off", "3-point", "Bilinear"};
static const char *const audio_names[2] = {"Original", "Improved"};

static Dw1Menu menu;
static int menu_open, changed;

static const char *value_text(int id)
{
    if (id == DW1_OPT_TEXTURE_FILTER)
        return filter_names[dw1_option(id)];
    if (id == DW1_OPT_AUDIO)
        return audio_names[dw1_option(id)];
    return dw1_option(id) ? "On" : "Off";
}

static void build(void)
{
    int i;

    menu.title = "Options";
    menu.count = ROWS;
    menu.width = 0x110;
    menu.value_x = 0xA8;
    for (i = 0; i < ROWS; i++) {
        Dw1MenuRow *r = &menu.rows[i];
        int id = layout[i];

        r->value = NULL;
        r->header = 0;
        r->color = DW1_MENU_WHITE;
        if (id == BACK) {
            r->label = "Back";
        } else if (id < 0) {
            r->label = sections[SECTION(id)];
            r->header = 1;
            r->color = DW1_MENU_YELLOW;
        } else {
            r->label = labels[id];
            r->value = value_text(id);
            if (dw1_option_locked(id)) {
                r->color = r->value_color = DW1_MENU_GREY;
            } else {
                r->value_color = dw1_option(id) ? DW1_MENU_YELLOW : DW1_MENU_WHITE;
            }
        }
    }
}

void dw1_options_menu_open(void)
{
    menu_open = 1;
    changed = 0;
    build();
    dw1_menu_cursor_to(&menu, 0);
    dw1_menu_invalidate();
    dw1_menu_text(&menu);
}

static void change(int id, int step)
{
    int n;

    if (dw1_option_locked(id)) {
        playSound(0, 4);
        return;
    }
    n = dw1_option_count(id);
    dw1_option_set(id, (dw1_option(id) + step + n) % n);
    changed = 1;
    playSound(0, 3);
    build();
    dw1_menu_text(&menu);
}

int dw1_options_menu_tick(void)
{
    int input = CHANGED_INPUT & 0xFFFF;
    int id;

    if (!menu_open)
        return 0;
    id = layout[menu.cursor];
    if (dw1_menu_move(&menu, input)) {
    } else if (id != BACK && (input == DW1_PAD_RIGHT || input == CONFIRM_BUTTON)) {
        change(id, 1);
    } else if (id != BACK && input == DW1_PAD_LEFT) {
        change(id, -1);
    } else if (input == CANCEL_BUTTON || (id == BACK && input == CONFIRM_BUTTON)) {
        playSound(0, 4);
        if (changed)
            dw1_options_save();
        menu_open = 0;
        dw1_menu_invalidate();
    }
    return menu_open;
}

void dw1_options_menu_render(void)
{
    dw1_menu_render(&menu);
}
