// Source: uo_cgame_mp_x86.dll 0x3002ca80..0x3002cb40
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002ca80_3002cb40.mcode
//
// CG_PlaySoundAliasByName — choose and play a sound alias by name.
// The mechanical size-guess name Script_AddListItem is rejected: the body issues
// sound/UI syscalls 0xc4/0xc6/5, not a list operation.
//
// Custom register ABI (proven from the machine code, not cdecl):
//   ECX = soundPosition       -> forwarded to the 0xc4 and 0xc6 traps
//   EAX = aliasName           -> forwarded to the 0xc4 alias-selection trap
//   [ESP+0x1c] = entityNum    -> the single caller-pushed stack argument
// Returns the sound duration in milliseconds. The two syscall wrappers clean their own
// pushed args; the function itself uses a plain RET (caller owns entityNum), so this
// is not a RET-imm callee-cleanup ABI — the sole caller (CG_LocalSound) balances it.
//
// Machine-code trace:
//   3002ca80 SUB ESP,0x10 / PUSH ESI / PUSH EDI          ; 0x10 locals, save regs
//   3002ca85 MOV EDI,ECX                                 ; EDI = channelObj
//   3002ca87 PUSH EDI / PUSH EAX / PUSH 0xc4 / CALL      ; cgame_syscall(0xc4,name,chan)
//   3002ca94 MOV ESI,EAX / ADD ESP,0xc                   ; ESI = snd_alias_t pointer
//   3002ca99 TEST ESI,ESI / JNZ                          ; if !alias -> return EAX(0)
//   3002caa3 MOV ECX,[ESP+0x1c]                          ; ECX = entityNum
//   3002caa7 PUSH 0 / PUSH EDI / PUSH ECX / PUSH ESI /
//            PUSH 0xc6 / CALL                            ; cgame_syscall(0xc6,handle,
//                                                        ;   entityNum,chan,0)
//   3002cab7 MOV EDI,EAX / ADD ESP,0x14                  ; EDI = durationMs
//   3002cabc TEST EDI,EDI / JZ 0x3002cb38                ; if !duration -> return EDI(0)
//   3002cac0 MOV EAX,[ESI+0x8] / TEST EAX / JZ 0x3002cb38; if !alias->subtitle skip
//   3002cac7 FLD [0x3052ffa8] / FMUL [0x3007be88(1000)] /
//            FSTP [ESP+0x8]                              ; f = cg_subtitleMinTime_vmCvar.value*1000
//   3002cad7 FLD [0x3007be50(2^-30)] / FSTP [ESP+0x10]   ; rounding-bias double eps
//   3002cae1 FLD [ESP+0x8] / FADD [ESP+0x10] /
//            FISTP [ESP+0xc]                             ; v = (int)(f + eps)
//   3002caed CMP [ESP+0xc],EDI / JLE 0x3002cb1f          ; signed: if v<=started -> EAX=started
//   3002caf3 (else recompute the same v into EAX)        ; EAX = max(v, started)
//   3002cb21 MOV EDX,[0x3048c5cc] / PUSH EDX / PUSH EAX /
//            MOV EAX,[ESI+0x8] / PUSH EAX / PUSH 5 / CALL ; cgame_syscall(5,subtitle,
//                                                        ;   clamped, cg_subtitleWidth_vmCvar.integer)
//   3002cb35 ADD ESP,0x10
//   3002cb38 MOV EAX,EDI / POP EDI / POP ESI /
//            ADD ESP,0x10 / RET                          ; return durationMs

#include "../client_recovered.h"
#include "../globals.h"

#include <math.h>

int CG_PlaySoundAliasByName(int32_t entityNum, const void *soundPosition, const char *aliasName)
{
    /* 3002ca87: choose the named sound alias for this origin. */
    snd_alias_t *alias = trap_Com_PickSoundAlias(aliasName, (const float *)soundPosition);

    /* 3002ca99: registration failed -> return the syscall's 0. */
    if (alias == NULL) {
        return 0;
    }

    /* 3002caa7: start the sound. Mac MSS_PlaySoundAlias and its sample/stream
     * callees prove that the result is playback duration in milliseconds. */
    int durationMs = trap_MSS_PlaySoundAlias(alias, entityNum, soundPosition, 0);

    /* 3002cabc / 3002cac0: display a subtitle only when playback returned a
     * duration and the selected alias carries a subtitle reference. */
    if (durationMs != 0 && alias->subtitle != NULL) {
        /* 3002cac7..3002cae9: v = round(cg_subtitleMinTime_vmCvar.value * 1000.0f + eps),
         * where eps = 2^-30 (0x3e10000000000000) is a rounding bias added at double
         * precision before the FISTP. The float product is computed at single
         * precision (FSTP float ptr) then widened for the add. */
        float timeMs = cg_subtitleMinTime_vmCvar.value * 1000.0f;
        /* 0x3002cae9: bare FISTP with no FNSTCW/FLDCW dance rounds to nearest
         * (default x87 mode), so nearbyint -- not a truncating (int) cast (Class
         * 5) -- reproduces it. The 2^-30 bias is added at double precision before
         * the round; this is the house idiom (cf. cg_updatefadeoverlay.c). */
        double rounded = nearbyint((double)timeMs + 9.313225746154785e-10);
        int32_t v;

        /* FISTP dword returns its integer-indefinite value for NaN and values
         * outside the signed-dword range. Keep that Windows/i386 result defined. */
        if (!(rounded >= (double)INT32_MIN && rounded <= (double)INT32_MAX))
            v = INT32_MIN;
        else
            v = (int32_t)rounded;

        /* 3002caed: signed CMP v,duration / JLE. The winning minimum-time path
         * does not reuse v: 0x3002caf3 reloads the cvar and repeats the float
         * product, double bias, and FISTP conversion. It then uses that second
         * result without comparing it to durationMs again. */
        int32_t clamped;
        if (v <= durationMs) {
            clamped = durationMs;
        } else {
            float timeMsAgain = cg_subtitleMinTime_vmCvar.value * 1000.0f;
            double roundedAgain = nearbyint((double)timeMsAgain + 9.313225746154785e-10);

            if (!(roundedAgain >= (double)INT32_MIN && roundedAgain <= (double)INT32_MAX)) {
                clamped = INT32_MIN;
            } else {
                clamped = (int32_t)roundedAgain;
            }
        }

        /* 3002cb21: sound-update trap. */
        cgame_syscall(CG_SUBTITLE, (intptr_t)alias->subtitle, clamped, (int32_t)cg_subtitleWidth_vmCvar.integer);
    }

    /* 3002cb38: return the playback duration (EDI). */
    return durationMs;
}
