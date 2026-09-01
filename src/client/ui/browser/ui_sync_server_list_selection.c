#include "../module/ui_functions.h"

enum {
    UI_SERVER_LIST_FEEDER = 2
};

// Source: uo_ui_mp_x86.dll 0x4000f140..0x4000f1bd
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000f140_4000f1bd.mcode
// Role name derived from the exact feeder/current-server state transfer.
void UI_SyncServerListSelection(itemDef_t *item)
{
    listBoxDef_t *listBox;
    int32_t offset;
    int32_t maximum;

    if (item->special != (float)UI_SERVER_LIST_FEEDER) return;

    listBox = (listBoxDef_t *)item->typeData;
    if (listBox->endPos == 0) {
        item->cursorPos = -1;
        return;
    }
    if (ui_currentServer < 0) return;
    if (item->cursorPos < listBox->startPos ||
        item->cursorPos > listBox->endPos) {
        return;
    }

    offset = ui_currentServer - item->cursorPos;
    listBox->startPos += offset;
    listBox->endPos += offset;
    listBox->cursorPos += offset;
    item->cursorPos = ui_currentServer;

    maximum = Item_ListBox_MaxScroll(item);
    if (listBox->startPos > maximum) listBox->startPos = maximum;
    if (listBox->startPos < 0) listBox->startPos = 0;
}

/* NOT_FROM_ORIGINAL_SOURCE: bind the shared listbox entry points to the UI
 * DLL's original server-browser synchronization function. */
void client_ui_compat_sync_server_list_selection(itemDef_t *item)
{
    UI_SyncServerListSelection(item);
}
