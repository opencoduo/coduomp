#include "q_math.h"

#include "compat/coduo_x87emu.h"

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

/*
 * The four Windows bodies are instruction-identical apart from relocated
 * constants: CoDUOMP.exe 0x00433660, cgame 0x3004b7c0, UI 0x40003790, and
 * game 0x20018810. The two Linux bodies at coduo_lnxded 0x0806977c and game
 * RVA 0x0003d3cf agree after accounting for PIC.
 *
 * Windows folds the squared magnitude as w*w + z*z + y*y + x*x. Linux folds
 * x*x + y*y + z*z + w*w and reuses the first two input lanes as the rounded
 * scaled-X/scaled-Y temporaries. The resulting formulas agree for ordinary
 * inputs, but the operation order and x87 precision differ observably. The
 * complete platform bodies preserve both original graphs and their in-place
 * read-before-write behavior.
 */
#if defined(WINDOWS_BEHAVIOR)
void ConvertQuatToMat(float quaternionAndMatrix[9])
{
    const float x = quaternionAndMatrix[0];
    const float y = quaternionAndMatrix[1];
    const float z = quaternionAndMatrix[2];
    const float w = quaternionAndMatrix[3];
    float xSquared;
    float ySquared;
    float zSquared;
    float lengthSquared;

#if EMULATE_X87
    xSquared = x87f_store_f32(x87f_mul(x87f_load_f32(x), x87f_load_f32(x)));
    ySquared = x87f_store_f32(x87f_mul(x87f_load_f32(y), x87f_load_f32(y)));
    zSquared = x87f_store_f32(x87f_mul(x87f_load_f32(z), x87f_load_f32(z)));
    lengthSquared = x87f_store_f32(
        x87f_add(x87f_add(x87f_add(x87f_mul(x87f_load_f32(w), x87f_load_f32(w)), x87f_load_f32(zSquared)), x87f_load_f32(ySquared)),
                 x87f_load_f32(xSquared)));
#else
    xSquared = (float)((long double)x * x);
    ySquared = (float)((long double)y * y);
    zSquared = (float)((long double)z * z);
    lengthSquared = (float)((((long double)w * w + zSquared) + ySquared) + xSquared);
#endif

    if (lengthSquared != 0.0f) {
        float scale;
        float xx;
        float yy;
        float zz;
        float scaledX;
        float scaledY;
        float xy;
        float xz;
        float xw;
        float yz;
        float yw;
        float zw;

#if EMULATE_X87
        scale = x87f_store_f32(x87f_div(x87f_load_f32(2.0f), x87f_load_f32(lengthSquared)));
        xx = x87f_store_f32(x87f_mul(x87f_load_f32(xSquared), x87f_load_f32(scale)));
        yy = x87f_store_f32(x87f_mul(x87f_load_f32(ySquared), x87f_load_f32(scale)));
        zz = x87f_store_f32(x87f_mul(x87f_load_f32(zSquared), x87f_load_f32(scale)));
        scaledX = x87f_store_f32(x87f_mul(x87f_load_f32(x), x87f_load_f32(scale)));
        scaledY = x87f_store_f32(x87f_mul(x87f_load_f32(y), x87f_load_f32(scale)));
        xy = x87f_store_f32(x87f_mul(x87f_load_f32(scaledX), x87f_load_f32(y)));
        xz = x87f_store_f32(x87f_mul(x87f_load_f32(scaledX), x87f_load_f32(z)));
        xw = x87f_store_f32(x87f_mul(x87f_load_f32(scaledX), x87f_load_f32(w)));
        yz = x87f_store_f32(x87f_mul(x87f_load_f32(scaledY), x87f_load_f32(z)));
        yw = x87f_store_f32(x87f_mul(x87f_load_f32(scaledY), x87f_load_f32(w)));
        zw = x87f_store_f32(x87f_mul(x87f_mul(x87f_load_f32(z), x87f_load_f32(w)), x87f_load_f32(scale)));

        quaternionAndMatrix[0] = x87f_store_f32(x87f_sub(x87f_load_f32(1.0f), x87f_add(x87f_load_f32(zz), x87f_load_f32(yy))));
        quaternionAndMatrix[1] = x87f_store_f32(x87f_add(x87f_load_f32(zw), x87f_load_f32(xy)));
        quaternionAndMatrix[2] = x87f_store_f32(x87f_sub(x87f_load_f32(xz), x87f_load_f32(yw)));
        quaternionAndMatrix[3] = x87f_store_f32(x87f_sub(x87f_load_f32(xy), x87f_load_f32(zw)));
        quaternionAndMatrix[4] = x87f_store_f32(x87f_sub(x87f_load_f32(1.0f), x87f_add(x87f_load_f32(zz), x87f_load_f32(xx))));
        quaternionAndMatrix[5] = x87f_store_f32(x87f_add(x87f_load_f32(yz), x87f_load_f32(xw)));
        quaternionAndMatrix[6] = x87f_store_f32(x87f_add(x87f_load_f32(yw), x87f_load_f32(xz)));
        quaternionAndMatrix[7] = x87f_store_f32(x87f_sub(x87f_load_f32(yz), x87f_load_f32(xw)));
        quaternionAndMatrix[8] = x87f_store_f32(x87f_sub(x87f_load_f32(1.0f), x87f_add(x87f_load_f32(yy), x87f_load_f32(xx))));
#else
        scale = (float)((long double)2.0f / lengthSquared);
        xx = (float)((long double)xSquared * scale);
        yy = (float)((long double)ySquared * scale);
        zz = (float)((long double)zSquared * scale);
        scaledX = (float)((long double)x * scale);
        scaledY = (float)((long double)y * scale);
        xy = (float)((long double)scaledX * y);
        xz = (float)((long double)scaledX * z);
        xw = (float)((long double)scaledX * w);
        yz = (float)((long double)scaledY * z);
        yw = (float)((long double)scaledY * w);
        zw = (float)((long double)z * w * scale);

        quaternionAndMatrix[0] = (float)((long double)1.0f - ((long double)zz + yy));
        quaternionAndMatrix[1] = (float)((long double)zw + xy);
        quaternionAndMatrix[2] = (float)((long double)xz - yw);
        quaternionAndMatrix[3] = (float)((long double)xy - zw);
        quaternionAndMatrix[4] = (float)((long double)1.0f - ((long double)zz + xx));
        quaternionAndMatrix[5] = (float)((long double)yz + xw);
        quaternionAndMatrix[6] = (float)((long double)yw + xz);
        quaternionAndMatrix[7] = (float)((long double)yz - xw);
        quaternionAndMatrix[8] = (float)((long double)1.0f - ((long double)yy + xx));
#endif
        return;
    }

    quaternionAndMatrix[0] = 1.0f;
    quaternionAndMatrix[1] = 0.0f;
    quaternionAndMatrix[2] = 0.0f;
    quaternionAndMatrix[3] = 0.0f;
    quaternionAndMatrix[4] = 1.0f;
    quaternionAndMatrix[5] = 0.0f;
    quaternionAndMatrix[6] = 0.0f;
    quaternionAndMatrix[7] = 0.0f;
    quaternionAndMatrix[8] = 1.0f;
}
#else
void ConvertQuatToMat(float quaternionAndMatrix[9])
{
    float xx;
    float yy;
    float zz;
    float scale;
    float xy;
    float xz;
    float xw;
    float yz;
    float yw;
    float zw;

#if EMULATE_X87
    xx = x87f_store_f32(x87f_mul(x87f_load_f32(quaternionAndMatrix[0]), x87f_load_f32(quaternionAndMatrix[0])));
    yy = x87f_store_f32(x87f_mul(x87f_load_f32(quaternionAndMatrix[1]), x87f_load_f32(quaternionAndMatrix[1])));
    zz = x87f_store_f32(x87f_mul(x87f_load_f32(quaternionAndMatrix[2]), x87f_load_f32(quaternionAndMatrix[2])));
    scale = x87f_store_f32(x87f_add(x87f_add(x87f_add(x87f_load_f32(xx), x87f_load_f32(yy)), x87f_load_f32(zz)),
                                    x87f_mul(x87f_load_f32(quaternionAndMatrix[3]), x87f_load_f32(quaternionAndMatrix[3]))));

    if (scale != 0.0f) {
        scale = x87f_store_f32(x87f_div(x87f_load_f32(2.0f), x87f_load_f32(scale)));
        xx = x87f_store_f32(x87f_mul(x87f_load_f32(xx), x87f_load_f32(scale)));
        yy = x87f_store_f32(x87f_mul(x87f_load_f32(yy), x87f_load_f32(scale)));
        zz = x87f_store_f32(x87f_mul(x87f_load_f32(zz), x87f_load_f32(scale)));

        quaternionAndMatrix[0] = x87f_store_f32(x87f_mul(x87f_load_f32(quaternionAndMatrix[0]), x87f_load_f32(scale)));
        xy = x87f_store_f32(x87f_mul(x87f_load_f32(quaternionAndMatrix[0]), x87f_load_f32(quaternionAndMatrix[1])));
        xz = x87f_store_f32(x87f_mul(x87f_load_f32(quaternionAndMatrix[0]), x87f_load_f32(quaternionAndMatrix[2])));
        xw = x87f_store_f32(x87f_mul(x87f_load_f32(quaternionAndMatrix[0]), x87f_load_f32(quaternionAndMatrix[3])));
        quaternionAndMatrix[1] = x87f_store_f32(x87f_mul(x87f_load_f32(quaternionAndMatrix[1]), x87f_load_f32(scale)));
        yz = x87f_store_f32(x87f_mul(x87f_load_f32(quaternionAndMatrix[1]), x87f_load_f32(quaternionAndMatrix[2])));
        yw = x87f_store_f32(x87f_mul(x87f_load_f32(quaternionAndMatrix[1]), x87f_load_f32(quaternionAndMatrix[3])));
        zw = x87f_store_f32(
            x87f_mul(x87f_mul(x87f_load_f32(quaternionAndMatrix[2]), x87f_load_f32(quaternionAndMatrix[3])), x87f_load_f32(scale)));

        quaternionAndMatrix[0] = x87f_store_f32(x87f_sub(x87f_load_f32(1.0f), x87f_add(x87f_load_f32(yy), x87f_load_f32(zz))));
        quaternionAndMatrix[1] = x87f_store_f32(x87f_add(x87f_load_f32(xy), x87f_load_f32(zw)));
        quaternionAndMatrix[2] = x87f_store_f32(x87f_sub(x87f_load_f32(xz), x87f_load_f32(yw)));
        quaternionAndMatrix[3] = x87f_store_f32(x87f_sub(x87f_load_f32(xy), x87f_load_f32(zw)));
        quaternionAndMatrix[4] = x87f_store_f32(x87f_sub(x87f_load_f32(1.0f), x87f_add(x87f_load_f32(xx), x87f_load_f32(zz))));
        quaternionAndMatrix[5] = x87f_store_f32(x87f_add(x87f_load_f32(yz), x87f_load_f32(xw)));
        quaternionAndMatrix[6] = x87f_store_f32(x87f_add(x87f_load_f32(xz), x87f_load_f32(yw)));
        quaternionAndMatrix[7] = x87f_store_f32(x87f_sub(x87f_load_f32(yz), x87f_load_f32(xw)));
        quaternionAndMatrix[8] = x87f_store_f32(x87f_sub(x87f_load_f32(1.0f), x87f_add(x87f_load_f32(xx), x87f_load_f32(yy))));
    } else {
#else
    xx = quaternionAndMatrix[0] * quaternionAndMatrix[0];
    yy = quaternionAndMatrix[1] * quaternionAndMatrix[1];
    zz = quaternionAndMatrix[2] * quaternionAndMatrix[2];
    scale = ((xx + yy) + zz) + quaternionAndMatrix[3] * quaternionAndMatrix[3];

    if (scale != 0.0f) {
        scale = 2.0f / scale;
        xx *= scale;
        yy *= scale;
        zz *= scale;
        quaternionAndMatrix[0] *= scale;
        xy = quaternionAndMatrix[0] * quaternionAndMatrix[1];
        xz = quaternionAndMatrix[0] * quaternionAndMatrix[2];
        xw = quaternionAndMatrix[0] * quaternionAndMatrix[3];
        quaternionAndMatrix[1] *= scale;
        yz = quaternionAndMatrix[1] * quaternionAndMatrix[2];
        yw = quaternionAndMatrix[1] * quaternionAndMatrix[3];
        zw = quaternionAndMatrix[2] * quaternionAndMatrix[3] * scale;

        quaternionAndMatrix[0] = 1.0f - (yy + zz);
        quaternionAndMatrix[1] = xy + zw;
        quaternionAndMatrix[2] = xz - yw;
        quaternionAndMatrix[3] = xy - zw;
        quaternionAndMatrix[4] = 1.0f - (xx + zz);
        quaternionAndMatrix[5] = yz + xw;
        quaternionAndMatrix[6] = xz + yw;
        quaternionAndMatrix[7] = yz - xw;
        quaternionAndMatrix[8] = 1.0f - (xx + yy);
    } else {
#endif
        quaternionAndMatrix[0] = 1.0f;
        quaternionAndMatrix[1] = 0.0f;
        quaternionAndMatrix[2] = 0.0f;
        quaternionAndMatrix[3] = 0.0f;
        quaternionAndMatrix[4] = 1.0f;
        quaternionAndMatrix[5] = 0.0f;
        quaternionAndMatrix[6] = 0.0f;
        quaternionAndMatrix[7] = 0.0f;
        quaternionAndMatrix[8] = 1.0f;
    }
}
#endif
