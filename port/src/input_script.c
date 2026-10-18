/* Pad recording and replay, for repeatable test runs.
 *
 *   DW1_RECORD=file   play normally; every change of the pad state is written
 *                     to file (flushed at once, so a crash keeps the log)
 *   DW1_REPLAY=file   feed the recorded pad states back instead of the real
 *                     pad; once the log runs out the last state is held for
 *                     REPLAY_TAIL more calls (a recording cut by a crash ends
 *                     on the press that caused it), then the real pad takes over
 *
 * Time is counted in PadRead calls rather than wall-clock frames: the game
 * polls the pad once per logic tick, so a replay stays in step even when
 * loading or rendering runs at a different speed. Each line is
 * "<call> <state in hex>", meaning the pad reads <state> from that call on.
 *
 * Hooked with the linker's --wrap=PadRead so PSY-Z stays untouched. */
#include <stdio.h>
#include <stdlib.h>

unsigned int __real_PadRead(int id);

enum { MODE_UNSET, MODE_OFF, MODE_RECORD, MODE_REPLAY };

#define REPLAY_TAIL 240

static int mode = MODE_UNSET;
static FILE *script;
static unsigned long calls;
static unsigned int last_state;
static unsigned long next_call;     /* replay: call at which next_state applies */
static unsigned int next_state;
static int replay_done;
static unsigned long tail_end;  /* replay: last call that still holds last_state */

static void read_next(void)
{
    if (fscanf(script, "%lu %x", &next_call, &next_state) != 2) {
        replay_done = 1;
        tail_end = calls + REPLAY_TAIL;
        fprintf(stderr, "dw1: replay finished at call %lu, real pad active from call %lu\n",
                calls, tail_end);
    }
}

static void init(void)
{
    const char *path;

    mode = MODE_OFF;
    if ((path = getenv("DW1_REPLAY")) != NULL) {
        script = fopen(path, "r");
        if (script == NULL) {
            fprintf(stderr, "dw1: cannot open replay '%s'\n", path);
            return;
        }
        mode = MODE_REPLAY;
        read_next();
    } else if ((path = getenv("DW1_RECORD")) != NULL) {
        script = fopen(path, "w");
        if (script == NULL) {
            fprintf(stderr, "dw1: cannot create recording '%s'\n", path);
            return;
        }
        mode = MODE_RECORD;
        fprintf(script, "0 0\n");
        fflush(script);
    }
}

unsigned int __wrap_PadRead(int id)
{
    unsigned int state;

    if (mode == MODE_UNSET)
        init();

    if (mode == MODE_REPLAY && (!replay_done || calls <= tail_end)) {
        while (!replay_done && next_call <= calls) {
            last_state = next_state;
            read_next();
        }
        calls++;
        return last_state;
    }

    state = __real_PadRead(id);
    if (mode == MODE_RECORD && state != last_state) {
        fprintf(script, "%lu %x\n", calls, state);
        fflush(script);
        last_state = state;
    }
    calls++;
    return state;
}
