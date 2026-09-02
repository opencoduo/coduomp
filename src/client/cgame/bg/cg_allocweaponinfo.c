// Source: uo_cgame_mp_x86.dll 0x3000fe00..0x3000fed1
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3000fe00_3000fed1.mcode
//
// CG_AllocWeaponInfo — allocate and default-initialize a weaponInfo_t record during
// weapon-definition parsing.
//
// The .mcode header's assigned name `Scr_PlayerKilled` is REJECTED: it was a pure
// size match (win size 0xd1 == matched size 0xd1) with no behavioral basis. The
// machine code allocates a 0x4bc-byte weaponInfo_t, registers it in the bg_weaponInfos
// pointer array, and defaults its string fields — it is weapon-info allocation, not
// a script player-killed callback. Name is provisional-by-role (no cgame symbol
// table recovered); the "CG_AllocWeaponInfo" role is proven by:
//   - allocation size 0x4bc == sizeof(weaponInfo_t) (server weaponInfo_s extent);
//   - the result stored into bg_weaponInfos[index] (0x30134cd8) with weaponIndex
//     (+0x00) = index and name (+0x04) = CopyString("");
//   - the field descriptor table it walks (0x300843a8: "displayName"@0x08,
//     "gunModel"@0x10, "modeName"@0x78, ...) matching weaponInfo_t field offsets.
//
// Nonstandard calling convention (register + stack args, plain RET / caller-cleaned):
//   EAX = fieldCount, ECX = index, [ESP+arg0] = fields table pointer.
// EBX/ESI/EDI/EBP are callee-saved (pushed at entry, popped at exit).
//
// Machine-code notes:
//   0x3000fe11 CALL [cgame_syscall]  cgame_syscall(206 /*0xce*/, 0x4bc)  -> weaponInfo_t*
//   0x3000fe1e MOV [bg_weaponInfos + index*4], weaponInfo
//   0x3000fe21 MOV [weaponInfo], index                 weaponInfo->weaponIndex = index
//   0x3000fe23 MOV AL,[g_str_empty]; TEST AL,AL        empty-string fast path test
//     if the source byte is 0: name = cg_emptyString (0x300a7e34, cached "").
//     else: buf = cgame_syscall(208 /*0xd0/, 1, 1); strcpy(buf, g_str_empty).
//   0x3000fe62 TEST EDI,EDI; JLE ...                   skip the field loop if count<=0
//   field loop (0x3000fe72..0x3000fec8): for each descriptor, only type-0 (string)
//     fields are defaulted; the field pointer is (char*)weaponInfo + offset, set to
//     either cg_emptyString or a fresh CopyString("").
//   0x3000fecd MOV EAX,EBX; RET                        returns the new weaponInfo_t.
//
// The empty-string fast path (byte at g_str_empty is 0) is always taken at runtime
// because g_str_empty is the read-only "" literal; the CopyString branch is a
// faithful translation of the emitted code, not observed to run for this source.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/*
 * cgame_syscall(CG_HUNK_ALLOC_LOW_ALIGN_EXPLICIT, len, 1) then strcpy the NUL-terminated source
 * into the returned buffer. Mirrors the byte-copy idiom at 0x3000fe49 / 0x3000fea0:
 *   MOV ECX,src; MOV ESI,dst; SUB ESI,ECX; loop: MOV DL,[ECX]; MOV [ESI+ECX],DL;
 *   INC ECX; TEST DL,DL; JNZ loop.
 * (ESI+ECX reconstructs the destination cursor since ESI == dst - src.) The size
 * argument is a literal 1 at both call sites here — the source is always the 1-byte
 * "" literal.
 */
/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the two identical byte
 * copy loops emitted inside CG_AllocWeaponInfo. */
static char *cgame_compat_copy_weapon_field_string(const char *src)
{
    char *dst = (char *)(intptr_t)cgame_syscall(CG_HUNK_ALLOC_LOW_ALIGN_EXPLICIT, 1, 1);
    char *d = dst;
    char c;
    do {
        c = *src;
        *d = c;
        ++src;
        ++d;
    } while (c != '\0');
    return dst;
}

weaponInfo_t *CG_AllocWeaponInfo(int32_t fieldCount, int32_t index,
                               const parseField_t *fields)
{
    weaponInfo_t *weaponInfo;

    /* 0x3000fe03: cgame_syscall(206, 0x4bc). The original i386 weaponInfo_t is
     * exactly 0x4bc bytes.  sizeof preserves that request on i386 and reserves
     * the pointer-expanded native record on 64-bit hosts. */
    weaponInfo = (weaponInfo_t *)(intptr_t)cgame_syscall(CG_HUNK_ALLOC_LOW_ALIGN,
                                                       sizeof(*weaponInfo));

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    bg_weaponInfos[index] = weaponInfo;
    weaponInfo->weaponIndex = index;

    /* 0x3000fe23: byte flag / copy-source is the shared "" literal g_str_empty. */
    if (g_str_empty[0] == '\0') {
        weaponInfo->pickupName = cg_emptyString;
    } else {
        weaponInfo->pickupName =
            cgame_compat_copy_weapon_field_string(g_str_empty);
    }

    /* 0x3000fe62: TEST EDI,EDI / JLE — signed count guard (skip when <= 0). */
    if (fieldCount > 0) {
        /* EBP walks the descriptor table; the loop runs fieldCount times. */
        int32_t i = fieldCount;
        const parseField_t *field = fields;
        do {
            /* 0x3000fe72: MOV EAX,[EBP+4] == field->type; JNZ skips non-strings. */
            if (field->type == PARSE_FIELD_STRING_ALLOC) {
                /* 0x3000fe79: EDI = field->offset + weaponInfo (pointer into the
                 * weaponInfo_t at this string field). */
                const char **slot =
                    (const char **)((char *)weaponInfo + field->offset);
                if (g_str_empty[0] == '\0') {
                    *slot = cg_emptyString;
                } else {
                    *slot =
                        cgame_compat_copy_weapon_field_string(g_str_empty);
                }
            }
            /* 0x3000fec0: ADD EBP,0xc — next descriptor; DEC count; JNZ. */
            ++field;
            --i;
        } while (i != 0);
    }

    /* 0x3000fecd: MOV EAX,EBX; RET — return the allocated weaponInfo_t. */
    return weaponInfo;
}
