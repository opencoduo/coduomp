#include "animation_private.h"

static const char dobj_defaultSurfaceName[] = "DEFAULT";

/* Sources: CoDUOMP.exe 0x00494e20 and coduo_lnxded 0x080c6d12.
 * Name: same-version Mac symbol DObjGetNumSurfaces. */
int32_t DObjGetNumSurfaces(const DObj *obj, const int32_t *lodIndices)
{
    int32_t surfaceCount = 0;

    for (int32_t modelIndex = obj->modelCount - 1; modelIndex >= 0; --modelIndex) {
        int32_t lodIndex = lodIndices[modelIndex];

        if (lodIndex < 0) {
            continue;
        }

        const XModelSurfs *surfs = obj->models[modelIndex]->info->lodRecords[lodIndex].surfs;
        if (surfs != NULL) {
            surfaceCount += surfs->surfs->surfaceCount;
        }
    }
    return surfaceCount;
}

/* Sources: CoDUOMP.exe 0x00494e60 and coduo_lnxded 0x080c6d9c.
 * Name: same-version Mac symbol DObjGetSurface. */
XSurface *DObjGetSurface(const DObj *obj, int32_t modelIndex, int32_t surfaceIndex, const int32_t *lodIndices)
{
    const XModelSurfs *surfsEntry = obj->models[modelIndex]->info->lodRecords[lodIndices[modelIndex]].surfs;

    return surfsEntry->surfs->surfaces[surfaceIndex];
}

/* Sources: CoDUOMP.exe 0x00494e90 and coduo_lnxded 0x080c6de2.
 * The full-width model index matches both original address calculations;
 * the former client declaration's uint8_t parameter was too narrow.
 * Name: same-version Mac symbol DObjGetSurfaceName. */
const char *DObjGetSurfaceName(const DObj *obj, int32_t modelIndex, int32_t surfaceIndex, const int32_t *lodIndices)
{
    const XModelLodInfo *lod = &obj->models[modelIndex]->info->lodRecords[lodIndices[modelIndex]];
    uint16_t name = lod->surfaceNameTable[surfaceIndex];

    return name != 0 ? SL_ConvertToString(name) : dobj_defaultSurfaceName;
}

/* Sources: CoDUOMP.exe 0x00494ed0 and coduo_lnxded 0x080c6e48.
 * Name: same-version Mac symbol DObjGetSurfaces. */
void DObjGetSurfaces(const DObj *obj, dobj_surface_ref_t *surfaceRefs, uint32_t *partBits, const int32_t *lodIndices)
{
    Com_Memset(partBits, 0, DOBJ_PART_BITSET_WORD_COUNT * sizeof(*partBits));

    for (int32_t modelIndex = 0; modelIndex < obj->modelCount; ++modelIndex) {
        int32_t lodIndex = lodIndices[modelIndex];
        if (lodIndex < 0) {
            continue;
        }

        const XModelSurfs *surfsEntry = obj->models[modelIndex]->info->lodRecords[lodIndex].surfs;
        if (surfsEntry == NULL) {
            continue;
        }

        const XModelSurfsData *surfs = surfsEntry->surfs;
        int32_t modelPartCount = XModelNumBones(obj->models[modelIndex]);
        /* NOT_FROM_ORIGINAL_SOURCE: model loading and DObj construction prove
         * a positive part count before deriving the last mask word. */
        int32_t lastModelWord = (modelPartCount - 1) >> 5;
        int32_t modelPartBase = obj->modelPartBaseIndices[modelIndex];
        int32_t baseWord = modelPartBase >> 5;
        int32_t baseBit = modelPartBase & 31;

        for (int32_t surfaceIndex = 0; surfaceIndex < surfs->surfaceCount; ++surfaceIndex) {
            const uint32_t *surfaceBits = surfs->surfaces[surfaceIndex]->boneUsage;

            surfaceRefs->modelIndex = (int16_t)modelIndex;
            surfaceRefs->surfaceIndex = (int16_t)surfaceIndex;
            ++surfaceRefs;

            if (baseBit == 0) {
                for (int32_t wordIndex = 0; wordIndex <= lastModelWord; ++wordIndex) {
                    partBits[baseWord + wordIndex] |= surfaceBits[wordIndex];
                }
                continue;
            }

            int32_t rightShift = DOBJ_PART_BITSET_WORD_BITS - baseBit;
            partBits[baseWord] |= surfaceBits[0] << baseBit;

            for (int32_t wordIndex = 0; wordIndex < lastModelWord; ++wordIndex) {
                partBits[baseWord + wordIndex + 1] |= (surfaceBits[wordIndex + 1] << baseBit) | (surfaceBits[wordIndex] >> rightShift);
            }

            const int32_t carryWord = baseWord + lastModelWord + 1;
            /* NOT_FROM_ORIGINAL_SOURCE: retain every carry within the
             * parameterized DObj mask and omit only the proved-zero tail. */
            if ((uint32_t)carryWord < DOBJ_PART_BITSET_WORD_COUNT) {
                partBits[carryWord] |= surfaceBits[lastModelWord] >> rightShift;
            }
        }
    }
}
