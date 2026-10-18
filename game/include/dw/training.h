#ifndef DW_TRAINING_H
#define DW_TRAINING_H

#include <dw/types.h>

typedef struct {
	int16_t x;
	int16_t y;
	int16_t z;
	int16_t stat;
} TrainingSpot;

typedef struct {
	int32_t mapId;
	TrainingSpot *spots;
} MapTrainingSpots;

typedef struct {
	int16_t result;
	int16_t payout;
	int16_t resultTimer;
	int16_t state;
	int8_t reelPos[3];
	int8_t targetSymbol[3];
	int8_t stopSteps[3];
	int8_t pad11;
	int16_t scrollY[3];
	int16_t spinSpeed[3];
	int16_t stat;
	int16_t autoStart;
	int8_t assist;
	int8_t settling[3];
	int8_t unk_26[6];
} SlotMachine;

/* Defined by TRN; TRN2_tickMpTraining passes it to removeObject by mistake. */
void TRN_tickMpTraining(int32_t instanceId);

#endif
