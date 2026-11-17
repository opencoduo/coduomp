// Source: uo_cgame_mp_x86.dll 0x3003e0c0..0x3003e0cc

#include "client/cgame/client_recovered.h"

/* Blend the saved screen copy over the frame. The original wrapper forwards
 * level (EAX) with syscall 0x47 (CG_R_BLEND_SAVED_SCREEN). */
void trap_R_BlendSavedScreen(int32_t level)
{
    cgame_syscall(CG_R_BLEND_SAVED_SCREEN, level);
}
