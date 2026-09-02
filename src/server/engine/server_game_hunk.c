#include "server_game_hunk.h"

#include "qcommon/hunk.h"
#include "qcommon/qcommon_runtime_types.h"
#include "qcommon/server_runtime_types.h"

extern serverHeader_t sv;

void Com_Error(errorParm_t code, const char *format, ...);

/*
 * Complete game-module hunk trap cluster:
 *
 *   CoDUOMP.exe   0x0045ccd0..0x0045cde3
 *   coduo_lnxded  0x0808e7a4..0x0808e897
 *
 * Both original engines use the same loading-state gate and diagnostics,
 * then forward to the corresponding common hunk operation. The canonical
 * names are retained from the Windows and supporting Mac symbols.
 */

void *SV_Hunk_AllocInternal(size_t size)
{
    if (sv.state != SS_LOADING) {
        Com_Error(ERR_DROP, "\x15trap_Hunk_Alloc can only be called in G_InitGame and "
                            "the first few frames of G_RunFrame\n");
    }
    return Hunk_AllocInternal(size);
}

void *SV_Hunk_AllocLowInternal(size_t size)
{
    if (sv.state != SS_LOADING) {
        Com_Error(ERR_DROP, "\x15trap_Hunk_AllocLow can only be called in G_InitGame "
                            "and the first few frames of G_RunFrame\n");
    }
    return Hunk_AllocLowInternal(size);
}

void *SV_Hunk_AllocAlignInternal(size_t size, size_t alignment)
{
    if (sv.state != SS_LOADING) {
        Com_Error(ERR_DROP, "\x15trap_Hunk_AllocAlign can only be called in G_InitGame "
                            "and the first few frames of G_RunFrame\n");
    }
    return Hunk_AllocAlignInternal(size, alignment);
}

void *SV_Hunk_AllocLowAlignInternal(size_t size, size_t alignment)
{
    if (sv.state != SS_LOADING) {
        Com_Error(ERR_DROP, "\x15trap_Hunk_AllocLowAlign can only be called in G_InitGame "
                            "and the first few frames of G_RunFrame\n");
    }
    return Hunk_AllocLowAlignInternal(size, alignment);
}

void *SV_Hunk_AllocateTempMemoryInternal(size_t size)
{
    return Hunk_AllocateTempMemoryInternal(size);
}

void SV_Hunk_FreeTempMemoryInternal(void *memory)
{
    Hunk_FreeTempMemory(memory);
}
