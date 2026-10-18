#ifndef DW_MOVE_H
#define DW_MOVE_H

#include <dw/types.h>

typedef struct {
	int32_t distance;
	int16_t power;
	uint8_t mpCost;
	uint8_t iframes;
	uint8_t range;
	uint8_t special;
	uint8_t status;
	uint8_t accuracy;
	uint8_t statusChance;
	uint8_t unk3;
	uint8_t unk4;
	uint8_t unk5;
} Move;

extern Move MOVE_DATA[];

#endif
