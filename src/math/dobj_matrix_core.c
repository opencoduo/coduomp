#include "q_math.h"

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

/*
 * Common DObj quaternion/matrix core.
 *
 * The Windows client bodies are at CoDUOMP.exe 0x004938c0, 0x00493b70,
 * 0x00493bd0, 0x00493c60, 0x004951a0, and 0x004951f0.  The corresponding
 * Linux dedicated bodies are at coduo_lnxded 0x080c7d82, 0x080c7f9a,
 * 0x080c8056, 0x080c818e, 0x080c723e, and 0x080c72ea.  Both families expose
 * the same names, signatures, layouts, aliasing behavior, and mathematical
 * transformations.  They differ in compiler-selected x87 fold and spill
 * order, so the complete platform bodies remain separate below.
 */

#if defined(WINDOWS_BEHAVIOR)

void DObjQuatToMatrix43(const vec4_t quat, DObjSkelMat *matrix)
{
    float q0Sq = quat[0] * quat[0];
    float q1Sq = quat[1] * quat[1];
    float q2Sq = quat[2] * quat[2];
    float lengthSq = ((quat[3] * quat[3] + q2Sq) + q1Sq) + q0Sq;

    if (lengthSq != 0.0f) {
        float scale = 2.0f / lengthSq;
        float xx = q0Sq * scale;
        float yy = q1Sq * scale;
        float zz = q2Sq * scale;
        float xScale = quat[0] * scale;
        float xy = xScale * quat[1];
        float xz = xScale * quat[2];
        float xw = xScale * quat[3];
        float yScale = quat[1] * scale;
        float yz = yScale * quat[2];
        float yw = yScale * quat[3];
        float zw = (quat[2] * quat[3]) * scale;

        matrix->axis[0][0] = 1.0f - (yy + zz);
        matrix->axis[0][1] = xy + zw;
        matrix->axis[0][2] = xz - yw;
        matrix->axis[0][3] = 0.0f;
        matrix->axis[1][0] = xy - zw;
        matrix->axis[1][1] = 1.0f - (xx + zz);
        matrix->axis[1][2] = yz + xw;
        matrix->axis[1][3] = 0.0f;
        matrix->axis[2][0] = xz + yw;
        matrix->axis[2][1] = yz - xw;
        matrix->axis[2][2] = 1.0f - (xx + yy);
        matrix->axis[2][3] = 0.0f;
    } else {
        matrix->axis[0][0] = 1.0f;
        matrix->axis[0][1] = 0.0f;
        matrix->axis[0][2] = 0.0f;
        matrix->axis[0][3] = 0.0f;
        matrix->axis[1][0] = 0.0f;
        matrix->axis[1][1] = 1.0f;
        matrix->axis[1][2] = 0.0f;
        matrix->axis[1][3] = 0.0f;
        matrix->axis[2][0] = 0.0f;
        matrix->axis[2][1] = 0.0f;
        matrix->axis[2][2] = 1.0f;
        matrix->axis[2][3] = 0.0f;
    }
}

void DObjMatrixTransformVector43InPlace(vec3_t point, const DObjSkelMat *matrix)
{
    float x = point[0];
    float y = point[1];
    float z = point[2];

    float outX = ((z * matrix->axis[2][0] + y * matrix->axis[1][0]) + x * matrix->axis[0][0]) + matrix->origin[0];
    float outY = ((z * matrix->axis[2][1] + y * matrix->axis[1][1]) + x * matrix->axis[0][1]) + matrix->origin[1];
    float outZ = ((z * matrix->axis[2][2] + y * matrix->axis[1][2]) + x * matrix->axis[0][2]) + matrix->origin[2];

    point[2] = outZ;
    point[0] = outX;
    point[1] = outY;
}

void DObjMatrixTransformVector43(const vec3_t point, const DObjSkelMat *matrix, vec3_t out)
{
    out[0] = ((point[2] * matrix->axis[2][0] + point[1] * matrix->axis[1][0]) + point[0] * matrix->axis[0][0]) + matrix->origin[0];
    out[1] = ((point[0] * matrix->axis[0][1] + point[2] * matrix->axis[2][1]) + point[1] * matrix->axis[1][1]) + matrix->origin[1];
    out[2] = ((point[0] * matrix->axis[0][2] + point[2] * matrix->axis[2][2]) + point[1] * matrix->axis[1][2]) + matrix->origin[2];
}

void DObjMatrixInverseTransformVector43(const vec3_t point, const DObjSkelMat *matrix, vec3_t out)
{
    const long double x = (long double)point[0] - (long double)matrix->origin[0];
    const long double y = (long double)point[1] - (long double)matrix->origin[1];
    const long double z = (long double)point[2] - (long double)matrix->origin[2];

    out[0] = (float)((z * (long double)matrix->axis[0][2] + y * (long double)matrix->axis[0][1]) + x * (long double)matrix->axis[0][0]);
    out[1] = (float)((z * (long double)matrix->axis[1][2] + y * (long double)matrix->axis[1][1]) + x * (long double)matrix->axis[1][0]);
    out[2] = (float)((z * (long double)matrix->axis[2][2] + y * (long double)matrix->axis[2][1]) + x * (long double)matrix->axis[2][0]);
}

void DObjQuatMultiplyIntoFirst(vec4_t quat, const vec4_t rhs)
{
    float rhsW = rhs[3];
    float quatW = quat[3];
    float rhsX = rhs[0];
    float rhsY = rhs[1];
    float rhsZ = rhs[2];
    float quatX = quat[0];
    float quatY = quat[1];
    float quatZ = quat[2];

    float outX = ((quatX * rhsW + quatW * rhsX) + quatZ * rhsY) - quatY * rhsZ;
    float outY = ((quatY * rhsW - quatZ * rhsX) + quatW * rhsY) + quatX * rhsZ;
    float outZ = ((quatZ * rhsW + quatY * rhsX) - quatX * rhsY) + quatW * rhsZ;

    quat[3] = ((quatW * rhsW - quatX * rhsX) - quatY * rhsY) - quatZ * rhsZ;
    quat[0] = outX;
    quat[1] = outY;
    quat[2] = outZ;
}

void DObjQuatMultiplyIntoSecond(const vec4_t lhs, vec4_t quat)
{
    float lhsX = lhs[0];
    float rhsW = quat[3];
    float lhsW = lhs[3];
    float lhsZ = lhs[2];
    float lhsY = lhs[1];
    float rhsX = quat[0];
    float rhsY = quat[1];
    float rhsZ = quat[2];

    float outX = ((lhsX * rhsW + lhsW * rhsX) + lhsZ * rhsY) - lhsY * rhsZ;
    float outY = ((lhsY * rhsW - lhsZ * rhsX) + lhsW * rhsY) + lhsX * rhsZ;
    float outZ = ((lhsZ * rhsW + lhsY * rhsX) - lhsX * rhsY) + lhsW * rhsZ;

    quat[3] = ((lhsW * rhsW - lhsX * rhsX) - lhsY * rhsY) - lhsZ * rhsZ;
    quat[0] = outX;
    quat[1] = outY;
    quat[2] = outZ;
}

#else

void DObjMatrixTransformVector43(const vec3_t in, const DObjSkelMat *matrix, vec3_t out)
{
    out[0] = in[0] * matrix->axis[0][0] + in[1] * matrix->axis[1][0] + in[2] * matrix->axis[2][0] + matrix->origin[0];
    out[1] = in[0] * matrix->axis[0][1] + in[1] * matrix->axis[1][1] + in[2] * matrix->axis[2][1] + matrix->origin[1];
    out[2] = in[0] * matrix->axis[0][2] + in[1] * matrix->axis[1][2] + in[2] * matrix->axis[2][2] + matrix->origin[2];
}

void DObjMatrixInverseTransformVector43(const vec3_t in, const DObjSkelMat *matrix, vec3_t out)
{
    float translated0 = in[0] - matrix->origin[0];
    float translated1 = in[1] - matrix->origin[1];
    float translated2 = in[2] - matrix->origin[2];

    out[0] = translated0 * matrix->axis[0][0] + translated1 * matrix->axis[0][1] + translated2 * matrix->axis[0][2];
    out[1] = translated0 * matrix->axis[1][0] + translated1 * matrix->axis[1][1] + translated2 * matrix->axis[1][2];
    out[2] = translated0 * matrix->axis[2][0] + translated1 * matrix->axis[2][1] + translated2 * matrix->axis[2][2];
}

void DObjQuatToMatrix43(const vec4_t quat, DObjSkelMat *matrix)
{
    float q0Sq = quat[0] * quat[0];
    float q1Sq = quat[1] * quat[1];
    float q2Sq = quat[2] * quat[2];
    float lengthSq = ((q0Sq + q1Sq) + q2Sq) + quat[3] * quat[3];

    if (lengthSq != 0.0f) {
        float scale = 2.0f / lengthSq;
        float xx = q0Sq * scale;
        float yy = q1Sq * scale;
        float zz = q2Sq * scale;
        float xScale = quat[0] * scale;
        float xy = xScale * quat[1];
        float xz = xScale * quat[2];
        float xw = xScale * quat[3];
        float yScale = quat[1] * scale;
        float yz = yScale * quat[2];
        float yw = yScale * quat[3];
        float zw = (quat[2] * quat[3]) * scale;

        matrix->axis[0][0] = 1.0f - (yy + zz);
        matrix->axis[0][1] = xy + zw;
        matrix->axis[0][2] = xz - yw;
        matrix->axis[0][3] = 0.0f;
        matrix->axis[1][0] = xy - zw;
        matrix->axis[1][1] = 1.0f - (xx + zz);
        matrix->axis[1][2] = yz + xw;
        matrix->axis[1][3] = 0.0f;
        matrix->axis[2][0] = xz + yw;
        matrix->axis[2][1] = yz - xw;
        matrix->axis[2][2] = 1.0f - (xx + yy);
        matrix->axis[2][3] = 0.0f;
    } else {
        matrix->axis[0][0] = 1.0f;
        matrix->axis[0][1] = 0.0f;
        matrix->axis[0][2] = 0.0f;
        matrix->axis[0][3] = 0.0f;
        matrix->axis[1][0] = 0.0f;
        matrix->axis[1][1] = 1.0f;
        matrix->axis[1][2] = 0.0f;
        matrix->axis[1][3] = 0.0f;
        matrix->axis[2][0] = 0.0f;
        matrix->axis[2][1] = 0.0f;
        matrix->axis[2][2] = 1.0f;
        matrix->axis[2][3] = 0.0f;
    }
}

void DObjMatrixTransformVector43InPlace(vec3_t point, const DObjSkelMat *matrix)
{
    float x = point[0];
    float y = point[1];
    float z = point[2];
    float outX = x * matrix->axis[0][0] + y * matrix->axis[1][0] + z * matrix->axis[2][0] + matrix->origin[0];
    float outY = x * matrix->axis[0][1] + y * matrix->axis[1][1] + z * matrix->axis[2][1] + matrix->origin[1];

    point[2] = x * matrix->axis[0][2] + y * matrix->axis[1][2] + z * matrix->axis[2][2] + matrix->origin[2];
    point[0] = outX;
    point[1] = outY;
}

void DObjQuatMultiplyIntoFirst(vec4_t quat, const vec4_t rhs)
{
    float rhsW = rhs[3];
    float quatW = quat[3];
    float rhsX = rhs[0];
    float rhsY = rhs[1];
    float rhsZ = rhs[2];
    float quatX = quat[0];
    float quatY = quat[1];
    float quatZ = quat[2];

    float outX = ((quatX * rhsW + quatW * rhsX) + quatZ * rhsY) - quatY * rhsZ;
    float outY = ((quatY * rhsW - quatZ * rhsX) + quatW * rhsY) + quatX * rhsZ;
    float outZ = ((quatZ * rhsW + quatY * rhsX) - quatX * rhsY) + quatW * rhsZ;

    quat[3] = ((quatW * rhsW - quatX * rhsX) - quatY * rhsY) - quatZ * rhsZ;
    quat[0] = outX;
    quat[1] = outY;
    quat[2] = outZ;
}

void DObjQuatMultiplyIntoSecond(const vec4_t lhs, vec4_t quat)
{
    float lhsX = lhs[0];
    float rhsW = quat[3];
    float lhsW = lhs[3];
    float lhsZ = lhs[2];
    float lhsY = lhs[1];
    float rhsX = quat[0];
    float rhsY = quat[1];
    float rhsZ = quat[2];

    float outX = ((lhsX * rhsW + lhsW * rhsX) + lhsZ * rhsY) - lhsY * rhsZ;
    float outY = ((lhsY * rhsW - lhsZ * rhsX) + lhsW * rhsY) + lhsX * rhsZ;
    float outZ = ((lhsZ * rhsW + lhsY * rhsX) - lhsX * rhsY) + lhsW * rhsZ;

    quat[3] = ((lhsW * rhsW - lhsX * rhsX) - lhsY * rhsY) - lhsZ * rhsZ;
    quat[0] = outX;
    quat[1] = outY;
    quat[2] = outZ;
}

#endif
