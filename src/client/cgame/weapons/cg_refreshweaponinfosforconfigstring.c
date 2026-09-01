// Source: uo_cgame_mp_x86.dll 0x30044a10..0x30044a7c
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30044a10_30044a7c.mcode

#include "../client_recovered.h"
#include "../globals.h"

#include <string.h>

/*
 * CG_RefreshWeaponInfosForConfigString (0x30044a10)
 *
 * Walk the registered-weapon table and, for every weapon whose cached name no
 * longer matches the supplied config string, (re)register that weapon from the
 * new config string. Used by the weapon config-string-modified path: when a
 * CS_WEAPONS_* config string changes, every weapon slot whose stored name
 * differs from the incoming string is refreshed.
 *
 * Naming: the .mcode's size-matched guess "Script_SetCvar" is rejected — this
 * function sets no cvar and touches no script VM. It iterates the weaponInfo_t
 * table (count = bg_numWeapons at 0x30134cd4, the same signed count read by
 * BG_GetWeaponIndexForName) and dispatches per-weapon registration; named by
 * proven role. The per-weapon callee FUN_30044890 is caller-observed only
 * (declared provisionally in client_recovered.h) and must be superseded by its
 * own reconstruction.
 *
 * Argument passing: the sole caller (0x3003cf96) computes the config string as
 *   EBX = &cg_gameState.stringData[gameState.stringOffsets[csIndex]]
 * (a CG_ConfigString result, csIndex clamped to [0,0x800)) and then
 * `CALL 0x30044a10` with no stack argument — so the string arrives in EBX
 * (register-passed). EBX is never modified inside this function; it is copied to
 * ESI for each strcmp and pushed unchanged as the callee's config-string arg.
 *
 * Machine-code trace (0x30044a10..0x30044a7b):
 *   mov  eax,[bg_numWeapons]              ; count
 *   mov  edi,1                               ; i = 1  (1-based weapon index)
 *   cmp  eax,edi ; jl exit                   ; if count < 1, do nothing (signed)
 *   mov  ebp,0x304137a8                      ; ptr = &cg_weaponInfos[1].name
 *                                            ;   (0x30413580 + 1*0x1c4 + 0x64)
 * loop (0x30044a26):
 *   mov  esi,ebx                             ; ESI = walking copy of configString
 *   mov  eax,ebp                             ; EAX = walking copy of &entry.name
 *   ; --- inline strcmp(entry.name, configString) producing -1/0/+1 ---
 *   ...  (case-SENSITIVE byte compare: CMP DL,[ESI] with no case fold) ...
 *   test eax,eax ; jz skip                   ; only the ==0 (equal) test matters
 *   push ebx ; mov ecx,edi ; call 0x30044890 ; register weapon i (ECX=i, stack=cfg)
 *   add  esp,4
 * skip (0x30044a68):
 *   mov  eax,[bg_numWeapons]              ; reload count each iteration
 *   inc  edi                                 ; ++i
 *   add  ebp,0x1c4                           ; ptr += sizeof(cgWeaponInfo_t)
 *   cmp  edi,eax ; jle loop                  ; while i <= count (signed)
 * exit (0x30044a7a): ret
 *
 * The compares are signed (JL/JLE); count is bg_numWeapons (int32). The
 * strcmp is the standard SBB/SBB(-1) idiom yielding -1/0/+1, but only equality
 * gates the call, so the sign is behaviourally irrelevant. EBP is a byte-stride
 * walk over cg_weaponInfos[i].name that is exactly array[i].name (stride 0x1c4,
 * name at +0x64), so it is expressed as the array access, not pointer math.
 */
void CG_RefreshWeaponInfosForConfigString(const char *configString)
{
    int32_t i;

    for (i = 1; i <= bg_numWeapons; i++) {
        if (strcmp(cg_weaponInfos[i].name, configString) != 0) {
            CG_RefreshWeaponDObjModelSet(i, configString);
        }
    }
}
