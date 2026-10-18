/* libpress for the movie player (MOV_REL), on the C MDEC in port/src/mdec.c.
 *
 * PSY-Z has DecDCTvlc2 only as assembly, and its DecDCTin / DecDCTout drive
 * the MDEC hardware registers (MDEC_in / MDEC_out are NOT_IMPLEMENTED), so the
 * decoder runs here instead. The four hardware functions are taken over with
 * the linker's --wrap; DecDCTvlc2 and DecDCTvlcBuild are defined directly.
 *
 * Decoding is instant: DecDCTout decodes `size` words and then runs the
 * DecDCToutCallback handler, as the MDEC's DMA completion would. The game's
 * handler (MOV_onSliceDecoded) issues the next DecDCTout itself; a call made
 * from inside the handler decodes at once but only marks one more completion,
 * which runs after the handler returns, so the slices do not recurse.
 * (Same scheme as DW2-PC's psyq/libpress.c, CC0.) */
#include <libpress.h>

#include "dw1_mdec.h"

/* MOV_RUN_LEVEL_BUFFER_0/1 hold 40960 words each */
#define RUN_LEVEL_WORDS 40960

static void (*out_cb)(void);
static int in_cb;
static int pending;

void DecDCTvlcBuild(u_short *table)
{
    (void)table; /* Mdec_Vlc decodes without the RAM table */
}

int DecDCTvlc2(u_long *bs, u_long *buf, DECDCTTAB table)
{
    (void)table;
    return Mdec_Vlc((const uint8_t *)bs, (uint32_t *)buf, RUN_LEVEL_WORDS) < 0 ? -1 : 0;
}

void __wrap_DecDCTReset(int mode)
{
    (void)mode;
}

/* mode bit 0: 24-bit output, bit 1: 15-bit output with the mask bit set */
void __wrap_DecDCTin(u_long *buf, int mode)
{
    Mdec_Start((const uint32_t *)buf, mode & 1, (mode >> 1) & 1);
}

void __wrap_DecDCTout(u_long *buf, int size)
{
    Mdec_Out((uint32_t *)buf, size);
    if (out_cb == NULL)
        return;
    if (in_cb) {
        pending = 1;
        return;
    }
    do {
        pending = 0;
        in_cb = 1;
        out_cb();
        in_cb = 0;
    } while (pending && out_cb != NULL);
}

DecDCCb __wrap_DecDCToutCallback(DecDCCb func)
{
    DecDCCb old = (DecDCCb)out_cb;

    out_cb = (void (*)(void))func;
    return old;
}
