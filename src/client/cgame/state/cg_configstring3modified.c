// Source: uo_cgame_mp_x86.dll 0x3002c9c0..0x3002ca27
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002c9c0_3002ca27.mcode

#include "../client_recovered.h"

#include <stdint.h>

/*
 * CG_ConfigString3Modified (0x3002c9c0) — re-register the engine asset described
 * by config-string index 3 and submit its handle plus a nonnegative time delta to
 * the engine. Called from the shutdown-then-reregister flow (callers 0x3002e373,
 * 0x30038eff, 0x30039590 each issue the asset-reset trap 0xd7 immediately before).
 *
 * Config-string source (proven): the info string is
 *   cs = &cg_gameState.stringData[cg_gameState.stringOffsets[3]]
 * i.e. CG_ConfigString(3). cg_gameState.stringData (0x30442a00) is the cgame
 * gameState_t string heap; cg_gameState.stringOffsets[3] (0x30440a0c) is element 3
 * of the stringOffsets[2048] array based at 0x30440a00 (base + 0x2000). The i386 code
 * loads MOV ESI,[0x30440a0c] then LEA ESI,[ESI + 0x30442a00].
 *
 * Behavior:
 *   1. name  = Info_ValueForKey(cs, "n")
 *      handle = trap(0xc4, name, &vec3_origin)   // register named asset -> handle
 *   2. t   = Q_atoi(Info_ValueForKey(cs, "t"))    // a millisecond timestamp
 *      delta = t - cg_time
 *      // clamp: negative delta OR uninitialized cg_time (== 0) yields 0
 *      if (delta < 0 || cg_time == 0) delta = 0;
 *      trap(0xda, handle, delta)
 *
 * Name adjudication: the .mcode header's size-matched "Q_DrawStrlen" guess is
 * REJECTED — this function contains no string-length scan and no color-code
 * skipping; it is a config-string-3 reload handler. Named by its proven behavior
 * and call graph (config-string index 3 + asset re-register + time submit). The
 * exact original CoD symbol is unproven. The engine receiver proves traps 0xc4
 * and 0xda are CG_COM_PICK_SOUND_ALIAS and CG_MSS_PLAY_AMBIENT_ALIAS.
 * Info_ValueForKey (0x3004e960) and Q_atoi (0x3005b6ce) are caller-observed
 * provisional callee declarations.
 *
 * Instruction map:
 *   3002c9c0 PUSH EBX/ESI ; 3002c9ce PUSH EDI      callee-saved
 *   3002c9c2 MOV  ESI,[0x30440a0c]                  ESI = stringOffsets[3]
 *   3002c9c8 LEA  ESI,[ESI + 0x30442a00]            ESI = &stringData[off] = cs
 *   3002c9cf MOV  EBX,0x30077d8c                    key = "n"
 *   3002c9d4 MOV  ECX,ESI ; 3002c9d6 CALL 0x3004e960  EAX = Info_ValueForKey(cs,"n")
 *   3002c9db PUSH 0x30071f58                         arg1 = &vec3_origin {0,0,0}
 *   3002c9e0 PUSH EAX                                arg0 = name
 *   3002c9e1 PUSH 0xc4                               trap id
 *   3002c9e6 CALL [0x30085e9c]                       EAX = trap(0xc4,name,&vec3_origin)
 *   3002c9ec MOV  EBX,0x30077d88                    key = "t"
 *   3002c9f1 MOV  ECX,ESI                            cs
 *   3002c9f3 MOV  EDI,EAX                            handle = trap result
 *   3002c9f5 CALL 0x3004e960                         EAX = Info_ValueForKey(cs,"t")
 *   3002c9fa PUSH EAX ; 3002c9fb CALL 0x3005b6ce     EAX = Q_atoi(valueForT)
 *   3002ca00 MOV  ECX,[0x304831b0]                   ECX = cg_time
 *   3002ca06 ADD  ESP,0x10                           clean trap-0xc4(12) + atoi(4)
 *   3002ca09 SUB  EAX,ECX                            EAX = t - cg_time
 *   3002ca0b JS   3002ca11                           if (EAX < 0) -> zero
 *   3002ca0d TEST ECX,ECX ; 3002ca0f JNZ 3002ca13    if (cg_time != 0) keep EAX
 *   3002ca11 XOR  EAX,EAX                            delta = 0
 *   3002ca13 PUSH EAX ; 3002ca14 PUSH EDI ; PUSH 0xda
 *   3002ca1a CALL [0x30085e9c]                       trap(0xda, handle, delta)
 *   3002ca20 ADD  ESP,0xc ; POP EDI/ESI/EBX ; RET
 */
void CG_ConfigString3Modified(void)
{
    /* CG_ConfigString(3): the info string at gameState.stringOffsets[3]. */
    const char *cs = &cg_gameState.stringData[cg_gameState.stringOffsets[3]];

    /* Key "n": the asset name. Register it and take the returned engine handle. */
    snd_alias_t *alias = trap_Com_PickSoundAlias(Info_ValueForKey(cs, "n"),
                                                  vec3_origin);

    /* Key "t": a millisecond timestamp. The nonnegative delta from cg_time is
     * submitted alongside the handle; a negative delta or an uninitialized
     * cg_time (== 0) collapses to 0. */
    int32_t t = coduo_crt_atoi(Info_ValueForKey(cs, "t"));
    int32_t currentTime = coduo_int32_from_bits((uint32_t)cg_time);
    int32_t delta = coduo_int32_from_bits((uint32_t)t - (uint32_t)currentTime);
    if (delta < 0 || currentTime == 0)
        delta = 0;

    cgame_syscall(CG_MSS_PLAY_AMBIENT_ALIAS, (intptr_t)alias, delta);
}
