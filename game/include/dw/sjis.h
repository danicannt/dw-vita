#ifndef DW_SJIS_H
#define DW_SJIS_H

#include <dw/types.h>

int32_t isAsciiEncoded(const char *value);
/* uint16_t convertAsciiToJis(uint8_t input); */
int32_t convertAsciiToJis();
void asciiToShiftJIS(uint8_t *src, uint16_t *dst);
int32_t swapShortBytes(uint16_t input);

#endif
