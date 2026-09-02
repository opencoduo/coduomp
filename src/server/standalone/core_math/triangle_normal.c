#include "core_math_private.h"

void TriangleNormal(const vec3_t point, const vec3_t edgePoint0,
                  const vec3_t edgePoint1, vec3_t out)
{
    vec3_t edge0;
    vec3_t edge1;

    edge0[0] = point[0] - edgePoint0[0];
    edge0[1] = point[1] - edgePoint0[1];
    edge0[2] = point[2] - edgePoint0[2];
    VectorNormalize(edge0);

    edge1[0] = point[0] - edgePoint1[0];
    edge1[1] = point[1] - edgePoint1[1];
    edge1[2] = point[2] - edgePoint1[2];
    VectorNormalize(edge1);

    CrossProduct(edge0, edge1, out);
    VectorNormalize(out);
}
