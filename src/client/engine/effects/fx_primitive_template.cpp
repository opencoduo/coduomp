#include "fx_primitive_template.hpp"

#include "fx_api.h"
#include "fx_memory.h"
#include "fx_scheduler.hpp"
#include "../platform/crt_boundary.h"
#include "qcommon/q_shared_types.h"
#include "qcommon/q_string.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Original fixed-pool descriptor 0x0389ffe8.  Startup initializer
 * 0x00585170 computes the usable 32-KiB payload divided by
 * sizeof(CPrimitiveTemplate), then clears the block-list pointer.  The
 * resulting count is intentionally host-dependent because this private class
 * contains implementation-specific std::vector layouts (MSVC i386 yields 50;
 * libstdc++ i386 yields 52). PE_ZERO_WITH_RUNTIME_INITIALIZER */
fx_pool_allocator_t fxPrimitiveTemplateAllocator = { /* original 0x0389ffe8 */
    static_cast<int32_t>(
        sizeof(((fx_mem_block_t *)nullptr)->storage) /
        sizeof(CPrimitiveTemplate)),
    nullptr
};

/* Source: CoDUOMP.exe 0x004a5fd0..0x004a607c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004a5fd0_004a607c.mcode.
 * The same-module Mac build names the generated vector cleanup
 * CMediaHandles::~CMediaHandles.  The Windows calls pass the complete
 * CPrimitiveTemplate address, and the maintained flat class owns the same five
 * vectors directly, so its default destructor expresses the proven ownership
 * without inventing an unproven base-class split. */
CPrimitiveTemplate::~CPrimitiveTemplate() = default;

/* Source: CoDUOMP.exe 0x004ab170..0x004ab706 plus the caller's in-use store at
 * 0x004a6393. The Windows optimizer outlined this field-copy sequence from the
 * temporary-effect-template clone at 0x004a62f0. It is intentionally not the
 * C++ default copy operation: temporary is set only after the copy, while
 * nonUniformScale, height, windModifier, and velocityClamp retain the fresh
 * constructor defaults. The unreferenced related emission at
 * 0x004ab710..0x004abc88 has a narrower field set that also omits
 * materialImpact and cullRange; it is not another copy of this operation. */
void CPrimitiveTemplate::CopyForTemporaryEffect(
    const CPrimitiveTemplate &source)
{
    strcpy(name, source.name);
    primitiveType = source.primitiveType;
    delay = source.delay;
    count = source.count;
    life = source.life;
    cullRange = source.cullRange;

    resources = source.resources;
    impactEffects = source.impactEffects;
    deathEffects = source.deathEffects;
    emitterEffects = source.emitterEffects;
    playEffects = source.playEffects;

    strcpy(materialImpact, source.materialImpact);
    flags = source.flags;
    parameterFlags = source.parameterFlags;
    spawnFlags = source.spawnFlags;
    for (int32_t component = 0; component < 3; ++component) {
        boundsMin[component] = source.boundsMin[component];
        boundsMax[component] = source.boundsMax[component];
    }
    origin1 = source.origin1;
    origin2 = source.origin2;
    radius = source.radius;
    rotation = source.rotation;
    rotationClamp = source.rotationClamp;
    rotationDelta = source.rotationDelta;
    rotationAccel = source.rotationAccel;
    angle = source.angle;
    angleDelta = source.angleDelta;
    velocity = source.velocity;
    acceleration = source.acceleration;
    gravity = source.gravity;
    density = source.density;
    variance = source.variance;
    rgb = source.rgb;
    alpha = source.alpha;
    size = source.size;
    size2 = source.size2;
    length = source.length;
    elasticity = source.elasticity;

    temporary = true;
}

/* NOT_FROM_ORIGINAL_SOURCE: the retail completion path performs this same
 * decrement/delete sequence inside CreateEffect, but cancellation and failure
 * exits omit it. Keep the ownership operation in one compatibility method so
 * every completed or abandoned pending spawn retires exactly one reference. */
void CPrimitiveTemplate::coduomp_retire_pending_spawn()
{
    if (!temporary) {
        return;
    }
    --remainingSpawnCount;
    if (remainingSpawnCount <= 0) {
        delete this;
    }
}

/* Source: CoDUOMP.exe 0x004a6080..0x004a608c.  The initial Ghidra export
 * omitted this complete allocation adapter from its function records. */
void *CPrimitiveTemplate::operator new(size_t size) noexcept
{
    return coduomp_fx_mem_alloc_from_pool(&fxPrimitiveTemplateAllocator,
                                          size);
}

/* Source: CoDUOMP.exe 0x004a6090..0x004a609a. */
void CPrimitiveTemplate::operator delete(void *allocation) noexcept
{
    coduomp_fx_mem_free_from_pool(&fxPrimitiveTemplateAllocator, allocation);
}

/* Source: CoDUOMP.exe 0x004aad00..0x004ab153.
 * Name and class layout: same-module Mac symbol
 * CPrimitiveTemplate::CPrimitiveTemplate. primitiveType,
 * remainingSpawnCount, and the explicitly reserved alignment bytes are
 * intentionally left indeterminate because the Windows constructor does not
 * write them. */
CPrimitiveTemplate::CPrimitiveTemplate()
{
    temporary = false;
    name[0] = '\0';
    materialImpact[0] = '\0';
    delay = {};
    count = {1.0f, 1.0f};
    life = {1.0f, 1.0f};
    cullRange = 0;
    flags = 0;
    parameterFlags = 0;
    spawnFlags = 0;
    nonUniformScale = false;
    boundsMin[0] = boundsMin[1] = boundsMin[2] = 0.0f;
    boundsMax[0] = boundsMax[1] = boundsMax[2] = 0.0f;
    origin1 = {};
    origin2 = {};
    radius = {1.0f, 1.0f};
    height = {1.0f, 1.0f};
    windModifier = {};
    rotation = {};
    rotationClamp = {};
    rotationDelta = {};
    rotationAccel = {};
    angle = {};
    angleDelta = {};
    velocity = {};
    velocityClamp = {};
    acceleration = {};
    gravity = {};
    density = {10.0f, 10.0f};
    variance = {1.0f, 1.0f};
    rgb = {};
    for (int32_t component = 0; component < 3; ++component) {
        rgb.start.start[component] = 1.0f;
        rgb.start.end[component] = 1.0f;
        rgb.end.start[component] = 1.0f;
        rgb.end.end[component] = 1.0f;
    }
    alpha = {};
    alpha.start = {1.0f, 1.0f};
    alpha.end = {1.0f, 1.0f};
    size = {};
    size.start = {1.0f, 1.0f};
    size.end = {1.0f, 1.0f};
    size2 = {};
    size2.start = {1.0f, 1.0f};
    size2.end = {1.0f, 1.0f};
    length = {};
    length.start = {1.0f, 1.0f};
    length.end = {1.0f, 1.0f};
    elasticity = {};
    elasticity.start = {1.0f, 1.0f};
    elasticity.end = {1.0f, 1.0f};
}

/* Source: CoDUOMP.exe 0x004abc90..0x004abcba.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004abc90_004abcbb.mcode.
 * Name and signature: exact same-module Mac symbol
 * CPrimitiveTemplate::ParseFloat. MSVC also inlines this source method into
 * the scalar wrappers recovered below. */
qboolean CPrimitiveTemplate::ParseFloat(const char *text,
                                         float *start, float *end)
{
    if (start == nullptr || end == nullptr) {
        return qfalse;
    }

    const int32_t parsed = sscanf(text, "%f %f", start, end);
    /* The Windows code tests only for zero. EOF therefore reports success
     * without modifying either direct destination. The scalar member wrappers
     * below deliberately stage through locals before publishing the result. */
    if (parsed == 0) {
        return qfalse;
    }
    if (parsed == 1) {
        *end = *start;
    }
    return qtrue;
}

/* Source: CoDUOMP.exe 0x004ac070..0x004ac0be.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004ac070_004ac0bf.mcode.
 * Name and member binding: same-module Mac symbol
 * CPrimitiveTemplate::ParseCount. MSVC inlines ParseFloat into this body and
 * additionally inlines the resulting operation into ParsePrimitive. */
qboolean CPrimitiveTemplate::ParseCount(const char *text)
{
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    float parsed[2] = {0.0f, 0.0f};
    if (ParseFloat(text, &parsed[0], &parsed[1]) == qfalse) {
        return qfalse;
    }
    memcpy(&count, parsed, sizeof(parsed));
    return qtrue;
}

/* Source: CoDUOMP.exe 0x004abcc0..0x004abd3d.
 * Name and signature: same-module Mac symbol CPrimitiveTemplate::ParseVector.
 * A three-component spelling supplies one vector for both endpoints; the
 * six-component spelling supplies independent start and end vectors. */
qboolean CPrimitiveTemplate::ParseVector(const char *text,
                                          float *start, float *end)
{
    if (start == nullptr || end == nullptr) {
        return qfalse;
    }

    const int32_t parsed = sscanf(text, "%f %f %f   %f %f %f",
                                  &start[0], &start[1], &start[2],
                                  &end[0], &end[1], &end[2]);
    if (parsed < 3 || parsed == 4 || parsed == 5) {
        return qfalse;
    }

    if (parsed == 3) {
        end[0] = start[0];
        end[1] = start[1];
        end[2] = start[2];
    }
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
qboolean CPrimitiveTemplate::ParseGroupFlags(const char *text,
                                              int32_t *flags)
{
    if (flags == nullptr) {
        return qfalse;
    }

    char tokens[4][MAX_TOKEN_CHARS] = {};
    const int32_t parsed = sscanf(text, "%s %s %s %s",
                                  tokens[0], tokens[1],
                                  tokens[2], tokens[3]);
    *flags = 0;

    for (int32_t tokenIndex = 0;
         tokenIndex < parsed && tokenIndex < 4;
         ++tokenIndex) {
        const char *token = tokens[tokenIndex];
        if (coduo_crt_stricmp(token, "linear") == 0) {
            *flags |= FX_PARAMETER_LINEAR;
        } else if (coduo_crt_stricmp(token, "nonlinear") == 0) {
            *flags |= FX_PARAMETER_NONLINEAR;
        } else if (coduo_crt_stricmp(token, "wave") == 0) {
            *flags |= FX_PARAMETER_WAVE;
        } else if (coduo_crt_stricmp(token, "random") == 0) {
            *flags |= FX_PARAMETER_RANDOM;
        } else if (coduo_crt_stricmp(token, "clamp") == 0) {
            *flags |= FX_PARAMETER_CLAMP;
        } else {
            return qfalse;
        }
    }
    return qtrue;
}

/* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
#define FX_PARSE_SCALAR_PROPERTY(destination_)                           \
    do {                                                                 \
        float parsed[2] = {0.0f, 0.0f};                                  \
        if (ParseFloat(text, &parsed[0], &parsed[1]) == qfalse) {        \
            return qfalse;                                               \
        }                                                                \
        memcpy(&(destination_), parsed, sizeof(parsed));                 \
        return qtrue;                                                    \
    } while (0)

/* The retained vector wrappers likewise parse into a complete temporary and
 * publish all six components only after ParseVector succeeds. This prevents a
 * rejected one-, two-, four-, or five-component spelling from partially
 * changing the live template. */
#define FX_PARSE_VECTOR_PROPERTY(destination_)                           \
    do {                                                                 \
        fx_vector_range_t parsed;                                        \
        if (ParseVector(text, parsed.start, parsed.end) == qfalse) {      \
            return qfalse;                                               \
        }                                                                \
        memcpy(&(destination_), &parsed, sizeof(parsed));                \
        return qtrue;                                                    \
    } while (0)

qboolean CPrimitiveTemplate::ParseMin(const char *text)
{
    vec3_t parsedBounds;
    if (ParseVector(text, parsedBounds, parsedBounds) == qfalse) {
        return qfalse;
    }
    for (int32_t component = 0; component < 3; ++component) {
        boundsMin[component] = parsedBounds[component];
    }
    flags |= FX_PRIMITIVE_FLAG_BOUNDS;
    return qtrue;
}

qboolean CPrimitiveTemplate::ParseMax(const char *text)
{
    vec3_t parsedBounds;
    if (ParseVector(text, parsedBounds, parsedBounds) == qfalse) {
        return qfalse;
    }
    for (int32_t component = 0; component < 3; ++component) {
        boundsMax[component] = parsedBounds[component];
    }
    flags |= FX_PRIMITIVE_FLAG_BOUNDS;
    return qtrue;
}

qboolean CPrimitiveTemplate::ParseLife(const char *text)
{
    FX_PARSE_SCALAR_PROPERTY(life);
}

qboolean CPrimitiveTemplate::ParseDelay(const char *text)
{
    FX_PARSE_SCALAR_PROPERTY(delay);
}

qboolean CPrimitiveTemplate::ParseElasticity(const char *text)
{
    /* Determinized empty-value carrier; see FX_PARSE_SCALAR_PROPERTY. */
    float parsed[2] = {0.0f, 0.0f};
    if (ParseFloat(text, &parsed[0], &parsed[1]) == qfalse) {
        return qfalse;
    }
    memcpy(&elasticity.parm, parsed, sizeof(parsed));
    flags |= FX_PRIMITIVE_FLAG_USE_PHYSICS;
    return qtrue;
}

qboolean CPrimitiveTemplate::ParseOrigin1(const char *text)
{
    FX_PARSE_VECTOR_PROPERTY(origin1);
}

qboolean CPrimitiveTemplate::ParseOrigin2(const char *text)
{
    FX_PARSE_VECTOR_PROPERTY(origin2);
}

qboolean CPrimitiveTemplate::ParseRadius(const char *text)
{
    FX_PARSE_SCALAR_PROPERTY(radius);
}

qboolean CPrimitiveTemplate::ParseHeight(const char *text)
{
    FX_PARSE_SCALAR_PROPERTY(height);
}

qboolean CPrimitiveTemplate::ParseWindModifier(const char *text)
{
    FX_PARSE_SCALAR_PROPERTY(windModifier);
}

qboolean CPrimitiveTemplate::ParseRotation(const char *text)
{
    FX_PARSE_SCALAR_PROPERTY(rotation);
}

qboolean CPrimitiveTemplate::ParseRotationDelta(const char *text)
{
    FX_PARSE_SCALAR_PROPERTY(rotationDelta);
}

qboolean CPrimitiveTemplate::ParseRotationAccel(const char *text)
{
    FX_PARSE_SCALAR_PROPERTY(rotationAccel);
}

qboolean CPrimitiveTemplate::ParseRotationClamp(const char *text)
{
    FX_PARSE_SCALAR_PROPERTY(rotationClamp);
}

qboolean CPrimitiveTemplate::ParseAngle(const char *text)
{
    FX_PARSE_VECTOR_PROPERTY(angle);
}

qboolean CPrimitiveTemplate::ParseAngleDelta(const char *text)
{
    FX_PARSE_VECTOR_PROPERTY(angleDelta);
}

qboolean CPrimitiveTemplate::ParseVelocity(const char *text)
{
    FX_PARSE_VECTOR_PROPERTY(velocity);
}

qboolean CPrimitiveTemplate::ParseVelocityClamp(const char *text)
{
    FX_PARSE_VECTOR_PROPERTY(velocityClamp);
}

qboolean CPrimitiveTemplate::ParseAcceleration(const char *text)
{
    FX_PARSE_VECTOR_PROPERTY(acceleration);
}

qboolean CPrimitiveTemplate::ParseGravity(const char *text)
{
    FX_PARSE_SCALAR_PROPERTY(gravity);
}

qboolean CPrimitiveTemplate::ParseDensity(const char *text)
{
    FX_PARSE_SCALAR_PROPERTY(density);
}

qboolean CPrimitiveTemplate::ParseVariance(const char *text)
{
    FX_PARSE_SCALAR_PROPERTY(variance);
}

/* Source: CoDUOMP.exe 0x004acf90..0x004ad54c. The same-module Mac symbols
 * supply the original property names; Windows stores and shifts prove each
 * destination and the packed parameterFlags lane. */
qboolean CPrimitiveTemplate::ParseRGBStart(const char *text)
{
    FX_PARSE_VECTOR_PROPERTY(rgb.start);
}

qboolean CPrimitiveTemplate::ParseRGBEnd(const char *text)
{
    FX_PARSE_VECTOR_PROPERTY(rgb.end);
}

qboolean CPrimitiveTemplate::ParseRGBParm(const char *text)
{
    FX_PARSE_SCALAR_PROPERTY(rgb.parm);
}

qboolean CPrimitiveTemplate::ParseRGBFlags(const char *text)
{
    int32_t parsedFlags;
    if (ParseGroupFlags(text, &parsedFlags) == qfalse) {
        return qfalse;
    }
    parameterFlags |= (uint32_t)parsedFlags << 4;
    return qtrue;
}

qboolean CPrimitiveTemplate::ParseAlphaStart(const char *text)
{
    FX_PARSE_SCALAR_PROPERTY(alpha.start);
}

qboolean CPrimitiveTemplate::ParseAlphaEnd(const char *text)
{
    FX_PARSE_SCALAR_PROPERTY(alpha.end);
}

qboolean CPrimitiveTemplate::ParseAlphaParm(const char *text)
{
    FX_PARSE_SCALAR_PROPERTY(alpha.parm);
}

qboolean CPrimitiveTemplate::ParseAlphaFlags(const char *text)
{
    int32_t parsedFlags;
    if (ParseGroupFlags(text, &parsedFlags) == qfalse) {
        return qfalse;
    }
    parameterFlags |= (uint32_t)parsedFlags;
    return qtrue;
}

qboolean CPrimitiveTemplate::ParseSizeStart(const char *text)
{
    FX_PARSE_SCALAR_PROPERTY(size.start);
}

qboolean CPrimitiveTemplate::ParseSizeEnd(const char *text)
{
    FX_PARSE_SCALAR_PROPERTY(size.end);
}

qboolean CPrimitiveTemplate::ParseSizeParm(const char *text)
{
    FX_PARSE_SCALAR_PROPERTY(size.parm);
}

qboolean CPrimitiveTemplate::ParseSizeFlags(const char *text)
{
    int32_t parsedFlags;
    if (ParseGroupFlags(text, &parsedFlags) == qfalse) {
        return qfalse;
    }
    parameterFlags |= (uint32_t)parsedFlags << 8;
    return qtrue;
}

qboolean CPrimitiveTemplate::ParseSize2Start(const char *text)
{
    FX_PARSE_SCALAR_PROPERTY(size2.start);
}

qboolean CPrimitiveTemplate::ParseSize2End(const char *text)
{
    FX_PARSE_SCALAR_PROPERTY(size2.end);
}

qboolean CPrimitiveTemplate::ParseSize2Parm(const char *text)
{
    FX_PARSE_SCALAR_PROPERTY(size2.parm);
}

qboolean CPrimitiveTemplate::ParseSize2Flags(const char *text)
{
    int32_t parsedFlags;
    if (ParseGroupFlags(text, &parsedFlags) == qfalse) {
        return qfalse;
    }
    parameterFlags |= (uint32_t)parsedFlags << 16;
    return qtrue;
}

qboolean CPrimitiveTemplate::ParseLengthStart(const char *text)
{
    FX_PARSE_SCALAR_PROPERTY(length.start);
}

qboolean CPrimitiveTemplate::ParseLengthEnd(const char *text)
{
    FX_PARSE_SCALAR_PROPERTY(length.end);
}

qboolean CPrimitiveTemplate::ParseLengthParm(const char *text)
{
    FX_PARSE_SCALAR_PROPERTY(length.parm);
}

#undef FX_PARSE_SCALAR_PROPERTY
#undef FX_PARSE_VECTOR_PROPERTY

qboolean CPrimitiveTemplate::ParseLengthFlags(const char *text)
{
    int32_t parsedFlags;
    if (ParseGroupFlags(text, &parsedFlags) == qfalse) {
        return qfalse;
    }
    parameterFlags |= (uint32_t)parsedFlags << 12;
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
qboolean CPrimitiveTemplate::ParseRGB(CGPGroup *group)
{
    for (CGPValue *pair = static_cast<CGPValue *>(group->pairs);
         pair != nullptr;
         pair = static_cast<CGPValue *>(pair->next)) {
        const char *value = pair->values != nullptr
            ? pair->values->name
            : nullptr;
        if (value == nullptr) {
            SFxHelper_Print("^1FxTemplate: property '%s' requires a value\n",
                            pair->name);
            return qfalse;
        }

        if (coduo_crt_stricmp(pair->name, "start") == 0) {
            (void)ParseRGBStart(value);
        } else if (coduo_crt_stricmp(pair->name, "end") == 0) {
            (void)ParseRGBEnd(value);
        } else if (coduo_crt_stricmp(pair->name, "parms") == 0 ||
                   coduo_crt_stricmp(pair->name, "parm") == 0) {
            (void)ParseRGBParm(value);
        } else if (coduo_crt_stricmp(pair->name, "flags") == 0 ||
                   coduo_crt_stricmp(pair->name, "flag") == 0) {
            (void)ParseRGBFlags(value);
        } else {
            SFxHelper_Print("^3Unknown key parsing an RGB group: %s\n",
                            pair->name);
        }
    }
    return qtrue;
}

qboolean CPrimitiveTemplate::ParseAlpha(CGPGroup *group)
{
    for (CGPValue *pair = static_cast<CGPValue *>(group->pairs);
         pair != nullptr;
         pair = static_cast<CGPValue *>(pair->next)) {
        const char *value = pair->values != nullptr
            ? pair->values->name
            : nullptr;
        if (value == nullptr) {
            SFxHelper_Print("^1FxTemplate: property '%s' requires a value\n",
                            pair->name);
            return qfalse;
        }

        if (coduo_crt_stricmp(pair->name, "start") == 0) {
            (void)ParseAlphaStart(value);
        } else if (coduo_crt_stricmp(pair->name, "end") == 0) {
            (void)ParseAlphaEnd(value);
        } else if (coduo_crt_stricmp(pair->name, "parms") == 0 ||
                   coduo_crt_stricmp(pair->name, "parm") == 0) {
            (void)ParseAlphaParm(value);
        } else if (coduo_crt_stricmp(pair->name, "flags") == 0 ||
                   coduo_crt_stricmp(pair->name, "flag") == 0) {
            (void)ParseAlphaFlags(value);
        } else {
            SFxHelper_Print("^3Unknown key parsing an Alpha group: %s\n",
                            pair->name);
        }
    }
    return qtrue;
}

qboolean CPrimitiveTemplate::ParseSize(CGPGroup *group)
{
    for (CGPValue *pair = static_cast<CGPValue *>(group->pairs);
         pair != nullptr;
         pair = static_cast<CGPValue *>(pair->next)) {
        const char *value = pair->values != nullptr
            ? pair->values->name
            : nullptr;
        if (value == nullptr) {
            SFxHelper_Print("^1FxTemplate: property '%s' requires a value\n",
                            pair->name);
            return qfalse;
        }

        if (coduo_crt_stricmp(pair->name, "start") == 0) {
            (void)ParseSizeStart(value);
        } else if (coduo_crt_stricmp(pair->name, "end") == 0) {
            (void)ParseSizeEnd(value);
        } else if (coduo_crt_stricmp(pair->name, "parms") == 0 ||
                   coduo_crt_stricmp(pair->name, "parm") == 0) {
            (void)ParseSizeParm(value);
        } else if (coduo_crt_stricmp(pair->name, "flags") == 0 ||
                   coduo_crt_stricmp(pair->name, "flag") == 0) {
            (void)ParseSizeFlags(value);
        } else {
            SFxHelper_Print("^3Unknown key parsing a Size group: %s\n",
                            pair->name);
        }
    }
    return qtrue;
}

qboolean CPrimitiveTemplate::ParseSize2(CGPGroup *group)
{
    for (CGPValue *pair = static_cast<CGPValue *>(group->pairs);
         pair != nullptr;
         pair = static_cast<CGPValue *>(pair->next)) {
        const char *value = pair->values != nullptr
            ? pair->values->name
            : nullptr;
        if (value == nullptr) {
            SFxHelper_Print("^1FxTemplate: property '%s' requires a value\n",
                            pair->name);
            return qfalse;
        }

        if (coduo_crt_stricmp(pair->name, "start") == 0) {
            (void)ParseSize2Start(value);
        } else if (coduo_crt_stricmp(pair->name, "end") == 0) {
            (void)ParseSize2End(value);
        } else if (coduo_crt_stricmp(pair->name, "parms") == 0 ||
                   coduo_crt_stricmp(pair->name, "parm") == 0) {
            (void)ParseSize2Parm(value);
        } else if (coduo_crt_stricmp(pair->name, "flags") == 0 ||
                   coduo_crt_stricmp(pair->name, "flag") == 0) {
            (void)ParseSize2Flags(value);
        } else {
            SFxHelper_Print("^3Unknown key parsing a Size2 group: %s\n",
                            pair->name);
        }
    }
    return qtrue;
}

qboolean CPrimitiveTemplate::ParseLength(CGPGroup *group)
{
    for (CGPValue *pair = static_cast<CGPValue *>(group->pairs);
         pair != nullptr;
         pair = static_cast<CGPValue *>(pair->next)) {
        const char *value = pair->values != nullptr
            ? pair->values->name
            : nullptr;
        if (value == nullptr) {
            SFxHelper_Print("^1FxTemplate: property '%s' requires a value\n",
                            pair->name);
            return qfalse;
        }

        if (coduo_crt_stricmp(pair->name, "start") == 0) {
            (void)ParseLengthStart(value);
        } else if (coduo_crt_stricmp(pair->name, "end") == 0) {
            (void)ParseLengthEnd(value);
        } else if (coduo_crt_stricmp(pair->name, "parms") == 0 ||
                   coduo_crt_stricmp(pair->name, "parm") == 0) {
            (void)ParseLengthParm(value);
        } else if (coduo_crt_stricmp(pair->name, "flags") == 0 ||
                   coduo_crt_stricmp(pair->name, "flag") == 0) {
            (void)ParseLengthFlags(value);
        } else {
            SFxHelper_Print("^3Unknown key parsing a Length group: %s\n",
                            pair->name);
        }
    }
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
qboolean CPrimitiveTemplate::ParseMaterialImpact(const char *text)
{
    if (strlen(text) >= sizeof(materialImpact)) {
        SFxHelper_Print("^1FxTemplate: materialImpact value exceeds field capacity\n");
        return qfalse;
    }

    strcpy(materialImpact, text);
    return qtrue;
}

/* Source: CoDUOMP.exe 0x004ad550..0x004adc00. Same-module Mac symbols identify
 * the seven CGPValue consumers. A value with a next link is the parser's list
 * spelling; a solitary value uses the same registration path but retains the
 * original explicit null-text check. */
qboolean CPrimitiveTemplate::ParseShaders(CGPValue *value)
{
    CGPObject *entry = value->values;
    if (entry == nullptr) {
        SFxHelper_Print(
            "^3CPrimitiveTemplate::ParseShaders called with an empty list!\n");
        return qfalse;
    }

    if (entry->next != nullptr) {
        do {
            fx_template_resource_t resource{};
            resource.handle = SFxHelper_RegisterShader(entry->name);
            resources.push_back(resource);
            entry = entry->next;
        } while (entry != nullptr);
    } else {
        if (entry->name == nullptr) {
            SFxHelper_Print(
                "^3CPrimitiveTemplate::ParseShaders called with an empty list!\n");
            return qfalse;
        }
        fx_template_resource_t resource{};
        resource.handle = SFxHelper_RegisterShader(entry->name);
        resources.push_back(resource);
    }
    return qtrue;
}

qboolean CPrimitiveTemplate::ParseSounds(CGPValue *value)
{
    CGPObject *entry = value->values;
    if (entry == nullptr) {
        SFxHelper_Print(
            "^3CPrimitiveTemplate::ParseSounds called with an empty list!\n");
        return qfalse;
    }

    if (entry->next != nullptr) {
        do {
            fx_template_resource_t resource{};
            resource.handle = SFxHelper_RegisterSound(entry->name);
            resources.push_back(resource);
            entry = entry->next;
        } while (entry != nullptr);
    } else {
        if (entry->name == nullptr) {
            SFxHelper_Print(
                "^3CPrimitiveTemplate::ParseSounds called with an empty list!\n");
            return qfalse;
        }
        fx_template_resource_t resource{};
        resource.handle = SFxHelper_RegisterSound(entry->name);
        resources.push_back(resource);
    }
    return qtrue;
}

qboolean CPrimitiveTemplate::ParseModels(CGPValue *value)
{
    CGPObject *entry = value->values;
    if (entry == nullptr) {
        SFxHelper_Print(
            "^3CPrimitiveTemplate::ParseModels called with an empty list!\n");
        return qfalse;
    }

    if (entry->next != nullptr) {
        do {
            fx_template_resource_t resource{};
            resource.model = SFxHelper_RegisterModel(entry->name);
            resources.push_back(resource);
            entry = entry->next;
        } while (entry != nullptr);
    } else {
        if (entry->name == nullptr) {
            SFxHelper_Print(
                "^3CPrimitiveTemplate::ParseModels called with an empty list!\n");
            return qfalse;
        }
        fx_template_resource_t resource{};
        resource.model = SFxHelper_RegisterModel(entry->name);
        resources.push_back(resource);
    }
    flags |= FX_PRIMITIVE_FLAG_USE_MODEL;
    return qtrue;
}

qboolean CPrimitiveTemplate::ParseImpactFxStrings(CGPValue *value)
{
    CGPObject *entry = value->values;
    if (entry == nullptr) {
        SFxHelper_Print(
            "^3CPrimitiveTemplate::ParseImpactFxStrings called with an empty list!\n");
        return qfalse;
    }

    if (entry->next != nullptr) {
        do {
            const int32_t effectId =
                fxScheduler.RegisterEffect(entry->name, false);
            if (effectId == 0) {
                SFxHelper_Print(
                    "^1FxTemplate: Impact effect file not found.\n");
                return qfalse;
            }
            impactEffects.push_back(effectId);
            entry = entry->next;
        } while (entry != nullptr);
    } else {
        if (entry->name == nullptr) {
            SFxHelper_Print(
                "^3CPrimitiveTemplate::ParseImpactFxStrings called with an empty list!\n");
            return qfalse;
        }
        const int32_t effectId =
            fxScheduler.RegisterEffect(entry->name, false);
        if (effectId == 0) {
            SFxHelper_Print(
                "FxTemplate: Impact effect file not found.\n");
            return qfalse;
        }
        impactEffects.push_back(effectId);
    }
    flags |= FX_PRIMITIVE_FLAG_USE_PHYSICS |
             FX_PRIMITIVE_FLAG_IMPACT_EFFECTS;
    return qtrue;
}

qboolean CPrimitiveTemplate::ParseDeathFxStrings(CGPValue *value)
{
    CGPObject *entry = value->values;
    if (entry == nullptr) {
        SFxHelper_Print(
            "^1CPrimitiveTemplate::ParseDeathFxStrings called with an empty list!\n");
        return qfalse;
    }

    if (entry->next != nullptr) {
        do {
            const int32_t effectId =
                fxScheduler.RegisterEffect(entry->name, false);
            if (effectId == 0) {
                SFxHelper_Print(
                    "^1FxTemplate: Death effect file not found.\n");
                return qfalse;
            }
            deathEffects.push_back(effectId);
            entry = entry->next;
        } while (entry != nullptr);
    } else {
        if (entry->name == nullptr) {
            SFxHelper_Print(
                "^1CPrimitiveTemplate::ParseDeathFxStrings called with an empty list!\n");
            return qfalse;
        }
        const int32_t effectId =
            fxScheduler.RegisterEffect(entry->name, false);
        if (effectId == 0) {
            SFxHelper_Print("^1FxTemplate: Death effect file not found.\n");
            return qfalse;
        }
        deathEffects.push_back(effectId);
    }

    flags |= FX_PRIMITIVE_FLAG_DEATH_EFFECTS;
    return qtrue;
}

qboolean CPrimitiveTemplate::ParseEmitterFxStrings(CGPValue *value)
{
    CGPObject *entry = value->values;
    if (entry == nullptr) {
        SFxHelper_Print(
            "^3CPrimitiveTemplate::ParseEmitterFxStrings called with an empty list!\n");
        return qfalse;
    }

    if (entry->next != nullptr) {
        do {
            const int32_t effectId =
                fxScheduler.RegisterEffect(entry->name, false);
            if (effectId == 0) {
                SFxHelper_Print(
                    "^1FxTemplate: Emitter effect file not found.\n");
                return qfalse;
            }
            emitterEffects.push_back(effectId);
            entry = entry->next;
        } while (entry != nullptr);
    } else {
        if (entry->name == nullptr) {
            SFxHelper_Print(
                "^3CPrimitiveTemplate::ParseEmitterFxStrings called with an empty list!\n");
            return qfalse;
        }
        const int32_t effectId =
            fxScheduler.RegisterEffect(entry->name, false);
        if (effectId == 0) {
            SFxHelper_Print("^1FxTemplate: Emitter effect file not found.\n");
            return qfalse;
        }
        emitterEffects.push_back(effectId);
    }

    flags |= FX_PRIMITIVE_FLAG_EMITTER_EFFECTS;
    return qtrue;
}

qboolean CPrimitiveTemplate::ParsePlayFxStrings(CGPValue *value)
{
    CGPObject *entry = value->values;
    if (entry == nullptr) {
        SFxHelper_Print(
            "^3CPrimitiveTemplate::ParsePlayFxStrings called with an empty list!\n");
        return qfalse;
    }

    if (entry->next != nullptr) {
        do {
            const int32_t effectId =
                fxScheduler.RegisterEffect(entry->name, false);
            if (effectId == 0) {
                SFxHelper_Print("^1FxTemplate: Effect file not found.\n");
                return qfalse;
            }
            playEffects.push_back(effectId);
            entry = entry->next;
        } while (entry != nullptr);
    } else {
        if (entry->name == nullptr) {
            SFxHelper_Print(
                "^3CPrimitiveTemplate::ParsePlayFxStrings called with an empty list!\n");
            return qfalse;
        }
        const int32_t effectId =
            fxScheduler.RegisterEffect(entry->name, false);
        if (effectId == 0) {
            SFxHelper_Print("^1FxTemplate: Effect file not found.\n");
            return qfalse;
        }
        playEffects.push_back(effectId);
    }
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
qboolean CPrimitiveTemplate::ParseFlags(const char *text)
{
    char words[7][MAX_TOKEN_CHARS] = {};
    const int32_t wordCount = sscanf(
        text, "%s %s %s %s %s %s %s",
        words[0], words[1], words[2], words[3],
        words[4], words[5], words[6]);
    qboolean valid = qtrue;

    for (int32_t index = 0; index < 7 && index < wordCount; ++index) {
        const char *word = words[index];
        if (coduo_crt_stricmp(word, "useModel") == 0) {
            flags |= FX_PRIMITIVE_FLAG_USE_MODEL;
        } else if (coduo_crt_stricmp(word, "useBBox") == 0) {
            flags |= FX_PRIMITIVE_FLAG_USE_BBOX;
        } else if (coduo_crt_stricmp(word, "usePhysics") == 0) {
            flags |= FX_PRIMITIVE_FLAG_USE_PHYSICS;
        } else if (coduo_crt_stricmp(word, "expensivePhysics") == 0) {
            /* Recognized legacy spelling; Windows intentionally sets no bit. */
        } else if (coduo_crt_stricmp(word, "impactKills") == 0) {
            flags |= FX_PRIMITIVE_FLAG_IMPACT_KILLS;
        } else if (coduo_crt_stricmp(word, "impactFx") == 0) {
            flags |= FX_PRIMITIVE_FLAG_IMPACT_EFFECTS;
        } else if (coduo_crt_stricmp(word, "deathFx") == 0) {
            flags |= FX_PRIMITIVE_FLAG_DEATH_EFFECTS;
        } else if (coduo_crt_stricmp(word, "useAlpha") == 0) {
            flags |= FX_PRIMITIVE_FLAG_USE_ALPHA;
        } else if (coduo_crt_stricmp(word, "emitFx") == 0) {
            flags |= FX_PRIMITIVE_FLAG_EMITTER_EFFECTS;
        } else if (coduo_crt_stricmp(word, "depthHack") == 0) {
            flags |= FX_PRIMITIVE_FLAG_DEPTH_HACK;
        } else if (coduo_crt_stricmp(word, "relative") == 0) {
            flags |= FX_PRIMITIVE_FLAG_RELATIVE;
        } else if (coduo_crt_stricmp(word, "setShaderTime") == 0) {
            flags |= FX_PRIMITIVE_FLAG_SET_SHADER_TIME;
        } else if (coduo_crt_stricmp(word, "continualLighting") == 0) {
            flags |= FX_PRIMITIVE_FLAG_CONTINUAL_LIGHTING;
        } else if (coduo_crt_stricmp(word, "clampVelocity") == 0) {
            flags |= FX_PRIMITIVE_FLAG_CLAMP_VELOCITY;
        } else if (coduo_crt_stricmp(word, "clampVelocityX") == 0) {
            flags |= FX_PRIMITIVE_FLAG_CLAMP_VELOCITY_X;
        } else if (coduo_crt_stricmp(word, "clampVelocityY") == 0) {
            flags |= FX_PRIMITIVE_FLAG_CLAMP_VELOCITY_Y;
        } else if (coduo_crt_stricmp(word, "clampVelocityZ") == 0) {
            flags |= FX_PRIMITIVE_FLAG_CLAMP_VELOCITY_Z;
        } else if (coduo_crt_stricmp(word, "clampRotation") == 0) {
            flags |= FX_PRIMITIVE_FLAG_CLAMP_ROTATION;
        } else {
            valid = qfalse;
        }
    }
    return valid;
}

qboolean CPrimitiveTemplate::ParseSpawnFlags(const char *text)
{
    char words[7][MAX_TOKEN_CHARS] = {};
    const int32_t wordCount = sscanf(
        text, "%s %s %s %s %s %s %s",
        words[0], words[1], words[2], words[3],
        words[4], words[5], words[6]);
    qboolean valid = qtrue;

    for (int32_t index = 0; index < 7 && index < wordCount; ++index) {
        const char *word = words[index];
        if (coduo_crt_stricmp(word, "org2fromTrace") == 0) {
            spawnFlags |= FX_SPAWNFLAG_ORIGIN2_FROM_TRACE;
        } else if (coduo_crt_stricmp(word, "traceImpactFx") == 0) {
            spawnFlags |= FX_SPAWNFLAG_TRACE_IMPACT_EFFECT;
        } else if (coduo_crt_stricmp(word, "org2isOffset") == 0) {
            spawnFlags |= FX_SPAWNFLAG_ORIGIN2_IS_OFFSET;
        } else if (coduo_crt_stricmp(word, "cheapOrgCalc") == 0) {
            spawnFlags |= FX_SPAWNFLAG_CHEAP_ORIGIN;
        } else if (coduo_crt_stricmp(word, "cheapOrg2Calc") == 0) {
            spawnFlags |= FX_SPAWNFLAG_CHEAP_ORIGIN2;
        } else if (coduo_crt_stricmp(word, "absoluteVel") == 0) {
            spawnFlags |= FX_SPAWNFLAG_ABSOLUTE_VELOCITY;
        } else if (coduo_crt_stricmp(word, "absoluteAccel") == 0) {
            spawnFlags |= FX_SPAWNFLAG_ABSOLUTE_ACCELERATION;
        } else if (coduo_crt_stricmp(word, "oppositeAccel") == 0) {
            spawnFlags |= FX_SPAWNFLAG_OPPOSITE_ACCELERATION;
        } else if (coduo_crt_stricmp(word, "oppositeRotation") == 0) {
            spawnFlags |= FX_SPAWNFLAG_OPPOSITE_ROTATION;
        } else if (coduo_crt_stricmp(word, "orgOnSphere") == 0) {
            spawnFlags |= FX_SPAWNFLAG_ORIGIN_ON_SPHERE;
        } else if (coduo_crt_stricmp(word, "orgOnCylinder") == 0) {
            spawnFlags |= FX_SPAWNFLAG_ORIGIN_ON_CYLINDER;
        } else if (coduo_crt_stricmp(word, "axisFromSphere") == 0) {
            spawnFlags |= FX_SPAWNFLAG_AXIS_FROM_SPHERE;
        } else if (coduo_crt_stricmp(word, "randrotaroundfwd") == 0) {
            spawnFlags |= FX_SPAWNFLAG_RANDOM_ROTATION_AROUND_FORWARD;
        } else if (coduo_crt_stricmp(word, "evenDistribution") == 0) {
            spawnFlags |= FX_SPAWNFLAG_EVEN_DISTRIBUTION;
        } else if (coduo_crt_stricmp(
                       word, "rgbComponentInterpolation") == 0) {
            spawnFlags |= FX_SPAWNFLAG_RGB_COMPONENT_INTERPOLATION;
        } else if (coduo_crt_stricmp(word, "affectedByWind") == 0) {
            spawnFlags |= FX_SPAWNFLAG_AFFECTED_BY_WIND;
        } else if (coduo_crt_stricmp(word, "lessAttenuation") == 0) {
            spawnFlags |= FX_SPAWNFLAG_LESS_ATTENUATION;
        } else {
            valid = qfalse;
        }
    }
    return valid;
}

/* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
qboolean CPrimitiveTemplate::ParsePrimitive(CGPGroup *group)
{
    for (CGPValue *pair = static_cast<CGPValue *>(group->pairs);
         pair != nullptr;
         pair = static_cast<CGPValue *>(pair->next)) {
        const char *key = pair->name;
        const char *value = pair->values != nullptr
            ? pair->values->name
            : nullptr;
        if (value == nullptr) {
            SFxHelper_Print("^1FxTemplate: property '%s' requires a value\n",
                            key);
            return qfalse;
        }

        if (coduo_crt_stricmp(key, "count") == 0) {
            (void)ParseCount(value);
        } else if (coduo_crt_stricmp(key, "shaders") == 0 ||
                   coduo_crt_stricmp(key, "shader") == 0) {
            (void)ParseShaders(pair);
        } else if (coduo_crt_stricmp(key, "models") == 0 ||
                   coduo_crt_stricmp(key, "model") == 0) {
            (void)ParseModels(pair);
        } else if (coduo_crt_stricmp(key, "sounds") == 0 ||
                   coduo_crt_stricmp(key, "sound") == 0) {
            (void)ParseSounds(pair);
        } else if (coduo_crt_stricmp(key, "impactfx") == 0) {
            (void)ParseImpactFxStrings(pair);
        } else if (coduo_crt_stricmp(key, "deathfx") == 0) {
            (void)ParseDeathFxStrings(pair);
        } else if (coduo_crt_stricmp(key, "emitfx") == 0) {
            (void)ParseEmitterFxStrings(pair);
        } else if (coduo_crt_stricmp(key, "playfx") == 0) {
            (void)ParsePlayFxStrings(pair);
        } else if (coduo_crt_stricmp(key, "life") == 0) {
            (void)ParseLife(value);
        } else if (coduo_crt_stricmp(key, "cullrange") == 0) {
            cullRange = coduo_crt_atoi(value);
        } else if (coduo_crt_stricmp(key, "delay") == 0) {
            (void)ParseDelay(value);
        } else if (coduo_crt_stricmp(key, "bounce") == 0 ||
                   coduo_crt_stricmp(key, "intensity") == 0) {
            (void)ParseElasticity(value);
        } else if (coduo_crt_stricmp(key, "min") == 0) {
            (void)ParseMin(value);
        } else if (coduo_crt_stricmp(key, "max") == 0) {
            (void)ParseMax(value);
        } else if (coduo_crt_stricmp(key, "angle") == 0 ||
                   coduo_crt_stricmp(key, "angles") == 0) {
            (void)ParseAngle(value);
        } else if (coduo_crt_stricmp(key, "angleDelta") == 0) {
            (void)ParseAngleDelta(value);
        } else if (coduo_crt_stricmp(key, "velocity") == 0 ||
                   coduo_crt_stricmp(key, "vel") == 0) {
            (void)ParseVelocity(value);
        } else if (coduo_crt_stricmp(key, "velocityClamp") == 0 ||
                   coduo_crt_stricmp(key, "vel") == 0) {
            /* The second `vel` comparison is present in Windows but is
             * unreachable because the velocity alias above matches first. */
            (void)ParseVelocityClamp(value);
        } else if (coduo_crt_stricmp(key, "acceleration") == 0 ||
                   coduo_crt_stricmp(key, "accel") == 0) {
            (void)ParseAcceleration(value);
        } else if (coduo_crt_stricmp(key, "gravity") == 0) {
            (void)ParseGravity(value);
        } else if (coduo_crt_stricmp(key, "density") == 0) {
            (void)ParseDensity(value);
        } else if (coduo_crt_stricmp(key, "variance") == 0) {
            (void)ParseVariance(value);
        } else if (coduo_crt_stricmp(key, "origin") == 0) {
            (void)ParseOrigin1(value);
        } else if (coduo_crt_stricmp(key, "origin2") == 0) {
            (void)ParseOrigin2(value);
        } else if (coduo_crt_stricmp(key, "radius") == 0) {
            (void)ParseRadius(value);
        } else if (coduo_crt_stricmp(key, "height") == 0) {
            (void)ParseHeight(value);
        } else if (coduo_crt_stricmp(key, "wind") == 0) {
            (void)ParseWindModifier(value);
        } else if (coduo_crt_stricmp(key, "rotation") == 0) {
            (void)ParseRotation(value);
        } else if (Q_stricmp(key, "rotationDelta") == 0) {
            (void)ParseRotationDelta(value);
        } else if (Q_stricmp(key, "rotationAccel") == 0) {
            (void)ParseRotationAccel(value);
        } else if (Q_stricmp(key, "rotationClamp") == 0) {
            (void)ParseRotationClamp(value);
        } else if (coduo_crt_stricmp(key, "flags") == 0 ||
                   coduo_crt_stricmp(key, "flag") == 0) {
            (void)ParseFlags(value);
        } else if (coduo_crt_stricmp(key, "spawnFlags") == 0 ||
                   coduo_crt_stricmp(key, "spawnFlag") == 0) {
            (void)ParseSpawnFlags(value);
        } else if (coduo_crt_stricmp(key, "nonUniformScale") == 0) {
            nonUniformScale = coduo_crt_atoi(value) != 0;
        } else if (coduo_crt_stricmp(key, "name") == 0) {
            if (value != nullptr) {
                /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
                if (strlen(value) >= sizeof(name)) {
                    SFxHelper_Print("^1FxTemplate: primitive name exceeds field capacity\n");
                    return qfalse;
                }
                strcpy(name, value);
            }
        } else if (coduo_crt_stricmp(key, "materialImpact") == 0) {
            if (ParseMaterialImpact(value) == qfalse) {
                return qfalse;
            }
        } else {
            SFxHelper_Print(
                "^3Unknown key parsing an effect primitive: %s\n", key);
        }
    }

    for (CGPGroup *child =
             static_cast<CGPGroup *>(group->subGroups);
         child != nullptr;
        child = static_cast<CGPGroup *>(child->next)) {
        if (coduo_crt_stricmp(child->name, "rgb") == 0) {
            if (ParseRGB(child) == qfalse) {
                return qfalse;
            }
        } else if (coduo_crt_stricmp(child->name, "alpha") == 0) {
            if (ParseAlpha(child) == qfalse) {
                return qfalse;
            }
        } else if (coduo_crt_stricmp(child->name, "size") == 0 ||
                   coduo_crt_stricmp(child->name, "width") == 0) {
            if (ParseSize(child) == qfalse) {
                return qfalse;
            }
        } else if (coduo_crt_stricmp(child->name, "size2") == 0 ||
                   coduo_crt_stricmp(child->name, "width2") == 0) {
            if (ParseSize2(child) == qfalse) {
                return qfalse;
            }
        } else if (coduo_crt_stricmp(child->name, "length") == 0 ||
                   coduo_crt_stricmp(child->name, "height") == 0) {
            if (ParseLength(child) == qfalse) {
                return qfalse;
            }
        } else {
            SFxHelper_Print(
                "^3Unknown group key parsing a particle: %s\n",
                child->name);
        }
    }
    return qtrue;
}
