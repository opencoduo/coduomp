#include <math.h>
#include <stdint.h>
#include "server/standalone/bindings/coduo_engine_structs.h"
#include "script_runtime_private.h"

/*
 * Checked 2026-06-29: recovered script-runtime leaf returns qfalse and has no
 * maintained callers; no source-level name proven.
 */
qboolean FUN_080aa918(void)
{
    return qfalse;
}

long double FUN_080b085c(float value)
{
    return (long double)fabsf(value);
}
