// Source: uo_cgame_mp_x86.dll 0x3002fd90..0x3002fe65
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002fd90_3002fe65.mcode

#include "../client_recovered.h"
#include "../globals.h"

#include <stdint.h>
#include <string.h>

/*
 * CG_GetObjectiveShaderForDir (0x3002fd90) — resolve the HUD objective/compass
 * icon shader handle for a direction slot, optionally building it on the fly
 * from a per-shader config-string name plus a direction suffix.
 *
 * ABI (custom register-arg, proven from the sole caller 0x3003022d): the
 * direction index arrives in ECX (0x3002fda3 MOV ESI,ECX) and the shader
 * config-string index is the single stack argument at [ESP+0x54] (= arg0 at
 * entry). Bare RET, but ADD ESP,4 at the caller — the one stack arg is
 * caller-cleaned (cdecl for that slot). Modeled as (dir, shaderIndex).
 *
 * Behavior:
 *   - dir selects a direction suffix from the on-stack table
 *     {"", "_up", "_down"} at [ESP+0x4/0x8/0xc]
 *     (0x30074a0c = "", 0x300798a0 = "_up", 0x30079898 = "_down").
 *   - If shaderIndex == 0 (0x3002fda0 TEST/JZ), return the preregistered
 *     cg_objectiveShaders[dir] (0x3002fe50 MOV EAX,[ESI*4 + 0x3044bb4c]).
 *   - Otherwise fetch the shader-index config-string name into a 64-byte stack
 *     buffer via CG_GetShaderConfigString(shaderIndex, name, 64)
 *     (0x3002fdc7 PUSH 0x40 / PUSH &name / CALL 0x3002fd10). On a false return,
 *     fall through to the cg_objectiveShaders[dir] path (0x3002fdd4 JZ else).
 *   - Truncate the name at the first '.' (0x3002fde2 CMP AL,0x2e) — strip the
 *     file extension.
 *   - Append the direction suffix (0x3002fe1f REP MOVSD / 0x3002fe28 REP MOVSB =
 *     strcat(name, suffix)).
 *   - Call CG_DrawInformation(qfalse) (0x3002fe26 PUSH 0 / 0x3002fe2a CALL
 *     0x3002a530) then register and return the shader:
 *     trap(CG_R_REGISTERSHADER, name, 5) (0x3002fe36 PUSH 0x59 / 0x3002fe38
 *     CALL *cgame_syscall). Its EAX result is the return value.
 *
 * Both exits load the /GS stack cookie into ECX and CALL 0x30061639
 * (__security_check_cookie: CMP [0x30081650],ECX) before RET; that is a
 * compiler-inserted canary check, not source-level behavior, so it is omitted.
 *
 * Name adjudication: the .mcode's "G_GetHintStringIndex" is a pure size match
 * (win 0xd5 == PPC 0xd5) to a server routine qboolean(int *outIndex, const char
 * *value) and is REJECTED — this function registers/selects an objective HUD
 * shader handle and shares no signature or subsystem with it. Role name from
 * proven behavior; exact original CoD symbol unproven.
 *
 * Instruction map (key points):
 *   3002fd9c MOV EAX,[ESP+0x54]          shaderIndex
 *   3002fda0 TEST EAX,EAX ; JZ else      if (shaderIndex == 0) -> cg_objectiveShaders[dir]
 *   3002fda3 MOV ESI,ECX                 dir
 *   3002fdc7 PUSH 0x40 ; PUSH &name ; CALL 0x3002fd10   CG_GetShaderConfigString
 *   3002fdd2 TEST EAX,EAX ; JZ else      if (!ok) -> cg_objectiveShaders[dir]
 *   3002fde2 loop: CMP AL,'.' / JZ ; ... truncate name at first '.'
 *   3002fdf0 EAX = suffixTable[dir] ; *cut = 0
 *   3002fe00.. strlen(suffix) ; strlen(name) ; REP MOVS = strcat(name, suffix)
 *   3002fe26 PUSH 0 ; CALL 0x3002a530     CG_DrawInformation(qfalse)
 *   3002fe36 PUSH 0x59 ; PUSH &name ; PUSH 5 ; CALL *cgame_syscall  register shader
 *   3002fe50 else: EAX = cg_objectiveShaders[dir]
 */

#define CG_OBJECTIVE_LONGEST_SUFFIX "_down"

enum {
    CG_OBJECTIVE_NAME_MAX = MAX_QPATH, /* 0x3002fdc7 PUSH 0x40 */
    CG_OBJECTIVE_LONGEST_SUFFIX_LENGTH =
        sizeof(CG_OBJECTIVE_LONGEST_SUFFIX) - 1,
    CG_OBJECTIVE_COMPOSED_NAME_CAPACITY =
        MAX_QPATH + CG_OBJECTIVE_LONGEST_SUFFIX_LENGTH
};

_Static_assert(MAX_QPATH <=
                   INT32_MAX - CG_OBJECTIVE_LONGEST_SUFFIX_LENGTH,
               "objective shader composition capacity overflows int32_t");

qhandle_t CG_GetObjectiveShaderForDir(int dir, int shaderIndex)
{
    /* Direction-suffix table: index 0/1/2 -> "" / "_up" / "_down"
     * (0x30074a0c / 0x300798a0 / 0x30079898). */
    const char *const dirSuffix[3] = { "", "_up", "_down" };

    char name[CG_OBJECTIVE_COMPOSED_NAME_CAPACITY];
    char *cut;

    if (shaderIndex != 0 &&
        CG_GetShaderConfigString(shaderIndex, name, CG_OBJECTIVE_NAME_MAX)) {

        /* Strip the file extension: truncate at the first '.'. */
        cut = name;
        while (*cut != '\0' && *cut != '.')
            cut++;
        *cut = '\0';

        const size_t baseLength = strlen(name);
        const size_t suffixLength = strlen(dirSuffix[dir]);

        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
        if (baseLength + suffixLength >= MAX_QPATH) {
            Com_Printf(
                "WARNING: rejected overlong objective shader name\n");
            return cg_objectiveShaders[dir];
        }
        coduo_client_crt_strcpy(name + baseLength, dirSuffix[dir]);

        CG_DrawInformation(qfalse);

        return coduo_int32_from_bits((uint32_t)cgame_syscall(
            CG_R_REGISTERSHADER, name, R_IMAGE_TRACK_HUD));
    }

    /* No per-shader override, or the lookup failed: use the preregistered
     * objective icon handle for this direction. */
    return cg_objectiveShaders[dir];
}
