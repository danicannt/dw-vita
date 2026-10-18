#ifndef DW_SOUND_ASYNC_H
#define DW_SOUND_ASYNC_H

#include <dw/types.h>

int32_t isSoundBufferLoading(int32_t vabId);
int32_t loadSB(void);
int32_t readVBALLSection(int32_t vabId, int32_t idx);
int32_t loadMapSounds2(int32_t mapSoundId);
int32_t isSoundLoaded(int32_t mode, int32_t vabId);
int32_t loadVLALL(int32_t idx, uint8_t *buffer);
void waitForSoundBufferLoading(int32_t vabId);

#endif
