// Source: uo_cgame_mp_x86.dll 0x3001bd50..0x3001bed8
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001bd50_3001bed8.mcode
//
// CG_DrawSpectatorFollowHints — draw the spectator "follow" key-hint HUD: up to
// three bottom-of-screen text lines that tell a spectator who is following a player
// which keys advance to the next/previous player and which stops following. Each
// line is "<localized label>: <bound key(s)>" (built with va(label, keyString)),
// drawn white at x=240, font scale 0.2083, stacked down y=416/426/436 via the 2D
// text-draw trap trap_R_Text_Paint (id 54).
//
// Gates (both must pass or the function returns immediately):
//   1. cg_descriptiveText_vmCvar.integer (.data 0x3052f6ac) — a cvar-integer mirror read
//      as a boolean enable flag. `if (!cg_descriptiveText_vmCvar.integer) return;`
//      (MOV EAX,[0x3052f6ac]; TEST EAX,EAX; JZ end at 0x3001bd53/bd58/bd7a).
//   2. cg_snap->ps.playerStateFlags (cg_snap = 0x30459160, field +0x18) must carry
//      at least one of the follow bits: `if (!(flags & 0x300000)) return;`
//      (TEST [EAX+0x18],0x300000; JZ end at 0x3001bd85/bd8c).
//
// After the gates it refreshes the UI key-binding cache (Controls_GetConfig,
// 0x30056790) so the key strings it is about to render reflect the live bindings.
//
// The three lines (each guarded by a distinct playerState flag bit):
//   A. flags & 0x100000  -> label CGAME_FOLLOWNEXTPLAYER, key of "+attack",       y=416
//   B. flags & 0x100000  -> label CGAME_FOLLOWPREVIOUSPLAYER, key of "toggle cl_run", y=426
//      (lines A+B share the 0x100000 branch; if it runs, the running y for line C
//       is advanced to 436, otherwise line C keeps y=416.)
//   C. flags & 0x200000  -> label CGAME_FOLLOWSTOP, key of "toggle cl_run" (or, if
//      that is unbound, "+speed"), y = running y (436 if A/B ran, else 416).
//
// Per-line key lookup (UI_KeysStringForBinding, 0x30056b40; register-split ABI
// EAX=&outStr, ECX=command): it fills `keyStr` with the human-readable name of the
// key(s) bound to the command and returns the bound-key count. When the command is
// unbound (count == 0) the machine code overwrites the outStr slot with the command
// literal itself (MOV [ESP],cmd), so the hint falls back to showing the raw command
// token rather than an empty/garbage string. Line C additionally treats a fully
// unbound pair (both "toggle cl_run" and "+speed" return 0) as "skip line C".
//
// The label is localized via CG_SafeTranslateString_Internal("cgame", "CGAME_FOLLOW..."), and
// va(label, keyStr) formats the label (a printf-style format string with one %s
// slot) against the bound key name. The result is drawn with trap_R_Text_Paint's 9-word
// vector (x, y, 0, scale, &color, text, 0, 0, style=3) where &color points at the
// 4-float white RGBA {1,1,1,1} built on the stack at entry.
//
// Name adjudication: the .mcode header's "PM_AirMove" is a pure win-size==0x188
// corpus match with zero behavioral basis and is REJECTED — this is a cgame HUD
// draw routine (reads cg_snap, calls UI_* key-binding helpers, CG_SafeTranslateString_Internal,
// va, and the trap_R_Text_Paint 2D text emitter), not a bg_pmove air-acceleration step
// (no velocity/wishdir/accel math, no pmove_t). Named by proven role. The gate
// global 0x3052f6ac is renamed cg_descriptiveText_vmCvar.integer (its sole DLL reference
// is this read); the exact cvar name is unproven.
//
// Float precision: every coordinate/scale/color constant is a raw 32-bit float
// immediate forwarded verbatim to the variadic trap; passed through CG_FloatBits so
// the bit pattern is reproduced exactly (no double promotion), matching the family.
//   0x43700000 = 240.0f (x)   0x43d00000 = 416.0f   0x43d50000 = 426.0f
//   0x43da0000 = 436.0f       0x3e555555 = 0.20833333f (scale)  0x3f800000 = 1.0f
//
// ABI: plain __cdecl, void(void). SUB ESP,0x18 reserves the frame (outStr slot at
// [ESP+0], the running y at [ESP+4], and the 4-float white color at [ESP+8..0x14]);
// each draw block pushes 8 bytes of va args + 0x24 of trap args and balances them
// with ADD ESP,0x2c (va is cdecl caller-clean); ADD ESP,0x18 / RET closes the frame.

#include "client/cgame/globals.h"
#include "client/cgame/client_recovered.h"
#include "client/common/client_format_validation.h"

// Text style/flags constant passed as trap_R_Text_Paint's final word for every line.
#define FOLLOW_HINT_TEXT_STYLE 3

/* NOT_FROM_ORIGINAL_SOURCE: applies the one-string localization contract used
 * by all three follow hints. Invalid mounted text remains visible literally. */
static const char *cgame_compat_format_follow_hint(const char *format, const char *keyString)
{
    if (client_compat_validate_format_signature(format, "s") == qfalse) {
        Com_Printf("WARNING: rejected invalid spectator-follow format\n");
        return format;
    }
    return va(format, keyString);
}

void CG_DrawSpectatorFollowHints(void)
{
    /* The cvar dword is read at 0x3001bd53 before the four color stores and the
     * captured value is tested afterward. */
    int32_t descriptiveTextEnabled = cg_descriptiveText_vmCvar.integer;
    float followHintColorWhite[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    if (!descriptiveTextEnabled) {
        return;
    }

    // Both follow bits must be resolvable before doing any work.
    if (!(cg_snap->ps.playerStateFlags & 0x300000u)) {
        return;
    }

    // Refresh the cached key bindings so the rendered key names are current.
    Controls_GetConfig();

    /* 0x3001bd97 snapshots the post-update flags before the running-y store. */
    uint32_t nextPreviousFlags = cg_snap->ps.playerStateFlags;
    float stopLineY = 416.0f;

    if (nextPreviousFlags & 0x100000u) {
        const char *keyStr;
        const char *translatedLabel;
        const char *text;

        // Line A: "Next Player" bound to "+attack".
        if (UI_KeysStringForBinding("+attack", (char **)&keyStr) == 0) {
            keyStr = "+attack"; // unbound: show the raw command token
        }
        translatedLabel = CG_SafeTranslateString_Internal("cgame", "CGAME_FOLLOWNEXTPLAYER");
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
        text = cgame_compat_format_follow_hint(translatedLabel, keyStr);
        trap_R_Text_Paint(CG_FloatBits(240.0f), CG_FloatBits(416.0f), 0, CG_FloatBits(0.20833333f), (intptr_t)followHintColorWhite,
                          (intptr_t)text, 0, 0, FOLLOW_HINT_TEXT_STYLE);

        // Line B: "Previous Player" bound to "+melee" (0x30076aa8, loaded at
        // 0x3001be0b/0x3001be19). A prior pass used the ADJACENT .rdata string
        // "toggle cl_run" (0x30076a98) -- which is Line C's command, not this one.
        if (UI_KeysStringForBinding("+melee", (char **)&keyStr) == 0) {
            keyStr = "+melee"; // unbound: show the raw command token
        }
        translatedLabel = CG_SafeTranslateString_Internal("cgame", "CGAME_FOLLOWPREVIOUSPLAYER");
        text = cgame_compat_format_follow_hint(translatedLabel, keyStr);
        trap_R_Text_Paint(CG_FloatBits(240.0f), CG_FloatBits(426.0f), 0, CG_FloatBits(0.20833333f), (intptr_t)followHintColorWhite,
                          (intptr_t)text, 0, 0, FOLLOW_HINT_TEXT_STYLE);

        stopLineY = 436.0f;
    }

    if (cg_snap->ps.playerStateFlags & 0x200000u) {
        const char *keyStr;
        const char *translatedLabel;
        const char *text;

        // Line C: "Stop Following" — its key is whichever of "toggle cl_run" or
        // "+speed" is currently bound; if neither is bound, the line is skipped.
        if (UI_KeysStringForBinding("toggle cl_run", (char **)&keyStr) == 0 && UI_KeysStringForBinding("+speed", (char **)&keyStr) == 0) {
            return;
        }
        translatedLabel = CG_SafeTranslateString_Internal("cgame", "CGAME_FOLLOWSTOP");
        text = cgame_compat_format_follow_hint(translatedLabel, keyStr);
        trap_R_Text_Paint(CG_FloatBits(240.0f), CG_FloatBits(stopLineY), 0, CG_FloatBits(0.20833333f), (intptr_t)followHintColorWhite,
                          (intptr_t)text, 0, 0, FOLLOW_HINT_TEXT_STYLE);
    }
}
