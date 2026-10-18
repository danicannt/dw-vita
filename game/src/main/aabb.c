#include <libgpu.h>
#include <libgs.h>
#include <libgte.h>

#include <dw/aabb.h>
#include <dw/entity.h>
#include <dw/params.h>

extern GsOT *ACTIVE_ORDERING_TABLE;

void renderAABB(AABB *aabb)
{
	LINE_F4 *line;
	uint8_t *idx;
	SVECTOR v;
	DVECTOR xy[8];
	GsOT_TAG *ot;
	int32_t i;
	long p;
	long flag;
	SVECTOR *center;

	return;

	ot = ACTIVE_ORDERING_TABLE->org;
	center = aabb->center;
	v.vx = center->vx - aabb->extent.vx;
	v.vy = center->vy - aabb->extent.vy;
	v.vz = center->vz - aabb->extent.vz;
	RotTransPers(&v, (long *)&xy[0], &p, &flag);
	line = (LINE_F4 *)GsGetWorkBase();
	for (i = 0; i < 4; i++) {
		SetLineF4(line);
		setRGB0(line, 0x80, 0, 0);
		line->x0 = xy[*idx].vx;
		line->y0 = xy[*idx++].vy;
		line->x1 = xy[*idx].vx;
		line->y1 = xy[*idx++].vy;
		line->x2 = xy[*idx].vx;
		line->y2 = xy[*idx++].vy;
		line->x3 = xy[*idx].vx;
		line->y3 = xy[*idx].vy;
		AddPrim(&ot[33], line);
	}
	GsSetWorkBase((PACKET *)line);
}

int32_t findAABBHitEntity(AABB *aabb, Entity *ignoreEntity, int32_t startId)
{
	Entity *entity;
	VECTOR *location;
	SVECTOR center;
	AABB b;

	for (; startId < ENTITY_MAX; ++startId) {
		if (ENTITY_TABLE[startId] && (ENTITY_TABLE[startId] != ignoreEntity)) {
			entity = ENTITY_TABLE[startId];
			location = &entity->posData->location;
			center.vx = location->vx;
			center.vy = location->vy -
				(DIGIMON_DATA[entity->type].height >> 1);
			center.vz = location->vz;
			b.center = &center;
			b.extent.vx = DIGIMON_DATA[entity->type].radius >> 1;
			b.extent.vy = DIGIMON_DATA[entity->type].height >> 1;
			b.extent.vz = DIGIMON_DATA[entity->type].radius >> 1;
			if (hasAABBOverlap(aabb, &b) == 1) {
				return startId;
			}
		}
	}

	return -1;
}

int32_t hasAABBOverlap(AABB *a, AABB *b)
{
	if ((b->center->vx + b->extent.vx) <
	    (a->center->vx - a->extent.vx)) {
		return 0;
	} else if ((b->center->vx - b->extent.vx) >
		   (a->center->vx + a->extent.vx)) {
		return 0;
	} else {
		if ((b->center->vy + b->extent.vy) <
		    (a->center->vy - a->extent.vy)) {
			return 0;
		} else if ((b->center->vy - b->extent.vy) >
			   (a->center->vy + a->extent.vy)) {
			return 0;
		} else {
			if ((b->center->vz + b->extent.vz) <
			    (a->center->vz - a->extent.vz)) {
				return 0;
			} else if ((b->center->vz - b->extent.vz) >
				   (a->center->vz + a->extent.vz)) {
				return 0;
			}
		}
	}

	return 1;
}
