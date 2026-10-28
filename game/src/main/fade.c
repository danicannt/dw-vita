#include <libgs.h>

#include <dw/clock.h>
#include <dw/fade.h>
#include <dw/version.h>

#include "common.h"

int16_t FADE_DATA;
int16_t FADE_IN_TARGET;
int16_t FADE_OUT_CURRENT;
int16_t FADE_IN_CURRENT;
uint8_t FADE_PROGRESS;
uint8_t FADE_MODE;
int32_t FADE_PROTECTION;
uint8_t FADE_OUT_IN_PROGRESS;

int32_t addObject(int16_t objectId, int16_t instanceId, void *tick, void *render);
int32_t removeObject(int16_t objectId, int16_t instanceId);
void setPosDataPolyFT4(POLY_FT4 *prim, int16_t posX, int16_t posY, int16_t width, int16_t height);
void setUVDataPolyFT4(POLY_FT4 *prim, int16_t xPos, int16_t yPos, int16_t width, int16_t height);
extern GsOT *ACTIVE_ORDERING_TABLE;

#if VERSION_IS(EU)
static void *fade_functions[] = {
	fadeFromWhite,
	fadeToWhite,
	renderFade,
	fadeFromBlack,
	renderFadeIn,
	fadeToBlack,
	renderFadeOut,
	initializeFadeData,
};

static void *fade_sbss_order[] = {
	&FADE_OUT_IN_PROGRESS,
	&FADE_OUT_CURRENT,
	&FADE_IN_CURRENT,
	&FADE_PROGRESS,
	&FADE_MODE,
	&FADE_PROTECTION,
	&FADE_DATA,
	&FADE_IN_TARGET,
};
#endif

// Garbage function to force sbss symbol order and ensure
// correct codegen for renderFadeOut()
static void __garbage__()
{
	FADE_DATA /= 7;
	FADE_IN_TARGET /= 7;
	FADE_OUT_CURRENT = 0;
	FADE_IN_CURRENT = 0;
	FADE_PROGRESS = 0;
	FADE_MODE = 0;
	FADE_PROTECTION = 0;
	FADE_OUT_IN_PROGRESS = 0;
}

void initializeFadeData(void)
{
	FADE_DATA = 0;
	FADE_OUT_CURRENT = 0;
	FADE_IN_TARGET = 0;
	FADE_IN_CURRENT = 0;
	FADE_PROGRESS = 0;
	FADE_MODE = 2;
	FADE_PROTECTION = 0;
}

void fadeToBlack(int16_t frames)
{
	if (FADE_OUT_CURRENT || FADE_IN_CURRENT) {
		removeObject(4005, 0);
	}

	FADE_DATA = frames + 1;
	FADE_OUT_CURRENT = 1;
	FADE_MODE = 2;
	addObject(4005, 0, 0, renderFadeOut);
	stopGameTime();
	FADE_PROTECTION = 1;
}

void fadeFromBlack(int16_t frames)
{
	removeObject(4005, 0);
	FADE_OUT_CURRENT = 0;
#if VERSION_IS(JP)
	removeObject(4005, 0);
#endif

	FADE_IN_TARGET = frames + 1;
	FADE_IN_CURRENT = 1;
	FADE_MODE = 2;
	addObject(4005, 0, 0, renderFadeIn);
}

void renderFadeIn(int16_t instanceId)
{
	int16_t temp;
	uint8_t progress;

	if (FADE_IN_CURRENT < FADE_IN_TARGET) {
		temp = FADE_IN_CURRENT * (160 / FADE_IN_TARGET);

		if (temp < 1) {
			temp = FADE_IN_CURRENT;
		}

		FADE_PROGRESS = progress = 160 - temp;

		if (progress < 0) {
			FADE_PROGRESS = progress = 0;
		}

		renderFade(progress);

		FADE_IN_CURRENT++;
	} else {
		FADE_IN_CURRENT = 0;
		removeObject(4005, instanceId);
		FADE_PROTECTION = 0;
	}
}

void renderFadeOut(void)
{
	uint8_t next;

#ifdef DW1_NATIVE
	/* FADE_DATA is frames + 1 and fadeToWhite takes frames from the script
	 * (0xffff gives 0); the PS1's div does not trap */
	next = FADE_PROGRESS += dw1_div(160, FADE_DATA);
#else
	next = FADE_PROGRESS += 160 / FADE_DATA;
#endif

	if (160 < next) {
		FADE_PROGRESS = next = 160;
	}

	renderFade(next);

	FADE_OUT_CURRENT++;
}

#ifdef DW1_NATIVE
/* frames asked about through dw1_fade_black, and that count + 1 when
 * renderFade last drew the screen fully black (0: never) */
static unsigned fade_frames, fade_black_frame;

/* 1 while a fade to black covers the whole screen, for what is drawn under
 * it and need not be: call once per frame. Black means drawn black this frame
 * or the one before (the order of the two in a frame is not fixed), so once
 * the fade lifts, at most one frame of its nearly black start misses it. */
int dw1_fade_black(void)
{
	fade_frames++;
	return fade_black_frame != 0 && fade_frames + 1 - fade_black_frame <= 1;
}
#endif

void renderFade(uint8_t progress)
{
	POLY_FT4 *prim;

#ifdef DW1_NATIVE
	/* The PS1 quad samples texels of CLUT index 0, which is 0xffff (white,
	 * semi-transparent): it blends a flat colour, white scaled by progress.
	 * Drawn untextured instead, that colour costs one pass of the renderer's
	 * cheap untextured shader over the screen, where the textured subtractive
	 * quad took two passes of the full one: on the Vita every fade (map
	 * changes, menus, the title) ran at 15-20 fps. At progress 0 it blends
	 * nothing and is skipped. */
	{
		POLY_F4 *flat;
		DR_TPAGE *mode;
		int c = 31 * progress >> 7; /* the 5-bit texel 31 times progress / 128 */
		int order = FADE_MODE == 2 ? 1 : 10;

		if (c == 0)
			return;
		if (c > 31)
			c = 31;
		if (c == 31 && FADE_MODE == 2)
			fade_black_frame = fade_frames + 1;
		flat = (POLY_F4 *)GsGetWorkBase();
		setPolyF4(flat);
		setSemiTrans(flat, 1);
		setRGB0(flat, c << 3, c << 3, c << 3);
		setXYWH(flat, -160 - dw1_wide_extra, -120, 320 + 2 * dw1_wide_extra, 240);
		addPrim(&ACTIVE_ORDERING_TABLE->org[order], flat);
		/* blend mode for the untextured quad, added after it so it runs first */
		mode = (DR_TPAGE *)&flat[1];
		setDrawTPage(mode, 1, 1, GetTPage(0, FADE_MODE, 896, 448));
		addPrim(&ACTIVE_ORDERING_TABLE->org[order], mode);
		GsSetWorkBase((PACKET *)&mode[1]);
		return;
	}
#endif
	prim = (POLY_FT4 *)GsGetWorkBase();

	SetPolyFT4(prim);
	SetSemiTrans(prim, 1);
	prim->tpage = GetTPage(0, FADE_MODE, 896, 448);
	prim->clut = GetClut(256, 502);
#ifdef DW1_NATIVE
	/* widescreen: the whole wider screen */
	setPosDataPolyFT4(prim, -160 - dw1_wide_extra, -120, 320 + 2 * dw1_wide_extra, 240);
#else
	setPosDataPolyFT4(prim, -160, -120, 320, 240);
#endif
	setUVDataPolyFT4(prim, 250, 509, 2, 2);
	
	setRGB0(prim, progress, progress, progress);

	if (FADE_MODE == 2) {
		AddPrim(&ACTIVE_ORDERING_TABLE->org[1], prim);
	} else {
		AddPrim(&ACTIVE_ORDERING_TABLE->org[10], prim);
	}

	prim++;

	GsSetWorkBase((PACKET*)(prim));
}

void fadeToWhite(int16_t frames)
{
#if VERSION_EQUAL_OR_NEWER(JP_TRIAL)
	if (FADE_OUT_CURRENT || FADE_IN_CURRENT) {
		removeObject(4005, 0);
	}
#endif

	FADE_DATA = frames + 1;
	FADE_OUT_CURRENT = 1;
	FADE_MODE = 1;

#if VERSION_IS(JP)
	if (!FADE_OUT_IN_PROGRESS) {
		addObject(4005, 0, 0, renderFadeOut);
	}

	FADE_OUT_IN_PROGRESS = 1;
#else
	addObject(4005, 0, 0, renderFadeOut);
#endif
}

void fadeFromWhite(int16_t frames)
{
	removeObject(4005, 0);
	FADE_OUT_CURRENT = 0;
	FADE_IN_TARGET = frames + 1;
	FADE_IN_CURRENT = 1;
	FADE_MODE = 1;
	FADE_OUT_IN_PROGRESS = 0;
	addObject(4005, 0, 0, renderFadeIn);
}
