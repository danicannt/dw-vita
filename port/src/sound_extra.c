/* libsnd functions the game uses that PSY-Z only has as assembly. */
#include <libsnd.h>

#include "dw1_native.h"

/* SPU address of each open VAB's sample body (PSY-Z's decompiled libsnd) */
extern unsigned _svm_vab_start[];

u_long SsUtGetVBaddrInSB(short vabid)
{
    if (vabid < 0 || vabid >= 16)
        return 0;
    return _svm_vab_start[vabid];
}

/* Volume ramp of one voice over delta_time ticks.
 * TODO: ramp over time; for now the end volume is applied at once. */
short SsUtAutoVol(short vc, short start_vol, short end_vol, short delta_time)
{
    (void)start_vol;
    (void)delta_time;
    if (vc < 0 || vc >= 24)
        return -1;
    SsUtSetVVol(vc, end_vol, end_vol);
    return 0;
}
