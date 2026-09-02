// Source: uo_cgame_mp_x86.dll 0x3002a310..0x3002a3e3
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002a310_3002a3e3.mcode

#include "../client_recovered.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*
 * CG_DrawSingleHudElem — draw a single HUD element.
 *
 * The .mcode header's size-matched guess "VectorNormalize2" is REJECTED: this is
 * not a vector-math routine. It reserves a >0x2000-byte /GS-protected stack frame,
 * builds a cgAlignedDrawItem from the hud element, conditionally emits up to two
 * aligned strings through CG_DrawHudElemString (the CG_R_TEXT_PAINT 2D-draw service),
 * and dispatches the remaining draw work on the element's `type` discriminant.
 * The identification is by behavior + call graph:
 *   - `this` (ECX) is a hudElem_s: the switch reads elem->type (+0x00) and the
 *     valid range 1..9 matches the game_mp_uo hudElemType_t enum exactly.
 *   - the SHADER (3) branch calls CG_DrawHudElemShader (0x3002a1d0), which reads
 *     elem->materialIndex/shaderRightTexcoord/shaderBottomTexcoord — a shader draw;
 *   - the CLOCK/CLOCK_UP (8,9) branch calls CG_DrawHudElemClock (0x3002a000);
 *   - the TEXT/VALUE/TIMER* (1,2,4,5,6,7) branches emit a string.
 * The same-module PPC name bank names these siblings CG_DrawHudElemShader /
 * CG_DrawHudElemClock / CG_DrawHudElemString, confirming the cluster. The Mac
 * symbol CG_DrawSingleHudElem shares the shader and clock callees and additionally
 * calls the platform string helper, resolving the source name.
 *
 * ABI: the hud element arrives in ECX (`this`). Plain RET (cdecl, no stack args).
 * The prologue is the standard MSVC __chkstk + /GS pattern:
 *   3002a310 MOV EAX,0x2044 ; CALL __chkstk (0x30060a30)   reserve 0x2044 frame
 *   3002a31a MOV EAX,[__security_cookie 0x30081650]         snapshot /GS cookie
 *   3002a31f PUSH EDI                                       (this saved in EDI)
 *   3002a320 MOV [ESP+0x2044],EAX                           store cookie at frame top
 * epilogues verify via __security_check_cookie (0x30061639) with the reloaded
 * cookie in ECX, then ADD ESP,0x2044 / RET. Those are calling-convention details,
 * expressed here as plain C.
 *
 * Machine-code proof of the body (frame base F = ESP after the PUSH EDI; the
 * cgAlignedDrawItem `item` lives at F+4, a 0x2000-byte scratch buffer at F+0x44):
 *
 *   build item:
 *     3002a327 PUSH 0x2000            scratchLen
 *     3002a32c LEA EAX,[&scratch]     PUSH -> scratch buffer @ F+0x44
 *     3002a333 PUSH EDI               -> hud element (this)
 *     3002a334 LEA EAX,[&item]        EAX = &item @ F+4
 *     3002a338 CALL 0x30029c00        CG_GetHudElemInfo(item, this, scratch, 0x2000)
 *     3002a33d MOV EAX,[F+0x14]       EAX = item->label (+0x10)      (before the pop)
 *     3002a341 MOV CL,[EAX]           first byte of label
 *     3002a343 ADD ESP,0xc           caller-clean the 3 pushed args
 *     3002a346 TEST CL,CL ; JZ skip   emit only if label non-empty
 *       3002a34a PUSH EAX             stringArg = item->label
 *       3002a34b LEA EAX,[&item]      item ptr
 *       3002a34f MOV EDX,EDI          ctx = this (hud element)
 *       3002a351 CALL 0x30029f70      CG_DrawHudElemString(item, this, item->label)
 *       3002a356 FLD  [F+4]           item->x (float)
 *       3002a35a FADD [F+0x18]        + item->labelWidth  (+0x14)
 *       3002a35e ADD ESP,4
 *       3002a361 FSTP [F+4]           item->x += labelWidth
 *
 *   dispatch on this->type:
 *     3002a365 MOV EAX,[EDI]          EAX = elem->type
 *     3002a367 DEC EAX ; CMP EAX,8 ; JA default   (type-1 in 0..8, i.e. type 1..9)
 *     3002a36d MOVZX ECX,[EAX + 0x3002a3f0]        index table (bytes):
 *                                                  {0,0,1,0,0,0,0,2,2}
 *     3002a374 JMP [ECX*4 + 0x3002a3e4]            jump table (dwords):
 *                {0x3002a37b, 0x3002a3c6, 0x3002a3a7}
 *       -> case index 0 (types 1,2,4,5,6,7): block 0x3002a37b (string emit)
 *       -> case index 1 (type 3):            block 0x3002a3c6 (shader)
 *       -> case index 2 (types 8,9):         block 0x3002a3a7 (clock)
 *
 *   string block 0x3002a37b:
 *     MOV EAX,[F+0x1c]=item->text (+0x18) ; CMP [EAX],0 ; JZ default
 *     PUSH EAX ; LEA EAX,[&item] ; MOV EDX,EDI ; CALL 0x30029f70
 *       -> if item->text non-empty: CG_DrawHudElemString(item, this, item->text)
 *   shader block 0x3002a3c6:
 *     LEA ECX,[&item] ; CALL 0x3002a1d0   -> CG_DrawHudElemShader(item, this)  (EDI=this)
 *   clock block 0x3002a3a7:
 *     LEA ECX,[&item] ; MOV EDX,EDI ; CALL 0x3002a000 -> CG_DrawHudElemClock(item, this)
 *   default: no draw.
 *
 * The jump/index tables at 0x3002a3e4/0x3002a3f0 were dumped from the binary
 * (.rdata just past the function) to prove the type->branch mapping above.
 *
 * label/text are C strings whose non-empty test is a single byte compare against
 * 0; item->x and item->labelWidth are accessed as 32-bit floats (single-precision x87
 * FLD/FADD/FSTP), so the accumulate is done in float.
 */

/* Provenance asserts for the item fields this function proves. The item carries
 * char* fields (label @ +0x10, text @ +0x18), so offsets past +0x10 only match
 * the 32-bit target ABI; guard those against 4-byte pointer width. */
_Static_assert(offsetof(cgAlignedDrawItem, x) == 0x00, "item.x @ +0x00");
_Static_assert(offsetof(cgAlignedDrawItem, label) == 0x10, "item.label @ +0x10");
_Static_assert(offsetof(hudElem_t, type)          == 0x00, "hudElem.type @ +0x00");
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
_Static_assert(offsetof(cgAlignedDrawItem, labelWidth) == 0x14,
               "item.labelWidth @ +0x14");
_Static_assert(offsetof(cgAlignedDrawItem, text) == 0x18, "item.text @ +0x18");
#endif

void CG_DrawSingleHudElem(struct hudElem_s *elem)
{
    /* The 0x2000-byte scratch string buffer lives in the reserved frame; it is
     * only handed to the item builder (which formats strings into it) and never
     * read directly here. */
    cgAlignedDrawItem item;
    char scratch[8192];

    CG_GetHudElemInfo(&item, elem, scratch, sizeof(scratch));


    /* First string: drawn if non-empty; then x += labelWidth (both as floats). */
    if (item.label[0] != '\0') {
        CG_DrawHudElemString(&item, elem, item.label);

        item.x += item.labelWidth;
    }

    switch (elem->type) {
    case HE_TYPE_TEXT:            /* 1 */
    case HE_TYPE_VALUE:          /* 2 */
    case HE_TYPE_TIMER:          /* 4 */
    case HE_TYPE_TIMER_UP:       /* 5 */
    case HE_TYPE_TENTHS_TIMER:   /* 6 */
    case HE_TYPE_TENTHS_TIMER_UP: /* 7 */
        /* Second string: drawn if non-empty. */
        if (item.text[0] != '\0') {
            CG_DrawHudElemString(&item, elem, item.text);
        }
        break;

    case HE_TYPE_SHADER:         /* 3 */
        CG_DrawHudElemShader(&item, elem);
        break;

    case HE_TYPE_CLOCK:          /* 8 */
    case HE_TYPE_CLOCK_UP:       /* 9 */
        CG_DrawHudElemClock(&item, elem);
        break;

    default:
        /* type outside 1..9 (or the index-table default): no draw. */
        break;
    }
}
