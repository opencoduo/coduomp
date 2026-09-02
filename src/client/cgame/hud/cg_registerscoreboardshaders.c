// FUN_30037e90_30038051  (0x30037e90..0x30038051)  uo_cgame_mp_x86.dll
//
// CG_RegisterScoreboardShaders — precache every shader the multiplayer
// scoreboard draws.
//
// This is the shader-registration step of the scoreboard subsystem. The sibling
// CG_ScoreboardHeight (0x30036e50) documents the same caller (CG_DrawScoreboard,
// 0x30037b50), which "pushes the scoreboard strings ... registers scoreboard
// shaders" right before measuring the list — this function is that registration.
// It registers, in order:
//   - the solid fill shaders "black"/"white" used for the row backgrounds and
//     the header/footer bars (six registrations, the black/white pattern the
//     scoreboard alternates),
//   - the four scroll-indicator shaders hudScoreboardScroll_UpArrow / _UpKey /
//     _DownArrow / _DownKey,
//   - the four team-banner shaders whose *shader names* are taken from the
//     cvars g_ScoresBanner_Spectators / _Axis / _Allies / _None: for each, the
//     cvar's current string value is copied into a local 1KB buffer via
//     trap_Cvar_VariableStringBuffer (id 0xb) and that value is then registered.
//
// Before every single registration it pumps the loading HUD with
// CG_DrawInformation(0) — the same loading-pump idiom used by the sibling
// precachers CG_RegisterMenuAssets (0x3002dcf0) and CG_RegisterConfigStringShader
// (0x300387e0). Each cgame_syscall(CG_R_REGISTERSHADER, name, 5) returns a
// qhandle_t that is discarded here: this function only forces the assets to load
// (precache side effect), it stores nothing.
//
// Naming: the .mcode size-guess "script_func_objective_add" is REJECTED — that is
// a *server* script command from game_mp_uo.dll (wrong DLL), matched only by byte
// size (0x1c1 == 0x1c1) with zero behavioral basis (see memory
// size-match-name-is-noise). The body proves scoreboard shader precaching in
// cgame, so it is named CG_RegisterScoreboardShaders from that role. Exact
// original CoD symbol unproven.
//
// Identities proven from bytes / call graph:
//   0x3002a530  = CG_DrawInformation(qboolean force) — loading-HUD pump, called
//                 with 0 (client_recovered.h decl).
//   trap 0x59   = CG_R_REGISTERSHADER  — cgame_syscall(0x59, name, sort=5); same
//                 shape and sort=5 as CG_RegisterConfigStringShader (0x300387e0).
//   trap 0xb    = CG_CVAR_VARIABLE_STRING_BUFFER — trap_Cvar_VariableStringBuffer
//                 (name, buffer, 0x400).
//   [0x30081650]/0x30061639 = /GS stack cookie + __security_check_cookie (frame
//                 guard around the 0x400 stack buffer). Not modelled in C.
//
// The register-shader calls are written as raw cgame_syscall(CG_R_REGISTERSHADER,
// ...) (not a trap_ wrapper) to mirror the discard-result siblings above. The
// four cvar buffers are distinct locals in the machine frame; the write (trap
// 0xb) and the following read (trap 0x59) of each pair provably target the SAME
// local (the intervening pushes exactly offset the differing LEA displacements),
// so each is modelled as one buffer written then registered.

#include "client/cgame/globals.h"
#include "client/cgame/client_recovered.h"

enum {
    CG_SB_REGISTER_SHADER_SORT = 5
};

void CG_RegisterScoreboardShaders(void)
{
    char cvarValue[MAX_STRING_CHARS];

    /* 0x30037e9d..0x30037f7e: fixed fill + scroll-indicator shaders. Each is
     * preceded by a CG_DrawInformation(0) loading pump (0x3002a530). */
    CG_DrawInformation(0);
    cgame_syscall(CG_R_REGISTERSHADER, (intptr_t)cg_blackMaterialName, CG_SB_REGISTER_SHADER_SORT);
    CG_DrawInformation(0);
    cgame_syscall(CG_R_REGISTERSHADER, (intptr_t)cg_whiteMaterialName, CG_SB_REGISTER_SHADER_SORT);
    CG_DrawInformation(0);
    cgame_syscall(CG_R_REGISTERSHADER, (intptr_t)cg_blackMaterialName, CG_SB_REGISTER_SHADER_SORT);
    CG_DrawInformation(0);
    cgame_syscall(CG_R_REGISTERSHADER, (intptr_t)cg_whiteMaterialName, CG_SB_REGISTER_SHADER_SORT);

    CG_DrawInformation(0);
    cgame_syscall(CG_R_REGISTERSHADER, (intptr_t)cg_whiteMaterialName, CG_SB_REGISTER_SHADER_SORT);
    CG_DrawInformation(0);
    cgame_syscall(CG_R_REGISTERSHADER, (intptr_t)cg_blackMaterialName, CG_SB_REGISTER_SHADER_SORT);
    CG_DrawInformation(0);
    cgame_syscall(CG_R_REGISTERSHADER, (intptr_t)cg_scoreboardScrollUpArrowMaterialName, CG_SB_REGISTER_SHADER_SORT);
    CG_DrawInformation(0);
    cgame_syscall(CG_R_REGISTERSHADER, (intptr_t)cg_scoreboardScrollUpKeyMaterialName, CG_SB_REGISTER_SHADER_SORT);

    CG_DrawInformation(0);
    cgame_syscall(CG_R_REGISTERSHADER, (intptr_t)cg_scoreboardScrollDownArrowMaterialName, CG_SB_REGISTER_SHADER_SORT);
    CG_DrawInformation(0);
    cgame_syscall(CG_R_REGISTERSHADER, (intptr_t)cg_scoreboardScrollDownKeyMaterialName, CG_SB_REGISTER_SHADER_SORT);

    /* 0x30037f84..0x30038038: the four team banners. Each banner's shader name is
     * whatever the corresponding g_ScoresBanner_* cvar currently holds; copy that
     * value into a local buffer, then register it. */
    trap_Cvar_VariableStringBuffer(cg_scoreboardSpectatorsBannerCvarName, cvarValue, MAX_STRING_CHARS);
    CG_DrawInformation(0);
    cgame_syscall(CG_R_REGISTERSHADER, (intptr_t)cvarValue, CG_SB_REGISTER_SHADER_SORT);

    trap_Cvar_VariableStringBuffer(cg_scoreboardAxisBannerCvarName, cvarValue, MAX_STRING_CHARS);
    CG_DrawInformation(0);
    cgame_syscall(CG_R_REGISTERSHADER, (intptr_t)cvarValue, CG_SB_REGISTER_SHADER_SORT);

    trap_Cvar_VariableStringBuffer(cg_scoreboardAlliesBannerCvarName, cvarValue, MAX_STRING_CHARS);
    CG_DrawInformation(0);
    cgame_syscall(CG_R_REGISTERSHADER, (intptr_t)cvarValue, CG_SB_REGISTER_SHADER_SORT);

    trap_Cvar_VariableStringBuffer(cg_scoreboardNoneBannerCvarName, cvarValue, MAX_STRING_CHARS);
    CG_DrawInformation(0);
    cgame_syscall(CG_R_REGISTERSHADER, (intptr_t)cvarValue, CG_SB_REGISTER_SHADER_SORT);
}
