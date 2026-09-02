// Source: uo_cgame_mp_x86.dll 0x30059e20..0x30059f7c;
//         uo_ui_mp_x86.dll    0x4001b990..0x4001baec.
// The transformation is identical; parser token reads use the owning module's
// canonical trap_PC_ReadToken boundary.
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30059e20_30059f7c.mcode

#include "ui_parse.h"
#include "ui_memory.h"
#include "ui_runtime.h"
#include "qcommon/sound_types.h"

#include <string.h>

void Com_Printf(const char *format, ...);

#if defined(__APPLE__) || defined(__linux__)
/* NOT_FROM_ORIGINAL_SOURCE: retail menu data hard-codes the Windows Miles
 * provider list. Replace it with only the adapters this native build can
 * actually open. */
static void coduomp_ui_append_native_sound_provider(multiDef_t *multi, const char *name)
{
    if (multi->count >= MAX_MULTI_CVARS)
        return;
    multi->cvarList[multi->count] = String_Alloc(name);
    multi->cvarStr[multi->count] = String_Alloc(name);
    ++multi->count;
}

static void coduomp_ui_replace_native_sound_providers(itemDef_t *item, multiDef_t *multi)
{
    if (item->cvar == NULL || strcmp(item->cvar, "ui_mss_3d_provider") != 0)
        return;
    multi->count = 0;
    coduomp_ui_append_native_sound_provider(multi, CODUOMP_MINIAUDIO_3D_PROVIDER_NAME);
    coduomp_ui_append_native_sound_provider(multi, CODUOMP_OPENAL_3D_PROVIDER_NAME);
}
#endif

qboolean ItemParse_cvarStrList(itemDef_t *item, int handle)
{
    pc_token_t token;
    Item_ValidateTypeData(item, handle);
    multiDef_t *multi = (multiDef_t *)item->typeData;
    if (multi == NULL || item->typeValidated != ITEM_TYPE_MULTI) {
        if (multi != NULL)
            Com_Printf("^1Menu Error: Expecting type: ITEM_TYPE_MULTI\n");
        return qfalse;
    }

    multi->count = 0;
    multi->strDef = 1;
    if (!trap_PC_ReadToken(handle, &token) || token.string[0] != '{')
        return qfalse;

    qboolean valueToken = qfalse;
    while (trap_PC_ReadToken(handle, &token)) {
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (token.string[0] == '}') {
#if defined(__APPLE__) || defined(__linux__)
            coduomp_ui_replace_native_sound_providers(item, multi);
#endif
            return qtrue;
        }
        if (token.string[0] == ',' || token.string[0] == ';')
            continue;

        const char *value = String_Alloc(token.string);
        if (!valueToken) {
            multi->cvarList[multi->count] = value;
            valueToken = qtrue;
        } else {
            multi->cvarStr[multi->count] = value;
            valueToken = qfalse;
            /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
            if (++multi->count >= MAX_MULTI_CVARS)
                return qfalse;
        }
    }

    PC_SourceError(handle, "end of file inside menu item\n");
    return qfalse;
}
