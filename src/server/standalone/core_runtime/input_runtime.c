#include "core_runtime_private.h"

/*
 * Checked 2026-06-29: these input address-band FUN_ leaves are constant-return
 * or empty hooks in the dedicated binary; no source-level names are proven.
 */
qboolean FUN_080c83a0(void)
{
    return qfalse;
}

qboolean FUN_080c83aa(void)
{
    return qtrue;
}

qboolean FUN_080c83b4(void)
{
    return qfalse;
}

qboolean FUN_080c83be(void)
{
    return qfalse;
}

void FUN_080c83c8(void)
{
}

void Sys_InitInput(void)
{
}

void Sys_ShutdownInput(void)
{
}

void IN_Restart_f(void)
{
    Sys_ShutdownInput();
    Sys_InitInput();
}
