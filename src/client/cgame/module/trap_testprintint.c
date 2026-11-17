// Source: uo_cgame_mp_x86.dll 0x3003e390..0x3003e39d

#include "client/cgame/client_recovered.h"

/* Print a label and an integer. The original wrapper forwards string (ECX) and
 * value (EAX) with syscall 0x78 (CG_TEST_PRINT_INT). */
void trap_TestPrintInt(const char *string, int32_t value)
{
    cgame_syscall(CG_TEST_PRINT_INT, (intptr_t)string, value);
}
