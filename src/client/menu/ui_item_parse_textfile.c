// Source: uo_cgame_mp_x86.dll 0x30058f70..0x30058fee;
//         uo_ui_mp_x86.dll    0x4001aae0..0x4001ab5e.
// The transformation is identical; the original inlined parser trap uses the
// owning module's command number (cgame 99, UI 76), now isolated behind the
// canonical trap_PC_ReadToken boundary.
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30058f70_30058fee.mcode

#include "ui_parse.h"
#include "ui_memory.h"

qboolean ItemParse_textfile(itemDef_t *item, int handle)
{
    pc_token_t token;
    displayContextDef_t *display;
    const char *resolved;
    const char *allocated;

    if (!trap_PC_ReadToken(handle, &token))
        return qfalse;

    display = DC;
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    resolved = display->resolveTextToken(token.string);
    allocated = String_Alloc(resolved);
    item->text = allocated;
    return qtrue;
}
