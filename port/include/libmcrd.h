/* PSY-Z's libmcrd.h guards its own (wrong) struct DIRENTRY with #ifndef DIRENTRY,
 * which is always true, so it clashes with kernel.h's. Use kernel.h's (the PSY-Q
 * layout: name, attr, size, next, system) and keep libmcrd.h's out. */
#ifndef DW1_LIBMCRD_H
#define DW1_LIBMCRD_H
#include <kernel.h>
#define DIRENTRY DIRENTRY
#include_next <libmcrd.h>
#undef DIRENTRY
#endif
