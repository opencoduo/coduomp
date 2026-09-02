// Small machine-code-exact accessors and default callbacks. Each body maps
// directly to the complete instruction record named in the adjacent comment.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"
#include "client/menu/ui_display_context_types.h"

// 0x300162f0 / 0x30016300 / 0x30016330: cg_scriptExports defaults.
// Source RVA: 0x300162f0
scr_function_callback_t CGAME_ABI_CDECL Scr_GetFunction(const char **name, int32_t *developerOnly)
{
    (void)name;
    (void)developerOnly;
    return NULL;
}
// Source RVA: 0x30016300
scr_method_callback_t CGAME_ABI_CDECL Scr_GetMethod(const char **name, int32_t *developerOnly)
{
    (void)name;
    (void)developerOnly;
    return NULL;
}
// Source RVA: 0x30016330
void *CGAME_ABI_CDECL Scr_LoadRead(uint32_t size)
{
    (void)size;
    return NULL;
}

// 0x3002d470: default owner-draw key handler never consumes a key.
qboolean CG_OwnerDrawHandleKey(int32_t ownerDraw, int32_t flags, float *special, int32_t key)
{
    (void)ownerDraw;
    (void)flags;
    (void)special;
    (void)key;
    return 0;
}

// 0x3002d500 / 0x3002d510: empty feeder providers.
// Source RVA: 0x3002d500
const char *CG_FeederItemText(float feederID, int32_t index, int32_t column, int32_t *handleOut)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    *handleOut = UI_FEEDER_IMAGE_HANDLE_NONE;
    (void)feederID;
    (void)index;
    (void)column;
    /* 0x3002d500 returns the shared .rdata object at 0x30074a0c, not an
     * arbitrary equal-content literal. */
    return g_str_empty;
}

// Source RVA: 0x3002d510
int32_t CG_FeederItemImage(float feederID, int32_t index)
{
    (void)feederID;
    (void)index;
    return 0;
}
