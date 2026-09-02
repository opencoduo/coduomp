// Source: uo_cgame_mp_x86.dll 0x30029f70..0x30029ffa
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30029f70_30029ffa.mcode

#include "../client_recovered.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*
 * CG_DrawHudElemString — a member of the CG_R_TEXT_PAINT (cgame trap 54, a 2D
 * draw/text service) emitter family. It computes the vertically aligned text
 * baseline for a draw item, then hands the item to the engine via
 * cgame_syscall(54, ...). The .mcode header's size-matched name guess
 * "script_func_isalive" (win size 0x8a == matched size 0x8a) is REJECTED — this
 * function performs no entity/script liveness test; it is a small x87 alignment
 * computation feeding a trap-54 emit, in the same family as the area-chat trio
 * (0x30031940 / 0x30031a00 / 0x300319a0) and the other emitters catalogued in
 * client_recovered.h. The exact name is anchored by the same-module Mac
 * traceback symbol order. Both callers (0x3002a351 and 0x3002a38b, inside
 * FUN_3002a310) build a cgAlignedDrawItem on the stack, load its address into EAX,
 * pass their own `this` object in EDX (the alignment context), and push a single
 * cdecl string-pointer word.
 *
 * ABI (register in/out): item pointer in EAX, align-context pointer in EDX; one
 * cdecl stack word (stringArg) follows. The function ends in a plain RET — it does
 * NOT clean the incoming stack arg; the caller (add esp,4 at 0x3002a35e /
 * 0x3002a390) cleans it. The in-body ADD ESP,0x38 unwinds this frame's
 * SUB ESP,0x10 plus the ten dwords pushed for the syscall.
 *
 * Machine-code proof of the aligned coordinate (x87, single precision throughout):
 *   30029f73 MOV [ESP],item->fontHeight        (spill float @ +0x28 into local L0)
 *   30029f79 MOV ECX, ctx->alignMode       (dword @ EDX+0x18)
 *   30029f7c SUB ECX,0 ; JZ  case TOP (0)
 *   30029f81 DEC ECX   ; JZ  case MIDDLE (1)
 *   30029f84 DEC ECX   ; JZ  case BOTTOM (2)
 *   default : FLD [0x3007bcec]  -> 0.0f          (floatZero)
 *   BOTTOM : FLD item->height ; FADD item->y ; FSUB L0
 *   MIDDLE : FLD item->height ; FSUB L0 ; FMUL 0.5f ; FADD item->y
 *   TOP    : FLD item->y
 *   merge  : FADD item->fontHeight
 * so the text baseline per mode is:
 *   TOP    : y + fontHeight
 *   MIDDLE : y + (height + fontHeight) * 0.5
 *   BOTTOM : y + height
 *   other  : fontHeight
 * (The FLD/FADD/FSUB operand at ESP is the item->fontHeight spill, because ESP has not
 *  moved between the spill at 30029f76 and these reads at 30029f95/30029f9d.)
 *
 * The coordinate is forwarded through a 32-bit syscall dword as its float bit
 * pattern (FSTP into the argument slot; no FILD/int conversion).
 *
 * Argument-vector proof (push order reversed = call order), frame base F = ESP
 * after SUB ESP,0x10; return addr @ F+0x10, first stack arg @ F+0x14:
 *   FSTP [F+0x8]                 coordinate bits -> scratch (via ESP+0xc after PUSH 3)
 *   PUSH 0x36                    -> arg0 = 54 (CG_R_TEXT_PAINT)
 *   PUSH item->xBits (EAX+0x0)   -> arg1
 *   PUSH coordBits   (F+0x8)     -> arg2
 *   PUSH item->font (EAX+0x20) -> arg3
 *   PUSH item->fontScaleBits (EAX+0x24) -> arg4
 *   PUSH &item->color[0] (EAX+0x30 via LEA) -> arg5 (RGBA color block address)
 *   PUSH stringArg   (F+0x14)    -> arg6
 *   PUSH item->fontWidthBits (EAX+0x2c) -> arg7
 *   PUSH 0x0                     -> arg8
 *   PUSH 0x3                     -> arg9
 *
 * Float constants: 0.5f lives at rdata 0x3007bce8 (floatOneHalf,
 * stored as a uint32 bit pattern shared 155x) and 0.0f at rdata 0x3007bcec
 * (floatZero, a widely shared zero dword read here as a float).
 * They are the CENTER-mode 0.5 scale and the default-mode 0.0 respectively; read
 * as floats via memcpy from the shared bit-pattern globals to preserve their
 * canonical rdata identity.
 */

/* Provenance offset asserts for the two provisional descriptor structs. The item
 * carries char* fields (label @ +0x10, text @ +0x18, added when
 * CG_DrawSingleHudElem was
 * reconstructed), so offsets past +0x10 only match the 32-bit target ABI; guard
 * those against 4-byte pointer width. */
_Static_assert(offsetof(cgAlignedDrawItem, x) == 0x00, "item.x @ +0x00");
_Static_assert(offsetof(cgAlignedDrawItem, y) == 0x04, "item.y @ +0x04");
_Static_assert(offsetof(cgAlignedDrawItem, height) == 0x0c, "item.height @ +0x0c");
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
_Static_assert(offsetof(cgAlignedDrawItem, font) == 0x20, "item.font @ +0x20");
_Static_assert(offsetof(cgAlignedDrawItem, fontScale) == 0x24, "item.fontScale @ +0x24");
_Static_assert(offsetof(cgAlignedDrawItem, fontHeight) == 0x28, "item.fontHeight @ +0x28");
_Static_assert(offsetof(cgAlignedDrawItem, fontWidth) == 0x2c, "item.fontWidth @ +0x2c");
_Static_assert(offsetof(cgAlignedDrawItem, color) == 0x30, "item.color @ +0x30");
#endif
_Static_assert(offsetof(hudElem_t, alignY) == 0x18, "hudElem.alignY @ +0x18");

/* 0x30029f30..0x30029f64: alignment-only sibling used by the HUD setup path.
 *
 * Returns UNROUNDED st(0): every one of the four branches RETs straight off the
 * x87 stack with no FSTP (0x30029f3e/f44, 0x30029f45..f4f, 0x30029f50..f60,
 * 0x30029f61/f64) -- the house `long double` idiom for a raw-st0 return. A float
 * return type would insert a round-to-float at each `return` that the DLL does
 * not perform. The decl is local to this file (no header prototype, no callers
 * in the tree), so the retype is contained here. */
// Source RVA: 0x30029f30
long double CG_HudElemAlignY(const hudElem_t *elem, const cgAlignedDrawItem *item, float fontHeight)
{
    float half;
    float zero;

    /* Keep the original m32 constant operands.  A direct long-double cast of
     * the literal is exact numerically, but GCC can materialize it as an m80
     * constant instead of the target's FLD/FMUL dword operands. */
    memcpy(&half, &floatOneHalf, sizeof(half));
    memcpy(&zero, &floatZero, sizeof(zero));

    switch (elem->alignY) {
    case HUDELEM_ALIGN_START:
        return (long double)item->y;
    case HUDELEM_ALIGN_CENTER:
        return ((long double)item->height - (long double)fontHeight) * (long double)half + (long double)item->y;
    case HUDELEM_ALIGN_END:
        return (long double)item->height + (long double)item->y - (long double)fontHeight;
    default:
        return (long double)zero;
    }
}

// Source RVA: 0x30029f70
void CG_DrawHudElemString(cgAlignedDrawItem *item, hudElem_t *elem, const char *string)
{
    /* 0.5f and 0.0f rdata constants read as floats from their shared bit patterns. */
    float half;
    float zero;
    memcpy(&half, &floatOneHalf, sizeof(half));
    memcpy(&zero, &floatZero, sizeof(zero));

    /* long double: no branch stores its result -- each case leaves the value in
     * st0 and falls into the SHARED FADD item->fontHeight (0x30029fb1), with the ONLY
     * rounding at the single FSTP into the argument slot (0x30029fbd). A float
     * `coord` would round twice (once per branch, once at the +=). */
    long double coord;
    switch (elem->alignY) {
    case HUDELEM_ALIGN_START:                      /* alignMode == 0 */
        coord = (long double)item->y;             /* 0x30029fab FLD, no store */
        break;
    case HUDELEM_ALIGN_CENTER:                     /* alignMode == 1 */
        coord =
            ((long double)item->height - (long double)item->fontHeight) * (long double)half + (long double)item->y; /* 0x30029f9a..fa6 */
        break;
    case HUDELEM_ALIGN_END: /* alignMode == 2 */
        coord = (long double)item->height + (long double)item->y - (long double)item->fontHeight; /* 0x30029f8f..f95 */
        break;
    default:
        coord = (long double)zero; /* 0x30029f87 FLD, no store */
        break;
    }
    coord += (long double)item->fontHeight; /* shared FADD [EAX+0x28] at merge */

    /* forward the coordinate through a syscall dword as its 32-bit float bits.
     * The FSTP at 0x30029fbd is the single round-to-float of the whole chain. */
    float coordRounded = (float)coord;
    int32_t coordBits;
    memcpy(&coordBits, &coordRounded, sizeof(coordBits));

    cgame_syscall(CG_R_TEXT_PAINT, item->xBits, coordBits, item->font, item->fontScaleBits, (intptr_t)&item->color[0], (intptr_t)string,
                  item->fontWidthBits, 0, 3);
}
