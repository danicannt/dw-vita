/* inline_n.h - GTE inline macros for the native port.
 * PSY-Z's libgte.h already maps the gte_* macros to its software GTE; this adds
 * the few the game uses that PSY-Z lacks. */
#ifndef DW1_INLINE_N_H
#define DW1_INLINE_N_H

#include <libgte.h>

/* LZCS/LZCR: count of leading bits equal to the sign bit */
extern long dw1_gte_lzcs;
static inline long dw1_gte_lzc(long v)
{
    unsigned long u = (unsigned long)(v < 0 ? ~v : v);
    long n = 0;
    if (u == 0)
        return 32;
    while (!(u & 0x80000000ul)) {
        u <<= 1;
        n++;
    }
    return n;
}
#ifndef gte_ldlzc
#define gte_ldlzc(r0) (dw1_gte_lzcs = (long)(r0))
#endif
#ifndef gte_stlzc
#define gte_stlzc(r0) (*(long *)(r0) = dw1_gte_lzc(dw1_gte_lzcs))
#endif

#endif
