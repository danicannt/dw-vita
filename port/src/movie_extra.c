/* libpress VLC functions the game's movie player uses that PSY-Z only has as
 * assembly. DecDCTvlc2 decodes with a RAM table built by DecDCTvlcBuild; PSY-Z's
 * DecDCTvlc decodes the same bitstream without one.
 * TODO: honour DecDCTvlcSize2 (partial decodes) if the movie player needs it. */
#include <libpress.h>

void DecDCTvlcBuild(u_short *table)
{
    (void)table;
}

int DecDCTvlc2(u_long *bs, u_long *buf, DECDCTTAB table)
{
    (void)table;
    return DecDCTvlc(bs, buf);
}
