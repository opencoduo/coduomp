#ifndef CODUOMP_FX_CLASSES_HPP
#define CODUOMP_FX_CLASSES_HPP

#include "fx_api.h"
#include "fx_memory.h"

#include <cstddef>

class CEffect {
public:
    CEffect();
    virtual ~CEffect();
    static void operator delete(void *allocation) noexcept;
    virtual void Die();
    virtual qboolean Update();
    virtual qboolean Cull();
    virtual void Draw();
    virtual int32_t TypeID();
    virtual void Archive(fx_archive_t &archive);

    void SetShaderTexCoord(float s, float t);
    void SetTraceMins(const vec3_t value);
    void SetTraceMaxs(const vec3_t value);
    void SetFlags(uint32_t value);
    void ClearFlags(uint32_t mask);
    void SetLerpFlags(int32_t value);
    void SetOrigin(const vec3_t value);
    void SetLightingOrigin(const vec3_t value);
    int32_t GetTimeStart() const;
    void SetTimeStart(int32_t value);
    void SetTimeEnd(int32_t value);
    void SetImpactEffectID(int32_t effectId);
    void SetDeathEffectID(int32_t effectId);

    vec3_t origin;
    int32_t timeStart;
    int32_t timeEnd;
    uint32_t flags;
    int32_t lerpFlags;
    vec3_t traceMins;
    vec3_t traceMaxs;
    /* These two original dwords are role-dependent FX resource references.
     * Model-bearing effects consume the pointer member; particle collision
     * paths consume the integer effect id.  A union preserves that proven
     * overlap without narrowing a native pointer on 64-bit hosts. */
    union resource_reference_t {
        DObj *model;
        int32_t effectId;
    } impactResource, deathResource;
    refEntity_t renderEntity;
    cfx_bolt_frame_ptr_t boltFrame;
};

/* Particle animation channels serialized by CParticle::Archive. The Windows
 * accesses prove the field widths and roles; exact original member spellings
 * are unavailable from the Mac traceback symbols. */
class CParticle : public CEffect {
public:
    CParticle();
    ~CParticle() override;
    static void *operator new(size_t size) noexcept;
    static void operator delete(void *allocation) noexcept;
    void Die() override;
    qboolean Update() override;
    qboolean Cull() override;
    void Draw() override;
    int32_t TypeID() override;
    void Archive(fx_archive_t &archive) override;

    qboolean UpdateOrigin();
    void SetVelocity(const vec3_t value);
    void SetVelocityGoal(const vec3_t value);
    void SetAcceleration(const vec3_t value);
    void SetSizeStart(float value);
    void SetSizeEnd(float value);
    void SetSizeTime(float value);
    void SetSize2Start(float value);
    void SetSize2End(float value);
    void SetSize2Time(float value);
    void SetColorStart(const vec3_t value);
    void SetColorEnd(const vec3_t value);
    void SetColorTime(float value);
    void SetAlphaStart(float value);
    void SetAlphaEnd(float value);
    void SetAlphaTime(float value);
    void SetRotation(float value);
    void SetRotationGoal(float value);
    void SetRotationVelocity(float value);
    void SetRotationAcceleration(float value);
    void SetElasticity(float value);
    void SetSpriteShaderHandle(int32_t value);
    void UpdateVelocity();
    void UpdateSize();
    void UpdateSize2();
    void UpdateRGB();
    void UpdateAlpha();
    void UpdateRotation();

    vec3_t particleVelocity;
    vec3_t velocityGoal;
    /* CPrimitiveTemplate::CreateEffect supplies its acceleration vector here;
     * UpdateOrigin integrates it as 0.5*a*dt^2 and UpdateVelocity as a*dt. */
    vec3_t acceleration;
    float sizeStart;
    float sizeEnd;
    float sizeTime;
    float size2Start;
    float size2End;
    float size2Time;
    vec3_t colorStart;
    vec3_t colorEnd;
    float colorTime;
    float alphaStart;
    float alphaEnd;
    float alphaTime;
    float rotationGoal;
    float rotationVelocity;
    float rotationAcceleration;
    /* Collision reflection multiplier sampled from the template elasticity. */
    float elasticity;
};

#define DECLARE_PARTICLE_EFFECT_CLASS(name_) \
    class name_ : public CParticle { \
    public: \
        name_(); \
        ~name_() override; \
        static void *operator new(size_t size) noexcept; \
        static void operator delete(void *allocation) noexcept; \
        qboolean Update() override; \
        qboolean Cull() override; \
        void Draw() override; \
        int32_t TypeID() override; \
        void Archive(fx_archive_t &archive) override;

DECLARE_PARTICLE_EFFECT_CLASS(COrientedParticle)
    void SetOrientation(const vec3_t value);
    vec3_t orientation;
};

DECLARE_PARTICLE_EFFECT_CLASS(CLine)
    void Die() override;
    void SetEnd(const vec3_t value);
    vec3_t end;
};

DECLARE_PARTICLE_EFFECT_CLASS(CElectricity)
    void Die() override;
    void Initialize();
    void SetElectricityParm(float value);
    vec3_t end;
    /* Float passed by FX_AddElectricity and forwarded to renderer state +0x84.
     * Its exact original source name is not present in the symbol bank. */
    float electricityParm;
    /* Raw renderer-consumed electricity segment payload. Its 384-byte extent
     * is archived verbatim and is therefore an honest packed boundary. */
    uint8_t rendererSegmentData[384];
};

DECLARE_PARTICLE_EFFECT_CLASS(CTail)
    void UpdateLength();
    void CalcNewEndpoint();
    void SetLengthStart(float value);
    void SetLengthEnd(float value);
    void SetLengthTime(float value);
    vec3_t end;
    float lengthStart;
    float lengthEnd;
    float lengthTime;
    float currentLength;
};

class CCylinder : public CTail {
public:
    CCylinder();
    ~CCylinder() override;
    static void *operator new(size_t size) noexcept;
    static void operator delete(void *allocation) noexcept;
    qboolean Update() override;
    qboolean Cull() override;
    void Draw() override;
    int32_t TypeID() override;
    void Archive(fx_archive_t &archive) override;

    void SetSecondarySizeStart(float value);
    void SetSecondarySizeEnd(float value);
    void SetSecondarySizeTime(float value);
    void SetDirection(const vec3_t value);
};

DECLARE_PARTICLE_EFFECT_CLASS(CEmitter)
    void SetEmitterOrigin(const vec3_t value);
    void SetEmitterVelocity(const vec3_t value);
    void SetEmitterBoltOrigin(const vec3_t value);
    void SetModel(DObj *value);
    void SetAngles(const vec3_t value);
    void SetAngularVelocity(const vec3_t value);
    void SetEmitterEffectID(int32_t value);
    void SetDensity(float value);
    void SetVariance(float value);
    void SetLastEmitTime(int32_t value);
    void RandomizeDensity();
    void UpdateAngles();
    vec3_t emitterOrigin;
    vec3_t emitterVelocity;
    /* World-space bolt translation tracked separately from the emitter's
     * local ballistic origin. It is zero for unbolted emitters. */
    vec3_t emitterBoltOrigin;
    /* The emitter update advances lastEmitTime in 12 ms samples and rejects
     * samples closer than currentDensity to the previous emitted point. */
    int32_t lastEmitTime;
    float currentDensity;
    vec3_t angles;
    vec3_t angularVelocity;
    int32_t emitterEffectId;
    float density;
    float variance;
};

/* One corner of the Windows-only type-14 quad. Every field is proved by the
 * interpolation loop at 0x004a4500 and the two-triangle draw at 0x004a41a0. */
typedef struct fx_quad_vertex_s {
    vec3_t position;
    vec3_t colorStart;
    vec3_t colorEnd;
    vec3_t color;
    float alphaStart;
    float alphaEnd;
    float alpha;
    vec2_t textureCoordinatesStart;
    vec2_t textureCoordinatesEnd;
    vec2_t textureCoordinates;
} fx_quad_vertex_t;

/* The exact original Windows-only class spelling is unavailable from the Mac
 * build. CQuad is a role name: vtable 0x005a2be8 identifies a CEffect-derived
 * type-14 object whose only draw path emits four tracked vertices as a quad. */
class CQuad : public CEffect {
public:
    CQuad();
    ~CQuad() override;
    static void *operator new(size_t size) noexcept;
    static void operator delete(void *allocation) noexcept;
    qboolean Update() override;
    qboolean Cull() override;
    void Draw() override;
    int32_t TypeID() override;
    void Archive(fx_archive_t &archive) override;

    fx_quad_vertex_t vertices[4];
    int32_t shaderHandle;
};

/* Windows runtime TypeID 15 is the polygonal decal effect. The recovered
 * parsed `decal` template path uses CGVM_IMPACT_MARK instead; this separate
 * object owns five local polygon points and a dedicated 0x1cc-byte fixed
 * pool. */
DECLARE_PARTICLE_EFFECT_CLASS(CDecal)
    void Init();
    void RotatePoints();
    void SetPointCount(int32_t value);
    void SetAngularVelocity(const vec3_t value);
    void SetRotationDelay(int32_t delay);
    int32_t pointCount;
    vec3_t angularVelocity;
    int32_t rotationStartTime;
    vec3_t points[FX_DECAL_POINT_CAPACITY];
    float textureCoordinates[FX_DECAL_POINT_CAPACITY][2];
};

#undef DECLARE_PARTICLE_EFFECT_CLASS

class CLight : public CEffect {
public:
    CLight();
    ~CLight() override;
    static void *operator new(size_t size) noexcept;
    static void operator delete(void *allocation) noexcept;
    qboolean Update() override;
    qboolean Cull() override;
    void Draw() override;
    int32_t TypeID() override;
    void Archive(fx_archive_t &archive) override;

    void UpdateRGB();
    void UpdateSize();
    void SetSizeStart(float value);
    void SetSizeEnd(float value);
    void SetSizeTime(float value);
    void SetColorStart(const vec3_t value);
    void SetColorEnd(const vec3_t value);
    void SetColorTime(float value);

    float sizeStart;
    float sizeEnd;
    float sizeTime;
    vec3_t colorStart;
    vec3_t colorEnd;
    float colorTime;
};

/* Windows vtable 0x005a2bcc and the same-module Mac CFlash symbols prove that
 * flash is a fieldless CLight subclass.  It applies view-dependent attenuation
 * to the inherited light colors and submits a sprite-like render entity. */
class CFlash : public CLight {
public:
    CFlash();
    ~CFlash() override;
    qboolean Update() override;
    qboolean Cull() override;
    void Draw() override;
    int32_t TypeID() override;
    void Archive(fx_archive_t &archive) override;

    void Init();
    void SetSpriteShaderHandle(int32_t value);
};

/* Same-module Mac symbol:
 * FX_AddParticle(CFxBoltFramePtr &, float *, float *, float *, float *, bool,
 *                ...). The complete semantic parameter binding is proved by
 * CPrimitiveTemplate::CreateEffect's Windows call at 0x004a8873 and the
 * destination fields in the factory at 0x004b0150. */
CParticle *FX_AddParticle(
    cfx_bolt_frame_ptr_t &boltFrame,
    vec3_t origin,
    vec3_t velocity,
    vec3_t velocityGoal,
    vec3_t acceleration,
    bool nonUniformScale,
    float sizeStart, float sizeEnd, float sizeParm,
    float size2Start, float size2End, float size2Parm,
    float alphaStart, float alphaEnd, float alphaParm,
    vec3_t colorStart, vec3_t colorEnd, float colorParm,
    float rotation, float rotationDelta, float rotationAccel,
    float rotationClamp,
    vec3_t traceMins, vec3_t traceMaxs,
    float elasticity,
    int32_t deathEffectId, int32_t impactEffectId,
    int32_t lifetime, int32_t shaderHandle,
    uint32_t flags, int32_t lerpFlags);

/* Same-module Mac symbol FX_AddLine(CFxBoltFramePtr &, float *, float *,
 * ...). Windows vtable 0x005a2050 and the 0x160-byte line pool bind the
 * factory at 0x004b05e0 to CLine. */
CLine *FX_AddLine(
    cfx_bolt_frame_ptr_t &boltFrame,
    vec3_t start, vec3_t end,
    float sizeStart, float sizeEnd, float sizeParm,
    float alphaStart, float alphaEnd, float alphaParm,
    vec3_t colorStart, vec3_t colorEnd, float colorParm,
    int32_t lifetime, int32_t shaderHandle,
    uint32_t flags, int32_t lerpFlags);

/* Same-module Mac symbol FX_AddElectricity(CFxBoltFramePtr &, float *,
 * float *, ...). Windows vtable 0x005a206c and the 0x2e4-byte pool prove the
 * class binding. */
CElectricity *FX_AddElectricity(
    cfx_bolt_frame_ptr_t &boltFrame,
    vec3_t start, vec3_t end,
    float sizeStart, float sizeEnd, float sizeParm,
    float alphaStart, float alphaEnd, float alphaParm,
    vec3_t colorStart, vec3_t colorEnd, float colorParm,
    float electricityParm,
    int32_t lifetime, int32_t shaderHandle,
    uint32_t flags, int32_t lerpFlags);

/* Same-module Mac symbol FX_AddTail(CFxBoltFramePtr &, float *, float *,
 * float *, float *, ...). Windows vtable 0x005a20c0 binds 0x004b0b90 to
 * CTail. */
CTail *FX_AddTail(
    cfx_bolt_frame_ptr_t &boltFrame,
    vec3_t origin,
    vec3_t velocity,
    vec3_t velocityGoal,
    vec3_t acceleration,
    float sizeStart, float sizeEnd, float sizeParm,
    float lengthStart, float lengthEnd, float lengthParm,
    float alphaStart, float alphaEnd, float alphaParm,
    vec3_t colorStart, vec3_t colorEnd, float colorParm,
    vec3_t traceMins, vec3_t traceMaxs,
    float elasticity,
    int32_t deathEffectId, int32_t impactEffectId,
    int32_t lifetime, int32_t shaderHandle,
    uint32_t flags, int32_t lerpFlags);

/* Same-module Mac symbol FX_AddCylinder(CFxBoltFramePtr &, float *, float *,
 * ...). Windows vtable 0x005a2034 and pool 0x0389ffa0 prove CCylinder. */
CCylinder *FX_AddCylinder(
    cfx_bolt_frame_ptr_t &boltFrame,
    vec3_t origin, vec3_t direction,
    float sizeStart, float sizeEnd, float sizeParm,
    float size2Start, float size2End, float size2Parm,
    float lengthStart, float lengthEnd, float lengthParm,
    float alphaStart, float alphaEnd, float alphaParm,
    vec3_t colorStart, vec3_t colorEnd, float colorParm,
    int32_t lifetime, int32_t shaderHandle,
    uint32_t flags, int32_t lerpFlags);

/* Same-module Mac symbol FX_AddEmitter(CFxBoltFramePtr &, float *, float *,
 * float *, float *, ...). The sole Windows call at 0x004a8de6 proves the
 * semantic argument binding, including the final model pointer. */
CEmitter *FX_AddEmitter(
    cfx_bolt_frame_ptr_t &boltFrame,
    vec3_t origin,
    vec3_t velocity,
    vec3_t velocityGoal,
    vec3_t acceleration,
    float sizeStart, float sizeEnd, float sizeParm,
    float alphaStart, float alphaEnd, float alphaParm,
    vec3_t colorStart, vec3_t colorEnd, float colorParm,
    vec3_t angles, vec3_t angularVelocity,
    vec3_t traceMins, vec3_t traceMaxs,
    float elasticity,
    int32_t deathEffectId, int32_t impactEffectId,
    int32_t emitterEffectId,
    float density, float variance,
    int32_t lifetime, DObj *model,
    uint32_t flags, int32_t lerpFlags);

/* Same-module Mac symbol FX_AddLight(CFxBoltFramePtr &, float *, ...).
 * Windows vtable 0x005a2088 and pool 0x0389ffb0 bind 0x004b17a0 to CLight. */
CLight *FX_AddLight(
    cfx_bolt_frame_ptr_t &boltFrame,
    vec3_t origin,
    float sizeStart, float sizeEnd, float sizeParm,
    vec3_t colorStart, vec3_t colorEnd, float colorParm,
    int32_t lifetime, uint32_t flags, int32_t lerpFlags);

/* Same-module Mac symbol FX_AddOrientedParticle(CFxBoltFramePtr &, float *,
 * ...). Windows vtable 0x005a2018 and pool 0x0389ffc0 bind 0x004b19b0 to
 * COrientedParticle. */
COrientedParticle *FX_AddOrientedParticle(
    cfx_bolt_frame_ptr_t &boltFrame,
    vec3_t origin, vec3_t orientation,
    vec3_t velocity, vec3_t velocityGoal, vec3_t acceleration,
    bool nonUniformScale,
    float sizeStart, float sizeEnd, float sizeParm,
    float size2Start, float size2End, float size2Parm,
    float alphaStart, float alphaEnd, float alphaParm,
    vec3_t colorStart, vec3_t colorEnd, float colorParm,
    float rotation, float rotationDelta, float rotationAccel,
    float rotationClamp,
    vec3_t traceMins, vec3_t traceMaxs,
    float elasticity,
    int32_t deathEffectId, int32_t impactEffectId,
    int32_t lifetime, int32_t shaderHandle,
    uint32_t flags, int32_t lerpFlags);

/* Windows-only type-15 polygon factory at 0x004b1e80. No direct reference or
 * same-module Mac symbol survives, so FX_AddDecal is a role-based name proved
 * by its CDecal allocation, fields, and Init call. */
CDecal *FX_AddDecal(
    const vec3_t *points, const vec2_t *textureCoordinates,
    int32_t pointCount,
    vec3_t velocity, vec3_t acceleration,
    float alphaStart, float alphaEnd, float alphaParm,
    vec3_t colorStart, vec3_t colorEnd, float colorParm,
    vec3_t angularVelocity, float elasticity,
    int32_t rotationDelay, int32_t lifetime,
    int32_t shaderHandle, uint32_t flags, int32_t lerpFlags);

/* Same-module Mac symbol FX_AddFlash(float *, float *, float *, float,
 * int, int, int, int). Windows vtable 0x005a2bcc and the shared light pool
 * bind 0x004b2220 to CFlash. */
CFlash *FX_AddFlash(
    vec3_t origin,
    vec3_t colorStart, vec3_t colorEnd, float colorParm,
    int32_t lifetime, int32_t shaderHandle,
    uint32_t flags, int32_t lerpFlags);

/* Original fixed-pool descriptors.  The Windows descriptor addresses and
 * class bindings are proved by each deleting destructor and operator-delete
 * adapter; the native definitions recompute capacity from host layout. */
extern fx_pool_allocator_t fxEffectAllocator;             /* 0x0389ffd0 */
extern fx_pool_allocator_t fxParticleAllocator;           /* 0x0389ff98 */
extern fx_pool_allocator_t fxOrientedParticleAllocator;   /* 0x0389ffc0 */
extern fx_pool_allocator_t fxLineAllocator;               /* 0x0389ffe0 */
extern fx_pool_allocator_t fxElectricityAllocator;        /* 0x0389ff88 */
extern fx_pool_allocator_t fxTailAllocator;               /* 0x0389ffc8 */
extern fx_pool_allocator_t fxCylinderAllocator;           /* 0x0389ffa0 */
extern fx_pool_allocator_t fxEmitterAllocator;            /* 0x0389ff80 */
extern fx_pool_allocator_t fxQuadAllocator;               /* 0x0389ffd8 */
extern fx_pool_allocator_t fxLightAllocator;              /* 0x0389ffb0 */
extern fx_pool_allocator_t fxDecalAllocator;              /* 0x0389ffb8 */

#if UINTPTR_MAX == UINT32_MAX
static_assert(alignof(CEffect::resource_reference_t) == 0x04,
              "i386 CEffect resource-reference alignment changed");
static_assert(offsetof(CEffect::resource_reference_t, model) == 0x00,
              "i386 CEffect model-reference offset changed");
static_assert(sizeof(((CEffect::resource_reference_t *)nullptr)->model) ==
                  0x04,
              "i386 CEffect model-reference extent changed");
static_assert(offsetof(CEffect::resource_reference_t, effectId) == 0x00,
              "i386 CEffect effect-reference offset changed");
static_assert(sizeof(((CEffect::resource_reference_t *)nullptr)->effectId) ==
                  0x04,
              "i386 CEffect effect-reference extent changed");
static_assert(sizeof(CEffect::resource_reference_t) == 0x04,
              "i386 CEffect resource-reference size changed");
static_assert(offsetof(CEffect, timeStart) == 0x10,
              "i386 CEffect start-time offset changed");
static_assert(offsetof(CEffect, impactResource) == 0x38,
              "i386 CEffect impact-resource offset changed");
static_assert(offsetof(CEffect, deathResource) == 0x3c,
              "i386 CEffect death-resource offset changed");
static_assert(offsetof(CEffect, renderEntity) == 0x40,
              "i386 CEffect render-entity offset changed");
static_assert(offsetof(CEffect, boltFrame) == 0xdc,
              "i386 CEffect bolt-frame offset changed");
static_assert(offsetof(CParticle, particleVelocity) == 0xe0,
              "i386 CParticle velocity offset changed");
static_assert(offsetof(CParticle, sizeStart) == 0x104,
              "i386 CParticle size channel offset changed");
static_assert(offsetof(CParticle, colorStart) == 0x11c,
              "i386 CParticle color channel offset changed");
static_assert(offsetof(CParticle, rotationGoal) == 0x144,
              "i386 CParticle rotation channel offset changed");
static_assert(sizeof(CParticle) == 0x154,
              "i386 CParticle size changed");
static_assert(offsetof(COrientedParticle, orientation) == 0x154,
              "i386 COrientedParticle orientation offset changed");
static_assert(offsetof(CLine, end) == 0x154,
              "i386 CLine endpoint offset changed");
static_assert(offsetof(CElectricity, end) == 0x154,
              "i386 CElectricity endpoint offset changed");
static_assert(offsetof(CElectricity, electricityParm) == 0x160,
              "i386 CElectricity renderer-parameter offset changed");
static_assert(offsetof(CElectricity, rendererSegmentData) == 0x164,
              "i386 CElectricity renderer-data offset changed");
static_assert(sizeof(CElectricity) == 0x2e4,
              "i386 CElectricity size changed");
static_assert(offsetof(CTail, end) == 0x154,
              "i386 CTail endpoint offset changed");
static_assert(offsetof(CTail, currentLength) == 0x16c,
              "i386 CTail current-length offset changed");
static_assert(sizeof(CTail) == 0x170,
              "i386 CTail size changed");
static_assert(sizeof(CCylinder) == sizeof(CTail),
              "i386 CCylinder/CTail archive layouts diverged");
static_assert(offsetof(CEmitter, emitterOrigin) == 0x154,
              "i386 CEmitter origin offset changed");
static_assert(offsetof(CEmitter, angles) == 0x180,
              "i386 CEmitter angles offset changed");
static_assert(offsetof(CEmitter, emitterEffectId) == 0x198,
              "i386 CEmitter effect-id offset changed");
static_assert(sizeof(CEmitter) == 0x1a4,
              "i386 CEmitter size changed");
static_assert(alignof(fx_quad_vertex_t) == 0x04,
              "i386 FX quad-vertex alignment changed");
static_assert(offsetof(fx_quad_vertex_t, position) == 0x00,
              "i386 FX quad-vertex position offset changed");
static_assert(sizeof(((fx_quad_vertex_t *)nullptr)->position) == 0x0c,
              "i386 FX quad-vertex position extent changed");
static_assert(offsetof(fx_quad_vertex_t, colorStart) == 0x0c,
              "i386 FX quad-vertex start-color offset changed");
static_assert(sizeof(((fx_quad_vertex_t *)nullptr)->colorStart) == 0x0c,
              "i386 FX quad-vertex start-color extent changed");
static_assert(offsetof(fx_quad_vertex_t, colorEnd) == 0x18,
              "i386 FX quad-vertex end-color offset changed");
static_assert(sizeof(((fx_quad_vertex_t *)nullptr)->colorEnd) == 0x0c,
              "i386 FX quad-vertex end-color extent changed");
static_assert(offsetof(fx_quad_vertex_t, color) == 0x24,
              "i386 FX quad-vertex current-color offset changed");
static_assert(sizeof(((fx_quad_vertex_t *)nullptr)->color) == 0x0c,
              "i386 FX quad-vertex current-color extent changed");
static_assert(offsetof(fx_quad_vertex_t, alphaStart) == 0x30,
              "i386 FX quad-vertex start-alpha offset changed");
static_assert(sizeof(((fx_quad_vertex_t *)nullptr)->alphaStart) == 0x04,
              "i386 FX quad-vertex start-alpha extent changed");
static_assert(offsetof(fx_quad_vertex_t, alphaEnd) == 0x34,
              "i386 FX quad-vertex end-alpha offset changed");
static_assert(sizeof(((fx_quad_vertex_t *)nullptr)->alphaEnd) == 0x04,
              "i386 FX quad-vertex end-alpha extent changed");
static_assert(offsetof(fx_quad_vertex_t, alpha) == 0x38,
              "i386 FX quad-vertex current-alpha offset changed");
static_assert(sizeof(((fx_quad_vertex_t *)nullptr)->alpha) == 0x04,
              "i386 FX quad-vertex current-alpha extent changed");
static_assert(offsetof(fx_quad_vertex_t, textureCoordinatesStart) == 0x3c,
              "i386 FX quad-vertex start-UV offset changed");
static_assert(
    sizeof(((fx_quad_vertex_t *)nullptr)->textureCoordinatesStart) == 0x08,
    "i386 FX quad-vertex start-UV extent changed");
static_assert(offsetof(fx_quad_vertex_t, textureCoordinatesEnd) == 0x44,
              "i386 FX quad-vertex end-UV offset changed");
static_assert(
    sizeof(((fx_quad_vertex_t *)nullptr)->textureCoordinatesEnd) == 0x08,
    "i386 FX quad-vertex end-UV extent changed");
static_assert(offsetof(fx_quad_vertex_t, textureCoordinates) == 0x4c,
              "i386 FX quad-vertex current-UV offset changed");
static_assert(sizeof(((fx_quad_vertex_t *)nullptr)->textureCoordinates) ==
                  0x08,
              "i386 FX quad-vertex current-UV extent changed");
static_assert(sizeof(fx_quad_vertex_t) == 0x54,
              "i386 FX quad-vertex size changed");
static_assert(offsetof(CQuad, vertices) == 0xe0,
              "i386 CQuad vertex-array offset changed");
static_assert(offsetof(CQuad, shaderHandle) == 0x230,
              "i386 CQuad shader offset changed");
static_assert(sizeof(CQuad) == 0x234,
              "i386 CQuad size changed");
static_assert(offsetof(CDecal, pointCount) == 0x154,
              "i386 CDecal point-count offset changed");
static_assert(offsetof(CDecal, points) == 0x168,
              "i386 CDecal point-array offset changed");
static_assert(FX_DECAL_POINT_CAPACITY != 5 ||
                  offsetof(CDecal, textureCoordinates) == 0x1a4,
              "i386 CDecal texture-coordinate offset changed");
static_assert(FX_DECAL_POINT_CAPACITY != 5 || sizeof(CDecal) == 0x1cc,
              "i386 CDecal size changed");
static_assert(offsetof(CLight, sizeStart) == 0xe0,
              "i386 CLight size-channel offset changed");
static_assert(offsetof(CLight, colorStart) == 0xec,
              "i386 CLight color-channel offset changed");
static_assert(sizeof(CLight) == 0x108,
              "i386 CLight size changed");
static_assert(sizeof(CFlash) == sizeof(CLight),
              "i386 CFlash/CLight layouts diverged");
static_assert(sizeof(((fx_mem_block_t *)nullptr)->storage) /
                  sizeof(CEffect) == 146,
              "i386 CEffect pool capacity changed");
static_assert(sizeof(((fx_mem_block_t *)nullptr)->storage) /
                  sizeof(CParticle) == 96,
              "i386 CParticle pool capacity changed");
static_assert(sizeof(((fx_mem_block_t *)nullptr)->storage) /
                  sizeof(COrientedParticle) == 93,
              "i386 COrientedParticle pool capacity changed");
static_assert(sizeof(((fx_mem_block_t *)nullptr)->storage) /
                  sizeof(CLine) == 93,
              "i386 CLine pool capacity changed");
static_assert(sizeof(((fx_mem_block_t *)nullptr)->storage) /
                  sizeof(CElectricity) == 44,
              "i386 CElectricity pool capacity changed");
static_assert(sizeof(((fx_mem_block_t *)nullptr)->storage) /
                  sizeof(CTail) == 89,
              "i386 CTail pool capacity changed");
static_assert(sizeof(((fx_mem_block_t *)nullptr)->storage) /
                  sizeof(CCylinder) == 89,
              "i386 CCylinder pool capacity changed");
static_assert(sizeof(((fx_mem_block_t *)nullptr)->storage) /
                  sizeof(CEmitter) == 77,
              "i386 CEmitter pool capacity changed");
static_assert(sizeof(((fx_mem_block_t *)nullptr)->storage) /
                  sizeof(CQuad) == 58,
              "i386 CQuad pool capacity changed");
static_assert(sizeof(((fx_mem_block_t *)nullptr)->storage) /
                  sizeof(CLight) == 124,
              "i386 CLight pool capacity changed");
static_assert(FX_DECAL_POINT_CAPACITY != 5 ||
                  sizeof(((fx_mem_block_t *)nullptr)->storage) /
                          sizeof(CDecal) == 71,
              "i386 CDecal pool capacity changed");
#endif

#endif
