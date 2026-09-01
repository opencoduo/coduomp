#include "backend.h"

#include <float.h>

/* Source: CoDUOMP.exe 0x005181b0..0x00518217.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005181b0_00518218.mcode.
 * Name and signature: same-module Mac R_ModelBounds and renderer export slot
 * 41. Windows LTCG inlines R_GetModelByHandle. A non-brush model has no
 * bmodel_t and therefore reports an all-zero box. */
void R_ModelBounds(int32_t modelHandle, vec3_t mins, vec3_t maxs)
{
    model_t *model = R_GetModelByHandle(modelHandle);

    if (model->bmodel != NULL) {
        mins[0] = model->bmodel->bounds[0][0];
        mins[1] = model->bmodel->bounds[0][1];
        mins[2] = model->bmodel->bounds[0][2];
        maxs[0] = model->bmodel->bounds[1][0];
        maxs[1] = model->bmodel->bounds[1][1];
        maxs[2] = model->bmodel->bounds[1][2];
        return;
    }

    mins[0] = 0.0f;
    mins[1] = 0.0f;
    mins[2] = 0.0f;
    maxs[0] = 0.0f;
    maxs[1] = 0.0f;
    maxs[2] = 0.0f;
}

/* Source: CoDUOMP.exe 0x00517430..0x00517626.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00517430_00517627.mcode.
 * Name and normal four-argument source signature: same-module Mac symbol
 * R_GetXModelBounds. MSVC LTCG passed transform in EBX at the sole Windows
 * call site and inlined DObjBad, DObjGetMatrixArray, DObjGetSurface, and
 * XSurfaceGetNumVerts. The corresponding Mac function retains those source
 * calls and receives obj/transform/mins/maxs in r3/r4/r5/r6.
 *
 * The scalar expressions retain the Windows x87 multiply/add order. The
 * explicit comparisons also retain its unordered behavior: a NaN vertex
 * component changes neither bound. */
void R_GetXModelBounds(const DObj *obj, const axis_t transform,
                       vec3_t mins, vec3_t maxs)
{
    int32_t lodIndex = 0;
    uint32_t partBits[DOBJ_PART_BITSET_WORD_COUNT];
    vec3_t surfaceVertices[XMODEL_MAX_VERTICES];

    if (DObjBad(obj) != qfalse) {
        mins[0] = -FLT_MAX;
        mins[1] = -FLT_MAX;
        mins[2] = -FLT_MAX;
        maxs[0] = FLT_MAX;
        maxs[1] = FLT_MAX;
        maxs[2] = FLT_MAX;
        return;
    }

    mins[0] = FLT_MAX;
    mins[1] = FLT_MAX;
    mins[2] = FLT_MAX;
    maxs[0] = -FLT_MAX;
    maxs[1] = -FLT_MAX;
    maxs[2] = -FLT_MAX;

    int32_t surfaceCount = DObjGetNumSurfaces(obj, &lodIndex);
    dobj_surface_ref_t *surfaceRefs = CODUOMP_ALLOCA(
        (size_t)surfaceCount * sizeof(surfaceRefs[0]));
    DObjGetSurfaces(obj, surfaceRefs, partBits, &lodIndex);

    for (int32_t surfaceRefIndex = 0;
         surfaceRefIndex < surfaceCount; ++surfaceRefIndex) {
        const dobj_surface_ref_t *surfaceRef =
            &surfaceRefs[surfaceRefIndex];
        int32_t modelIndex = surfaceRef->modelIndex;
        const DObjSkelMat *matrixArray =
            &obj->evaluationStorage
                 ->partSpans[obj->modelPartBaseIndices[modelIndex]]
                 .basePose;
        XSurface *surface = DObjGetSurface(
            obj, modelIndex, surfaceRef->surfaceIndex, &lodIndex);

        XSurfaceGetVerts(surface, matrixArray, surfaceVertices, NULL, NULL);

        for (int32_t vertexIndex = 0;
             vertexIndex < surface->vertexCount; ++vertexIndex) {
            const vec3_t *vertex = &surfaceVertices[vertexIndex];
            const long double transformedRaw[3] = {
                ((long double)(*vertex)[0] * transform[0][0] +
                 (long double)(*vertex)[2] * transform[2][0]) +
                (long double)(*vertex)[1] * transform[1][0],
                ((long double)(*vertex)[2] * transform[2][1] +
                 (long double)(*vertex)[1] * transform[1][1]) +
                (long double)(*vertex)[0] * transform[0][1],
                ((long double)(*vertex)[2] * transform[2][2] +
                 (long double)(*vertex)[1] * transform[1][2]) +
                (long double)(*vertex)[0] * transform[0][2]
            };

            for (int32_t axisIndex = 0; axisIndex < 3; ++axisIndex) {
                if (transformedRaw[axisIndex] <
                    (long double)mins[axisIndex]) {
                    mins[axisIndex] = (float)transformedRaw[axisIndex];
                }
                if ((long double)maxs[axisIndex] <
                    transformedRaw[axisIndex]) {
                    maxs[axisIndex] = (float)transformedRaw[axisIndex];
                }
            }
        }
    }
}
