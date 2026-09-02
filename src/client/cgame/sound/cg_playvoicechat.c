// Source: uo_cgame_mp_x86.dll 0x30039ff0..0x3003a12c
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30039ff0_3003a12c.mcode
//
// CG_PlayVoiceChat — handle a received voice-chat message.
//
// NAME ADJUDICATION: the .mcode header's mechanical size-guess `Item_EnableShowViaCvar`
// is REJECTED. This function never touches an itemDef_t, a cvar-value string, or an
// enable/disable list. Its proven behavior is the voice-chat notify path:
//   - play the voice sound on the local sound channel (CG_PlaySoundAliasByName),
//   - stamp the speaker's talking head-icon (cgs_voiceChatIcon, "headiconVoiceChat")
//     and a cg.time display deadline — into the local cg_localVoiceChat* globals when
//     the speaker is the local client, otherwise into cg_entities[speaker],
//   - append the chat text to the team-chat buffer and print it (unless voice-only
//     or cg_noVoiceText is set).
// That matches the classic Quake3/CoD CG_PlayVoiceChat role.
//
// ABI: the message pointer arrives in EAX (register arg); the function has no stack
// arguments and returns with a bare RET (only ESI is saved/restored).
//
// Machine-code facts preserved:
//   * gate on cg_noVoiceChats_vmCvar.integer (0x3045818c): JNZ skips the whole sound/icon
//     block (nonzero => no voice sound and no icon).
//   * CG_PlaySoundAliasByName(this=&cg_snap->ps.psOrigin (LEA [cg_snap+0x20]),
//     soundName=msg->soundName (ESI+0x4), entityNum=cg_snap->ps.psClientNum (cg_snap+0xe0));
//     the single stack arg (entityNum) is caller-cleaned (ADD ESP,0x4).
//   * speaker test: msg->clientNum (ESI+0x0) == cg_snap->ps.psClientNum (cg_snap+0xe0).
//   * cg_entities[] is the 0x288-stride client-entity array at base 0x3048c6e0,
//     addressed by clientNum*0x288 (IMUL ...,0x288); voiceChatIcon @+0x230
//     (0x3048c910), voiceChatTime @+0x234 (0x3048c914), lerpOrigin vec3 @+0x208..
//     (0x3048c8e8/0x8ec/0x8f0).
//   * display deadline: cg.time (0x304831b0) + duration (0x30450dac) when the icon is
//     cgs_voiceChatIcon (ADD), else cg.time + 2*duration (LEA [t + d*2]).
//   * text path gate: msg->voiceOnly (ESI+0xc) == 0 AND cg_noVoiceText_vmCvar.integer
//     (0x304505cc) == 0.
//   * CG_AddToTeamChat(msg->text) takes its arg in ECX (MOV ECX,ESI after ADD
//     ESI,0xa6); then Com_PrintMessage(va(": %s\n", msg->text)); the two va() args and
//     the one Com_PrintMessage arg are cleaned together (ADD ESP,0xc).

#include "client/cgame/globals.h"
#include "client/cgame/client_recovered.h"

void CG_PlayVoiceChat(cgVoiceChatMsg_t *msg)
{
    /* 0x30039ff0..0x30039ffa: gate on cg_noVoiceChats. */
    if (cg_noVoiceChats_vmCvar.integer == 0) {
        /* 0x3003a000..0x3003a015: play the voice sound on the local channel. */
        CG_PlaySoundAliasByName(cg_snap->ps.psClientNum, &cg_snap->ps.psOrigin, msg->soundName);

        /* 0x3003a017..0x3003a02a: is the speaker the local player? */
        if (msg->clientNum == cg_snap->ps.psClientNum) {
            /* 0x3003a02c..0x3003a072: local player — stamp the cg.* voice-chat state. */
            cg_predictedEventEntity.voiceChatIcon = msg->icon;
            if ((uint32_t)msg->icon == cgs_voiceChatIcon) {
                cg_predictedEventEntity.voiceChatTime =
                    coduo_int32_from_bits((uint32_t)cg_time + (uint32_t)cg_voiceSpriteTime_vmCvar.integer);
            } else {
                cg_predictedEventEntity.voiceChatTime =
                    coduo_int32_from_bits((uint32_t)cg_time + 2u * (uint32_t)cg_voiceSpriteTime_vmCvar.integer);
            }
        } else {
            /* 0x3003a074..0x3003a0f9: remote speaker — stamp cg_entities[speaker]. */
            centity_t *cent = cg_entities + msg->clientNum;

            cent->voiceChatIcon = msg->icon;
            cent->lerpOrigin[0] = msg->spriteOrigin[0];
            cent->lerpOrigin[1] = msg->spriteOrigin[1];
            cent->lerpOrigin[2] = msg->spriteOrigin[2];

            if ((uint32_t)msg->icon == cgs_voiceChatIcon) {
                cent->voiceChatTime = coduo_int32_from_bits((uint32_t)cg_time + (uint32_t)cg_voiceSpriteTime_vmCvar.integer);
            } else {
                cent->voiceChatTime = coduo_int32_from_bits((uint32_t)cg_time + 2u * (uint32_t)cg_voiceSpriteTime_vmCvar.integer);
            }
        }
    }

    /* 0x3003a0f9..0x3003a127: unless voice-only or cg_noVoiceText, show the text. */
    if (msg->voiceOnly == 0 && cg_noVoiceText_vmCvar.integer == 0) {
        CG_AddToTeamChat(msg->text);
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
        Com_PrintMessage("%s", va(": %s\n", msg->text));
    }
    /* 0x3003a12a..0x3003a12b: POP ESI / RET. */
}
