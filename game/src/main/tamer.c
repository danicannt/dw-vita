#include <inline_n.h>
#include <libetc.h>
#include <libgpu.h>
#include <libgs.h>
#include <libgte.h>

#include <dw/anim.h>
#include <dw/butterfly.h>
#include <dw/clock.h>
#include <dw/dooa.h>
#include <dw/eab.h>
#include <dw/endi.h>
#include <dw/entity.h>
#include <dw/fade.h>
#include <dw/font.h>
#include <dw/input.h>
#include <dw/item.h>
#include <dw/kar.h>
#include <dw/main.h>
#include <dw/map.h>
#include <dw/math.h>
#include <dw/model.h>
#include <dw/murd.h>
#include <dw/params.h>
#include <dw/partner.h>
#include <dw/pstat.h>
#include <dw/std.h>
#include <dw/tamer.h>
#include <dw/types.h>
#include <dw/ui.h>
#include <dw/utils.h>
#include <dw/version.h>
#include <text/main/tamer.h>

RECT ITEM_PICKUP_TEXT_AREA = {0, 12, 256, 200};
#if VERSION_IS(EU)
RECT ITEM_PICKUP_SECOND_LINE = {0, 24, 256, 12};
#endif

MESSAGES_TEXT

RECT TAKE_CHEST_TEXT_AREA = {0, 12, 256, 200};
#if VERSION_IS(EU)
RECT TAKE_CHEST_SECOND_LINE = {0, 24, 256, 12};
#endif
RECT AWARD_SOMETHING_TEXT_AREA = {0, 12, 256, 200};

int8_t HAS_ROTATION_DATA[8];
int8_t PREVIOUS_CAMERA_POS_INITIALIZED;
int32_t HAS_PICKED_UP_ITEM;
int32_t TAMER_LEVEL_AWARD_PENDING;
int32_t MEDAL_AWARD_PENDING;
int8_t TAMER_LEVELS_AWARDED;
int8_t TAMER_STATE;
int8_t TAMER_SUB_STATE;
uint8_t PICKED_UP_DROP_ID;
int8_t TAKE_CHEST_STATE;
int8_t TAKE_ITEM_FRAME_COUNTER;
uint8_t TAKE_CHEST_ITEM;
int8_t INTERACTED_CHEST;
int16_t MOVE_TO_DELTA_X;
int16_t MOVE_TO_DELTA_Z;
int8_t TALKED_TO_ENTITY;
int32_t TRAINING_COMPLETE;
int8_t IMMORTAL_HOUR;
int32_t HAS_IMMORTAL_HOUR;

static void *tamer_sbss_order[] = {
	&HAS_IMMORTAL_HOUR,
	&IMMORTAL_HOUR,
	&TRAINING_COMPLETE,
	&TALKED_TO_ENTITY,
#if VERSION_IS(EU)
	&MOVE_TO_DELTA_X,
	&MOVE_TO_DELTA_Z,
	&INTERACTED_CHEST,
	&TAKE_CHEST_ITEM,
	&TAMER_SUB_STATE,
	&TAKE_CHEST_STATE,
	&PICKED_UP_DROP_ID,
	&TAKE_ITEM_FRAME_COUNTER,
	&TAMER_STATE,
	HAS_ROTATION_DATA,
	&PREVIOUS_CAMERA_POS_INITIALIZED,
	&HAS_PICKED_UP_ITEM,
	&TAMER_LEVEL_AWARD_PENDING,
	&MEDAL_AWARD_PENDING,
	&TAMER_LEVELS_AWARDED,
#else
	&MOVE_TO_DELTA_Z,
	&MOVE_TO_DELTA_X,
	&INTERACTED_CHEST,
	&TAKE_CHEST_ITEM,
	&TAKE_ITEM_FRAME_COUNTER,
	&TAKE_CHEST_STATE,
	&PICKED_UP_DROP_ID,
	&TAMER_SUB_STATE,
	&TAMER_STATE,
	&TAMER_LEVELS_AWARDED,
	&MEDAL_AWARD_PENDING,
	&TAMER_LEVEL_AWARD_PENDING,
	&HAS_PICKED_UP_ITEM,
	&PREVIOUS_CAMERA_POS_INITIALIZED,
	HAS_ROTATION_DATA,
#endif
};

extern int8_t GAME_STATE;
extern int32_t HAS_BUTTERFLY;
extern int32_t BUTTERFLY_ID;
extern int32_t SOME_SCRIPT_SYNC_BIT;
extern uint8_t TEXTBOX_OPEN_TIMER;
extern uint8_t TARGET_MAP;
extern uint8_t CURRENT_EXIT;
extern uint8_t PREVIOUS_EXIT;
extern GsRVIEW2 GS_VIEWPOINT;
extern int32_t MAIN_D_80185BB0[3];
extern int32_t MAIN_D_80185BB4[3];
extern int32_t MAIN_D_80185BBC[3];

extern int32_t IS_IN_MENU;
extern int8_t MAIN_D_80134DF9;
extern int16_t MAIN_D_801386A4[16];
extern uint32_t POLLED_INPUT;
extern uint32_t CHANGED_INPUT;
extern int32_t IS_SCRIPT_PAUSED;
extern int8_t ITEM_SCOLD_FLAG;
extern int8_t FADE_PROTECTION_FULL;
extern uint16_t CURRENT_SCRIPT_ID;
extern uint8_t CURRENT_SCREEN;
extern uint8_t ACTIVE_BGM_FONT;
extern int32_t ACTIVE_FRAMEBUFFER;
extern GsOT *ACTIVE_ORDERING_TABLE;
extern uint8_t SKIP_DAYTIME_TRANSITION;
extern uint16_t CURRENT_FRAME;
extern int16_t MINUTE;
extern int16_t HOUR;
extern int16_t DAY;
extern uint8_t YEAR;
extern int32_t MONEY;
extern int32_t NPC_IS_WALKING_TOWARDS[8];

typedef struct {
	VECTOR location;
	VECTOR trayLocation;
	SVECTOR rotation;
	SVECTOR trayRotation;
	uint16_t trigger;
	uint8_t item;
	uint8_t isTaken;
	uint8_t tileX;
	uint8_t tileY;
	uint16_t padding;
} Chest;

extern Chest CHEST_ARRAY[8];

typedef struct {
	int16_t spawnX[10];
	int16_t spawnY[10];
	int16_t spawnZ[10];
	int16_t rotation[10];
	int16_t targetMap[10];
	int16_t targetExit[10];
} MapWarps;

uint8_t UNKNOWN_TAMER_DATA[10];
VECTOR ROTATION_DATA[8];
VECTOR STORED_TAMER_POS;
MapWarps MAP_WARPS;
VECTOR PREVIOUS_CAMERA_POS;

static void *tamer_bss_order[] = {
	&PREVIOUS_CAMERA_POS,
	&MAP_WARPS,
	&STORED_TAMER_POS,
	ROTATION_DATA,
	UNKNOWN_TAMER_DATA,
};

void entityLookAtLocation(Entity *entity, VECTOR *pos);
void setupEntityMatrix(int32_t entityId);
void changeMap(uint8_t mapId, uint8_t exitId);
void addMapNameObject(uint8_t mapId);
void renderString(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t,
                  int32_t, int32_t, int32_t);
void renderUIBox(int32_t id);
void setMapLayerEnabled(int32_t enabled);
int32_t isSoundLoaded(int32_t isAsync, int32_t soundId);
void playSound(int32_t vabId, uint32_t note);
void setCameraFollowPlayer(void);
void unsetCameraFollowPlayer(void);
int32_t entityCheckCollision(Entity *a, Entity *b, int32_t c, int32_t d);
void collisionGrace(Entity *a, Entity *b, int32_t c, int32_t d);
int32_t tickEntityWalkTo(/* uint8_t scriptId, uint8_t targetId, int16_t x, int16_t z, int8_t useCamera */);
void setTrigger(uint16_t trigger);
int32_t tickOpenChestTray(int32_t chestId);
int32_t tickCloseChestTray(int32_t chestId);
int32_t hasMove(int32_t moveId);
int32_t hasMedal(uint16_t medal);
int32_t hasDigimonRaised(int32_t digimonId);
int32_t getCardAmount(uint8_t cardId);
void unlockMedal(uint16_t medal);
void closeTriangleMenu(void);
void removeTriangleMenu(void);
void closeInventoryBoxes(void);
void removeUIBox1(void);
void callScriptSection(uint16_t scriptId, uint32_t scriptSection, uint32_t param);
void addGameMenu(void);
void setPartnerIdling(void);
int32_t getTileTrigger(VECTOR *pos);
void getModelTile(VECTOR *pos, int16_t *outTileX, int16_t *outTileY);
int32_t createPoopPile(int16_t tileX, int16_t tileY);
void moveCameraByDiff(VECTOR *prev, VECTOR *cur);
void getRotationDifference(PositionData *data, VECTOR *target,
                           int16_t *outX, int16_t *outY, int16_t *outZ);
int32_t rotateEntity(SVECTOR *rot, int16_t *outX, int16_t *outY,
                     int16_t *outZ, int32_t speed);
void tickTamerWaypoints(void);
void loadMapDigimon(uint8_t *data, int16_t a);
void tamerTickBattle(int32_t instanceId);
void tickConditionBoundaries(void);
void handlePostBattleTiredness(void);
int32_t getEntityScreenPos(Entity *entity, int32_t flag, DVECTOR *outPos);
int32_t isUIBoxAvailable(int32_t id);
void playBGM(int16_t bgmId);
void readMapTFS(int32_t mapId);
void initializeDaytimeTransition(int32_t mode);
int32_t handleBattleStart(int32_t instanceId);
void loadBattleData(int32_t a, int32_t b);
int32_t BTL_battleMain(void);
int32_t isKeyDown(uint32_t key);
#ifndef DW1_NATIVE /* PS1 prototype; the native SDK declares it */
void memcpy(uint8_t *dst, uint8_t *src, uint32_t size);
#endif
#ifndef DW1_NATIVE /* PS1 prototype; the native SDK declares it */
int32_t strlen(char *s);
#endif
void setupPartnerOnWarp(int32_t x, int32_t y, int32_t z, int32_t rotationY);
extern int32_t DRAWING_OFFSET_X;
extern int32_t DRAWING_OFFSET_Y;
extern GsOT GS_ORDERING_TABLE[2];
extern PACKET GS_WORK_BASES[2][0x14000];
extern MATRIX GsWSMATRIX;
extern int32_t FADE_PROTECTION;

void tamerTickBattle(int32_t instanceId);
void tamerTickOverworld(int16_t instanceId);
void tamerTickWalkingState();
void tamerTickChangeMap(void);
void tamerTickPickupItem(void);
void tamerTickTakeChest(void);
void tamerTickIdle(void);
void tamerTickTraining(void);
void tamerTickPraiseScold(int8_t state);
void tamerTickFishing(void);
void tamerTickOpening(void);
void tamerTickEnding(void);
void tamerTickSicknessLostLife(void);
void tamerTickMachinedramonSpawn(void);
void tamerTickBattleLostLife(void);
void tamerTickAwardSomething(void);

void tamerTick(int16_t instanceId);
void renderItemPickupTextbox(int32_t instanceId);
void renderAwardSomethingTextbox(int32_t instanceId);
void setTamerDirection(int32_t direction);
void setupTamerOnWarp(int16_t x, int16_t y, int16_t z, int16_t rotationY);
int16_t getMapRotation(void);
void advanceBattleTime(int32_t result);
void addTamerLevel(int32_t chance, int32_t amount);
void checkItemPickup(void);
void checkMedalConditions(void);
uint8_t checkChestCollision(void);
void checkPendingAwards(void);
void tamerTickPickupItem(void);
void tamerTickTakeChest(void);
void tamerTickAwardSomething(void);
void tamerTickWalkingState();
void tamerTickChangeMap(void);
void tamerTickPraiseScold(int8_t state);
void tamerTickOpening(void);
void tamerTickEnding(void);
void tamerTickSicknessLostLife(void);
void tamerTickMachinedramonSpawn(void);
void tamerTickBattleLostLife(void);
void tamerTickIdle(void);
void tamerTickTraining(void);

static void *tamer_functions[] = {
	renderAwardSomethingTextbox,
	isTrainingComplete,
	addTamerLevel,
	checkChestCollision,
	worldPosToScreenPos2,
	tickEntityMoveToAxis,
	tickEntityMoveTo,
	tickEntitySetRotation,
	tickLookAtEntity,
	getEntityFromScriptId,
	tickEntityWalkTo,
	startAnimationTamer,
	tamerGetState,
	tamerSetFullState,
	advanceBattleTime,
	startBattle,
	renderItemPickupTextbox,
	checkPendingAwards,
	checkMedalConditions,
	checkMapInteraction,
	checkItemPickup,
	setTamerDirection,
	getMapRotation,
	tamerTickAwardSomething,
	tamerTickBattleLostLife,
	tamerTickMachinedramonSpawn,
	tamerTickSicknessLostLife,
	tamerTickEnding,
	tamerTickOpening,
	tamerTickPraiseScold,
	tamerTickTraining,
	tamerTickIdle,
	tamerTickTakeChest,
	tamerTickPickupItem,
	tamerTickChangeMap,
	tamerSetState,
	tamerTickWalkingState,
	tamerTickOverworld,
	setupTamerOnWarp,
	loadMapEntities,
#if VERSION_IS(EU)
	initializeTamer,
	tamerTick,
#else
	tamerTick,
	initializeTamer,
#endif
};

void initializeTamer(int32_t type, int32_t posX, int32_t posY, int32_t posZ,
		     int32_t rotX, int32_t rotY, int32_t rotZ)
{
	int32_t i;

	PLAYER_SHADOW_ENABLED = 1;
	thunkLoadMMD(type, 2);

	ENTITY_TABLE[0] = &TAMER_ENTITY.entity;

	initializeDigimonObject(type, 0, (TickFunction)tamerTick);
	setEntityPosition(0, posX, posY, posZ);
	setEntityRotation(0, rotX, rotY, rotZ);
	setupEntityMatrix(0);

	STORED_TAMER_POS.vx = posX;
	STORED_TAMER_POS.vy = posY;
	STORED_TAMER_POS.vz = posZ;

	startAnimation(ENTITY_TABLE[0], 0);

	GAME_STATE = 0;
	ENTITY_TABLE[0]->isOnMap = 1;
	ENTITY_TABLE[0]->isOnScreen = 1;

	for (i = 0; i < 8; ++i) {
		HAS_ROTATION_DATA[i] = 0;
	}

	for (i = 0; i < 10; ++i) {
		UNKNOWN_TAMER_DATA[i] = 0;
	}

	PREVIOUS_CAMERA_POS_INITIALIZED = 0;
	HAS_PICKED_UP_ITEM = 0;
	STORED_TAMER_POS = TAMER_ENTITY.entity.posData->location;
	TAMER_LEVEL_AWARD_PENDING = 0;
	MEDAL_AWARD_PENDING = 0;
	IS_IN_MENU = 0;
	TAMER_LEVELS_AWARDED = 1;
}

// clang-format off
void loadMapEntities(data, mapId, warpIdx)
	uint8_t *data;
	int16_t mapId;
	int16_t warpIdx;
// clang-format on
{
	int16_t x;
	int16_t spawnY;
	int16_t z;
	int16_t rotation;

	memcpy((void *)&MAP_WARPS, data, sizeof(MAP_WARPS));
	data += sizeof(MAP_WARPS);

	x = MAP_WARPS.spawnX[warpIdx];
	spawnY = MAP_WARPS.spawnY[warpIdx];
	z = MAP_WARPS.spawnZ[warpIdx];
	rotation = MAP_WARPS.rotation[warpIdx];

	setupTamerOnWarp(x, spawnY, z, rotation);

	if ((rotation <= 0x200) || (rotation > 0xe00)) {
		z += 200;
	} else if ((rotation > 0x200) && (rotation <= 0x600)) {
		x += 200;
	} else if ((rotation > 0x600) && (rotation <= 0xa00)) {
		z -= 200;
	} else if ((rotation > 0xa00) && (rotation <= 0xe00)) {
		x -= 200;
	}

	setupPartnerOnWarp(x, spawnY, z, (rotation + 0x800) % 0x1000);
	loadMapDigimon(data, mapId);
	STORED_TAMER_POS = TAMER_ENTITY.entity.posData->location;
}

void setupTamerOnWarp(int16_t x, int16_t y, int16_t z, int16_t rotationY)
{
	setEntityPosition(0, x, y, z);
	setEntityRotation(0, TAMER_ENTITY.entity.posData->rotation.vx, rotationY,
	                  TAMER_ENTITY.entity.posData->rotation.vz);
	setupEntityMatrix(0);
	startAnimation(ENTITY_TABLE[0], 0);
}

void tamerTick(int16_t instanceId)
{
	if (((GAME_STATE != 0) || (TAMER_STATE != 0)) && (HAS_BUTTERFLY == 0)) {
		unsetButterfly(BUTTERFLY_ID);
		HAS_BUTTERFLY = -1;
	}
	switch (GAME_STATE) {
	case 0:
		tamerTickOverworld(instanceId);
		break;
	case 1:
	case 2:
	case 3:
		tamerTickBattle(instanceId);
		break;
	case 4:
	case 5:
		STD_tickTamerTournament(instanceId);
	default:
		break;
	}
}

void tamerTickOverworld(int16_t instanceId)
{
#if VERSION_IS(JP_TRIAL) || VERSION_IS(JP_BOMBOM)
	if ((DAY >= 5) && (IS_SCRIPT_PAUSED == 1)) {
		return;
	}
#endif
	switch (TAMER_STATE) {
	case 0:
		tamerTickWalkingState(instanceId);
		break;
	case 1:
		tamerSetState(6);
		break;
	case 5:
		tamerTickChangeMap();
		break;
	case 7:
		tamerTickPickupItem();
		break;
	case 14:
		tamerTickTakeChest();
		break;
	case 6:
		tamerTickIdle();
		break;
	case 8:
		tamerTickTraining();
		break;
	case 9:
	case 13:
		tamerTickPraiseScold(TAMER_STATE);
		break;
	case 11:
		tamerTickFishing();
		break;
	case 12:
		KAR_tick();
		break;
	case 15:
		tamerTickOpening();
		break;
	case 16:
		tamerTickEnding();
		break;
	case 17:
		tamerTickSicknessLostLife();
		break;
	case 18:
		tamerTickMachinedramonSpawn();
		break;
	case 19:
		tamerTickBattleLostLife();
		break;
	case 20:
		tamerTickAwardSomething();
	default:
		break;
	}

	tickAnimation(&TAMER_ENTITY.entity);
}

#ifdef DW1_NATIVE
static int32_t dw1TamerExtraInput(void);
static void dw1TouchWalkFace(void);
extern int dw1_quick_feed;
#endif

void tamerTickWalkingState(void)
{
	int16_t rotation;
	int16_t quadrant;

	tickTamerWaypoints();

#if VERSION_IS(EU)
	if ((isKeyDown(0x20) != 0) &&
#else
	if ((isKeyDown(0x10) != 0) &&
#endif
	    (IS_SCRIPT_PAUSED == 1) &&
#if VERSION_EQUAL_OR_NEWER(US)
	    (FADE_PROTECTION == 0) &&
#endif
	    (UI_BOX_DATA[0].state != 1) &&
	    (UI_BOX_DATA[0].frame == 0)) {
		addGameMenu();
		tamerSetState(1);
		startAnimation(&TAMER_ENTITY.entity, 0);
		unsetCameraFollowPlayer();

		copyVector(&STORED_TAMER_POS, &TAMER_ENTITY.entity.posData->location);
		IS_IN_MENU = 1;

		stopGameTime();
		setPartnerIdling();
#ifdef DW1_NATIVE
		dw1_quick_feed = 0;
#endif
		return;
	}

#ifdef DW1_NATIVE
	/* L1 quick feeding and the touch controls, at the end of this file */
	if (dw1TamerExtraInput()) {
		return;
	}
#endif

	if (((POLLED_INPUT & 0x1000) != 0) ||
	    ((POLLED_INPUT & 0x4000) != 0) ||
	    ((POLLED_INPUT & 0x8000) != 0) ||
	    ((POLLED_INPUT & 0x2000) != 0)) {
		if (((POLLED_INPUT & 8) != 0) ||
		    ((POLLED_INPUT & CANCEL_BUTTON) != 0)) {
			if (TAMER_ENTITY.entity.anim.animId != 2) {
				startAnimation(&TAMER_ENTITY.entity, 2);
			}
		} else if (TAMER_ENTITY.entity.anim.animId != 3) {
			startAnimation(&TAMER_ENTITY.entity, 3);
		}
		ITEM_SCOLD_FLAG = 0;
	} else if (TAMER_ENTITY.entity.anim.animId != 0) {
		startAnimation(&TAMER_ENTITY.entity, 0);
	}

	rotation = getMapRotation();
	quadrant = (int16_t)(rotation / 0x200);
	if ((rotation % 0x200) >= 0x100) {
		quadrant++;
	}
	rotation = (int16_t)(quadrant * 0x200);

	if ((POLLED_INPUT & 0x1000) != 0) {
		if ((POLLED_INPUT & 0x8000) != 0) {
			setTamerDirection((int16_t)(rotation + 0x600));
		} else if ((POLLED_INPUT & 0x2000) != 0) {
			setTamerDirection((int16_t)(rotation + 0xa00));
		} else {
			setTamerDirection((int16_t)(rotation + 0x800));
		}
	} else if ((POLLED_INPUT & 0x4000) != 0) {
		if ((POLLED_INPUT & 0x8000) != 0) {
			setTamerDirection((int16_t)(rotation + 0x200));
		} else if ((POLLED_INPUT & 0x2000) != 0) {
			setTamerDirection((int16_t)(rotation + 0xe00));
		} else {
			setTamerDirection((int16_t)(rotation + 0));
		}
	} else if ((POLLED_INPUT & 0x8000) != 0) {
		setTamerDirection((int16_t)(rotation + 0x400));
	} else if ((POLLED_INPUT & 0x2000) != 0) {
		setTamerDirection((int16_t)(rotation + 0xc00));
	}
#ifdef DW1_NATIVE
	dw1TouchWalkFace();
#endif

	checkItemPickup();
	checkMapInteraction();
	checkMedalConditions();
	checkPendingAwards();

	copyVector(&STORED_TAMER_POS, &TAMER_ENTITY.entity.posData->location);
}

int16_t getMapRotation(void)
{
	int16_t angle;
	int16_t dx;
	int16_t dz;

	dx = (int16_t)(GS_VIEWPOINT.vpx - GS_VIEWPOINT.vrx);
	dz = (int16_t)(GS_VIEWPOINT.vpz - GS_VIEWPOINT.vrz);
	angle = (int16_t)_atan(dz, dx);
	return angle;
}

// clang-format off
void setTamerDirection(direction)
	int16_t direction;
// clang-format on
{
	if (direction > 0xfff) {
		direction -= 0x1000;
	}
	TAMER_ENTITY.entity.posData->rotation.vy = direction;
}

void checkItemPickup(void)
{
	DroppedItem *item;
	int32_t i;
	int16_t tileX;
	int16_t tileY;

	getModelTile(&TAMER_ENTITY.entity.posData->location, &tileX, &tileY);
	item = DROPPED_ITEMS;
	PICKED_UP_DROP_ID = 0xff;
	for (i = 0; i < 11; ++i) {
		if (item->worldItem.type == 0xff) {
			++item;
			continue;
		}

		if ((tileX - 1 > item->tileX) ||
		    (item->tileX > tileX + 1)) {
			goto next;
		}

		if ((tileY - 1 > item->tileY) ||
		    (item->tileY > tileY + 1)) {
			goto next;
		}

		PICKED_UP_DROP_ID = i;

		if (HAS_PICKED_UP_ITEM != 1) {
			tamerSetState(7);
			HAS_PICKED_UP_ITEM = 1;
		}
		break;
next:
		++item;
	}

	if (PICKED_UP_DROP_ID == 0xff) {
		HAS_PICKED_UP_ITEM = 0;
	}
}

void checkMapInteraction(void)
{
	int16_t collision;
	uint8_t trigger;

	collision = entityCheckCollision(&PARTNER_ENTITY.digimonEntity.entity,
	                                 &TAMER_ENTITY.entity, 0, 0);
	if ((collision >= 2) && (collision < 10)) {
		TAMER_ENTITY.entity.anim.animFlag |= 2;
		if (((NPC_ENTITIES[collision - 2].autotalk == 1) ||
		     ((NPC_ENTITIES[collision - 2].autotalk == 0) &&
		      ((CHANGED_INPUT & CONFIRM_BUTTON) != 0))) &&
		    (IS_SCRIPT_PAUSED == 1)) {
			TALKED_TO_ENTITY = collision;
			removeTriangleMenu();
			closeInventoryBoxes();
			removeUIBox1();
			callScriptSection(CURRENT_SCRIPT_ID,
			                  NPC_ENTITIES[TALKED_TO_ENTITY - 2].scriptId, 1);
		}
	} else if (collision == 10) {
		collisionGrace(NULL, ENTITY_TABLE[0], 0, 0);
	}

	if (TAMER_STATE != 0) {
		return;
	}

	trigger = getTileTrigger(&ENTITY_TABLE[0]->posData->location);
	if (trigger == 120) {
		if (((PARTNER_PARA.condition & 8) != 0) &&
		    (IS_SCRIPT_PAUSED == 1)) {
			callScriptSection(0, 0x4e2, 0);
		}
	} else if ((trigger >= 110) && (trigger < 120)) {
		TARGET_MAP = MAP_WARPS.targetMap[trigger - 110];
		MAIN_D_80134DF9 = trigger - 110;
		CURRENT_EXIT = MAP_WARPS.targetExit[trigger - 110];
		PREVIOUS_EXIT = trigger - 110;

		tamerSetState(5);
		unsetCameraFollowPlayer();
		stopGameTime();
	} else if ((trigger > 50) && (trigger < 80)) {
		if (IS_SCRIPT_PAUSED == 1) {
			callScriptSection(CURRENT_SCRIPT_ID, trigger, 0);
		}
	} else if ((trigger >= 80) && (trigger < 110)) {
		if (((CHANGED_INPUT & CONFIRM_BUTTON) != 0) &&
		    (IS_SCRIPT_PAUSED == 1)) {
			callScriptSection(CURRENT_SCRIPT_ID, trigger, 0);
		}
	}

	trigger = checkChestCollision();
	if ((trigger != 0xff) && ((CHANGED_INPUT & CONFIRM_BUTTON) != 0)) {
		INTERACTED_CHEST = trigger;
		tamerSetState(14);
	}
}

void checkMedalConditions(void)
{
	int32_t i;

	if (hasMedal(5) == 0) {
		for (i = 0; i < 0x39; ++i) {
			if (hasMove(i) == 0) {
				break;
			}
		}

		if (i == 0x39) {
			unlockMedal(5);
			MEDAL_AWARD_PENDING = 1;
		}
	}

	if (hasMedal(7) == 0) {
		BaseStats *bs = &PARTNER_ENTITY.digimonEntity.stats.base;
		if ((bs->hp == 9999) && (bs->mp == 9999) &&
		    (bs->off == 999) && (bs->def == 999) &&
		    (bs->brain == 999) && (bs->speed == 999)) {
			unlockMedal(7);
			MEDAL_AWARD_PENDING = 1;
		}
	}

	if ((hasMedal(0xD) == 0) && (MONEY == 999999)) {
		unlockMedal(0xD);
		MEDAL_AWARD_PENDING = 1;
	}

	if ((hasMedal(0xE) == 0) && (YEAR == 10)) {
		unlockMedal(0xE);
		MEDAL_AWARD_PENDING = 1;
	}

	if (hasMedal(6) == 0) {
		for (i = 1; i < 62; ++i) {
			if (hasDigimonRaised((uint16_t)i) == 0) {
				break;
			}
		}

		if (i == 62) {
			unlockMedal(6);
			MEDAL_AWARD_PENDING = 1;

			++TAMER_ENTITY.tamerLevel;
			if (TAMER_ENTITY.tamerLevel >= 11) {
				TAMER_ENTITY.tamerLevel = 10;
			}
		}
	}
	if ((hasMedal(9) == 0) && (PARTNER_PARA.fishCaught >= 100)) {
		unlockMedal(9);
		MEDAL_AWARD_PENDING = 1;
	}
	if (hasMedal(0xC) == 0) {
		for (i = 0; i < 66; ++i) {
			if (getCardAmount((uint8_t)i) == 0) {
				break;
			}
		}

		if (i == 66) {
			unlockMedal(0xC);
			MEDAL_AWARD_PENDING = 1;
		}
	}
}

void checkPendingAwards(void)
{
	if ((TAMER_LEVEL_AWARD_PENDING == 1) || (MEDAL_AWARD_PENDING == 1)) {
		tamerSetState(0x14);
		stopGameTime();
	}
}

void renderItemPickupTextbox(int32_t instanceId)
{
#if !VERSION_IS(US)
	int32_t halfLength;

	renderString(0xf, -0x7d, 0x2d, 0x48, 0xc, 0, 0xc, 5, 0);
#endif
	if (TAKE_CHEST_STATE == 0) {
#if VERSION_IS(EU)
		renderString(0, -0x7d, 0x39, 0x30, 0xc, 0, 0x24, 5, 0);
		renderString(0xf, -0x7d, 0x45, 0x60, 0xc, 0, 0x18, 5, 0);
		halfLength = strlen(ITEM_NAME(TAKE_CHEST_ITEM)) / 2;
#elif !VERSION_IS(US)
		renderString(0, -0x7d, 0x39, 0x30, 0xc, 0, 0x24, 5, 0);
		renderString(0xf, -0x4d, 0x39, 0x60, 0xc, 0, 0x18, 5, 0);
		halfLength = strlen(ITEM_NAME(TAKE_CHEST_ITEM)) / 2;
		renderString(0, halfLength * 12 - 0x4d, 0x39, 0x24, 0xc, 0x30, 0x24, 5, 0);
#else
		renderString(0, 0xffffff83, 0x39, 0x5a, 0xc, 0, 0x24, 5, 0);
		renderString(0xf, 0xffffff83, 0x2d, 0x60, 0xc, 0, 0x18, 5, 0);
#endif
	} else if (TAKE_CHEST_STATE == 1) {
		renderString(0, 0xffffff83, 0x39, 0xf0, 0xc, 0, 0x18, 5, 0);
	} else {
		renderString(0, 0xffffff83, 0x39, 0x90, 0xc, 0, 0x18, 5, 0);
	}

	renderUIBox(1);
	++TEXTBOX_OPEN_TIMER;
}

int8_t startBattle(int16_t instanceId)
{
	int8_t result;
	int32_t battleData;

	GAME_STATE = 1;
	copyVector(&STORED_TAMER_POS, &TAMER_ENTITY.entity.posData->location);

	unsetCameraFollowPlayer();
	closeTriangleMenu();
	stopGameTime();

	battleData = handleBattleStart(instanceId);
	loadBattleData(instanceId, battleData);
	result = BTL_battleMain();

	GAME_STATE = 0;

	if (result == -1) {
		PARTNER_PARA.happiness -= 0x1e;
		PARTNER_PARA.discipline -= 0x14;
		partnerSetState(-1);
		SKIP_DAYTIME_TRANSITION = 1;
	} else if (result == 0) {
		PARTNER_PARA.happiness -= 10;
		PARTNER_PARA.discipline -= 6;
		PARTNER_PARA.tiredness += 2;
		handlePostBattleTiredness();
		SKIP_DAYTIME_TRANSITION = 1;
	} else if (result == 1) {
		playBGM(ACTIVE_BGM_FONT);
		readMapTFS(CURRENT_SCREEN);
		STORED_TAMER_POS = TAMER_ENTITY.entity.posData->location;
		partnerSetState(1);
		PARTNER_PARA.happiness += 2;
		handlePostBattleTiredness();
		++PARTNER_PARA.battles;
		SKIP_DAYTIME_TRANSITION = 0;
	}

	tickConditionBoundaries();

	ACTIVE_FRAMEBUFFER = GsGetActiveBuff();
	GsSetWorkBase(GS_WORK_BASES[ACTIVE_FRAMEBUFFER]);
	GsClearOt(0, 0, GS_ORDERING_TABLE + ACTIVE_FRAMEBUFFER);
	ACTIVE_ORDERING_TABLE = GS_ORDERING_TABLE + ACTIVE_FRAMEBUFFER;

	advanceBattleTime(result);

	return result;
}

// clang-format off
void advanceBattleTime(result)
	int8_t result;
// clang-format on
{
	MINUTE += 20;

	CURRENT_FRAME += 400;
	if (MINUTE > 59) {
		++HOUR;
		MINUTE -= 60;
#ifdef DW1_NATIVE
		/* original bug: an hour that passes in a battle did not count
		 * towards the evolution timer, as tickGameClock's hours do */
		if (dw1_fix())
			++PARTNER_PARA.evoTimer;
#endif

		--PARTNER_PARA.remainingLifetime;
		if (PARTNER_PARA.remainingLifetime < 0) {
			PARTNER_PARA.remainingLifetime = 0;
		}

		if ((PARTNER_PARA.condition & 1) != 0) {
			++PARTNER_PARA.sicknessCounter;
			++PARTNER_PARA.missedSleepHours;
		}

		if ((PARTNER_PARA.condition & 0x40) != 0) {
			++PARTNER_PARA.sicknessTries;
		}

		if ((PARTNER_PARA.condition & 0x20) != 0) {
			++PARTNER_PARA.injuryTimer;
		}

		if ((PARTNER_PARA.condition & 0x40) != 0) {
			++PARTNER_PARA.sicknessTimer;
		}

		if (0x17 < HOUR) {
			++DAY;
			HOUR = 0;
			CURRENT_FRAME = MINUTE * 20;
#ifdef DW1_NATIVE
			/* original bug: nor did a day: the partner did not age and
			 * the daily events (meat farm, ...) were not reset */
			if (dw1_fix()) {
				PARTNER_PARA.age++;
				dailyPStatTrigger();
			}
#endif
			if (0x1d < DAY) {
				DAY = 0;
				++YEAR;
				if (YEAR > 99) {
					YEAR = 0;
				}
			}
		}
	}

	if ((PARTNER_PARA.condition & 4) != 0) {
		PARTNER_PARA.starvationTimer -= 40;
		if ((PARTNER_PARA.starvationTimer < 1) &&
		    (PARTNER_PARA.energyLevel <
		     RAISE_DATA[PARTNER_ENTITY.digimonEntity.entity.type].energyThreshold)) {
			++PARTNER_PARA.careMistakes;
		}
	} else {
		PARTNER_PARA.foodLevel -= 20;
	}

	if ((PARTNER_PARA.condition & 8) != 0) {
		PARTNER_PARA.poopingTimer -= 400;
	} else {
		PARTNER_PARA.poopLevel -= 2;
	}

	updateMinuteHand(HOUR, MINUTE);

	if (result == 1) {
		int8_t flags;

		flags = MAP_ENTRIES[CURRENT_SCREEN].flags & 0x40;
		if (flags == 0) {
			if (HOUR == 0x10) {
				initializeDaytimeTransition(0);
			} else if (HOUR == 0x14) {
				initializeDaytimeTransition(1);
			} else if (HOUR == 6) {
				initializeDaytimeTransition(2);
			}
		}
	}
}

// clang-format off
void tamerSetFullState(state, substate)
	int16_t state;
	int16_t substate;
// clang-format on
{
	TAMER_STATE = state;
	TAMER_SUB_STATE = substate;
}

int32_t tamerGetState(void)
{
	return TAMER_STATE;
}

// clang-format off
void startAnimationTamer(animId)
	int16_t animId;
// clang-format on
{
	startAnimation(&TAMER_ENTITY.entity, animId);
}

// clang-format off
int32_t tickEntityWalkTo(scriptId, targetId, x, z, useCamera)
	uint8_t scriptId;
	uint8_t targetId;
	int16_t x;
	int16_t z;
	int8_t useCamera;
// clang-format on
{
	Entity *e;
	Entity *target;
	VECTOR from;
	VECTOR to;
	int16_t fromTileX;
	int16_t fromTileY;
	int16_t toTileX;
	int16_t toTileY;
	int16_t col;

	col = -1;
	e = getEntityFromScriptId(&scriptId);

	if (scriptId >= 2) {
		NPC_IS_WALKING_TOWARDS[scriptId - 2] = 1;
	}

	from = e->posData->location;

	if (targetId == 0xFF) {
		to.vx = x;
		to.vy = e->posData->location.vy;
		to.vz = z;
	} else {
		target = getEntityFromScriptId(&targetId);
		to = target->posData->location;
	}

	if ((PREVIOUS_CAMERA_POS_INITIALIZED == 0) && (useCamera == 1)) {
		PREVIOUS_CAMERA_POS = from;
		PREVIOUS_CAMERA_POS_INITIALIZED = 1;
	}

	getModelTile(&from, &fromTileX, &fromTileY);
	getModelTile(&to, &toTileX, &toTileY);
	entityLookAtLocation(e, &to);

	if (useCamera == 1) {
		moveCameraByDiff(&PREVIOUS_CAMERA_POS, &from);
		PREVIOUS_CAMERA_POS = from;
	}

	if (targetId != 0xFF) {
		col = entityCheckCollision(0, e, 0, 0);
	}

	if (((fromTileX == toTileX) && (fromTileY == toTileY)) || ((col != -1) && (col < 9))) {
		PREVIOUS_CAMERA_POS_INITIALIZED = 0;

		if (scriptId >= 2) {
			NPC_IS_WALKING_TOWARDS[scriptId - 2] = 0;
		}

		return 1;
	}

	return 0;
}

int32_t tickLookAtEntity(uint8_t scriptId1, uint8_t scriptId2)
{
	Entity *target;
	int32_t result;
	Entity *entity;
	int8_t entityId;
	int32_t i;
	int16_t rotX;
	int16_t rotZ;
	int16_t rotY;

	if (scriptId1 == 0xfd) {
		entityId = 0;
	} else if (scriptId1 == 0xfc) {
		entityId = 1;
	} else {
		for (i = 0; i < 8; i++) {
			if (scriptId1 == NPC_ENTITIES[i].scriptId) {
				entityId = i + 2;
				break;
			}
		}
	}

	result = 0;
#ifdef DW1_NATIVE
	/* turning an NPC that is not on this map, or towards one: entityId is
	 * never set and the target is NULL. Done at once */
	{
		uint8_t s1 = scriptId1, s2 = scriptId2;

		if (getEntityFromScriptId(&s1) == NULL || getEntityFromScriptId(&s2) == NULL) {
			return 1;
		}
	}
#endif
	switch (HAS_ROTATION_DATA[entityId]) {
	case 0:
		target = getEntityFromScriptId(&scriptId2);
		ROTATION_DATA[entityId] = target->posData->location;
		HAS_ROTATION_DATA[entityId] = 1;
		break;
	case 1:
		entity = getEntityFromScriptId(&scriptId1);
		getRotationDifference(entity->posData,
		                      &ROTATION_DATA[entityId],
		                      &rotX, &rotY, &rotZ);
		result = rotateEntity(&entity->posData->rotation, &rotX,
				      &rotY, &rotZ, 0x200);
		if (result == 1) {
			HAS_ROTATION_DATA[entityId] = 0;
		}
		break;
	}

	return result;
}

int32_t tickEntitySetRotation(uint32_t scriptId, int16_t rotationY)
{
	Entity *entity;

	entity = getEntityFromScriptId((uint8_t *)&scriptId);
#ifdef DW1_NATIVE
	if (entity == NULL) {
		return 1; /* NPC not on this map */
	}
#endif
	entity->posData->rotation.vy = rotationY;
	return 1;
}

int32_t tickEntityMoveTo(scriptId1, scriptId2, targetX, targetZ, speed,
			 withCamera)
	uint8_t scriptId1;
	uint8_t scriptId2;
	int16_t targetX;
	int32_t targetZ;
	int8_t speed;
	int8_t withCamera;
{
#ifdef __MWERKS__
	extern void setEntityPosition(int32_t entityId, long x, long y, long z);
#endif
	Entity *entity;
	Entity *targetEntity;
	int8_t finishedX;
	int8_t finishedZ;

	entity = getEntityFromScriptId(&scriptId1);
	if (PREVIOUS_CAMERA_POS_INITIALIZED == 0) {
		if (scriptId2 == 0xff) {
#ifdef DW1_NATIVE
			/* speed is a byte of the script; with 0 the PS1's div gives -1/1
			 * (it does not trap) and the entity snaps to the target next frame */
			MOVE_TO_DELTA_X = dw1_div(targetX - entity->posData->location.vx, speed);
			MOVE_TO_DELTA_Z = dw1_div(targetZ - entity->posData->location.vz, speed);
#else
			MOVE_TO_DELTA_X =
				(targetX - entity->posData->location.vx) / speed;
			MOVE_TO_DELTA_Z =
				(targetZ - entity->posData->location.vz) / speed;
#endif
		} else {
			targetEntity = getEntityFromScriptId(&scriptId2);
#ifdef DW1_NATIVE
			/* speed 0: see above */
			MOVE_TO_DELTA_X = dw1_div(targetEntity->posData->location.vx - entity->posData->location.vx, speed);
			MOVE_TO_DELTA_Z = dw1_div(targetEntity->posData->location.vz - entity->posData->location.vz, speed);
#else
			MOVE_TO_DELTA_X =
				(targetEntity->posData->location.vx -
				 entity->posData->location.vx) / speed;
			MOVE_TO_DELTA_Z =
				(targetEntity->posData->location.vz -
				 entity->posData->location.vz) / speed;
#endif
		}
		PREVIOUS_CAMERA_POS_INITIALIZED = 1;
		PREVIOUS_CAMERA_POS = entity->posData->location;
	} else {
		setEntityPosition(scriptId1,
		                  entity->posData->location.vx + MOVE_TO_DELTA_X,
		                  entity->posData->location.vy,
		                  entity->posData->location.vz + MOVE_TO_DELTA_Z);
		setupEntityMatrix(scriptId1);

		finishedX = finishedZ = 0;
		if (MOVE_TO_DELTA_X < 0) {
			if (targetX >= entity->posData->location.vx) {
				entity->posData->location.vx = targetX;
				finishedX = 1;
			}
#ifdef DW1_NATIVE
		/* original bug: when the distance is shorter than the step count
		 * the step is 0 and an entity short of the target never got there,
		 * freezing the script; with the fix it snaps to it */
		} else if ((MOVE_TO_DELTA_X == 0 && dw1_fix()) ||
		           targetX <= entity->posData->location.vx) {
#else
		} else if (targetX <= entity->posData->location.vx) {
#endif
			entity->posData->location.vx = targetX;
			finishedX = 1;
		}

		if (MOVE_TO_DELTA_Z < 0) {
			if (entity->posData->location.vz <= targetZ) {
				entity->posData->location.vz = targetZ;
				finishedZ = 1;
			}
#ifdef DW1_NATIVE
		} else if ((MOVE_TO_DELTA_Z == 0 && dw1_fix()) || /* original bug: see X */
		           entity->posData->location.vz >= targetZ) {
#else
		} else if (entity->posData->location.vz >= targetZ) {
#endif
			entity->posData->location.vz = targetZ;
			finishedZ = 1;
		}

		if ((finishedX == 1) && (finishedZ == 1)) {
			PREVIOUS_CAMERA_POS_INITIALIZED = 0;
			return 1;
		}
	}

	if (withCamera == 1) {
		moveCameraByDiff(&PREVIOUS_CAMERA_POS,
		                 &entity->posData->location);
		PREVIOUS_CAMERA_POS = entity->posData->location;
	}

	return 0;
}

int32_t tickEntityMoveToAxis(scriptId, target, axis, speed, withCamera)
	uint8_t scriptId;
	int32_t target;
	int8_t axis;
	int8_t speed;
	int8_t withCamera;
{
#ifdef __MWERKS__
	extern void setEntityPosition(int32_t entityId, long x, long y, long z);
#endif
	Entity *entity;
	long *axisValue;
	int8_t finishedX;
	int8_t finishedZ;

	entity = getEntityFromScriptId(&scriptId);
	if (axis == 0) {
		axisValue = &entity->posData->location.vx;
	} else if (axis == 1) {
		axisValue = &entity->posData->location.vy;
	} else {
		axisValue = &entity->posData->location.vz;
	}

	if (PREVIOUS_CAMERA_POS_INITIALIZED == 0) {
#ifdef DW1_NATIVE
		/* speed is a byte of the script; the PS1's div does not trap on 0 */
		MOVE_TO_DELTA_X = dw1_div(target - *axisValue, speed);
#else
		MOVE_TO_DELTA_X = (target - *axisValue) / speed;
#endif
		PREVIOUS_CAMERA_POS = entity->posData->location;
		PREVIOUS_CAMERA_POS_INITIALIZED = 1;
	} else {
		*axisValue += MOVE_TO_DELTA_X;
		setEntityPosition(scriptId, entity->posData->location.vx,
		                  entity->posData->location.vy,
		                  entity->posData->location.vz);
		setupEntityMatrix(scriptId);
		finishedX = finishedZ = 0;

		if (MOVE_TO_DELTA_X < 0) {
			if (*axisValue <= target) {
				*axisValue = target;
				PREVIOUS_CAMERA_POS_INITIALIZED = 0;
				return 1;
			}
#ifdef DW1_NATIVE
		/* original bug: a step of 0 never arrived, as in tickEntityMoveTo */
		} else if ((MOVE_TO_DELTA_X == 0 && dw1_fix()) || *axisValue >= target) {
#else
		} else if (*axisValue >= target) {
#endif
			*axisValue = target;
			PREVIOUS_CAMERA_POS_INITIALIZED = 0;
			return 1;
		}
	}

	if (withCamera == 1) {
		moveCameraByDiff(&PREVIOUS_CAMERA_POS,
		                 &entity->posData->location);
		PREVIOUS_CAMERA_POS = entity->posData->location;
	}
	return 0;
}

Entity *getEntityFromScriptId(uint8_t *scriptId)
{
	int32_t i;

	if (*scriptId == 0xfd) {
		*scriptId = 0;
		return ENTITY_TABLE[0];
	}
	if (*scriptId == 0xfc) {
		*scriptId = 1;
		return ENTITY_TABLE[1];
	}
	for (i = 0; i < 8; i++) {
		if ((ENTITY_TABLE[i + 2] != 0) &&
		    (*scriptId == NPC_ENTITIES[i].scriptId)) {
			*scriptId = i + 2;
			return ENTITY_TABLE[i + 2];
		}
	}
#ifdef DW1_NATIVE
	/* not on this map: the original falls off the end and returns
	 * whatever was left in v0 */
	return NULL;
#endif
}

void worldPosToScreenPos2(int16_t *x, int16_t *y, int16_t *z)
{
	SVECTOR v;
	SVECTOR sxy;

	SetRotMatrix(&GsWSMATRIX);
	SetTransMatrix(&GsWSMATRIX);
	v.vx = *x;
	v.vy = *y;
	v.vz = *z;
	gte_ldv0(&v);
	gte_rtps();
	gte_stsxy((long *)&sxy);
	*x = (int16_t)sxy.vx - (0xA0 - DRAWING_OFFSET_X);
	*y = sxy.vy - (0x78 - DRAWING_OFFSET_Y);
}

uint8_t checkChestCollision(void)
{
	int32_t i;
	int16_t tileX;
	int16_t tileY;

	getModelTile(&TAMER_ENTITY.entity.posData->location, &tileX, &tileY);

	for (i = 0; i < 8; ++i) {
		if ((CHEST_ARRAY[i].item != 0xff) &&
		    (CHEST_ARRAY[i].tileX - 1) <= tileX &&
		    (tileX <= CHEST_ARRAY[i].tileX + 1) &&
		    (CHEST_ARRAY[i].tileY - 1) <= tileY &&
		    (tileY <= CHEST_ARRAY[i].tileY + 1)) {
			return i;
		}
	}

	return 0xff;
}

// clang-format off
void addTamerLevel(chance, amount)
	int16_t chance;
	int8_t amount;
// clang-format on
{
	int32_t r;

	r = randomLimit(100);
	if (r < chance) {
		TAMER_ENTITY.tamerLevel += amount;
		if ((0 <= TAMER_ENTITY.tamerLevel) &&
		    (TAMER_ENTITY.tamerLevel < 11)) {
			clearTextArea();

			if (amount >= 1) {
				TAMER_LEVELS_AWARDED = 1;
			} else {
				TAMER_LEVELS_AWARDED = -1;
			}

			TAMER_LEVEL_AWARD_PENDING = 1;
		}

		if (TAMER_ENTITY.tamerLevel < 0) {
			TAMER_ENTITY.tamerLevel = 0;
		}

		if (TAMER_ENTITY.tamerLevel > 10) {
			TAMER_ENTITY.tamerLevel = 10;
		}
	}
}

int32_t isTrainingComplete(void)
{
	if (TRAINING_COMPLETE == 1) {
		TRAINING_COMPLETE = 0;
		return 1;
	}
	return 0;
}

void renderAwardSomethingTextbox(int32_t instanceId)
{
	renderString(0, 0xffffff83, 0x2d, 0xf0, 0x24, 0, 0x78, 5, 0);
	renderUIBox(1);
	++TEXTBOX_OPEN_TIMER;
}

// clang-format off
void tamerSetState(state)
	int16_t state;
// clang-format on
{
	TAMER_STATE = state;
	TAMER_SUB_STATE = 0;
}

void tamerTickChangeMap(void)
{
	switch (TAMER_SUB_STATE) {
	case 0:
		fadeToBlack(0x14);
		startAnimationTamer(2);
		TAMER_SUB_STATE = 1;
		break;
	case 1:
		if (FADE_OUT_CURRENT == 10) {
			addMapNameObject(TARGET_MAP);
		}
		if (0x13 < FADE_OUT_CURRENT) {
			changeMap(TARGET_MAP, CURRENT_EXIT);
			STORED_TAMER_POS = TAMER_ENTITY.entity.posData->location;
			fadeFromBlack(0x14);
			removeObject(0xfa1, TARGET_MAP);
			TAMER_SUB_STATE = 2;
		}
		break;
	case 2:
		if (0x13 < FADE_IN_CURRENT) {
			tamerSetState(0);
			partnerSetState(1);
			checkMapInteraction();
			STORED_TAMER_POS = TAMER_ENTITY.entity.posData->location;
			startGameTime();
		}
	default:
		break;
	}
}

void tamerTickPickupItem(void)
{
	RECT textRect;
#if VERSION_IS(EU)
	RECT lineRect;
#endif
	RECT targetRect;
	RECT sourceRect;
	DVECTOR screenPos;

	textRect = ITEM_PICKUP_TEXT_AREA;
#if VERSION_IS(EU)
	lineRect = ITEM_PICKUP_SECOND_LINE;
#endif

	switch (TAMER_SUB_STATE) {
	case 0:
		startAnimation(ENTITY_TABLE[0], 0xc);
		unsetCameraFollowPlayer();
		clearTextSubArea(&textRect);
		drawString(DIGIMON_NAME(0), 0, 0xc);
		drawString(ITEM_NAME(DROPPED_ITEMS[PICKED_UP_DROP_ID].worldItem.type),
		           0, 0x18);
		drawString(MAIN_D_801341FC, 0, 0x24);
		TAKE_CHEST_STATE = 0;
		TAKE_ITEM_FRAME_COUNTER = 0;
		TAMER_SUB_STATE = 1;
		break;
	case 1:
		if (isUIBoxAvailable(1) == 1) {
			setRECT(&targetRect, -130, 42, 262, 59);
			getEntityScreenPos(ENTITY_TABLE[0], 1, &screenPos);
			setRECT(&sourceRect,
				screenPos.vx - 5,
				screenPos.vy - 5,
				10, 10);
			TAKE_CHEST_ITEM =
				DROPPED_ITEMS[PICKED_UP_DROP_ID].worldItem.type;
			createAnimatedUIBox(1, 0, 2, &targetRect, &sourceRect, 0,
			                    renderItemPickupTextbox);
			TAMER_SUB_STATE = 2;
		}
		break;
	case 2:
		++TAKE_ITEM_FRAME_COUNTER;
		if ((isKeyDown(CONFIRM_BUTTON) != 0) && (4 < TAKE_ITEM_FRAME_COUNTER)) {
			if (TAKE_ITEM_FRAME_COUNTER < 0x3c) {
				playSound(0, 3);
			}
			if (giveItem(DROPPED_ITEMS[PICKED_UP_DROP_ID].worldItem.type, 0) == 0) {
#if VERSION_IS(EU)
				clearTextSubArea(&lineRect);
#endif
				drawString(MAIN_D_80122D68, 0, 0x18);
				TAKE_ITEM_FRAME_COUNTER = 0;
				TAKE_CHEST_STATE = 1;
				TAMER_SUB_STATE = 3;
			} else {
				playSound(0, 7);
				TAMER_SUB_STATE = 4;
			}
		}
		break;
	case 3:
		++TAKE_ITEM_FRAME_COUNTER;
		if ((isKeyDown(CONFIRM_BUTTON) != 0) && (4 < TAKE_ITEM_FRAME_COUNTER)) {
			if (TAKE_ITEM_FRAME_COUNTER < 0x3c) {
				playSound(0, 3);
			}
			TAMER_SUB_STATE = 4;
		}
		break;
	case 4:
		getEntityScreenPos(ENTITY_TABLE[0], 1, &screenPos);
		setRECT(&textRect,
			screenPos.vx - 5,
			screenPos.vy - 5,
			10, 10);
		removeAnimatedUIBox(1, &textRect);
		if (TAKE_CHEST_STATE == 0) {
			pickupItem(PICKED_UP_DROP_ID);
		}
		tamerSetState(0);
		setCameraFollowPlayer();
	default:
		break;
	}

	if (9 < TAKE_ITEM_FRAME_COUNTER) {
		TAKE_ITEM_FRAME_COUNTER = 10;
	}
}

void tamerTickTakeChest(void)
{
	RECT textRect;
#if VERSION_IS(EU)
	RECT lineRect;
#endif
	RECT targetRect;
	RECT sourceRect;
	DVECTOR screenPos;

	textRect = TAKE_CHEST_TEXT_AREA;
#if VERSION_IS(EU)
	lineRect = TAKE_CHEST_SECOND_LINE;
#endif

	switch (TAMER_SUB_STATE) {
	case 0:
		startAnimation(ENTITY_TABLE[0], 0);
		partnerSetState(0xb);
		unsetCameraFollowPlayer();
		entityLookAtLocation(ENTITY_TABLE[0],
				     &CHEST_ARRAY[INTERACTED_CHEST].location);
		clearTextSubArea(&textRect);
		drawString(DIGIMON_NAME(0), 0, 0xc);
		if (CHEST_ARRAY[INTERACTED_CHEST].isTaken == 0) {
			drawString(ITEM_NAME(CHEST_ARRAY[INTERACTED_CHEST].item),
			           0, 0x18);
			drawString(MAIN_D_801341FC, 0, 0x24);
			TAKE_CHEST_STATE = 0;
		} else {
			drawString(MAIN_D_80122D80, 0, 0x18);
			TAKE_CHEST_STATE = 2;
		}
		TAMER_SUB_STATE = 1;
		break;
	case 1:
		if (isUIBoxAvailable(1) == 1) {
			if (tickOpenChestTray(INTERACTED_CHEST) == 1) {
				setRECT(&targetRect, -130, 42, 262, 59);
				getEntityScreenPos(ENTITY_TABLE[0], 1,
						   &screenPos);
				setRECT(&sourceRect,
					screenPos.vx - 5,
					screenPos.vy - 5,
					10, 10);
				TAKE_CHEST_ITEM =
					CHEST_ARRAY[INTERACTED_CHEST].item;
				createAnimatedUIBox(1, 0, 2, &targetRect,
						    &sourceRect, 0,
				                    renderItemPickupTextbox);
				TAKE_ITEM_FRAME_COUNTER = 0;
				if (TAKE_CHEST_STATE == 0) {
					TAMER_SUB_STATE = 2;
				} else {
					TAMER_SUB_STATE = 4;
				}
			}
		}
		break;
	case 2:
		++TAKE_ITEM_FRAME_COUNTER;
		if (((POLLED_INPUT & CONFIRM_BUTTON) != 0) &&
		    (5 < TAKE_ITEM_FRAME_COUNTER)) {
			TAKE_ITEM_FRAME_COUNTER = 0;
			if (giveItem(TAKE_CHEST_ITEM, 0) == 0) {
#if VERSION_IS(EU)
				clearTextSubArea(&lineRect);
#endif
				drawString(MAIN_D_80122D68, 0, 0x18);
				TAKE_CHEST_STATE = 1;
				TAMER_SUB_STATE = 3;
			} else {
				setTrigger(CHEST_ARRAY[INTERACTED_CHEST].trigger);
				TAMER_SUB_STATE = 4;
			}
		}
		break;
	case 3:
		if (tickCloseChestTray(INTERACTED_CHEST) == 1) {
			TAMER_SUB_STATE = 4;
		}
		break;
	case 4:
		++TAKE_ITEM_FRAME_COUNTER;
		if (((POLLED_INPUT & CONFIRM_BUTTON) != 0) &&
		    (5 < TAKE_ITEM_FRAME_COUNTER)) {
			getEntityScreenPos(&TAMER_ENTITY.entity, 1,
					   &screenPos);
			setRECT(&textRect,
				screenPos.vx - 5,
				screenPos.vy - 5,
				10, 10);
			removeAnimatedUIBox(1, &textRect);
			if (TAKE_CHEST_STATE == 0) {
				giveItem(TAKE_CHEST_ITEM, 1);
			}
			tamerSetState(0);
			partnerSetState(1);
			setCameraFollowPlayer();
		}
	default:
		break;
	}

	if (9 < TAKE_ITEM_FRAME_COUNTER) {
		TAKE_ITEM_FRAME_COUNTER = 10;
	}
}

void tamerTickIdle(void)
{
	if (TAMER_SUB_STATE == 0) {
		startAnimation(&TAMER_ENTITY.entity, 0);
		TAMER_SUB_STATE = 1;
	}
}

void tamerTickTraining(void)
{
	switch (TAMER_SUB_STATE) {
	case 0:
		startAnimation(&TAMER_ENTITY.entity, 10);
		TAMER_SUB_STATE = 1;
		break;
	case 1:
		entityLookAtLocation(&TAMER_ENTITY.entity,
		                     &PARTNER_ENTITY.digimonEntity.entity.posData->location);
	default:
		break;
	}
}

void tamerTickPraiseScold(int8_t state)
{
	int32_t type;

	switch (TAMER_SUB_STATE) {
	case 0:
		if (state == 0xd) {
			type = PARTNER_ENTITY.digimonEntity.entity.type;
			if ((DIGIMON_DATA[type].level < 4) ||
			    (type == 0xd) ||
			    (type == 0x1b) ||
			    (type == 0x38)) {
				startAnimation(&TAMER_ENTITY.entity, 0x26);
			} else {
				startAnimation(&TAMER_ENTITY.entity, 0x27);
			}
			startAnimation(&PARTNER_ENTITY.digimonEntity.entity,
				       0xb);

			TAMER_SUB_STATE = 1;
		} else {
			playSound(0, 0xd);
			startAnimation(&TAMER_ENTITY.entity, 7);
			if (ITEM_SCOLD_FLAG == 1) {
				startAnimation(&PARTNER_ENTITY.digimonEntity.entity,
					       0x19);
			} else {
				startAnimation(&PARTNER_ENTITY.digimonEntity.entity,
					       0xc);
			}
		}
		TAMER_SUB_STATE = 1;
		break;
	case 1:
		if (TAMER_ENTITY.entity.anim.animFrame >=
		    TAMER_ENTITY.entity.anim.frameCount) {
			tamerSetState(6);
		}
	default:
		break;
	}
}

void tamerTickOpening(void)
{
	switch (TAMER_SUB_STATE) {
	case 0:
		TAMER_SUB_STATE = 1;
		break;
	case 1:
		setMapLayerEnabled(1);
		SOME_SCRIPT_SYNC_BIT = 1;
	default:
		break;
	}
}

void tamerTickEnding(void)
{
	switch (TAMER_SUB_STATE) {
	case 0:
		isSoundLoaded(0, 8);
		ENDI_tickEnding(ENTITY_TABLE[0], 0);
		TAMER_SUB_STATE = 1;
		break;
	case 1:
		if (0 > ENDI_tickEnding(ENTITY_TABLE[0], 1)) {
			SOME_SCRIPT_SYNC_BIT = 1;
		}
	default:
		break;
	}
}

void tamerTickSicknessLostLife(void)
{
	switch (TAMER_SUB_STATE) {
	case 0:
		loadDynamicLibrary(MURD_REL, 0, 0, 0, 0);
		MURD_tick((PartnerEntity *)ENTITY_TABLE[1], 0);
		TAMER_SUB_STATE = 1;
		break;
	case 1:
		if ((0 > MURD_tick((PartnerEntity *)ENTITY_TABLE[1], 1)) &&
		    (0 > DOOA_tick((PartnerEntity *)ENTITY_TABLE[1],
				   GENERAL_BUFFER_PTR + 0x4b000, 1))) {
			SOME_SCRIPT_SYNC_BIT = 1;
		}
	default:
		break;
	}
}

void tamerTickMachinedramonSpawn(void)
{
	switch (TAMER_SUB_STATE) {
	case 0:
		EAB_tick(ENTITY_TABLE[2], 0);
		TAMER_SUB_STATE = 1;
		break;
	case 1:
		if (0 > EAB_tick(ENTITY_TABLE[2], 1)) {
			SOME_SCRIPT_SYNC_BIT = 1;
		}
	default:
		break;
	}
}

void tamerTickBattleLostLife(void)
{
	switch (TAMER_SUB_STATE) {
	case 0:
		loadDynamicLibrary(MURD_REL, 0, 0, 0, 0);
		MURD_tick((PartnerEntity *)ENTITY_TABLE[1], 0);
		TAMER_SUB_STATE = 1;
		break;
	case 1:
		if (0 > MURD_tick((PartnerEntity *)ENTITY_TABLE[1], 1)) {
			SOME_SCRIPT_SYNC_BIT = 1;
		}
	default:
		break;
	}
}

void tamerTickAwardSomething(void)
{
	RECT textRect;
	RECT targetRect;
	RECT sourceRect;
	DVECTOR screenPos;

	textRect = AWARD_SOMETHING_TEXT_AREA;

	switch (TAMER_SUB_STATE) {
	case 0:
		stopGameTime();
		startAnimation(&TAMER_ENTITY.entity, 0);
		partnerSetState(0xb);
		unsetCameraFollowPlayer();
		if (MEDAL_AWARD_PENDING == 1) {
			TAMER_SUB_STATE = 1;
		} else if (TAMER_LEVEL_AWARD_PENDING == 1) {
			clearTextArea();
			if (TAMER_LEVELS_AWARDED == 1) {
				setTextColor(7);
				drawString(MAIN_D_80122D94, 0, 0x78);
			} else {
				setTextColor(3);
				drawString(MAIN_D_80122DAC, 0, 0x78);
			}
			setTextColor(1);
			TAMER_SUB_STATE = 4;
		} else {
			TAMER_SUB_STATE = 4;
		}
		break;
	case 1:
		clearTextArea();
		setTextColor(7);
		drawString(MAIN_D_80122DC8, 0, 0x78);
		TAMER_SUB_STATE = 2;
		break;
	case 2:
		drawString(MAIN_D_80122DDC, 0, 0x84);
		TAMER_SUB_STATE = 3;
		break;
	case 3:
		drawString(MAIN_D_80122DF4, 0, 0x90);
		setTextColor(1);
		TAMER_SUB_STATE = 4;
		break;
	case 4:
		if (isUIBoxAvailable(1) == 1) {
			setRECT(&targetRect, -130, 42, 262, 59);
			getEntityScreenPos(ENTITY_TABLE[0], 1, &screenPos);
			setRECT(&sourceRect,
				screenPos.vx - 5,
				screenPos.vy - 5,
				10, 10);
			TAKE_CHEST_ITEM =
				CHEST_ARRAY[INTERACTED_CHEST].item;
			createAnimatedUIBox(1, 0, 2, &targetRect, &sourceRect,
					    0, renderAwardSomethingTextbox);
			TAMER_SUB_STATE = 5;
			playSound(0, 7);
		}
		break;
	case 5:
		if ((POLLED_INPUT & CONFIRM_BUTTON) != 0) {
			TAKE_ITEM_FRAME_COUNTER = 0;
			getEntityScreenPos(&TAMER_ENTITY.entity, 1,
					   &screenPos);
			setRECT(&textRect,
				screenPos.vx - 5,
				screenPos.vy - 5,
				10, 10);
			removeAnimatedUIBox(1, &textRect);
			tamerSetState(0);
			partnerSetState(1);
			setCameraFollowPlayer();
			startGameTime();
			MEDAL_AWARD_PENDING = 0;
			TAMER_LEVEL_AWARD_PENDING = 0;
			startGameTime();
			setTextColor(1);
		}
	default:
		break;
	}
}

#ifdef DW1_NATIVE
/* Controls the PS1 did not have, all from the walking state (port/src/
 * touch_input.c reads the host side):
 *
 *   L1               opens the inventory straight away; Confirm on an item
 *                    the partner can take feeds it at once (Use in the item
 *                    menu), Cancel closes everything (game/src/main/
 *                    inventory.c). DW1_QUICKFEED=0 turns it off.
 *   touch the partner  praise, as Praise in the Triangle menu
 *   left stick       walks any way, not only the D-pad's eight
 *                    (DW1_STICK=0 turns it off)
 *   touch elsewhere  the tamer walks to that spot on the ground (runs when
 *                    it is far), steering while the finger moves; the D-pad
 *                    or the stick takes over. DW1_TOUCH=0 turns both off.
 *
 * The walk asks for D-pad bits in the next PadRead (dw1_touch_pad), so the
 * game walks as with the pad, and turns the tamer to the exact angle. */
#include <stdlib.h>

#define DW1_TOUCH_DOWN 1
#define DW1_TOUCH_BEGAN 2
int dw1_touch_state(int *x, int *y);
int dw1_pad_manual_move(void);
int dw1_stick_dir(int *x, int *y);
int dw1_quick_feed_enabled(void);
unsigned dw1_pad_calls(void);
extern unsigned int dw1_touch_pad;
extern int32_t TRIANGLE_MENU_STATE;
void drawInventoryText(void);
extern uint32_t POLLED_INPUT_PREVIOUS;

int dw1_quick_feed;

static int32_t dw1WalkActive;  /* walking to dw1WalkX/Z */
static int32_t dw1WalkX, dw1WalkZ;
static int16_t dw1WalkAngle;
static int32_t dw1TouchIgnored; /* this touch began on the partner */
static unsigned dw1LastTick;
static int32_t dw1StuckX, dw1StuckZ, dw1StuckFrames;

static void dw1ToScreen(int32_t x, int32_t y, int32_t z, float *sx, float *sy)
{
	int16_t px, py, pz;

	px = (int16_t)x;
	py = (int16_t)y;
	pz = (int16_t)z;
	worldPosToScreenPos2(&px, &py, &pz);
	*sx = px;
	*sy = py;
}

/* the point on the ground (at the tamer's height) under screen point
 * tx, ty: Newton steps on the GTE projection, kept near the tamer */
static void dw1ScreenToGround(int32_t tx, int32_t ty, int32_t *outX, int32_t *outZ)
{
	VECTOR *loc = &TAMER_ENTITY.entity.posData->location;
	float x = loc->vx, z = loc->vz, y = loc->vy;
	float sx, sy, ax, ay, bx, by, det, dx, dz;
	int32_t i;

	for (i = 0; i < 4; i++) {
		dw1ToScreen(x, y, z, &sx, &sy);
		dw1ToScreen(x + 64, y, z, &ax, &ay);
		dw1ToScreen(x, y, z + 64, &bx, &by);
		ax -= sx;
		ay -= sy;
		bx -= sx;
		by -= sy;
		det = ax * by - ay * bx;
		if (det > -0.5f && det < 0.5f) {
			break;
		}
		dx = 64 * ((tx - sx) * by - (ty - sy) * bx) / det;
		dz = 64 * (ax * (ty - sy) - ay * (tx - sx)) / det;
		if (dx > 2000) dx = 2000;
		if (dx < -2000) dx = -2000;
		if (dz > 2000) dz = 2000;
		if (dz < -2000) dz = -2000;
		x += dx;
		z += dz;
		if (x > loc->vx + 4000) x = loc->vx + 4000;
		if (x < loc->vx - 4000) x = loc->vx - 4000;
		if (z > loc->vz + 4000) z = loc->vz + 4000;
		if (z < loc->vz - 4000) z = loc->vz - 4000;
		if (dx > -4 && dx < 4 && dz > -4 && dz < 4) {
			break;
		}
	}
	*outX = (int32_t)x;
	*outZ = (int32_t)z;
}

static int32_t dw1TouchOnPartner(int32_t tx, int32_t ty)
{
	DVECTOR p;

	if (!PARTNER_ENTITY.digimonEntity.entity.isOnScreen) {
		return 0;
	}
	getEntityScreenPos(&PARTNER_ENTITY.digimonEntity.entity, 1, &p);
	/* a generous box around the body: fingers are wide */
	return tx >= p.vx - 36 && tx <= p.vx + 36 &&
	       ty >= p.vy - 48 && ty <= p.vy + 32;
}

/* 1 when it changed the tamer's state (walking stops for this tick) */
static int32_t dw1TamerExtraInput(void)
{
	int32_t canMenu, touch, tx, ty;
	unsigned now;

	canMenu = (IS_SCRIPT_PAUSED == 1) &&
#if VERSION_EQUAL_OR_NEWER(US)
	          (FADE_PROTECTION == 0) &&
#endif
	          (UI_BOX_DATA[0].state != 1) && (UI_BOX_DATA[0].frame == 0);

	/* L1: the inventory, as Items in the Triangle menu */
	if (canMenu && dw1_quick_feed_enabled() &&
	    ((POLLED_INPUT & ~POLLED_INPUT_PREVIOUS) & 4) &&
	    (TAMER_ITEM.worldItem.type == 0xff)) {
		dw1WalkActive = 0;
		addGameMenu();
		tamerSetState(1);
		startAnimation(&TAMER_ENTITY.entity, 0);
		unsetCameraFollowPlayer();
		copyVector(&STORED_TAMER_POS, &TAMER_ENTITY.entity.posData->location);
		IS_IN_MENU = 1;
		stopGameTime();
		setPartnerIdling();
		/* straight to the inventory, as Items (handleGameMenuSelection(1)):
		 * the item names go to the text area first */
		TRIANGLE_MENU_STATE = 2;
		drawInventoryText();
		dw1_quick_feed = 1;
		playSound(0, 3);
		return 1;
	}

	now = dw1_pad_calls();
	if (now - dw1LastTick > 2) {
		dw1WalkActive = 0; /* another map, a menu, a script... */
	}
	dw1LastTick = now;

	touch = dw1_touch_state(&tx, &ty);
	if (dw1_pad_manual_move()) {
		dw1WalkActive = 0;
		return 0;
	}
	if (!(touch & DW1_TOUCH_DOWN)) {
		dw1TouchIgnored = 0;
	} else if (touch & DW1_TOUCH_BEGAN) {
		dw1TouchIgnored = dw1TouchOnPartner(tx, ty);
		if (dw1TouchIgnored && canMenu) {
			/* Praise, as handleGameMenuSelection(4) */
			dw1WalkActive = 0;
			tamerSetState(6);
			startAnimation(&TAMER_ENTITY.entity, 0);
			unsetCameraFollowPlayer();
			copyVector(&STORED_TAMER_POS, &TAMER_ENTITY.entity.posData->location);
			setPartnerIdling();
			partnerSetState(0xf);
			return 1;
		}
	}
	if ((touch & DW1_TOUCH_DOWN) && !dw1TouchIgnored && (IS_SCRIPT_PAUSED == 1)) {
		if (!dw1WalkActive) {
			dw1StuckX = TAMER_ENTITY.entity.posData->location.vx;
			dw1StuckZ = TAMER_ENTITY.entity.posData->location.vz;
			dw1StuckFrames = 0;
		}
		dw1ScreenToGround(tx, ty, &dw1WalkX, &dw1WalkZ);
		dw1WalkActive = 1;
	}

	if (dw1WalkActive) {
		VECTOR *loc = &TAMER_ENTITY.entity.posData->location;
		float sx, sy, gx, gy, ex, ey;
		uint32_t bits = 0;

		dw1ToScreen(loc->vx, loc->vy, loc->vz, &sx, &sy);
		dw1ToScreen(dw1WalkX, loc->vy, dw1WalkZ, &gx, &gy);
		ex = gx - sx;
		ey = gy - sy;
		if (ex * ex + ey * ey < 8 * 8) {
			dw1WalkActive = 0; /* there */
			return 0;
		}
		/* against a wall: give up after half a second without moving */
		if (abs(loc->vx - dw1StuckX) + abs(loc->vz - dw1StuckZ) > 8) {
			dw1StuckX = loc->vx;
			dw1StuckZ = loc->vz;
			dw1StuckFrames = 0;
		} else if (++dw1StuckFrames > 30 && !(touch & DW1_TOUCH_DOWN)) {
			dw1WalkActive = 0;
			return 0;
		}
		/* the D-pad bits nearest the screen direction, for the game's
		 * walking; dw1TouchWalkFace sets the exact angle */
		if (ex * 5 >= ey * 2 && ex * 5 >= -ey * 2) bits |= 0x2000;
		if (-ex * 5 >= ey * 2 && -ex * 5 >= -ey * 2) bits |= 0x8000;
		if (ey * 5 >= ex * 2 && ey * 5 >= -ex * 2) bits |= 0x4000;
		if (-ey * 5 >= ex * 2 && -ey * 5 >= -ex * 2) bits |= 0x1000;
		if (ex * ex + ey * ey > 64 * 64) {
			bits |= 8; /* R1: run */
		}
		dw1_touch_pad = bits;
		dw1WalkAngle = (int16_t)(_atan(dw1WalkZ - loc->vz, dw1WalkX - loc->vx) & 0xfff);
	}
	return 0;
}

static void dw1TouchWalkFace(void)
{
	int x, y;

	/* the stick walks any way: the D-pad bits it gave set the walk, its
	 * angle (down the screen 0, left 0x400) turns the tamer, relative to
	 * the camera quantised as for the D-pad above */
	if (dw1_stick_dir(&x, &y) && (POLLED_INPUT & 0xf000)) {
		int16_t rotation = getMapRotation();
		int16_t quadrant = (int16_t)(rotation / 0x200);

		if ((rotation % 0x200) >= 0x100) {
			quadrant++;
		}
		/* _atan(a, b) is the angle from +a towards +b, plus 0x800
		 * (game/src/main/math.c): from down (+y) towards left (-x) */
		setTamerDirection((int16_t)((quadrant * 0x200 + _atan(y / 16, -x / 16) - 0x800) & 0xfff));
		return;
	}
	if (dw1WalkActive && !dw1_pad_manual_move() &&
	    (POLLED_INPUT & 0xf000)) {
		setTamerDirection(dw1WalkAngle);
	}
}
#endif
