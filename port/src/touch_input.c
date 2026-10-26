/* Host input the PS1 pad never had: the left stick, the touch panel (the
 * mouse on the desktop) and the bits port/game/touch_controls.c asks for.
 *
 *   DW1_STICK=0      the left stick does not move like the D-pad
 *   DW1_TOUCH=0      the touch panel and the mouse are ignored
 *   DW1_QUICKFEED=0  L1 does not open the feeding inventory
 *
 * PSY-Z's PadRead only reports the stick in the analog bytes, which DW1
 * never reads (it asks for the digital pad), so on its own the stick did
 * nothing: dw1_host_input turns it into D-pad bits, eight ways.
 *
 * Touches are mapped to the game's screen as the renderer fits it in the
 * window (sdl3_gl.c: 4:3 plus dw1_wide_extra on each side, centred with
 * black bars). Called from __wrap_PadRead, before a recording sees the
 * state, so DW1_RECORD keeps the stick and the touch walking; a replay
 * ignores the touch panel. */
#include <stdlib.h>

#include <SDL3/SDL.h>
#include <libetc.h>

#include "dw1_native.h"

#define STICK_DEAD 12000 /* of 32767 */
#define DPAD (PADLup | PADLdown | PADLleft | PADLright)

int dw1_replay_running(void);

unsigned int dw1_touch_pad;

static int init_done;
static int stick_on = 1, touch_on = 1, quick_feed_on = 1;
static int touch_down, touch_began, touch_x, touch_y;
static int manual_move;

static int env_off(const char *name)
{
    const char *v = getenv(name);
    return v != NULL && atoi(v) == 0;
}

static void init(void)
{
    init_done = 1;
    stick_on = !env_off("DW1_STICK");
    touch_on = !env_off("DW1_TOUCH");
    quick_feed_on = !env_off("DW1_QUICKFEED");
}

int dw1_quick_feed_enabled(void)
{
    if (!init_done)
        init();
    return quick_feed_on;
}

static SDL_Gamepad *first_gamepad(void)
{
    SDL_Gamepad *pad = NULL;
    int count = 0;
    SDL_JoystickID *ids = SDL_GetGamepads(&count);

    if (ids != NULL) {
        if (count > 0)
            pad = SDL_GetGamepadFromID(ids[0]); /* opened by PSY-Z */
        SDL_free(ids);
    }
    return pad;
}

static unsigned int stick_bits(void)
{
    SDL_Gamepad *pad = first_gamepad();
    int x, y, ax, ay;
    unsigned int r = 0;

    if (pad == NULL)
        return 0;
    x = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX);
    y = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY);
    ax = abs(x);
    ay = abs(y);
    if (ax < STICK_DEAD && ay < STICK_DEAD)
        return 0;
    /* eight ways: a diagonal when the smaller axis is over ~22.5 degrees */
    if (ax * 5 >= ay * 2)
        r |= x < 0 ? PADLleft : PADLright;
    if (ay * 5 >= ax * 2)
        r |= y < 0 ? PADLup : PADLdown;
    return r;
}

/* nx, ny: 0..1 across the window or panel */
static void to_game(float nx, float ny, float win_w, float win_h)
{
    float aspect = (4.0f / 3.0f) * (320 + 2 * dw1_wide_extra) / 320.0f;
    float rx = 0, ry = 0, rw = win_w, rh = win_h;

    if (win_w / win_h > aspect) {
        rw = win_h * aspect;
        rx = (win_w - rw) / 2;
    } else {
        rh = win_w / aspect;
        ry = (win_h - rh) / 2;
    }
    touch_x = (int)((nx * win_w - rx) / rw * (320 + 2 * dw1_wide_extra)) - 160 - dw1_wide_extra;
    touch_y = (int)((ny * win_h - ry) / rh * 240) - 120;
}

static void read_touch(void)
{
    int was_down = touch_down;
    int devices = 0;
    SDL_TouchID *ids;

    touch_down = 0;
    ids = SDL_GetTouchDevices(&devices);
    if (ids != NULL) {
        int i;
        for (i = 0; i < devices && !touch_down; i++) {
            int fingers = 0;
            SDL_Finger **f;
#ifdef __vita__
            /* the rear panel would walk the tamer by accident */
            if (SDL_GetTouchDeviceType(ids[i]) == SDL_TOUCH_DEVICE_INDIRECT_ABSOLUTE)
                continue;
#endif
            f = SDL_GetTouchFingers(ids[i], &fingers);
            if (f != NULL) {
                if (fingers > 0) {
#ifdef __vita__
                    to_game(f[0]->x, f[0]->y, 960, 544);
#else
                    SDL_Window *w = SDL_GetMouseFocus();
                    int ww = 4, wh = 3;
                    if (w != NULL)
                        SDL_GetWindowSize(w, &ww, &wh);
                    to_game(f[0]->x, f[0]->y, (float)ww, (float)wh);
#endif
                    touch_down = 1;
                }
                SDL_free(f);
            }
        }
        SDL_free(ids);
    }
#ifndef __vita__
    /* the left mouse button stands in for a finger */
    if (!touch_down) {
        float mx, my;
        SDL_Window *w = SDL_GetMouseFocus();
        if (w != NULL && (SDL_GetMouseState(&mx, &my) & SDL_BUTTON_LMASK)) {
            int ww, wh;
            SDL_GetWindowSize(w, &ww, &wh);
            if (ww > 0 && wh > 0) {
                to_game(mx / ww, my / wh, (float)ww, (float)wh);
                touch_down = 1;
            }
        }
    }
#endif
    touch_began = touch_down && !was_down;
}

void dw1_host_input(unsigned int *pad, int id)
{
    int manual;

    if (!init_done)
        init();
    manual = (*pad & DPAD) != 0;
    if (stick_on && !manual)
        *pad |= stick_bits();
    /* the overworld polls PadRead(0) once a tick (pollInputGame); battles
     * and the pause screen read PadRead(1) and only get the stick */
    if (id != 0)
        return;
    manual_move = (*pad & DPAD) != 0;
    if (touch_on && !dw1_replay_running())
        read_touch();
    else
        touch_down = touch_began = 0;
    /* asked for by the last game tick; it asks again every tick */
    if (!manual_move)
        *pad |= dw1_touch_pad;
    dw1_touch_pad = 0;
}

int dw1_touch_state(int *x, int *y)
{
    int r = 0;

    if (touch_down) {
        *x = touch_x;
        *y = touch_y;
        r = DW1_TOUCH_DOWN | (touch_began ? DW1_TOUCH_BEGAN : 0);
    }
    return r;
}

int dw1_pad_manual_move(void)
{
    return manual_move;
}
