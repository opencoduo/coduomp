// Source: uo_cgame_mp_x86.dll 0x3001b7d0..0x3001bbb7
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001b7d0_3001bbb7.mcode
//
// CG_DrawVote -- draw either the active team-kill complaint prompt/status or the
// current server vote. Identity is proven by the cgs voteTime/voteYes/voteNo/
// voteString globals, the "vote yes"/"vote no" key binding queries, the complaint
// localization tokens, and the remaining-seconds calculation. The .mcode header's
// CG_DrawTracer label is rejected as a forbidden size match: no trajectory, tracer,
// scene, or effect operation occurs here.

#include "../client_recovered.h"
#include "client/common/client_format_validation.h"

#include <stdint.h>
#include <string.h>

#define PS_EFLAG_VOTE_KEYS_HIDDEN ((uint32_t)0x10000)

#define CG_VOTE_TEXT_X 8.0f

enum {
    CG_VOTE_TEXT_STYLE = 3,
    /* Jump table @0x3001bbb8: cg_complaintClientNum+4 indexes; -1 -> "CGAME_COMPLAINTFILED"
     * (0x3001b8c4), -2 -> DISMISSED, -3 -> SERVERHOST, -4 -> "CGAME_SERVERHOSTTEAMKILLED"
     * (0x3001b8d9). A prior pass had these enum values reversed (FILED=-4..TEAMKILL=-1),
     * so every complaint status string was shown for the wrong code. */
    CG_COMPLAINT_STATUS_FILED = -1,
    CG_COMPLAINT_STATUS_DISMISSED = -2,
    CG_COMPLAINT_STATUS_SERVER_HOST = -3,
    CG_COMPLAINT_STATUS_HOST_TEAMKILL = -CG_COMPLAINT_STATUS_COUNT
};

void CG_DrawVote(void)
{
    int32_t initialNow = coduo_int32_from_bits((uint32_t)cg_time);
    int32_t initialComplaintEnd = cg_complaintEndTime;
    float color[4] = {1.0f, 1.0f, 0.0f, 1.0f};
    char yesKey[256];
    char noKey[256];
    const char *binding;

    /* 0x3001b7f0..0x3001b893 skips only the binding construction when the
     * entry snapshots say that neither display is active. It does not return:
     * both timing globals are read again at 0x3001b89a. */
    if (initialComplaintEnd > initialNow || cg_voteTime != 0) {
        /* Both complaint and ordinary vote text can show the current yes/no
         * bindings. The DLL uses CRT strncpy(count=255) followed by an explicit
         * byte-255 NUL. */
        if (UI_KeysStringForBinding("vote yes", (char **)&binding) != 0) {
            strncpy(yesKey, binding, 255);
        } else {
            strncpy(yesKey, "vote yes", 255);
        }
        yesKey[255] = '\0';

        if (UI_KeysStringForBinding("vote no", (char **)&binding) != 0) {
            strncpy(noKey, binding, 255);
        } else {
            strncpy(noKey, "vote no", 255);
        }
        noKey[255] = '\0';
    }

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (cg_complaintEndTime > coduo_int32_from_bits((uint32_t)cg_time)) {
        const char *statusToken = NULL;
        const char *text;

        switch (cg_complaintClientNum) {
        case CG_COMPLAINT_STATUS_FILED:
            statusToken = "CGAME_COMPLAINTFILED";
            break;
        case CG_COMPLAINT_STATUS_DISMISSED:
            statusToken = "CGAME_COMPLAINTDISMISSED";
            break;
        case CG_COMPLAINT_STATUS_SERVER_HOST:
            statusToken = "CGAME_COMPLAINTSERVERHOST";
            break;
        case CG_COMPLAINT_STATUS_HOST_TEAMKILL:
            statusToken = "CGAME_SERVERHOSTTEAMKILLED";
            break;
        default:
            break;
        }

        if (statusToken != NULL) {
            text = CG_SafeTranslateString_Internal("cgame", statusToken);
            if (text != NULL) {
                /* This one status leg calls the syscall pointer directly at
                 * 0x3001b92c; the other four draw sites call the wrapper at
                 * 0x3003de30. Preserve the distinct call target. */
                cgame_syscall(CG_R_TEXT_PAINT,
                              CG_FloatBits(CG_VOTE_TEXT_X), CG_FloatBits(200.0f), 0,
                              CG_FloatBits(0.20833333f), (intptr_t)color,
                              (intptr_t)text, 0, 0, CG_VOTE_TEXT_STYLE);
            }
            /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
            return;
        }

        int32_t validityClientNum = cg_complaintClientNum;
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
        if ((uint32_t)validityClientNum >= MAX_CLIENTS) {
            Com_Error(ERR_DROP,
                      "\x15" "CG_DrawVote: invalid complaint client number "
                      "%i",
                      validityClientNum);
            return;
        }
        clientInfo_t *validityClient = &bgs.clientinfo[validityClientNum];
        if (validityClient->infoValid == 0) {
            return;
        }

        /* 0x3001b960 translates first, then 0x3001b96f reloads the possibly
         * callback-mutated client number and repeats the target-width IMUL. */
        const char *teamkillFormat =
            CG_SafeTranslateString_Internal("cgame", "CGAME_COMPLAINTTEAMKILLFILE");
        int32_t nameClientNum = cg_complaintClientNum;
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
        if ((uint32_t)nameClientNum >= MAX_CLIENTS) {
            Com_Error(ERR_DROP,
                      "\x15" "CG_DrawVote: invalid complaint client number "
                      "%i after localization",
                      nameClientNum);
            return;
        }
        clientInfo_t *nameClient = &bgs.clientinfo[nameClientNum];
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
        if (client_compat_validate_format_signature(teamkillFormat, "s") ==
            qfalse) {
            Com_Printf("WARNING: rejected invalid complaint format\n");
            text = teamkillFormat;
        } else {
            text = va(teamkillFormat, nameClient->name);
        }
        trap_R_Text_Paint(CG_FloatBits(CG_VOTE_TEXT_X), CG_FloatBits(200.0f), 0,
                  CG_FloatBits(0.20833333f), (intptr_t)color,
                  (intptr_t)text, 0, 0, CG_VOTE_TEXT_STYLE);

        /* 0x3001b9ae/0x3001b9b6: va() receives BOTH yesKey and noKey (PUSH noKey then
         * yesKey); the format has two %s. A prior pass passed only yesKey. */
        const char *const pressFormat =
            CG_SafeTranslateString_Internal("cgame", "CGAME_PRESSYESNO");
        if (client_compat_validate_format_signature(pressFormat, "ss") ==
            qfalse) {
            Com_Printf("WARNING: rejected invalid vote-key format\n");
            text = pressFormat;
        } else {
            text = va(pressFormat, yesKey, noKey);
        }
        trap_R_Text_Paint(CG_FloatBits(CG_VOTE_TEXT_X), CG_FloatBits(210.0f), 0,
                  CG_FloatBits(0.20833333f), (intptr_t)color,
                  (intptr_t)text, 0, 0, CG_VOTE_TEXT_STYLE);
        return;
    }

    /* 0x3001ba02..0x3001ba08: without an active vote the target returns before
     * clearing voteModified, playing the alert, or querying milliseconds. */
    if (cg_voteTime == 0) {
        return;
    }

    if (cg_voteModified != qfalse) {
        snapshot_t *soundSnap = cg_snap;
        const void *soundOrigin = &soundSnap->ps.psOrigin;
        cg_voteModified = qfalse;
        int32_t soundClientNum = soundSnap->ps.psClientNum;
        const char *talkSound = cgs_media_playerTalkSound;
        CG_PlaySoundAliasByName(soundClientNum, soundOrigin, talkSound);
    }

    {
        int32_t milliseconds = coduo_int32_from_bits(
            (uint32_t)cgame_syscall(CG_MILLISECONDS));
        int32_t delta = coduo_int32_from_bits(
            (uint32_t)cg_voteTime - (uint32_t)milliseconds);
        int32_t seconds = delta / 1000;
        qboolean keysHidden;
        const char *voteLabel;
        const char *yesLabel;
        const char *noLabel;
        const char *text;

        if (seconds < 0) {
            if (seconds < -2) {
                return;
            }
            seconds = 0;
        }

        /* The entity-state flag is tested at 0x3001ba6f, before either path's
         * translation/format/draw calls. Do not reload cg_snap after them. */
        keysHidden = (cg_snap->ps.entityStateFlags &
                      PS_EFLAG_VOTE_KEYS_HIDDEN) != 0;
        voteLabel = CG_SafeTranslateString_Internal("cgame", "CGAME_VOTE");
        text = va("%s(%i):%s", voteLabel, seconds, cg_voteString);
        trap_R_Text_Paint(CG_FloatBits(CG_VOTE_TEXT_X), CG_FloatBits(200.0f), 0,
                  CG_FloatBits(0.20833333f), (intptr_t)color,
                  (intptr_t)text, 0, 0, CG_VOTE_TEXT_STYLE);

        noLabel = CG_SafeTranslateString_Internal("cgame", "CGAME_NO");
        yesLabel = CG_SafeTranslateString_Internal("cgame", "CGAME_YES");
        /* Both paths read voteNo before voteYes (0x3001badb/0x3001bae1 and
         * 0x3001bb56/0x3001bb5d), after both translation calls. */
        {
            int32_t voteNo = cg_voteNo;
            int32_t voteYes = cg_voteYes;

            if (keysHidden) {
                text = va("%s:%i, %s:%i", yesLabel, voteYes, noLabel, voteNo);
            } else {
                text = va("%s(%s):%i, %s(%s):%i", yesLabel, yesKey, voteYes,
                          noLabel, noKey, voteNo);
            }
        }
        trap_R_Text_Paint(CG_FloatBits(CG_VOTE_TEXT_X), CG_FloatBits(210.0f), 0,
                  CG_FloatBits(0.20833333f), (intptr_t)color,
                  (intptr_t)text, 0, 0, CG_VOTE_TEXT_STYLE);
    }
}
