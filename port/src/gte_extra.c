/* libgte functions the game uses that PSY-Z declares or lacks but does not
 * implement. Built on PSY-Z's software GTE where a GTE command is involved,
 * otherwise plain fixed-point C with the GTE's 4.12 format and saturation. */
#include <libgte.h>

#include "dw1_native.h"

/* GTE registers the game uses that PSY-Z's software GTE does not model */
long dw1_gte_lzcs;

static short sat16(long long v)
{
    if (v > 0x7fff)
        return 0x7fff;
    if (v < -0x8000)
        return -0x8000;
    return (short)v;
}

/* v1 = m * v0, 32-bit vectors, full precision (no IR saturation) */
VECTOR *ApplyMatrixLV(MATRIX *m, VECTOR *v0, VECTOR *v1)
{
    long long x = v0->vx, y = v0->vy, z = v0->vz;
    int i;
    int r[3];

    for (i = 0; i < 3; i++)
        r[i] = (int)((m->m[i][0] * x + m->m[i][1] * y + m->m[i][2] * z) >> 12);
    v1->vx = r[0];
    v1->vy = r[1];
    v1->vz = r[2];
    return v1;
}

/* v1 = m * v0, 16-bit vectors, saturated like the GTE's IR registers */
SVECTOR *ApplyMatrixSV(MATRIX *m, SVECTOR *v0, SVECTOR *v1)
{
    long long x = v0->vx, y = v0->vy, z = v0->vz;
    short r[3];
    int i;

    for (i = 0; i < 3; i++)
        r[i] = sat16((m->m[i][0] * x + m->m[i][1] * y + m->m[i][2] * z) >> 12);
    v1->vx = r[0];
    v1->vy = r[1];
    v1->vz = r[2];
    return v1;
}

/* v1 = transpose(m) * v0 */
VECTOR *ApplyTransposeMatrixLV(MATRIX *m, VECTOR *v0, VECTOR *v1)
{
    long long x = v0->vx, y = v0->vy, z = v0->vz;
    int i;
    int r[3];

    for (i = 0; i < 3; i++)
        r[i] = (int)((m->m[0][i] * x + m->m[1][i] * y + m->m[2][i] * z) >> 12);
    v1->vx = r[0];
    v1->vy = r[1];
    v1->vz = r[2];
    return v1;
}

/* m2 = m0 * m1 (rotation part only), column by column through the GTE: each
 * element saturates to 16 bits */
MATRIX *MulMatrix0(MATRIX *m0, MATRIX *m1, MATRIX *m2)
{
    short r[3][3];
    int i, j;

    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            r[i][j] = sat16(((long long)m0->m[i][0] * m1->m[0][j] +
                             (long long)m0->m[i][1] * m1->m[1][j] +
                             (long long)m0->m[i][2] * m1->m[2][j]) >> 12);
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            m2->m[i][j] = r[i][j];
    return m2;
}

/* three NormalColorCol in one (the GTE's NCCT) */
void NormalColorCol3(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, CVECTOR *v3,
                     CVECTOR *v4, CVECTOR *v5, CVECTOR *v6)
{
    NormalColorCol(v0, v3, v4);
    NormalColorCol(v1, v3, v5);
    NormalColorCol(v2, v3, v6);
}

/* Rotate, translate and project 3 (4) vertices and return the normal clip
 * value of the first three; the average Z goes to *otz only when the polygon
 * faces the camera (opz > 0), as on the PS1. */
long RotNclip3(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, int *sxy0, int *sxy1,
               int *sxy2, int *p, int *otz, int *flag)
{
    long z = RotTransPers3(v0, v1, v2, sxy0, sxy1, sxy2, p, flag);
    long opz = NormalClip(*sxy0, *sxy1, *sxy2);

    if (opz > 0)
        *otz = (int)z;
    return opz;
}

long RotNclip4(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *v3, int *sxy0,
               int *sxy1, int *sxy2, int *sxy3, int *p, int *otz, int *flag)
{
    long z = RotTransPers4(v0, v1, v2, v3, sxy0, sxy1, sxy2, sxy3, p, flag);
    long opz = NormalClip(*sxy0, *sxy1, *sxy2);

    if (opz > 0)
        *otz = (int)z;
    return opz;
}
