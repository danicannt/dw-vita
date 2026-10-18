/* Shared state of the port's libgs implementation. */
#ifndef DW1_LIBGS_INTERNAL_H
#define DW1_LIBGS_INTERNAL_H

#include <libgs.h>

extern short GsORGOFSX, GsORGOFSY;
extern long gs_projection;    /* GsSetProjection */
extern long gs_near_clip;     /* GsSetNearClip */
extern int gs_light_mode;     /* GsSetLightMode */

void gs_init_3d(void);

/* Log a libgs function that is not implemented yet, once per call site. */
#include <stdio.h>
#define GS_TODO(name)                                                          \
    do {                                                                       \
        static int warned_;                                                    \
        if (!warned_) {                                                        \
            warned_ = 1;                                                       \
            fprintf(stderr, "dw1: libgs %s not implemented yet\n", name);      \
        }                                                                      \
    } while (0)

#endif
