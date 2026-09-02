// Source: uo_cgame_mp_x86.dll 0x3003ddc0..0x3003dde0
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003ddc0_3003dde0.mcode

#include "../client_recovered.h"

/*
 * trap_R_RegisterFont — cgame trap-51 wrapper installed into
 * displayContextDef_t::registerFont by CG_UIDisplayContextInit.
 *
 * The original body forwards its four incoming dwords unchanged, in order, after
 * command 51.  The native source uses the corresponding host pointers while the
 * asset load mode remains an integer word at the engine syscall boundary.
 */
void trap_R_RegisterFont(const char *name, int32_t pointSize, fontInfo_t *font, intptr_t loadMode)
{
    (void)cgame_syscall(CG_R_REGISTER_FONT, (intptr_t)name, pointSize, (intptr_t)font, loadMode);
}
