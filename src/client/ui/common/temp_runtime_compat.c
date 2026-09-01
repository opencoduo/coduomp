#include "../module/ui_globals.h"

#include "qcommon/q_string.h"

/* NOT_FROM_ORIGINAL_SOURCE: restore the load-initialized temporary formatter
 * and vector cursors when a native loader retains this module across VM
 * lifecycles.  Every returned slot is completely overwritten before use, so
 * the backing arrays do not require a separate compatibility clear. */
void ui_compat_reset_temp_runtime_state(void)
{
    q_vaStringOffset = 0;
    q_tempVectorIndex = 0;
}
