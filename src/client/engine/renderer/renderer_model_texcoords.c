#include "backend.h"

#include "qcommon/hunk.h"
#include "compat/crt/qsort_compat.h"

#include <stdint.h>
#include <stdlib.h>

enum {
    R_MAX_XSURFACE_REMAPS = 65536
};

typedef struct xsurface_remap_s {
    /* Sort/search key: original surface first, then original shader. */
    shader_t *sourceShader;
    XSurface *sourceSurface;
    /* Clone when another record still owns the source; otherwise the source. */
    XSurface *remappedSurface;
} xsurface_remap_t;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(xsurface_remap_t) == 4,
               "i386 XSurface remap alignment changed");
_Static_assert(offsetof(xsurface_remap_t, sourceShader) == 0x00,
               "i386 XSurface remap source shader moved");
_Static_assert(offsetof(xsurface_remap_t, sourceSurface) == 0x04,
               "i386 XSurface remap source surface moved");
_Static_assert(offsetof(xsurface_remap_t, remappedSurface) == 0x08,
               "i386 XSurface remap result surface moved");
_Static_assert(sizeof(xsurface_remap_t) == 0x0c,
               "i386 XSurface remap size changed");
#endif

/* Source: CoDUOMP.exe 0x00517880..0x005178aa.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00517880_005178aa.mcode.
 * Name: exact same-module Mac symbol compare_xsurface_remap. The original
 * comparator returns the raw surface-address difference, then the shader-array
 * index difference (pointer difference divided by the 0x198 shader stride).
 * qsort consumes only the sign. Explicit address ordering retains that sign
 * without narrowing native 64-bit pointers into the original int32_t result. */
static int compare_xsurface_remap(const void *leftValue,
                                  const void *rightValue)
{
    const xsurface_remap_t *left = leftValue;
    const xsurface_remap_t *right = rightValue;
    uintptr_t leftSurface = (uintptr_t)left->sourceSurface;
    uintptr_t rightSurface = (uintptr_t)right->sourceSurface;
    uintptr_t leftShader;
    uintptr_t rightShader;

    if (leftSurface < rightSurface)
        return -1;
    if (leftSurface > rightSurface)
        return 1;

    leftShader = (uintptr_t)left->sourceShader;
    rightShader = (uintptr_t)right->sourceShader;
    if (leftShader < rightShader)
        return -1;
    if (leftShader > rightShader)
        return 1;
    return 0;
}

/* Source: CoDUOMP.exe 0x005178b0..0x00517c3b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005178b0_00517c3b.mcode.
 * Name and source-level helper calls: exact same-module Mac symbol
 * R_FixupXModelTexCoords. Both binaries prove the two model walks, unique
 * (shader,surface) records, address ordering, shared-surface cloning, texture-
 * sheet UV remapping, remapped shader handles, and final surface replacement. */
void R_FixupXModelTexCoords(void)
{
    /* The original reserves one extra record. Its clone-selection loop reads
     * that following slot for the final initialized record. The 65537-record
     * extent plus locals exactly accounts for the Windows 0x0c0028-byte frame. */
    xsurface_remap_t remaps[R_MAX_XSURFACE_REMAPS + 1];
    int32_t remapCount = 0;

    for (int32_t modelIndex = 1; modelIndex < tr.modelCount; ++modelIndex) {
        model_t *model = tr.models[modelIndex];
        XSurface **surfaces;
        int32_t lodCount;

        if (model->xmodel == NULL)
            continue;

        lodCount = XModelGetNumLods(model->xmodel);
        for (int32_t lodIndex = 0; lodIndex < lodCount; ++lodIndex) {
            int32_t surfaceCount =
                XModelGetSurfaces(model->xmodel, &surfaces, lodIndex);

            for (int32_t surfaceIndex = 0;
                 surfaceIndex < surfaceCount; ++surfaceIndex) {
                uint16_t shaderHandle =
                    model->shaderHandles[lodIndex][surfaceIndex];
                shader_t *sourceShader;
                int32_t remapIndex;

                if (shaderHandle == 0)
                    continue;

                sourceShader = tr.shaders[shaderHandle];
                if ((sourceShader->flags & SHADER_FLAG_REMAPPED) == 0)
                    sourceShader = NULL;

                if (remapCount == R_MAX_XSURFACE_REMAPS) {
                    ri.Error(
                        ERR_DROP,
                        "\x15More than %i xmodel surfaces need to be remapped\n"
                        "You may need to set r_optimizeTextures temporarily to 0\n"
                        "Eventually you want to use fewer unique model surfaces\n",
                        remapCount);
                }

                for (remapIndex = 0; remapIndex < remapCount;
                     ++remapIndex) {
                    if (remaps[remapIndex].sourceSurface ==
                            surfaces[surfaceIndex] &&
                        remaps[remapIndex].sourceShader == sourceShader) {
                        break;
                    }
                }
                if (remapIndex != remapCount)
                    continue;

                remaps[remapCount].sourceShader = sourceShader;
                remaps[remapCount].sourceSurface = surfaces[surfaceIndex];
                remaps[remapCount].remappedSurface = NULL;
                ++remapCount;
            }
        }
    }

    if (remapCount == 0)
        return;

    coduo_crt_qsort(remaps, (size_t)remapCount, sizeof(remaps[0]),
                        compare_xsurface_remap);

    /* Determinized sentinel: NULL never equals a real surface pointer, so
     * the final record deterministically takes the keep-original branch.
     * See the ORIGINAL_BINARY_BUG note below. */
    remaps[remapCount].sourceSurface = NULL;

    for (int32_t remapIndex = 0;
         remapIndex < remapCount; ++remapIndex) {
        xsurface_remap_t *remap = &remaps[remapIndex];

        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (remapIndex < remapCount &&
            remaps[remapIndex + 1].sourceSurface == remap->sourceSurface) {
            remap->remappedSurface =
                XSurfaceCloneSurface(remap->sourceSurface, Hunk_AllocInternal);
        } else {
            remap->remappedSurface = remap->sourceSurface;
        }

        if (remap->sourceShader != NULL) {
            vec2_t scale;
            vec2_t offset;
            int32_t sourceUIndex;
            int32_t sourceVIndex;

            R_SetupTextureCoordinateRemap(
                remap->sourceShader, scale, offset,
                &sourceUIndex, &sourceVIndex);
            XSurfaceRemapTextureCoordinates(
                remap->remappedSurface, scale, offset,
                sourceUIndex, sourceVIndex);
        }
    }

    for (int32_t modelIndex = 1; modelIndex < tr.modelCount; ++modelIndex) {
        model_t *model = tr.models[modelIndex];
        XSurface **surfaces;
        int32_t lodCount;

        if (model->xmodel == NULL)
            continue;

        lodCount = XModelGetNumLods(model->xmodel);
        for (int32_t lodIndex = 0; lodIndex < lodCount; ++lodIndex) {
            int32_t surfaceCount =
                XModelGetSurfaces(model->xmodel, &surfaces, lodIndex);

            for (int32_t surfaceIndex = 0;
                 surfaceIndex < surfaceCount; ++surfaceIndex) {
                uint16_t *shaderHandle =
                    &model->shaderHandles[lodIndex][surfaceIndex];
                shader_t *sourceShader;

                if (*shaderHandle == 0)
                    continue;

                sourceShader = tr.shaders[*shaderHandle];
                if ((sourceShader->flags & SHADER_FLAG_REMAPPED) != 0) {
                    *shaderHandle =
                        (uint16_t)sourceShader->remappedShader->index;
                } else {
                    sourceShader = NULL;
                }

                for (int32_t remapIndex = 0;
                     remapIndex < remapCount; ++remapIndex) {
                    if (remaps[remapIndex].sourceShader == sourceShader &&
                        remaps[remapIndex].sourceSurface ==
                            surfaces[surfaceIndex]) {
                        surfaces[surfaceIndex] =
                            remaps[remapIndex].remappedSurface;
                        break;
                    }
                }
            }
        }
    }
}
