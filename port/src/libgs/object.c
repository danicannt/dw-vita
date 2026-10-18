/* libgs: TMD objects (GsSortObject4/4J) and the TMD primitive drivers.
 *
 * PSY-Q ships one hand-written driver per primitive type, lighting mode and
 * subdivision mode (GsTMDfastTG3L, GsTMDdivTNF4, ...), and GsSortObject4J
 * picks them from the GsFCALL4 table. The game fills part of that table itself
 * (graphics.c), so the table and every driver name the game assigns exist
 * here, but all drivers share one generic implementation that decodes the
 * primitive from its TMD header.
 *
 * Not done (yet): polygon subdivision (the "div" drivers draw like the "fast"
 * ones), fog (GsLMODE_FOG), screen-size clipping of huge polygons.
 *
 * Callers set the GTE matrices first (GsSetLsMatrix / GsSetLightMatrix). */
#include <libgpu.h>
#include <libgte.h>
#include <libgs.h>

#include "libgs_internal.h"

/* TMD primitive header */
#define TMD_OLEN(h) ((h) & 0xff)
#define TMD_ILEN(h) (((h) >> 8) & 0xff)
#define TMD_FLAG(h) (((h) >> 16) & 0xff)
#define TMD_MODE(h) (((h) >> 24) & 0xff)

/* mode bits */
#define TMD_TGE 0x01  /* brightness calculation off (textured) */
#define TMD_ABE 0x02  /* semi-transparency */
#define TMD_TME 0x04  /* textured */
#define TMD_QUAD 0x08 /* 4 vertices */
#define TMD_IIP 0x10  /* gouraud */
/* flag bits */
#define TMD_LGT 0x01  /* no lighting: colours are in the primitive */
#define TMD_FCE 0x02  /* double sided */
#define TMD_GRD 0x04  /* gradation: one colour per vertex */

enum { LIGHT_ON, LIGHT_OFF };

typedef struct {
    int quad, tex, gour, unlit, grad, abe, tge, fce;
} PrimType;

typedef struct {
    u_short v[4], n[4];
    CVECTOR c[4];        /* input colours (unlit or gradation), else c[0] */
    u_char u[4], vv[4];
    u_short clut, tpage;
} PrimData;

static PrimType decode_type(u_long header)
{
    PrimType t;
    int mode = TMD_MODE(header), flag = TMD_FLAG(header);

    t.quad = (mode & TMD_QUAD) != 0;
    t.tex = (mode & TMD_TME) != 0;
    t.gour = (mode & TMD_IIP) != 0;
    t.abe = (mode & TMD_ABE) != 0;
    t.tge = (mode & TMD_TGE) != 0;
    t.unlit = (flag & TMD_LGT) != 0;
    t.grad = (flag & TMD_GRD) != 0;
    t.fce = (flag & TMD_FCE) != 0;
    return t;
}

static void set_cv(CVECTOR *c, u_char r, u_char g, u_char b)
{
    c->r = r;
    c->g = g;
    c->b = b;
    c->cd = 0;
}

/* Read one primitive (op points at its header word) into PrimData,
 * following the TMD_P_* layouts. */
static void decode_prim(const PrimType *t, u_long *op, PrimData *d)
{
    int nv = t->quad ? 4 : 3;
    int i;

    if (t->tex) {
        /* both textured layouts start with the same UV/clut/tpage block */
        TMD_P_TG4 *q = (TMD_P_TG4 *)op;
        d->u[0] = q->tu0; d->vv[0] = q->tv0;
        d->u[1] = q->tu1; d->vv[1] = q->tv1;
        d->u[2] = q->tu2; d->vv[2] = q->tv2;
        d->u[3] = t->quad ? q->tu3 : 0;
        d->vv[3] = t->quad ? q->tv3 : 0;
        d->clut = q->clut;
        d->tpage = q->tpage;
        for (i = 0; i < 4; i++)
            set_cv(&d->c[i], 128, 128, 128);
    }

    if (!t->unlit) {
        /* lit: normals in the primitive */
        if (t->tex && !t->quad) {
            if (t->gour) {
                TMD_P_TG3 *p = (TMD_P_TG3 *)op;
                d->n[0] = p->n0; d->v[0] = p->v0; d->n[1] = p->n1; d->v[1] = p->v1;
                d->n[2] = p->n2; d->v[2] = p->v2;
            } else {
                TMD_P_TF3 *p = (TMD_P_TF3 *)op;
                d->n[0] = d->n[1] = d->n[2] = p->n0;
                d->v[0] = p->v0; d->v[1] = p->v1; d->v[2] = p->v2;
            }
        } else if (t->tex) {
            if (t->gour) {
                TMD_P_TG4 *p = (TMD_P_TG4 *)op;
                d->n[0] = p->n0; d->v[0] = p->v0; d->n[1] = p->n1; d->v[1] = p->v1;
                d->n[2] = p->n2; d->v[2] = p->v2; d->n[3] = p->n3; d->v[3] = p->v3;
            } else {
                TMD_P_TF4 *p = (TMD_P_TF4 *)op;
                d->n[0] = d->n[1] = d->n[2] = d->n[3] = p->n0;
                d->v[0] = p->v0; d->v[1] = p->v1; d->v[2] = p->v2; d->v[3] = p->v3;
            }
        } else if (t->grad) {
            /* gradation: 3/4 colours, then normals and vertices */
            u_char *c = (u_char *)op + 4;
            u_short *ix = (u_short *)(c + 4 * nv);
            for (i = 0; i < nv; i++)
                set_cv(&d->c[i], c[i * 4], c[i * 4 + 1], c[i * 4 + 2]);
            if (t->gour) {
                for (i = 0; i < nv; i++) {
                    d->n[i] = ix[i * 2];
                    d->v[i] = ix[i * 2 + 1];
                }
            } else {
                for (i = 0; i < nv; i++) {
                    d->n[i] = ix[0];
                    d->v[i] = ix[i + 1];
                }
            }
        } else {
            u_char *c = (u_char *)op + 4;
            u_short *ix = (u_short *)(c + 4);
            for (i = 0; i < 4; i++)
                set_cv(&d->c[i], c[0], c[1], c[2]);
            if (t->gour) {
                for (i = 0; i < nv; i++) {
                    d->n[i] = ix[i * 2];
                    d->v[i] = ix[i * 2 + 1];
                }
            } else {
                for (i = 0; i < nv; i++) {
                    d->n[i] = ix[0];
                    d->v[i] = ix[i + 1];
                }
            }
        }
    } else {
        /* unlit: colours, then vertices; textured ones after the UV block */
        u_char *c = (u_char *)op + (t->tex ? 4 + (t->quad ? 16 : 12) : 4);
        int ncol = t->gour ? nv : 1;
        u_short *ix = (u_short *)(c + 4 * ncol);
        for (i = 0; i < nv; i++) {
            int k = t->gour ? i : 0;
            set_cv(&d->c[i], c[k * 4], c[k * 4 + 1], c[k * 4 + 2]);
            d->v[i] = ix[i];
            d->n[i] = 0;
        }
    }
}

static GsOT_TAG *ot_entry(GsOT *ot, long otz, int shift)
{
    long idx = otz >> shift;
    long last = (1L << ot->length) - 1;

    if (idx > last)
        idx = last;
    return &ot->org[idx];
}

/* Transform, cull, light and emit one primitive. Returns the next packet. */
static PACKET *draw_prim(u_long *op, SVECTOR *vp, SVECTOR *np, PACKET *pk, int shift,
                         GsOT *ot, int light, u_long attr)
{
    PrimType t = decode_type(op[0]);
    PrimData d;
    int sxy[4], p, flag;
    short sx[4] = {0}, sy[4] = {0};
    long otz, nclip;
    CVECTOR col[4];
    int i, nv = t.quad ? 4 : 3;
    int semi = t.abe || (attr & GsALON);
    int multi;

    decode_prim(&t, op, &d);

    if (t.quad)
        otz = RotTransPers4(&vp[d.v[0]], &vp[d.v[1]], &vp[d.v[2]], &vp[d.v[3]],
                            &sxy[0], &sxy[1], &sxy[2], &sxy[3], &p, &flag);
    else
        otz = RotTransPers3(&vp[d.v[0]], &vp[d.v[1]], &vp[d.v[2]],
                            &sxy[0], &sxy[1], &sxy[2], &p, &flag);
    if (otz <= 0 || otz < (gs_near_clip >> 2))
        return pk;
    nclip = NormalClip(sxy[0], sxy[1], sxy[2]);
    if (nclip <= 0 && !t.fce)
        return pk;

    /* colours */
    if (!t.unlit && light == LIGHT_ON) {
        int nn = (t.gour || t.grad) ? nv : 1;
        for (i = 0; i < nn; i++)
            NormalColorCol(&np[d.n[i]], &d.c[t.grad ? i : 0], &col[i]);
        for (; i < nv; i++)
            col[i] = col[0];
    } else {
        for (i = 0; i < nv; i++)
            col[i] = d.c[i];
    }
    multi = t.gour || t.grad;
    for (i = 0; i < nv; i++) {
        sx[i] = (short)(sxy[i] & 0xffff);
        sy[i] = (short)((unsigned)sxy[i] >> 16);
    }

    if (t.tex) {
        u_short tpage = d.tpage;
        if (attr & GsALON)
            tpage = (u_short)((tpage & ~0x60u) | (((attr >> 28) & 3) << 5));
        if (t.quad && multi) {
            POLY_GT4 *g = (POLY_GT4 *)pk;
            setPolyGT4(g);
            setRGB0(g, col[0].r, col[0].g, col[0].b); setRGB1(g, col[1].r, col[1].g, col[1].b);
            setRGB2(g, col[2].r, col[2].g, col[2].b); setRGB3(g, col[3].r, col[3].g, col[3].b);
            setXY4(g, sx[0], sy[0], sx[1], sy[1], sx[2], sy[2], sx[3], sy[3]);
            setUV4(g, d.u[0], d.vv[0], d.u[1], d.vv[1], d.u[2], d.vv[2], d.u[3], d.vv[3]);
            g->clut = d.clut; g->tpage = tpage;
            setSemiTrans(g, semi); setShadeTex(g, t.tge);
            addPrim(ot_entry(ot, otz, shift), g);
            return (PACKET *)(g + 1);
        } else if (t.quad) {
            POLY_FT4 *f = (POLY_FT4 *)pk;
            setPolyFT4(f);
            setRGB0(f, col[0].r, col[0].g, col[0].b);
            setXY4(f, sx[0], sy[0], sx[1], sy[1], sx[2], sy[2], sx[3], sy[3]);
            setUV4(f, d.u[0], d.vv[0], d.u[1], d.vv[1], d.u[2], d.vv[2], d.u[3], d.vv[3]);
            f->clut = d.clut; f->tpage = tpage;
            setSemiTrans(f, semi); setShadeTex(f, t.tge);
            addPrim(ot_entry(ot, otz, shift), f);
            return (PACKET *)(f + 1);
        } else if (multi) {
            POLY_GT3 *g = (POLY_GT3 *)pk;
            setPolyGT3(g);
            setRGB0(g, col[0].r, col[0].g, col[0].b); setRGB1(g, col[1].r, col[1].g, col[1].b);
            setRGB2(g, col[2].r, col[2].g, col[2].b);
            setXY3(g, sx[0], sy[0], sx[1], sy[1], sx[2], sy[2]);
            setUV3(g, d.u[0], d.vv[0], d.u[1], d.vv[1], d.u[2], d.vv[2]);
            g->clut = d.clut; g->tpage = tpage;
            setSemiTrans(g, semi); setShadeTex(g, t.tge);
            addPrim(ot_entry(ot, otz, shift), g);
            return (PACKET *)(g + 1);
        } else {
            POLY_FT3 *f = (POLY_FT3 *)pk;
            setPolyFT3(f);
            setRGB0(f, col[0].r, col[0].g, col[0].b);
            setXY3(f, sx[0], sy[0], sx[1], sy[1], sx[2], sy[2]);
            setUV3(f, d.u[0], d.vv[0], d.u[1], d.vv[1], d.u[2], d.vv[2]);
            f->clut = d.clut; f->tpage = tpage;
            setSemiTrans(f, semi); setShadeTex(f, t.tge);
            addPrim(ot_entry(ot, otz, shift), f);
            return (PACKET *)(f + 1);
        }
    }

    if (t.quad && multi) {
        POLY_G4 *g = (POLY_G4 *)pk;
        setPolyG4(g);
        setRGB0(g, col[0].r, col[0].g, col[0].b); setRGB1(g, col[1].r, col[1].g, col[1].b);
        setRGB2(g, col[2].r, col[2].g, col[2].b); setRGB3(g, col[3].r, col[3].g, col[3].b);
        setXY4(g, sx[0], sy[0], sx[1], sy[1], sx[2], sy[2], sx[3], sy[3]);
        setSemiTrans(g, semi);
        addPrim(ot_entry(ot, otz, shift), g);
        return (PACKET *)(g + 1);
    } else if (t.quad) {
        POLY_F4 *f = (POLY_F4 *)pk;
        setPolyF4(f);
        setRGB0(f, col[0].r, col[0].g, col[0].b);
        setXY4(f, sx[0], sy[0], sx[1], sy[1], sx[2], sy[2], sx[3], sy[3]);
        setSemiTrans(f, semi);
        addPrim(ot_entry(ot, otz, shift), f);
        return (PACKET *)(f + 1);
    } else if (multi) {
        POLY_G3 *g = (POLY_G3 *)pk;
        setPolyG3(g);
        setRGB0(g, col[0].r, col[0].g, col[0].b); setRGB1(g, col[1].r, col[1].g, col[1].b);
        setRGB2(g, col[2].r, col[2].g, col[2].b);
        setXY3(g, sx[0], sy[0], sx[1], sy[1], sx[2], sy[2]);
        setSemiTrans(g, semi);
        addPrim(ot_entry(ot, otz, shift), g);
        return (PACKET *)(g + 1);
    } else {
        POLY_F3 *f = (POLY_F3 *)pk;
        setPolyF3(f);
        setRGB0(f, col[0].r, col[0].g, col[0].b);
        setXY3(f, sx[0], sy[0], sx[1], sy[1], sx[2], sy[2]);
        setSemiTrans(f, semi);
        addPrim(ot_entry(ot, otz, shift), f);
        return (PACKET *)(f + 1);
    }
}

/* attribute of the object being sorted, for the drivers (PSY-Q passes it in
 * a register; the driver signature has no room for it) */
static u_long current_attr;

/* draw n consecutive primitives starting at op */
static PACKET *draw_prims(u_long *op, SVECTOR *vp, SVECTOR *np, PACKET *pk, int n,
                          int shift, GsOT *ot, int light)
{
    while (n-- > 0) {
        pk = draw_prim(op, vp, np, pk, shift, ot, light, current_attr);
        op += 1 + TMD_ILEN(op[0]);
    }
    return pk;
}

/* ---- the named drivers ---------------------------------------------------- */

#define DRIVER(name, light)                                                       \
    PACKET *name(u_long *op, SVECTOR *vp, SVECTOR *np, PACKET *pk, int n, int shift, \
                 GsOT *ot, u_long *scratch)                                       \
    {                                                                             \
        (void)scratch;                                                            \
        return draw_prims(op, vp, np, pk, n, shift, ot, light);                   \
    }

#define DRIVER_SET(kind)                                                          \
    DRIVER(GsTMDfast##kind##L, LIGHT_ON)                                          \
    DRIVER(GsTMDfast##kind##NL, LIGHT_OFF)                                        \
    DRIVER(GsTMDdiv##kind##L, LIGHT_ON)                                           \
    DRIVER(GsTMDdiv##kind##NL, LIGHT_OFF)

DRIVER_SET(F3) DRIVER_SET(G3) DRIVER_SET(TF3) DRIVER_SET(TG3)
DRIVER_SET(F4) DRIVER_SET(G4) DRIVER_SET(TF4) DRIVER_SET(TG4)

/* unlit primitive types (colours in the data) */
DRIVER(GsTMDfastNF3, LIGHT_OFF) DRIVER(GsTMDdivNF3, LIGHT_OFF)
DRIVER(GsTMDfastNG3, LIGHT_OFF) DRIVER(GsTMDdivNG3, LIGHT_OFF)
DRIVER(GsTMDfastTNF3, LIGHT_OFF) DRIVER(GsTMDdivTNF3, LIGHT_OFF)
DRIVER(GsTMDfastTNG3, LIGHT_OFF) DRIVER(GsTMDdivTNG3, LIGHT_OFF)
DRIVER(GsTMDfastNF4, LIGHT_OFF) DRIVER(GsTMDdivNF4, LIGHT_OFF)
DRIVER(GsTMDfastNG4, LIGHT_OFF) DRIVER(GsTMDdivNG4, LIGHT_OFF)
DRIVER(GsTMDfastTNF4, LIGHT_OFF) DRIVER(GsTMDdivTNF4, LIGHT_OFF)
DRIVER(GsTMDfastTNG4, LIGHT_OFF) DRIVER(GsTMDdivTNG4, LIGHT_OFF)

/* ---- driver table ----------------------------------------------------------- */

#define LIT3(kind)                                                                  \
    {{GsTMDfast##kind##L, GsTMDfast##kind##L, GsTMDfast##kind##NL},                \
     {GsTMDdiv##kind##L, GsTMDdiv##kind##L, GsTMDdiv##kind##NL}}

_GsFCALL GsFCALL4 = {
    LIT3(F3), {GsTMDfastNF3, GsTMDdivNF3}, LIT3(G3), {GsTMDfastNG3, GsTMDdivNG3},
    LIT3(TF3), {GsTMDfastTNF3, GsTMDdivTNF3}, LIT3(TG3), {GsTMDfastTNG3, GsTMDdivTNG3},
    LIT3(F4), {GsTMDfastNF4, GsTMDdivNF4}, LIT3(G4), {GsTMDfastNG4, GsTMDdivNG4},
    LIT3(TF4), {GsTMDfastTNF4, GsTMDdivTNF4}, LIT3(TG4), {GsTMDfastTNG4, GsTMDdivTNG4},
    {GsTMDfastF3L, GsTMDfastF3L, GsTMDfastF3NL},
    {GsTMDfastG3L, GsTMDfastG3L, GsTMDfastG3NL},
    {GsTMDfastF4L, GsTMDfastF4L, GsTMDfastF4NL},
    {GsTMDfastG4L, GsTMDfastG4L, GsTMDfastG4NL},
};

static GsTMD_DRIVER pick_driver(const PrimType *t, int div, int lmode)
{
    const _GsFCALL *f = &GsFCALL4;
    GsTMD_DRIVER d;

    if (t->unlit) {
        if (t->tex)
            d = t->quad ? (t->gour ? f->ntg4[div] : f->ntf4[div])
                        : (t->gour ? f->ntg3[div] : f->ntf3[div]);
        else
            d = t->quad ? (t->gour ? f->ng4[div] : f->nf4[div])
                        : (t->gour ? f->ng3[div] : f->nf3[div]);
    } else if (t->tex) {
        d = t->quad ? (t->gour ? f->tg4[div][lmode] : f->tf4[div][lmode])
                    : (t->gour ? f->tg3[div][lmode] : f->tf3[div][lmode]);
    } else if (t->grad) {
        d = t->quad ? (t->gour ? f->g4g[lmode] : f->f4g[lmode])
                    : (t->gour ? f->g3g[lmode] : f->f3g[lmode]);
    } else {
        d = t->quad ? (t->gour ? f->g4[div][lmode] : f->f4[div][lmode])
                    : (t->gour ? f->g3[div][lmode] : f->f3[div][lmode]);
    }
    return d;
}

void GsSortObject4J(GsDOBJ2 *objp, GsOT *otp, int shift, u_long *scratch)
{
    struct TMD_STRUCT *obj = (struct TMD_STRUCT *)objp->tmd;
    u_long *op;
    u_long i;
    int div, lmode;

    if (obj == NULL || (objp->attribute & GsDOFF))
        return;

    div = ((objp->attribute >> 9) & 7) ? GsDivMODE_DIV : GsDivMODE_NDIV;
    if (objp->attribute & GsLOFF)
        lmode = GsLMODE_LOFF;
    else
        lmode = (gs_light_mode == GsLMODE_LOFF) ? GsLMODE_LOFF : GsLMODE_NORMAL;

    current_attr = objp->attribute;
    op = obj->primtop;
    for (i = 0; i < obj->primn; i++) {
        PrimType t = decode_type(op[0]);
        GsTMD_DRIVER d = pick_driver(&t, div, lmode);

        if (d == NULL) /* not set by the game: use the generic driver */
            GsOUT_PACKET_P = draw_prims(op, (SVECTOR *)obj->vertop, (SVECTOR *)obj->nortop,
                                        GsOUT_PACKET_P, 1, shift, otp,
                                        lmode == GsLMODE_LOFF ? LIGHT_OFF : LIGHT_ON);
        else
            GsOUT_PACKET_P = d(op, (SVECTOR *)obj->vertop, (SVECTOR *)obj->nortop,
                               GsOUT_PACKET_P, 1, shift, otp, scratch);
        op += 1 + TMD_ILEN(op[0]);
    }
}

void GsSortObject4(GsDOBJ2 *objp, GsOT *otp, int shift, u_long *scratch)
{
    GsSortObject4J(objp, otp, shift, scratch);
}
