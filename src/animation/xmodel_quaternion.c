#include "xmodel.h"

#include "compat/coduo_x87emu.h"

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

/*
 * XModel quaternion-to-axis expansion.
 *
 * Windows authority: CoDUOMP.exe 0x0049ce40..0x0049cf67.
 * Linux authority: coduo_lnxded 0x080c4eb4..0x080c5084.
 *
 * The inputs, output slots, zero-length fallback, and matrix result agree.
 * The original compilers selected different x87 operation graphs and spill
 * points, so the complete behavior bodies remain separate.  Windows retains
 * length, scale, xx/yy/zz, xy, and zw in PC=53 x87 registers.  Linux stores
 * length, scale, all scaled squares, and all pair products as binary32 under
 * the server PC=64 policy before forming the output expressions.
 */

#if defined(WINDOWS_BEHAVIOR)
void XModelExpandQuatToAxis(float *quat)
{
    const float q0 = quat[0];
    const float q1 = quat[1];
    const float q2 = quat[2];
    const float q3 = quat[3];

#if EMULATE_X87
    const float q0Sq = x87f_store_f32(x87f_mul(x87f_load_f32(q0), x87f_load_f32(q0)));
    const float q1Sq = x87f_store_f32(x87f_mul(x87f_load_f32(q1), x87f_load_f32(q1)));
    const float q2Sq = x87f_store_f32(x87f_mul(x87f_load_f32(q2), x87f_load_f32(q2)));
    const x87f lengthSq = x87f_add(
        x87f_add(x87f_add(x87f_mul(x87f_load_f32(q3), x87f_load_f32(q3)), x87f_load_f32(q2Sq)), x87f_load_f32(q1Sq)), x87f_load_f32(q0Sq));

    if (!x87f_eq(lengthSq, x87f_load_f32(0.0f))) {
        const x87f scale = x87f_div(x87f_load_f32(2.0f), lengthSq);
        const x87f xx = x87f_mul(x87f_load_f32(q0Sq), scale);
        const x87f yy = x87f_mul(x87f_load_f32(q1Sq), scale);
        const x87f zz = x87f_mul(x87f_load_f32(q2Sq), scale);
        const x87f x = x87f_mul(x87f_load_f32(q0), scale);
        const x87f xy = x87f_mul(x, x87f_load_f32(q1));
        const float xz = x87f_store_f32(x87f_mul(x, x87f_load_f32(q2)));
        const float xw = x87f_store_f32(x87f_mul(x, x87f_load_f32(q3)));
        const x87f y = x87f_mul(x87f_load_f32(q1), scale);
        const float yz = x87f_store_f32(x87f_mul(y, x87f_load_f32(q2)));
        const float yw = x87f_store_f32(x87f_mul(y, x87f_load_f32(q3)));
        const x87f zw = x87f_mul(x87f_mul(x87f_load_f32(q3), x87f_load_f32(q2)), scale);

        quat[0] = x87f_store_f32(x87f_sub(x87f_load_f32(1.0f), x87f_add(zz, yy)));
        quat[1] = x87f_store_f32(x87f_add(zw, xy));
        quat[2] = x87f_store_f32(x87f_sub(x87f_load_f32(xz), x87f_load_f32(yw)));
        quat[4] = x87f_store_f32(x87f_sub(xy, zw));
        quat[5] = x87f_store_f32(x87f_sub(x87f_load_f32(1.0f), x87f_add(zz, xx)));
        quat[6] = x87f_store_f32(x87f_add(x87f_load_f32(yz), x87f_load_f32(xw)));
        quat[8] = x87f_store_f32(x87f_add(x87f_load_f32(yw), x87f_load_f32(xz)));
        quat[9] = x87f_store_f32(x87f_sub(x87f_load_f32(yz), x87f_load_f32(xw)));
        quat[10] = x87f_store_f32(x87f_sub(x87f_load_f32(1.0f), x87f_add(yy, xx)));
        return;
    }
#else
    const float q0Sq = (float)((long double)q0 * (long double)q0);
    const float q1Sq = (float)((long double)q1 * (long double)q1);
    const float q2Sq = (float)((long double)q2 * (long double)q2);
    const long double lengthSq = (((long double)q3 * (long double)q3 + (long double)q2Sq) + (long double)q1Sq) + (long double)q0Sq;

    if (lengthSq != (long double)0.0f) {
        const long double scale = (long double)2.0f / lengthSq;
        const long double xx = (long double)q0Sq * scale;
        const long double yy = (long double)q1Sq * scale;
        const long double zz = (long double)q2Sq * scale;
        const long double x = (long double)q0 * scale;
        const long double xy = x * (long double)q1;
        const float xz = (float)(x * (long double)q2);
        const float xw = (float)(x * (long double)q3);
        const long double y = (long double)q1 * scale;
        const float yz = (float)(y * (long double)q2);
        const float yw = (float)(y * (long double)q3);
        const long double zw = ((long double)q3 * (long double)q2) * scale;

        quat[0] = (float)((long double)1.0f - (zz + yy));
        quat[1] = (float)(zw + xy);
        quat[2] = (float)((long double)xz - (long double)yw);
        quat[4] = (float)(xy - zw);
        quat[5] = (float)((long double)1.0f - (zz + xx));
        quat[6] = (float)((long double)yz + (long double)xw);
        quat[8] = (float)((long double)yw + (long double)xz);
        quat[9] = (float)((long double)yz - (long double)xw);
        quat[10] = (float)((long double)1.0f - (yy + xx));
        return;
    }
#endif

    quat[0] = 1.0f;
    quat[1] = 0.0f;
    quat[2] = 0.0f;
    quat[4] = 0.0f;
    quat[5] = 1.0f;
    quat[6] = 0.0f;
    quat[8] = 0.0f;
    quat[9] = 0.0f;
    quat[10] = 1.0f;
}
#else
void XModelExpandQuatToAxis(float *quat)
{
#if EMULATE_X87
    float xx = x87f_store_f32(x87f_mul(x87f_load_f32(quat[0]), x87f_load_f32(quat[0])));
    float yy = x87f_store_f32(x87f_mul(x87f_load_f32(quat[1]), x87f_load_f32(quat[1])));
    float zz = x87f_store_f32(x87f_mul(x87f_load_f32(quat[2]), x87f_load_f32(quat[2])));
    float scale = x87f_store_f32(x87f_add(x87f_add(x87f_add(x87f_load_f32(xx), x87f_load_f32(yy)), x87f_load_f32(zz)),
                                          x87f_mul(x87f_load_f32(quat[3]), x87f_load_f32(quat[3]))));

    if (scale != 0.0f) {
        scale = x87f_store_f32(x87f_div(x87f_load_f32(2.0f), x87f_load_f32(scale)));
        xx = x87f_store_f32(x87f_mul(x87f_load_f32(xx), x87f_load_f32(scale)));
        yy = x87f_store_f32(x87f_mul(x87f_load_f32(yy), x87f_load_f32(scale)));
        zz = x87f_store_f32(x87f_mul(x87f_load_f32(zz), x87f_load_f32(scale)));

        quat[0] = x87f_store_f32(x87f_mul(x87f_load_f32(quat[0]), x87f_load_f32(scale)));
        const float xy = x87f_store_f32(x87f_mul(x87f_load_f32(quat[0]), x87f_load_f32(quat[1])));
        const float xz = x87f_store_f32(x87f_mul(x87f_load_f32(quat[0]), x87f_load_f32(quat[2])));
        const float xw = x87f_store_f32(x87f_mul(x87f_load_f32(quat[0]), x87f_load_f32(quat[3])));
        quat[1] = x87f_store_f32(x87f_mul(x87f_load_f32(quat[1]), x87f_load_f32(scale)));
        const float yz = x87f_store_f32(x87f_mul(x87f_load_f32(quat[1]), x87f_load_f32(quat[2])));
        const float yw = x87f_store_f32(x87f_mul(x87f_load_f32(quat[1]), x87f_load_f32(quat[3])));
        const float zw = x87f_store_f32(x87f_mul(x87f_mul(x87f_load_f32(quat[2]), x87f_load_f32(quat[3])), x87f_load_f32(scale)));

        quat[0] = x87f_store_f32(x87f_sub(x87f_load_f32(1.0f), x87f_add(x87f_load_f32(yy), x87f_load_f32(zz))));
        quat[1] = x87f_store_f32(x87f_add(x87f_load_f32(xy), x87f_load_f32(zw)));
        quat[2] = x87f_store_f32(x87f_sub(x87f_load_f32(xz), x87f_load_f32(yw)));
        quat[4] = x87f_store_f32(x87f_sub(x87f_load_f32(xy), x87f_load_f32(zw)));
        quat[5] = x87f_store_f32(x87f_sub(x87f_load_f32(1.0f), x87f_add(x87f_load_f32(xx), x87f_load_f32(zz))));
        quat[6] = x87f_store_f32(x87f_add(x87f_load_f32(yz), x87f_load_f32(xw)));
        quat[8] = x87f_store_f32(x87f_add(x87f_load_f32(xz), x87f_load_f32(yw)));
        quat[9] = x87f_store_f32(x87f_sub(x87f_load_f32(yz), x87f_load_f32(xw)));
        quat[10] = x87f_store_f32(x87f_sub(x87f_load_f32(1.0f), x87f_add(x87f_load_f32(xx), x87f_load_f32(yy))));
        return;
    }
#else
    float xx = quat[0] * quat[0];
    float yy = quat[1] * quat[1];
    float zz = quat[2] * quat[2];
    float scale = ((xx + yy) + zz) + quat[3] * quat[3];

    if (scale != 0.0f) {
        scale = 2.0f / scale;
        xx *= scale;
        yy *= scale;
        zz *= scale;
        quat[0] *= scale;
        const float xy = quat[0] * quat[1];
        const float xz = quat[0] * quat[2];
        const float xw = quat[0] * quat[3];
        quat[1] *= scale;
        const float yz = quat[1] * quat[2];
        const float yw = quat[1] * quat[3];
        const float zw = quat[2] * quat[3] * scale;

        quat[0] = 1.0f - (yy + zz);
        quat[1] = xy + zw;
        quat[2] = xz - yw;
        quat[4] = xy - zw;
        quat[5] = 1.0f - (xx + zz);
        quat[6] = yz + xw;
        quat[8] = xz + yw;
        quat[9] = yz - xw;
        quat[10] = 1.0f - (xx + yy);
        return;
    }
#endif

    quat[0] = 1.0f;
    quat[1] = 0.0f;
    quat[2] = 0.0f;
    quat[4] = 0.0f;
    quat[5] = 1.0f;
    quat[6] = 0.0f;
    quat[8] = 0.0f;
    quat[9] = 0.0f;
    quat[10] = 1.0f;
}
#endif
