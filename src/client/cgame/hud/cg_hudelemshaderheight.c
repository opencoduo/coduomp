// Source: uo_cgame_mp_x86.dll 0x30029980..0x300299dd
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30029980_300299dd.mcode
//
// Assigned name in the .mcode header was `_Vector5Scale`, adopted only by a size
// match (win 0x5d == matched 0x5d). REJECTED: _Vector5Scale is
// `void _Vector5Scale(const float*, float, float*)` (a 5-component vector scale),
// which has no FILD, no cg.time read, and no x87 reverse-divide. This function
// instead evaluates a time-based linear interpolation of a value node and returns
// a scalar in ST0. Named by behavior; see below.
//
// This is one evaluation mode of a small value-node evaluator dispatched by the
// caller at 0x30029a30 (__fastcall, this in ESI), which switches on the node type
// discriminant `[EAX+0]` through a 9-entry jump table: type 1 -> base value, type
// 2 (and 8,9) -> this lerp helper. The immediately-preceding sibling
// FUN_30029920_3002997d is byte-for-byte identical except it interpolates the
// field pair at +0x30/+0x3c instead of +0x34/+0x40, so the two evaluate two
// separate width/height tracks of the same HUD element. The exact name
// CG_HudElemShaderHeight is anchored by the same-module Mac traceback symbol
// order.
//
// EAX points at the value node; ESI is the caller's owning object whose float
// field at +0x28 supplies the default component value when a node integer field
// is zero. Both are caller-supplied registers (leaf reads only), so the ABI here
// is register-passed (EAX = node, ESI = owner); expressed as explicit params.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/*
 * CG_HudElemShaderHeight: return the current interpolated value of the +0x34/+0x40
 * track of a value node.
 *
 * result = prev + (goal - prev) * (elapsed / duration), where
 *   elapsed  = cg.time - node->startTime   (clamped implicitly: see guards)
 *   goal     = node->goalTrackB or owner->defaultValue when goalTrackB == 0
 *   prev     = node->prevTrackB or owner->defaultValue when prevTrackB == 0
 *
 * Guards (both return `goal` directly, i.e. the settled endpoint):
 *   - duration <= 0                       (signed JLE at 0x3002999e)
 *   - elapsed  >= duration                (signed JGE at 0x300299af)
 */
long double CG_HudElemShaderHeight(const hudElem_t *elem, const cgAlignedDrawItem *item)
{
    /* Float faithfulness: this function performs NO float store anywhere
     * (0x30029980..0x300299dc contains not one FST/FSTP DWORD). goal, prev and
     * frac all live in x87 registers from their FILD/FLD to their final use, and
     * the result is returned raw in st0. `float` locals would round at each
     * assignment; long double is the project's raw-register carrier. The
     * return type carries raw st(0) to the caller without a float conversion:
     * CG_HudElemHeight FCOMs it against the clamp floor without storing it
     * first (0x30029a5a), which a float return would round. The two callers that
     * store the result to a float slot immediately (cg_drawhudelemshader.c,
     * cg_drawhudelemclock.c) are unaffected -- their float assignment rounds
     * once, matching the DLL FSTP. */

    /* goal endpoint: node integer or owner default when the integer is 0.
     * FILD of a stack-stored int (30029983..30029990) vs FLD owner->defaultValue
     * (30029992). The FILD has no float store after it, so the int stays exact. */
    long double goal;
    if (elem->height != 0) {
        goal = (long double)elem->height;       /* FILD dword [ESP] */
    } else {
        goal = item->fontHeight;                    /* FLD float [ESI+0x28] */
    }

    int32_t duration = elem->scaleTime;   /* MOV EDX,[EAX+0x48]; [ESP+4]=EDX */
    if (duration <= 0) {
        return goal;                      /* JLE 0x300299d9: settled, ST0 = goal */
    }

    /* elapsed = cg.time - startTime (signed subtract; stored at [ESP+8]). */
    int32_t elapsed = coduo_int32_from_bits((uint32_t)cg_time - (uint32_t)elem->scaleStartTime);
    if (elapsed >= duration) {
        return goal;                      /* JGE 0x300299d9: window elapsed, ST0 = goal */
    }

    /* previous endpoint: node integer or owner default when the integer is 0.
     * FILD [EAX+0x40] (300299bb) vs FLD owner->defaultValue (300299c0). */
    long double prev;
    if (elem->scaleFromHeight != 0) {
        prev = (long double)elem->scaleFromHeight;
    } else {
        prev = item->fontHeight;
    }

    /* fraction = elapsed / duration.
     * 300299c3 FILD [ESP+4]            -> duration (exact, no float store)
     * 300299c7 FDIVR float [0x3007bce0]-> 1.0f / duration   (g_const_float_one)
     * 300299cd FIMUL [ESP+8]           -> (1.0f/duration) * elapsed
     * The 1.0f is floatOne (bit pattern 0x3f800000). Note FIMUL consumes the
     * INTEGER elapsed straight into the x87 chain -- there is no float
     * conversion of elapsed in the instruction stream. */
    const long double frac = (1.0f / (long double)duration) * (long double)elapsed;

    /* 300299d1 FXCH ST2 / 300299d3 FSUB ST0,ST1 -> (goal - prev)
     * 300299d5 FMULP ST2               -> frac * (goal - prev)
     * 300299d7 FADDP                   -> st1+st0 = frac*(goal-prev) + prev
     * (product-first addend order mirrors the FADDP; a single pairwise FADD is
     * commutative and bit-exact either way, so this is form, not value). */
    return frac * (goal - prev) + prev;
}
