#include "client/menu/ui_runtime.h"

/* NOT_FROM_ORIGINAL_SOURCE: cgame has no server-browser selection state to
 * synchronize at the two common listbox entry points. */
void client_ui_compat_sync_server_list_selection(itemDef_t *item)
{
    (void)item;
}
