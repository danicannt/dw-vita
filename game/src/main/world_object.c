#include <dw/world_object.h>


WorldObject WORLD_OBJECTS[NUM_WORLD_OBJECTS + 1];

void initializeWorldObjects(void)
{
	int32_t i;

	for (i = 0; i < NUM_WORLD_OBJECTS; ++i) {
		WORLD_OBJECTS[i].objectId = -1;
		WORLD_OBJECTS[i].instanceId = -1;
		WORLD_OBJECTS[i].tick = NULL;
		WORLD_OBJECTS[i].render = NULL;
	}
}


#ifdef DW1_NATIVE
/* for the port's watchdogs: some instance of objectId is registered */
int32_t dw1_object_exists(int32_t objectId)
{
	int32_t i;

	for (i = 0; i < NUM_WORLD_OBJECTS; i++) {
		if (WORLD_OBJECTS[i].objectId == objectId) {
			return 1;
		}
	}
	return 0;
}
#endif

int32_t addObject(objectId, instanceId, tick, render)
int16_t objectId;
int16_t instanceId;
TickFunction tick;
RenderFunction render;
{
	int32_t i;

	WORLD_OBJECTS[NUM_WORLD_OBJECTS].objectId = -1;

	i = 0;
	while (1) {
		if (WORLD_OBJECTS[i].objectId == -1) {
			break;
		}

		++i;
	}

	if (i == NUM_WORLD_OBJECTS) {
		return 0;
	}

	WORLD_OBJECTS[i].objectId = objectId;
	WORLD_OBJECTS[i].instanceId = instanceId;
	WORLD_OBJECTS[i].tick = tick;
	WORLD_OBJECTS[i].render = render;

	return 1;
}


int32_t removeObject(objectId, instanceId)
int16_t objectId;
int16_t instanceId;
{
	int32_t i;

	WORLD_OBJECTS[NUM_WORLD_OBJECTS].objectId = objectId;
	WORLD_OBJECTS[NUM_WORLD_OBJECTS].instanceId = instanceId;

	i = 0;
	while (1) {
		if ((WORLD_OBJECTS[i].objectId == objectId) &&
		    (WORLD_OBJECTS[i].instanceId == instanceId)) {
			break;
		}

		++i;
	}

	if (i == NUM_WORLD_OBJECTS) {
		return 0;
	}

	WORLD_OBJECTS[i].objectId = -1;
	WORLD_OBJECTS[i].instanceId = -1;
	WORLD_OBJECTS[i].tick = NULL;
	WORLD_OBJECTS[i].render = NULL;

	return 1;
}

#ifdef DW1_NATIVE
/* the slot tickObjects is running: on the PS1 its loop index stayed in $s1
 * and handlePraiseScold reads it as an uninitialised local, see there */
int32_t DW1_TICK_SLOT;
#endif

void tickObjects(void)
{
	int32_t i;

	for (i = 0; i < NUM_WORLD_OBJECTS; ++i) {
		if ((WORLD_OBJECTS[i].objectId != -1) &&
		    (WORLD_OBJECTS[i].tick != NULL)) {
#ifdef DW1_NATIVE
			DW1_TICK_SLOT = i;
#endif
			WORLD_OBJECTS[i].tick(WORLD_OBJECTS[i].instanceId);
		}
	}
}

void renderObjects(void)
{
	int32_t i;

	for (i = 0; i < NUM_WORLD_OBJECTS; ++i) {
		if ((WORLD_OBJECTS[i].objectId != -1) &&
		    (WORLD_OBJECTS[i].render != NULL)) {
			WORLD_OBJECTS[i].render(WORLD_OBJECTS[i].instanceId);
		}
	}
}
