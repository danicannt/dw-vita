/* Force-included in every game translation unit (see CMakeLists.txt).
 *
 * Keep it small: native fixes belong next to the code they fix, marked
 * DW1_NATIVE. This header only carries what the whole game needs. */
#ifndef DW1_NATIVE_H
#define DW1_NATIVE_H

#include <psyz.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- PS1 main RAM ----------------------------------------------------------
 * The game uses a few fixed PS1 addresses as buffers (GENERAL_BUFFER at
 * 0x80010000, TEXTURE_BUFFER, the battle model area, two ordering tables, ...),
 * all in the RAM the retail executable leaves free. Natively they live in one
 * 2 MB arena laid out like PS1 RAM, so offsets between them stay as on the PS1.
 * The heap (InitHeap3) is placed in the same arena at its retail address. */
#define DW1_PS1_RAM_SIZE 0x200000
extern unsigned char dw1_ps1_ram[DW1_PS1_RAM_SIZE];
#define DW1_PS1_ADDR(addr) ((void *)(dw1_ps1_ram + ((unsigned long)(addr) & (DW1_PS1_RAM_SIZE - 1))))

/* ---- scratchpad ------------------------------------------------------------
 * 1 KB of fast RAM at 0x1F800000 on the PS1. */
#define DW1_SCRATCHPAD_SIZE 0x400
extern unsigned char dw1_scratchpad[DW1_SCRATCHPAD_SIZE];
#ifndef getScratchAddr
#define getScratchAddr(offset) ((unsigned long *)(dw1_scratchpad + (offset) * 4))
#endif

/* ---- overlays --------------------------------------------------------------
 * The PS1 loads one *_REL.BIN overlay at a time over the same memory. Natively
 * every overlay is linked into the executable; loading one restores its data
 * to the initial state, as a fresh load from disc did (port/src/overlay.c). */
void dw1_overlay_init(void);
void dw1_overlay_reset(int lib);

#ifdef __cplusplus
}
#endif

#endif
