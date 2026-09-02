#include "../module/ui_functions.h"
#include "../module/ui_globals.h"

static const float ui_stringPoolReciprocal = 0x1.555556p-19f;
#if UINTPTR_MAX == UINT32_MAX
static const float ui_memoryPoolReciprocal = 0x1p-20f;
#else
/* NOT_FROM_ORIGINAL_SOURCE: native UI records contain widened host pointers,
 * so report utilization against the correspondingly enlarged native pool. */
static const float ui_memoryPoolReciprocal = 0x1p-21f;
#endif

// Source: uo_ui_mp_x86.dll 0x40011a50..0x40011ac9
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40011a50_40011ac9.mcode
// Exact same-module PPC symbol: String_Report.
void String_Report(void)
{
    Com_Printf("Memory/String Pool Info\n");
    Com_Printf("----------------\n");
    Com_Printf("String Pool is %.1f%% full, %i bytes out of %i used.\n",
               (double)(strPoolIndex * ui_stringPoolReciprocal *
                        100.0f),
               strPoolIndex,
               UI_STRING_POOL_CAPACITY);
    Com_Printf("Memory Pool is %.1f%% full, %i bytes out of %i used.\n",
               (double)(allocPoint * ui_memoryPoolReciprocal *
                        100.0f),
               allocPoint,
               UI_MEMORY_POOL_CAPACITY);
}

// Source: uo_ui_mp_x86.dll 0x400092f0..0x400092f5
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_400092f0_400092f5.mcode
// Exact same-module PPC symbol: UI_Report.
void UI_Report(void)
{
    String_Report();
}
