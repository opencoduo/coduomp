#include "../client_recovered.h"

// Source: uo_cgame_mp_x86.dll 0x3002b290..0x3002b2ae
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002b290_3002b2ae.mcode
//
// CG_CrosshairPlayer — returns the entity/client number currently under the
// crosshair, but only while the acquisition is still fresh (latched within the
// last CROSSHAIR_TIMEOUT ms); otherwise returns -1. This is the classic
// Quake3/CoD idiom and is confirmed by the same-module PPC name bank
// (cgame_mp.dll: CG_CrosshairPlayer) and by the three globals it reads, all of
// which are resolved crosshair/time state in globals.h.
//
// The mechanical `.mcode` header name `Scr_AddUndefined` is a pure size-guess
// (win size 0x1e vs corpus 0x1f) and is contradicted by the body: this function
// reads cg_crosshairEntTime / cg_time / cg_crosshairEntNum and has no script
// engine involvement. Name rejected; resolved by behavior + call graph instead.
//
// Instruction trace:
//   3002b290 MOV EAX,[0x3048adf0]   EAX = cg_crosshairEntTime
//   3002b295 MOV ECX,[0x304831b0]   ECX = cg_time
//   3002b29b ADD EAX,0x3e8          EAX = cg_crosshairEntTime + 1000 (32-bit wrap)
//   3002b2a0 CMP ECX,EAX            signed compare cg_time vs (time+1000)
//   3002b2a2 JLE 0x3002b2a8         if cg_time <= time+1000 -> return the entnum
//   3002b2a4 OR  EAX,0xffffffff     else EAX = -1
//   3002b2a7 RET
//   3002b2a8 MOV EAX,[0x3048adec]   EAX = cg_crosshairEntNum
//   3002b2ad RET
//
// The CMP/JLE pair is a SIGNED comparison, while the preceding ADD is a plain
// modulo-2^32 target operation. Preserve the ADD's bits explicitly before the
// signed comparison; ISO-C signed overflow is not the Windows/i386 contract.

/* CROSSHAIR_TIMEOUT: the crosshair-name latch stays valid for 1000 ms after the
 * traced entity was last acquired (0x3e8). */
enum {
    CROSSHAIR_TIMEOUT = 1000
};

int32_t CG_CrosshairPlayer(void)
{
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    int32_t expiry = coduo_int32_from_bits((uint32_t)cg_crosshairEntTime + (uint32_t)CROSSHAIR_TIMEOUT);

    if (coduo_int32_from_bits((uint32_t)cg_time) > expiry) {
        return -1;
    }
    return cg_crosshairEntNum;
}
