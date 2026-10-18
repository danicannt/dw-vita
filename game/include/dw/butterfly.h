#ifndef DW_BUTTERFLY_H
#define DW_BUTTERFLY_H

#include <dw/entity.h>
#include <dw/types.h>

void initializeButterfly(void);
int32_t setButterfly(Entity *entity);
void tickButterfly(int32_t instanceId);
void renderButterfly(int32_t instanceId);
void unsetButterfly(int32_t butterflyId);

#endif
