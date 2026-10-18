#ifndef DW_ANIM_H
#define DW_ANIM_H

#include <dw/entity.h>
#include <dw/types.h>

void setupModelMatrix(PositionData *posData);

void startAnimation(/* Entity *entity, uint8_t animId */);
void tickAnimation(Entity *entity);

#endif
