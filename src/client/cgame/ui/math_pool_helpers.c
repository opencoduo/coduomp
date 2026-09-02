// Complete UI/common leaves recovered from exact machine-code bodies.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

// Source RVA: 0x3001c490
void CG_AdjustFrom640Rect(float *x, float *y, float *w, float *h)
{
    *x *= cgs_screenXScale;
    *y *= cgs_screenYScale;
    *w *= cgs_screenXScale;
    *h *= cgs_screenYScale;
}

void CG_FreeMarkPoly(markPoly_t *mark) /* 0x3002e450 */
{
    if (mark->prevMark == 0)
        Com_ErrorMessage("CG_FreeLocalEntity: not active");

    mark->prevMark->nextMark = mark->nextMark;
    mark->nextMark->prevMark = mark->prevMark;
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    mark->prevMark = NULL;
    mark->nextMark = cg_freeMarkPolys;
    cg_freeMarkPolys = mark;
}

void UI_ReportMemory(void) /* 0x3004ff30 */
{
    enum { UI_POOL_BYTES = 0x20000 };

    Com_Printf("Memory/String Pool Info\n");
    Com_Printf("----------------\n");
    Com_Printf("String Pool is %.1f%% full, %i bytes out of %i used.\n",
               (double)strPoolIndex * (double)0x1p-17f * (double)100.0f,
               strPoolIndex, UI_POOL_BYTES);
    Com_Printf("Memory Pool is %.1f%% full, %i bytes out of %i used.\n",
               (double)allocPoint * (double)0x1p-17f * (double)100.0f,
               allocPoint, UI_POOL_BYTES);
}
