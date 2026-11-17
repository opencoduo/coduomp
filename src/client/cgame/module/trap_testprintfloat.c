// Source: uo_cgame_mp_x86.dll 0x3003e3a0..0x3003e3b7

#include "client/cgame/client_recovered.h"

/* Print a label and a float. The original wrapper forwards string (EDX) and
 * value (stack) with syscall 0x79 (CG_TEST_PRINT_FLOAT). */
void trap_TestPrintFloat(const char *string, float value)
{
    cgame_syscall(CG_TEST_PRINT_FLOAT, (intptr_t)string, CG_FloatBits(value));
}
