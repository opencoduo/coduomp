// Source: uo_cgame_mp_x86.dll 0x30032ac0..0x30032c1e
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30032ac0_30032c1e.mcode
//
// CG_AddHeadIcon — per-frame dispatcher that selects and submits at most one
// floating head icon for a single rendered client entity (cent, arriving in ESI).
// It hands the actual render-entity build to CG_AddHeadIconSprite (0x30032910).
//
// NAME ADJUDICATION: the .mcode mechanical pre-hint `CG_DebugCircleEx` (a
// size-guess: win 0x15e vs matched 0x160) is REJECTED. CG_DebugCircleEx is a
// sin/cos debug ring drawer that issues CG_ADD_DEBUG_LINE (trap 202) in a loop;
// it was reconstructed separately at 0x3001db70. This body has no trig, no loop,
// and no debug-line trap — it is a head-icon selector. The name here is derived
// from behavior (it builds head-icon sprites through CG_AddHeadIconSprite) and the
// icon shaders it chooses; the exact CoD symbol is unproven.
//
// Register-arg ABI: the subject entity is passed in ESI (MOV ESI,EAX at 0x30032ac2),
// modeled as the sole parameter `cent`. The function is caller-cleaned with a plain
// RET (no immediate); the pushed ECX at entry reserves one 4-byte stack local, which
// holds the icon `size` float (initialized to 0.0 at 0x30032ad8, set to 16.0 in the
// visibility-icon path). EBX/EDI/ESI/ECX save/restore and the several early RET exits
// are i386 calling-convention detail, expressed here as ordinary control flow.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/* Config-string base added to cent->currentState.headIconCsIndex before the
 * CG_ConfigString lookup that yields the visibility head-icon shader name (ADD
 * EAX,0x25 at 0x30032b30). The same +37 base is used by the sibling head-icon
 * builder at 0x300217ef. Exact CoD CS_* symbol unproven; the numeric offset is
 * proven. */
enum { CG_HEADICON_CONFIGSTRING_BASE = 37 };

/* Sprite scale (pixels) used for the visibility head icon (MOV [ESP+0xc],0x41800000
 * at 0x30032b55 == 16.0f). */
#define CG_HEADICON_SPRITE_SIZE 16.0f

/* Talk-balloon icon is drawn a little smaller: its scale is reduced by 5.0 before
 * rounding (FSUB [0x3007bde0] == 5.0f at 0x30032bff). */
#define CG_TALKBALLOON_SIZE_BIAS 5.0f

void CG_AddHeadIcon(centity_t *cent)
{
    float size; /* [ESP+0xc]; initialized after the first infoValid load below */

    /* bgs.clientinfo[cent->currentState.clientNum]: skip the entity entirely if its per-client
     * anim/HUD state was never populated (infoValid == 0). The table is indexed
     * as clientNum * 0x4d0 from base 0x305e1f34 (IMUL EAX,EAX,0x4d0; MOV ECX,[EAX+base]). */
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    const int32_t centClientNum = cent->currentState.clientNum;
    if ((uint32_t)centClientNum >= (uint32_t)MAX_CLIENTS) {
        Com_Error(ERR_DROP,
                  "\x15" "CG_AddHeadIcon: invalid entity client number %i",
                  centClientNum);
        return;
    }
    clientInfo_t *centState = &bgs.clientinfo[centClientNum];
    const int32_t centInfoValid = centState->infoValid;
    /* 0x30032ad8 stores zero after TEST loads infoValid and before its JZ. */
    size = 0.0f;
    if (centInfoValid == 0) {
        return;
    }
    int32_t centHudTeam = centState->team; /* [element + 0x2c] (0x305e1f60) */

    /* bgs.clientinfo[cg_snap->ps.psClientNum]: same guard for the LOCAL player's state. */
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    const int32_t localClientNum = cg_snap->ps.psClientNum;
    if ((uint32_t)localClientNum >= (uint32_t)MAX_CLIENTS) {
        Com_Error(ERR_DROP,
                  "\x15" "CG_AddHeadIcon: invalid local client number %i",
                  localClientNum);
        return;
    }
    clientInfo_t *localState = &bgs.clientinfo[localClientNum];
    if (localState->infoValid == 0) {
        return;
    }
    int32_t localHudTeam = localState->team;

    /* (1) Per-entity visibility head icon (e.g. objective / role marker). Drawn when
     * the entity carries a headIconCsIndex and the local player's team is allowed to
     * see it: no team restriction (headIconTeam == 0), local player spectating
     * (localHudTeam == TEAM_SPECTATOR), or the teams match. */
    if (cent->currentState.iHeadIcon != 0) {
        int32_t iconTeam = cent->currentState.headIconTeam;
        if (iconTeam == 0 ||
            localHudTeam == TEAM_SPECTATOR ||
            iconTeam == localHudTeam) {
            const int32_t configStringIndex = coduo_int32_from_bits(
                (uint32_t)cent->currentState.iHeadIcon +
                (uint32_t)CG_HEADICON_CONFIGSTRING_BASE);
            const char *name = CG_ConfigString(configStringIndex);
            qhandle_t shader = CG_RegisterMaterial(name, R_IMAGE_TRACK_HUD);
            if (shader != 0) {
                CG_AddHeadIconSprite(cent, shader, 0, 0);
                size = CG_HEADICON_SPRITE_SIZE;
            }
        }
    }

    /* (2) The local client's own entity, rendered as the kill-cam subject: draw the
     * "you in kill cam" icon and return. cent->currentState.number == cg_clientNum is the
     * local draw-client; the icon is suppressed if the snapshot's own client already
     * IS that client (i.e. we are not actually watching ourselves die). */
    if (cent->currentState.number == cg_clientNum) {
        if (cg_snap->ps.psClientNum != cg_clientNum) {
            CG_AddHeadIconSprite(cent, cgs_youInKillCamIcon, coduo_fp_to_i32_extended(size), 1);
            return;
        }
        /* 0x30032b72 je 0x30032b94: when psClientNum == cg_clientNum (we are not
         * actually watching ourselves die) the killcam icon+return block
         * (0x30032b74..0x30032b93) is skipped and control falls through to the normal
         * status-icon path below. A prior pass returned UNCONDITIONALLY here, so the
         * local player's own entity never drew disconnected/voice/talk-balloon icons. */
    }

    /* (3) Status head icons. */
    uint32_t eFlags = cent->currentState.eFlags;

    /* Disconnected clients get the "disconnected" icon regardless of team. */
    if ((eFlags & EF_HEADICON_DISCONNECTED) != 0) {
        CG_AddHeadIconSprite(cent, cgs_disconnectedIcon, coduo_fp_to_i32_extended(size), 0);
        return;
    }

    /* Voice-chat / talk-balloon icons are only shown to teammates (or a spectator). */
    if (centHudTeam == localHudTeam || localHudTeam == TEAM_SPECTATOR) {
        /* Active per-client voice chat: draw the entity's currently-registered
         * voiceChatIcon while its deadline has not yet passed (voiceChatTime > cg.time). */
        if (cent->voiceChatTime > coduo_int32_from_bits(cg_time)) {
            CG_AddHeadIconSprite(cent, cent->voiceChatIcon, coduo_fp_to_i32_extended(size), 0);
            return;
        }
        /* Otherwise, if the entity is flagged as typing/chatting, draw the talk balloon
         * (rendered slightly smaller: size - 5.0). */
        if ((eFlags & EF_HEADICON_TALKING) != 0) {
            CG_AddHeadIconSprite(cent, cgs_talkBalloonIcon,
                                 coduo_fp_to_i32_extended(
                                     (long double)size -
                                     (long double)CG_TALKBALLOON_SIZE_BIAS),
                                 0);
        }
    }
}
