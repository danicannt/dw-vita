/*
 * libgs.h - PSY-Q 4.x extended graphics library (libgs) API for the native port.
 *
 * Written for this port from the public PSY-Q library documentation and the TMD
 * file format; it shadows PSY-Z's own libgs.h, whose libgs subset does not match
 * the PSY-Q 4.1 API that Digimon World was built against (GsSortSprite's
 * priority argument, GsIMAGE's TIM fields, GsF_LIGHT, the TMD primitive types).
 * The implementation lives in port/src/libgs/.
 *
 * Layout notes:
 *  - The build is 32-bit (like the PS1 and the Vita): u_long and pointers are
 *    4 bytes, so TMD_STRUCT and GsDOBJ2 keep the on-disc / retail layout.
 *  - GsOT_TAG follows PSY-Z's ordering table tag (O_TAG: full pointer + length)
 *    instead of the PS1's packed 24-bit link, so GPU packets are larger than on
 *    the PS1. Game code that hard-codes packet sizes must use sizeof.
 */
#ifndef DW1_LIBGS_H
#define DW1_LIBGS_H

/* PSY-Z's header uses this guard; defining it keeps it out if something else
 * includes it after us. */
#define LIBGS_H

#include <libgpu.h>
#include <libgte.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- constants ----------------------------------------------------------- */

#define GsOFSGTE 0
#define GsOFSGPU 4
#define GsINTER 1
#define GsNONINTER 0
#define GsRESET0 0
#define GsRESET3 (3 << 4)

/* lighting modes (second index of the _GsFCALL tables) */
#define GsLMODE_NORMAL 0
#define GsLMODE_FOG 1
#define GsLMODE_LOFF 2
#define GsLMODE_NORMAL_FOG 3

/* subdivision modes (first index of the _GsFCALL tables) */
#define GsDivMODE_NDIV 0
#define GsDivMODE_DIV 1

/* object attribute bits (GsDOBJ2.attribute) */
#define GsDOFF (1u << 31)    /* do not display */
#define GsALON (1u << 30)    /* semi-transparency on */
#define GsAZERO (0u << 28)   /* semi-transparency rate 0.5 back + 0.5 front */
#define GsAONE (1u << 28)    /* 1.0 back + 1.0 front */
#define GsATWO (2u << 28)    /* 1.0 back - 1.0 front */
#define GsATHREE (3u << 28)  /* 1.0 back + 0.25 front */
#define GsLLMOD (1u << 5)    /* local light mode */
#define GsLOFF (1u << 6)     /* light source calculation off */
#define GsLLIGHT (1u << 7)
#define GsDIV1 (1u << 9)     /* polygon subdivision 2x2 */
#define GsDIV2 (2u << 9)
#define GsDIV3 (3u << 9)
#define GsDIV4 (4u << 9)
#define GsDIV5 (5u << 9)

#define GsMATE_C (3 << 24)

/* ---- basic types --------------------------------------------------------- */

typedef unsigned char PACKET;

/* ordering table entry: same shape as a PSY-Z OT entry (see O_TAG) */
typedef struct {
    O_TAG;
} GsOT_TAG;

typedef struct {
    u_long length;    /* OT resolution: log2 of the number of entries */
    GsOT_TAG *org;    /* start of the OT entries */
    u_long offset;    /* Z offset of the OT */
    u_long point;     /* representative Z when sorted into another OT */
    GsOT_TAG *tag;    /* last (deepest) entry, the start of the drawing list */
} GsOT;

typedef struct {
    VECTOR scale;
    SVECTOR rotate;
    VECTOR trans;
} GsCOORD2PARAM;

typedef struct _GsCOORDINATE2 {
    u_long flg;                    /* 0 when coord changed since the last GsGetLw */
    MATRIX coord;                  /* local matrix relative to super */
    MATRIX workm;                  /* cached local-to-world matrix */
    GsCOORD2PARAM *param;
    struct _GsCOORDINATE2 *super;  /* parent system, NULL = world */
    struct _GsCOORDINATE2 *sub;
} GsCOORDINATE2;

typedef struct {
    MATRIX view;
    GsCOORDINATE2 *super;
} GsVIEW2;

typedef struct {
    long vpx, vpy, vpz;    /* viewpoint */
    long vrx, vry, vrz;    /* reference point */
    long rz;               /* twist, 4096 = 360 degrees */
    GsCOORDINATE2 *super;
} GsRVIEW2;

typedef struct {
    long vx, vy, vz;       /* light direction */
    u_char r, g, b;
} GsF_LIGHT;

typedef struct {
    u_long attribute;
    GsCOORDINATE2 *coord2;
    u_long *tmd;           /* first primitive of the object, set by GsLinkObject4 */
    u_long id;
} GsDOBJ2;

typedef struct {
    u_long attribute;
    short x, y;
    u_short w, h;
    u_short tpage;
    u_char u, v;
    short cx, cy;          /* CLUT position in VRAM */
    u_char r, g, b;
    short mx, my;          /* rotation / scaling centre */
    short scalex, scaley;  /* 4096 = 1.0 */
    long rotate;           /* 4096 = 1 degree */
} GsSPRITE;

typedef struct {
    u_long attribute;
    short x, y;
    u_short w, h;
    u_char r, g, b;
} GsBOXF;

typedef struct {
    u_long attribute;
    short x0, y0;
    short x1, y1;
    u_char r, g, b;
} GsLINE;

typedef struct {
    u_long attribute;
    short x0, y0;
    short x1, y1;
    u_char r0, g0, b0;
    u_char r1, g1, b1;
} GsGLINE;

/* TIM image description, filled by GsGetTimInfo */
typedef struct {
    u_long pmode;          /* pixel mode: 0 4-bit, 1 8-bit, 2 16-bit, 3 24-bit; bit 3 = has CLUT */
    short px, py;          /* image position in VRAM */
    u_short pw, ph;        /* image size in VRAM units (16-bit pixels) */
    u_long *pixel;
    short cx, cy;          /* CLUT position */
    u_short cw, ch;        /* CLUT size */
    u_long *clut;
} GsIMAGE;

/* ---- TMD model data ------------------------------------------------------ */

/* one object of a TMD file; GsMapModelingData turns the offsets into pointers */
struct TMD_STRUCT {
    u_long *vertop;
    u_long vern;
    u_long *nortop;     /* normals (PSY-Q's spelling) */
    u_long norn;
    u_long *primtop;
    u_long primn;
    u_long scale;
};

/* TMD primitive packets. The first word of every packet is
 * out (olen), in (ilen), flg (flag), cd (mode). */
#define TMD_P_HEADER u_char out, in, flg, cd

typedef struct { TMD_P_HEADER; u_char r0, g0, b0, code; u_short n0, v0, v1, v2; } TMD_P_F3;
typedef struct { TMD_P_HEADER; u_char r0, g0, b0, code; u_short n0, v0, n1, v1, n2, v2; } TMD_P_G3;
typedef struct {
    TMD_P_HEADER;
    u_char r0, g0, b0, code;
    u_char r1, g1, b1, dummy1;
    u_char r2, g2, b2, dummy2;
    u_short n0, v0, v1, v2;
} TMD_P_F3G;
typedef struct {
    TMD_P_HEADER;
    u_char r0, g0, b0, code;
    u_char r1, g1, b1, dummy1;
    u_char r2, g2, b2, dummy2;
    u_short n0, v0, n1, v1, n2, v2;
} TMD_P_G3G;
typedef struct { TMD_P_HEADER; u_char r0, g0, b0, code; u_short v0, v1, v2, dummy; } TMD_P_NF3;
typedef struct {
    TMD_P_HEADER;
    u_char r0, g0, b0, code;
    u_char r1, g1, b1, dummy1;
    u_char r2, g2, b2, dummy2;
    u_short v0, v1, v2, dummy;
} TMD_P_NG3;
typedef struct {
    TMD_P_HEADER;
    u_char tu0, tv0; u_short clut;
    u_char tu1, tv1; u_short tpage;
    u_char tu2, tv2; u_short id;
    u_short n0, v0, v1, v2;
} TMD_P_TF3;
typedef struct {
    TMD_P_HEADER;
    u_char tu0, tv0; u_short clut;
    u_char tu1, tv1; u_short tpage;
    u_char tu2, tv2; u_short id;
    u_short n0, v0, n1, v1, n2, v2;
} TMD_P_TG3;
typedef struct {
    TMD_P_HEADER;
    u_char tu0, tv0; u_short clut;
    u_char tu1, tv1; u_short tpage;
    u_char tu2, tv2; u_short id;
    u_char r0, g0, b0, p0;
    u_short v0, v1, v2, dummy;
} TMD_P_TNF3;
typedef struct {
    TMD_P_HEADER;
    u_char tu0, tv0; u_short clut;
    u_char tu1, tv1; u_short tpage;
    u_char tu2, tv2; u_short id;
    u_char r0, g0, b0, p0;
    u_char r1, g1, b1, p1;
    u_char r2, g2, b2, p2;
    u_short v0, v1, v2, dummy;
} TMD_P_TNG3;

typedef struct { TMD_P_HEADER; u_char r0, g0, b0, code; u_short n0, v0, v1, v2, v3, dummy; } TMD_P_F4;
typedef struct { TMD_P_HEADER; u_char r0, g0, b0, code; u_short n0, v0, n1, v1, n2, v2, n3, v3; } TMD_P_G4;
typedef struct {
    TMD_P_HEADER;
    u_char r0, g0, b0, code;
    u_char r1, g1, b1, dummy1;
    u_char r2, g2, b2, dummy2;
    u_char r3, g3, b3, dummy3;
    u_short n0, v0, v1, v2, v3, dummy;
} TMD_P_F4G;
typedef struct {
    TMD_P_HEADER;
    u_char r0, g0, b0, code;
    u_char r1, g1, b1, dummy1;
    u_char r2, g2, b2, dummy2;
    u_char r3, g3, b3, dummy3;
    u_short n0, v0, n1, v1, n2, v2, n3, v3;
} TMD_P_G4G;
typedef struct { TMD_P_HEADER; u_char r0, g0, b0, code; u_short v0, v1, v2, v3; } TMD_P_NF4;
typedef struct {
    TMD_P_HEADER;
    u_char r0, g0, b0, code;
    u_char r1, g1, b1, dummy1;
    u_char r2, g2, b2, dummy2;
    u_char r3, g3, b3, dummy3;
    u_short v0, v1, v2, v3;
} TMD_P_NG4;
typedef struct {
    TMD_P_HEADER;
    u_char tu0, tv0; u_short clut;
    u_char tu1, tv1; u_short tpage;
    u_char tu2, tv2; u_short id;
    u_char tu3, tv3; u_short dummy;
    u_short n0, v0, v1, v2, v3, dummy0;
} TMD_P_TF4;
typedef struct {
    TMD_P_HEADER;
    u_char tu0, tv0; u_short clut;
    u_char tu1, tv1; u_short tpage;
    u_char tu2, tv2; u_short id;
    u_char tu3, tv3; u_short dummy;
    u_short n0, v0, n1, v1, n2, v2, n3, v3;
} TMD_P_TG4;
typedef struct {
    TMD_P_HEADER;
    u_char tu0, tv0; u_short clut;
    u_char tu1, tv1; u_short tpage;
    u_char tu2, tv2; u_short id;
    u_char tu3, tv3; u_short dummy;
    u_char r0, g0, b0, p0;
    u_short v0, v1, v2, v3;
} TMD_P_TNF4;
typedef struct {
    TMD_P_HEADER;
    u_char tu0, tv0; u_short clut;
    u_char tu1, tv1; u_short tpage;
    u_char tu2, tv2; u_short id;
    u_char tu3, tv3; u_short dummy;
    u_char r0, g0, b0, p0;
    u_char r1, g1, b1, p1;
    u_char r2, g2, b2, p2;
    u_char r3, g3, b3, p3;
    u_short v0, v1, v2, v3;
} TMD_P_TNG4;

/* GPU command codes of the packets the TMD drivers build */
#define GPU_COM_F3 0x20
#define GPU_COM_TF3 0x24
#define GPU_COM_G3 0x30
#define GPU_COM_TG3 0x34
#define GPU_COM_F4 0x28
#define GPU_COM_TF4 0x2c
#define GPU_COM_G4 0x38
#define GPU_COM_TG4 0x3c
#define GPU_COM_NF3 0x21
#define GPU_COM_NTF3 0x25
#define GPU_COM_NG3 0x31
#define GPU_COM_NTG3 0x35
#define GPU_COM_NF4 0x29
#define GPU_COM_NTF4 0x2d
#define GPU_COM_NG4 0x39
#define GPU_COM_NTG4 0x3d

/* ---- TMD primitive drivers ------------------------------------------------- */

/* Signature shared by every driver: draws n primitives starting at primtop
 * into the packet area pk and the ordering table ot; returns the next free
 * packet address. Declared unprototyped, like the game does. */
typedef PACKET *(*GsTMD_DRIVER)();

/* Driver table used by GsSortObject4J, indexed [GsDivMODE_*][GsLMODE_*]
 * for the lit types and [GsDivMODE_*] for the unlit ("n") types. */
typedef struct {
    GsTMD_DRIVER f3[2][3], nf3[2], g3[2][3], ng3[2];
    GsTMD_DRIVER tf3[2][3], ntf3[2], tg3[2][3], ntg3[2];
    GsTMD_DRIVER f4[2][3], nf4[2], g4[2][3], ng4[2];
    GsTMD_DRIVER tf4[2][3], ntf4[2], tg4[2][3], ntg4[2];
    GsTMD_DRIVER f3g[3], g3g[3], f4g[3], g4g[3];
} _GsFCALL;

extern _GsFCALL GsFCALL4;

PACKET *GsTMDfastF3L(), *GsTMDfastF3NL(), *GsTMDfastNF3();
PACKET *GsTMDfastG3L(), *GsTMDfastG3NL(), *GsTMDfastNG3();
PACKET *GsTMDfastTF3L(), *GsTMDfastTF3NL(), *GsTMDfastTNF3();
PACKET *GsTMDfastTG3L(), *GsTMDfastTG3NL(), *GsTMDfastTNG3();
PACKET *GsTMDfastF4L(), *GsTMDfastF4NL(), *GsTMDfastNF4();
PACKET *GsTMDfastG4L(), *GsTMDfastG4NL(), *GsTMDfastNG4();
PACKET *GsTMDfastTF4L(), *GsTMDfastTF4NL(), *GsTMDfastTNF4();
PACKET *GsTMDfastTG4L(), *GsTMDfastTG4NL(), *GsTMDfastTNG4();
PACKET *GsTMDdivF3L(), *GsTMDdivF3NL(), *GsTMDdivNF3();
PACKET *GsTMDdivG3L(), *GsTMDdivG3NL(), *GsTMDdivNG3();
PACKET *GsTMDdivTF3L(), *GsTMDdivTF3NL(), *GsTMDdivTNF3();
PACKET *GsTMDdivTG3L(), *GsTMDdivTG3NL(), *GsTMDdivTNG3();
PACKET *GsTMDdivF4L(), *GsTMDdivF4NL(), *GsTMDdivNF4();
PACKET *GsTMDdivG4L(), *GsTMDdivG4NL(), *GsTMDdivNG4();
PACKET *GsTMDdivTF4L(), *GsTMDdivTF4NL(), *GsTMDdivTNF4();
PACKET *GsTMDdivTG4L(), *GsTMDdivTG4NL(), *GsTMDdivTNG4();

/* ---- globals ------------------------------------------------------------- */

extern MATRIX GsWSMATRIX;       /* world-to-screen matrix */
extern MATRIX GsIDMATRIX;       /* identity */
extern MATRIX GsIDMATRIX2;      /* identity with aspect correction */
extern MATRIX GsLIGHTWSMATRIX;  /* light directions in world space */
extern PACKET *GsOUT_PACKET_P;  /* packet work area */
extern DRAWENV GsDRAWENV;
extern DISPENV GsDISPENV;
extern GsCOORDINATE2 WORLD;     /* PSY-Q exports the world system as WORLD (a NULL super is world) */

/* ---- functions ----------------------------------------------------------- */

/* system */
void GsInitGraph(u_short x, u_short y, u_short intmode, u_short dith, u_short varmmode);
void GsInit3D(void);
void GsDefDispBuff(u_short x0, u_short y0, u_short x1, u_short y1);
void GsSwapDispBuff(void);
int GsGetActiveBuff(void);
void GsSetDrawBuffClip(void);
void GsSetDrawBuffOffset(void);
void GsSetOrign(long x, long y);
void GsSetWorkBase(PACKET *base);
PACKET *GsGetWorkBase(void);
void GsClearVcount(void);
long GsGetVcount(void);

/* ordering tables */
void GsClearOt(u_short offset, u_short point, GsOT *otp);
void GsDrawOt(GsOT *otp);
GsOT *GsSortOt(GsOT *src, GsOT *dst);
void GsSortClear(u_char r, u_char g, u_char b, GsOT *otp);

/* 2D */
void GsSortSprite(GsSPRITE *sp, GsOT *otp, u_short pri);
void GsSortFastSprite(GsSPRITE *sp, GsOT *otp, u_short pri);
void GsSortBoxFill(GsBOXF *bp, GsOT *otp, u_short pri);
void GsSortLine(GsLINE *lp, GsOT *otp, u_short pri);
void GsSortGLine(GsGLINE *lp, GsOT *otp, u_short pri);
void GsSortPoly(void *pp, GsOT *otp, u_short pri);
void GsGetTimInfo(u_long *tim, GsIMAGE *im);

/* coordinates and matrices */
void GsInitCoordinate2(GsCOORDINATE2 *super, GsCOORDINATE2 *base);
void GsGetLw(GsCOORDINATE2 *coord, MATRIX *m);
void GsGetLs(GsCOORDINATE2 *coord, MATRIX *m);
void GsGetLws(GsCOORDINATE2 *coord, MATRIX *lw, MATRIX *ls);
void GsSetLsMatrix(MATRIX *mp);
void GsSetLightMatrix(MATRIX *mp);
void GsMulCoord0(MATRIX *m1, MATRIX *m2, MATRIX *m3);
void GsMulCoord2(MATRIX *m1, MATRIX *m2);
void GsMulCoord3(MATRIX *m1, MATRIX *m2);

/* view and light */
void GsSetProjection(long h);
int GsSetView2(GsVIEW2 *pv);
int GsSetRefView2(GsRVIEW2 *pv);
void GsSetNearClip(long nearz);
int GsSetFlatLight(int id, GsF_LIGHT *lt);
void GsSetAmbient(long r, long g, long b);
void GsSetLightMode(int mode);
void GsSetFogParam(void *fogparm);

/* 3D objects */
void GsMapModelingData(u_long *p);
void GsLinkObject4(u_long tmd_obj, GsDOBJ2 *objp, int n);
void GsSortObject4(GsDOBJ2 *objp, GsOT *otp, int shift, u_long *scratch);
void GsSortObject4J(GsDOBJ2 *objp, GsOT *otp, int shift, u_long *scratch);

#ifdef __cplusplus
}
#endif

#endif /* DW1_LIBGS_H */
