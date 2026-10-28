/* Drawing for the port's menus (port/game/port_menu.h): one row of the text
 * area (704,256 in VRAM, the game's font) per menu row, shown with the title
 * menu's renderText and renderMenuBox from game/src/main/main_menu.c. */
#include <string.h>

#include <libgpu.h>
#include <libgs.h>

#include <dw/font.h>
#include <dw/sound.h>
#include <dw/types.h>

#include "port_menu.h"

extern GsOT *ACTIVE_ORDERING_TABLE;
void renderText(POLY_FT4 *prim, int32_t x, int32_t y, int32_t u, int32_t v, int32_t w,
                int32_t h, int32_t textColor);
void renderMenuBox(int32_t x, int32_t y, int32_t w, int32_t h);

#define ROW_H 12
#define TEXT_W 0x100 /* the text area, in pixels */
#define INDENT 8     /* rows under a section title */

/* what the text area holds: the title (row 0), then label and value per row */
static char drawn[DW1_MENU_MAX_ROWS + 1][2][40];
static int drawn_valid;

void dw1_menu_invalidate(void)
{
    drawn_valid = 0;
}

static void put(char *cache, const char *s, int x, int v, int w)
{
    RECT rect;

    if (s == NULL)
        s = "";
    if (drawn_valid && strncmp(cache, s, 39) == 0)
        return;
    setRECT(&rect, x, v, w, ROW_H);
    clearTextSubArea(&rect);
    drawString(s, x, v);
    DrawSync(0);
    strncpy(cache, s, 39);
    cache[39] = '\0';
}

void dw1_menu_text(const Dw1Menu *m)
{
    int i;

    if (!drawn_valid) {
        clearTextArea();
        DrawSync(0);
        memset(drawn, 0, sizeof(drawn));
    }
    put(drawn[0][0], m->title, 0, 0, TEXT_W);
    for (i = 0; i < m->count; i++) {
        int v = ROW_H * (i + 1);

        put(drawn[i + 1][0], m->rows[i].label, 0, v, m->value_x);
        put(drawn[i + 1][1], m->rows[i].value, m->value_x, v, TEXT_W - m->value_x);
    }
    drawn_valid = 1;
}

static int box_h(const Dw1Menu *m)
{
    return ROW_H * (m->count + 1) + 4 + 12;
}

void dw1_menu_render(const Dw1Menu *m)
{
    LINE_F3 *line = (LINE_F3 *)GsGetWorkBase();
    POLY_FT4 *text;
    int bx = (320 - m->width) / 2, by = (240 - box_h(m)) / 2;
    int tx = bx + 8, ty = by + 6, ry = ty + ROW_H + 4;
    int x = tx - 3 - 0xA0, y = ry + ROW_H * m->cursor - 2 - 0x78;
    int w = m->width - 2 * 8 + 6, h = ROW_H + 2;
    int i, sections = 0;

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
    if (m->title != NULL)
        renderText(text++, tx, ty, 0, 0, TEXT_W - 1, ROW_H, DW1_MENU_YELLOW);
    for (i = 0; i < m->count; i++)
        sections |= m->rows[i].header;
    for (i = 0; i < m->count; i++) {
        const Dw1MenuRow *r = &m->rows[i];
        int rx = tx + (sections && !r->header ? INDENT : 0);
        int v = ROW_H * (i + 1), lw = r->value != NULL ? m->value_x : TEXT_W - 1;

        renderText(text++, rx, ry + ROW_H * i, 0, v, lw, ROW_H, r->color);
        if (r->value != NULL)
            renderText(text++, tx + m->value_x, ry + ROW_H * i, m->value_x, v,
                       TEXT_W - 1 - m->value_x, ROW_H, r->value_color);
    }
    GsSetWorkBase((PACKET *)text);
    renderMenuBox(bx, by, m->width, box_h(m));
}

void dw1_menu_cursor_to(Dw1Menu *m, int row)
{
    int i;

    for (i = 0; i < m->count; i++) {
        int r = (row + i) % m->count;

        if (!m->rows[r].header) {
            m->cursor = r;
            return;
        }
    }
}

int dw1_menu_move(Dw1Menu *m, int input)
{
    int step, i, r = m->cursor;

    if (input == DW1_PAD_UP)
        step = m->count - 1;
    else if (input == DW1_PAD_DOWN)
        step = 1;
    else
        return 0;
    for (i = 0; i < m->count; i++) {
        r = (r + step) % m->count;
        if (!m->rows[r].header)
            break;
    }
    m->cursor = r;
    playSound(0, 2);
    return 1;
}
