#ifndef CODUOMP_FX_PRIMITIVE_TEMPLATE_HPP
#define CODUOMP_FX_PRIMITIVE_TEMPLATE_HPP

#include "../q_shared.h"
#include "../parser/generic_parser.hpp"

#include <cstddef>
#include <stdint.h>
#include <vector>

struct DObj_s;
struct fx_pool_allocator_s;
typedef struct fx_pool_allocator_s fx_pool_allocator_t;

enum fx_parameter_flag_e {
    FX_PARAMETER_LINEAR = 0x1,
    FX_PARAMETER_RANDOM = 0x2,
    FX_PARAMETER_NONLINEAR = 0x4,
    FX_PARAMETER_WAVE = 0x8,
    /* The original parser represents "clamp" as the combined nonlinear and
     * wave bits rather than a separate flag. */
    FX_PARAMETER_CLAMP = FX_PARAMETER_NONLINEAR | FX_PARAMETER_WAVE
};

/* Source primitive kinds parsed by CFxScheduler::ParseEffect and dispatched
 * by CFxScheduler::CreateEffect.  These are distinct from the runtime
 * CEffect::TypeID values: sound, decal, FX-runner, and camera-shake templates
 * dispatch outside the maintained CEffect class hierarchy. */
enum fx_primitive_type_e {
    FX_PRIMITIVE_TYPE_PARTICLE = 1,
    FX_PRIMITIVE_TYPE_LINE = 2,
    FX_PRIMITIVE_TYPE_TAIL = 3,
    FX_PRIMITIVE_TYPE_CYLINDER = 4,
    FX_PRIMITIVE_TYPE_EMITTER = 5,
    FX_PRIMITIVE_TYPE_SOUND = 6,
    FX_PRIMITIVE_TYPE_DECAL = 7,
    FX_PRIMITIVE_TYPE_ORIENTED_PARTICLE = 8,
    FX_PRIMITIVE_TYPE_ELECTRICITY = 9,
    FX_PRIMITIVE_TYPE_FX_RUNNER = 10,
    FX_PRIMITIVE_TYPE_LIGHT = 11,
    FX_PRIMITIVE_TYPE_CAMERA_SHAKE = 12,
    FX_PRIMITIVE_TYPE_FLASH = 13
};

enum fx_primitive_flag_e {
    FX_PRIMITIVE_FLAG_CLAMP_VELOCITY_X = 0x00000001,
    FX_PRIMITIVE_FLAG_CLAMP_VELOCITY_Y = 0x00000002,
    FX_PRIMITIVE_FLAG_CLAMP_VELOCITY_Z = 0x00000004,
    FX_PRIMITIVE_FLAG_CLAMP_ROTATION = 0x00000008,
    FX_PRIMITIVE_FLAG_DEPTH_HACK = 0x00100000,
    FX_PRIMITIVE_FLAG_RELATIVE = 0x00200000,
    FX_PRIMITIVE_FLAG_SET_SHADER_TIME = 0x00400000,
    FX_PRIMITIVE_FLAG_CONTINUAL_LIGHTING = 0x00800000,
    FX_PRIMITIVE_FLAG_USE_MODEL = 0x01000000,
    FX_PRIMITIVE_FLAG_USE_PHYSICS = 0x02000000,
    FX_PRIMITIVE_FLAG_USE_BBOX = 0x04000000,
    FX_PRIMITIVE_FLAG_USE_ALPHA = 0x08000000,
    FX_PRIMITIVE_FLAG_EMITTER_EFFECTS = 0x10000000,
    FX_PRIMITIVE_FLAG_DEATH_EFFECTS = 0x20000000,
    FX_PRIMITIVE_FLAG_IMPACT_KILLS = 0x40000000,
    FX_PRIMITIVE_FLAG_IMPACT_EFFECTS = 0x80000000,
    FX_PRIMITIVE_FLAG_CLAMP_VELOCITY =
        FX_PRIMITIVE_FLAG_CLAMP_VELOCITY_X |
        FX_PRIMITIVE_FLAG_CLAMP_VELOCITY_Y |
        FX_PRIMITIVE_FLAG_CLAMP_VELOCITY_Z,
    FX_PRIMITIVE_FLAG_BOUNDS =
        FX_PRIMITIVE_FLAG_USE_PHYSICS | FX_PRIMITIVE_FLAG_USE_BBOX
};

enum fx_spawn_flag_e {
    FX_SPAWNFLAG_ORIGIN_ON_SPHERE = 0x00000001,
    FX_SPAWNFLAG_AXIS_FROM_SPHERE = 0x00000002,
    FX_SPAWNFLAG_ORIGIN_ON_CYLINDER = 0x00000004,
    FX_SPAWNFLAG_ORIGIN2_FROM_TRACE = 0x00000010,
    FX_SPAWNFLAG_TRACE_IMPACT_EFFECT = 0x00000020,
    FX_SPAWNFLAG_ORIGIN2_IS_OFFSET = 0x00000040,
    FX_SPAWNFLAG_CHEAP_ORIGIN = 0x00000100,
    FX_SPAWNFLAG_CHEAP_ORIGIN2 = 0x00000200,
    FX_SPAWNFLAG_ABSOLUTE_VELOCITY = 0x00000400,
    FX_SPAWNFLAG_ABSOLUTE_ACCELERATION = 0x00000800,
    FX_SPAWNFLAG_RANDOM_ROTATION_AROUND_FORWARD = 0x00001000,
    FX_SPAWNFLAG_EVEN_DISTRIBUTION = 0x00002000,
    FX_SPAWNFLAG_RGB_COMPONENT_INTERPOLATION = 0x00004000,
    FX_SPAWNFLAG_AFFECTED_BY_WIND = 0x00010000,
    FX_SPAWNFLAG_LESS_ATTENUATION = 0x00020000,
    FX_SPAWNFLAG_OPPOSITE_ACCELERATION = 0x00040000,
    FX_SPAWNFLAG_OPPOSITE_ROTATION = 0x00080000
};

/* The original random-range helpers read two adjacent float32 endpoints at
 * +0x00/+0x04. Template parsers and CreateEffect prove the nested vector and
 * animation-parameter ordering below. */
typedef struct fx_float_range_s {
    float start;
    float end;
} fx_float_range_t;

typedef struct fx_vector_range_s {
    vec3_t start;
    vec3_t end;
} fx_vector_range_t;

typedef struct fx_float_parameter_s {
    fx_float_range_t start;
    fx_float_range_t end;
    /* Exact authored `Parm` slot: a mode-dependent channel parameter, or the
     * sole sampled scalar for properties such as elasticity. */
    fx_float_range_t parm;
} fx_float_parameter_t;

typedef struct fx_vector_parameter_s {
    fx_vector_range_t start;
    fx_vector_range_t end;
    /* Exact authored `Parm` slot; interpolation flags decide whether the
     * sampled value is an absolute time, lifetime percentage, or curve scale. */
    fx_float_range_t parm;
} fx_vector_parameter_t;

/* Primitive type determines whether the shared original resource dword is a
 * renderer/sound handle or a model pointer. Native builds widen only the
 * pointer-bearing alternative instead of narrowing it through int32_t. */
typedef union fx_template_resource_u {
    int32_t handle;
    DObj_s *model;
} fx_template_resource_t;

#if UINTPTR_MAX == UINT32_MAX
static_assert(alignof(fx_float_range_t) == 0x04,
              "i386 FX float-range alignment changed");
static_assert(offsetof(fx_float_range_t, start) == 0x00,
              "i386 FX float-range start offset changed");
static_assert(sizeof(((fx_float_range_t *)0)->start) == 0x04,
              "i386 FX float-range start extent changed");
static_assert(offsetof(fx_float_range_t, end) == 0x04,
              "i386 FX float-range end offset changed");
static_assert(sizeof(((fx_float_range_t *)0)->end) == 0x04,
              "i386 FX float-range end extent changed");
static_assert(sizeof(fx_float_range_t) == 0x08,
              "i386 FX float-range size changed");

static_assert(alignof(fx_vector_range_t) == 0x04,
              "i386 FX vector-range alignment changed");
static_assert(offsetof(fx_vector_range_t, start) == 0x00,
              "i386 FX vector-range start offset changed");
static_assert(sizeof(((fx_vector_range_t *)0)->start) == 0x0c,
              "i386 FX vector-range start extent changed");
static_assert(offsetof(fx_vector_range_t, end) == 0x0c,
              "i386 FX vector-range end offset changed");
static_assert(sizeof(((fx_vector_range_t *)0)->end) == 0x0c,
              "i386 FX vector-range end extent changed");
static_assert(sizeof(fx_vector_range_t) == 0x18,
              "i386 FX vector-range size changed");

static_assert(alignof(fx_float_parameter_t) == 0x04,
              "i386 FX float-parameter alignment changed");
static_assert(offsetof(fx_float_parameter_t, start) == 0x00,
              "i386 FX float-parameter start offset changed");
static_assert(sizeof(((fx_float_parameter_t *)0)->start) == 0x08,
              "i386 FX float-parameter start extent changed");
static_assert(offsetof(fx_float_parameter_t, end) == 0x08,
              "i386 FX float-parameter end offset changed");
static_assert(sizeof(((fx_float_parameter_t *)0)->end) == 0x08,
              "i386 FX float-parameter end extent changed");
static_assert(offsetof(fx_float_parameter_t, parm) == 0x10,
              "i386 FX float-parameter curve offset changed");
static_assert(sizeof(((fx_float_parameter_t *)0)->parm) == 0x08,
              "i386 FX float-parameter curve extent changed");
static_assert(sizeof(fx_float_parameter_t) == 0x18,
              "i386 FX float-parameter size changed");

static_assert(alignof(fx_vector_parameter_t) == 0x04,
              "i386 FX vector-parameter alignment changed");
static_assert(offsetof(fx_vector_parameter_t, start) == 0x00,
              "i386 FX vector-parameter start offset changed");
static_assert(sizeof(((fx_vector_parameter_t *)0)->start) == 0x18,
              "i386 FX vector-parameter start extent changed");
static_assert(offsetof(fx_vector_parameter_t, end) == 0x18,
              "i386 FX vector-parameter end offset changed");
static_assert(sizeof(((fx_vector_parameter_t *)0)->end) == 0x18,
              "i386 FX vector-parameter end extent changed");
static_assert(offsetof(fx_vector_parameter_t, parm) == 0x30,
              "i386 FX vector-parameter curve offset changed");
static_assert(sizeof(((fx_vector_parameter_t *)0)->parm) == 0x08,
              "i386 FX vector-parameter curve extent changed");
static_assert(sizeof(fx_vector_parameter_t) == 0x38,
              "i386 FX vector-parameter size changed");

static_assert(alignof(fx_template_resource_t) == 0x04,
              "i386 FX template-resource alignment changed");
static_assert(offsetof(fx_template_resource_t, handle) == 0x00,
              "i386 FX template handle offset changed");
static_assert(sizeof(((fx_template_resource_t *)0)->handle) == 0x04,
              "i386 FX template handle extent changed");
static_assert(offsetof(fx_template_resource_t, model) == 0x00,
              "i386 FX template model offset changed");
static_assert(sizeof(((fx_template_resource_t *)0)->model) == 0x04,
              "i386 FX template model extent changed");
static_assert(sizeof(fx_template_resource_t) == 0x04,
              "i386 FX template-resource size changed");
#endif

/* Same-module Mac symbols prove CPrimitiveTemplate and the member bindings.
 * Windows 0x004aad00 and the parser writers prove this complete semantic
 * order. On the original MSVC i386 build each vector occupies 16 bytes, giving
 * the 0x288-byte object and the offsets cited beside the grouped fields. Native
 * 64-bit builds intentionally use their platform's pointer/vector layout. */
class CPrimitiveTemplate {
public:
    CPrimitiveTemplate();
    ~CPrimitiveTemplate();

    static void *operator new(size_t size) noexcept;
    static void operator delete(void *allocation) noexcept;

    void CopyForTemporaryEffect(const CPrimitiveTemplate &source);
    /* NOT_FROM_ORIGINAL_SOURCE: balances one pending spawn owned by a
     * temporary clone, deleting the clone after its final outcome. */
    void coduomp_retire_pending_spawn();

    qboolean ParseVector(const char *text, float *start, float *end);
    qboolean ParseGroupFlags(const char *text, int32_t *flags);
    qboolean ParseMin(const char *text);
    qboolean ParseMax(const char *text);
    qboolean ParseLife(const char *text);
    qboolean ParseDelay(const char *text);
    qboolean ParseElasticity(const char *text);
    qboolean ParseOrigin1(const char *text);
    qboolean ParseOrigin2(const char *text);
    qboolean ParseRadius(const char *text);
    qboolean ParseHeight(const char *text);
    qboolean ParseWindModifier(const char *text);
    qboolean ParseRotation(const char *text);
    qboolean ParseRotationDelta(const char *text);
    qboolean ParseRotationAccel(const char *text);
    qboolean ParseRotationClamp(const char *text);
    qboolean ParseAngle(const char *text);
    qboolean ParseAngleDelta(const char *text);
    qboolean ParseVelocity(const char *text);
    qboolean ParseVelocityClamp(const char *text);
    qboolean ParseAcceleration(const char *text);
    qboolean ParseGravity(const char *text);
    qboolean ParseDensity(const char *text);
    qboolean ParseVariance(const char *text);
    qboolean ParseRGBStart(const char *text);
    qboolean ParseRGBEnd(const char *text);
    qboolean ParseRGBParm(const char *text);
    qboolean ParseRGBFlags(const char *text);
    qboolean ParseAlphaStart(const char *text);
    qboolean ParseAlphaEnd(const char *text);
    qboolean ParseAlphaParm(const char *text);
    qboolean ParseAlphaFlags(const char *text);
    qboolean ParseSizeStart(const char *text);
    qboolean ParseSizeEnd(const char *text);
    qboolean ParseSizeParm(const char *text);
    qboolean ParseSizeFlags(const char *text);
    qboolean ParseSize2Start(const char *text);
    qboolean ParseSize2End(const char *text);
    qboolean ParseSize2Parm(const char *text);
    qboolean ParseSize2Flags(const char *text);
    qboolean ParseLengthStart(const char *text);
    qboolean ParseLengthEnd(const char *text);
    qboolean ParseLengthParm(const char *text);
    qboolean ParseLengthFlags(const char *text);
    qboolean ParseRGB(CGPGroup *group);
    qboolean ParseAlpha(CGPGroup *group);
    qboolean ParseSize(CGPGroup *group);
    qboolean ParseSize2(CGPGroup *group);
    qboolean ParseLength(CGPGroup *group);
    qboolean ParseMaterialImpact(const char *text);
    qboolean ParseShaders(CGPValue *value);
    qboolean ParseSounds(CGPValue *value);
    qboolean ParseModels(CGPValue *value);
    qboolean ParseImpactFxStrings(CGPValue *value);
    qboolean ParseDeathFxStrings(CGPValue *value);
    qboolean ParseEmitterFxStrings(CGPValue *value);
    qboolean ParsePlayFxStrings(CGPValue *value);
    qboolean ParseFlags(const char *text);
    qboolean ParseSpawnFlags(const char *text);
    qboolean ParsePrimitive(CGPGroup *group);
    qboolean ParseCount(const char *text);

private:
    friend class CFxScheduler;

    qboolean ParseFloat(const char *text, float *start, float *end);

    bool temporary;                     /* +0x000 */
    [[maybe_unused]] uint8_t reservedHeaderPadding[3]; /* +0x001: alignment */
    int32_t remainingSpawnCount;        /* +0x004 */
    char name[32];                      /* +0x008 */
    char materialImpact[32];            /* +0x028 */
    int32_t primitiveType;                 /* +0x048: set by derived templates */
    fx_float_range_t delay;             /* +0x04c */
    fx_float_range_t count;             /* +0x054 */
    fx_float_range_t life;              /* +0x05c */
    int32_t cullRange;                  /* +0x064 */
    std::vector<fx_template_resource_t> resources; /* +0x068 */
    std::vector<int32_t> impactEffects; /* +0x078 */
    std::vector<int32_t> deathEffects;  /* +0x088 */
    std::vector<int32_t> emitterEffects;/* +0x098 */
    std::vector<int32_t> playEffects;   /* +0x0a8 */
    uint32_t flags;                     /* +0x0b8 */
    uint32_t parameterFlags;            /* +0x0bc */
    uint32_t spawnFlags;                /* +0x0c0 */
    bool nonUniformScale;               /* +0x0c4 */
    [[maybe_unused]] uint8_t reservedNonUniformPadding[3]; /* +0x0c5: alignment only */
    vec3_t boundsMin;                   /* +0x0c8 */
    vec3_t boundsMax;                   /* +0x0d4 */
    fx_vector_range_t origin1;          /* +0x0e0 */
    fx_vector_range_t origin2;          /* +0x0f8 */
    fx_float_range_t radius;            /* +0x110 */
    fx_float_range_t height;            /* +0x118 */
    fx_float_range_t windModifier;      /* +0x120 */
    fx_float_range_t rotation;          /* +0x128 */
    fx_float_range_t rotationClamp;     /* +0x130 */
    fx_float_range_t rotationDelta;     /* +0x138 */
    fx_float_range_t rotationAccel;     /* +0x140 */
    fx_vector_range_t angle;            /* +0x148 */
    fx_vector_range_t angleDelta;       /* +0x160 */
    fx_vector_range_t velocity;         /* +0x178 */
    fx_vector_range_t velocityClamp;    /* +0x190 */
    fx_vector_range_t acceleration;     /* +0x1a8 */
    fx_float_range_t gravity;           /* +0x1c0 */
    fx_float_range_t density;           /* +0x1c8 */
    fx_float_range_t variance;          /* +0x1d0 */
    fx_vector_parameter_t rgb;          /* +0x1d8 */
    fx_float_parameter_t alpha;         /* +0x210 */
    fx_float_parameter_t size;          /* +0x228 */
    fx_float_parameter_t size2;         /* +0x240 */
    fx_float_parameter_t length;        /* +0x258 */
    fx_float_parameter_t elasticity;    /* +0x270 */
};

extern fx_pool_allocator_t fxPrimitiveTemplateAllocator; /* 0x0389ffe8 */

#endif
