// Source: uo_cgame_mp_x86.dll 0x3003d920..0x3003d929

#include "client/cgame/client_recovered.h"

/* Finish loading the queued models. The original wrapper pushes only syscall
 * 0x2e (CG_R_FINISH_LOADING_MODELS). */
void trap_R_FinishLoadingModels(void)
{
    cgame_syscall(CG_R_FINISH_LOADING_MODELS);
}
