// Source: uo_cgame_mp_x86.dll 0x3001a610..0x3001a7b7
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001a610_3001a7b7.mcode
//
// CG_DrawCrosshairNames — the classic idTech/CoD "draw the name of the player
// currently under the crosshair" HUD element. When enabled, it refreshes the
// crosshair target, computes a fade color (so the name lingers briefly after the
// target is lost), tints the name by the target's health when that health is known
// for the same entity (green->yellow->red), and draws the cleaned display name.
//
// Name adjudication: the .mcode header names this CG_DrawPlayerWeaponNameBack, a
// PURE SIZE match (win 0x1a7 vs some cgame_mp.dll PPC function of size 0x1a8) with
// zero behavioral basis — REJECTED per the no-size-matching rule. There is no
// weapon-name and no "back(ground)" here: the body reads the crosshair-entity
// globals (cg_crosshairEntNum/cg_crosshairEntTime, written by
// CG_ScanForCrosshairEntity 0x3001a4d0, which it calls first), fetches that
// client's display name out of bgs.clientinfo[], and issues the 2D-text draw
// trap CG_R_TEXT_PAINT. The Mac cgame symbol CG_DrawCrosshairNames has the exact
// same five named direct callees, resolving the source name as well as the role.
//
// Machine-code trace (0x3001a610..0x3001a7b6):
//   MOV EAX,[0x3044fa8c]; TEST; JL exit        ; cg_drawCrosshair_vmCvar.integer < 0 -> off
//   MOV EAX,[0x3045788c]; TEST; JZ exit        ; cg_drawCrosshairNames_vmCvar.integer == 0 -> off
//   MOV EAX,[0x304831c0]; TEST; JNZ exit        ; view-state flag set -> off
//   CALL 0x3001a4d0                             ; CG_ScanForCrosshairEntity()
//   MOV EDX,[0x3048adf0]; MOV ECX,0x96; CALL 0x3001d200  ; CG_FadeColor(cg_crosshairEntTime, 150)
//   TEST EDI,EDI; JZ exit                       ; fade expired / not started -> nothing to draw
//   MOV EAX,[0x3048adec]; CMP EAX,0x40; JG exit ; cg_crosshairEntNum > MAX_CLIENTS(64) -> off
//   ECX = cg_snap->ps.psClientNum (*0x4d0 index into bgs.clientinfo)
//   local.infoValid==0 || local.team==0 -> exit
//   crosshair.infoValid==0 -> exit
//   if (local.team != 3 && crosshair.team != local.team) exit  ; team gate
//   name = Q_CleanStr(va("%s", crosshair.name)); if empty -> exit
//   compose RGBA color; draw via trap_R_Text_Paint.
//
// The team gate: team==3 is the "see everyone" case (FFA / spectator); otherwise
// the target must share the local player's team for the name to show — the standard
// team-crosshair-name behavior.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

void CG_DrawCrosshairNames(void)
{
    // 0x3001a610..0x3001a634: three enable gates. cg_drawCrosshair_vmCvar.integer is treated as a
    // signed flag here (JL): a negative value disables. The cvar toggle and the
    // view-state flag are plain nonzero/zero tests.
    if (cg_drawCrosshair_vmCvar.integer < 0) {
        return;
    }
    if (cg_drawCrosshairNames_vmCvar.integer == 0) {
        return;
    }
    if (cg_thirdPerson != 0) {
        return;
    }

    // 0x3001a63b: refresh cg_crosshairEntNum / cg_crosshairEntTime for this frame.
    CG_ScanForCrosshairEntity();

    // 0x3001a640..0x3001a654: fade the name over 150 ms from when the target was
    // last acquired. CG_FadeColor returns a static vec4_t RGBA (white, alpha ramped)
    // or NULL once the fade has expired. The alpha (color[3]) is reused below as the
    // name's draw opacity.
    vec_t *fade = CG_FadeColor(cg_crosshairEntTime, 150);
    if (fade == NULL) {
        return;
    }

    // 0x3001a65a: crosshair entity must be a real client slot.
    int32_t entityNum = cg_crosshairEntNum;
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if ((uint32_t)entityNum >= (uint32_t)MAX_CLIENTS) {
        return;
    }

    // 0x3001a668..0x3001a690: the LOCAL player's per-client state must be live and
    // have a team. cg_snap->ps.psClientNum indexes bgs.clientinfo[] (stride 0x4d0).
    snapshot_t *snap = cg_snap;
    clientInfo_t *localState = &bgs.clientinfo[snap->ps.psClientNum];
    if (localState->infoValid == 0) {
        return;
    }
    int32_t localTeam = localState->team;
    if (localTeam == TEAM_FREE) {
        return;
    }

    // 0x3001a696..0x3001a6b5: the crosshair TARGET's per-client state must be live,
    // and (unless the local player is on team 3 = "see everyone") the target must be
    // on the local player's team.
    clientInfo_t *targetState = &bgs.clientinfo[entityNum];
    if (targetState->infoValid == 0) {
        return;
    }
    if (localTeam != TEAM_SPECTATOR && targetState->team != localTeam) {
        return;
    }

    // 0x3001a6bb..0x3001a6dd: format the target's display name, clean color codes,
    // and require a non-empty result. va() returns a pointer into its own writable
    // static buffer (typed const char* in the header), and Q_CleanStr rewrites that
    // buffer in place, so the const is cast off to match the machine code, which
    // hands va's EAX result straight to Q_CleanStr.
    // 0x3001a6c8 va; 0x3001a6d2 null-check; 0x3001a6da empty-check on the RAW va
    // result; 0x3001a6e5 Q_CleanStr called EXACTLY ONCE, AFTER the guards. A prior
    // pass cleaned before the empty guard (and cleaned twice), so an all-color-code
    // name (e.g. "^1^2") -- non-empty raw but empty once cleaned -- was dropped by the
    // recon where the DLL proceeds to draw it.
    char *name = (char *)va("%s", targetState->name);
    if (name == NULL || name[0] == '\0') {
        return;
    }
    Q_CleanStr(name);

    // 0x3001a6ea..0x3001a77f: compose the name color (RGBA). The array is drawn as a
    // vec4_t; only R,G,B are computed by the branches below, and the alpha (index 3)
    // is set from the faded-out crosshair opacity at the end.
    float color[4];

    // 0x3001a6f0: only tint by health when the latched health entity matches the
    // entity actually under the crosshair; otherwise the name is plain white.
    if (cg_crosshairEntNum == cg_crosshairHealthEntNum) {
        // 0x3001a6f8..0x3001a72e: frac = clamp(cg_crosshairHealth * 0.01, 0, 1).
        // 0x3001a6f8 FILD feeds FMUL 0.01 directly (no FSTP DWORD), so the health
        // is not rounded to float first. It remains live in x87 through both
        // clamps, the 0.5 comparison, and the selected color calculation.
        long double frac = (long double)cg_crosshairHealth * 0.01f;
        if (frac > 1.0f) {
            frac = 1.0f;
        } else if (frac < 0.0f) {
            frac = 0.0f;
        }

        // 0x3001a730 FCOM frac,0.5; TEST AH,0x41; JNE 0x3001a757: the (R=1, G=2*frac)
        // arm runs when JNE is taken, i.e. frac <= 0.5 (or unordered); the fall-through
        // (frac > 0.5) gives (R=2*(1-frac), G=1). A prior pass attached the arms to
        // frac>=0.5, inverting the health gradient (full health drew yellow, not green).
        //   frac <= 0.5 (hurt side):    R = 1,           G = 2*frac
        //   frac >  0.5 (healthy side): R = 2*(1 - frac), G = 1
        color[2] = 0.0f;
        if (!(frac > 0.5f)) {
            color[0] = 1.0f; // 0x3001a759 MOV [ESP+8],1.0
            color[1] = (float)(frac + frac); // 0x3001a757 FADD ST0,ST0; FSTP [ESP+0xc]
        } else {
            long double red = 1.0L - frac;
            color[0] = (float)(red + red); // 0x3001a745 FSUB; FADD ST0,ST0
            color[1] = 1.0f; // 0x3001a777 MOV [ESP+0xc],1.0
        }
    } else {
        // 0x3001a767..0x3001a77d: white.
        color[0] = 1.0f;
        color[2] = 1.0f;
        color[1] = 1.0f;
    }

    // 0x3001a77f..0x3001a793: alpha = crosshairFadeAlpha * 0.6.
    color[3] = (float)((long double)fade[3] * 0.6f);

    // 0x3001a782..0x3001a7a9: draw the name centered via CG_R_TEXT_PAINT (2D text draw).
    // The push order proves the slot mapping:
    //   a0 = 345.0 (x), a1 = 217.0 (y), a2 = 0, a3 = 0.25 (scale),
    //   a4 = &color, a5 = name, a6 = 0, a7 = 0, a8 = 3 (style/flags).
    trap_R_Text_Paint(CG_FloatBits(345.0f), CG_FloatBits(217.0f), 0, CG_FloatBits(0.25f), (intptr_t)color, (intptr_t)name, 0, 0, 3);
}
