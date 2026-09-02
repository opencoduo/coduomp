#include "../client_recovered.h"
#include "../globals.h"

#include <limits.h>
#include <math.h>
#include <stdint.h>

// Source: uo_cgame_mp_x86.dll 0x300174b0..0x3001758e
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300174b0_3001758e.mcode
//
// CG_ShellShock_f: the console-command handler for "cg_shellshock". It is one of
// the "_f" command callbacks dispatched by CG_ConsoleCommand (0x300178c0); it
// takes no arguments and reads the command tokens through the trap_Arg* syscalls.
//
// Behavior (proven instruction-by-instruction):
//   - argc = trap_Argc().
//     * argc == 2: use the currently-loaded shellshock cvars; token 1 is the
//       <duration> in seconds.
//     * argc == 3: token 2 is a <filename?>; load "scripts/<filename>.shock"
//       first (CG_ShellShockLoad). If that load fails, do nothing further; if it
//       succeeds, fall through to the argc==2 body (token 1 = <duration>).
//     * otherwise: print the usage string and return.
//   - In the argc==2 body it reads token 1 into a 256-byte buffer via
//     trap_Argv(1, ...), converts it to seconds with atof, rounds
//     (seconds * 1000) to an integer millisecond duration, resolves the loaded
//     cg_shock_* cvars into the manual shellshock parameter block
//     cg_consoleShellShock (CG_SetShellShockParams), and arms the effect by
//     recording the current cg.time as cg_shellShockStartTime and the rounded
//     duration as cg_shellShockDuration. The scene reader (0x30042160) then plays
//     the manual shellshock from these three globals.
//
// The .mcode's mechanical name `ConcatArgs` is a pure size match (win 0xde ==
// server 0xde) and is REJECTED: there is no argument-concatenation behavior. The
// name is proven by the emitted usage string
// "USAGE: cg_shellshock <duration> <filename?>\n" (0x30076978), the optional
// CG_ShellShockLoad("scripts/%s.shock") call, and the arming of the shellshock
// start-time/duration globals consumed by the scene reader.
//
// /GS notes: this function owns a 256-byte stack buffer, so MSVC emits the
// stack-cookie prologue (MOV EAX,[__security_cookie]; MOV [ESP+0x120],EAX) and
// the __security_check_cookie epilogues. Those are compiler-generated protection
// code, not source statements; the source body is the argc dispatch below.
//
// Float->int millisecond idiom (0x3001753c..0x3001755b): the machine code does
//   FMUL float 1000.0        ; ST(0) = atof(token)*1000  (single precision)
//   FSTP float [ESP+0xc]     ; round the product to 32-bit float
//   FLD  float [ESP+0xc]
//   FADD double 9.3132257e-10 ; += 2^-30 nudge
//   FISTP dword [ESP+0x8]    ; store rounded-to-nearest int32 ms
// i.e. duration_ms = lrint((float)(atof(token) * 1000.0f) + 2^-30) under the
// default round-to-nearest FPU mode. Modeled below with an explicit round.

/* 0x3007be88 .rdata: the seconds->milliseconds scale, a 32-bit float 1000.0. */
#define CG_MS_PER_SECOND 1000.0f
/* 0x3007be50 .rdata: the 2^-30 rounding nudge added before FISTP, a double. */
#define CG_SHELLSHOCK_ROUND_NUDGE 9.313225746154785e-10

void CG_ShellShock_f(void)
{
    /* token buffer: 256 bytes on the /GS-guarded frame, reused for both the
     * <filename?> (argc==3) and the <duration> (argc==2) tokens. */
    char token[256];

    int32_t argc = trap_Argc();

    /* 0x300174d4..0x300174db: dispatch on argc via (argc-2) then DEC. */
    if (argc != 2 && argc != 3) {
        /* 0x300174dc: neither 2 nor 3 tokens -> print usage and return. */
        Com_PrintMessage("USAGE: cg_shellshock <duration> <filename?>\n");
        return;
    }

    if (argc == 3) {
        /* 0x300174fa: token 2 is a shellshock definition name; load
         * "scripts/<name>.shock" first. On failure, stop. */
        trap_Argv(2, token, (int32_t)sizeof(token));
        if (CG_ShellShockLoad(token) == 0) {
            return;
        }
        /* success falls through into the argc==2 body (0x3001751e). */
    }

    /* 0x3001751e: token 1 is the duration in seconds. */
    trap_Argv(1, token, (int32_t)sizeof(token));

    /* 0x30017537..0x3001755b: multiply in x87 width, round the product to a
     * float slot, add the double nudge, then use a bare FISTP under the active
     * (normally nearest-even) control word. */
    float scaledMilliseconds = (float)((long double)atof(token) * (long double)CG_MS_PER_SECOND);
    double rounded = nearbyint((double)scaledMilliseconds + CG_SHELLSHOCK_ROUND_NUDGE);
    int32_t durationMs;
    if (!(rounded >= (double)INT32_MIN && rounded <= (double)INT32_MAX)) {
        durationMs = INT32_MIN;
    } else {
        durationMs = (int32_t)rounded;
    }

    /* 0x3001755f..0x30017564: resolve the loaded cg_shock_* cvars into the
     * manual shellshock parameter block (destination passed in ESI). */
    CG_SetShellShockParams(&cg_consoleShellShock);

    /* 0x30017569..0x30017577: arm the manual shellshock for the scene reader:
     * record the current game time as the start and the rounded duration. */
    cg_shellShockStartTime = coduo_int32_from_bits(cg_time);
    cg_shellShockDuration = durationMs;
}
