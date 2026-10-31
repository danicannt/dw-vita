/* libgs: TMD objects (GsSortObject4/4J) and the TMD primitive drivers.
 *
 * PSY-Q ships one hand-written driver per primitive type, lighting mode and
 * subdivision mode (GsTMDfastTG3L, GsTMDdivTNF4, ...), and GsSortObject4J
 * picks them from the GsFCALL4 table. The game fills part of that table itself
 * (graphics.c), so the table and every driver name the game assigns exist
 * here, but all drivers share one generic implementation that decodes the
 * primitive from its TMD header.
 *
 * Polygons the GTE or the GPU cannot take as they are (crossing the near
 * plane, past the projection limits, too big for the GPU) are clipped and
 * subdivided whatever the driver; the "div" drivers do nothing else. Not done
 * (yet): fog (GsLMODE_FOG).
 *
 * Callers set the GTE matrices first (GsSetLsMatrix / GsSetLightMatrix). */
#include <stdlib.h>

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

/* lit colours for the vertices of a primitive, or its own */
static void prim_colours(const PrimType *t, PrimData *d, SVECTOR *np, int light, CVECTOR *col)
{
    int i, nv = t->quad ? 4 : 3;

    if (!t->unlit && light == LIGHT_ON) {
        int nn = (t->gour || t->grad) ? nv : 1;
        for (i = 0; i < nn; i++)
            NormalColorCol(&np[d->n[i]], &d->c[t->grad ? i : 0], &col[i]);
        for (; i < nv; i++)
            col[i] = col[0];
    } else {
        for (i = 0; i < nv; i++)
            col[i] = d->c[i];
    }
}

/* Emit one projected primitive. Returns the next packet. */
static PACKET *emit_prim(const PrimType *tp, const PrimData *dp, const CVECTOR *col, const int *sxy,
                         long otz, PACKET *pk, int shift, GsOT *ot, u_long attr)
{
    PrimType t = *tp;
    PrimData d = *dp;
    short sx[4] = {0}, sy[4] = {0};
    int i, nv = t.quad ? 4 : 3;
    int semi = t.abe || (attr & GsALON);
    int multi = t.gour || t.grad;

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

/* ---- clipping --------------------------------------------------------------- */

/* Polygons the GTE or the GPU cannot take as they are: cut at the near plane in
 * view space, then bisected until each visible piece is small enough for the
 * GPU. Pieces off screen stop there, so a big floor costs a few dozen. */

#define CLIP_PIECES 300     /* per frame: GS_WORK_BASES has to hold them */
#define CLIP_DEPTH 8
#define CLIP_MAX_EDGE 400   /* screen pixels */
#define CLIP_SCREEN_X 460   /* past any widescreen edge */
#define CLIP_SCREEN_Y 140

int gs_split_pieces; /* cleared by GsSwapDispBuff */

typedef struct {
    long x, y, z;  /* view space */
    long ox, oy, oz; /* object space */
    int u, v, r, g, b;
} ClipVert;

static void clip_lerp(const ClipVert *a, const ClipVert *b, int t, ClipVert *m)
{
#define CLIP_MIX(f) m->f = a->f + (long)(((long long)(b->f - a->f) * t) >> 12)
    CLIP_MIX(x); CLIP_MIX(y); CLIP_MIX(z);
    CLIP_MIX(ox); CLIP_MIX(oy); CLIP_MIX(oz);
    CLIP_MIX(u); CLIP_MIX(v); CLIP_MIX(r); CLIP_MIX(g); CLIP_MIX(b);
#undef CLIP_MIX
}

static void clip_project(const ClipVert *v, long *sx, long *sy)
{
    *sx = (long)((long long)v->x * gs_projection / v->z);
    *sy = (long)((long long)v->y * gs_projection / v->z);
}

/* draw one triangle in front of the near plane, or its two halves */
static PACKET *clip_tri(const PrimType *t, PrimData *d, const ClipVert *a, const ClipVert *b,
                        const ClipVert *c, int depth, PACKET *pk, int shift, GsOT *ot, u_long attr)
{
    const ClipVert *v[3];
    long sx[3], sy[3], e[3];
    int i, longest;

    v[0] = a; v[1] = b; v[2] = c;
    for (i = 0; i < 3; i++)
        clip_project(v[i], &sx[i], &sy[i]);
    if ((sx[0] < -CLIP_SCREEN_X && sx[1] < -CLIP_SCREEN_X && sx[2] < -CLIP_SCREEN_X) ||
        (sx[0] > CLIP_SCREEN_X && sx[1] > CLIP_SCREEN_X && sx[2] > CLIP_SCREEN_X) ||
        (sy[0] < -CLIP_SCREEN_Y && sy[1] < -CLIP_SCREEN_Y && sy[2] < -CLIP_SCREEN_Y) ||
        (sy[0] > CLIP_SCREEN_Y && sy[1] > CLIP_SCREEN_Y && sy[2] > CLIP_SCREEN_Y))
        return pk;

    longest = 0;
    for (i = 0; i < 3; i++) {
        int j = (i + 1) % 3;
        long dx = labs(sx[j] - sx[i]), dy = labs(sy[j] - sy[i]);
        e[i] = dx > dy ? dx : dy;
        if (e[i] > e[longest])
            longest = i;
    }

    if (e[longest] > CLIP_MAX_EDGE && depth < CLIP_DEPTH && gs_split_pieces < CLIP_PIECES) {
        ClipVert m;
        const ClipVert *p = v[longest], *q = v[(longest + 1) % 3], *r = v[(longest + 2) % 3];

        clip_lerp(p, q, 0x800, &m);
        pk = clip_tri(t, d, p, &m, r, depth + 1, pk, shift, ot, attr);
        return clip_tri(t, d, &m, q, r, depth + 1, pk, shift, ot, attr);
    }

    if (e[longest] >= 1024)
        return pk;
    {
        PrimType tri = *t;
        CVECTOR col[4];
        int sxy[4], p, flag;
        long otz;
        SVECTOR o[3];

        tri.quad = 0;
        for (i = 0; i < 3; i++) {
            o[i].vx = (short)v[i]->ox; o[i].vy = (short)v[i]->oy;
            o[i].vz = (short)v[i]->oz; o[i].pad = 0;
            d->u[i] = (u_char)v[i]->u;
            d->vv[i] = (u_char)v[i]->v;
            col[i].r = (u_char)v[i]->r; col[i].g = (u_char)v[i]->g;
            col[i].b = (u_char)v[i]->b; col[i].cd = 0;
        }
        /* through the GTE when it can, so the piece has PGXP positions */
        otz = RotTransPers3(&o[0], &o[1], &o[2], &sxy[0], &sxy[1], &sxy[2], &p, &flag);
        if (flag < 0 || otz <= 0) {
            for (i = 0; i < 3; i++)
                sxy[i] = (int)(((unsigned)(short)sy[i] << 16) | (u_short)(short)sx[i]);
            otz = (v[0]->z + v[1]->z + v[2]->z) / 12;
        }
        gs_split_pieces++;
        return emit_prim(&tri, d, col, sxy, otz, pk, shift, ot, attr);
    }
}

/* cut a polygon (vertices in order around it) at the near plane and draw it */
static PACKET *clip_prim(const PrimType *t, PrimData *d, const ClipVert *in, int n,
                         PACKET *pk, int shift, GsOT *ot, u_long attr)
{
    ClipVert out[5];
    long near_z = gs_near_clip > 4 ? gs_near_clip : 4;
    int i, m = 0;

    for (i = 0; i < n; i++) {
        const ClipVert *a = &in[i], *b = &in[(i + 1) % n];

        if (a->z >= near_z)
            out[m++] = *a;
        if ((a->z >= near_z) != (b->z >= near_z))
            clip_lerp(a, b, (int)(((long long)(near_z - a->z) << 12) / (b->z - a->z)), &out[m++]);
    }
    for (i = 0; i < m; i++)
        if (out[i].z < near_z)
            out[i].z = near_z; /* rounding of clip_lerp */
    for (i = 1; i + 1 < m; i++)
        pk = clip_tri(t, d, &out[0], &out[i], &out[i + 1], 0, pk, shift, ot, attr);
    return pk;
}

/* a primitive that needs clipping, from its object space vertices */
static PACKET *clip_start(const PrimType *t, PrimData *d, SVECTOR *vp, const CVECTOR *col,
                          PACKET *pk, int shift, GsOT *ot, u_long attr)
{
    static const int quad_order[4] = {0, 1, 3, 2}; /* TMD quads are a Z */
    ClipVert v[4];
    MATRIX m;
    long long det;
    int i, nv = t->quad ? 4 : 3;

    ReadRotMatrix(&m);
    for (i = 0; i < nv; i++) {
        int k = t->quad ? quad_order[i] : i;
        const SVECTOR *o = &vp[d->v[k]];

        v[i].ox = o->vx; v[i].oy = o->vy; v[i].oz = o->vz;
        v[i].x = ((m.m[0][0] * o->vx + m.m[0][1] * o->vy + m.m[0][2] * o->vz) >> 12) + m.t[0];
        v[i].y = ((m.m[1][0] * o->vx + m.m[1][1] * o->vy + m.m[1][2] * o->vz) >> 12) + m.t[1];
        v[i].z = ((m.m[2][0] * o->vx + m.m[2][1] * o->vy + m.m[2][2] * o->vz) >> 12) + m.t[2];
        v[i].u = d->u[k]; v[i].v = d->vv[k];
        v[i].r = col[k].r; v[i].g = col[k].g; v[i].b = col[k].b;
    }

    /* the side facing the camera: NormalClip without the projection */
    det = (long long)v[0].x * ((long long)v[1].y * v[2].z - (long long)v[1].z * v[2].y)
        - (long long)v[0].y * ((long long)v[1].x * v[2].z - (long long)v[1].z * v[2].x)
        + (long long)v[0].z * ((long long)v[1].x * v[2].y - (long long)v[1].y * v[2].x);
    if (det <= 0 && !t->fce)
        return pk;
    return clip_prim(t, d, v, nv, pk, shift, ot, attr);
}

static int prim_too_big(const int *sxy, int nv)
{
    int i, x0 = 32767, x1 = -32768, y0 = 32767, y1 = -32768;

    for (i = 0; i < nv; i++) {
        int x = (short)(sxy[i] & 0xffff), y = (short)((unsigned)sxy[i] >> 16);
        x0 = x < x0 ? x : x0;
        x1 = x > x1 ? x : x1;
        y0 = y < y0 ? y : y0;
        y1 = y > y1 ? y : y1;
    }
    return x1 - x0 >= 1024 || y1 - y0 >= 512;
}

/* Transform, cull, light and emit one primitive. Returns the next packet. */
static PACKET *draw_prim(u_long *op, SVECTOR *vp, SVECTOR *np, PACKET *pk, int shift,
                         GsOT *ot, int light, u_long attr)
{
    PrimType t = decode_type(op[0]);
    PrimData d;
    int sxy[4], p, flag;
    long otz;
    CVECTOR col[4];
    int nv = t.quad ? 4 : 3;

    decode_prim(&t, op, &d);

    if (t.quad)
        otz = RotTransPers4(&vp[d.v[0]], &vp[d.v[1]], &vp[d.v[2]], &vp[d.v[3]],
                            &sxy[0], &sxy[1], &sxy[2], &sxy[3], &p, &flag);
    else
        otz = RotTransPers3(&vp[d.v[0]], &vp[d.v[1]], &vp[d.v[2]],
                            &sxy[0], &sxy[1], &sxy[2], &p, &flag);

    if (flag < 0 || otz <= 0 || otz < (gs_near_clip >> 2) || prim_too_big(sxy, nv)) {
        prim_colours(&t, &d, np, light, col);
        return clip_start(&t, &d, vp, col, pk, shift, ot, attr);
    }

    if (NormalClip(sxy[0], sxy[1], sxy[2]) <= 0 && !t.fce)
        return pk;
    prim_colours(&t, &d, np, light, col);
    return emit_prim(&t, &d, col, sxy, otz, pk, shift, ot, attr);
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
