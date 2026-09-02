// Source: uo_cgame_mp_x86.dll 0x30017e90..0x30018017
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30017e90_30018017.mcode

#include "../client_recovered.h"
#include "../globals.h"

#include <stdint.h>

/*
 * CG_DrawScriptUsage (0x30017e90) — a small debug HUD routine that draws three
 * script-VM (GSC interpreter) diagnostic counters as a stacked column of 2D text.
 * Each line is built by formatting the return value of a zero-argument counter
 * trap into a label string with va(), then handed to the CG_R_TEXT_PAINT 2D-text
 * emitter with a fixed draw frame.
 *
 * The three counter traps (proven from the pushed ids and the adjacent label
 * strings):
 *   CG_GET_NUM_SCRIPT_VARS (0xd1) -> int32, formatted as "num vars:    %d"   (0x30076dac)
 *   CG_GET_NUM_SCRIPT_THREADS (0xd2) -> int32, formatted as "num threads: %d"   (0x30076d9c)
 *   CG_GET_STRING_USAGE (0xd3) -> int32, formatted as "string usage: %d"  (0x30076d88)
 * Each is issued as `PUSH id; CALL cgame_syscall; PUSH EAX(result); PUSH fmt;
 * CALL va` — a bare int32 result used directly as the %d argument of va(). The
 * three ids are consecutive and the labels identify them as the interpreter's
 * live variable count, active thread count, and string-table byte usage.
 *
 * The draw for each line is the CG_R_TEXT_PAINT fixed-arity 2D-text emitter (the same
 * 10-argument frame documented on CG_EmitTrap54DrawScaled / CG_R_TEXT_PAINT). All
 * three draws share the same frame except the vertical position (arg2 = Z), which
 * steps 92.8 -> 108.8 -> 124.8 (a 16.0-unit line pitch), and the string handle
 * (arg6 = the va() result). Proven by simulating the interleaved PUSH/MOV stack
 * stores; ascending memory = argument order, id first:
 *   arg0 = 54                              (CG_R_TEXT_PAINT, the 2D-text service)
 *   arg1 = 480.0f                          x position
 *   arg2 = 92.8f / 108.8f / 124.8f         y position (16.0 pitch per line)
 *   arg3 = 5                               style/font id
 *   arg4 = 1/3 (0.33333334f)               scale
 *   arg5 = &cg_colorWhite     color pointer (the shared 1.0f .rdata
 *                                          const, start of a white RGBA)
 *   arg6 = va() result                     the formatted text string
 *   arg7 = 8.0f                            width / char size
 *   arg8 = 0                               extra
 *   arg9 = 3                               mode
 * (The prologue also writes four 1.0f dwords into stack scratch as an inline white
 * color vec4, but the emitted color pointer is the .rdata 1.0f const, so those
 * scratch stores are dead in this routine — preserved implicitly by not modelling
 * them, exactly as the machine code leaves them unused.)
 *
 * Float arguments are forwarded as their raw 32-bit dword bit patterns (the i386
 * code stores the immediate float words and PUSHes them, never promoting to
 * double); CG_FloatBits reproduces that exactly.
 *
 * Name adjudication: the .mcode header size-match guess "G_CheckPointInsideTrigger-
 * Mount" is REJECTED. That is a server-side trigger/mount geometry test; this
 * function takes no arguments, reads no trigger/entity state, and only issues the
 * script-VM stat traps plus CG_R_TEXT_PAINT 2D-text draws. The match was a pure size
 * collision (0x187) with no behavioral basis (the contract forbids size-based
 * naming). The Mac cgame symbol CG_DrawScriptUsage calls the corresponding
 * script-usage counters and text painter, resolving the source name.
 */

/* Fixed CG_R_TEXT_PAINT draw parameters shared by all three stat lines, proven from
 * the pushed immediates. */
enum {
    CG_SCRIPTSTAT_STYLE = 5, /* arg3 style/font id                              */
    CG_SCRIPTSTAT_EXTRA = 0, /* arg8 opaque extra dword                         */
    CG_SCRIPTSTAT_MODE = 3, /* arg9 draw mode                                  */
};
#define CG_SCRIPTSTAT_X 480.0f          /* arg1 x position               */
#define CG_SCRIPTSTAT_Y0 92.8f           /* arg2 y of first line          */
#define CG_SCRIPTSTAT_Y_STEP 16.0f           /* line pitch (92.8/108.8/124.8) */
#define CG_SCRIPTSTAT_SCALE (1.0f / 3.0f)   /* arg4 scale (0x3eaaaaab)        */
#define CG_SCRIPTSTAT_WIDTH 8.0f            /* arg7 width / char size        */

void CG_DrawScriptUsage(void)
{
    int32_t numVars = coduo_int32_from_bits((uint32_t)cgame_syscall(CG_GET_NUM_SCRIPT_VARS));
    const char *text = va(cg_scriptNumVarsDebugFormat, numVars);
    cgame_syscall(CG_R_TEXT_PAINT, CG_FloatBits(CG_SCRIPTSTAT_X), CG_FloatBits(CG_SCRIPTSTAT_Y0), CG_SCRIPTSTAT_STYLE,
                  CG_FloatBits(CG_SCRIPTSTAT_SCALE), (intptr_t)&cg_colorWhite, (intptr_t)text, CG_FloatBits(CG_SCRIPTSTAT_WIDTH),
                  CG_SCRIPTSTAT_EXTRA, CG_SCRIPTSTAT_MODE);

    int32_t numThreads = coduo_int32_from_bits((uint32_t)cgame_syscall(CG_GET_NUM_SCRIPT_THREADS));
    text = va(cg_scriptNumThreadsDebugFormat, numThreads);
    cgame_syscall(CG_R_TEXT_PAINT, CG_FloatBits(CG_SCRIPTSTAT_X), CG_FloatBits(CG_SCRIPTSTAT_Y0 + CG_SCRIPTSTAT_Y_STEP),
                  CG_SCRIPTSTAT_STYLE, CG_FloatBits(CG_SCRIPTSTAT_SCALE), (intptr_t)&cg_colorWhite, (intptr_t)text,
                  CG_FloatBits(CG_SCRIPTSTAT_WIDTH), CG_SCRIPTSTAT_EXTRA, CG_SCRIPTSTAT_MODE);

    int32_t stringUsage = coduo_int32_from_bits((uint32_t)cgame_syscall(CG_GET_STRING_USAGE));
    text = va(cg_scriptStringUsageDebugFormat, stringUsage);
    cgame_syscall(CG_R_TEXT_PAINT, CG_FloatBits(CG_SCRIPTSTAT_X), CG_FloatBits(CG_SCRIPTSTAT_Y0 + 2.0f * CG_SCRIPTSTAT_Y_STEP),
                  CG_SCRIPTSTAT_STYLE, CG_FloatBits(CG_SCRIPTSTAT_SCALE), (intptr_t)&cg_colorWhite, (intptr_t)text,
                  CG_FloatBits(CG_SCRIPTSTAT_WIDTH), CG_SCRIPTSTAT_EXTRA, CG_SCRIPTSTAT_MODE);
}
