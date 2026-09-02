// Source: uo_cgame_mp_x86.dll 0x3002af00..0x3002b148
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002af00_3002b148.mcode

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"
#include "client/cgame/abi/cgame_module_abi.h"

#include <stddef.h>
#include <stdint.h>

/* CG_DrawActiveFrame and CG_Init are declared centrally from their recovered
 * bodies. The remaining dispatch targets are also declared in the shared
 * header; vmMain proves the six frame-command argument slots and their order. */

/*
 * Exported cgame dispatcher. The binary uses an unsigned range check, so negative
 * commands take the same diagnostic path as values above 20. The default result
 * is zero; only commands 0, 3, 5, 7, 10 and 18 replace it, while invalid commands
 * report the original diagnostic and return -1.
 */
CGAME_EXPORT intptr_t CGAME_ABI_CDECL vmMain(
    int32_t command, intptr_t arg0, intptr_t arg1, intptr_t arg2,
    intptr_t arg3, intptr_t arg4, intptr_t arg5, intptr_t arg6,
    intptr_t arg7, intptr_t arg8, intptr_t arg9, intptr_t arg10,
    intptr_t arg11)
{
    if ((uint32_t)command > (uint32_t)CGVM_LAST_COMMAND) {
        Com_ErrorMessage("vmMain: unknown command %i", command);
        return -1;
    }

    switch ((cgVmCommand_t)command) {
    case CGVM_GET_API_VERSION:
        return CGVM_API_VERSION;

    case CGVM_INIT:
        CG_Init(coduo_int32_from_bits((uint32_t)arg0),
                coduo_int32_from_bits((uint32_t)arg1),
                coduo_int32_from_bits((uint32_t)arg2));
        break;

    case CGVM_SHUTDOWN:
        CG_ShutdownEffectsAndHud();
        break;

    case CGVM_CONSOLE_COMMAND:
        return (intptr_t)CG_ConsoleCommand();

    case CGVM_DRAW_ACTIVE_FRAME:
        CG_DrawActiveFrame(coduo_int32_from_bits((uint32_t)arg0),
                           coduo_int32_from_bits((uint32_t)arg1),
                           coduo_int32_from_bits((uint32_t)arg2),
                           coduo_int32_from_bits((uint32_t)arg3),
                           coduo_int32_from_bits((uint32_t)arg4),
                           coduo_int32_from_bits((uint32_t)arg5));
        break;

    case CGVM_CROSSHAIR_PLAYER:
        return (intptr_t)CG_CrosshairPlayer();

    case CGVM_LAST_ATTACKER:
        break;

    case CGVM_KEY_EVENT:
        return (intptr_t)CG_KeyEvent(coduo_int32_from_bits((uint32_t)arg0),
                                     coduo_int32_from_bits((uint32_t)arg1));

    case CGVM_MOUSE_EVENT: {
        /* 0x3002b001..0x3002b015: both source words are loaded before either
         * destination write; the high word is loaded/stored first. */
        int32_t cursorY = cgs_cursorY;
        int32_t cursorX = cgs_cursorX;
        g_uiDCInstance.cursory = cursorY;
        g_uiDCInstance.cursorx = cursorX;
        break;
    }

    case CGVM_EVENT_HANDLING:
        break;

    case CGVM_GET_MODEL_HANDLE: {
        int32_t modelIndex =
            coduo_int32_from_bits((uint32_t)arg0);
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if ((uint32_t)modelIndex >= (uint32_t)CS_MODELS_COUNT) {
            return 0;
        }
        return (intptr_t)cg_gameModels[modelIndex];
    }

    case CGVM_DOBJ_CALC_POSE:
        CG_DObjCalcPose(
            (centity_t *)(uintptr_t)arg0,
            (struct DObj_s *)(uintptr_t)arg1,
            (uint32_t *)(uintptr_t)arg2);
        break;

    case CGVM_DOBJ_CALC_BONE_GENERIC:
        CG_DObjCalcBoneGeneric(coduo_int32_from_bits((uint32_t)arg0),
                               coduo_int32_from_bits((uint32_t)arg1));
        break;

    case CGVM_GET_ENTITY_ORIGIN_AXIS:
        CG_GetEntityOriginAxis(
            coduo_int32_from_bits((uint32_t)arg0),
            (float *)(uintptr_t)arg1,
            (float (*)[3])(uintptr_t)arg2);
        break;

    case CGVM_GET_EFFECT_ORIGIN_AXIS:
        CG_GetEffectOriginAxis(
            coduo_int32_from_bits((uint32_t)arg0),
            (float *)(uintptr_t)arg1,
            (float (*)[3])(uintptr_t)arg2);
        break;

    case CGVM_IMPACT_MARK:
        CG_ImpactMark(
            coduo_int32_from_bits((uint32_t)arg0),
            (const float *)(uintptr_t)arg1,
            (const float *)(uintptr_t)arg2,
            CG_FloatFromBits((uint32_t)arg3),
            CG_FloatFromBits((uint32_t)arg4),
            CG_FloatFromBits((uint32_t)arg5),
            CG_FloatFromBits((uint32_t)arg6),
            CG_FloatFromBits((uint32_t)arg7),
            coduo_int32_from_bits((uint32_t)arg8),
            CG_FloatFromBits((uint32_t)arg9),
            coduo_int32_from_bits((uint32_t)arg10), /* temporary -> entry+0x28 */
            coduo_int32_from_bits((uint32_t)arg11));/* markLifeTime -> entry+0x2c */
        break;

    case CGVM_ADD_CAMERA_SHAKE: {
        /* arg0 points to the amplitude float; radius is a numerical conversion
         * from arg3 (FILD/FSTP), not a raw float-bit transport. */
        long double radiusValue =
            (long double)coduo_int32_from_bits((uint32_t)arg3);
        int32_t duration = coduo_int32_from_bits((uint32_t)arg1);
        float amplitude = *(const float *)(uintptr_t)arg0;
        float radius = (float)radiusValue;
        const float *origin = (const float *)(uintptr_t)arg2;
        CG_AddCameraShake(origin, amplitude, duration, radius);
        break;
    }

    case CGVM_DRAW_SCALED:
        /* The four coordinates are also FILD conversions in the dispatcher.
         * Register args are mode=arg3, adjust=1, color=NULL. */
        CG_EmitTrap54DrawScaled(
            coduo_int32_from_bits((uint32_t)arg3), 1, NULL,
            (float)coduo_int32_from_bits((uint32_t)arg0),
            (float)coduo_int32_from_bits((uint32_t)arg1),
            (void *)(uintptr_t)arg2,
            (float)coduo_int32_from_bits((uint32_t)arg4),
            (float)coduo_int32_from_bits((uint32_t)arg5),
            coduo_int32_from_bits((uint32_t)arg6));
        break;

    case CGVM_SCRIPT_FAR_HOOK:
        return (intptr_t)Scr_FarHook(
            (const cg_scriptImportTable_t *)(uintptr_t)arg0);

    case CGVM_SAVE_STATE:
    case CGVM_RESTORE_STATE:
        break;
    }

    return 0;
}
