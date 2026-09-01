#include "../module/ui_functions.h"
#include "compat/crt/qsort_compat.h"

#include <stdlib.h>

// Source: uo_ui_mp_x86.dll 0x4000b6c0..0x4000b6ee
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000b6c0_4000b6ee.mcode
// Exact same-module PPC symbol: UI_ServersQsortCompare.
int32_t UI_ServersQsortCompare(const void *left, const void *right)
{
    int32_t server1 = *(const int32_t *)left;
    int32_t server2 = *(const int32_t *)right;

    return trap_LAN_CompareServers(ui_netSource, ui_serverSortKey,
                                   ui_serverSortDirection, server1, server2);
}

// Source: uo_ui_mp_x86.dll 0x4000b6f0..0x4000b71c
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000b6f0_4000b71c.mcode
// Exact same-module PPC symbol: UI_ServersSort.
void UI_ServersSort(int32_t column, qboolean force)
{
    if (!force && ui_serverSortKey == column) {
        return;
    }

    ui_serverSortKey = column;
    coduo_crt_qsort(ui_displayServers,
                        (size_t)ui_displayServerCount,
                        sizeof(ui_displayServers[0]),
                        UI_ServersQsortCompare);
}
