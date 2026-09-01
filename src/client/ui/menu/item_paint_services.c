#include "../module/ui_functions.h"

/* NOT_FROM_ORIGINAL_SOURCE: UI has no cgame passive-HUD projection to apply
 * around the shared original Item_Paint body. */
float client_ui_compat_begin_item_paint(itemDef_t *item)
{
    (void)item;
    return 0.0f;
}

/* NOT_FROM_ORIGINAL_SOURCE: UI has no cgame passive-HUD projection to close. */
void client_ui_compat_end_item_paint(itemDef_t *item, float offset)
{
    (void)item;
    (void)offset;
}
