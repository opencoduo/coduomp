#include "hunk.h"
#include "hunk_clear_services.h"

#include "filesystem/filesystem.h"

#include <stdint.h>
#include <stdlib.h>

enum { HUNK_MARK_UNSET = 0 };

void Com_Printf(const char *format, ...);
void XModelClearData(void *rangeStart, void *rangeEnd);

/*
 * Complete common hunk state/mark cluster.  The Windows client and Linux
 * dedicated engine perform the same field transitions and use the same
 * half-open free-hunk interval.  Their hunk_state_t field order differs, but
 * that target ABI distinction is already confined to hunk_types.h.
 *
 *   CoDUOMP.exe   0x00435af0..0x00435d5a
 *   coduo_lnxded  0x0806c1f1..0x0806c3d3
 *
 * The canonical names follow the exact same-module Mac client symbols.  The
 * former Linux reconstruction names Hunk_RetouchMemory, Hunk_SetHighMark,
 * Hunk_ClearToHighMark, Hunk_SetLowMark, and Hunk_ClearToLowMark described
 * these same original operations rather than separate interfaces.
 */

/* CoDUOMP.exe 0x00435af0; coduo_lnxded 0x0806c1f1. */
size_t Hunk_MemoryRemaining(void)
{
    return hunk.totalSize - hunk.highTemp - hunk.lowTemp;
}

/* CoDUOMP.exe 0x00435b10; coduo_lnxded 0x0806c20a. Exact same-module Mac
 * symbol: Hunk_ClearData. */
void Hunk_ClearData(void)
{
    void *const rangeStart = hunk_data + hunk.lowUsed;
    void *const rangeEnd = hunk_data + hunk.totalSize - hunk.highUsed;

    XModelClearData(rangeStart, rangeEnd);
    FS_ClearDataForFiles(rangeStart, rangeEnd);
}

/* CoDUOMP.exe 0x00435b50; coduo_lnxded 0x0806c258. Exact same-module Mac
 * symbol: Hunk_SetMark2. */
void Hunk_SetMark2(void)
{
    hunk.highMark = hunk.highUsed;
}

/* CoDUOMP.exe 0x00435b60; coduo_lnxded 0x0806c267. */
void Hunk_SetHighTempMark(void)
{
    hunk.highTempMark = hunk.highUsed;
}

/* CoDUOMP.exe 0x00435b70; coduo_lnxded 0x0806c276. */
qboolean Hunk_CheckHighMark(const void *pointer)
{
    const uint8_t *const highMarkAddress =
        hunk_data + hunk.totalSize - hunk.highMark;
    return (uintptr_t)pointer >= (uintptr_t)highMarkAddress;
}

/* CoDUOMP.exe 0x00435b90; coduo_lnxded 0x0806c295. Exact same-module Mac
 * symbol: Hunk_ClearToMark2. */
void Hunk_ClearToMark2(void)
{
    hunk.highTemp = hunk.highMark;
    hunk.highUsed = hunk.highMark;
    Hunk_ClearData();
}

/* CoDUOMP.exe 0x00435bd0; coduo_lnxded 0x0806c2b1. */
void Hunk_ClearToHighTempMark(void)
{
    hunk.highTemp = hunk.highTempMark;
    hunk.highUsed = hunk.highTempMark;
    Hunk_ClearData();
}

/* CoDUOMP.exe 0x00435c10; coduo_lnxded 0x0806c2cd. */
qboolean Hunk_HighMarkIsSet(void)
{
    return hunk.highMark != HUNK_MARK_UNSET;
}

/* CoDUOMP.exe 0x00435c20; coduo_lnxded 0x0806c2f1. Exact same-module Mac
 * symbol: Hunk_SetMarkLow. */
void Hunk_SetMarkLow(void)
{
    hunk.lowMark = hunk.lowUsed;
}

/* CoDUOMP.exe 0x00435c30; coduo_lnxded 0x0806c300. Exact same-module Mac
 * symbol: Hunk_ClearToMarkLow. */
void Hunk_ClearToMarkLow(void)
{
    hunk.lowTemp = hunk.lowMark;
    hunk.lowUsed = hunk.lowMark;
    Hunk_ClearData();
}

/* CoDUOMP.exe 0x00435c70; coduo_lnxded 0x0806c31c. Exact same-module Mac
 * symbol: Hunk_Clear. Both original compilers inline Hunk_ClearData here. */
void Hunk_Clear(void)
{
    hunk.lowMark = 0;
    hunk.lowTempMark = 0;
    hunk.lowUsed = 0;
    hunk.lowTemp = 0;
    hunk.highMark = 0;
    hunk.highTempMark = 0;
    hunk.highUsed = 0;
    hunk.highTemp = 0;
    hunk_used = 0;

    Com_Printf("Hunk_Clear: reset the hunk ok\n");
    Hunk_ClearData();
    FS_RefreshLookupCache();
}

/*
 * CoDUOMP.exe 0x00435d20 and coduo_lnxded 0x0806c394 retain the same
 * server-game shutdown, hunk clear, and VM clear sequence.  The client engine
 * additionally shuts down cgame/UI before it and cinematics after it; those
 * unavailable dedicated-server edges remain target-owned.
 */
void Hunk_ClearToStart(void)
{
    HUNK_CLEAR_TO_START_PRE_SERVER();
    SV_ShutdownGameProgs();
    HUNK_CLEAR_TO_START_POST_SERVER();
    Hunk_Clear();
    VM_Clear();
}

/* CoDUOMP.exe 0x00435d40; coduo_lnxded 0x0806c3ab. Exact same-module Mac
 * symbol: Hunk_Shutdown. */
void Hunk_Shutdown(void)
{
    free(hunk_allocData);
    hunk_data = NULL;
    hunk_allocData = NULL;
}
