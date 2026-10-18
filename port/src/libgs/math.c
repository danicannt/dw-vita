/* libgs: coordinate systems, view, lights and TMD model setup.
 *
 * Matrices are the GTE's 4.12 fixed point. Products use 64-bit intermediates
 * and truncate like the GTE; the view and light setup use doubles internally
 * and round to 4.12 (TODO: compare GsSetRefView2/GsSetFlatLight output with a
 * PS1 capture; small differences only move pixels, they do not affect logic). */
#include <math.h>
#include <string.h>

#include <libgpu.h>
#include <libgte.h>
#include <libgs.h>

#include "libgs_internal.h"

MATRIX GsWSMATRIX;
MATRIX GsIDMATRIX = {{{4096, 0, 0}, {0, 4096, 0}, {0, 0, 4096}}, {0, 0, 0}};
MATRIX GsIDMATRIX2 = {{{4096, 0, 0}, {0, 4096, 0}, {0, 0, 4096}}, {0, 0, 0}};
MATRIX GsLIGHTWSMATRIX;
static MATRIX gs_light_color;
GsCOORDINATE2 WORLD = {0, {{{4096, 0, 0}, {0, 4096, 0}, {0, 0, 4096}}, {0, 0, 0}},
                       {{{4096, 0, 0}, {0, 4096, 0}, {0, 0, 4096}}, {0, 0, 0}},
                       NULL, NULL, NULL};

long gs_projection = 1000;
long gs_near_clip = 0;
int gs_light_mode = GsLMODE_NORMAL;

static short clamp16(long long v)
{
    return (short)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
}

/* out = a * b (rotation and translation), out may alias a or b */
static void compose(const MATRIX *a, const MATRIX *b, MATRIX *out)
{
    MATRIX r;
    int i, j;

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++)
            r.m[i][j] = clamp16(((long long)a->m[i][0] * b->m[0][j] +
                                 (long long)a->m[i][1] * b->m[1][j] +
                                 (long long)a->m[i][2] * b->m[2][j]) >> 12);
        r.t[i] = (int)((((long long)a->m[i][0] * b->t[0] + (long long)a->m[i][1] * b->t[1] +
                         (long long)a->m[i][2] * b->t[2]) >> 12) + a->t[i]);
    }
    *out = r;
}

/* inverse of a rigid transform (orthonormal rotation) */
static void invert_rigid(const MATRIX *a, MATRIX *out)
{
    MATRIX r;
    int i, j;

    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            r.m[i][j] = a->m[j][i];
    for (i = 0; i < 3; i++)
        r.t[i] = (int)(-(((long long)r.m[i][0] * a->t[0] + (long long)r.m[i][1] * a->t[1] +
                          (long long)r.m[i][2] * a->t[2]) >> 12));
    *out = r;
}

/* ---- coordinate systems --------------------------------------------------- */

void GsInitCoordinate2(GsCOORDINATE2 *super, GsCOORDINATE2 *base)
{
    base->flg = 0;
    base->coord = GsIDMATRIX;
    base->workm = GsIDMATRIX;
    base->param = NULL;
    base->super = super;
    base->sub = NULL;
}

/* local-to-world matrix of a coordinate system */
void GsGetLw(GsCOORDINATE2 *coord, MATRIX *m)
{
    MATRIX parent;

    if (coord == NULL || coord == &WORLD) {
        *m = GsIDMATRIX;
        return;
    }
    if (coord->super != NULL) {
        GsGetLw(coord->super, &parent);
        compose(&parent, &coord->coord, &coord->workm);
    } else {
        coord->workm = coord->coord;
    }
    coord->flg = 1;
    *m = coord->workm;
}

/* local-to-screen matrix */
void GsGetLs(GsCOORDINATE2 *coord, MATRIX *m)
{
    MATRIX lw;

    GsGetLw(coord, &lw);
    compose(&GsWSMATRIX, &lw, m);
}

void GsGetLws(GsCOORDINATE2 *coord, MATRIX *lw, MATRIX *ls)
{
    GsGetLw(coord, lw);
    compose(&GsWSMATRIX, lw, ls);
}

void GsSetLsMatrix(MATRIX *mp)
{
    SetRotMatrix(mp);
    SetTransMatrix(mp);
}

/* lights are kept in world space: light matrix = light directions * local-to-world */
void GsSetLightMatrix(MATRIX *mp)
{
    MATRIX lm;

    compose(&GsLIGHTWSMATRIX, mp, &lm);
    SetLightMatrix(&lm);
}

void GsMulCoord0(MATRIX *m1, MATRIX *m2, MATRIX *m3)
{
    compose(m1, m2, m3);
}

void GsMulCoord3(MATRIX *m1, MATRIX *m2)
{
    compose(m1, m2, m2);
}

/* ---- view ------------------------------------------------------------------ */

void GsSetProjection(long h)
{
    gs_projection = h;
    SetGeomScreen(h);
}

void GsSetNearClip(long nearz)
{
    gs_near_clip = nearz;
}

static void set_world_screen(const MATRIX *view, GsCOORDINATE2 *super)
{
    if (super != NULL && super != &WORLD) {
        MATRIX lw, inv;

        GsGetLw(super, &lw);
        invert_rigid(&lw, &inv);
        compose(view, &inv, &GsWSMATRIX);
    } else {
        GsWSMATRIX = *view;
    }
}

int GsSetView2(GsVIEW2 *pv)
{
    set_world_screen(&pv->view, pv->super);
    return 0;
}

/* view from a viewpoint looking at a reference point; screen x right, y down,
 * z into the screen; rz twists the view (4096 = 360 degrees) */
int GsSetRefView2(GsRVIEW2 *pv)
{
    double fx = (double)pv->vrx - pv->vpx;
    double fy = (double)pv->vry - pv->vpy;
    double fz = (double)pv->vrz - pv->vpz;
    double len = sqrt(fx * fx + fy * fy + fz * fz);
    double z[3], y[3], x[3], d, a, c, s;
    MATRIX view;
    int i;

    if (len == 0)
        return -1;
    z[0] = fx / len;
    z[1] = fy / len;
    z[2] = fz / len;
    /* screen down is world +y projected off the view direction */
    d = z[1];
    y[0] = -d * z[0];
    y[1] = 1.0 - d * z[1];
    y[2] = -d * z[2];
    len = sqrt(y[0] * y[0] + y[1] * y[1] + y[2] * y[2]);
    if (len < 1e-6) { /* looking straight up or down: pick world +z as down */
        d = z[2];
        y[0] = -d * z[0];
        y[1] = -d * z[1];
        y[2] = 1.0 - d * z[2];
        len = sqrt(y[0] * y[0] + y[1] * y[1] + y[2] * y[2]);
    }
    for (i = 0; i < 3; i++)
        y[i] /= len;
    /* x = y cross z */
    x[0] = y[1] * z[2] - y[2] * z[1];
    x[1] = y[2] * z[0] - y[0] * z[2];
    x[2] = y[0] * z[1] - y[1] * z[0];
    /* twist around the view axis */
    a = pv->rz * (2.0 * M_PI / 4096.0);
    c = cos(a);
    s = sin(a);
    for (i = 0; i < 3; i++) {
        double xi = x[i], yi = y[i];
        x[i] = c * xi + s * yi;
        y[i] = -s * xi + c * yi;
    }
    for (i = 0; i < 3; i++) {
        view.m[0][i] = (short)lround(x[i] * 4096.0);
        view.m[1][i] = (short)lround(y[i] * 4096.0);
        view.m[2][i] = (short)lround(z[i] * 4096.0);
    }
    for (i = 0; i < 3; i++)
        view.t[i] = (int)(-(((long long)view.m[i][0] * pv->vpx + (long long)view.m[i][1] * pv->vpy +
                             (long long)view.m[i][2] * pv->vpz) >> 12));
    set_world_screen(&view, pv->super);
    return 0;
}

/* ---- lights ---------------------------------------------------------------- */

/* Three flat lights. A light shines along (vx, vy, vz); the GTE lights a
 * surface by dot(row, normal), so the row holds the reversed direction.
 * TODO: confirm the sign against a PS1 capture. */
int GsSetFlatLight(int id, GsF_LIGHT *lt)
{
    double vx = lt->vx, vy = lt->vy, vz = lt->vz;
    double len = sqrt(vx * vx + vy * vy + vz * vz);

    if (id < 0 || id > 2)
        return -1;
    if (len == 0) {
        GsLIGHTWSMATRIX.m[id][0] = GsLIGHTWSMATRIX.m[id][1] = GsLIGHTWSMATRIX.m[id][2] = 0;
    } else {
        GsLIGHTWSMATRIX.m[id][0] = (short)lround(-vx / len * 4096.0);
        GsLIGHTWSMATRIX.m[id][1] = (short)lround(-vy / len * 4096.0);
        GsLIGHTWSMATRIX.m[id][2] = (short)lround(-vz / len * 4096.0);
    }
    gs_light_color.m[0][id] = (short)(lt->r << 4);
    gs_light_color.m[1][id] = (short)(lt->g << 4);
    gs_light_color.m[2][id] = (short)(lt->b << 4);
    SetColorMatrix(&gs_light_color);
    return 0;
}

/* ambient light, 4096 = 1.0 */
void GsSetAmbient(long r, long g, long b)
{
    SetBackColor(r >> 4, g >> 4, b >> 4);
}

void GsSetLightMode(int mode)
{
    gs_light_mode = mode;
}

void GsSetFogParam(void *fogparm)
{
    (void)fogparm;
    GS_TODO("GsSetFogParam");
}

void gs_init_3d(void)
{
    GsWSMATRIX = GsIDMATRIX;
    memset(&GsLIGHTWSMATRIX, 0, sizeof(GsLIGHTWSMATRIX));
    memset(&gs_light_color, 0, sizeof(gs_light_color));
    SetColorMatrix(&gs_light_color);
    SetBackColor(0, 0, 0);
    GsSetProjection(1000);
    gs_light_mode = GsLMODE_NORMAL;
}

/* ---- TMD models ------------------------------------------------------------- */

/* p points at the TMD flags word (just after the ID). Object table offsets
 * are relative to the table; turn them into pointers once (FIXP flag). */
void GsMapModelingData(u_long *p)
{
    u_long nobj = p[1];
    struct TMD_STRUCT *objs = (struct TMD_STRUCT *)(p + 2);
    u_long i;

    if (p[0] & 1)
        return;
    for (i = 0; i < nobj; i++) {
        objs[i].vertop = (u_long *)((u_char *)objs + (u_long)objs[i].vertop);
        objs[i].nortop = (u_long *)((u_char *)objs + (u_long)objs[i].nortop);
        objs[i].primtop = (u_long *)((u_char *)objs + (u_long)objs[i].primtop);
    }
    p[0] |= 1;
}

/* tmd_obj: address of the object table; link object n to objp */
void GsLinkObject4(u_long tmd_obj, GsDOBJ2 *objp, int n)
{
    objp->tmd = (u_long *)((struct TMD_STRUCT *)tmd_obj + n);
}
