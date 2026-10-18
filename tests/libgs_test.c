/* Tests for the port's libgs: TMD layouts, TIM parsing, model mapping and the
 * TMD object renderer, with synthetic data (no game files needed). */
#include <stdio.h>
#include <string.h>

#include <libgpu.h>
#include <libgte.h>
#include <libgs.h>

static int failures;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__,   \
                    #cond);                                                    \
            failures++;                                                        \
        }                                                                      \
    } while (0)

/* TMD packet sizes from the TMD format: (ilen + 1) words */
_Static_assert(sizeof(TMD_P_F3) == 16, "F3");
_Static_assert(sizeof(TMD_P_G3) == 20, "G3");
_Static_assert(sizeof(TMD_P_TF3) == 24, "TF3");
_Static_assert(sizeof(TMD_P_TG3) == 28, "TG3");
_Static_assert(sizeof(TMD_P_NF3) == 16, "NF3");
_Static_assert(sizeof(TMD_P_TNF3) == 28, "TNF3");
_Static_assert(sizeof(TMD_P_TNG3) == 36, "TNG3");
_Static_assert(sizeof(TMD_P_F4) == 20, "F4");
_Static_assert(sizeof(TMD_P_G4) == 24, "G4");
_Static_assert(sizeof(TMD_P_TF4) == 32, "TF4");
_Static_assert(sizeof(TMD_P_TG4) == 36, "TG4");
_Static_assert(sizeof(TMD_P_NF4) == 16, "NF4");
_Static_assert(sizeof(TMD_P_TNF4) == 32, "TNF4");
_Static_assert(sizeof(TMD_P_TNG4) == 44, "TNG4");
_Static_assert(sizeof(struct TMD_STRUCT) == 28, "TMD object table entry");
_Static_assert(sizeof(GsDOBJ2) == 16, "GsDOBJ2");
_Static_assert(sizeof(GsIMAGE) == 28, "GsIMAGE");

static void test_tim(void)
{
    /* 4-bit TIM with a 16-colour CLUT at (0, 480) and a 16x16 image at (640, 0) */
    static u_long tim[3 + 8 + 3 + 32];
    GsIMAGE im;

    tim[0] = 0x10;                 /* ID */
    tim[1] = 8;                    /* flags: 4-bit, CLUT present */
    tim[2] = 12 + 32;              /* CLUT block length in bytes */
    tim[3] = (480u << 16) | 0;     /* y, x */
    tim[4] = (1u << 16) | 16;      /* h, w */
    /* tim[5..12]: 16 CLUT entries */
    tim[13] = 12 + 128;            /* pixel block length in bytes */
    tim[14] = (0u << 16) | 640;
    tim[15] = (16u << 16) | 4;     /* 16 lines of 4 16-bit units */

    GsGetTimInfo(tim + 1, &im);
    CHECK(im.pmode == 8);
    CHECK(im.cx == 0 && im.cy == 480 && im.cw == 16 && im.ch == 1);
    CHECK(im.clut == tim + 5);
    CHECK(im.px == 640 && im.py == 0 && im.pw == 4 && im.ph == 16);
    CHECK(im.pixel == tim + 16);
}

/* one object: 4 vertices, 1 normal, a lit flat triangle and an unlit
 * textured quad, as a TMD file stores them (offsets relative to the table) */
static u_long tmd[64];

static void build_tmd(void)
{
    u_long *p = tmd;
    struct TMD_STRUCT *obj;
    SVECTOR *v, *n;
    u_long *prim;
    TMD_P_F3 *f3;
    TMD_P_TNF4 *t4;

    p[0] = 0x41;  /* ID */
    p[1] = 0;     /* flags: offsets not mapped yet */
    p[2] = 1;     /* objects */
    obj = (struct TMD_STRUCT *)(p + 3);
    prim = p + 3 + 7;
    v = (SVECTOR *)(prim + 4 + 8);
    n = v + 4;

    obj->primtop = (u_long *)((u_char *)prim - (u_char *)obj);
    obj->primn = 2;
    obj->vertop = (u_long *)((u_char *)v - (u_char *)obj);
    obj->vern = 4;
    obj->nortop = (u_long *)((u_char *)n - (u_char *)obj);
    obj->norn = 1;
    obj->scale = 0;

    /* lit flat triangle, facing the camera (screen winding is clockwise) */
    f3 = (TMD_P_F3 *)prim;
    f3->out = 4; f3->in = 3; f3->flg = 0; f3->cd = 0x20;
    f3->r0 = 200; f3->g0 = 100; f3->b0 = 50; f3->code = 0x20;
    f3->n0 = 0; f3->v0 = 0; f3->v1 = 1; f3->v2 = 2;

    /* unlit textured quad (flag LGT), semi-transparent (mode ABE) */
    t4 = (TMD_P_TNF4 *)(prim + 4);
    t4->out = 9; t4->in = 7; t4->flg = 1; t4->cd = 0x2c | 0x02;
    t4->tu0 = 0; t4->tv0 = 0; t4->clut = 0x7800;
    t4->tu1 = 63; t4->tv1 = 0; t4->tpage = 0x000a;
    t4->tu2 = 0; t4->tv2 = 63; t4->id = 0;
    t4->tu3 = 63; t4->tv3 = 63; t4->dummy = 0;
    t4->r0 = 128; t4->g0 = 128; t4->b0 = 128; t4->p0 = 0;
    t4->v0 = 0; t4->v1 = 1; t4->v2 = 3; t4->v3 = 2;

    v[0].vx = -100; v[0].vy = -100; v[0].vz = 0;
    v[1].vx = 100;  v[1].vy = -100; v[1].vz = 0;
    v[2].vx = -100; v[2].vy = 100;  v[2].vz = 0;
    v[3].vx = 100;  v[3].vy = 100;  v[3].vz = 0;
    n[0].vx = 0; n[0].vy = 0; n[0].vz = -4096;
}

static void test_object(void)
{
    static GsOT_TAG ot_tags[1 << 8];
    static u_long packets[256];
    GsOT ot;
    GsDOBJ2 obj;
    MATRIX m = {{{4096, 0, 0}, {0, 4096, 0}, {0, 0, 4096}}, {0, 0, 1024}};
    struct TMD_STRUCT *t;
    P_TAG *first, *p;
    int count = 0, codes = 0;

    build_tmd();
    GsMapModelingData(tmd + 1);
    CHECK(tmd[1] & 1);
    t = (struct TMD_STRUCT *)(tmd + 3);
    CHECK(t->primtop == tmd + 10);
    GsMapModelingData(tmd + 1); /* mapping twice is a no-op */
    CHECK(t->primtop == tmd + 10);

    GsLinkObject4((u_long)(tmd + 3), &obj, 0);
    CHECK(obj.tmd == (u_long *)t);
    obj.attribute = 0;
    obj.coord2 = NULL;

    ResetGraph(0);
    InitGeom();
    SetGeomOffset(160, 120);
    SetGeomScreen(512);
    GsSetLsMatrix(&m);
    GsSetLightMatrix(&GsIDMATRIX);

    ot.length = 8;
    ot.org = ot_tags;
    GsClearOt(0, 0, &ot);
    CHECK(ot.tag == &ot_tags[255]);
    GsSetWorkBase((PACKET *)packets);
    GsSortObject4J(&obj, &ot, 2, NULL);
    CHECK(GsGetWorkBase() > (PACKET *)packets);

    /* walk the OT from the deepest entry until the list leaves the OT and the
     * packet buffer (the terminator lives in libgpu) */
    first = (P_TAG *)ot.tag;
    for (p = first; p != NULL;) {
        u_char *a = (u_char *)p;
        int in_ot = a >= (u_char *)ot_tags && a < (u_char *)(ot_tags + 256);
        int in_pk = a >= (u_char *)packets && a < (u_char *)(packets + 256);
        if (!in_ot && !in_pk)
            break;
        if (p->len != 0) {
            count++;
            if ((p->code & ~3) == 0x20) {
                POLY_F3 *f = (POLY_F3 *)p;
                codes |= 1;
                /* projected 100 units at distance 1024 with h = 512: +-50 px */
                CHECK(f->x0 == 110 && f->y0 == 70);
                CHECK(f->x1 == 210 && f->y1 == 70);
            }
            if ((p->code & ~3) == 0x2c) {
                POLY_FT4 *f = (POLY_FT4 *)p;
                codes |= 2;
                CHECK(p->code & 2);          /* semi-transparent */
                CHECK(f->r0 == 128);         /* unlit: colour from the data */
                CHECK(f->u3 == 63 && f->v3 == 63);
                CHECK(f->tpage == 0x000a && f->clut == 0x7800);
            }
        }
        if (count > 16)
            break;
        p = (P_TAG *)p->addr;
    }
    CHECK(count == 2);
    CHECK(codes == 3);

    /* a hidden object draws nothing */
    GsClearOt(0, 0, &ot);
    GsSetWorkBase((PACKET *)packets);
    obj.attribute = GsDOFF;
    GsSortObject4J(&obj, &ot, 2, NULL);
    CHECK(GsGetWorkBase() == (PACKET *)packets);
}

static void test_coords(void)
{
    GsCOORDINATE2 parent, child;
    MATRIX lw;

    GsInitCoordinate2(NULL, &parent);
    GsInitCoordinate2(&parent, &child);
    parent.coord.t[0] = 100;
    child.coord.t[1] = 50;
    GsGetLw(&child, &lw);
    CHECK(lw.t[0] == 100 && lw.t[1] == 50 && lw.t[2] == 0);
    CHECK(lw.m[0][0] == 4096 && lw.m[1][1] == 4096);
}

static void test_view(void)
{
    GsRVIEW2 view;

    memset(&view, 0, sizeof(view));
    view.vpz = -1000; /* looking down +z from z = -1000 */
    CHECK(GsSetRefView2(&view) == 0);
    view.vrz = 1;
    CHECK(GsSetRefView2(&view) == 0);
    CHECK(GsWSMATRIX.m[0][0] == 4096 && GsWSMATRIX.m[1][1] == 4096 && GsWSMATRIX.m[2][2] == 4096);
    CHECK(GsWSMATRIX.t[2] == 1000);
}

int main(void)
{
    test_tim();
    test_object();
    test_coords();
    test_view();
    if (failures) {
        fprintf(stderr, "libgs_test: %d failure(s)\n", failures);
        return 1;
    }
    printf("libgs_test: ok\n");
    return 0;
}
