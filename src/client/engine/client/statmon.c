#include "../q_shared.h"

#include "cgame.h"
#include "debug_lines.h"
#include "../effects/fx_api.h"

#include <string.h>

enum {
    STATMON_SHADER_LOAD_MODE = 1
};

statmon_entry_t statmonEntries[STATMON_ENTRY_CAPACITY]; /* 0x009cd328 */
int32_t statmonEntryCount;                              /* 0x009cd528 */

/* Source: CoDUOMP.exe 0x00457300..0x0045739f.
 * Name and source structure: exact same-module Mac symbol StatMon_Warning.
 * The Windows compiler inlines Sys_Milliseconds and the internal
 * StatMon_UpdateEntry body whose invalid-index diagnostic survives here. */
void StatMon_Warning(int32_t entryIndex, int32_t durationMsec, const char *shaderName)
{
    if (com_statmon->integer == 0)
        return;

    if (entryIndex < 0 || entryIndex >= STATMON_ENTRY_CAPACITY) {
        Com_Error(ERR_DROP,
                  "\x15"
                  "StatMon_UpdateEntry: invalid entry '%i'\n",
                  entryIndex);
    }

    statmon_entry_t *const entry = &statmonEntries[entryIndex];
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    entry->expireTime = (int32_t)(Sys_Milliseconds() + (uint32_t)durationMsec);
    if (entry->shaderHandle == 0 && cls.rendererStarted != qfalse) {
        entry->shaderHandle = RE_RegisterShaderNoMip(shaderName, STATMON_SHADER_LOAD_MODE);
    }

    if (entryIndex >= statmonEntryCount)
        statmonEntryCount = entryIndex + 1;
}

/* Source: CoDUOMP.exe 0x004573a0..0x004573af, recovered from the executable
 * gap following StatMon_Warning. Name and outputs: exact same-module Mac
 * symbol StatMon_GetStatsArray. */
void StatMon_GetStatsArray(statmon_entry_t **entries, int32_t *entryCount)
{
    *entries = statmonEntries;
    *entryCount = statmonEntryCount;
}

/* Source: CoDUOMP.exe 0x004573b0..0x004573c6, recovered from the same broken
 * Ghidra gap. Name and 512-byte clear: exact same-module Mac symbol
 * StatMon_Reset. */
void StatMon_Reset(void)
{
    memset(statmonEntries, 0, sizeof(statmonEntries));
    statmonEntryCount = 0;
}
