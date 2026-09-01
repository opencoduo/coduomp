// Source: uo_cgame_mp_x86.dll 0x3002fd10..0x3002fd84
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002fd10_3002fd84.mcode

#include "../client_recovered.h"
#include "../globals.h"

#include <stdint.h>

/*
 * CG_GetShaderConfigString (0x3002fd10) — resolve a shader index to its shader
 * config-string name and copy that name into a caller buffer, returning qtrue on
 * success (qfalse for an out-of-range index, an empty config string, or a name
 * that does not fit).
 *
 * Custom register-argument ABI (proven from the two callers 0x3002a01c and
 * 0x3002a1e9, and 0x3002fdca): the shader index arrives in EAX
 * (MOV EAX,[reg+0x38] at the caller), and the destination buffer pointer and its
 * size are the two stack arguments, pushed size-last then buffer:
 *     PUSH size ; PUSH dest ; MOV EAX,index ; CALL 0x3002fd10
 * The function is caller-cleaned (bare RET, no immediate), so both stack slots
 * are cdecl args. Expressed here as (index@EAX, dest, size).
 *
 * Config-string mapping (proven): the shader index is validated to 0 < index <
 * CS_SHADERS_COUNT(256), then the config-string index is CS_SHADERS(1653) + index
 * (0x3002fd1c LEA ESI,[EAX + 0x675]). The string is
 *     cg_gameState.stringData[cg_gameState.stringOffsets[cfgIndex]]
 * (0x3002fd3c MOV ECX,[ESI*4 + 0x30440a00]  = stringOffsets[cfgIndex];
 *  0x3002fd43 ADD ECX,0x30442a00            = &stringData[offset]).
 *
 * The generic CG_ConfigString bounds check on the composed index
 * (0 <= cfgIndex < MAX_CONFIGSTRINGS, else Com_ErrorMessage("CG_ConfigString:
 * bad index: %i", cfgIndex)) is emitted but, given 1 <= index <= 255, the
 * composed index is always in [1654, 1908] and the error path is never taken at
 * runtime; it is preserved faithfully.
 *
 * Name adjudication: the .mcode header's "BG_IsPlayerWeaponAnAlt" is a pure
 * size-match (win 0x74 == PPC 0x74) and is REJECTED — this function performs a
 * config-string lookup and a bounded string copy, not a weapon/alt comparison.
 * The exact original CoD symbol is unproven; CG_GetShaderConfigString is a role
 * name from proven behavior (shader config-string range + copy). Com_ErrorMessage
 * is a caller-observed provisional decl (callee 0x3002b300).
 *
 * Instruction map:
 *   3002fd10 TEST EAX,EAX ; JLE  ret0        if (index <= 0) return qfalse   (signed)
 *   3002fd14 CMP  EAX,0x100 ; JGE ret0       if (index >= 256) return qfalse (signed)
 *   3002fd1b PUSH ESI
 *   3002fd1c LEA  ESI,[EAX + 0x675]          cfgIndex = index + CS_SHADERS
 *   3002fd22 TEST ESI,ESI ; JL   err         if (cfgIndex < 0)  -> bad index (signed)
 *   3002fd26 CMP  ESI,0x800 ; JL  ok         if (cfgIndex < 2048) skip error
 *   3002fd2e err: PUSH ESI ; PUSH "CG_ConfigString: bad index: %i"
 *   3002fd34      CALL Com_ErrorMessage ; ADD ESP,8
 *   3002fd3c ok: MOV ECX,[ESI*4 + 0x30440a00]   ECX = stringOffsets[cfgIndex]
 *   3002fd43      ADD ECX,0x30442a00             ECX = &stringData[offset] = cs
 *   3002fd49      CMP byte [ECX],0 ; JNZ copy    if (*cs == 0) return qfalse
 *   3002fd4e ret0: XOR EAX,EAX ; POP ESI ; RET   return qfalse
 *   3002fd52 copy: strlen scan of cs (EAX walks, ESI = cs+1)
 *   3002fd5e      MOV EDX,[ESP+0xc] = size
 *   3002fd62      SUB EAX,ESI ; CMP EAX,EDX ; JNC ret0   if (strlen >= size) qfalse (unsigned)
 *   3002fd68      MOV EDX,[ESP+8] = dest ; SUB EDX,ECX   strcpy(dest, cs)
 *   3002fd70      loop: MOV CL,[EAX] ; MOV [EDX+EAX],CL ; INC EAX ; TEST CL ; JNZ
 *   3002fd7a      MOV EAX,1 ; POP ESI ; RET       return qtrue
 */
qboolean CG_GetShaderConfigString(int index, char *dest, int size)
{
    int cfgIndex;
    const char *cs;
    int len;

    /* Signed bounds on the shader index: valid range is 1..255. */
    if (index <= 0 || index >= CS_SHADERS_COUNT)
        return qfalse;

    /* Map the shader index into the shared config-string space. */
    cfgIndex = index + CS_SHADERS;

    /* Generic CG_ConfigString bounds check (unreachable for a valid shader index,
     * but emitted by the compiler and preserved). */
    if (cfgIndex < 0 || cfgIndex >= MAX_CONFIGSTRINGS)
        Com_ErrorMessage("CG_ConfigString: bad index: %i", cfgIndex);

    cs = &cg_gameState.stringData[cg_gameState.stringOffsets[cfgIndex]];

    /* Empty config string: nothing to copy. */
    if (cs[0] == '\0')
        return qfalse;

    /* Reject a name that would not fit (the terminator excluded from the compare:
     * strlen >= size fails; strlen == size-1 still fits with its NUL). */
    len = 0;
    while (cs[len] != '\0')
        len++;
    if ((unsigned)len >= (unsigned)size)
        return qfalse;

    /* Copy the name, including its terminator. */
    {
        int i = 0;
        char c;
        do {
            c = cs[i];
            dest[i] = c;
            i++;
        } while (c != '\0');
    }

    return qtrue;
}
