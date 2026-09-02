#include "bg_animation.h"
#include "bg_animation_services.h"
#include "compat/coduo_int32_bits.h"

#include <stddef.h>
#include <stdint.h>

/*
 * Mac cgame/game symbols retain the canonical BG_EvaluateConditions name. The
 * authoritative Windows bodies are instruction-identical at 0x30002ee0 and
 * 0x20002ec0; Linux game retains the same operation graph at RVA 0x0001be6c.
 */
qboolean BG_EvaluateConditions(clientInfo_t *clientInfo, const bg_anim_script_t *script)
{
    int32_t index;

    for (index = 0; index < script->conditionCount; ++index) {
        const bg_anim_condition_t *condition = &script->conditions[index];
        const int32_t type = condition->type;

        if (bgAnimConditionTypes[type].mode == ANIM_CONDMODE_BITMASK) {
            if (((uint32_t)condition->value[0] & clientInfo->conditionWords[type][0]) != 0 ||
                ((uint32_t)condition->value[1] & clientInfo->conditionWords[type][1]) != 0) {
                continue;
            }
            return qfalse;
        }

        if (bgAnimConditionTypes[type].mode == ANIM_CONDMODE_EQUAL &&
            clientInfo->conditionWords[type][0] != (uint32_t)condition->value[0]) {
            return qfalse;
        }
    }

    return qtrue;
}

/*
 * Mac symbols retain BG_FirstValidItem. Windows cgame/game bodies at
 * 0x30002f40/0x20002f20 and Linux game RVA 0x0001bf35 agree on signed count,
 * first-match order, and the unchecked client-info row selection.
 */
bg_anim_script_t *BG_FirstValidItem(int32_t clientNum, const bg_anim_script_list_t *scriptList)
{
    clientInfo_t *clientInfo;
    int32_t index;

    if (scriptList->count <= 0) {
        return NULL;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: require the client identifier to belong to the
     * shared client-info row domain before forming its pointer. */
    if ((uint32_t)clientNum >= (uint32_t)MAX_CLIENTS) {
        Com_Error(ERR_DROP,
                  "\x15"
                  "BG_FirstValidItem: invalid client number %i",
                  clientNum);
        return NULL;
    }
    clientInfo = &bgs.clientinfo[clientNum];
    for (index = 0; index < scriptList->count; ++index) {
        if (BG_EvaluateConditions(clientInfo, scriptList->scripts[index])) {
            return scriptList->scripts[index];
        }
    }
    return NULL;
}

/* Windows cgame/game 0x30003490/0x20003470; Linux game RVA 0x0001c718. */
void BG_UpdateConditionValue(int32_t clientNum, int32_t conditionType, int32_t value, qboolean checkConversion)
{
    /* NOT_FROM_ORIGINAL_SOURCE: require the client identifier to belong to the
     * writable client-info row domain before forming its pointer. */
    if ((uint32_t)clientNum >= (uint32_t)MAX_CLIENTS) {
        Com_Error(ERR_DROP,
                  "\x15"
                  "BG_UpdateConditionValue: invalid client number %i",
                  clientNum);
        return;
    }
    clientInfo_t *clientInfo = &bgs.clientinfo[clientNum];

    if (checkConversion && bgAnimConditionTypes[conditionType].mode == ANIM_CONDMODE_BITMASK) {
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if ((uint32_t)value >= (uint32_t)BG_ANIM_CONDITION_BIT_COUNT) {
            if (conditionType == ANIM_COND_WEAPON && (uint32_t)value < (uint32_t)MAX_WEAPONS) {
                clientInfo->conditionWords[conditionType][0] = 0;
                clientInfo->conditionWords[conditionType][1] = 0;
                return;
            }
            Com_Error(ERR_DROP,
                      "\x15"
                      "BG_UpdateConditionValue: "
                      "invalid condition value %i",
                      value);
            return;
        }
        clientInfo->conditionWords[conditionType][0] = 0;
        clientInfo->conditionWords[conditionType][1] = 0;
        clientInfo->conditionWords[conditionType][coduo_int32_sar((uint32_t)value, 5)] |= UINT32_C(1) << ((uint32_t)value & UINT32_C(31));
    } else {
        clientInfo->conditionWords[conditionType][0] = (uint32_t)value;
    }
}

/* Windows cgame/game 0x30003500/0x200034e0; Linux game RVA 0x0001c7ce. */
int32_t BG_GetConditionValue(clientInfo_t *clientInfo, int32_t conditionType, qboolean convertBitset)
{
    uint32_t value = clientInfo->conditionWords[conditionType][0];
    int32_t bit;

    if (!convertBitset || bgAnimConditionTypes[conditionType].mode != ANIM_CONDMODE_BITMASK) {
        return coduo_int32_from_bits(value);
    }

    for (bit = 0; bit < BG_ANIM_CONDITION_BIT_COUNT; ++bit) {
        if ((clientInfo->conditionWords[conditionType][bit >> 5] & (UINT32_C(1) << ((uint32_t)bit & UINT32_C(31)))) != 0) {
            return bit;
        }
    }
    return 0;
}
