// Source: uo_cgame_mp_x86.dll 0x3003a250..0x3003a403
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003a250_3003a403.mcode
//
// CG_VoiceChat (0x3003a250) — the voice-chat DISPLAY routine. Called (fastcall,
// ECX=&message) by CG_ParseVoiceChat (0x3003a410) after it decodes an incoming
// server "vchat"/"vtchat"/"vttchat" command. Assembles a cgVoiceChatMsg_t descriptor
// and hands it to CG_PlayVoiceChat (0x30039ff0), which plays the voice sound, stamps
// the speaker's head-icon, and prints/appends the chat line.
//
// The .mcode size-match name "PlayerCmd_ClonePlayer" (broad game_mp corpus guess,
// win size 0x1b3 ~= 0x1b4) is REJECTED: this reads the voice-chat alias tables, the
// 0x4d0-stride clientInfo[] table, formats a team/quick-chat display string,
// and dispatches to CG_PlayVoiceChat — it clones nothing.
//
// SIGNATURE (proven; the provisional arity/types were right, the field names wrong):
//   fastcall ECX = cgVoiceChatMessage_t *msg (origin vec3 read from +0x0/+0x4/+0x8);
//   five caller-cleaned (ADD ESP,0x14) cdecl stack args (entry-relative ESP0+4..+0x14):
//     mode            arg1  0/1/2 -> which display-string variant
//     voiceOnly       arg2  stored to cgVoiceChatMsg_t->voiceOnly
//     clientNum       arg3  speaker; range-clamped to [0,64), indexes the table
//     color           arg4  parser argv[5] int; printed via the second %c after '^'
//                           (RTCW CG_VoiceChatLocal signature lineage). All three
//                           mode blocks read it at [ESP+0x17c] one push deeper than
//                           the [ESP+0x17c] voiceChatString read at 0x3003a30a —
//                           same displacement, different ESP depth = arg4, not arg5.
//     voiceChatString arg5  the sound-alias token (picked + copied to msg.token)
//
// Machine-code facts preserved:
//   * clientNum clamp (0x3003a263..0x3003a279): if clientNum < 0 OR >= 0x40, use 0.
//   * ESI = clientNum * 0x4d0; the anim-state element is bgs.clientinfo[clientNum]
//     (base 0x305e1f34). Early-out to the epilogue when element.infoValid
//     ([ESI+0x305e1f34], +0x0) is 0 (0x3003a281..0x3003a289).
//   * alias-table select (0x3003a292..0x3003a2a2): default axis table
//     (cg_voiceChatTables[0] @ 0x3016a9e0); if infoValid != 0 AND element.team
//     ([ESI+0x305e1f60], +0x2c) == 1 keep axis, otherwise the allies table
//     (cg_voiceChatTables[1] @ 0x301b3b28). (The redundant TEST EAX,EAX / JZ that guards
//     the team compare is a compiler artifact of the shared infoValid test; the
//     JZ can never fire here because we already returned when infoValid==0.)
//   * CG_PickSoundAlias(table, voiceChatString, &sndA, &sndB, &pickedText) — EBX carries
//     the table base; the 3 outputs land in local ints. On qfalse (no matching alias)
//     jump to the epilogue (0x3003a2c6..0x3003a2c8).
//   * non-team gate (0x3003a2d5..0x3003a2e1): if mode != 1 (not a team message) AND
//     cg_teamChatsOnly_vmCvar.integer (0x3052edac) != 0, skip display (jump to epilogue).
//   * assemble the cgVoiceChatMsg_t (built on the stack, then relocated to the global
//     scratch at 0x303b3420 == &cg_voiceChatTables[8] via the MOVSD.REP of 0x52 dwords):
//       msg.soundName = sndA (0x3003a2e7/0x3003a2f6)  [dword; see type note below]
//       msg.icon      = sndB (0x3003a2eb/0x3003a2fc)
//       msg.voiceOnly = voiceOnly arg (0x3003a2ef/0x3003a31d)
//       msg.clientNum = clientNum (0x3003a32a)
//       Q_strncpyz(msg.token, voiceChatString, 0x96) (0x3003a311..0x3003a343:
//       strncpy(token, src, 0x95) then token[0x95]=0 at +0xa5 — exactly the
//       Q_strncpyz(dst, src, 0x96) expansion)
//       msg.spriteOrigin = { msg->origin[0], [1], [2] } from ECX
//         (0x3003a2fa/0x3003a300/0x3003a303/0x3003a316/0x3003a321/0x3003a32e)
//   * translated text: cfgHint = CG_GetTranslatedLocationString(element.location
//     ([ESI+0x305e1f6c], +0x38)) (0x3003a33a/0x3003a34b); translated =
//     CG_GetTranslatedVoiceChatString(pickedText) (0x3003a352 loads [ESP+0x14] at
//     depth ESP0-0x168 = ESP0-0x154 — the pick call's FOURTH out slot, the display
//     text, NOT the -0x150 soundName slot).
//   * Com_sprintf(msg.text, 0x96, fmt, element.name, "^3", cfgHint,
//     Q_COLOR_ESCAPE, color, translated) (0x3003a35b..0x3003a3ca), where fmt
//     is one of three .rdata strings chosen by mode:
//       mode==2 : "[%s]%s[%s]: %c%c%s"   (0x3007a158)
//       mode==1 : "(%s)%s(%s): %c%c%s"   (0x3007a144)
//       else    : "%s %s(%s): %c%c%s"    (0x3007a130)
//     and "^3" == 0x3007a16c, Q_COLOR_ESCAPE == 0x5e == '^'.
//   * CG_PlayVoiceChat(&cgVoiceChatScratch) (0x3003a3e7).
//
// (An earlier draft claimed the second %c consumed the voiceChatString pointer and
// flagged it as an apparent source bug. That was an ESP-depth misread: the operand
// is arg4 `color`. There is no source bug here.)
//
// TYPE NOTE: cgVoiceChatMsg_t.soundName is modeled as const char* (its CG_PlayVoiceChat
// contract), but CG_PickSoundAlias returns int32 sound handles; the DLL stores the raw
// dword either way, so the pick output is written through soundName as a resolved sound
// reference. Not re-typing another function's struct here.
//
// The /GS stack cookie (0x30081650 snapshot at 0x3003a256, checked via
// __security_check_cookie 0x30061639 at the RET) is a compiler artifact, not modeled.

#include "client/cgame/globals.h"
#include "client/cgame/client_recovered.h"

/* 0x3007a16c — the "^3" (yellow) color-code prefix pushed as the second "%s". */
static const char voiceChatYellowColorPrefix[] = "^3";
/* The three per-mode chat-line format strings (.rdata @0x3007a158/44/30). */
static const char teamVoiceChatFormat[]  = "[%s]%s[%s]: %c%c%s";
static const char groupVoiceChatFormat[] = "(%s)%s(%s): %c%c%s";
static const char openVoiceChatFormat[]  = "%s %s(%s): %c%c%s";

enum { CG_VOICECHAT_COLOR_ESCAPE = 0x5e /* '^' */ };

void CG_VoiceChat(cgVoiceChatMessage_t *msg, int32_t mode, int32_t voiceOnly,
                  int32_t clientNum, int32_t color, const char *voiceChatString)
{
    clientInfo_t *element;
    cgVoiceChatTable_t *aliasTable;
    const char *sndA;
    qhandle_t sndB;
    const char *pickedText;
    cgVoiceChatMsg_t out;
    const char *cfgHint;
    const char *translated;
    const char *fmt;

    /* 0x3003a263..0x3003a279: clamp the speaker clientNum to [0,64). */
    if (clientNum < 0 || clientNum >= MAX_CLIENTS_IN_SNAPSHOT) {
        clientNum = 0;
    }

    /* 0x3003a27b..0x3003a289: index the per-client anim-state table; the slot must be
     * live (infoValid != 0) or there is nothing to display. */
    element = &bgs.clientinfo[clientNum];
    if (element->infoValid == 0) {
        return;
    }

    /* 0x3003a292..0x3003a2a2: pick the axis table for team 1, the allies table
     * otherwise. */
    if (element->team == 1) {
        aliasTable = &cg_voiceChatTables[0];
    } else {
        aliasTable = &cg_voiceChatTables[1];
    }

    /* 0x3003a2a7..0x3003a2c8: resolve the token to a sound + head-icon handle. */
    if (!CG_PickSoundAlias(aliasTable, voiceChatString, &sndA, &sndB, &pickedText)) {
        return;
    }

    /* 0x3003a2d5..0x3003a2e1: non-team (mode != 1) messages are suppressed when the
     * cg_teamChatsOnly_vmCvar.integer gate is set. */
    if (mode != 1 && cg_teamChatsOnly_vmCvar.integer != 0) {
        return;
    }

    /* 0x3003a2e7..0x3003a32e: assemble the CG_PlayVoiceChat descriptor. */
    out.soundName = sndA;
    out.icon = sndB;
    out.voiceOnly = voiceOnly;
    out.clientNum = clientNum;

    /* 0x3003a311..0x3003a343: copy the token into the descriptor's token slot
     * (+0x10): strncpy(token, src, 0x95) then token[0x95] = 0 at +0xa5 — the
     * exact Q_strncpyz(dst, src, sizeof(token)) expansion for the 0x96 slot. */
    Q_strncpyz(out.token, voiceChatString, sizeof(out.token));

    /* 0x3003a2fa/0x3003a300/0x3003a303/0x3003a316/0x3003a321/0x3003a32e: origin copy. */
    out.spriteOrigin[0] = msg->origin[0];
    out.spriteOrigin[1] = msg->origin[1];
    out.spriteOrigin[2] = msg->origin[2];

    /* 0x3003a33a..0x3003a356: the HUD-hint location string and the localized
     * display text. 0x3003a352 loads the pick call's FOURTH out slot (the display
     * text), not the soundName slot. */
    cfgHint = CG_GetTranslatedLocationString(element->location);
    translated = CG_GetTranslatedVoiceChatString(pickedText);

    /* 0x3003a35b..0x3003a3b9: select the per-mode format string. */
    if (mode == 2) {
        fmt = teamVoiceChatFormat;
    } else if (mode == 1) {
        fmt = groupVoiceChatFormat;
    } else {
        fmt = openVoiceChatFormat;
    }

    /* 0x3003a3be..0x3003a3ca: format the display line into the descriptor's text.
     * The "%c%c" pair prints '^' then (char)color (arg4, read at 0x3003a361/
     * 0x3003a384/0x3003a3a2 — one push deeper than the 0x3003a30a voiceChatString
     * read with the same 0x17c displacement). */
    Com_sprintf(out.text, sizeof(out.text), fmt,
               element->name,
               voiceChatYellowColorPrefix,
               cfgHint,
               CG_VOICECHAT_COLOR_ESCAPE,
               color,
               translated);

    /* 0x3003a3cf..0x3003a3e7: relocate the descriptor to the global scratch and play. */
    cgVoiceChatScratch = out;
    CG_PlayVoiceChat(&cgVoiceChatScratch);
}
