#ifndef CODUOMP_FX_SCHEDULER_HPP
#define CODUOMP_FX_SCHEDULER_HPP

#include "fx_api.h"
#include "fx_memory.h"

#include <list>
#include <map>
#include <string>

enum {
    FX_EFFECT_TEMPLATE_COUNT = 512,
    /* Retail uses 24. Keep this as the single capacity knob so a modified
     * client can raise the template limit together with its owned table. */
    FX_EFFECT_TEMPLATE_PRIMITIVE_CAPACITY = 24
};

class CPrimitiveTemplate;
class CGPGroup;

/* Same-module Mac symbols identify SEffectTemplate.  Windows scheduler
 * accesses prove the complete fixed header: active and temporary flags, the
 * name beginning at +2, a primitive count at +0x44, and 24 pointers beginning
 * at +0x48. Bytes +0x42/+0x43 are natural alignment padding: whole-record
 * initialization clears them, but CoDUOMP.exe never accesses them otherwise. */
typedef struct SEffectTemplate {
    uint8_t active;
    /* Set on a copied template at 0x004a6d70; CreateEffect clears active after
     * consuming such a template at 0x004a73df. */
    uint8_t temporary;
    char name[FX_EFFECT_TEMPLATE_NAME_CAPACITY];
    int32_t primitiveCount;
    CPrimitiveTemplate *primitives[FX_EFFECT_TEMPLATE_PRIMITIVE_CAPACITY];
} sfx_effect_template_t;

#if UINTPTR_MAX == UINT32_MAX
static_assert(alignof(sfx_effect_template_t) == 0x04,
              "i386 SEffectTemplate alignment changed");
static_assert(offsetof(sfx_effect_template_t, active) == 0x00,
              "i386 SEffectTemplate active offset changed");
static_assert(sizeof(((sfx_effect_template_t *)0)->active) == 0x01,
              "i386 SEffectTemplate active extent changed");
static_assert(offsetof(sfx_effect_template_t, temporary) == 0x01,
              "i386 SEffectTemplate temporary offset changed");
static_assert(sizeof(((sfx_effect_template_t *)0)->temporary) == 0x01,
              "i386 SEffectTemplate temporary extent changed");
static_assert(offsetof(sfx_effect_template_t, name) == 0x02,
              "i386 SEffectTemplate name offset changed");
static_assert(sizeof(((sfx_effect_template_t *)0)->name) == 0x40,
              "i386 SEffectTemplate name extent changed");
static_assert(offsetof(sfx_effect_template_t, primitiveCount) == 0x44,
              "i386 SEffectTemplate primitive-count offset changed");
static_assert(sizeof(((sfx_effect_template_t *)0)->primitiveCount) == 0x04,
              "i386 SEffectTemplate primitive-count extent changed");
static_assert(offsetof(sfx_effect_template_t, primitives) == 0x48,
              "i386 SEffectTemplate primitive-table offset changed");
static_assert(sizeof(((sfx_effect_template_t *)0)->primitives) ==
                  FX_EFFECT_TEMPLATE_PRIMITIVE_CAPACITY * sizeof(void *),
              "i386 SEffectTemplate primitive-table extent changed");
static_assert(sizeof(sfx_effect_template_t) ==
                  offsetof(sfx_effect_template_t, primitives) +
                      FX_EFFECT_TEMPLATE_PRIMITIVE_CAPACITY * sizeof(void *),
              "i386 SEffectTemplate capacity no longer determines size");
#endif

/* The Windows object stores 512 SEffectTemplate records followed by an MSVC
 * std::map<string,int> at +0x15000 and a scheduled-effect pointer list at
 * +0x1500c. Maintained source uses the platform standard-library layouts;
 * these containers never cross an engine, file, or network ABI boundary. */
class CFxScheduler {
public:
    CFxScheduler();
    ~CFxScheduler();

    int32_t RegisterEffect(const char *name, bool useDirectFileName);
    sfx_effect_template_t *GetNewEffectTemplate(int32_t *effectId,
                                                const char *name);
    void AddPrimitiveToEffect(sfx_effect_template_t *effectTemplate,
                              CPrimitiveTemplate *primitiveTemplate);
    int32_t ParseEffect(const char *name, CGPGroup *parser);
    sfx_effect_template_t *CopyEffectTemplate(
        int32_t sourceEffectId, const char *copyName,
        int32_t *copyEffectId);
    sfx_effect_template_t *CopyEffectTemplate(
        const char *sourceName, const char *copyName,
        int32_t *copyEffectId);
    static CPrimitiveTemplate *FindPrimitiveTemplate(
        const sfx_effect_template_t *effectTemplate,
        const char *primitiveName);
    int32_t GetScheduledEffectCount() const;
    void PlayEntityEffectID(int32_t effectId, const vec3_t origin,
                            const axis_t axis,
                            const sfx_bolt_info_t *boltInfo);
    void AddScheduledEffects();
    void CreateEffect(CPrimitiveTemplate *primitiveTemplate,
                      const sfx_bolt_info_t *boltInfo,
                      const vec3_t origin, const axis_t axis,
                      int32_t timeOffset);

    sfx_effect_template_t effectTemplates[FX_EFFECT_TEMPLATE_COUNT];
    std::map<std::string, int32_t> effectIdsByName;
    std::list<sfx_scheduled_effect_t *> scheduledEffects;
};

extern fx_pool_allocator_t fxScheduledEffectAllocator; /* 0x0389fff0 */

qboolean CFxScheduler_CopyEffectTemplate(
    const sfx_effect_template_t *source,
    sfx_effect_template_t *destination);

#endif
