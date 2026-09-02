// Source: uo_cgame_mp_x86.dll at the RVAs noted below.
// Evidence: corresponding cgame_mp/mcode/uo_cgame_mp_x86/FUN_*.mcode records.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

#include <math.h>
#include <string.h>

void CG_FreeLocalEntity(localEntity_t *le) /* 0x3002aa30 */
{
    uint32_t countBits;
    localEntity_t *next;
    localEntity_t *prev;
    localEntity_t *freeHead;

    if (le->prev == NULL) {
        Com_ErrorMessage("CG_FreeLocalEntity: not active");
    }

    /* Preserve the target's individual loads and their order.  In particular,
     * next is loaded before DEC/store, while prev and next are reloaded around
     * the first link write rather than treated as one abstract unlink. */
    countBits = (uint32_t)cg_numLocalEntities; /* 0x3002aa42 */
    next = le->next;                           /* 0x3002aa47 */
    cg_numLocalEntities = coduo_int32_from_bits(countBits - 1u); /* aa4a..aa4b */
    prev = le->prev;                           /* 0x3002aa50 */
    prev->next = next;                         /* 0x3002aa52 */
    next = le->next;                           /* 0x3002aa55 */
    prev = le->prev;                           /* 0x3002aa58 */
    freeHead = cg_freeLocalEntities;            /* 0x3002aa5a */
    next->prev = prev;                         /* 0x3002aa60 */
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    le->prev = NULL;
    le->next = freeHead;
    cg_freeLocalEntities = le;
}

void CG_LoadingString(const char *text) /* 0x3002a4e0 */
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (text == NULL) {
        cg_loadingScratch[0] = '\0';
    } else {
        strncpy(cg_loadingScratch, text, sizeof(cg_loadingScratch) - 1);
        cg_loadingScratch[sizeof(cg_loadingScratch) - 1] = '\0';
    }
    if (text != NULL && text[0] != '\0') {
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
        Com_PrintMessage("%s", va("LOADING... %s\n", text));
    }
    cgame_syscall(CG_UPDATE_SCREEN);
}

void VectorLerp(const vec3_t from, const vec3_t to, float fraction, vec3_t out) /* 0x30023b20 */
{
    /* 0x30023b20: inverse stays unrounded in st(0) across the three
     * multiplies (FLD ST0 copies, no float store); each inverse*from[k]
     * is STORED to out[k] (rounds to float) and then reloaded by the
     * fraction*to[k] add -- two roundings per component, in this order. */
    long double inverse = 1.0f - fraction;
    out[0] = inverse * from[0];
    out[1] = inverse * from[1];
    out[2] = inverse * from[2];
    out[0] += fraction * to[0];
    out[1] += fraction * to[1];
    out[2] += fraction * to[2];
}

qboolean CG_CheckDrawScoreboardLine(int32_t *line, float y, float height) /* 0x30036ed0 */
{
    if (cg_scoreboardOverflowed != 0) {
        return qfalse;
    }
    if (*line < cg_scoreboardScrollPos) {
        *line = coduo_int32_from_bits((uint32_t)*line + 1u);
        return qfalse;
    }
    long double bottom = (long double)y + (long double)height;
    /* TEST AH,0x41 takes the draw path for ordered <= and unordered. */
    if (!(bottom > 432.0f)) {
        *line = coduo_int32_from_bits((uint32_t)*line + 1u);
        return qtrue;
    }
    cg_scoreboardOverflowed = qtrue;
    return qfalse;
}
