// Source: uo_cgame_mp_x86.dll 0x3002ad00..0x3002ada2
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002ad00_3002ada2.mcode

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/*
 * CG_AddLocalEntities — the per-frame cgame local-entity manager.
 *
 * Rejected .mcode header name `script_method_player_setentertime`: that is a pure
 * size guess (win size 0xa2 == matched size 0xa2, per the .mcode name_evidence).
 * It is contradicted by the machine code, which does no script/player-state work.
 * The function is proven to be the canonical Quake3/CoD CG_AddLocalEntities: it
 * walks the cg_activeLocalEntities (0x30537da0) doubly-linked list, expires nodes
 * past their endTime by unlinking them and pushing them onto cg_freeLocalEntities
 * (0x30537d80) — an inlined CG_FreeLocalEntity — and dispatches the live ones on
 * le->leType to the LE_* render handlers, exactly the sibling cluster at
 * CG_AllocLocalEntity (0x3002aa70) / CG_AddFadeRGB (0x3002abc0). The out-of-range
 * diagnostic string is "Bad leType: %i" (global_300778f8), a local-entity-specific
 * message that nails the identity.
 *
 * ABI: no arguments, no return (RET). ESI/EDI are the callee-saved scratch the
 * prologue pushes; the loop keeps the current node in ESI and its ->prev successor
 * in EDI. The handlers take the localEntity_t * in ESI (a register calling
 * convention); case 2 additionally loads it into EAX (MOV EAX,ESI) before the call.
 *
 * List-walk direction (proven by the .mcode): ESI starts at
 * cg_activeLocalEntities.prev (MOV ESI,[0x30537da0], the +0x0 = .prev field), the
 * loop tests `ESI == &cg_activeLocalEntities` (0x30537da0) for the sentinel, and
 * advances via EDI = le->prev (MOV EDI,[ESI]; ... MOV ESI,EDI). So it iterates the
 * active list backward through the ->prev links, which is why an entity unlinked
 * mid-iteration is safe: EDI (the next node) is captured before the unlink.
 *
 * Instruction-level proof of each statement:
 *   3002ad01  MOV ESI,[0x30537da0]        le = cg_activeLocalEntities.prev
 *   3002ad07  CMP ESI,0x30537da0          empty-list test against the sentinel addr
 *   3002ad0d  JZ  0x3002ada0              if empty -> return
 * loop (3002ad14):
 *   3002ad14  MOV EAX,[0x304831b0]        EAX = cg_time
 *   3002ad19  CMP EAX,[ESI+0x10]          cmp cg_time, le->endTime  (signed, JL below)
 *   3002ad1c  MOV EDI,[ESI]               EDI = le->prev  (captured next-to-visit)
 *   3002ad1e  JL  0x3002ad5c             if cg_time <  le->endTime -> still alive (dispatch)
 *   -- expired branch (cg_time >= le->endTime): inlined CG_FreeLocalEntity(le) --
 *   3002ad20  TEST EDI,EDI                le->prev
 *   3002ad22  JNZ 0x3002ad31              if le->prev != 0 skip error
 *   3002ad24  PUSH 0x30077908 ; CALL ...  Com_ErrorMessage("CG_FreeLocalEntity: not active")
 *   3002ad31  MOV EAX,[0x30134cfc]        EAX = cg_numLocalEntities
 *   3002ad36  MOV EDX,[ESI+0x4]           EDX = le->next
 *   3002ad39  MOV ECX,[ESI]               ECX = le->prev
 *   3002ad3b  MOV [ECX+0x4],EDX           le->prev->next = le->next
 *   3002ad46  DEC EAX ; MOV [0x30134cfc],EAX   --cg_numLocalEntities
 *   3002ad4c  MOV EAX,[ESI+0x4] ; MOV [EAX],ECX   le->next->prev = le->prev
 *   3002ad40  MOV EDX,[0x30537d80]        EDX = cg_freeLocalEntities
 *   3002ad51  MOV [ESI+0x4],EDX           le->next = cg_freeLocalEntities
 *   3002ad54  MOV [0x30537d80],ESI        cg_freeLocalEntities = le
 *   3002ad5a  JMP 0x3002ad91              -> advance
 *   -- alive branch (dispatch on leType) --
 *   3002ad5c  MOV ECX,[ESI+0x8]           ECX = le->leType
 *   3002ad61  SUB EAX,0x0 ; JZ 0x3002ad8c     case 0 -> CG_AddFadeRGB
 *   3002ad66  DEC EAX      ; JZ 0x3002ad85     case 1 -> CG_AddScaleFade (0x3002ac20)
 *   3002ad69  DEC EAX      ; JZ 0x3002ad7c     case 2 -> CG_AddMovingTracer (0x3002ab00)
 *   3002ad6c  PUSH ECX ; PUSH 0x300778f8 ; CALL Com_ErrorMessage("Bad leType: %i", leType)
 *   3002ad91  CMP EDI,0x30537da0 ; MOV ESI,EDI ; JNZ 0x3002ad14   advance & loop until sentinel
 */
void CG_AddLocalEntities(void)
{
    localEntity_t *le;
    localEntity_t *next;

    /* le = cg_activeLocalEntities.prev; if it is the sentinel itself, list empty. */
    le = cg_activeLocalEntities.prev;
    if (le == &cg_activeLocalEntities) {
        return;
    }

    do {
        /* 0x3002ad14 reads the signed target clock before 0x3002ad1c captures
         * the next node to visit. */
        int32_t now = coduo_int32_from_bits(cg_time);
        next = le->prev;

        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (now >= le->endTime) {
            uint32_t countBits;
            localEntity_t *linkNext;
            localEntity_t *linkPrev;
            localEntity_t *freeHead;

            /* Expired: inlined CG_FreeLocalEntity(le). */
            if (le->prev == 0) {
                Com_ErrorMessage("CG_FreeLocalEntity: not active");
            }

            /* The list-walk `next` above remains the pre-error traversal value;
             * the unlink itself performs fresh loads in the DLL's exact order. */
            countBits = (uint32_t)cg_numLocalEntities;       /* 0x3002ad31 */
            linkNext = le->next;                             /* 0x3002ad36 */
            linkPrev = le->prev;                             /* 0x3002ad39 */
            linkPrev->next = linkNext;                       /* 0x3002ad3b */
            linkPrev = le->prev;                             /* 0x3002ad3e */
            freeHead = cg_freeLocalEntities;                  /* 0x3002ad40 */
            cg_numLocalEntities = coduo_int32_from_bits(countBits - 1u); /* ad46..47 */
            linkNext = le->next;                             /* 0x3002ad4c */
            linkNext->prev = linkPrev;                       /* 0x3002ad4f */
            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            le->prev = NULL;
            /* Push le onto the head of the free list. */
            le->next = freeHead;
            cg_freeLocalEntities = le;
        } else {
            /* Still alive: dispatch on the leType discriminant. le is passed in ESI. */
            switch (le->leType) {
            case LE_FADE_RGB:
                CG_AddFadeRGB(le);
                break;
            case LE_SCALE_FADE:
                CG_AddScaleFade(le);
                break;
            case LE_MOVING_TRACER:
                CG_AddMovingTracer(le);
                break;
            default:
                Com_ErrorMessage("Bad leType: %i", le->leType);
                break;
            }
        }

        le = next;
    } while (le != &cg_activeLocalEntities);
}
