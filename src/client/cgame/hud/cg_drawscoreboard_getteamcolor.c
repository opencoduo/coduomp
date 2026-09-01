// Source: uo_cgame_mp_x86.dll 0x30036f20..0x3003708a
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30036f20_3003708a.mcode
//
// CG_DrawScoreboard_GetTeamColor — resolve the scoreboard section RGB color for a
// team into a caller-supplied vec3, then clamp every component into [0,1].
//
// Selector dispatch (0x30036f39..0x30036f3f: DEC EAX; JZ / DEC EAX; JZ):
//   team == TEAM_AXIS   (1) -> parse the "g_TeamColor_Axis"  cvar string
//   team == TEAM_ALLIES (2) -> parse the "g_TeamColor_Allies" cvar string
//   otherwise               -> the color is white {1,1,1} (no cvar read/parse)
//
// The cvar path (0x30036f5b / 0x30036f87) reads the current cvar value into a 1024
// (0x400) byte stack buffer via trap_Cvar_VariableStringBuffer(name, buf, 0x400)
// (cgame trap id 0xb), then parses three floats out of that text with
// sscanf(buf, "%f %f %f", &colorOut[0], &colorOut[1], &colorOut[2]).
//
// Each component is then clamped to [0,1] (0x30036fb9..0x30037083), three copies of
// the same idiom:  if (c < 0.0f) c = 0.0f;  else if (c > 1.0f) c = 1.0f;
// FLD c; FCOMP 0.0f; FNSTSW; TEST AH,5; JP -> (c >= 0) go compare vs 1.0f, else load
// 0.0f; then FCOMP 1.0f; TEST AH,0x41 (C0|C3 = "<="); JNZ -> keep c, else load 1.0f.
// The third component's "in range" tail does a MOV ECX,[EBX]; MOV [EBX],ECX
// reload-and-store-back of the same dword (a register-scheduling no-op; the value is
// unchanged) — not modeled in C.
//
// .rdata constants (objdump -s -j .rdata):
//   0x30079d24 "g_TeamColor_Allies"   0x30079d38 "%f %f %f"   0x30079d44 "g_TeamColor_Axis"
//   0x3007bce0 = 1.0f (0x3f800000)     0x3007bcec = 0.0f (0x00000000)
//
// NAMING: the .mcode header's size-matched guess "BG_AnimScriptAnimation" is REJECTED
// (pure win-size 0x16a/0x16b match, zero behavioral basis): this function has no anim
// script anything — it reads two named team-color cvars and clamps an RGB triple. The
// role-proven name CG_DrawScoreboard_GetTeamColor was already established from the two
// callers (CG_DrawScoreboard_ScoresList 0x30037810, CG_DrawScoreboard_ColorSection
// 0x30037090), both of which invoke it as GetTeamColor(team, &sectionColor) to fill a
// scoreboard row/section color; corroborated by the same-module PPC name bank.
//
// ABI (i386, evidence): the color-out pointer arrives in ESI (register argument; read
// as [ESI], [ESI+4], [ESI+8]; never loaded from a stack slot). The team selector is the
// single cdecl stack dword at [ESP_entry+4] (== [ESP+0x408] after SUB ESP,0x404). RET
// with no immediate: the caller cleans the one stack arg. The established provisional
// decl models the ESI out-pointer as an ordinary parameter (team, colorOut) to match
// the call sites for the syntax-only build.
//
// /GS: 0x404 frame = a 0x400 char buffer + the __security_cookie saved at [ESP+0x400]
// (MOV EAX,[0x30081650] / MOV [ESP+0x400],EAX) and validated on every exit via
// __security_check_cookie (0x30061639: CMP ECX,[0x30081650]; JNE __report; RET). The
// cookie handling is a compiler-emitted stack-guard and is not part of the logic.
//
// Callees:
//   trap_Cvar_VariableStringBuffer (cgame trap 0xb, client_recovered.h)
//   sscanf (MSVC CRT 0x3005bf24): builds a read/string _iobuf from the buffer
//     (_flag 0x49 = _IOREAD|_IOSTRG|..., _cnt = strlen via 0x3005e750), then calls the
//     scanf core _input (0x3005eff2). This is sscanf, NOT sprintf — the args are the
//     source text buffer, the "%f %f %f" format, and three float OUT pointers; it parses
//     values out of the string. (The "sprintf" label some sibling comments attach to
//     0x3005bf24 is a corpus mislabel; the byte-level FILE-from-string + _input shape is
//     unambiguously the scanf family.) The standard declaration comes from <stdio.h>.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

#include <stdint.h>
#include <stdio.h>

/* sscanf (MSVC CRT 0x3005bf24) — variadic string parse; caller-cleaned cdecl. Only
 * the three-float shape used here is exercised; portable recovered source uses
 * the standard declaration (source text, format, out pointers). */

#if defined(_MSC_VER)
#define CG_SB_COLOR_ALWAYS_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define CG_SB_COLOR_ALWAYS_INLINE inline __attribute__((always_inline))
#else
#define CG_SB_COLOR_ALWAYS_INLINE inline
#endif

/* NOT_FROM_ORIGINAL_SOURCE: source spelling of the three inlined clamp
 * clusters; forced inline so it cannot create an extra recovered function. */
static CG_SB_COLOR_ALWAYS_INLINE void
cgame_compat_clamp_scoreboard_color(vec_t *c)
{
    /* clamp(*c, 0.0f, 1.0f) — the per-component idiom at 0x30036fb9, 0x30036feb,
     * 0x3003701d. */
    if (*c < 0.0f)
        *c = 0.0f;         /* 0x3007bcec = 0.0f */
    else if (*c > 1.0f)
        *c = 1.0f;         /* 0x3007bce0 = 1.0f */
}

void CG_DrawScoreboard_GetTeamColor(int team, vec3_t colorOut)
{
    char cvarValue[MAX_STRING_CHARS]; /* the /GS-guarded 1024-byte cvar text buffer */

    if (team == TEAM_AXIS) {
        /* 0x30036f87: g_TeamColor_Axis */
        trap_Cvar_VariableStringBuffer("g_TeamColor_Axis", cvarValue,
                                       (int32_t)sizeof(cvarValue));
        sscanf(cvarValue, "%f %f %f", &colorOut[0], &colorOut[1], &colorOut[2]);
    } else if (team == TEAM_ALLIES) {
        /* 0x30036f5b: g_TeamColor_Allies */
        trap_Cvar_VariableStringBuffer("g_TeamColor_Allies", cvarValue,
                                       (int32_t)sizeof(cvarValue));
        sscanf(cvarValue, "%f %f %f", &colorOut[0], &colorOut[1], &colorOut[2]);
    } else {
        /* 0x30036f41: any other team selector -> white. */
        colorOut[0] = 1.0f;
        colorOut[1] = 1.0f;
        colorOut[2] = 1.0f;
    }

    cgame_compat_clamp_scoreboard_color(&colorOut[0]);
    cgame_compat_clamp_scoreboard_color(&colorOut[1]);
    cgame_compat_clamp_scoreboard_color(&colorOut[2]);
}

#undef CG_SB_COLOR_ALWAYS_INLINE
