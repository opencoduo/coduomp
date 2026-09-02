#include "../client_recovered.h"

#include <stddef.h>

// Offset guards proving the original i386 cgVoiceChatTable_t layout used below.
#if UINTPTR_MAX == 0xFFFFFFFFu
_Static_assert(offsetof(cgVoiceChatTable_t, entryCount) == 0x44,
               "cgVoiceChatTable_t.entryCount at +0x44");
_Static_assert(offsetof(cgVoiceChatTable_t, entries) == 0x48,
               "cgVoiceChatTable_t.entries at +0x48");
_Static_assert(offsetof(cgVoiceChatEntry_t, variantCount) == 0x40,
               "cgVoiceChatEntry_t.variantCount at +0x40");
_Static_assert(offsetof(cgVoiceChatEntry_t, sounds) == 0x44,
               "cgVoiceChatEntry_t.sounds at +0x44");
_Static_assert(offsetof(cgVoiceChatEntry_t, text) == 0x144,
               "cgVoiceChatEntry_t.text at +0x144");
_Static_assert(offsetof(cgVoiceChatEntry_t, icons) == 0x1144,
               "cgVoiceChatEntry_t.icons at +0x1144");
_Static_assert(sizeof(cgVoiceChatEntry_t) == 0x1244,
               "cgVoiceChatEntry_t stride 0x1244");
#endif

// Source: uo_cgame_mp_x86.dll 0x30039f10..0x30039fb4
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30039f10_30039fb4.mcode
//
// CG_PickSoundAlias - client sound-alias picker.
//
// Behavior: linearly scan the passed per-locale alias table for the entry whose
// name case-insensitively matches `name`; on a hit, pick one of that entry's
// variants at random and return the picked variant's data through the three
// output pointers, returning qtrue. If no entry matches, return qfalse with the
// outputs untouched.
//
// The .mcode name_note guess "Menu_Init" (pure size match, 0xa4 == 0xa4) is
// REJECTED: this reads a 0x1244-strided sound-alias table, case-insensitively
// compares alias names with Q_stricmpn(99999,...), and draws a weighted random
// variant via rand()/Q_rint. That is the CoD trap_Com_PickSoundAlias(name, out)
// role on the client's own fat alias table, not a menu initializer.
//
// Callees resolved by behavior (their .mcode size-guess names are rejected):
//   0x3004e620 Q_stricmpn  (case-fold compare; 99999 limit idiom == Q_stricmp)
//               - rejects "script_func_precachestring".
//   0x3005b879 rand        (15-bit CRT-domain sample)
//               - rejects "SP_light".
//   0x3006be3c Q_rint      (round float in ST0 to int, already named in header)
//               - rejects "SP_sound_blend".
//
// ABI: the alias-table base arrives in EBX (an implicit register parameter the
// caller loads with 0x3016a9e0 or 0x301b3b28 at 0x3003a292/0x3003a2a2). The four
// named arguments below are the __cdecl stack arguments (caller cleans 0x10).
// The prologue saves ECX/EBP/ESI/EDI; EBX is neither saved nor restored here,
// confirming it is a caller-owned input, not a local.

qboolean CG_PickSoundAlias(cgVoiceChatTable_t *table,
                           const char *name,
                           const char **outSoundName,
                           qhandle_t *outIcon,
                           const char **outText)
{
    // 0x30039f12 MOV EBP,[EBX+0x44]  : loop bound = number of aliases in table.
    // 0x30039f16 XOR ESI,ESI         : ESI = scan index i, starts at 0.
    // 0x30039f18 TEST EBP,EBP / JLE  : signed <= 0 -> skip loop, return qfalse.
    int32_t aliasCount = table->entryCount;
    int32_t i;

    for (i = 0; i < aliasCount; i++) {
        // 0x30039f20 MOV EDX,[ESP+0x14] : the search name (arg `name`).
        // 0x30039f24 TEST EDX,EDX / JZ  : if name==NULL, jump to the loop's
        //                                 increment/continue path.
        // 0x30039f28 TEST EDI,EDI / JZ  : EDI = &entry[i].name, the address of the
        //   inline name field. For this struct layout that address is always
        //   non-NULL, so the guard can never take the fall-through branch; it is
        //   an artifact of the compiler materializing &entry[i].name into a
        //   register and is intentionally not re-expressed as a C test.
        // 0x30039f2c MOV EAX,0x1869f    : Q_stricmpn limit 99999 (== Q_stricmp).
        // 0x30039f33 CALL Q_stricmpn    : compare(name, entry[i].name).
        // 0x30039f3a JZ  -> pick path   : EAX==0 means the names match.
        if (name == NULL ||
            Q_stricmpn(name, table->entries[i].name, 99999) != 0) {
            // 0x30039f3c INC ESI / ADD EDI,0x1244 / CMP / JL : keep scanning.
            continue;
        }

        // ---- pick path (0x30039f4e..0x30039fb3) : entry `i` is the match ----

        // 0x30039f4e MOV EDI,ESI / 0x30039f50 IMUL EDI,EDI,0x1244
        //   EDI = i * 0x1244 = byte offset of entry[i] from the table base.
        // 0x30039f56 CALL rand : EAX in [0, 32767].
        // 0x30039f5f FILD [rand] (bare) ; 0x30039f63 FMUL [0x3007bec0] (== 1/32768)
        // 0x30039f69 FIMUL [EDI+EBX+0x88] : * entry[i].variantCount (int32).
        //   value = rand() * (1.0f/32768.0f) * variantCount, a random position in
        //   [0, variantCount). rand feeds a bare FILD and variantCount an integer
        //   FIMUL -- neither takes a (float) cast (Class 4).
        // 0x30039f70 CALL _ftol2 : truncate to the variant index r (EAX).
        //   _ftol2 consumes the raw ST0 left by the FILD/FMUL/FIMUL chain, so the
        //   product stays 80-bit (no intermediate float `scaled` store).
        int32_t r = coduo_fp_to_i32_extended(
            (long double)coduo_crt_rand() *
            (long double)(1.0f / 32768.0f) *
            (long double)table->entries[i].variantCount);

        // 0x30039f75 IMUL ESI,ESI,0x491 : ESI = i * 0x491 (0x491*4 == 0x1244,
        //   so ESI*4 == i*0x1244 == entry byte offset; ECX below reuses this).
        // 0x30039f7b LEA ECX,[ESI+EAX]  : ECX = i*0x491 + r (flat int32 index).
        // 0x30039f7e MOV EDX,[EBX+ECX*4+0x8c]  : entry[i].soundIndex[r].
        // 0x30039f85 MOV ESI,[ESP+0x18] ; 0x30039f89 MOV [ESI],EDX
        //   *outSoundIndex = entry[i].soundIndex[r].
        *outSoundName = table->entries[i].sounds[r];

        // 0x30039f8b MOV ECX,[EBX+ECX*4+0x118c] : entry[i].soundIndex2[r].
        // 0x30039f92 MOV EDX,[ESP+0x1c] ; 0x30039f9b MOV [EDX],ECX
        //   *outSoundIndex2 = entry[i].soundIndex2[r].
        *outIcon = table->entries[i].icons[r];

        // 0x30039f96 SHL EAX,0x6        : r * 0x40 (variant record stride).
        // 0x30039f99 ADD EAX,EDI        : + i*0x1244 (entry byte offset).
        // 0x30039fa2 LEA EAX,[EAX+EBX+0x18c] : &entry[i].variant[r].
        // 0x30039f9d MOV ECX,[ESP+0x20] ; 0x30039faa MOV [ECX],EAX
        //   *outVariant = &entry[i].variant[r].
        *outText = table->entries[i].text[r];

        // 0x30039fac MOV EAX,1 ; ... RET : found -> return qtrue.
        return qtrue;
    }

    // 0x30039f47..0x30039f4d : POP EDI/ESI ; XOR EAX,EAX ; POP EBP/ECX ; RET.
    // No matching alias -> return qfalse, outputs left untouched.
    return qfalse;
}
