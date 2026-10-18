#ifndef DW_MOV_H
#define DW_MOV_H

#include <libgpu.h>

#include <dw/types.h>

typedef struct {
	u_long *runLevelBuffer[2];
	int32_t runLevelIndex;
	u_long *sliceBuffer[2];
	int32_t sliceIndex;
	RECT frame[2];
	int32_t frameIndex;
	RECT slice;
	int32_t flipPending;
} MovieEnv;

extern int32_t MOVIE_COMPLETED;
extern int32_t MOVIE_WIDTH;
extern int32_t MOVIE_HEIGHT;

extern MovieEnv MOV_ENV;
extern u_long MOV_RUN_LEVEL_BUFFER_0[40960];
extern u_long MOV_RUN_LEVEL_BUFFER_1[40960];
extern u_long MOV_SLICE_BUFFER_0[2880];
extern u_long MOV_SLICE_BUFFER_1[2880];
extern u_long MOV_RING_BUFFER[12288];
extern u_short MOV_VLC_TABLE[34818];

int32_t MOV_playMovie(int32_t movieId);

#endif
