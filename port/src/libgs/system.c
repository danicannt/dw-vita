/* libgs: display buffers, drawing environment, ordering tables, 2D primitives.
 *
 * The double-buffer bookkeeping follows PSY-Z's partial libgs.c (MIT,
 * external/psyz/psyz/src/psyz/libgs.c); this file replaces it completely so
 * that file is never linked (every public symbol it defines is defined here).
 *
 * Ordering tables are linked in reverse, as in PSY-Q: entry 0 is drawn last
 * (front), the last entry first (back), and GsOT.tag points at the last
 * entry, where drawing starts. */
#include <string.h>

#include <libetc.h>
#include <libgpu.h>
#include <libgte.h>
#include <libgs.h>

#include "libgs_internal.h"

static BLK_FILL bg_clear[2];
static int HWD0, VWD0;          /* display size */
static short PSDIDX;            /* current drawing buffer */
static short PSDGPU;            /* offsets applied by the GPU (GsOFSGPU) */
static short PSDCNT;
static short PSDOFSX[2], PSDOFSY[2];
static short PSDBASEX[2], PSDBASEY[2];

DRAWENV GsDRAWENV;
DISPENV GsDISPENV;
PACKET *GsOUT_PACKET_P;
short GsORGOFSX, GsORGOFSY;     /* screen origin set by GsSetOrign */

void gpu_init(u_short x, u_short y, u_short intmode, u_short dith, u_short varmmode)
{
    ResetGraph(((intmode >> 4) & 3) == 3 ? 3 : 0);
    memset(&GsDRAWENV, 0, sizeof(GsDRAWENV));
    memset(&GsDISPENV, 0, sizeof(GsDISPENV));
    GsDRAWENV.dtd = dith;
    GsDRAWENV.dfe = 1;
    GsDRAWENV.isbg = 0;
    GsDISPENV.disp.w = (short)x;
    GsDISPENV.disp.h = (short)y;
    if (GetVideoMode() == MODE_PAL) {
        GsDISPENV.screen.y = 24;
        GsDISPENV.pad0 = 1;
    }
    GsDISPENV.isrgb24 = varmmode;
    GsDISPENV.isinter = intmode & 1;
    PSDGPU = (short)(intmode & GsOFSGPU);
    PutDrawEnv(&GsDRAWENV);
    PutDispEnv(&GsDISPENV);
}

void gte_init(void)
{
    InitGeom();
    SetFarColor(0, 0, 0);
    SetGeomOffset(0, 0);
    GsORGOFSX = 0;
    GsORGOFSY = 0;
}

void GsInitGraph(u_short x, u_short y, u_short intmode, u_short dith, u_short varmmode)
{
    gpu_init(x, y, intmode, dith, varmmode);
    gte_init();
    HWD0 = x;
    VWD0 = y;
    PSDIDX = 0;
    PSDCNT = 1;
    memset(PSDOFSX, 0, sizeof(PSDOFSX));
    memset(PSDOFSY, 0, sizeof(PSDOFSY));
    memset(PSDBASEX, 0, sizeof(PSDBASEX));
    memset(PSDBASEY, 0, sizeof(PSDBASEY));
    GsSetDrawBuffClip();
    GsSetDrawBuffOffset();
}

void GsInit3D(void)
{
    gs_init_3d();
}

void GsDefDispBuff(u_short x0, u_short y0, u_short x1, u_short y1)
{
    PSDOFSX[0] = (short)x0;
    PSDOFSY[0] = (short)y0;
    PSDOFSX[1] = (short)x1;
    PSDOFSY[1] = (short)y1;
    if (PSDGPU) {
        /* the GPU adds the buffer origin: primitives stay in screen space */
        PSDBASEX[0] = PSDBASEX[1] = 0;
        PSDBASEY[0] = PSDBASEY[1] = 0;
    } else {
        PSDBASEX[0] = (short)x0;
        PSDBASEY[0] = (short)y0;
        PSDBASEX[1] = (short)x1;
        PSDBASEY[1] = (short)y1;
    }
    GsSetDrawBuffClip();
    GsSetDrawBuffOffset();
}

int GsGetActiveBuff(void)
{
    return PSDIDX;
}

/* clip the drawing area to the current buffer */
void GsSetDrawBuffClip(void)
{
    GsDRAWENV.clip.x = PSDOFSX[PSDIDX];
    GsDRAWENV.clip.y = PSDOFSY[PSDIDX];
    GsDRAWENV.clip.w = (short)HWD0;
    GsDRAWENV.clip.h = (short)VWD0;
    PutDrawEnv(&GsDRAWENV);
}

/* drawing offset: buffer origin (GPU mode) plus the screen origin */
void GsSetDrawBuffOffset(void)
{
    if (PSDGPU) {
        GsDRAWENV.ofs[0] = PSDOFSX[PSDIDX] + GsORGOFSX;
        GsDRAWENV.ofs[1] = PSDOFSY[PSDIDX] + GsORGOFSY;
        SetGeomOffset(0, 0);
    } else {
        GsDRAWENV.ofs[0] = 0;
        GsDRAWENV.ofs[1] = 0;
        SetGeomOffset(PSDBASEX[PSDIDX] + GsORGOFSX, PSDBASEY[PSDIDX] + GsORGOFSY);
    }
    PutDrawEnv(&GsDRAWENV);
}

void GsSetOrign(long x, long y)
{
    GsORGOFSX = (short)x;
    GsORGOFSY = (short)y;
    GsSetDrawBuffOffset();
}

void GsSwapDispBuff(void)
{
    /* show the buffer drawn during the last frame, draw into the other one */
    GsDISPENV.disp.x = PSDOFSX[PSDIDX];
    GsDISPENV.disp.y = PSDOFSY[PSDIDX];
    PutDispEnv(&GsDISPENV);
    SetDispMask(1);
    if (++PSDCNT == 0)
        PSDCNT = 1;
    PSDIDX ^= 1;
    GsSetDrawBuffClip();
    GsSetDrawBuffOffset();
}

void GsSetWorkBase(PACKET *base)
{
    GsOUT_PACKET_P = base;
}

PACKET *GsGetWorkBase(void)
{
    return GsOUT_PACKET_P;
}

static long gs_vcount_base;

void GsClearVcount(void)
{
    gs_vcount_base = VSync(-1);
}

long GsGetVcount(void)
{
    return VSync(-1) - gs_vcount_base;
}

/* ---- ordering tables ------------------------------------------------------ */

void GsClearOt(u_short offset, u_short point, GsOT *otp)
{
    int n = 1 << otp->length;

    otp->offset = offset;
    otp->point = point;
    otp->tag = &otp->org[n - 1];
    ClearOTagR((OT_TYPE *)otp->org, n);
}

void GsDrawOt(GsOT *otp)
{
    DrawOTag((OT_TYPE *)otp->tag);
}

/* link all of src into dst at the entry src->point */
GsOT *GsSortOt(GsOT *src, GsOT *dst)
{
    u_long point = src->point;
    u_long last = (1u << dst->length) - 1;
    GsOT_TAG *slot;

    if (point > last)
        point = last;
    slot = &dst->org[point];
    /* src is drawn from src->tag down to src->org[0]; splice it in front of
     * whatever slot pointed to */
    setaddr(&src->org[0], getaddr(slot));
    setaddr(slot, src->tag);
    return dst;
}

void GsSortClear(u_char r, u_char g, u_char b, GsOT *otp)
{
    BLK_FILL *p = &bg_clear[PSDIDX];

    setlen(p, 3);
    setcode(p, 0x02); /* fill rectangle: absolute VRAM coordinates */
    setRGB0(p, r, g, b);
    p->x0 = PSDOFSX[PSDIDX];
    p->y0 = PSDOFSY[PSDIDX];
    p->w = (short)(GsDISPENV.isrgb24 ? (3 * HWD0) >> 1 : HWD0);
    p->h = (short)VWD0;
    /* the deepest entry is drawn first */
    addPrim(otp->tag, p);
}

/* ---- 2D ------------------------------------------------------------------- */

static GsOT_TAG *ot_slot(GsOT *otp, u_short pri)
{
    u_long last = (1u << otp->length) - 1;
    return &otp->org[pri > last ? last : pri];
}

static u_short sprite_tpage(u_long attr, u_short tpage)
{
    /* attribute bits 24-25: colour mode, 28-29: blend rate */
    int tp = (attr >> 24) & 3;
    int abr = (attr >> 28) & 3;
    return (u_short)((tpage & ~0x1e0u) | (tp << 7) | (abr << 5));
}

void GsSortSprite(GsSPRITE *sp, GsOT *otp, u_short pri)
{
    POLY_FT4 *p;
    long cs, sn, sx, sy;
    int i;
    long px[4], py[4];

    if (sp->attribute & GsDOFF)
        return;
    if (sp->rotate == 0 && sp->scalex == 4096 && sp->scaley == 4096) {
        GsSortFastSprite(sp, otp, pri);
        return;
    }

    p = (POLY_FT4 *)GsOUT_PACKET_P;
    setPolyFT4(p);
    setRGB0(p, sp->r, sp->g, sp->b);
    if (sp->attribute & GsALON)
        setSemiTrans(p, 1);
    if (sp->attribute & (1u << 6)) /* brightness adjustment off */
        setShadeTex(p, 1);

    /* corners relative to (mx, my), scaled then rotated (rotate: 4096 = 1 deg) */
    cs = rcos(sp->rotate / 360);
    sn = rsin(sp->rotate / 360);
    for (i = 0; i < 4; i++) {
        long lx = ((i & 1) ? sp->w : 0) - sp->mx;
        long ly = ((i & 2) ? sp->h : 0) - sp->my;
        lx = (lx * sp->scalex) >> 12;
        ly = (ly * sp->scaley) >> 12;
        sx = (lx * cs - ly * sn) >> 12;
        sy = (lx * sn + ly * cs) >> 12;
        px[i] = sp->x + sx;
        py[i] = sp->y + sy;
    }
    setXY4(p, px[0], py[0], px[1], py[1], px[2], py[2], px[3], py[3]);
    setUVWH(p, sp->u, sp->v, sp->w, sp->h);
    p->tpage = sprite_tpage(sp->attribute, sp->tpage);
    p->clut = getClut(sp->cx, sp->cy);
    addPrim(ot_slot(otp, pri), p);
    GsOUT_PACKET_P = (PACKET *)(p + 1);
}

void GsSortFastSprite(GsSPRITE *sp, GsOT *otp, u_short pri)
{
    SPRT *p;
    DR_TPAGE *tp;

    if (sp->attribute & GsDOFF)
        return;
    p = (SPRT *)GsOUT_PACKET_P;
    setSprt(p);
    setRGB0(p, sp->r, sp->g, sp->b);
    if (sp->attribute & GsALON)
        setSemiTrans(p, 1);
    setXY0(p, sp->x, sp->y);
    setWH(p, sp->w, sp->h);
    setUV0(p, sp->u, sp->v);
    p->clut = getClut(sp->cx, sp->cy);
    addPrim(ot_slot(otp, pri), p);

    /* SPRT has no texture page: put one in front of it */
    tp = (DR_TPAGE *)(p + 1);
    setDrawTPage(tp, 0, 1, sprite_tpage(sp->attribute, sp->tpage));
    addPrim(ot_slot(otp, pri), tp);
    GsOUT_PACKET_P = (PACKET *)(tp + 1);
}

void GsSortBoxFill(GsBOXF *bp, GsOT *otp, u_short pri)
{
    TILE *p;

    if (bp->attribute & GsDOFF)
        return;
    p = (TILE *)GsOUT_PACKET_P;
    setTile(p);
    setRGB0(p, bp->r, bp->g, bp->b);
    if (bp->attribute & GsALON)
        setSemiTrans(p, 1);
    setXY0(p, bp->x, bp->y);
    setWH(p, bp->w, bp->h);
    addPrim(ot_slot(otp, pri), p);
    GsOUT_PACKET_P = (PACKET *)(p + 1);
}

void GsSortLine(GsLINE *lp, GsOT *otp, u_short pri)
{
    LINE_F2 *p;

    if (lp->attribute & GsDOFF)
        return;
    p = (LINE_F2 *)GsOUT_PACKET_P;
    setLineF2(p);
    setRGB0(p, lp->r, lp->g, lp->b);
    if (lp->attribute & GsALON)
        setSemiTrans(p, 1);
    setXY2(p, lp->x0, lp->y0, lp->x1, lp->y1);
    addPrim(ot_slot(otp, pri), p);
    GsOUT_PACKET_P = (PACKET *)(p + 1);
}

void GsSortGLine(GsGLINE *lp, GsOT *otp, u_short pri)
{
    LINE_G2 *p;

    if (lp->attribute & GsDOFF)
        return;
    p = (LINE_G2 *)GsOUT_PACKET_P;
    setLineG2(p);
    setRGB0(p, lp->r0, lp->g0, lp->b0);
    setRGB1(p, lp->r1, lp->g1, lp->b1);
    if (lp->attribute & GsALON)
        setSemiTrans(p, 1);
    setXY2(p, lp->x0, lp->y0, lp->x1, lp->y1);
    addPrim(ot_slot(otp, pri), p);
    GsOUT_PACKET_P = (PACKET *)(p + 1);
}

/* register an already built primitive */
void GsSortPoly(void *pp, GsOT *otp, u_short pri)
{
    addPrim(ot_slot(otp, pri), pp);
}

/* ---- TIM ------------------------------------------------------------------- */

/* tim points just after the TIM ID word (at the flags word) */
void GsGetTimInfo(u_long *tim, GsIMAGE *im)
{
    u_long flags = *tim++;

    im->pmode = flags;
    if (flags & 8) {
        u_long bnum = tim[0];
        im->cx = (short)(tim[1] & 0xffff);
        im->cy = (short)(tim[1] >> 16);
        im->cw = (u_short)(tim[2] & 0xffff);
        im->ch = (u_short)(tim[2] >> 16);
        im->clut = tim + 3;
        tim += bnum / 4;
    } else {
        im->cx = im->cy = 0;
        im->cw = im->ch = 0;
        im->clut = NULL;
    }
    im->px = (short)(tim[1] & 0xffff);
    im->py = (short)(tim[1] >> 16);
    im->pw = (u_short)(tim[2] & 0xffff);
    im->ph = (u_short)(tim[2] >> 16);
    im->pixel = tim + 3;
}
