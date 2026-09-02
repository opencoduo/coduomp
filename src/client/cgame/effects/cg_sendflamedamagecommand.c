// Source: uo_cgame_mp_x86.dll 0x300291c0..0x30029204
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300291c0_30029204.mcode
//
// CG_SendFlameDamageCommand — throttled per-client flame-damage notification to
// the server. For client `clientNum`, if at least 2000 ms have passed since this
// client's last flame-damage pain event, latch the current time and send the
// reliable client command va("fdc %i", painId) via cgame_syscall(0x18).
//
// NAMING: the .mcode pre-hint `# name AngleNormalize360Accurate` is REJECTED. It
// is a pure size-match guess ("win size 0x44, matched size 0x44") and the body
// contains no float/x87 work, no 360.0 constant, and no fmod/floor/BAMS angle
// wrapping at all. This function reads cg_time, indexes the per-client
// cg_flameInfo array (stride 0xb8) by clientNum, does a 2000ms throttle on
// the .lastPainTime field, and forwards a formatted "fdc %i" reliable command to
// the server through the cgame VM trap (id 0x18 = CG_SEND_CLIENT_COMMAND). Named
// by proven behavior + call graph; exact original symbol unproven (there is no
// cgame syscall-id/symbol table recovered), so the name is provisional-by-role.
// "fdc" is the reliable-command verb this cgame sends for flame damage.
//
// ABI: register (fastcall-style) arguments — clientNum in EAX, painId in ECX; no
// incoming stack arguments. `RET` with no immediate, so the caller cleans (there
// are no stack args to clean). The sole caller (0x300268c1) loads EAX from a
// client-number field and ECX from its own local before the CALL.
//
// THROTTLE / SIGNEDNESS: the compare is signed 32-bit. EDI=lastPainTime,
// ESI=cg_time-2000 (0x300291d4 LEA ESI,[cg_time-0x7d0]); JG skips the send when
// lastPainTime > cg_time - 2000. So the send fires only when
// lastPainTime <= cg_time - 2000, i.e. at least 2000 ms have elapsed. cg_time is
// read as a 32-bit value; the subtraction and compare are signed like the sibling
// "score" throttle in CG_DrawScoreboard (0x30037d90).

#include "client/cgame/globals.h"
#include "client/cgame/client_recovered.h"

/* ms between successive flame-damage notifications for one client (LEA cg_time-0x7d0). */
enum { CG_FLAME_DAMAGE_COMMAND_INTERVAL = 2000 };

void CG_SendFlameDamageCommand(int32_t clientNum, int32_t painId)
{
    /* 0x300291c0 MOV EDX,cg_time ; 0x300291d4 LEA ESI,[cg_time-0x7d0] */
    int32_t now = coduo_int32_from_bits(cg_time);

    cgFlameInfo_t *state = &cg_flameInfo[clientNum];

    /* 0x300291da CMP EDI(lastPainTime),ESI(cg_time-2000) ; 0x300291de JG ret.
     * Signed compare: skip when the last event is more recent than the window. */
    int32_t windowStart = coduo_int32_from_bits(
        (uint32_t)now - (uint32_t)CG_FLAME_DAMAGE_COMMAND_INTERVAL);
    if (state->lastPainTime > windowStart) {
        return;                                  /* 0x30029203 RET */
    }

    /* 0x300291e6 store latch time; 0x300291ec store the pain id into +0x08. */
    state->lastPainTime = now;
    state->painCounter = painId;

    /* 0x300291e1 PUSH "fdc %i"; 0x300291e0 PUSH painId; 0x300291f2 CALL va ->
     * 0x300291f8 PUSH 0x18; 0x300291f7 PUSH result; 0x300291fa CALL cgame_syscall.
     * cgame_syscall(CG_SEND_CLIENT_COMMAND, va("fdc %i", painId)); ADD ESP,0x10. */
    cgame_syscall(CG_SEND_CLIENT_COMMAND,
                  (intptr_t)va("fdc %i", painId));
}
