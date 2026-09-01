// Source: uo_cgame_mp_x86.dll 0x3005a490..0x3005a4e5;
//         uo_ui_mp_x86.dll    0x4001c0b0..0x4001c105 (exact after rebasing).
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3005a490_3005a4e5.mcode

#include "ui_parse.h"
#include "ui_runtime.h"

void Com_Printf(const char *format, ...);

void Item_PostParse(itemDef_t *item)
{
    if (item == NULL || item->type != ITEM_TYPE_LISTBOX)
        return;

    listBoxDef_t *listBox = NULL;
    if (item->typeValidated == ITEM_TYPE_LISTBOX)
        listBox = (listBoxDef_t *)item->typeData;
    else
        Com_Printf("^1Menu Error: Expecting type: ITEM_TYPE_LISTBOX\n");

    item->cursorPos = 0;
    if (listBox != NULL) {
        listBox->startPos = 0;
        listBox->endPos = 0;
        listBox->cursorPos = 0;
    }
}
