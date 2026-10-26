/* The title screen's Options menu (the fifth entry under New Game,
 * Continue, ...): one row per port option (port/src/options.c), drawn like
 * the title menu in game/src/main/main_menu.c, in the game's font and UI box.
 *
 *   Up / Down          choose a row
 *   Left / Right / OK  change it (it applies at once)
 *   Cancel, or Back    save the options and return to the title menu
 *
 * Options forced by their environment variable are greyed out. */
#include <libgpu.h>
#include <libgs.h>

#include <dw/font.h>
#include <dw/input.h>
#include <dw/sound.h>
#include <dw/types.h>

extern int32_t CHANGED_INPUT;
extern GsOT *ACTIVE_ORDERING_TABLE;
void renderText(POLY_FT4 *prim, int32_t x, int32_t y, int32_t u, int32_t v, int32_t w,
                int32_t h, int32_t textColor);
void renderMenuBox(int32_t x, int32_t y, int32_t w, int32_t h);

static const char *const labels[DW1_OPT_COUNT] = {
    "Widescreen 16:9",   "HD backgrounds",    "HD movies",
    "No polygon wobble", "Model depth fix",   "3D texture filter",
    "Fair Bonus Try",    "Better battle gains",
};
static const char *const filter_names[3] = {"Off", "3-point", "Bilinear"};

#define ROWS (DW1_OPT_COUNT + 1) /* the options and Back */
#define ROW_H 12
#define VALUE_U 0xA8 /* text column of the values (a multiple of 4) */
#define VALUE_W (0x100 - VALUE_U - 1)
#define BOX_X 0x18
#define BOX_W 0x110
#define BOX_Y 0x28
#define BOX_H (ROW_H * (ROWS + 1) + 4 + 12)
#define TEXT_X (BOX_X + 8)
#define TITLE_Y (BOX_Y + 6)
#define ROW_Y(i) (TITLE_Y + ROW_H + 4 + ROW_H * (i))

/* text colours of renderText: white, grey, yellow */
enum { WHITE, GREY, YELLOW };

static int menu_open, cursor, changed;

static const char *value_text(int id)
{
    if (id == DW1_OPT_TEXTURE_FILTER)
        return filter_names[dw1_option(id)];
    return dw1_option(id) ? "On" : "Off";
}

/* the strings go to the text area in VRAM (row i + 1 for option i), as
 * drawMainMenuStrings does for the game's menus */
static void draw_value(int id)
{
    RECT rect;

    setRECT(&rect, VALUE_U, ROW_H * (id + 1), 0x100 - VALUE_U, ROW_H);
    clearTextSubArea(&rect);
    drawString(value_text(id), VALUE_U, ROW_H * (id + 1));
    DrawSync(0);
}

static void draw_strings(void)
{
    int i;

    clearTextArea();
    drawString("Options", 0, 0);
    DrawSync(0);
    for (i = 0; i < DW1_OPT_COUNT; i++) {
        drawString(labels[i], 0, ROW_H * (i + 1));
        DrawSync(0);
        draw_value(i);
    }
    drawString("Back", 0, ROW_H * ROWS);
    DrawSync(0);
}

void dw1_options_menu_open(void)
{
    menu_open = 1;
    cursor = 0;
    changed = 0;
    draw_strings();
}

static void change(int step)
{
    int n;

    if (dw1_option_locked(cursor)) {
        playSound(0, 4);
        return;
    }
    n = dw1_option_count(cursor);
    dw1_option_set(cursor, (dw1_option(cursor) + step + n) % n);
    draw_value(cursor);
    changed = 1;
    playSound(0, 3);
}

int dw1_options_menu_tick(void)
{
    int input = CHANGED_INPUT & 0xFFFF;

    if (!menu_open)
        return 0;
    if (input == 0x1000) { /* up */
        cursor = (cursor + ROWS - 1) % ROWS;
        playSound(0, 2);
    } else if (input == 0x4000) { /* down */
        cursor = (cursor + 1) % ROWS;
        playSound(0, 2);
    } else if (cursor < DW1_OPT_COUNT && (input == 0x2000 || input == CONFIRM_BUTTON)) {
        change(1);
    } else if (cursor < DW1_OPT_COUNT && input == 0x8000) { /* left */
        change(-1);
    } else if (input == CANCEL_BUTTON || (cursor == DW1_OPT_COUNT && input == CONFIRM_BUTTON)) {
        playSound(0, 4);
        if (changed)
            dw1_options_save();
        menu_open = 0;
    }
    return menu_open;
}

void dw1_options_menu_render(void)
{
    LINE_F3 *line = (LINE_F3 *)GsGetWorkBase();
    POLY_FT4 *text;
    int x = TEXT_X - 3 - 0xA0, y = ROW_Y(cursor) - 2 - 0x78;
    int w = BOX_W - 2 * (TEXT_X - BOX_X) + 6, h = ROW_H + 2;
    int i;

    /* the selection frame, as renderMenuSelector draws it (on top: the
     * ordering table draws what was added last first) */
    SetLineF3(line);
    setRGB0(line, 0xA0, 0xA0, 0);
    setXY3(line, x, y, x + w, y, x + w, y + h);
    AddPrim(ACTIVE_ORDERING_TABLE->org, line++);
    SetLineF3(line);
    setRGB0(line, 0xA0, 0xA0, 0);
    setXY3(line, x + w, y + h, x, y + h, x, y);
    AddPrim(ACTIVE_ORDERING_TABLE->org, line++);

    text = (POLY_FT4 *)line;
    renderText(text++, TEXT_X, TITLE_Y, 0, 0, 0x60, ROW_H, YELLOW);
    for (i = 0; i < DW1_OPT_COUNT; i++) {
        int color = dw1_option_locked(i) ? GREY : WHITE;

        renderText(text++, TEXT_X, ROW_Y(i), 0, ROW_H * (i + 1), VALUE_U, ROW_H, color);
        renderText(text++, TEXT_X + VALUE_U, ROW_Y(i), VALUE_U, ROW_H * (i + 1), VALUE_W, ROW_H,
                   color == GREY ? GREY : dw1_option(i) ? YELLOW : WHITE);
    }
    renderText(text++, TEXT_X, ROW_Y(DW1_OPT_COUNT), 0, ROW_H * ROWS, VALUE_U, ROW_H, WHITE);
    GsSetWorkBase((PACKET *)text);
    renderMenuBox(BOX_X, BOX_Y, BOX_W, BOX_H);
}
