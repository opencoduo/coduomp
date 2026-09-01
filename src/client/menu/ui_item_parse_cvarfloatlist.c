// Source: uo_cgame_mp_x86.dll 0x30059f80..0x3005a0e2;
//         uo_ui_mp_x86.dll    0x4001baf0..0x4001bc52.
// The transformation is identical; parser token reads use the owning module's
// canonical trap_PC_ReadToken boundary.
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30059f80_3005a0e2.mcode

#include "ui_parse.h"
#include "ui_memory.h"
#include "ui_runtime.h"

void Com_Printf(const char *format, ...);

qboolean ItemParse_cvarFloatList(itemDef_t *item, int handle)
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
    multi->strDef = 0;
    if (!trap_PC_ReadToken(handle, &token) ||
        token.string[0] != '{')
        return qfalse;

    while (trap_PC_ReadToken(handle, &token)) {
        if (token.string[0] == '}')
            return qtrue;
        if (token.string[0] == ',' || token.string[0] == ';')
            continue;

        multi->cvarList[multi->count] = String_Alloc(token.string);
        if (!PC_Float_Parse(handle, &multi->cvarValue[multi->count]))
            return qfalse;
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (++multi->count >= MAX_MULTI_CVARS)
            return qfalse;
    }

    PC_SourceError(handle, "end of file inside menu item\n");
    return qfalse;
}
