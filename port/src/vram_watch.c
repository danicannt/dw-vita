/* DW1_VRAMWATCH=x,y,w,h: log every LoadImage / MoveImage / ClearImage that
 * writes into that VRAM rectangle, with its caller (resolve with addr2line).
 * Finds who overwrites a texture. Hooked with --wrap. */
#include <stdio.h>
#include <stdlib.h>

#include <libgpu.h>

int __real_LoadImage(RECT *rect, u_long *p);
int __real_MoveImage(RECT *rect, int x, int y);
int __real_ClearImage(RECT *rect, u_char r, u_char g, u_char b);
unsigned dw1_pad_calls(void);

static int parsed, active;
static int wx, wy, ww, wh;

static int hits(int x, int y, int w, int h)
{
    if (!parsed) {
        const char *s = getenv("DW1_VRAMWATCH");

        parsed = 1;
        active = s && sscanf(s, "%d,%d,%d,%d", &wx, &wy, &ww, &wh) == 4;
    }
    return active && x < wx + ww && wx < x + w && y < wy + wh && wy < y + h;
}

static void report(const char *what, int x, int y, int w, int h, void *caller)
{
    /* caller as an offset from this function, so addr2line works with ASLR:
     * addr2line -e dw1 $(( nm(report) + offset )) */
    fprintf(stderr, "dw1: vramwatch: %s %d,%d %dx%d pad %u caller report%+ld\n", what, x, y, w, h,
            dw1_pad_calls(), (long)((char *)caller - (char *)report));
}

int __wrap_LoadImage(RECT *rect, u_long *p)
{
    if (hits(rect->x, rect->y, rect->w, rect->h))
        report("LoadImage", rect->x, rect->y, rect->w, rect->h, __builtin_return_address(0));
    return __real_LoadImage(rect, p);
}

int __wrap_MoveImage(RECT *rect, int x, int y)
{
    if (hits(x, y, rect->w, rect->h))
        report("MoveImage", x, y, rect->w, rect->h, __builtin_return_address(0));
    return __real_MoveImage(rect, x, y);
}

int __wrap_ClearImage(RECT *rect, u_char r, u_char g, u_char b)
{
    if (hits(rect->x, rect->y, rect->w, rect->h))
        report("ClearImage", rect->x, rect->y, rect->w, rect->h, __builtin_return_address(0));
    return __real_ClearImage(rect, r, g, b);
}
