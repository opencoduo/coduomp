// Source: uo_cgame_mp_x86.dll 0x300435c0..0x300435c6
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300435c0_300435c6.mcode

#include "../client_recovered.h"

int32_t CG_WeaponDObjHandle(int32_t weaponIndex)
{
    /* The original is exactly `ADD EAX,0x400; RET`; EAX is both input and
     * result. This is an integer handle-band mapping, not pointer arithmetic.
     * ADD is modulo 2^32 on the retail Win32/x86 target. */
    return coduo_int32_from_bits((uint32_t)weaponIndex +
                            (uint32_t)CG_VIEW_WEAPON_DOBJ_HANDLE_BASE);
}
