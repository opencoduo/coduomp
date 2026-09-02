// Source: uo_cgame_mp_x86.dll 0x3002b1a0..0x3002b254
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002b1a0_3002b254.mcode
//
// CG_RegisterCvars — register every cgame cvar at startup and initialize the
// cgame "config values already registered" once-guard, then force a few
// per-connection cvars back to their defaults.
//
// Behavior (proven instruction-by-instruction from the .mcode):
//   1. Walk the 184-entry cg_cvarTable (0x300851f0, cgCvarTable_t, stride 0x10) and
//      register each entry:
//        3002b1c0 MOV EAX,[ESI+4]   = entry.cvarFlags     (entry+0xc)
//        3002b1c3 MOV ECX,[ESI]     = entry.defaultString (entry+0x8)
//        3002b1c5 MOV EDX,[ESI-4]   = entry.cvarName      (entry+0x4)
//        3002b1c9 MOV EAX,[ESI-8]   = entry.vmCvar        (entry+0x0)
//      (ESI starts at 0x300851f8 = &entry[0].defaultString, so entry base is ESI-8.)
//      pushes: flags, default, name, handle, then id 7 -> trap_Cvar_Register.
//      ADD ESP,0x14 cleans 5 dwords; ADD ESI,0x10 advances; DEC EDI (0xb8=184)
//      controls the loop (JNZ). Register order proves the argument order
//      (handle, name, default, flags).
//   2. Read the current "sv_running" cvar string into a 0x400 stack buffer via
//      trap_Cvar_VariableStringBuffer (id 0xb), then Q_atoi it and store the result
//      into cgs_localServer (0x30447ab8) — the once-guard other config
//      setters read (e.g. CG_ParseServerinfo forces g_gametype only on first init).
//      "sv_running" is nonzero exactly when a local server is up, so this seeds the
//      guard from the local-server flag. (0x3005b6ce is a JMP thunk to Q_atoi's body
//      at 0x3005b646.)
//   3. Force three per-connection cvars to fixed values via trap_Cvar_Set (id 9):
//      cl_stance="0", cl_run="1", cg_objectiveText="". Each id-9 site pushes
//      (value, name) then id 9; on-stack order is (id, name, value).
//
// Name adjudication: the .mcode header guesses CG_PlaySoundAliasByName purely by a
// 0xb4-byte size match. REJECTED — this function plays no sound; it registers the
// cgame cvar table and seeds the config-values guard. The broad-corpus cgame name
// for that role is CG_RegisterCvars (mirrors the server's G_RegisterCvars, which the
// server name bank lists with the same shape). Traps 7/0xb resolve to the id-Tech
// trap_Cvar_Register / trap_Cvar_VariableStringBuffer by their proven cvar-name/
// value/flags argument shapes; the server bank protos match.
//
// ABI: void(void). SUB ESP,0x404 reserves the 0x400 string buffer plus the /GS
// canary slot; MOV EAX,[__security_cookie] / MOV [ESP+0x408],EAX snapshots the
// cookie, and the epilogue reloads it (MOV ECX,[ESP+0x440]) and tail-checks it via
// __security_check_cookie (0x30061639) before ADD ESP,0x404 / RET. Those cookie
// save/check instructions are compiler-generated MSVC /GS stack-protector code, not
// source statements, and are omitted from the body (see CG_RegisterSurfaceTypeSounds
// for the same convention).

#include "../client_recovered.h"
#include "../globals.h"

#include <stdint.h>

void CG_RegisterCvars(void)
{
    // 3002b1c0..3002b1de: register each cgame cvar from the table.
    // The loop is count-controlled (DEC EDI from 184, JNZ), i.e. a fixed 184-entry
    // walk with stride 0x10.
    for (int32_t i = 0; i < CG_CVAR_TABLE_COUNT; ++i) {
        const cvarTable_t *entry = &cg_cvarTable[i];
        trap_Cvar_Register(entry->vmCvar, entry->cvarName,
                           entry->defaultString, entry->cvarFlags);
    }
    cgame_compat_register_presentation_cvars();

    // 3002b1e0..3002b1fc: read "sv_running" into a 0x400 buffer, then Q_atoi it.
    char svRunning[MAX_STRING_CHARS];
    trap_Cvar_VariableStringBuffer("sv_running", svRunning, (int32_t)sizeof(svRunning));

    // 3002b1fc..3002b20d: cgs_localServer = Q_atoi(sv_running string).
    // Nonzero when a local server is running; used as the config-values once-guard.
    cgs_localServer = coduo_crt_atoi(svRunning);

    // 3002b201..3002b236: force three per-connection cvars to fixed values.
    trap_Cvar_Set("cl_stance", "0");        // 3002b201..3002b212
    trap_Cvar_Set("cl_run", "1");           // 3002b218..3002b224
    trap_Cvar_Set("cg_objectiveText", "");  // 3002b22a..3002b236
}
