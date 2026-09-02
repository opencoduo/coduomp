// Source: uo_cgame_mp_x86.dll syscall adapters installed by
// CG_UIDisplayContextInit (0x3002da90).
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003def0_3003df01.mcode,
// FUN_3003df40_3003df4a.mcode, FUN_3003e090_3003e0a1.mcode,
// FUN_3003e2a0_3003e2bb.mcode, FUN_3003f100_3003f11e.mcode,
// FUN_3003f120_3003f139.mcode, and FUN_3003f140_3003f15e.mcode.

#include "../client_recovered.h"

const char *trap_SE_TranslateReference(const char *reference)
{
    return (const char *)cgame_syscall(CG_SE_TRANSLATE_REFERENCE, (intptr_t)reference);
}

void trap_R_ClearScene(void)
{
    (void)cgame_syscall(CG_R_CLEAR_SCENE);
}

void trap_R_RenderScene(const refdef_t *refdef)
{
    (void)cgame_syscall(CG_R_RENDER_SCENE, (intptr_t)refdef);
}

void trap_R_ModelBounds(int32_t model, vec3_t mins, vec3_t maxs)
{
    (void)cgame_syscall(CG_R_MODEL_BOUNDS, model, (intptr_t)mins, (intptr_t)maxs);
}

void trap_Key_GetBindingBuf(int32_t keynum, char *buffer, int32_t bufferSize)
{
    (void)cgame_syscall(CG_KEY_GET_BINDING_BUF, keynum, (intptr_t)buffer, bufferSize);
}

void trap_Key_SetBinding(int32_t keynum, const char *binding)
{
    (void)cgame_syscall(CG_KEY_SET_BINDING, keynum, (intptr_t)binding);
}

void trap_Key_KeynumToStringBuf(int32_t keynum, char *buffer, int32_t bufferSize)
{
    (void)cgame_syscall(CG_KEY_KEYNUM_TO_STRING_BUF, keynum, (intptr_t)buffer, bufferSize);
}

/* Exact Mac cgame symbols at 0x1004d720, 0x1004d6c0, 0x1004d660, and
 * 0x1004d5e0.  The Windows compiler inlines the same veneers into the
 * ui_shared parser callers. */
int32_t trap_PC_LoadSource(const char *filename)
{
    return (int32_t)cgame_syscall(CG_PC_LOAD_SOURCE, (intptr_t)filename);
}

void trap_PC_FreeSource(int32_t sourceHandle)
{
    (void)cgame_syscall(CG_PC_FREE_SOURCE, sourceHandle);
}

qboolean trap_PC_ReadToken(int32_t sourceHandle, pc_token_t *token)
{
    return (qboolean)cgame_syscall(CG_PC_READ_TOKEN, sourceHandle, (intptr_t)token);
}

void trap_PC_SourceFileAndLine(int32_t sourceHandle, char *filename, int32_t *line)
{
    (void)cgame_syscall(CG_PC_SOURCE_FILE_AND_LINE, sourceHandle, (intptr_t)filename, (intptr_t)line);
}
