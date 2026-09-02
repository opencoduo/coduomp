#include "backend.h"

#include "../math/vector_math.h"
#include "gl_state.h"

#include <math.h>
#include <stdint.h>

#define R_LIGHT_PRIORITY_SENTINEL \
    9999999980506447872.0f /* 0x5f0ac723 */
#define R_MERGE_LIGHT_DIRECTIONAL_DIFFUSE_SCALE \
    0.800000011920928955078125f /* 0x3f4ccccd */
#define R_MERGE_LIGHT_AMBIENT_SPLIT 0.5f /* 0x3f000000 */
#define R_MERGED_LIGHT_SPOT_CUTOFF 180.0f /* 0x43340000 */

enum {
    R_LIGHT_GRID_CORNER_COUNT = 8,
    R_LIGHT_GRID_XY_SHIFT = 5,
    R_LIGHT_GRID_Z_SHIFT = 6,
    R_LIGHT_GRID_XY_BIAS = 4096,
    R_LIGHT_GRID_Z_BIAS = 2048,
    R_LIGHT_GRID_XY_SIZE_INT = 32,
    R_LIGHT_GRID_Z_SIZE_INT = 64
};

#define R_LIGHT_GRID_WORLD_MIN -131072.0f /* 0xc8000000 */
#define R_LIGHT_GRID_WORLD_OFFSET 131072.0f /* 0x48000000 */
#define R_LIGHT_GRID_XY_SIZE 32.0f /* 0x42000000 */
#define R_LIGHT_GRID_Z_SIZE 64.0f /* 0x42800000 */
#define R_LIGHT_GRID_XY_SCALE 0.03125f /* 0x3d000000 */
#define R_LIGHT_GRID_Z_SCALE 0.015625f /* 0x3c800000 */
#define R_LIGHT_GRID_INTERPOLATION_MINIMUM \
    0.980000019073486328125f /* 0x3f7ae148 */
#define R_LIGHT_GRID_DEBUG_HALF_SIZE 1.0f /* 0x3f800000 */
#define R_LIGHT_GRID_DIRECTION_LENGTH 32768.0f /* 0x47000000 */
#define R_DIFFUSE_SUN_MINIMUM_CONTRIBUTION 0.25f /* 0x3e800000 */
#define R_SHOW_LEAF_LIGHT_VIEW_DOT_MINIMUM \
    0.949999988079071044921875f /* 0x3f733333 */
#define R_DYNAMIC_LIGHT_RADIUS_SQUARED_SCALE 4.0f /* 0x40800000 */
#define R_DIFFUSE_SUN_AMBIENT_HIGH 0.75f /* 0x3f400000 */
#define R_DIFFUSE_SUN_AMBIENT_LOW 0.25f /* 0x3e800000 */
#define R_DIFFUSE_SUN_OPPOSING_SCALE -0.25f /* 0xbe800000 */
#define R_DIFFUSE_SUN_FALLBACK_SCALE 0.5f /* 0x3f000000 */
#define R_LIGHT_DIRECTION_UP 1.0f
#define R_LIGHT_DIRECTION_DOWN -1.0f

static const vec4_t lightGridColorRed =
    {1.0f, 0.0f, 0.0f, 1.0f};
static const vec4_t lightGridColorGreen =
    {0.0f, 1.0f, 0.0f, 1.0f};
static const vec4_t lightGridColorBlue =
    {0.0f, 0.0f, 1.0f, 1.0f};
static const vec4_t lightGridColorYellow =
    {1.0f, 1.0f, 0.0f, 1.0f};
static const vec4_t lightGridColorWhite =
    {1.0f, 1.0f, 1.0f, 1.0f};

/* Source: CoDUOMP.exe 0x004c63d0..0x004c6474.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c63d0_004c6475.mcode.
 * Name and parameter roles: same-module Mac symbol R_MaxLightIntensity.
 * Windows proves the overbright scale, directional-light shortcut, squared
 * point distance, and constant/linear/quadratic attenuation denominator. */
float R_MaxLightIntensity(const vec3_t point,
                          const renderer_light_t *light)
{
    float maximumIntensity = light->intensity;

    if (tr.overbrightBits != 0)
        maximumIntensity *= (float)(1 << tr.overbrightBits);

    if (light->position[3] == 0.0f)
        return maximumIntensity;

    if (light->linearAttenuation != 0.0f ||
        light->quadraticAttenuation != 0.0f) {
        const float differenceX = light->position[0] - point[0];
        const float differenceY = light->position[1] - point[1];
        const float differenceZ = light->position[2] - point[2];
        const float distanceSquared =
            differenceZ * differenceZ +
            differenceY * differenceY +
            differenceX * differenceX;
        float attenuation = light->constantAttenuation +
            distanceSquared * light->quadraticAttenuation;

        if (light->linearAttenuation != 0.0f) {
            attenuation +=
                sqrtf(distanceSquared) * light->linearAttenuation;
        }
        return maximumIntensity / attenuation;
    }

    return maximumIntensity / light->constantAttenuation;
}

/* Source: CoDUOMP.exe 0x004c6480..0x004c6835.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c6480_004c6836.mcode.
 * Name and parameter roles: same-module Mac symbol R_MergeLights. Windows
 * proves the contribution cutoff walk, all eight entity-light slots, weighted
 * direction merge, ambient/diffuse split, generated-light fields, and final
 * replacement entry. The test of mergedLight->type in the diffuse gate is
 * also present in the Mac build; it is preserved even though testing the
 * current source light there would look more conventional. */
void R_MergeLights(const vec3_t point, trRefEntity_t *entity,
                   float *contributions,
                   const int32_t *sortedIndices)
{
    renderer_light_t *mergedLight;
    vec3_t lightDirections[R_MAX_ENTITY_LIGHTS];
    vec3_t mergedDirection = {0.0f, 0.0f, 0.0f};
    int32_t retainedSentinelLights;
    int32_t mergeIndex;
    float cutoffContribution;

    if (entity->lightCount <= tr.maxEntityLights)
        return;

    retainedSentinelLights =
        contributions[sortedIndices[0]] ==
                R_LIGHT_PRIORITY_SENTINEL
            ? 1
            : 0;
    mergeIndex = retainedSentinelLights + 1;
    cutoffContribution =
        contributions[sortedIndices[retainedSentinelLights]] *
        r_entLightCutoff->value;

    while (mergeIndex < tr.maxEntityLights - 1) {
        /* A NaN continues the original x87 scan rather than satisfying the
         * less-than-or-equal break condition. */
        if (contributions[sortedIndices[mergeIndex]] <=
            cutoffContribution) {
            break;
        }
        ++mergeIndex;
    }

    for (int32_t lightIndex = mergeIndex;
         lightIndex < entity->lightCount;
         ++lightIndex) {
        const renderer_light_t *light = entity->lights[lightIndex].light;
        vec3_t *direction = &lightDirections[lightIndex];
        float contribution;

        if (light->position[3] == 0.0f) {
            (*direction)[0] = light->position[0];
            (*direction)[1] = light->position[1];
            (*direction)[2] = light->position[2];

            if (light->type == R_LIGHT_TYPE_SUN ||
                light->type == R_LIGHT_TYPE_DIFFUSE_SUN) {
                contributions[sortedIndices[lightIndex]] -=
                    (float)(1 << tr.overbrightBits) *
                    tr.world->entitySunLightIntensity *
                    entity->diffuseSunContribution;
            }
        } else {
            (*direction)[0] = light->position[0] - point[0];
            (*direction)[1] = light->position[1] - point[1];
            (*direction)[2] = light->position[2] - point[2];
            (void)VectorNormalize(*direction);
        }

        contribution = contributions[sortedIndices[lightIndex]];
        mergedDirection[0] += contribution * (*direction)[0];
        mergedDirection[1] += contribution * (*direction)[1];
        mergedDirection[2] += contribution * (*direction)[2];
    }
    (void)VectorNormalize(mergedDirection);

    mergedLight = &entity->generatedLights[0];
    for (int32_t component = 0; component < 3; ++component) {
        mergedLight->ambient[component] = 0.0f;
        mergedLight->diffuse[component] = 0.0f;
        mergedLight->specular[component] = 0.0f;
    }

    for (int32_t lightIndex = mergeIndex;
         lightIndex < entity->lightCount;
         ++lightIndex) {
        const renderer_light_t *light = entity->lights[lightIndex].light;
        const vec3_t *direction = &lightDirections[lightIndex];
        const float contribution =
            contributions[sortedIndices[lightIndex]];
        float directionDot =
            mergedDirection[0] * (*direction)[0] +
            mergedDirection[2] * (*direction)[2] +
            mergedDirection[1] * (*direction)[1];
        float ambientFactor;

        if (directionDot <= 0.0f) {
            ambientFactor = R_MERGE_LIGHT_AMBIENT_SPLIT;
        } else {
            float diffuseContribution;

            /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
            if (light->type != R_LIGHT_TYPE_SUN &&
                mergedLight->type != R_LIGHT_TYPE_DIFFUSE_SUN) {
                directionDot *= R_MERGE_LIGHT_DIRECTIONAL_DIFFUSE_SCALE;
            }
            diffuseContribution =
                tr.identityLight * directionDot * contribution;
            mergedLight->diffuse[0] +=
                diffuseContribution * light->color[0];
            mergedLight->diffuse[1] +=
                diffuseContribution * light->color[1];
            mergedLight->diffuse[2] +=
                diffuseContribution * light->color[2];
            ambientFactor =
                R_MERGE_LIGHT_AMBIENT_SPLIT * (1.0f - directionDot);
        }

        {
            const float ambientContribution =
                tr.identityLight * ambientFactor * contribution;

            mergedLight->ambient[0] +=
                ambientContribution * light->color[0];
            mergedLight->ambient[1] +=
                ambientContribution * light->color[1];
            mergedLight->ambient[2] +=
                ambientContribution * light->color[2];
        }
    }

    mergedLight->position[0] = mergedDirection[0];
    mergedLight->position[1] = mergedDirection[1];
    mergedLight->position[2] = mergedDirection[2];
    mergedLight->ambient[3] = 1.0f;
    mergedLight->diffuse[3] = 1.0f;
    mergedLight->specular[3] = 1.0f;
    mergedLight->constantAttenuation = 1.0f;
    mergedLight->linearAttenuation = 0.0f;
    mergedLight->quadraticAttenuation = 0.0f;
    mergedLight->spotCutoff = R_MERGED_LIGHT_SPOT_CUTOFF;

    entity->lightCount = mergeIndex + 1;
    entity->lights[mergeIndex].light = mergedLight;
    entity->lights[mergeIndex].scale = 1.0f;
}

/* Source: CoDUOMP.exe 0x004c6840..0x004c7043.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c6840_004c7044.mcode.
 * Name and parameter roles: same-module Mac symbol
 * R_GetStaticLightContributions. Windows proves the leaf light list, all
 * eight cache-corner coordinates and weights, visibility-bit accumulation,
 * diffuse-sun byte scaling, incomplete-cell normalization, sun-light special
 * handling, and every developer debug primitive below. */
int32_t R_GetStaticLightContributions(
    const vec3_t point, float *contributions,
    float *diffuseSunContribution, renderer_light_t **lights)
{
    mnode_t *leaf;
    int32_t lightCount = 0;
    int32_t gridBase[3];
    float gridWeights[3][2];
    float totalWeight = 0.0f;

    *diffuseSunContribution = 0.0f;
    if (tr.world->nodes == NULL)
        return 0;

    leaf = R_PointInLeaf(point);
    if (leaf->data.leaf.cluster < 0) {
        if (tr.world->sunLight == NULL)
            return 0;

        lights[0] = tr.world->sunLight;
        contributions[0] = 1.0f;
        *diffuseSunContribution = 1.0f;
        return 1;
    }

    if (leaf->data.leaf.lightCount == 0 &&
        leaf->data.leaf.hasSunLight == qfalse) {
        return 0;
    }

    for (int32_t leafLightIndex = 0;
         leafLightIndex < leaf->data.leaf.lightCount;
         ++leafLightIndex) {
        const int32_t worldLightIndex = tr.world->lightIndexes[
            leaf->data.leaf.firstLightIndex + leafLightIndex];

        lights[lightCount] = &tr.world->lights[worldLightIndex];
        contributions[lightCount] = 0.0f;
        ++lightCount;
    }

    gridBase[0] = FastFloor(point[0] - R_LIGHT_GRID_WORLD_MIN) >>
                  R_LIGHT_GRID_XY_SHIFT;
    gridBase[1] = FastFloor(point[1] - R_LIGHT_GRID_WORLD_MIN) >>
                  R_LIGHT_GRID_XY_SHIFT;
    gridBase[2] = FastFloor(point[2] - R_LIGHT_GRID_WORLD_MIN) >>
                  R_LIGHT_GRID_Z_SHIFT;

    gridWeights[0][1] =
        (point[0] - R_LIGHT_GRID_WORLD_MIN) * R_LIGHT_GRID_XY_SCALE -
        (float)gridBase[0];
    gridWeights[0][0] = 1.0f - gridWeights[0][1];
    gridWeights[1][1] =
        (point[1] - R_LIGHT_GRID_WORLD_MIN) * R_LIGHT_GRID_XY_SCALE -
        (float)gridBase[1];
    gridWeights[1][0] = 1.0f - gridWeights[1][1];
    gridWeights[2][1] =
        (point[2] - R_LIGHT_GRID_WORLD_MIN) * R_LIGHT_GRID_Z_SCALE -
        (float)gridBase[2];
    gridWeights[2][0] = 1.0f - gridWeights[2][1];

    {
        const int32_t showMode = r_showLeafLights->integer;
        qboolean drawGridCell =
            showMode == R_SHOW_LEAF_LIGHTS_GRID ||
            showMode == R_SHOW_LEAF_LIGHTS_VIEW_CONE ||
            showMode < 0
                ? qtrue
                : qfalse;

        if (showMode == R_SHOW_LEAF_LIGHTS_VIEW_CONE) {
            vec3_t viewDirection;
            float viewDot;

            viewDirection[0] = point[0] - tr.refdef.vieworg[0];
            viewDirection[1] = point[1] - tr.refdef.vieworg[1];
            viewDirection[2] = point[2] - tr.refdef.vieworg[2];
            VectorNormalizeFast(viewDirection);
            viewDot = tr.refdef.viewaxis[0][2] * viewDirection[2] +
                      tr.refdef.viewaxis[0][1] * viewDirection[1] +
                      tr.refdef.viewaxis[0][0] * viewDirection[0];
            if (viewDot < R_SHOW_LEAF_LIGHT_VIEW_DOT_MINIMUM)
                drawGridCell = qfalse;
        }

        if (drawGridCell != qfalse) {
            vec3_t gridMins;
            vec3_t gridMaxs;

            gridMins[0] = (float)(
                (gridBase[0] - R_LIGHT_GRID_XY_BIAS) *
                R_LIGHT_GRID_XY_SIZE_INT);
            gridMins[1] = (float)(
                (gridBase[1] - R_LIGHT_GRID_XY_BIAS) *
                R_LIGHT_GRID_XY_SIZE_INT);
            gridMins[2] = (float)(
                (gridBase[2] - R_LIGHT_GRID_Z_BIAS) *
                R_LIGHT_GRID_Z_SIZE_INT);
            gridMaxs[0] = gridMins[0] + R_LIGHT_GRID_XY_SIZE;
            gridMaxs[1] = gridMins[1] + R_LIGHT_GRID_XY_SIZE;
            gridMaxs[2] = gridMins[2] + R_LIGHT_GRID_Z_SIZE;
            R_AddDebugBox(gridMins, gridMaxs, lightGridColorWhite);
        }
    }

    for (int32_t corner = 0;
         corner < R_LIGHT_GRID_CORNER_COUNT;
         ++corner) {
        const int32_t xCorner = corner & 1;
        const int32_t yCorner = (corner >> 1) & 1;
        const int32_t zCorner = (corner >> 2) & 1;
        const int32_t gridX = gridBase[0] + xCorner;
        const int32_t gridY = gridBase[1] + yCorner;
        const int32_t gridZ = gridBase[2] + zCorner;
        uint8_t diffuseSunVisibility;
        uint32_t visibleLightBits = R_GetCachedVisibility(
            gridX, gridY, gridZ, leaf->data.leaf.cluster,
            lightCount, lights, point, &diffuseSunVisibility);
        vec3_t samplePoint;
        vec3_t debugMins;
        vec3_t debugMaxs;

        if (r_showLeafLights->integer < 0) {
            /* Unlike the enclosing-cell box above, the DLL converts each
             * multiplied grid coordinate to float before subtracting the
             * world offset (0x004c6bc7..0x004c6c14). Keep that operation
             * order so out-of-range debug coordinates round identically. */
            samplePoint[0] =
                (float)(gridX * R_LIGHT_GRID_XY_SIZE_INT) -
                R_LIGHT_GRID_WORLD_OFFSET;
            samplePoint[1] =
                (float)(gridY * R_LIGHT_GRID_XY_SIZE_INT) -
                R_LIGHT_GRID_WORLD_OFFSET;
            samplePoint[2] =
                (float)(gridZ * R_LIGHT_GRID_Z_SIZE_INT) -
                R_LIGHT_GRID_WORLD_OFFSET;
            debugMins[0] =
                samplePoint[0] - R_LIGHT_GRID_DEBUG_HALF_SIZE;
            debugMins[1] =
                samplePoint[1] - R_LIGHT_GRID_DEBUG_HALF_SIZE;
            debugMins[2] =
                samplePoint[2] - R_LIGHT_GRID_DEBUG_HALF_SIZE;
            debugMaxs[0] =
                samplePoint[0] + R_LIGHT_GRID_DEBUG_HALF_SIZE;
            debugMaxs[1] =
                samplePoint[1] + R_LIGHT_GRID_DEBUG_HALF_SIZE;
            debugMaxs[2] =
                samplePoint[2] + R_LIGHT_GRID_DEBUG_HALF_SIZE;
        }

        if (visibleLightBits == UINT32_MAX) {
            if (r_showLeafLights->integer < 0) {
                R_AddDebugBox(debugMins, debugMaxs, lightGridColorRed);
            }
            continue;
        }

        {
            /* 0x004c6ca3..0x004c6cbd stores cornerWeight as float but adds
             * the still-retained x87 product to totalWeight. */
            const long double cornerWeightRaw =
                ((long double)gridWeights[1][yCorner] *
                 (long double)gridWeights[2][zCorner]) *
                (long double)gridWeights[0][xCorner];
            const float cornerWeight = (float)cornerWeightRaw;

            totalWeight =
                (float)(cornerWeightRaw + (long double)totalWeight);
            *diffuseSunContribution +=
                (float)diffuseSunVisibility * cornerWeight *
                tr.diffuseSunSampleScale;

            if (visibleLightBits == 0) {
                if (r_showLeafLights->integer < 0) {
                    R_AddDebugBox(debugMins, debugMaxs,
                                  lightGridColorBlue);
                }
            } else {
                for (int32_t lightIndex = 0;
                     lightIndex < lightCount;
                     ++lightIndex) {
                    if ((visibleLightBits &
                         (1U << (uint32_t)lightIndex)) == 0U) {
                        continue;
                    }

                    contributions[lightIndex] += cornerWeight;
                    if (r_showLeafLights->integer < 0) {
                        const renderer_light_t *light = lights[lightIndex];

                        if (light->position[3] == 0.0f) {
                            vec3_t lineEnd;

                            lineEnd[0] = samplePoint[0] +
                                light->position[0] *
                                    R_LIGHT_GRID_DIRECTION_LENGTH;
                            lineEnd[1] = samplePoint[1] +
                                light->position[1] *
                                    R_LIGHT_GRID_DIRECTION_LENGTH;
                            lineEnd[2] = samplePoint[2] +
                                light->position[2] *
                                    R_LIGHT_GRID_DIRECTION_LENGTH;
                            R_AddDebugLine(samplePoint, lineEnd,
                                           lightGridColorYellow);
                        } else {
                            R_AddDebugLine(samplePoint, light->position,
                                           lightGridColorGreen);
                        }
                    }
                }
            }
        }
    }

    if (totalWeight < R_LIGHT_GRID_INTERPOLATION_MINIMUM) {
        if (totalWeight == 0.0f) {
            if (leaf->data.leaf.cluster < 0) {
                for (int32_t lightIndex = 0;
                     lightIndex < lightCount;
                     ++lightIndex) {
                    contributions[lightIndex] = 1.0f;
                }
                *diffuseSunContribution = 1.0f;
            }
        } else {
            const float inverseWeight = 1.0f / totalWeight;

            if (*diffuseSunContribution != 0.0f)
                *diffuseSunContribution *= inverseWeight;

            for (int32_t lightIndex = 0;
                 lightIndex < lightCount;
                 ++lightIndex) {
                if (contributions[lightIndex] == 0.0f)
                    continue;

                if (lights[lightIndex] == tr.world->sunLight) {
                    contributions[lightIndex] += 1.0f - totalWeight;
                } else {
                    contributions[lightIndex] *= inverseWeight;
                }
            }
        }
    }

    if (leaf->data.leaf.hasSunLight != qfalse &&
        *diffuseSunContribution < R_DIFFUSE_SUN_MINIMUM_CONTRIBUTION) {
        *diffuseSunContribution = R_DIFFUSE_SUN_MINIMUM_CONTRIBUTION;
    }
    return lightCount;
}

/* Source: CoDUOMP.exe 0x004c7050..0x004c78ef.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c7050_004c78f0.mcode.
 * Name, parameter order, and source-level calling convention: same-module
 * Mac symbol R_PickFinalLights. The Windows whole-program optimizer carries
 * entity and contribution pointers in EDI/EBX, while both callers prove the
 * ordinary eight-parameter interface below. Windows proves the dynamic-light
 * radius test, generated diffuse-sun lights, intensity calculation, stable
 * eight-entry insertion, final light scaling, debug draw, and optional merge. */
void R_PickFinalLights(const trRefdef_t *refdef,
                       const vec3_t point, trRefEntity_t *entity,
                       int32_t lightCount, float *contributions,
                       float diffuseSunContribution,
                       renderer_light_t **lights, qboolean mergeLights)
{
    int32_t selectedIndices[R_MAX_ENTITY_LIGHTS];
    float candidateIntensities[R_MAX_ENTITY_LIGHT_CANDIDATES];
    int32_t selectedCount = 0;

    entity->hasDynamicLights = 0;
    if (refdef != NULL && refdef->entityDlightCount > 0) {
        for (int32_t dynamicIndex = 0;
             dynamicIndex < refdef->entityDlightCount;
             ++dynamicIndex) {
            renderer_light_t *light = &refdef->dlights[dynamicIndex];
            const float distanceSquared =
                VectorDistanceSquared(point, light->position);
            const float radiusSquared = light->radius * light->radius;

            if (distanceSquared <=
                radiusSquared * R_DYNAMIC_LIGHT_RADIUS_SQUARED_SCALE) {
                lights[lightCount] = light;
                contributions[lightCount] = 1.0f;
                ++lightCount;
                entity->hasDynamicLights = 1;
            }
        }
    }

    if (diffuseSunContribution != 0.0f &&
        tr.world->entitySunLightIntensity != 0.0f &&
        tr.diffuseSunQuality != 0) {
        renderer_light_t *upperLight = &entity->generatedLights[1];

        upperLight->type = R_LIGHT_TYPE_DIFFUSE_SUN;
        upperLight->color[0] = tr.world->entityAmbientScale[0];
        upperLight->color[1] = tr.world->entityAmbientScale[1];
        upperLight->color[2] = tr.world->entityAmbientScale[2];
        upperLight->intensity = tr.world->entitySunLightIntensity;
        upperLight->specular[0] = 0.0f;
        upperLight->specular[1] = 0.0f;
        upperLight->specular[2] = 0.0f;
        upperLight->specular[3] = 0.0f;
        upperLight->position[0] = 0.0f;
        upperLight->position[1] = 0.0f;
        upperLight->position[2] = R_LIGHT_DIRECTION_UP;
        upperLight->position[3] = 0.0f;
        upperLight->spotCutoff = R_MERGED_LIGHT_SPOT_CUTOFF;

        if (tr.diffuseSunQuality == 1 ||
            lightCount > tr.maxEntityLights) {
            upperLight->ambient[0] =
                tr.world->entityAmbientScale[0] *
                R_DIFFUSE_SUN_FALLBACK_SCALE;
            upperLight->ambient[1] =
                tr.world->entityAmbientScale[1] *
                R_DIFFUSE_SUN_FALLBACK_SCALE;
            upperLight->ambient[2] =
                tr.world->entityAmbientScale[2] *
                R_DIFFUSE_SUN_FALLBACK_SCALE;
            upperLight->ambient[3] =
                tr.world->entityAmbientScale[3] *
                R_DIFFUSE_SUN_FALLBACK_SCALE;
            upperLight->diffuse[0] =
                tr.world->entityAmbientScale[0] *
                R_DIFFUSE_SUN_FALLBACK_SCALE;
            upperLight->diffuse[1] =
                tr.world->entityAmbientScale[1] *
                R_DIFFUSE_SUN_FALLBACK_SCALE;
            upperLight->diffuse[2] =
                tr.world->entityAmbientScale[2] *
                R_DIFFUSE_SUN_FALLBACK_SCALE;
            upperLight->diffuse[3] =
                tr.world->entityAmbientScale[3] *
                R_DIFFUSE_SUN_FALLBACK_SCALE;

            contributions[lightCount] = diffuseSunContribution;
            lights[lightCount] = upperLight;
            ++lightCount;
        } else {
            renderer_light_t *lowerLight = &entity->generatedLights[2];

            upperLight->ambient[0] =
                tr.world->entityAmbientScale[0] *
                R_DIFFUSE_SUN_AMBIENT_HIGH;
            upperLight->ambient[1] =
                tr.world->entityAmbientScale[1] *
                R_DIFFUSE_SUN_AMBIENT_HIGH;
            upperLight->ambient[2] =
                tr.world->entityAmbientScale[2] *
                R_DIFFUSE_SUN_AMBIENT_HIGH;
            upperLight->ambient[3] =
                tr.world->entityAmbientScale[3] *
                R_DIFFUSE_SUN_AMBIENT_HIGH;
            upperLight->diffuse[0] =
                tr.world->entityAmbientScale[0] *
                R_DIFFUSE_SUN_AMBIENT_LOW;
            upperLight->diffuse[1] =
                tr.world->entityAmbientScale[1] *
                R_DIFFUSE_SUN_AMBIENT_LOW;
            upperLight->diffuse[2] =
                tr.world->entityAmbientScale[2] *
                R_DIFFUSE_SUN_AMBIENT_LOW;
            upperLight->diffuse[3] =
                tr.world->entityAmbientScale[3] *
                R_DIFFUSE_SUN_AMBIENT_LOW;
            contributions[lightCount] = diffuseSunContribution;
            lights[lightCount] = upperLight;
            ++lightCount;

            lowerLight->type = R_LIGHT_TYPE_DIFFUSE_SUN;
            lowerLight->color[0] = upperLight->color[0];
            lowerLight->color[1] = upperLight->color[1];
            lowerLight->color[2] = upperLight->color[2];
            lowerLight->intensity =
                tr.world->entitySunLightIntensity *
                R_DIFFUSE_SUN_OPPOSING_SCALE;
            lowerLight->ambient[0] = 0.0f;
            lowerLight->ambient[1] = 0.0f;
            lowerLight->ambient[2] = 0.0f;
            lowerLight->ambient[3] = 0.0f;
            lowerLight->diffuse[0] =
                tr.world->entityAmbientScale[0] *
                R_DIFFUSE_SUN_OPPOSING_SCALE;
            lowerLight->diffuse[1] =
                tr.world->entityAmbientScale[1] *
                R_DIFFUSE_SUN_OPPOSING_SCALE;
            lowerLight->diffuse[2] =
                tr.world->entityAmbientScale[2] *
                R_DIFFUSE_SUN_OPPOSING_SCALE;
            lowerLight->diffuse[3] =
                tr.world->entityAmbientScale[3] *
                R_DIFFUSE_SUN_OPPOSING_SCALE;
            lowerLight->specular[0] = 0.0f;
            lowerLight->specular[1] = 0.0f;
            lowerLight->specular[2] = 0.0f;
            lowerLight->specular[3] = 0.0f;
            lowerLight->position[0] = 0.0f;
            lowerLight->position[1] = 0.0f;
            lowerLight->position[2] = R_LIGHT_DIRECTION_DOWN;
            lowerLight->position[3] = 0.0f;
            lowerLight->spotCutoff = R_MERGED_LIGHT_SPOT_CUTOFF;
            contributions[lightCount] = diffuseSunContribution;
            lights[lightCount] = lowerLight;
            ++lightCount;
        }

        diffuseSunContribution = 0.0f;
    }

    for (int32_t candidateIndex = 0;
         candidateIndex < lightCount;
         ++candidateIndex) {
        renderer_light_t *light;
        float intensity;
        int32_t insertionIndex;

        if (contributions[candidateIndex] == 0.0f)
            continue;

        light = lights[candidateIndex];
        if (!(light->intensity >= 0.0f)) {
            intensity = R_LIGHT_PRIORITY_SENTINEL;
        } else {
            intensity = contributions[candidateIndex] *
                        R_MaxLightIntensity(point, light);
            if (light->type == R_LIGHT_TYPE_SUN ||
                light->type == R_LIGHT_TYPE_DIFFUSE_SUN) {
                intensity += (float)(1 << tr.overbrightBits) *
                             tr.world->entitySunLightIntensity *
                             diffuseSunContribution;
            }

            if (intensity < r_minEntLightIntensity->value)
                continue;
        }
        candidateIntensities[candidateIndex] = intensity;

        insertionIndex = 0;
        while (insertionIndex < selectedCount &&
               candidateIntensities[selectedIndices[insertionIndex]] >
                   intensity) {
            ++insertionIndex;
        }

        if (insertionIndex >= R_MAX_ENTITY_LIGHTS)
            continue;
        if (selectedCount == R_MAX_ENTITY_LIGHTS)
            selectedCount = R_MAX_ENTITY_LIGHTS - 1;
        for (int32_t moveIndex = selectedCount;
             moveIndex > insertionIndex;
             --moveIndex) {
            selectedIndices[moveIndex] = selectedIndices[moveIndex - 1];
        }
        selectedIndices[insertionIndex] = candidateIndex;
        ++selectedCount;
    }

    entity->lightCount = selectedCount;
    for (int32_t selectedIndex = 0;
         selectedIndex < selectedCount;
         ++selectedIndex) {
        const int32_t candidateIndex = selectedIndices[selectedIndex];

        entity->lights[selectedIndex].light = lights[candidateIndex];
        entity->lights[selectedIndex].scale =
            contributions[candidateIndex] * r_LightScale->value;
    }
    entity->diffuseSunContribution = diffuseSunContribution;

    if (r_showLeafLights->integer > 0)
        R_ShowLeafLights(point, entity);
    if (mergeLights != qfalse)
        R_MergeLights(point, entity, contributions, selectedIndices);
}

/* Source: CoDUOMP.exe 0x004c78f0..0x004c7977.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c78f0_004c7978.mcode.
 * Name and four-parameter interface: same-module Mac symbol R_PickLights.
 * Windows proves both 50-entry scratch arrays and the fullbright/world gates. */
void R_PickLights(const trRefdef_t *refdef, const vec3_t point,
                  trRefEntity_t *entity, qboolean mergeLights)
{
    float contributions[R_MAX_ENTITY_LIGHT_CANDIDATES];
    renderer_light_t *lights[R_MAX_ENTITY_LIGHT_CANDIDATES];
    float diffuseSunContribution;
    int32_t lightCount;

    if (tr.world == NULL || tr.world->nodes == NULL ||
        r_entFullbright->integer != 0) {
        entity->lightCount = 0;
        return;
    }

    lightCount = R_GetStaticLightContributions(
        point, contributions, &diffuseSunContribution, lights);
    R_PickFinalLights(refdef, point, entity, lightCount, contributions,
                      diffuseSunContribution, lights, mergeLights);
}

/* Source: CoDUOMP.exe 0x004c7980..0x004c7aab.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c7980_004c7aac.mcode.
 * Name and parameter roles: same-module Mac symbol R_SetupEntityLighting.
 * The inverse-axis debug direction is deliberately accumulated through the
 * entity field after each source-axis contribution, matching the DLL's float
 * stores and rounding at 0x004c79f3..0x004c7aa1. */
void R_SetupEntityLighting(const trRefdef_t *refdef,
                           trRefEntity_t *entity)
{
    vec3_t samplePoint;

    if (entity->lightingCalculated != 0)
        return;
    entity->lightingCalculated = 1;

    if (((uint32_t)entity->e.renderfx &
         RF_LIGHTING_ORIGIN) != 0U) {
        samplePoint[0] = entity->e.lightingOrigin[0];
        samplePoint[1] = entity->e.lightingOrigin[1];
        samplePoint[2] = entity->e.lightingOrigin[2];
    } else {
        samplePoint[0] = entity->e.origin[0];
        samplePoint[1] = entity->e.origin[1];
        samplePoint[2] = entity->e.origin[2];
    }
    R_PickLights(refdef, samplePoint, entity, qtrue);

    if (r_debugEntLight->integer == 2) {
        axis_t inverseAxis;

        MatrixInverse(entity->e.axis, inverseAxis);
        entity->lightDir[0] =
            inverseAxis[0][0] * tr.sunDirection[0];
        entity->lightDir[1] =
            inverseAxis[0][1] * tr.sunDirection[0];
        entity->lightDir[2] =
            inverseAxis[0][2] * tr.sunDirection[0];
        entity->lightDir[0] +=
            inverseAxis[1][0] * tr.sunDirection[1];
        entity->lightDir[1] +=
            inverseAxis[1][1] * tr.sunDirection[1];
        entity->lightDir[2] +=
            inverseAxis[1][2] * tr.sunDirection[1];
        entity->lightDir[0] +=
            inverseAxis[2][0] * tr.sunDirection[2];
        entity->lightDir[1] +=
            inverseAxis[2][1] * tr.sunDirection[2];
        entity->lightDir[2] +=
            inverseAxis[2][2] * tr.sunDirection[2];
    }
}

/* Source: CoDUOMP.exe 0x004c7ab0..0x004c7bed.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c7ab0_004c7bee.mcode.
 * Name and parameter roles: same-module Mac symbol
 * R_SetupStaticModelLighting. The persistent model record supplies the
 * precomputed 49-entry contributions/light arrays at +0xc0/+0x184. */
void R_SetupStaticModelLighting(const trRefdef_t *refdef,
                                trRefEntity_t *entity)
{
    renderer_static_model_t *lighting;

    if (r_showLeafLights->integer < 0) {
        R_SetupEntityLighting(refdef, entity);
        return;
    }
    if (entity->lightingCalculated != 0)
        return;

    entity->lightingCalculated = 1;
    lighting = entity->staticModelLighting;
    R_PickFinalLights(refdef, entity->e.lightingOrigin, entity,
                      lighting->lightCount, lighting->contributions,
                      lighting->diffuseSunContribution, lighting->lights,
                      qtrue);

    if (r_debugEntLight->integer == 2) {
        axis_t inverseAxis;

        MatrixInverse(entity->e.axis, inverseAxis);
        entity->lightDir[0] =
            inverseAxis[0][0] * tr.sunDirection[0];
        entity->lightDir[1] =
            inverseAxis[0][1] * tr.sunDirection[0];
        entity->lightDir[2] =
            inverseAxis[0][2] * tr.sunDirection[0];
        entity->lightDir[0] +=
            inverseAxis[1][0] * tr.sunDirection[1];
        entity->lightDir[1] +=
            inverseAxis[1][1] * tr.sunDirection[1];
        entity->lightDir[2] +=
            inverseAxis[1][2] * tr.sunDirection[1];
        entity->lightDir[0] +=
            inverseAxis[2][0] * tr.sunDirection[2];
        entity->lightDir[1] +=
            inverseAxis[2][1] * tr.sunDirection[2];
        entity->lightDir[2] +=
            inverseAxis[2][2] * tr.sunDirection[2];
    }
}
