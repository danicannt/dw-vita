/* The port's own menus (Options, the Start pause menu, the Select debug
 * menu), drawn like the game's title menu: its font, written into the text
 * area in VRAM, and its blue UI box (port/game/port_menu.c). */
#ifndef DW1_PORT_MENU_H
#define DW1_PORT_MENU_H

/* text colours of the title menu's renderText */
enum { DW1_MENU_WHITE, DW1_MENU_GREY, DW1_MENU_YELLOW };

/* pad bits of CHANGED_INPUT (pad 1) */
#define DW1_PAD_SELECT 0x0100
#define DW1_PAD_START 0x0800
#define DW1_PAD_UP 0x1000
#define DW1_PAD_RIGHT 0x2000
#define DW1_PAD_DOWN 0x4000
#define DW1_PAD_LEFT 0x8000

#define DW1_MENU_MAX_ROWS 18

typedef struct {
    const char *label;
    const char *value; /* NULL: none */
    int color;         /* of the label */
    int value_color;
    int header; /* a section title: not selectable */
} Dw1MenuRow;

typedef struct {
    const char *title;
    Dw1MenuRow rows[DW1_MENU_MAX_ROWS];
    int count;
    int width;   /* of the box, in screen pixels */
    int value_x; /* where the values start (a multiple of 4) */
    int cursor;
} Dw1Menu;

/* writes the strings that changed since the last call to the text area */
void dw1_menu_text(const Dw1Menu *m);
/* the next dw1_menu_text redraws everything (another menu used the area) */
void dw1_menu_invalidate(void);
/* queues the box, the strings and the selection frame in the active
 * ordering table */
void dw1_menu_render(const Dw1Menu *m);
/* Up / Down: moves the cursor over the selectable rows; 1 when it moved */
int dw1_menu_move(Dw1Menu *m, int input);
/* the cursor on the first selectable row from `row` on */
void dw1_menu_cursor_to(Dw1Menu *m, int row);

#endif
