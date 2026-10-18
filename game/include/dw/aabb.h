#ifndef DW_AABB_H
#define DW_AABB_H

#include <libgte.h>

#include <dw/entity.h>
#include <dw/types.h>

typedef struct {
	SVECTOR *center;
	SVECTOR extent;
} AABB;

void renderAABB(AABB *aabb);
int32_t findAABBHitEntity(AABB *aabb, Entity *ignoreEntity, int32_t startId);
int32_t hasAABBOverlap(AABB *a, AABB *b);

#endif
