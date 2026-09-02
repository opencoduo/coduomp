#include "backend.h"

#include <stdint.h>

#include "gl_api.h"
#include "gl_state.h"

/* Source: CoDUOMP.exe 0x004be8f0..0x004bea2a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004be8f0_004bea2b.mcode.
 * Name: same-module Mac symbol RB_SetupLight. The Windows optimizer carries
 * the light pointer and slot index in registers; maintained source uses a
 * normal portable signature. */
void RB_SetupLight(const renderer_light_t *light, int32_t lightIndex, float scale)
{
    const uint32_t glLight = GL_LIGHT0 + (uint32_t)lightIndex;
    vec4_t ambient;
    vec4_t diffuse;
    const float *ambientValues = light->ambient;
    const float *diffuseValues = light->diffuse;

    if (scale != 1.0f) {
        ambient[0] = light->ambient[0] * scale;
        ambient[1] = light->ambient[1] * scale;
        ambient[2] = light->ambient[2] * scale;
        ambient[3] = 1.0f;
        diffuse[0] = light->diffuse[0] * scale;
        diffuse[1] = light->diffuse[1] * scale;
        diffuse[2] = light->diffuse[2] * scale;
        diffuse[3] = 1.0f;
        ambientValues = ambient;
        diffuseValues = diffuse;
    }

    qglLightfv(glLight, GL_AMBIENT, ambientValues);
    qglLightfv(glLight, GL_DIFFUSE, diffuseValues);
    qglLightfv(glLight, GL_SPECULAR, light->specular);
    qglLightfv(glLight, GL_POSITION, light->position);
    qglLightfv(glLight, GL_SPOT_DIRECTION, light->spotDirection);
    qglLightf(glLight, GL_SPOT_EXPONENT, light->spotExponent);
    qglLightf(glLight, GL_SPOT_CUTOFF, light->spotCutoff);
    qglLightf(glLight, GL_CONSTANT_ATTENUATION, light->constantAttenuation);
    qglLightf(glLight, GL_LINEAR_ATTENUATION, light->linearAttenuation);
    qglLightf(glLight, GL_QUADRATIC_ATTENUATION, light->quadraticAttenuation);

    if (lightIndex >= glState.enabledLightCount)
        qglEnable(glLight);
}

/* Source: CoDUOMP.exe 0x004bea30..0x004bec61.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004bea30_004bec62.mcode.
 * Name: same-module Mac symbol RB_EnableHWLights. */
qboolean RB_EnableHWLights(void)
{
    trRefEntity_t *entity = backEnd.currentEntity;
    renderer_entity_light_t *sortedLights[R_MAX_ENTITY_LIGHTS];
    const uint32_t lightingFlags = tess.shader->lightingFlags & SHADER_LIGHTING_ENTITY_MASK;
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    vec4_t ambient;
    qboolean loadedLightMatrix = qfalse;
    int32_t lightIndex;

    if (glState.currentLightingEntity == entity && glState.currentLightingFlags == lightingFlags) {
        return qfalse;
    }

    glState.currentLightingEntity = entity;
    glState.currentLightingFlags = lightingFlags;

    ambient[0] = tr.world->entityAmbientBase[0] + tr.world->entityAmbientScale[0] * entity->diffuseSunContribution;
    ambient[1] = tr.world->entityAmbientBase[1] + tr.world->entityAmbientScale[1] * entity->diffuseSunContribution;
    ambient[2] = tr.world->entityAmbientBase[2] + tr.world->entityAmbientScale[2] * entity->diffuseSunContribution;
    ambient[3] = 1.0f; /* determinized; see ORIGINAL_BINARY_BUG above */
    if (((uint32_t)entity->e.renderfx & RENDERER_ENTITY_FORCE_MIN_LIGHT) != 0) {
        const float luminance = ambient[0] * 0.29899999499320984f + ambient[1] * 0.5870000123977661f + ambient[2] * 0.11400000005960464f;
        const float minimumLight = tr.identityLight * r_entMinLight->value;

        if (luminance == 0.0f) {
            ambient[0] = minimumLight;
            ambient[1] = minimumLight;
            ambient[2] = minimumLight;
        } else if (luminance < minimumLight) {
            const float scale = minimumLight / luminance;
            ambient[0] *= scale;
            ambient[1] *= scale;
            ambient[2] *= scale;
        }
    }

    qglLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);

    for (lightIndex = 0; lightIndex < entity->lightCount; ++lightIndex) {
        renderer_entity_light_t *entry = &entity->lights[lightIndex];
        int32_t insertionIndex = lightIndex;

        while (insertionIndex > 0) {
            renderer_entity_light_t *previous = sortedLights[insertionIndex - 1];

            /* The second comparison is intentionally the original raw light
             * address ordering, not an unresolved pointer representation. */
            if (entry->light->type > previous->light->type || (uintptr_t)entry->light > (uintptr_t)previous->light) {
                break;
            }

            sortedLights[insertionIndex] = previous;
            --insertionIndex;
        }

        sortedLights[insertionIndex] = entry;
    }

    if (entity->lightCount != 0) {
        qglLoadMatrixf(tr.viewParms.world.modelMatrix);
        loadedLightMatrix = qtrue;
    }

    for (lightIndex = 0; lightIndex < entity->lightCount; ++lightIndex) {
        RB_SetupLight(sortedLights[lightIndex]->light, lightIndex, sortedLights[lightIndex]->scale);
    }

    for (; lightIndex < glState.enabledLightCount; ++lightIndex)
        qglDisable(GL_LIGHT0 + (uint32_t)lightIndex);

    glState.enabledLightCount = entity->lightCount;
    return loadedLightMatrix;
}
