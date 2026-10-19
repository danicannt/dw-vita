/* F9 while playing: mark the moment something looks wrong.
 *
 * Each press logs "dw1: MARCA n en llamada C" (C counted in PadRead calls,
 * the same clock as recordings), saves both display buffers as
 * marca_<n>_<time>.ppm in DW1_DUMP_DIR (default: ./marcas) and the script state
 * to the log. While recording with DW1_RECORD=file the mark also goes to
 * file.marcas as "<call> <n>", so a replay can be stopped right there.
 *
 * Polled from __wrap_PadRead, i.e. from the game loop itself. */
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>

#include <SDL3/SDL.h>

unsigned dw1_pad_calls(void);
void dw1_dump_request(const char *name);
void dw1_dump_state(void);

void dw1_bug_mark_poll(void)
{
    static int was_down, marks;
    const bool *keys;
    int down;
    const char *rec;
    char name[48], stamp[16];
    time_t now;

    keys = SDL_GetKeyboardState(NULL);
    down = keys != NULL && keys[SDL_SCANCODE_F9];
    if (!down || was_down) {
        was_down = down;
        return;
    }
    was_down = 1;
    marks++;
    fprintf(stderr, "dw1: MARCA %d en llamada %u\n", marks, dw1_pad_calls());
    if (getenv("DW1_DUMP_DIR") == NULL) {
        mkdir("marcas", 0755);
        setenv("DW1_DUMP_DIR", "marcas", 0);
    }
    /* the time keeps marks of separate sessions from overwriting each other */
    now = time(NULL);
    strftime(stamp, sizeof(stamp), "%H%M%S", localtime(&now));
    snprintf(name, sizeof(name), "marca_%d_%s", marks, stamp);
    fprintf(stderr, "dw1: captura en %s/%s.ppm\n", getenv("DW1_DUMP_DIR"), name);
    dw1_dump_request(name);
    dw1_dump_state();
    if ((rec = getenv("DW1_RECORD")) != NULL) {
        char path[512];
        FILE *f;

        snprintf(path, sizeof(path), "%s.marcas", rec);
        if ((f = fopen(path, "a")) != NULL) {
            fprintf(f, "%u %d\n", dw1_pad_calls(), marks);
            fclose(f);
        }
    }
}
