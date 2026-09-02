#ifndef CLIENT_RECOVERED_H
#define CLIENT_RECOVERED_H

#ifndef EMULATE_X87
#define EMULATE_X87 0
#endif

#include "compat/coduo_int32_bits.h"
#include "compat/coduo_native_x87.h"
#include "bg/bg_animation.h"
#include "bg/bg_movement.h"
#include "bg/bg_player_state.h"
#include "bg/bg_pmove.h"
#include "bg/bg_vehicle.h"
#include "bg/bg_weapon.h"
#include "qcommon/com_parse.h"
#include "qcommon/com_sprintf.h"
#include "qcommon/dobj_types.h"
#include "qcommon/info.h"
#include "qcommon/q_endian.h"
#include "qcommon/q_bits.h"
#include "math/q_math.h"
#include "client/common/client_common.h"
#include "client/math/client_math.h"
#include "qcommon/q_path.h"
#include "qcommon/q_renderer_types.h"
#include "qcommon/q_shared_types.h"
#include "qcommon/q_string.h"
#include "qcommon/sound_types.h"
#include "client/menu/ui_display_context_types.h"
#include "client/menu/ui_memory.h"
#include "client/menu/ui_menu_types.h"
#include "client/menu/ui_parse.h"
#include "client/menu/ui_runtime.h"
#include "qcommon/xmodel_types.h"
#include "platform/crt_boundary.h"
#include "globals.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

/* Cross-translation-unit declarations for partially recovered helpers. Keep
 * these centralized while the recovered source still lives in address-shaped
 * files; they can move into subsystem headers when that source is reorganized. */
typedef struct cgAlignedDrawItem cgAlignedDrawItem;

float CG_HudElemStringWidth(const char *text, const cgAlignedDrawItem *item);
/* The two lerp leaves return raw ST0 values. long double is the project's
 * register-carrier convention; their dispatchers consume the value before any
 * float store (FADD at 0x30029a06; FCOM at 0x30029a5a). */
long double CG_HudElemShaderWidth(const hudElem_t *elem, const cgAlignedDrawItem *item);
long double CG_HudElemShaderHeight(const hudElem_t *elem, const cgAlignedDrawItem *item);
long double CG_HudElemShaderDimension(int32_t value, const cgAlignedDrawItem *item);
long double CG_HudElemWidth(const hudElem_t *elem, const cgAlignedDrawItem *item);
long double CG_HudElemHeight(const hudElem_t *elem, const cgAlignedDrawItem *item);
void CG_PoissonDiskSample(vec2_t out, const vec2_t ref, float minDist);
void CG_AddLightningBeam(const centity_t *src);
void CG_Player(centity_t *cent);
void CG_AddPlayerCorpseEntity(centity_t *cent);
void CG_Missile(centity_t *cent);
void CG_AddCEntity_ET11(centity_t *cent);
qboolean CG_PlayerVehiclePositionAndBlend(centity_t *rider);
void CG_DrawScoreboard_ListBanner(float fade);
void CG_DrawScriptUsage(void);
qboolean CG_DrawFollowingMessage(void);
void CG_DrawExpiringIconGrid(void);
void CG_DrawDebugOverlays(void);
long double CG_CubicInterpolate(float t, float p0, float p1, float p2, float p3);
void CG_UpdateFlameTime(void);
int SortStringPtrsCaseInsensitive(const void *a, const void *b);
void CG_TransitionSnapshot(void);
int32_t GetEntityTypeIfModelLoaded(int32_t entityNum);
void CG_ShutdownEffectsAndHud(void);
void CG_LoadingString(const char *text);
int32_t CG_CrosshairPlayer(void);
void CG_GetEntityOriginAxis(int32_t index, vec3_t outOrigin, axis_t outAxis);

/*
 * Recovered client functions use their resolved source names; an address-shaped
 * name is kept only where the machine code has not yet proven a real one.
 */
/* KeywordHash_Key/Add/Find are shared by ui_runtime.h. */
/*
 * bg_indexed_string_t and BG_INDEXED_STRING_HASH_UNSET come from the shared BG
 * animation-type boundary included by globals.h. It is the same 8-byte i386
 * {name +0x00, hash +0x04} layout proven by BG_IndexForString (0x30001420) and
 * by the game-module implementations.
 */

/* vmCvar_t is the shared engine/module cvar mirror from q_shared_types.h, so
 * globals.h can use its complete 0x110-byte definition before this header. */

/*
 * cvarTable_t — one entry of the cgame startup cvar-registration table
 * cg_cvarTable (0x300851f0, 184 entries, stride 0x10), walked by CG_RegisterCvars.
 * The shared client stride is 0x10 (four fields); this is a real divergence from the
 * server game_cvar_table_s (which has two extra trailing fields, modificationCount
 * and trackChange, stride 0x18). The client i386 machine code is authority: it
 * reads only (handle, name, defaultString, flags) at +0/+4/+8/+0xc and advances
 * ESI by 0x10 per iteration. The complete client record is shared in
 * q_shared_types.h because the UI module uses the same layout.
 */

enum {
    CG_CVAR_TABLE_COUNT = 184
}; /* MOV EDI,0xb8 loop count in both walkers */

/*
 * cg_cvarTable — 0x300851f0 .data, the cgame startup cvar-registration table:
 * CG_CVAR_TABLE_COUNT (184) cvarTable_t entries, stride 0x10. Supersedes two
 * broken mechanical scalars that had
 * each captured only the first dword of one entry (see the note at 0x300851f0 in
 * globals.h). Two functions walk it, both proving count 184 and stride 0x10:
 * CG_RegisterCvars (0x3002b1a0) registers each entry via trap_Cvar_Register;
 * FUN_30042160 (0x30042209) issues trap(8, entry.vmCvar) per entry (handle field
 * only). Storage (the 184 handle objects and name/default strings are distinct
 * .data/.rdata objects) is a follow-up data-recovery task; declared extern so both
 * consumers type-check against the real shape. */
extern const cvarTable_t cg_cvarTable[CG_CVAR_TABLE_COUNT];

/*
 * BG_ParseConditionBits (0x30001920) — reconstructed; see
 * src/client/cgame/animation/bg_parseconditionbits.c. Parse one animation-script
 * condition clause from the shared Com parse cursor *text_pp and fold the matched
 * condition-value masks into the caller's 64-bit result bitset. It reads space- or
 * comma-separated tokens via Com_Parse, treating "AND"/"MINUS"/"NOT" as operators
 * ("NOT" is rewritten to "MINUS", MINUS inverts the next value), "none"/"none,"
 * as the sentinel (result[0] |= 1), and "," as the clause terminator; it
 * accumulates the remaining words into a space-joined key, then resolves the key
 * to a mask via the per-condition value-name table
 * (&bgAnimConditionAliases[condIndex*16] -> bgAnimConditionAliasBits) or, failing
 * that, via the caller-supplied `stringTable` used as a bit index; "default"
 * yields an all-ones mask. Inverted clauses AND-NOT the mask out of result,
 * otherwise it is OR'd in. Diagnostics "BG_ParseConditionBits: unexpected '%s'"
 * and "... unexpected end of condition" are emitted via Com_Error.
 *   text_pp     char **  the Com parse text cursor (Com_Parse advances it)
 *   stringTable bg_indexed_string_t *  fallback bit-index name table (arg1)
 *   condIndex   int      which animation condition (0..BG_ANIM_MAX_CONDITIONS-1)
 *   result      int[2]   in/out 64-bit condition mask accumulator
 * Name proven by the two "BG_ParseConditionBits:" diagnostics it references and
 * the same-module PPC bank (cgame_mp.dll). The .mcode's size-matched "Cmd_Give_f"
 * guess is REJECTED (no give/item logic; this is the anim condition parser).
 * Source: uo_cgame_mp_x86.dll 0x30001920..0x30001cc7.
 */
void BG_ParseConditionBits(char **text_pp, bg_indexed_string_t *stringTable, int condIndex, bg_condition_bits_t *result);

/*
 * CG_SafeTranslateString_Internal (0x3002d6e0) — resolve the localized text for the string
 * `reference` and return a pointer to the static result buffer cg_translatedString
 * (0x300d9888). It asks the engine via cgame_syscall(CG_SE_TRANSLATE_REFERENCE / 0x38,
 * reference); a nonzero return is the engine-owned translated string and is
 * returned directly. On a miss the behavior is gated by cl_languagewarnings_vmCvar.integer:
 *   - flag == 0: silently return a plain copy of `reference` in cg_translatedString.
 *   - flag != 0: emit a diagnostic (fatal BG_AnimParseError(7,...) when
 *     cl_languagewarningsaserrors_vmCvar.integer is set, else a Com_Printf "^3WARNING: Could not
 *     translate %s string \"%s\"\n" — %s = domain, %s = reference) and return an
 *     "^1UNLOCALIZED(^7" + reference + "^1)^7" placeholder built in cg_translatedString.
 * `domain` is used only as the first %s of the diagnostics (every call site passes
 * "cgame" in EAX; the reference token in ECX). This is the domain-tagged sibling of
 * CG_GetTranslatedVoiceChatString (0x3003a150) and CG_GetTranslatedLocationString
 * (0x300310b0): same structure and shared gating globals. Reconstructed from its
 * own .mcode (register ABI: domain=EAX, reference=ECX). The mechanical size-guess
 * name script_method_scriptbuiltin_sethintstring is rejected (this parses no script
 * args and sets no hint string — it is a string-editor localization lookup).
 */
/*
 * CG_GetTranslatedVoiceChatString (0x3003a150) — resolve the localized text for a
 * voice-chat string reference. Tries cgame_syscall(CG_SE_TRANSLATE_REFERENCE, string); on a hit
 * returns the engine string, on a miss returns either a plain copy of the input or
 * an "^1UNLOCALIZED(^7...^1)^7" placeholder (in cg_translatedVoiceChatString),
 * gated by cl_languagewarnings_vmCvar.integer / cl_languagewarningsaserrors_vmCvar.integer. Reconstructed
 * from its .mcode; name from the same-module PPC bank and the "voice chat string"
 * .rdata diagnostics. Input string arrives in EAX at the call site.
 */
const char *CG_GetTranslatedVoiceChatString(const char *string);
/*
 * Scr_FarHook (0x3004fd00) — cgame vmMain command 18. When imports is
 * non-NULL, copies the engine-supplied 102-entry (408-byte) import function-pointer
 * table into cg_scriptImports; then seeds the five script-export callbacks and
 * returns &cg_scriptExports. The exact Scr_FarHook name and callback roles come
 * from the same-module Mac symbols and the matching server/engine interface.
 */
cg_scriptExportTable_t *Scr_FarHook(const cg_scriptImportTable_t *imports);
/*
 * cg_scriptExports defaults. Their Windows bodies ignore every argument and
 * return NULL or do nothing; their signatures and semantic names are supplied
 * by the same cgame Mac/server script-export interface.
 */
scr_function_callback_t CGAME_ABI_CDECL Scr_GetFunction(const char **name, int32_t *developerOnly);
scr_method_callback_t CGAME_ABI_CDECL Scr_GetMethod(const char **name, int32_t *developerOnly);
void CGAME_ABI_CDECL Scr_SetObjectField(int32_t classNum, int32_t objectNum, int32_t fieldIndex);
void CGAME_ABI_CDECL Scr_GetObjectField(int32_t classNum, int32_t objectNum, int32_t fieldIndex);
void *CGAME_ABI_CDECL Scr_LoadRead(uint32_t size);
qboolean CG_OwnerDrawHandleKey(int32_t ownerDraw, int32_t flags, float *special, int32_t key);
int32_t CG_FeederCount(float feederID);
const char *CG_FeederItemText(float feederID, int32_t index, int32_t column, int32_t *handleOut);
int32_t CG_FeederItemImage(float feederID, int32_t index);
void CG_FlameSmokeParticle(void);
void CG_FeederSelection(float feederID, int32_t index);
void CG_RunMenuScript(char **args);
qboolean CG_OwnerDrawVisible(int32_t flags);
void CG_GetTeamColor(vec4_t color);
long double CG_Cvar_Get(const char *name);
/*
 * CG_GetTranslatedLocationString (0x300310b0) — resolve the localized display text
 * for a map location by its location index. Inlines CG_ConfigString at
 * (CS_LOCATIONS + locationIndex) to get the raw location string (or "CGAME_UNKNOWN"
 * when the config slot is empty), then tries cgame_syscall(CG_SE_TRANSLATE_REFERENCE, raw); on a
 * hit returns the engine string, on a miss returns either a plain copy of the raw
 * string or an "^1UNLOCALIZED(^7...^1)^7" placeholder in cg_translatedLocationString,
 * gated by cl_languagewarnings_vmCvar.integer / cl_languagewarningsaserrors_vmCvar.integer. Reconstructed
 * from its .mcode; name from the same-module PPC bank and the "map location string"
 * .rdata diagnostics. Location index arrives in EAX at the call site; the mechanical
 * size-guess "Menus_RemoveFromStack" is rejected. */
const char *CG_GetTranslatedLocationString(int32_t locationIndex);
/*
 * CG_CalcAdsViewOffset (0x300451a0) — compute the ADS (aim-down-sight)
 * view-relative offset for a world point. Looks up the current predicted-weapon
 * definition (bg_weaponInfos[cg.predictedPlayerState.currentWeapon]) and, only if
 * that weapon supports ADS (weaponInfo_t::adsEnabled != 0) AND the player is
 * currently zoomed in (cg.predictedPlayerState.adsFraction > 0.0f), stores
 *   cg_adsViewOffset[i] = (pos[i] - cg_refdef.vieworg[i]) * adsFraction
 * for i in {0,1,2}; otherwise it zeroes cg_adsViewOffset. `pos` arrives in ECX
 * (register-arg ABI); modeled here as a normal vec3 pointer parameter. Role name
 * from the ADS-gated projection; the mechanical size-matched
 * "script_method_scriptbuiltin_setrightarc" guess is rejected (that is a server
 * GSC script builtin taking script args, whereas this is a pure x87 view
 * projection over cgame globals with no strings, params, or calls).
 */
void CG_CalcAdsViewOffset(const vec3_t pos);
/*
 * cgFovFade_t and cg_fovFade are defined in globals.h (included above) so that the
 * global's typed storage lives with the rest of the recovered data inventory.
 *
 * CG_StartFovFade (0x3001ab50) — reconstructed (see functions/FUN_3001ab50_3001ab81.c).
 * Begins a cg_fovFade animation: startTime = ECX, durationMs = EAX, and
 * startValue = (stack arg) / 255.0f; if (startTime + durationMs) <= cg.time the
 * fade is already finished so currentValue is snapped to startValue. Nonstandard
 * convention: startTime in ECX, durationMs in EAX, the 0..255 numerator on the
 * stack (caller-cleaned). The .mcode size-guess name
 * script_func_setplayerignoreradiusdamage is rejected (this is x87 HUD/FOV fade
 * state, not a script damage builtin).
 */
void CG_StartFovFade(int32_t startTime, int32_t durationMs, int32_t numerator255);
/*
 * CG_ScreenFade (0x3001a7c0) — advance the cg_fovFade timed alpha animator by real
 * elapsed time, then draw a fullscreen black quad at the resulting alpha. Called
 * once per frame (no arguments). While the fade is still scheduled
 * (startTime + durationMs >= cg.time) and the current alpha has not yet reached the
 * target, it steps currentValue toward startValue by deltaMs/durationMs, where
 * deltaMs = trap_Milliseconds() - cg_screenFadeLastMs and the step is applied only
 * when 0 < deltaMs < 500 (a frame-hitch clamp); reaching/overshooting the target
 * snaps currentValue to startValue. Once the schedule has expired it snaps to
 * startValue immediately. When the (settled or stepped) alpha is > 0 it fills the
 * whole virtual 640x480 screen with color (0,0,0,alpha) via CG_FillRect. The
 * animator's numerator/255 anchor (CG_StartFovFade) confirms currentValue is a
 * 0..1 screen-fade alpha, not an FOV value; the struct keeps its cgFovFade name
 * because CG_CalcFov drives the same instance during the zoom/scope transition.
 * The .mcode size-guess name vectosignedangles is rejected: this function reads no
 * vector, does no atan2/FSQRT/BAMS angle math, and takes no arguments — it is a
 * timed screen-fade drawer. Provisional match to the cgame_mp.dll PPC name
 * CG_ScreenFade by behavior (timed fade fraction + fullscreen black quad), not by
 * size.
 */
void CG_ScreenFade(void);
/*
 * CG_CalcFov (0x3003ffc0) — compute the current view field of view (degrees),
 * returned as a float. Clamps the base FOV (cg_fov_vmCvar.value) to [80, 160] degrees,
 * forces 90 degrees in game mode 5 (cg_predictedPlayerState.pmType), and — when the local player's
 * entityStateFlags bit EF_IN_VEHICLE is set (a vehicle/turret view) —
 * blends the FOV toward the current weapon's adsZoomFov by the predicted ADS
 * fraction and maintains the FOV-zoom transition trackers and cg_fovFade animator.
 * Returns cg_zoomedFovConst degrees when the flags select the zoom/scope pair
 * (ENTITY_STATE_FLAG bits 0x6000), else the computed FOV. Takes no source-level
 * arguments (the caller's `push esi` is a saved register, not a parameter). The
 * .mcode size-guess name BG_ParseWeaponInfoSpecificFieldType is rejected: this
 * function parses nothing and touches no weapon-info field table — it is pure x87
 * FOV/zoom math over cgame view globals. Provisional match to the cgame_mp.dll PPC
 * name CG_CalcFov by the 80/90/160-degree FOV-clamp signature (not by size).
 */
float CG_CalcFov(void);
/*
 * CG_AddToTeamChat (0x30039390) — append a (color-coded) team-chat line to the
 * team-chat scroll ring, word-wrapping at TEAMCHAT_WIDTH and re-emitting the
 * active "^x" color code at the start of each wrapped continuation line. Takes
 * the source string in ECX (register/__fastcall-style first arg); no stack args
 * (RET without imm). Writes teamChatMsgs/teamChatMsgTimes/teamChatPos/
 * teamChatLastPos in globals.h. */
void CG_AddToTeamChat(const char *str);

/*
 * cgVoiceChatMsg_t (provisional) — the voice-chat descriptor CG_PlayVoiceChat
 * (0x30039ff0) receives by pointer (in EAX). Built on the caller's stack by the
 * voice-chat dispatcher near 0x3003a300 from a 0x148-byte template and a per-mode
 * chat-prefix format ("%s %s(%s): ..." etc). The fields below cover the complete
 * template through byte 0x147. Provisional field names are based on proven roles
 * (exact CoD source symbol unproven). */
typedef struct cgVoiceChatMsg_s {
    int32_t clientNum;        /* +0x0: speaking client's number (compared to
                                   *       cg_snap->ps.psClientNum, else used to index
                                   *       cg_entities[]). */
    const char *soundName;        /* +0x4: sound alias to play (CG_PlaySoundAliasByName). */
    int32_t icon;             /* +0x8: head-icon shader handle to display for the
                                   *       speaker (compared to cgs_voiceChatIcon). */
    int32_t voiceOnly;        /* +0xc: when nonzero, suppress the printed/team-chat
                                   *       text (voice-only message). */
    char token[150]; /* +0x10: the raw voice-chat token, Q_strncpyz'd in
                                   *       (cap 0x95) by CG_VoiceChat (0x3003a250) with a
                                   *       forced NUL at +0xa5. An intermediate field
                                   *       CG_PlayVoiceChat does not read; superseded the
                                   *       former abiGap_010[] filler. */
    char text[150]; /* +0xa6: the chat text line; passed to
                                   *       CG_AddToTeamChat and printed as ": %s\n". */
    vec3_t spriteOrigin;     /* +0x13c: three dwords copied into
                                   *       cg_entities[clientNum].lerpOrigin
                                   *       (+0x208..+0x210) for a remote speaker. */
} cgVoiceChatMsg_t;
/* cgVoiceChatMsg_t contains a pointer (soundName), so these offset guards are only
 * meaningful at the 32-bit target pointer width the DLL actually uses. */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
_Static_assert(offsetof(cgVoiceChatMsg_t, soundName) == 0x4, "cgVoiceChatMsg soundName +0x4");
_Static_assert(offsetof(cgVoiceChatMsg_t, icon) == 0x8, "cgVoiceChatMsg icon +0x8");
_Static_assert(offsetof(cgVoiceChatMsg_t, voiceOnly) == 0xc, "cgVoiceChatMsg voiceOnly +0xc");
_Static_assert(offsetof(cgVoiceChatMsg_t, token) == 0x10, "cgVoiceChatMsg token +0x10");
_Static_assert(offsetof(cgVoiceChatMsg_t, text) == 0xa6, "cgVoiceChatMsg text +0xa6");
_Static_assert(offsetof(cgVoiceChatMsg_t, spriteOrigin) == 0x13c, "cgVoiceChatMsg spriteOrigin +0x13c");
_Static_assert(sizeof(cgVoiceChatMsg_t) == 0x148, "cgVoiceChatMsg size 0x148");
#endif

/* 0x303b3420 — first entry of the 32-element buffered voice-chat array.
 * CG_VoiceChat assembles entry zero and CG_PlayVoiceChat consumes it. */
enum {
    CG_VOICE_CHAT_BUFFER_COUNT = 32
};
extern cgVoiceChatMsg_t cg_voiceChatBuffer[CG_VOICE_CHAT_BUFFER_COUNT];
#define cgVoiceChatScratch (cg_voiceChatBuffer[0])

/*
 * CG_PlayVoiceChat (0x30039ff0) — react to a received voice-chat message: unless
 * cg_noVoiceChats is set, start the voice sound on the local sound channel and stamp
 * the speaker's talking head-icon plus a cg.time display deadline (into the local
 * cg_localVoiceChat* globals when the speaker is the local client, otherwise into
 * cg_entities[speaker].voiceChatIcon/voiceChatTime and copying msg->spriteOrigin into
 * that entity's lerpOrigin). Then, for a non-voiceOnly message and unless
 * cg_noVoiceText is set, append the text to the team-chat buffer and print it.
 * Takes the message pointer in EAX (register arg); no stack args (RET without imm).
 * Name adjudicated from behavior/call-graph (voice sound + head-icon + team-chat +
 * ": %s\n" print); the mechanical size-guess `Item_EnableShowViaCvar` is REJECTED —
 * there is no itemDef/cvar-list access, this is the voice-chat player-notify path. */
void CG_PlayVoiceChat(cgVoiceChatMsg_t *msg);

/*
 * CG_CalcViewLeanKickOffset (0x30045230) — build the per-frame first-person
 * view/weapon positional offset into the caller-supplied scratch vec3 `out`
 * (passed in EAX -> EDI; the sole caller FUN_30046570 zeroes a stack vec3 then
 * passes its address). Zeroes `out`, then accumulates: (1) a procedural lean sway
 * — an envelope (2-|leanFraction|)*leanFraction of the interpolated player lean
 * fraction (0x30483208), aimed by an FSINCOS forward vector and scaled by
 * (1-adsFraction)*1.6, evaluated only when leanFraction != 0 and adsFraction < 1;
 * (2) the current weapon-movement view angles via CG_ApplyWeaponMovementAngles plus a fixed per-frame
 * Y/Z view-kick nudge (0x30487af0/af4); the point is then rotated into world space
 * by CG_SpinEffectPointToWorld; (3) a recoil Z bump from the last impact-event kick
 * (0x304879e0) — rising over 150 ms then decaying over the next 300 ms since its
 * cg.time stamp (0x304879e4). Finally CG_CalcAdsViewOffset projects the finished
 * point to cg_adsViewOffset. Register-arg ABI (out in EAX/EDI, plain RET); modeled
 * as a normal vec3 out-parameter. Name is provisional-by-role; the mechanical
 * size-match "Info_SetValueForKey_Big" is REJECTED (no info-string/key-value work
 * whatsoever — this is dense x87 view math). See FUN_30045230_3004547e.c.
 */
void CG_CalcViewLeanKickOffset(vec3_t out);

/*
 * CG_CalcWeaponMovementAngles (0x30044ce0) — compute the current weapon-movement view
 * angle offset and accumulate it (three FADD/FSTP, pitch/yaw/roll) into the
 * caller-supplied vec3. It derives a target from sprint/stance, movement speed,
 * and the current weapon's movement/offset fields, smooths the persistent vec3
 * at 0x30487968 toward that target, and fades it during the first half of ADS.
 * Called only by CG_ApplyWeaponMovementAngles (0x30045070). The i386 build passes the output
 * vec3 pointer in ESI (register-arg ABI); here it is modeled as a normal
 * pointer parameter. Preserves EDI. Provisional caller-observed ABI — supersede
 * with this callee's own .mcode reconstruction; exact source name unresolved
 * (the mechanical "SetClientViewAngle" size-match is rejected as size-based).
 */
void CG_CalcWeaponMovementAngles(vec3_t angles);
/*
 * CG_ApplyWeaponMovementAngles (0x30045070) — compute this frame's weapon-movement view-angle offset
 * (via CG_CalcWeaponMovementAngles), publish it to cg_weaponMovementAngles, and add it
 * componentwise into the caller's view-angle accumulator `angles` (in/out, passed
 * in EDI; register-arg ABI). See FUN_30045070_300450d3.c. Mechanical "Com_DPrintf"
 * size-match rejected.
 */
void CG_ApplyWeaponMovementAngles(vec3_t angles);
/*
 * CG_SpinEffectPointToWorld (0x300450e0) — transform one point from the spinning
 * view-effect's local axis frame (AngleVectors of the spin-angle triple at
 * 0x30487ac8) into world space anchored at cg_refdef.vieworg, overwriting `point`
 * in place: point = viewOrigin + px*forward - py*right + pz*up. Single stack arg
 * (vec3_t *; caller cleans). See FUN_300450e0_3004519c.c. Mechanical
 * "Item_ListBox_MaxScroll" size-match rejected.
 */
void CG_SpinEffectPointToWorld(vec3_t point);
/*
 * CG_PerturbCamera (0x30046490) — recovered. Slides cg_refdef.vieworg along
 * cg_refdef.viewaxis[0] by `dist` (single float stack arg; the lone caller
 * 0x300468e2 passes -19.0f, a third-person pull-back), then recomputes
 * cg_refdefViewAngles[0] as the Q3 vectoangles pitch of the forward axis built
 * by AngleVectors from the spin-angle triple at 0x30487ac8. `dist` is the sole
 * arg (caller cleans it, plain RET). Name is the corpus label kept for its
 * behavioral fit, NOT the rejected size match; see the .c for the derivation.
 */
void CG_PerturbCamera(float dist);
void CG_TrackAdsZoomDirection(void);
void CG_AddViewWeapon(void);
void CG_AddMarks(void);
void CG_FireFlameChunks(void);
void CG_UpdateShellShockCamera(void);
void CG_DrawSkyBoxPortal(void);
void CG_DrawActive(int32_t stereoView);
/*
 * CG_DebugBox (0x3001d970) — draw the 12 edges of the axis-aligned box spanning
 * [mins, maxs] as debug lines in the given color. Builds the 8 box corners into
 * a local array (each corner picks its X/Y/Z from mins or maxs by the low three
 * bits of the corner index), then issues one CG_ADD_DEBUG_LINE trap per edge
 * from the cg_debugBoxEdges[12][2] index table. The i386 build passes all four
 * arguments in registers (mins=EAX, maxs=EDX, color=EBX, param=EDI) and forwards
 * color/param unchanged to each trap; modeled here as normal parameters. Role
 * name from the same-module PPC bank (cgame_mp!CG_DebugBox); the mechanical
 * "YawToAxis" size guess is rejected (it takes (float,float*) and does not build
 * box corners).
 */
void CG_DebugBox(const vec3_t mins, const vec3_t maxs, const float color[4], int param);
/*
 * CG_DebugCircle (0x3001d9f0) — draw a full 16-segment debug circle of `radius`
 * around `center`, oriented in the plane PERPENDICULAR to `dir`. Builds an
 * orthonormal frame (forward = normalize(dir); right = PerpendicularVector;
 * up = forward x right), places 16 points at angle i*(PI/8) (i=0..15) as
 * center + radius*(cos*right + sin*up), then connects them into a closed 16-gon
 * with CG_ADD_DEBUG_LINE (trap 202) in `color`, forwarding `param` and `flag`.
 * Sits between CG_DebugBox and CG_DebugCircleEx in the debug-draw cluster; name
 * role-derived from that cluster + behavior (exact spelling not binary-proven).
 * The size-guess "BG_CalculateView_DamageKick" is rejected (size match only; no
 * view/weapon-movement globals or math — pure frame-build + debug-line traps).
 * i386 register-arg ABI: dir=EAX, center=EBX (forwarded unchanged); radius,
 * color, param, flag arrive on the stack. Modeled as leading register params,
 * as with CG_DebugBox.
 */
void CG_DebugCircle(const vec3_t dir, const vec3_t center, float radius, const float color[4], int param, int flag);
/*
 * CG_DebugCircleEx (0x3001db70) — draw a debug circle/arc around `center` as a
 * 16-point sin/cos ring of the given `radius`, swept from `startAngle` to
 * `endAngle`, connected by 15 CG_ADD_DEBUG_LINE (trap 202) segments in `color`.
 * Normalizes the sweep so the step is non-negative (folds startAngle by 360 when
 * endAngle < startAngle). Sits right after CG_DebugBox in the debug-draw cluster;
 * name role-derived from the same-module PPC bank (cgame_mp!CG_DebugCircleEx /
 * CG_DebugArc neighbors) + the sin/cos ring behavior. The mechanical size-guess
 * "script_func_radiusdamage" is rejected (size match to a server script builtin;
 * this does no damage). Exact spelling (Circle/CircleEx/Arc) not binary-proven.
 * i386 register-arg ABI: center=EAX, flag=EBX (forwarded unchanged as the last
 * trap arg); radius/startAngle/endAngle/color/param arrive on the stack. Modeled
 * as leading register parameters, as with CG_DebugBox.
 */
void CG_DebugCircleEx(const vec3_t center, int flag, float radius, float startAngle, float endAngle, const float color[4], int param);
char *CG_SafeTranslateString_Internal(const char *domain, const char *reference);
const char *CG_SafeTranslateString(const char *reference);
/*
 * Three effect/HUD-subsystem "free all" helpers called by the shutdown-reset path
 * CG_ShutdownEffectsAndHud (0x3002e390). Each releases a contiguous range of
 * registered engine handles via cgame trap 0xa8 (release) and clears the parallel
 * registration table at 0x30487af8 (keys) / 0x30488af4 (values):
 *   - CG_FreeWeaponDObjHandles (0x30044a80): for each registered weapon index
 *     1..bg_numWeapons (0x30134cd4, NOT a HUD-element count — proven
 *     bg_numWeapons corpus-wide), releases the DObj/model registration keyed
 *     by (weaponIndex + 0x400), i.e. the per-weapon handle band based at 0x400.
 *     Nulls no table (pure release loop). Reconstructed in
 *     FUN_30044a80_30044ab3.c.
 *   - CG_FreeRegisteredHandlesLow (0x300163d0): frees table indices 0..63 and
 *     nulls both arrays for each.
 *   - CG_FreeRegisteredHandlesHigh (0x30016470): frees table indices 64..1022 and
 *     nulls both arrays for each.
 * The two lower helpers are provisional caller-observed decls (all void(void), no
 * stack args) — supersede with each callee's own .mcode reconstruction; role
 * names, exact source names unresolved. */
void CG_FreeWeaponDObjHandles(void);
void CG_FreeRegisteredHandlesLow(void);
void CG_FreeRegisteredHandlesHigh(void);
/*
 * CG_StartWeaponAnim (0x30042ac0) — emit CG_XANIM_SET_GOAL_WEIGHT once for each
 * of the 21 ordinary weapon XAnim nodes, setting `activeAnimIndex` to weight
 * 1.0f/notifyName 1/restart 1 and resetting the rest to zero. Register
 * ABI proven from the call sites at 0x30042e33/0x30042e8d: weaponIndex in EAX,
 * animTree in EBX, activeAnimIndex the single cdecl stack arg. The macOS/PPC body
 * named CG_StartWeaponAnim has the same 0x1c4 record stride, 1..21 loop, active
 * index comparison, animRates[] loads, and paired trap_XAnimSetGoalWeight calls. */
void CG_StartWeaponAnim(int32_t weaponIndex, intptr_t animTree, int32_t activeAnimIndex);
/* CG_StopAllWeaponAnims (0x30042a30) — the fixed-IDLE variant of
 * CG_StartWeaponAnim. The macOS/PPC body carrying this exact symbol has the same
 * 0x1c4 record stride, loop, animRates[] loads, and hardcoded index-1 compare.
 * Called by CG_MapRestart. */
void CG_StopAllWeaponAnims(int32_t weaponIndex, intptr_t animTree);
/*
 * CG_PlayADSAnim (0x30042b50) — toggle XAnim nodes ADS_UP/ADS_DOWN through
 * CG_XANIM_SET_GOAL_WEIGHT, then set their complementary times to adsFraction
 * and 1.0f-adsFraction. The macOS/PPC function carrying this exact symbol has
 * the same two goal-weight calls per branch and the same two XAnimSetTime calls.
 * Windows register ABI: activeAnimIndex in EAX and animTree in ESI. */
void CG_PlayADSAnim(int32_t activeAnimIndex, intptr_t animTree);
/*
 * CG_WeaponRunXModelAnims (0x30042d30) — drive a weapon's first-person XModel
 * overlay animation state for one frame. Given the local player state `ps` and the
 * weapon's cgWeaponInfo record `wi`, it commits the weapon's overlay DObj (trap
 * 0xb5), maps ps->weaponAnim (an ANIM_TOGGLEBIT-tagged anim-state enum, masked with
 * ~0x200) through the jump-table switch and activates the mapped XAnim node via
 * CG_StartWeaponAnim, and, for weapons with ADS enabled, cross-fades the ADS
 * overlay via CG_PlayADSAnim. It early-outs when the anim state is
 * unchanged since last frame (wi->lastRunAnim), and latches wi->lastRunAnim after
 * running. The anim-state value 0 (idle) instead scans all 21 XModel anims via
 * CG_XANIM_HAS_FINISHED and defers (lastRunAnim=-1) until they have all finished. Unknown
 * states log via Com_Printf. Register ABI proven from the sole caller
 * (BG_PlayerStateToEntityState path, 0x30046641): ps in EDI, &cg_weaponInfos[weapon]
 * the single cdecl stack arg (plain RET, caller-cleaned). The mechanical size-guess
 * "CG_ResetEntity" is rejected: the format string at 0x3007abc4 is
 * "CG_WeaponRunXModelAnims: Unknown weapon animation %i\n", naming the function.
 * Reconstructed in FUN_30042d30_30042f66.c. */
void CG_WeaponRunXModelAnims(playerState_t *ps, cgWeaponInfo_t *wi);
/* CG_ResetWeaponAnimTrees (0x30042fc0) — exact macOS/PPC symbol and matching
 * weapon loop, DObj tree reset, clip lookup, and CG_StartWeaponAnim call. */
void CG_ResetWeaponAnimTrees(playerState_t *ps);
/*
 * Q_atoi (0x3005b6ce, a JMP thunk to the real body at 0x3005b646) — parse a
 * leading optionally-signed decimal integer from `string` (skips ctype-whitespace
 * via the locale table, accepts one +/-, accumulates digits eax=eax*10+d), i.e.
 * standard atoi. Portable recovered callers use coduo_crt_atoi explicitly to
 * preserve the original unchecked uint32 wraparound without retaining an alias. */
/*
 * Q_StripControl0x19 (0x3003a9f0) — remove every 0x19 (ASCII EM) byte from a
 * NUL-terminated string in place; returns void. Reconstructed in
 * functions/FUN_3003a9f0_3003aa17.c. The string pointer arrives in ESI
 * (register-passed: caller does LEA ESI,[buf] then a bare CALL). Used by
 * CG_ServerCommand (0x3003ac90) to sanitize a chat line copied via Q_strncpyz
 * before it is appended by CG_AddToTeamChat (0x30039390). The .mcode size-guess
 * "G_ModelName" is REJECTED (pure single-char string filter, no globals/traps);
 * it is NOT id's Q_CleanStr (that strips ^<digit> escapes + non-printables and
 * returns char*). Descriptive q_shared-style name by proven behavior; the exact
 * source symbol is unconfirmed. */
void Q_StripControl0x19(char *str);
/*
 * Q_StripToAlphanumeric (0x3003aa20) — filter a NUL-terminated string in place,
 * keeping only ASCII [A-Za-z0-9] and dropping every other byte (spaces,
 * punctuation, color-escape carets, control and high-bit bytes), then
 * NUL-terminate; returns void. Reconstructed in
 * functions/FUN_3003aa20_3003aa65.c. The string pointer arrives in EDI
 * (register-passed: caller does LEA EDI,[buf] then a bare CALL). Direct sibling
 * of Q_StripControl0x19 above (same read/write-index in-place compaction) but
 * the keep predicate is "is alphanumeric" (a MOVSX + signed CMP range chain, so
 * high-bit bytes are dropped). Used by CG_ServerCommand (0x3003ac90): it fetches
 * the local player "name" cvar into a 0x20 buffer via
 * trap_Cvar_VariableStringBuffer (0xb), strips it to alphanumeric, then embeds
 * the cleaned name in a demo filename ("record %s-%s") and a screenshot filename
 * ("screenshotJPEG %s-%s"). The .mcode size-guess "Cmd_NextVehSlot_f" is
 * REJECTED (pure char-class filter, no dispatch/globals/traps); it is NOT id's
 * Q_CleanStr (that keeps spaces/punctuation and returns char*). Descriptive
 * q_shared-style name by proven behavior; the exact source symbol is
 * unconfirmed. */
void Q_StripToAlphanumeric(char *str);
/*
 * Q_CleanStr (0x3004e7c0) — the real id-Tech Q_CleanStr, reconstructed in
 * functions/FUN_3004e7c0_3004e809.c. Rewrites a NUL-terminated string in place,
 * dropping (a) two-byte "^<digit>" color escapes and (b) every non-printable
 * byte, keeping only printable ASCII 0x20..0x7e; then NUL-terminates and returns
 * the (same) string pointer. The string arrives in EAX (register-passed, no
 * stack frame) and is used as both the read cursor (ESI) and the write cursor
 * (EDI); EAX is never overwritten, so the return value is the input pointer.
 * This is the routine the sibling filters Q_StripControl0x19,
 * Q_StripToAlphanumeric and CG_PointContents each explicitly
 * disclaim being. The .mcode size-guess "script_func_sin" is REJECTED (this is
 * a string filter, not a trig/math helper). */
char *Q_CleanStr(char *string);
/*
 * CG_ParseServerinfo (0x30038380) — parse the serverinfo config string
 * (config string 0) and cache its fields in the cgs serverinfo mirror. Copies
 * sv_hostname -> cgs_hostname (256) and g_gametype -> cgs_gametype (32) via
 * Info_ValueForKey + Q_strncpyz, forces the g_gametype cvar (trap_Cvar_Set)
 * on first init only (guarded by cgs_localServer), stores
 * atoi(sv_maxclients) in cgs_maxclients, and builds cgs_mapname as
 * "maps/mp/<mapname>.bsp" (Com_sprintf, 64). Name from same-module PPC cgame_mp
 * CG_ParseServerinfo and the proven serverinfo-field behavior; the mechanical
 * size-guess PM_Weapon_AddFiringAimSpreadScale is rejected. */
void CG_ParseServerinfo(void);
/*
 * CG_RegisterCvars (0x3002b1a0) — register every cgame cvar and initialize the
 * config-values guard. Walks the 184-entry cg_cvarTable (0x300851f0) calling
 * trap_Cvar_Register(entry.vmCvar, entry.cvarName, entry.defaultString,
 * entry.cvarFlags) for each; then reads the "sv_running" cvar string into a local
 * buffer, stores Q_atoi(sv_running) into cgs_localServer (used elsewhere
 * as the once-guard, e.g. CG_ParseServerinfo forces g_gametype only on first init),
 * and forces cl_stance="0", cl_run="1", cg_objectiveText="" via trap_Cvar_Set.
 * Broad-corpus cgame name; the .mcode size-guess CG_PlaySoundAliasByName is rejected
 * (this registers cvars, it plays no sound). */
void CG_RegisterCvars(void);
/* CG_Init (0x3002df30) — top-level cgame connection/map initializer. The original
 * i386 call carries serverMessageNum in EDX and the other two arguments on the
 * stack; portable recovered C models the semantic ordered signature below. */
void CG_Init(int32_t serverMessageNum, int32_t serverCommandSequence, int32_t clientNum);
/* Caller-observed initialization helpers. CG_RegisterSounds is identified by its
 * sound-alias/media registration body and same-module PPC name. The config-string
 * helper parses/registers the initial config-string asset/client ranges; its exact
 * original spelling remains unproven until 0x30038830 is reconstructed. */
void CG_RegisterSounds(void);
enum {
    CG_MAX_VOICE_CHATS = 64,
    CG_VOICE_CHAT_TEXT = 64
};
typedef enum cgVoiceChatGender_e {
    CG_VOICE_GENDER_MALE = 0,
    CG_VOICE_GENDER_FEMALE = 1,
    CG_VOICE_GENDER_NEUTER = 2
} cgVoiceChatGender_t;
typedef struct cgVoiceChatEntry_s {
    char name[CG_VOICE_CHAT_TEXT];
    int32_t variantCount;
    const char *sounds[CG_MAX_VOICE_CHATS];
    char text[CG_MAX_VOICE_CHATS][CG_VOICE_CHAT_TEXT];
    qhandle_t icons[CG_MAX_VOICE_CHATS];
} cgVoiceChatEntry_t;
typedef struct cgVoiceChatTable_s {
    char fileName[CG_VOICE_CHAT_TEXT];
    cgVoiceChatGender_t gender;
    int32_t entryCount;
    cgVoiceChatEntry_t entries[CG_MAX_VOICE_CHATS];
} cgVoiceChatTable_t;
extern cgVoiceChatTable_t cg_voiceChatTables[CG_VOICE_CHAT_TABLE_COUNT];
#if UINTPTR_MAX == 0xFFFFFFFFu
_Static_assert(sizeof(cgVoiceChatEntry_t) == 0x1244, "voice chat entry stride 0x1244");
_Static_assert(sizeof(cgVoiceChatTable_t) == 0x49148, "voice chat table stride 0x49148");
#endif
qboolean CG_ParseVoiceChats(const char *fileName, cgVoiceChatTable_t *table, int32_t maxVoiceChats);
void CG_RegisterConfigStringAssets(void);
void CG_BuildVoteHudStrings(void);
void CG_BuildTimeoutHudStrings(void);
/*
 * CG_ConfigString3Modified (0x3002c9c0) — reload the config-string-index-3 asset
 * after a renderer/asset reset. See the C artifact for full behavior. Its callers
 * (0x3002e373, 0x30038eff, 0x30039590) invoke it right after a reset trap (0xd7)
 * as part of the shutdown-then-reregister flow. */
void CG_ConfigString3Modified(void);
void CG_ConfigStringModified(void);
void CG_ConfigString11Modified(void);

/*
 * CG_ConfigString (0x3002c990) — return the config string for a gameState index.
 * Proven from machine code: bounds-checks index into [0, 2048), then returns
 * &cg_gameState.stringData[cg_gameState.stringOffsets[index]] (table at 0x30440a00,
 * data base 0x30442a00). The index arrives on the stack ([ESP+8] after the callee
 * pushes ESI); an out-of-range index is reported via Com_ErrorMessage before the
 * (unbounded) lookup still proceeds. Provisional caller-observed decl — superseded
 * by the callee's own .mcode reconstruction. */
const char *CG_ConfigString(int index);

/*
 * CG_RegisterConfigStringShader (0x300387e0) — resolve the config string at a
 * gameState index and, when it is non-empty, (re)register it as a 2D/HUD shader
 * via cgame_syscall(CG_R_REGISTERSHADER, name, 5) after pumping the loading HUD
 * once (CG_DrawInformation(0)). The resulting qhandle_t is discarded — this is
 * a precache side effect. The index arrives in ECX (register arg); it shares
 * CG_ConfigString's inlined [0, MAX_CONFIGSTRINGS) bounds check and its
 * Com_ErrorMessage("CG_ConfigString: bad index: %i", index) diagnostic (after
 * which the lookup still proceeds). Invoked by the config-string-modified
 * dispatcher (0x300392dc) for the shader range CS_SHADERS..CS_SHADERS+
 * CS_SHADERS_COUNT. The .mcode size-guess "Scr_FreeHudElem" is rejected (frees
 * nothing; registers a shader). Named by proven behavior; exact CoD symbol
 * unproven. Proven by its own .mcode reconstruction. */
void CG_RegisterConfigStringShader(int index);

/*
 * CG_RegisterConfigStringMenu (0x30038790) — resolve the config string at a
 * gameState index and, when it is non-empty, load/register it as a script-menu
 * file via cgame_syscall(CG_R_REGISTERMENU, name); if the trap returns zero
 * (load failed) it reports Com_ErrorMessage("Could not load script menu file
 * '%s'", name). The index arrives in EAX (register arg); it shares CG_ConfigString's
 * inlined [0, MAX_CONFIGSTRINGS) bounds check and its Com_ErrorMessage(
 * "CG_ConfigString: bad index: %i", index) diagnostic (after which the lookup still
 * proceeds). Invoked by the config-string-modified dispatcher (0x300391xx) for the
 * menu-file range [0x535, 0x555). Sibling of CG_RegisterConfigStringShader and of
 * the identical registrar at 0x30038826. The .mcode size-guess "Q_stricmp" is
 * rejected (compares nothing; loads a menu). Named by proven behavior; exact CoD
 * symbol unproven. Proven by its own .mcode reconstruction. */
void CG_RegisterConfigStringMenu(int index);

/*
 * CG_SafeTranslateHudElemString (0x3002d800) — return the localized hud-element string for a
 * hud-element string index. The index arrives in EAX (register arg). A null/zero
 * index returns the shared empty-string literal g_str_empty (""). Otherwise it forms
 * cfgIndex = index + CS_LOCALIZED_STRINGS (0x575), reports an out-of-range cfgIndex via
 * Com_ErrorMessage("CG_ConfigString: bad index: %i", cfgIndex) (the shared inlined
 * CG_ConfigString bounds check), then resolves
 * &cg_gameState.stringData[cg_gameState.stringOffsets[cfgIndex]] and returns
 * CG_TranslateMessage(str, "hudelem string") — the config-string localize/text-format service
 * with the "hudelem string" diagnostic label. Sole caller is the hud-element
 * string-building dispatcher 0x30029c00 (stores the result as the element's display
 * string). The .mcode size-guess "G_InitTurrets" is rejected: this is a getter that
 * returns a default string, not turret initialization. */
const char *CG_SafeTranslateHudElemString(int index);

/*
 * CG_RegisterItems (0x30044b90) — register the visuals for every item present on
 * the current map. Reads the CS_ITEMS config string (inlined
 * &cg_gameState.stringData[cg_gameState.stringOffsets[CS_ITEMS]]) into a local
 * buffer, then walks item indices 1..133 (the fixed compare bound is 0x86 = 134):
 * the config string is a hex-packed bitfield, 4 item bits per character, so item i
 * lives in nibble (i >> 2), bit (i & 3). The hex char is decoded to 0..15
 * (char <= '9' ? char - '0' : char - ('a' - 10)) and if item i's bit is set,
 * CG_RegisterItemVisuals(i) is called. /GS-protected frame (snapshots
 * __security_cookie, verifies via __security_check_cookie on exit). The mechanical
 * size-match name "G_UpdateTagInfo" is rejected — the body is an item-visuals
 * precache loop, not a tag update. Name confirmed by call graph (matches the
 * cgame_mp CG_RegisterItems that drives CG_RegisterItemVisuals). */
void CG_RegisterItems(void);

/*
 * CG_RegisterItemVisuals (0x30044ac0) — register the models/shaders/sounds for a
 * single item, given its item index in EAX (custom register-arg ABI). Lazily
 * initialized: entry &cg_items[itemNum] (36-byte stride at 0x304531a0) is skipped
 * if its "registered" flag is already nonzero, otherwise it loads the item's models
 * and icon shader and registers its pickup sound (trap id 0xc3) from the
 * bg_itemlist[] definition (48-byte stride at 0x300827a0), then sets the flag.
 * Reconstructed (see the C artifact). Mechanical size-match name "G_DebugCircle"
 * (win/game size 0xc7==0xc7) is rejected: the body is item asset registration,
 * entirely x87-free with no sin/cos or debug-line traps. */
void CG_RegisterItemVisuals(int itemNum);

/* itemType_t, gitem_t, and the single bg_itemlist[134] storage are defined in
 * globals.h. BG_CanItemBeGrabbed is declared after entityState_t below. */

/*
 * CG_RegisterWeapon (0x300435d0) — register all render assets for one weapon index,
 * lazily (guarded by bg_weaponInfos[w].registered at +0xa8). Provisional caller-
 * observed decl: CG_RegisterItemVisuals calls it as PUSH weapon; CALL; ADD ESP,4,
 * i.e. one 32-bit weapon-index argument, caller-cleaned; the return is unused here.
 * It in turn calls back into CG_RegisterItemVisuals for the weapon's own item.
 * Superseded by its own .mcode reconstruction. */
void CG_RegisterWeapon(int weapon);

/*
 * CG_RefreshWeaponDObjModelSet (0x30044890) — RECONSTRUCTED
 * (src/client/cgame/weapons/cg_refreshweapondobjmodelset.c). Rebuild the first-person
 * view-weapon DObj model set for one already-registered weapon whose config-string
 * name changed, then re-cache that name into cg_weaponInfos[weaponIndex].name.
 * The mechanical "AxisToAngles" size-guess is REJECTED (no angle math; this formats
 * "xmodel/<gun/handModel>", registers/wraps XModels and issues the DObj model-set
 * traps 0x32/0xa7/0xa5). ABI proven from the sole caller
 * CG_RefreshWeaponInfosForConfigString (0x30044a10): ECX = weaponIndex,
 * one cdecl stack arg = the new config name; caller-cleaned. */
void CG_RefreshWeaponDObjModelSet(int weaponIndex, const char *configName);
void CG_RefreshWeaponInfosForConfigString(const char *configString);

/*
 * BG_GetMaxPickupableAmmo (0x300116b0) — how much more ammo the player can still
 * pick up for `weapon`, i.e. remaining room = ammo capacity - currently held.
 * Shared reconstruction: src/bg/bg_weapon_inventory.c.
 *
 * If the weapon has no shared-ammo pool (weaponInfo_t::sharedAmmoCapIndex < 0) it
 * returns a single weapon's remaining room: for a clip weapon (clipRequired != 0),
 * bg_ammoClipSizes[clipIndex] - ps->clips[clipIndex]; otherwise
 * bg_ammoTypeMax[ammoIndex] - ps->ammo[ammoIndex]. If it does share a pool, it
 * starts from bg_sharedAmmoCapSizes[sharedAmmoCapIndex] and subtracts, for every owned
 * weapon (ps->weaponBits) in the same pool, that weapon's held clips[] (clip
 * weapons) or ammo[] (non-clip), each ammoIndex/clipIndex counted once.
 *
 * Non-default register ABI (proven from the sole caller BG_CanItemBeGrabbed at
 * 0x30005e7c/0x30005ecd/0x30005eee): weapon index in EAX, playerState pointer in
 * EBX (inherited — the function saves only ESI/EDI, never EBX); returns the signed
 * count in EAX. The caller grabs the item only when the result is > 0. The server
 * bank name BG_GetMaxPickupableAmmo is adopted. The server's former gclient_t
 * pointer reached its first-member playerState_t; the shared contract names the
 * record actually consumed (offsets +0x134 ammo / +0x334 clips / +0x534
 * weaponBits). The .mcode size-guess "script_func_obituary" is rejected: this
 * function does no string/obituary work, it is a pure ammo-capacity query. */

/*
 * CG_SetConfigValues (0x30038430) — register the config-value cvars from the
 * gameState string table, once. Guarded by cgs_localServer: returns
 * immediately if config values are already registered. Otherwise it walks the
 * CS_CONFIGVALUE_COUNT (128) name/value config-string pairs — value config string
 * CS_CONFIGVALUE_VALUES + i, name config string CS_CONFIGVALUE_NAMES + i — and for
 * each nonempty name calls cgame_syscall(CG_CVAR_SET, name, value) (trap_Cvar_Set).
 * It stops at the first empty name. The config-string reads inline CG_ConfigString,
 * including its out-of-range Com_ErrorMessage("CG_ConfigString: bad index: %i", n).
 * Role name; exact CoD symbol unproven (mechanical guess PC_Rect_Parse rejected —
 * the body registers cvars, not a rectangle parse). */
void CG_SetConfigValues(void);

/*
 * CG_PlaySoundAliasByName (0x3002ca80) — choose and play a named sound alias at
 * a position, returning its duration in milliseconds (zero for failure/looping).
 * The semantic parameter order is proven by the named Mac function. The Windows
 * build carries soundPosition in ECX, aliasName in EAX, and entityNum on the
 * stack. It also dispatches the alias subtitle when playback returns a duration.
 * Reconstructed in src/sound/cg_playsoundaliasbyname.c. */
int CG_PlaySoundAliasByName(int32_t entityNum, const void *soundPosition, const char *aliasName);

/*
 * CG_PlayClientSoundAliasByName (0x3002ca30) — convenience wrapper that plays `sound`
 * on the LOCAL player's own snapshot sound channel. Reads cg_snap and forwards
 * CG_PlaySoundAliasByName(cg_snap->ps.psClientNum, &cg_snap->ps.psOrigin, sound). Plain
 * cdecl, one forwarded argument (callers PUSH one arg / CALL / ADD ESP,4 at
 * 0x3001a158, 0x3003adaf, 0x3003adf2, 0x3003ae6e). Reconstructed in
 * src/sound/cg_playclientsoundaliasbyname.c. Role name (behavioral); the mechanical
 * size-guess "CG_SetDObjInfo" is rejected — the body only reads cg_snap and
 * tail-forwards to the local-sound starter. */
void CG_PlayClientSoundAliasByName(const char *sound);

/*
 * CG_PlayEntitySoundAliasByName (0x3002ca50) — thin wrapper over CG_PlaySoundAliasByName that
 * plays `soundName` for client `clientNum`. It computes
 * &cg_entities[clientNum].currentStatePos.trBase (clientNum*0x288 + 0x3048c6f8,
 * where cg_entities begins at 0x3048c6e0 and currentStatePos.trBase is +0x18)
 * and forwards CG_PlaySoundAliasByName(clientNum,
 * &cg_entities[clientNum].currentStatePos.trBase, soundName).
 * Machine code: push soundName; imul clientNum,0x288;
 * add 0x3048c6f8 -> ECX; push clientNum; call. Role name
 * (behavioral); the size-guess "Use_Item" is rejected (the body only forms a
 * sound-channel address and tail-forwards to the local-sound starter).
 * Reconstructed in src/sound/cg_playentitysoundaliasbyname.c. */
void CG_PlayEntitySoundAliasByName(int clientNum, const char *soundName);
const char *trap_Com_SoundAliasString(const char *name);
snd_alias_t *trap_Com_PickSoundAlias(const char *name, const vec3_t origin);
int32_t trap_MSS_PlaySoundAlias(snd_alias_t *alias, int32_t entityNum, const void *soundPosition, int32_t timeShift);

/*
 * CG_LocalSound (0x3003ab70) — console command handler that plays a numbered
 * local sound. Reads the single console argument (Q_atoi of argv[1]) as a 1..256
 * sound index, looks up its CS_SOUNDS config string, and starts it as a local
 * sound. Prints a usage/range error via Com_PrintMessage otherwise. Proven from
 * the embedded error strings ("CG_LocalSound called with %i args (should be 2)"
 * and "...index %i (should be in range[1,%i])"); mechanical guess
 * script_func_clientannouncement is disregarded. */
void CG_LocalSound(void);

/*
 * CG_LocalSound_f (0x3003ac00) — the enable-gated console-command variant of
 * CG_LocalSound. Byte-for-byte identical to CG_LocalSound (0x3003ab70) except it is
 * prefixed by `if (!cg_announcerSounds_vmCvar.integer) return;` (MOV EAX,[0x3045884c] /
 * TEST / JZ) before doing the exact same work: trap_Argc()==2 check, trap_Argv(1,
 * g_textScratchBuffer, 1024), Q_atoi, 1..256 range check, CG_ConfigString(CS_SOUNDS
 * + index), CG_PlaySoundAliasByName(&cg_snap->ps.psOrigin, name, cg_snap->ps.psClientNum),
 * with the same two Com_PrintMessage error paths. Registered as a distinct command
 * in the cgame command dispatch table (0x3003ac90) alongside CG_LocalSound, so it is
 * a separate handler, not an alias. The mechanical size-match name
 * MenuParse_forecolor is rejected — the embedded strings prove local-sound behavior.
 * The exact CoD command name is unproven; named provisionally by role with the _f
 * command-handler suffix. */
void CG_LocalSound_f(void);

/*
 * CG_OutOfAmmoChange (0x30034a00) — per-frame check that plays the
 * "player_out_of_ammo" local warning sound when the local player has run low on
 * ammunition, tracking a persistent state so the sound fires only on the transition
 * into the low/empty state. Reads cg_snap->ps.weaponBits[0] (owned-weapon mask) and,
 * for each owned weapon index 1..bg_numWeapons, sums 1000 * cg_snap->ps.ammo[
 * bg_weaponInfos[i]->ammoIndex]; if that running total reaches 5000 it clears the
 * warning state (cg_outOfAmmoState = 0) and returns. Otherwise, if the warning state
 * was 0 it plays cg_soundOutOfAmmo via CG_PlaySoundAliasByName, then sets the state to
 * 1 (some ammo left) or 2 (no ammo). Takes no arguments; called from the per-frame
 * predicted-player-update path (0x30035014, 0x3003cbf6). Name is the same-module
 * cgame_mp.dll PPC symbol CG_OutOfAmmoChange, corroborated by the "player_out_of_ammo"
 * sound it plays; the mechanical size-guess "QuatEigenTrace" is rejected (no
 * quaternion/eigen math — this is integer ammo/weapon-bit logic and a sound call). */
void CG_CheckAmmo(void);

/*
 * CG_CheckOpenWaitingScriptMenu (0x3003a810) — reconciles the ui_waitingScriptMenu /
 * ui_newScriptMenu / ui_waitingScriptMenuIndex cvars each pass before new server
 * commands are executed (opens a pending script menu once the UI is ready). Takes no
 * arguments; plain cdecl RET. Fully reconstructed in
 * src/client/cgame/ui/cg_checkopenwaitingscriptmenu.c. Declared here because
 * CG_ExecuteNewServerCommands (0x3003b470) calls it. */
void CG_CheckOpenWaitingScriptMenu(void);

/*
 * CG_OpenScriptMenu (0x3003a5b0) — the "mr" (menu response) console-command handler
 * dispatched from CG_ServerCommand (call site 0x3003b0a6) when the server forwards a
 * request to open one of the 32 script-popup menus. Reads console argv[1] as the menu
 * index via CG_Argv+Q_atoi and validates it against [0, CS_SCRIPTMENUS_COUNT=32); an
 * out-of-range index is reported with Com_Printf("Server tried to open a bad script
 * menu index: %i\n", index) and answered with trap(CG_SEND_CONSOLE_COMMAND,
 * va("cmd mr %i bad\n", index)). Otherwise it resolves the menu-name config string at
 * index + CS_SCRIPTMENUS (sharing CG_ConfigString's inlined bounds check +
 * Com_ErrorMessage(cg_configStringBadIndexFmt, cfgIndex)); an empty (not-yet-loaded)
 * string is reported with "Server tried to open a non-loaded script menu index: %i\n"
 * and the same "cmd mr %i bad\n" answer. For a loaded menu it reads a "no-mouse"
 * flag from CG_Argc()>2 && CG_Argv(2)[0], sets ui_newScriptMenu = menu name and
 * ui_newScriptMenuIndex = index, and asks whether UIMENU_SCRIPT_POPUP[_NO_MOUSE] is
 * already open via trap(CG_UI_IS_MENU_OPEN, ...). If already open it returns; otherwise it
 * clears the new-menu cvars, and (when a different ui_waitingScriptMenu was pending)
 * emits trap(CG_SEND_CONSOLE_COMMAND, va("cmd mr %s noop\n", waitingIndex)) before
 * latching ui_waitingScriptMenu/Index/NoMouse to this request. /GS-protected frame
 * (0x404 locals; __security_cookie snapshot verified via __security_check_cookie).
 * The .mcode size-match name "BG_CalculateWeaponPosition_GunRecoil" (from the wrong
 * DLL) is REJECTED: no recoil math — this is script-menu console-command parse work
 * in the voice-chat/server-command band. Named by behavior; exact CoD symbol
 * unproven. Fully reconstructed in src/client/cgame/ui/cg_openscriptmenu.c. */
void CG_OpenScriptMenu(void);

/*
 * CG_ServerCommand (0x3003ac90) — dispatch handler for one reliable server command.
 * Fetches the command's argv (cgame_syscall(0xd/CG_ARGV-style buffer read), a movsx
 * of the first byte) and runs a large jump table over the command token. Takes no
 * arguments and returns nothing (my sole call site at 0x3003b496 does a bare
 * `call 0x3003ac90` with nothing pushed and ignores the return). PROVISIONAL,
 * caller-observed ABI only — its own .mcode (FUN_3003ac90_3003b37c) is not yet
 * reconstructed and supersedes this decl. Arity/return UNPROVEN beyond "no stack
 * args passed by this caller"; verify at each call site. */
void CG_ServerCommand(void);
void CG_ParseScores(void);
void CG_MapRestart(qboolean restart);
void CG_ReverbCmd(void);
void CG_GameMessage(const char *message);

/*
 * CG_ExecuteNewServerCommands (0x3003b470) — drain and dispatch every reliable
 * server command newer than cgs.serverCommandSequence, up to `latestSequence`.
 * See src/client/cgame/state/cg_executenewservercommands.c. */
void CG_ExecuteNewServerCommands(int32_t latestSequence);

/*
 * CG_SendFlameDamageCommand (0x300291c0) — throttled per-client flame-damage
 * notification to the server. clientNum arrives in EAX, painId in ECX (register/
 * fastcall convention). If at least 2000 ms have elapsed since
 * cg_flameInfo[clientNum].lastPainTime (signed compare against cg_time-2000),
 * it latches lastPainTime=cg_time, stores painId into painCounter, and sends the
 * reliable client command va("fdc %i", painId) via cgame_syscall(CG_SEND_CLIENT_
 * COMMAND, ...). Otherwise it returns without sending (2000ms rate limit, the same
 * throttle idiom as the "score" request in CG_DrawScoreboard). The .mcode
 * size-match pre-hint AngleNormalize360Accurate is rejected: no float/x87 work, no
 * 360.0 constant, no angle wrapping. Exact original symbol unproven (no cgame
 * syscall/symbol table recovered); named by proven role. See the C artifact at
 * src/client/cgame/effects/cg_sendflamedamagecommand.c. */
void CG_SendFlameDamageCommand(int32_t clientNum, int32_t painId);

/*
 * CG_GetShaderConfigString (0x3002fd10) — copy the shader-name config string for
 * a shader index into a caller buffer, returning qtrue on success. Proven from
 * machine code (see the C artifact): the shader index arrives in EAX and the dest
 * buffer / buffer size are the two stack args (custom register-arg ABI). It
 * validates 0 < index < 256, forms the config-string index CS_SHADERS + index,
 * and reads the string via cg_gameState.stringData[cg_gameState.stringOffsets[cfg]].
 * Returns qfalse for an empty string or one that does not fit in `size`.
 * Role name (exact CoD symbol unproven); adjudicate if a config-string-copy name
 * surfaces in the server bank. */
qboolean CG_GetShaderConfigString(int index, char *dest, int size);

/*
 * CG_GetObjectiveShaderForDir (0x3002fd90) — resolve the HUD objective/compass
 * icon shader handle for a direction slot. dir (register arg ECX, 0/1/2) selects
 * both the direction-suffix string {"","_up","_down"} and, in the no-override
 * path, the preregistered cg_objectiveShaders[dir]. shaderIndex (single stack
 * arg) is a shader config-string index: when nonzero, the name is fetched via
 * CG_GetShaderConfigString, its extension is stripped at the first '.', the
 * direction suffix is appended, and the result is registered on the fly with
 * trap(CG_R_REGISTERSHADER, name, 5); when zero (or on any lookup failure) the
 * function returns the preregistered cg_objectiveShaders[dir] instead. Returns
 * the qhandle_t. Role name from proven behavior; the .mcode size-guess
 * "G_GetHintStringIndex" is rejected (that is a server bool(int*,const char*)
 * hint-index lookup, an unrelated signature and subsystem). Provisional
 * caller-observed decl — superseded by its own .mcode reconstruction. */
qhandle_t CG_GetObjectiveShaderForDir(int dir, int shaderIndex);

/*
 * CG_DrawInformation (0x3002a530) — draw the connection/loading information
 * screen. With force==qfalse it operates only before the first snapshot and draws
 * the hunk-usage progress bar; force==qtrue also works with a snapshot, resets the
 * script-menu response state, and draws gametype/map/waiting-server text. Reentry
 * is guarded by cg_updateScreenActive. One cdecl qboolean arg, void return. Name
 * proven by behavior and the same-module PPC bank; reconstructed in
 * FUN_3002a530_3002a9d8.c. */
void CG_DrawInformation(qboolean force);

/*
 * CG_RegisterMenuAssets (0x3002dcf0) — precache the ui_shared.c cachedAssets_t
 * scrollbar/slider/gradient-bar shaders. Registers a fixed list of the
 * ui/assets .tga shaders via cgame_syscall(CG_R_REGISTERSHADER, name, 2), storing each
 * qhandle_t into the cached-asset fields of g_uiDCInstance. Calls
 * CG_DrawInformation(0) once before each registration as a loading pump. No
 * args, no return (bare RET). The .mcode size-guess "PlayerCmd_takeWeapon" is
 * rejected — the body does asset registration, not weapon/inventory work. */
void CG_RegisterMenuAssets(void);

/*
 * CG_RegisterScoreboardShaders (0x30037e90) — precache every shader the
 * multiplayer scoreboard draws: the "black"/"white" row-fill/bar shaders, the
 * hudScoreboardScroll_UpArrow/_UpKey/_DownArrow/_DownKey scroll indicators, and
 * the four team-banner shaders whose names come from the g_ScoresBanner_*
 * cvars (each cvar value is read via trap_Cvar_VariableStringBuffer then
 * registered). Every registration is preceded by a CG_DrawInformation(0)
 * loading pump; the resulting qhandle_t's are discarded (precache side effect).
 * No args, no return (bare RET). The .mcode size-guess "script_func_objective_add"
 * is rejected — that is a server script command (wrong DLL), matched only by
 * byte size. */
void CG_RegisterScoreboardShaders(void);

/*
 * CG_UpdateCompassOrientation (0x3001d6d0) — advance the compass/objective-pointer
 * reference yaw toward the animated effect spin angle via a critically-damped
 * angular spring, integrated in <=5 ms substeps. Proven from its own .mcode: takes
 * no arguments, returns nothing (bare RET). Reads the target cg_refdefViewAngles[1]
 * (0x30487acc), the time base cg.time (0x304831b0), and its own running state
 * cg_hudCompassSpringyPointers_vmCvar.integer (0x30450fec), cg_compassSpinPrevTime (0x30134ce4),
 * cg_compassRefYaw (0x3048b5d4, output) and cg_compassRefVel
 * (0x3048b5d8, spring velocity). Behavior:
 *   - First run (init flag == 0): snap cg_compassRefYaw = cg_refdefViewAngles[1], return.
 *   - Same-frame (prevTime == cg.time), or time running backwards (prevTime > cg.time),
 *     or a stall of dt > 500 ms: snap refYaw to the target, zero the velocity, return
 *     (the backwards/stall paths also resync prevTime).
 *   - Normal: dt = cg.time - prevTime; step = clamp(AngleSubtract(refYaw, target), +-10)
 *     is the signed angular error, capped so a big jump is chased at <=10 deg. Integrate
 *     in substeps of <=5 ms (dt_s = substep*0.001): if the error and velocity are both
 *     tiny (|step| < 0.5 and |vel| < 2.0) snap to target and zero velocity; otherwise
 *     predict newStep = AngleNormalize180(vel*dt_s + step), push the velocity
 *     toward closing the error by +-1500*dt_s (sign of newStep), apply damping
 *     (vel *= (1 - 3*dt_s), and an extra (1 - 5*dt_s) plus a +-2*dt_s bias term), clamp
 *     |vel| to 2000 and floor it at 0 in the closing direction. After the substeps,
 *     refYaw = AngleNormalize360(residualStep + target), i.e. requantized through 16-bit
 *     BAMS. Called once at the top of CG_DrawObjectivePointers (0x3002fe70) before the
 *     objective loop. The velocity/angle twin of CG_UpdateHudSpinAngle (0x3001d3a0).
 * The .mcode size-guess "VEH_CheckPushClients" (from game_mp_uo, wrong DLL) is rejected:
 * no vehicle/push-client work; this is pure HUD compass angle math. Exact CoD symbol
 * unproven; named by proven role. */
void CG_UpdateCompassOrientation(void);

/*
 * CG_DrawObjectivePointers (0x3002fe70) — draw the HUD compass/objective markers.
 * For the local player's snapshot (cg_snap) it iterates all 16
 * playerState.objectives[] entries and, for each whose state ==
 * OBJECTIVE_STATE_CURRENT, computes the objective's screen bearing relative to
 * the view (folding through ANGLE2SHORT/SHORT2ANGLE), a distance-based alpha fade
 * and size scale, an elevation-based up/down/level icon variant, and draws the
 * resulting icon around a compass ring via trap_R_DrawStretchPic. `rect` (register
 * arg EAX) is the compass HUD rectDef_t {x,y,w,h}; `color` (single stack arg) is the
 * base r,g,b marker color whose alpha this routine overrides per objective. The
 * .mcode header's size-matched "ClientEndFrame" guess is rejected — ClientEndFrame is
 * a server per-frame routine that issues no 2D draw traps, whereas this is pure cgame
 * HUD drawing (trap_R_SetColor / trap_R_DrawStretchPic / CG_GetObjectiveShaderForDir).
 * Register-argument ABI (rect in EAX) is recorded in the .c evidence comment. */
void CG_DrawObjectivePointers(const rectDef_t *rect, const vec3_t color);
void CG_DrawCompassFriendlies(const rectDef_t *rect, const vec4_t color);
void CG_DrawCompassTanks(const rectDef_t *rect, const vec4_t color);

/*
 * Com_ErrorMessage (0x3002b300) — the cgame fatal-error emitter: formats
 * (format, ...) into a 0x400 stack buffer via vsprintf (0x3005b538) after
 * snapshotting the vararg cookie [0x30081650], then invokes the engine CG_ERROR
 * syscall (trap id 1) through *0x30085e9c with the formatted text. Caller-cleaned
 * (format, ...) ABI. This is the inner emitter that Com_Error(level,...)
 * wraps; the name matches its existing use across the codebase. Provisional
 * caller-observed decl — superseded by its own .mcode reconstruction. */
void Com_ErrorMessage(const char *format, ...);

/*
 * CG_UpdateFadeOverlay (0x3003b7e0) — advance one timed 2D fade overlay for the
 * current frame and, while it is visible, drive it through the engine via the
 * CG_R_SAVE_SCREEN / CG_R_BLEND_SAVED_SCREEN pair. Register-arg ABI proven from both call sites
 * (0x3001c112, 0x3001c46e), which load ESI/EAX/ECX from cg_snap-derived state:
 *   overlay (ESI) — the canonical shellshock_t parameter block,
 *   startTime (EAX) — the overlay's start time (ms), and
 *   duration (ECX) — its total lifetime (ms).
 * It computes remaining = startTime - cg_time + duration; if the overlay is inactive
 * (startTime==0, duration<=0, or remaining<=0) it clears cg_fadeOverlayActive and
 * returns qfalse. Otherwise it ramps a level using screenBlendFadeTime and
 * screenBlendTime, issues CG_R_BLEND_SAVED_SCREEN(level) only when
 * the overlay was already active last frame, always issues CG_R_SAVE_SCREEN(), latches
 * cg_fadeOverlayActive = 1, and returns qtrue. Role name; the mechanical size-guess
 * ItemParse_forecolor is rejected — this parses no menu item and writes no color.
 */
qboolean CG_UpdateFadeOverlay(shellshock_t *overlay, int32_t startTime, int32_t duration);

/*
 * _vsnprintf (0x3005c1d5) — CRT-style bounded vsnprintf: formats `format`/`args`
 * into `buffer` writing at most `count` bytes, returning the character count (or
 * negative on overflow). Distinct from vsprintf (0x3005b538, no size arg):
 * this variant takes an explicit size and is what va (0x3004e8a0) and Com_sprintf
 * (0x3004e820) forward to. Proven from its machine code: it sets up a 0x42-mode
 * output descriptor bounded by `count` around `buffer`, calls the core formatter
 * (0x3005cbdb), and returns its result in EAX; callers pass the address of their
 * first vararg slot as `args`. Provisional caller-observed ABI; superseded by its
 * own .mcode reconstruction. Portable recovered callers invoke
 * coduo_crt_vsnprintf directly so this static-runtime entry does not remain an
 * unresolved source symbol or a global preprocessor alias.
 */

/*
 * CG_DrawBigString — draw `string` at screen position (x, y) using the big
 * (16px) glyph font at the given `scale`, in white. Callee at 0x3001cf10: sets
 * up a glyph-draw loop that issues cgame trap 0x36 (draw-stretch-pic) per
 * character with a 16x16 cell and a {1,1,1} white color vector. Signature is
 * (float x, float y, const char *string, float scale) per the caller's argument
 * order. Provisional caller-observed decl; superseded when 0x3001cf10 is
 * reconstructed.
 */
void CG_DrawBigString(float x, float y, const char *string, float scale);

/*
 * CG_DrawSmallString — draw `string` at screen position (x, y) using the small
 * (8px) glyph font at the given `scale`, in white. Callee at 0x3001cff0: the
 * exact sibling of CG_DrawBigString (0x3001cf10), but with an 8.0 cell scale
 * (0x41000000 vs 16.0) and glyph-set selector 5 (vs 3); both issue cgame trap
 * 0x36 per character with a {1,1,1} white color and take the same
 * (float x, float y, const char *string, float scale) argument order. CG_DrawFPS
 * (0x30018090) uses the big variant for the fps line and this small variant for
 * every detailed stats line. Provisional caller-observed decl; superseded when
 * 0x3001cff0 is reconstructed.
 */
void CG_DrawSmallString(float x, float y, const char *string, float scale);

/*
 * CG_DrawSnapshot — draw the one-line snapshot-timing debug HUD entry
 * ("time:%i snap:%i cmd:%i") starting at vertical position `y`, and return the
 * Y coordinate of the next line below it. Real out-of-line DLL function at
 * 0x30018020 (see src/client/cgame/hud/snapshot_debug.c). Takes and
 * returns a float `y` (the sole stack arg; result in ST0).
 */
float CG_DrawSnapshot(float y);

/*
 * CG_DrawFPS — the cgame frame-timing + renderer performance overlay (fps line
 * plus, when cg_drawFPS_vmCvar.integer > 1, the tris/verts/prims/ents/mb/dc lines). Real
 * out-of-line DLL function at 0x30018090 (see
 * src/client/cgame/hud/cg_drawfps.c). Takes the current vertical text
 * position `y` (sole stack arg) and returns the updated `y` (in ST0).
 */
float CG_DrawFPS(float y);

/*
 * trap_Cvar_Set — cgame syscall wrapper that sets cvar `name` to `value`.
 * Evidence: every cgame_syscall(9, ...) site across the DLL passes a cvar-name
 * pointer and a value string (e.g. ("cl_conXOffset","0"), ("cg_weaponSelect",
 * "%i"), ("cl_stance","0")). This is a real out-of-line DLL function at
 * 0x3003d570 (see src/client/cgame/module/trap_cvar_set.c) that forwards
 * both stack args to cgame_syscall(9, name, value); declared extern here so the
 * reconstructed body is the single definition and every caller converges on it.
 */
void trap_Cvar_Set(const char *name, const char *value);

/*
 * trap_Cvar_SetValue — cgame syscall wrapper (id 10) that sets the cvar bound to
 * the module-side vmCvar handle `cvar` to the string `value`. Unlike trap_Cvar_Set
 * (id 9), which addresses the cvar by NAME, this variant addresses it by the
 * already-registered vmCvar mirror. Evidence: CG_CalcVrect (0x3003f510) clamps
 * cg_viewSizeCvar.integer and re-sets it via trap(10, &cg_viewSizeCvar, "30"/"100").
 * Defined inline so it lowers to the raw trap call.
 */
static inline void trap_Cvar_SetValue(vmCvar_t *cvar, const char *value)
{
    cgame_syscall(CG_CVAR_SET_VALUE, (intptr_t)cvar, (intptr_t)value);
}

/*
 * trap_Cvar_Register — cgame syscall wrapper (id 7) that registers a cvar and binds
 * its module-side vmCvar handle. Evidence: CG_RegisterCvars (0x3002b1a0) issues
 * cgame_syscall(7, vmCvar, name, default, flags) for each cg_cvarTable entry. The
 * matching server proto is trap_Cvar_Register(void *cvar, const char *, const char
 * *, int). Defined inline so it lowers to the raw trap call.
 */
static inline void trap_Cvar_Register(vmCvar_t *vmCvar, const char *name, const char *defaultValue, int32_t flags)
{
    cgame_syscall(CG_CVAR_REGISTER, (intptr_t)vmCvar, (intptr_t)name, (intptr_t)defaultValue, flags);
}

/*
 * trap_Cvar_VariableStringBuffer — cgame syscall wrapper (id 0xb) that copies the
 * current string value of cvar `name` into `buffer` (<= size bytes). Evidence:
 * CG_RegisterCvars (0x3002b1a0) reads "sv_running" into a 0x400 buffer. The matching
 * server proto is trap_Cvar_VariableStringBuffer(const char *, char *, int).
 */
static inline void trap_Cvar_VariableStringBuffer(const char *name, char *buffer, int32_t size)
{
    cgame_syscall(CG_CVAR_VARIABLE_STRING_BUFFER, (intptr_t)name, (intptr_t)buffer, size);
}

/*
 * trap_CloseUIMenu — cgame syscall wrapper (id 125) that closes/hides the named
 * UI menu. Evidence: CG_CloseScriptMenu (0x3003a950) issues cgame_syscall(125,
 * menuName) for "UIMENU_SCRIPT_POPUP" and "UIMENU_SCRIPT_POPUP_NO_MOUSE". Named
 * by its proven role; exact engine symbol unresolved. Inline so it lowers to the
 * raw trap call. */
static inline void trap_CloseUIMenu(const char *menuName)
{
    cgame_syscall(CG_UI_CLOSE_POPUP, (intptr_t)menuName);
}

/*
 * CG_CloseScriptMenu (0x3003a950) — close the two script-popup UI menus. Body
 * evidence: it issues trap_CloseUIMenu (cgame_syscall id 125) for the fixed names
 * "UIMENU_SCRIPT_POPUP" (0x3007a014) and "UIMENU_SCRIPT_POPUP_NO_MOUSE" (0x3007a028).
 * No arguments; caller-cleaned. CG_MapRestart (0x30039500) invokes it twice on the
 * full-reinit path. Provisional caller-observed decl; superseded by its own .mcode
 * reconstruction.
 */
void CG_CloseScriptMenu(void);

/*
 * trap_UIMenuIsOpen — cgame syscall wrapper (id 124) that queries whether the named
 * UI menu is currently open/active, returning nonzero if so and zero otherwise.
 * Evidence: CG_CheckOpenWaitingScriptMenu (0x3003a810) issues cgame_syscall(124,
 * menuName) for "UIMENU_SCRIPT_POPUP"/"UIMENU_SCRIPT_POPUP_NO_MOUSE" and branches on
 * the int return. Named by its proven role; exact engine symbol unresolved. Inline so
 * it lowers to the raw trap call. */
static inline int32_t trap_UIMenuIsOpen(const char *menuName)
{
    return (int32_t)cgame_syscall(CG_UI_IS_MENU_OPEN, (intptr_t)menuName);
}

/*
 * trap_Argc — cgame syscall wrapper (id 12) that returns the current console
 * command's argument count. Signature matches the recovered server
 * trap_Argc(void) -> int (server_name_bank). Evidence: call site 0x3001759e
 * pushes only the trap id (0xc) and uses the returned EAX directly (compared
 * against the expected argument count). Defined inline so it lowers to the raw
 * trap call.
 */
static inline int32_t trap_Argc(void)
{
    return (int32_t)cgame_syscall(CG_ARGC);
}

/*
 * trType_t / trajectory_t - id-Tech / CoD trajectory (entity motion) descriptor.
 * Corroborated field-for-field by the recovered server layout trajectory_s in
 * cgame_mp/inputs/server_structs/game_mp_uo_structs.h (trType +0x00, trTime +0x04,
 * trDuration +0x08, trBase +0x0c, trDelta +0x18; 36-byte ABI). The client
 * BG_EvaluateTrajectory body (0x30005f30) proves these offsets and widths: it
 * switches on [tr+0x00], reads trTime [tr+0x04], trDuration [tr+0x08], and uses
 * trBase [tr+0x0c] / trDelta [tr+0x18] as the base and velocity vectors. The
 * canonical type is declared in shared trajectory_types.h ahead of
 * entityState_t and localEntity_t.
 */
/*
 * localEntity_t.leFlags value distinguishing the two moving-tracer parameter sets.
 * CG_AddMovingTracer (0x3002ab00) does `CMP dword [le+0xc], 0x20` (an exact
 * full-dword compare of leFlags against 32); == selects the mode-A width/length
 * pair (cg_tracerwidthlmg_vmCvar.value/cg_tracerlengthlmg_vmCvar.value), != selects the mode-B pair.
 * Exact original LEF_* symbol unresolved; named by proven role. */
#define LEF_TRACER_MODE_A ((uint32_t)0x20)

/*
 * localEntity_t.leFlags bit tested by CG_AddScaleFade (0x3002ac20): `TEST [le+0xc],0x1`
 * gates the growing-radius write (refEntity.radius = (1 - phase)*le->radius + 8). When
 * set, the handler leaves refEntity.radius untouched. Exact original LEF_* symbol
 * unresolved; named by proven role (a scale-fade sprite with a fixed radius). */
#define LEF_SCALE_FADE_NO_RADIUS ((uint32_t)0x1)

/*
 * localEntity_t leType discriminant (localEntity_t.leType, +0x8). Selected by the
 * per-frame dispatcher CG_AddLocalEntities (0x3002ad00): case 0 -> CG_AddFadeRGB
 * (0x3002abc0), case 1 -> CG_AddScaleFade (0x3002ac20), case 2 ->
 * CG_AddMovingTracer (0x3002ab00), anything else prints "Bad leType: %i".
 * The dispatcher proves exactly three valid discriminant values (0/1/2); it
 * decodes them with SUB 0/DEC/DEC so the values are dense from 0. Value 0
 * (CG_AddFadeRGB) and value 2 are behavior-derived: case 2's handler evaluates a
 * trajectory + normalized velocity and marches a stretched tracer poly, matching
 * the same-module PPC name CG_AddMovingTracer (cgame_mp!CG_AddMovingTracer, the
 * function immediately following CG_AddFadeRGB in that bank). Case 1 remains named
 * by role until its handler is reconstructed. Exact Quake3 enum name for case 1
 * (LE_MARK/LE_SCALE_FADE/...) not bound absolutely. */
enum {
    LE_FADE_RGB = 0,  /* leType handled by CG_AddFadeRGB: fade a colored refEntity */
    LE_SCALE_FADE = 1,  /* leType handled by CG_AddScaleFade (0x3002ac20): fade + growing radius */
    LE_MOVING_TRACER = 2   /* leType handled by CG_AddMovingTracer (0x3002ab00) */
};

/*
 * localEntity_t — a cgame transient render object (mark/effect/fade element) kept
 * on the cg_activeLocalEntities doubly-linked list and aged out by
 * CG_AddLocalEntities. Only the fields the recovered handlers prove are modeled;
 * the object is larger (it embeds a full refEntity_t at +0x50). Proven layout:
 * prev/next doubly-linked pointers (+0x0/+0x4), leType (+0x8), leFlags tested
 * `& 1` by the sibling handler and `== LEF_TRACER_MODE_A` by CG_AddMovingTracer
 * (+0xc), endTime (+0x10), lifeRate (+0x14, the 1/(endTime-startTime) fade rate),
 * the position trajectory `pos` (+0x18, a full trajectory_t; CG_AddMovingTracer
 * evaluates it and normalizes its trDelta at +0x30 as the tracer direction), the
 * RGBA color multipliers color[4] (+0x3c), and the embedded refEntity (+0x50).
 * Caller-observed, partial; superseded when the local-entity subsystem is fully
 * reconstructed.
 *
 * prev/next order proven by CG_AllocLocalEntity (0x3002aa70) and the CG_FreeLocalEntity
 * tail (0x3002ac20): the free-list/unlink code does `le->[+0]->[+4] = le->[+4]` and
 * `le->[+4]->[+0] = le->[+0]` (canonical `le->prev->next = le->next; le->next->prev
 * = le->prev`), so +0x0 is `prev` and +0x4 is `next`. (Earlier recovery had these
 * swapped; no reconstructed consumer had touched the links, so this corrects it.) */
typedef struct localEntity_s {
    struct localEntity_s *prev;       /* +0x00 */
    struct localEntity_s *next;       /* +0x04 */
    int32_t leType;              /* +0x08: LE_* discriminant */
    uint32_t leFlags;             /* +0x0c: bit 0 tested by 0x3002ac20; == LEF_TRACER_MODE_A by CG_AddMovingTracer */
    int32_t endTime;             /* +0x10: cg.time at which the entity expires */
    float lifeRate;            /* +0x14: 1/(endTime-startTime) fade rate */
    trajectory_t pos;                 /* +0x18: position trajectory (trDelta at +0x30 = velocity/dir) */
    vec4_t color;               /* +0x3c: RGBA color multipliers in [0,1] */
    float radius;              /* +0x4c: base radius; CG_AddScaleFade uses it as the
                                       *        view-distance cull threshold and the refEntity.radius base */
    refEntity_t refEntity;            /* +0x50: render entity submitted to the scene */
} localEntity_t;

/* localEntity_t has pointer fields (prev/next) before the asserted offsets, so the
 * layout guards are only meaningful at the target's 4-byte pointer width. */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
_Static_assert(offsetof(localEntity_t, leType) == 0x08, "localEntity_t.leType offset");
_Static_assert(offsetof(localEntity_t, leFlags) == 0x0c, "localEntity_t.leFlags offset");
_Static_assert(offsetof(localEntity_t, endTime) == 0x10, "localEntity_t.endTime offset");
_Static_assert(offsetof(localEntity_t, pos) == 0x18, "localEntity_t.pos (trajectory) offset");
_Static_assert(offsetof(localEntity_t, pos.trDelta) == 0x30, "localEntity_t.pos.trDelta offset");
_Static_assert(offsetof(localEntity_t, color) == 0x3c, "localEntity_t.color offset");
_Static_assert(offsetof(localEntity_t, radius) == 0x4c, "localEntity_t.radius offset");
_Static_assert(offsetof(localEntity_t, refEntity) == 0x50, "localEntity_t.refEntity offset");
#endif

/* The modeled localEntity_t is exactly the real 236-byte (0xec) entity: 0x50 header
 * + a refEntity_t whose recovered layout spans to +0x9c. CG_InitLocalEntities
 * (0x3002a9e0) proves the stride is 0xec (loop `EAX += 0xec`) and that the array
 * clear zeroes MAX_LOCAL_ENTITIES*0xec bytes (rep stosd ECX=0x1d80 dwords = 30208 =
 * 128*236), so the array element type below is size-exact. The struct contains
 * pointers (prev/next and refEntity.owner), so the 0xec size only holds at the
 * target's 4-byte pointer width; guard the assert to 32-bit. */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
_Static_assert(sizeof(localEntity_t) == 0xec, "sizeof(localEntity_t) == 236 (array stride)");
#endif

/* Capacity of the cg_localEntities pool. Proven by CG_InitLocalEntities: the
 * rep-stosd clears 0x1d80 dwords = 30208 bytes = 128 * sizeof(localEntity_t), and the
 * free-list link loop runs from &cg_localEntities[0].next up to (but excluding)
 * &cg_localEntities[127].next (bound 0x30537c98), i.e. MAX_LOCAL_ENTITIES-1 links. */
#define MAX_LOCAL_ENTITIES 128

/* 0x30530780 .data — cg_localEntities: the fixed pool of local entities. Declared here
 * (not globals.h) because it is an array of the complete localEntity_t and both the type
 * and MAX_LOCAL_ENTITIES are defined above; globals.h is included ahead of this header and
 * only forward-declares the struct. cg_freeLocalEntities is initialized to
 * &cg_localEntities[0] and the free list is chained through it. */
extern localEntity_t cg_localEntities[MAX_LOCAL_ENTITIES];

/*
 * trap_R_AddRefEntityToScene — cgame syscall wrapper (id 0x3d) that submits one
 * render entity to the current scene. Single const refEntity_t * argument; the return
 * value is unused. Name from the same-module PPC bank
 * (cgame_mp!trap_R_AddRefEntityToScene); see CG_R_ADD_REF_ENTITY_TO_SCENE for the
 * evidence. Defined inline so it lowers to the raw trap call.
 */
static inline void trap_R_AddRefEntityToScene(const refEntity_t *re)
{
    cgame_syscall(CG_R_ADD_REF_ENTITY_TO_SCENE, (intptr_t)re);
}

/*
 * trap_MSS_PlayBlendedSoundAliases — sound-alias blend wrapper for syscall id
 * 0xc7. The original i386 entry receives four stack arguments plus origin and
 * timeShift in EDX/ECX, and forwards all six to cgame_syscall. Reconstructed at
 * src/client/cgame/module/trap_mss_playblendedsoundaliases.c; declared here for reuse. */
void trap_MSS_PlayBlendedSoundAliases(snd_alias_t *alias0, snd_alias_t *alias1, float blend, int32_t entityNum, const vec3_t origin,
                                      int32_t timeShift);

/*
 * trap_XAnimIsLooped (0x3003e960) — exact same-module wrapper for the XAnim
 * loop query (CG_XANIM_IS_LOOPED_BY_TREE_INDEX = 0x92). It takes ONE packed
 * 32-bit animation id and unpacks it into the trap's two arguments: the high
 * word becomes the tree index and the low word the animation index, i.e. it issues
 * cgame_syscall(0x92, packed >> 16, packed & 0xffff). Reconstructed at
 * src/client/cgame/module/trap_xanimislooped.c; declared here for reuse. */
qboolean trap_XAnimIsLooped(uint32_t packed);

/*
 * trap_XAnimGetAnimName (0x3003eba0) — thin cdecl wrapper for the XAnim name
 * trap (CG_XANIM_GET_ANIM_NAME = 0xa4), the byte-for-byte twin of
 * trap_XAnimIsLooped differing only in trap id. It takes ONE packed
 * 32-bit argument and unpacks it into the trap's two arguments: the high word
 * becomes arg1 (treeHandle) and the low word becomes arg2 (animIndex), i.e. it
 * issues cgame_syscall(0xa4, packed >> 16, packed & 0xffff) and returns the
 * trap's const char * animation-name pointer. Reconstructed at
 * src/client/cgame/module/trap_xanimgetanimname.c; declared here for reuse. The name
 * is proven by CoDUOMP.exe's dispatcher and the same-module Mac symbol. */
const char *trap_XAnimGetAnimName(uint32_t packed);

/*
 * trap_R_AddLightToScene — cgame trap-0x42 wrapper adding one dynamic light to the
 * current scene (org, intensity, r, g, b). Reconstructed at
 * src/client/cgame/module/trap_r_addlighttoscene.c; declared here for reuse. */
void trap_R_AddLightToScene(const vec3_t org, float intensity, float r, float g, float b);

/*
 * CG_AddHeadIconSprite (0x30032910, provisional-by-role) — build and submit one
 * camera-facing icon sprite (reType RT_SPRITE) over an entity's head.
 * Positions it at the "Bip01 Head" DObj tag (+18 units) when the entity has a
 * skeleton, else at the entity placement origin (+72 units), optionally scales the
 * radius by distance from cg_refdef.vieworg, and forces opaque-white shaderRGBA.
 * Sets renderfx = RF_THIRD_PERSON when the local player is following/spectating this
 * entity in first person. Register-arg ABI: the subject entity arrives in ESI
 * (modeled as the leading parameter); sfxOrShaderHandle/iconScale/attenuateByDistance
 * are the three caller-cleaned cdecl stack args (callers do `add esp,0xc`). The five
 * call sites are the CG_AddHeadIcon dispatcher at 0x30032b20. The .mcode size-guess
 * name "CG_VoiceChat" is rejected (size match; the body is a pure render-entity
 * emitter with no voice-chat work). See the function .c for the per-instruction
 * derivation; exact CoD symbol unproven, name is role-derived. */
void CG_AddHeadIconSprite(centity_t *entity, int32_t sfxOrShaderHandle, int32_t iconScale, int32_t attenuateByDistance);

/*
 * CG_AddHeadIcon (0x30032ac0) — per-frame dispatcher that picks and submits at most
 * one floating head icon for one rendered client entity, by calling
 * CG_AddHeadIconSprite. The subject entity arrives in ESI (register-arg ABI, modeled
 * as the sole parameter). Order of checks: (1) if the entity carries a headIconCsIndex
 * and the local player's team may see it, register CG_ConfigString(headIconCsIndex+37)
 * and draw it at scale 16; (2) if the entity is the local kill-cam subject, draw the
 * "you in kill cam" icon and stop; (3) otherwise the "disconnected", per-client
 * voice-chat, or "talk balloon" icon depending on eFlags / voiceChatTime. Reconstructed
 * at src/client/cgame/entities/cg_addheadicon.c. The .mcode size-guess name
 * `CG_DebugCircleEx` is REJECTED (that ring drawer is at 0x3001db70; this body has no
 * sin/cos loop and no CG_ADD_DEBUG_LINE — it is a head-icon selector). Name is
 * role-derived; exact CoD symbol unproven. */
void CG_AddHeadIcon(centity_t *cent);

/*
 * Q_rint (0x3006be3c) — MSVC CRT float/double -> int64 conversion helper `_ftol2`.
 * STATIC_LINKAGE msvc_crt: this is statically-linked runtime code, NOT id-Tech
 * source, so it has no .c reconstruction (matching sqrt_f/CG_CrtFloor/CG_pow).
 * `Q_rint` is retained ONLY as the corpus-wide caller-facing name (37 recovered
 * .c files already spell it this way); the name is historical and MISLEADING.
 *
 * TRUE SEMANTICS — TRUNCATION TOWARD ZERO, *not* rounding. The body does
 * `FISTP qword` (which rounds per the current x87 control word, default
 * round-to-nearest-even) and then applies the canonical `_ftol2` fractional
 * correction (the XOR 0x80000000 / ADD 0x7fffffff / ADC/SBB dance) that UNDOES
 * that rounding and forces the C round-toward-zero result. It is exactly what
 * MSVC emits for `(int)someFloat`. Proven by bit-faithful emulation of the 0x36
 * bytes: for every test value the output equals `(int)x` truncation:
 *     x =  2.5 -> 2      x =  2.4 -> 2      x =  2.6 -> 2      x =  3.5 -> 3
 *     x = -2.5 -> -2     x = -2.4 -> -2     x = -0.5 -> 0      x =  0.9 -> 0
 * So it does NOT round-half-to-even and does NOT round-half-up. Callers that
 * want nearest-integer must pre-add 0.5 (they don't, here — they faithfully
 * reproduce the binary's `(int)` truncation, which is correct as a
 * reconstruction).
 *
 * ABI: argument arrives already on the x87 stack (ST0) and is consumed; the low
 * 32 bits of the int64 result are returned in EAX (EDX:EAX for the full 64-bit).
 * The FPU-stack convention is not expressible in portable C, so this declaration
 * exists for naming only. Portable recovered low-word consumers call
 * coduo_fp_to_i32_extended, which reproduces the target-defined low dword for finite,
 * exceptional, and out-of-range inputs. */

/*
 * CG_AddFadeRGB (0x3002abc0) — local-entity handler for LE_FADE_RGB. Computes the
 * remaining-life fraction, scales the entity's RGBA color by it (and by 255) into
 * the refEntity's shaderRGBA bytes, and submits the entity to the scene. Register
 * ABI: the localEntity_t * is passed in ESI (the CG_AddLocalEntities dispatch loop
 * holds the current node there). */
void CG_AddFadeRGB(localEntity_t *le);

/*
 * CG_AddScaleFade (0x3002ac20) — the LE_SCALE_FADE render handler dispatched by
 * CG_AddLocalEntities (reconstructed: src/client/cgame/effects/cg_addscalefade.c).
 * Register ABI: the localEntity_t * is held in ESI when called (same as CG_AddFadeRGB).
 * Fades the sprite's alpha by its remaining-life fraction, grows refEntity.radius from
 * le->radius toward 8 (unless LEF_SCALE_FADE_NO_RADIUS is set), then either submits the
 * refEntity to the scene or retires the entity depending on its camera distance vs
 * le->radius. Name from the same-module PPC bank cgame_mp!CG_AddScaleFade. */
void CG_AddScaleFade(localEntity_t *le);

/*
 * CG_AddMovingTracer (0x3002ab00) — the LE_MOVING_TRACER render handler
 * (reconstructed: src/client/cgame/effects/cg_addmovingtracer.c). Register ABI:
 * localEntity_t * in ESI (also copied to EAX). If both the mode-selected tracer
 * width and length are >= 1e-6, it evaluates le->pos at cg_time, marches the
 * trajectory point cg_tracerLength* units along the normalized le->pos.trDelta,
 * and hands the result plus cg_tracerWidth* to the tracer poly builder
 * (CG_DrawMovingTracerPoly). Name from the same-module PPC bank
 * cgame_mp!CG_AddMovingTracer, corroborated by the behavior. */
void CG_AddMovingTracer(localEntity_t *le);

/*
 * CG_DrawMovingTracerPoly (0x30048460) — tracer polygon builder used by
 * CG_AddMovingTracer. cdecl: takes the tracer end point (vec3 *) and a width
 * scale (float, passed as a dword), transforms the point into the current view
 * basis (a 3x3 matrix at 0x30487aa8..), builds a textured/colored quad and issues
 * the R_AddPolysToScene-style trap (the VM syscall at cgame_syscall). Provisional
 * caller-observed decl named by role; the width arg is the mode-selected tracer
 * width float reinterpreted through the caller's int slot. Superseded by its own
 * .mcode reconstruction. */
/*
 * ABI WIDENED from CG_SpawnMovingTracer's proven call site (0x30048b52): the earlier
 * 2-arg placeholder `(const vec3_t endPoint, float width)` was INCOMPLETE — it dropped
 * the register-passed vec3. The callee's own body (0x30048467..0x30048496) reads a
 * second vec3 through EDI (`[EBP] - [EDI]` component-wise, EBP = first pushed vec3), so
 * there are TWO endpoint vec3s plus the width:
 *   register EDI -> tailPoint (vec3*)                      [caller: LEA EDI,&tail]
 *   stack  arg0  -> headPoint (vec3*)                      [caller: PUSH &head]
 *   stack  arg1  -> width     (float via its int slot)     [caller: PUSH width]
 * caller-cleaned (caller does ADD ESP,8). The register/stack split is an i386 detail;
 * modeled here as ordered C params. arity/types remain UNPROVEN in full until this
 * callee's own .mcode reconstruction. */
void CG_DrawMovingTracerPoly(const vec3_t tailPoint, const vec3_t headPoint, float width);

/*
 * Provisional caller-observed declarations for the CG_SpawnTracer (0x30048d60)
 * callees. ABIs re-derived from CG_SpawnTracer's OWN machine code at each call site;
 * arity/types UNPROVEN in full — verify against each callee's own .mcode when it is
 * reconstructed. These are the tracer-spawn cluster (0x30048xxx: muzzle-point +
 * moving-tracer + line-effect builders).
 */

/* CG_CalcMuzzlePoint (0x30048b60) — RECONSTRUCTED in
 * src/client/cgame/weapons/cg_calcmuzzlepoint.c. Resolve the world muzzle/emit point of a
 * shot for entity `entityNum`, using the named model tag `weaponName` ("tag_flash" /
 * "tag_secondary_flash"), writing the vec3 `muzzle`. For the LOCAL predicted first-person
 * player it uses cg_snap's view origin; for other entities it queries the entity's DObj
 * tag (trap 0xa5), falling back to the entity lerpOrigin. Returns qtrue when a point was
 * produced. Register-argument ABI proven in its own file: weaponName in EAX, entityNum in
 * ECX, muzzle a single caller-pushed stack pointer. CG_SpawnTracer's call site matches
 * (EAX=tagName, ECX=entityNum, PUSH &muzzle; ADD ESP,4). */
qboolean CG_CalcMuzzlePoint(const char *weaponName, int32_t entityNum, vec3_t muzzle);

/* CG_SpawnMovingTracer (0x30048a00) — RECONSTRUCTED in
 * src/client/cgame/weapons/cg_spawnmovingtracer.c. Builds the moving/animated bullet-tracer
 * geometry for one shot: selects the tracer width/length twins by
 * bg_weaponInfos[weaponInfoIndex]->ammoType (LMG/HMG/UMG -> mode A, else mode B),
 * bails if either is < 1e-6, computes the unit direction from the muzzle start
 * toward the impact end (VectorNormalize) and the shot span, applies a near-shot
 * cull, then places two ray points (tail = start + tailScalar*dir,
 * head = start + min(tail+length,dist)*dir with
 * a per-frame rand() jitter) and hands them plus the width to CG_DrawMovingTracerPoly.
 *
 * ABI SUPERSEDED (the earlier placeholder guessed a phantom `int edxUnknownZero` slot and
 * mislabeled the mode-select index as a stack arg). Proven from this body + the sole call
 * site 0x30048e20 (CG_SpawnTracer's selector-7 branch: XOR EDX,EDX;
 * MOV ECX,impactOrigin; LEA EAX,&muzzle; PUSH weaponIndex; CALL; ADD ESP,4):
 *   register EAX -> startOrigin     (vec3*, muzzle point; copied to EDI at entry)
 *   register ECX -> endOrigin       (vec3*, impact point)
 *   register EDX -> weaponInfoIndex (bg_weaponInfos[] index used ONLY for mode select; caller passes 0)
 *   stack  arg0  -> weaponIndex     (int; only consulted by the near-shot cull)
 * caller-cleaned. Role name from the caller + behavior; exact CoD symbol unproven. The
 * .mcode's size-matched `G_RunItem` guess is REJECTED (this is a cgame tracer builder,
 * not a server item think). */
void CG_SpawnMovingTracer(vec3_t startOrigin, vec3_t endOrigin, int32_t weaponInfoIndex, int32_t weaponIndex);

/* CG_SpawnTracerLine (0x30048260) — RECONSTRUCTED in
 * src/client/cgame/weapons/cg_spawntracerline.c. Spawns a moving LINE tracer as a
 * LE_MOVING_TRACER local entity that flies from the shot start point toward the shot
 * end point. Selects the tracer length twin by bg_weaponInfos[weapon]->ammoType
 * (LMG/HMG/UMG -> mode A, else mode B); returns without spawning when the shot span
 * (VectorNormalize of end-start) is shorter than that length. Pulls the endpoint back
 * by tracerLength along the shot direction, builds a TR_LINEAR trajectory whose base is
 * the start point and whose trDelta is cg_tracerSpeed * dir, and sets startTime/endTime
 * so the entity ages out exactly when it reaches the (shortened) endpoint; the start
 * time is dithered back by (rand() % cg.frametime)/2 ms to de-alias per-frame spawns.
 * The tracer-mode-A class also OR-marks le->leFlags = LEF_TRACER_MODE_A.
 *
 * ABI proven from this body + CG_SpawnTracer's non-7 branch call site
 * 0x30048e42 (MOV EAX,impactOrigin; LEA ECX,&muzzle; PUSH weapon; CALL; ADD ESP,4):
 *   register EAX -> endOrigin   (vec3, shot end point; read only)
 *   register ECX -> startOrigin (vec3, shot start point; read only)
 *   stack  arg0  -> weapon      (int; index into bg_weaponInfos[] for length/flag select)
 * caller-cleaned. Role name adopted from CG_SpawnTracer's call site + behavior (it is the
 * alternate-path twin of CG_SpawnMovingTracer, which builds tracer geometry directly; this
 * one instead spawns a self-moving local entity). The .mcode's size-matched "Menus_Open"
 * guess is REJECTED (this is a cgame tracer/local-entity builder, not a UI menu opener). */
void CG_SpawnTracerLine(const vec3_t endOrigin, const vec3_t startOrigin, int32_t weapon);

/* CG_WhizbySound (0x300480f0) — reconstructed in
 * src/client/cgame/weapons/cg_whizbysound.c. From the bullet impact point (register
 * EAX, a vec3) and the muzzle point (register EBX, a vec3), project the view origin
 * (cg_refdef.vieworg @0x30487a90) onto the shot ray; if the closest approach lies
 * within the segment (t >= 64, ray length >= t+64) and within 140 units of the
 * listener, play the registered "whizby" sound (0x3044c1e4) at that point pulled 16
 * units back toward the muzzle. The prior caller-guess name CG_TracerMuzzleFlashDist
 * is superseded (void, plays a sound — no distance is returned; the "whizby" handle
 * proves it is the bullet fly-by sound, not a muzzle flash). */
void CG_WhizbySound(const vec3_t impactOrigin, const vec3_t muzzle);

/*
 * CG_SpawnTracer (0x30048d60) — reconstructed in
 * src/client/cgame/weapons/cg_spawntracer.c. Probabilistically spawns a bullet
 * tracer for one shot: gated on cg_tracerchance_vmCvar.value > 0, computes the muzzle point of the
 * shot (CG_CalcMuzzlePoint on the "tag_flash"-family tagName), suppresses the tracer for
 * the local first-person view, rolls rand()/32768 against the ammo-type-selected
 * chance, and on success spawns either a moving tracer (surface type 7) or a
 * line tracer (other selector values), then evaluates the whiz-by sound.
 * Mixed register/stack ABI proven from its three call sites: impactOrigin in EAX,
 * entityNum in ESI, weaponIndex in EDI,
 * plus stack args (surfaceType, tagName). The EAX vector is the bullet impact
 * point; the function resolves the muzzle point internally. Role name from
 * behavior; exact CoD symbol
 * unproven. The .mcode's size-matched "BG_AnimScriptStateChange" guess is REJECTED —
 * this is cgame tracer/muzzle-flash rendering (rand chance, tag muzzle point, tracer
 * local-entities), not a BG anim-script state machine. */
void CG_SpawnTracer(vec3_t impactOrigin, int32_t entityNum, int32_t weaponIndex, int32_t surfaceType, const char *tagName);

/*
 * Muzzle-tag name pointer slots (.data at 0x30085eec/0x30085efc/0x30085f00). Each holds
 * a const char* into .rdata; CG_BulletHitEvent forwards them to CG_SpawnTracer as the
 * tagName. NOT a syscall dispatcher (that is cgame_syscall at 0x30085e9c):
 *   0x30085eec -> 0x300772c0 "tag_flash"            (plain-weapon muzzle tag)
 *   0x30085efc -> 0x3007ac10 "tag_altfire"          (vehicle weapon mount position 1)
 *   0x30085f00 -> 0x3007abfc "tag_secondary_flash"  (vehicle weapon mount position 2)
 */

/*
 * CG_BulletHitEvent (0x30048e60) — reconstructed in
 * src/client/cgame/weapons/cg_bullethitevent.c. The client-side bullet fire/impact
 * event handler: plays the surface impact sound (CG_PlaySoundAliasByName) and the primary +
 * secondary oriented impact/blood effects (cgame_syscall CG_PLAY_EFFECT_ORIENTED) at the
 * hit point, then spawns the bullet tracer (CG_SpawnTracer) from the right muzzle tag —
 * "tag_flash" for a hand weapon, or "tag_altfire"/"tag_secondary_flash" for a
 * vehicle-mounted weapon's firing position. Names itself via its lone error string
 * "CG_BulletHitEvent: unknown vehicle position\n" (0x3007a7f4, printed at 0x30049013);
 * the .mcode size-guess "RegisterItem" is REJECTED. Plain cdecl caller-cleaned ABI
 * (both call sites push 8 dwords and ADD ESP,0x20). */
void CG_BulletHitEvent(int32_t fireEntityNum, vec3_t origin, vec3_t effectDir1, vec3_t effectDir2, int32_t weaponIndex, int32_t surfaceType,
                       int32_t linkedEntitySlotPlus1, int32_t vehicleMountPos);

/*
 * CG_AddLocalEntities (0x3002ad00) — per-frame local-entity manager. Walks the
 * cg_activeLocalEntities circular list from its .prev link toward the sentinel;
 * for each node, if cg_time has reached le->endTime it unlinks the node and
 * pushes it back onto cg_freeLocalEntities (inlined CG_FreeLocalEntity), otherwise
 * it dispatches on le->leType to the LE_* render handler (LE_FADE_RGB ->
 * CG_AddFadeRGB, LE_SCALE_FADE -> CG_AddScaleFade, LE_MOVING_TRACER ->
 * CG_AddMovingTracer). Raises the CG_ERROR "Bad leType: %i" for an out-of-range
 * discriminant. No args, no return. */
void CG_AddLocalEntities(void);

/*
 * CG_AllocLocalEntity (0x3002aa70) — pull a localEntity_t off the free list,
 * zero it, and link it at the head of the cg_activeLocalEntities list; returns
 * the new (zeroed) entity. When the free list is empty it recycles the oldest
 * active entity (cg_activeLocalEntities.prev) by inlining CG_FreeLocalEntity,
 * which raises the CG_ERROR "CG_FreeLocalEntity: not active" if that entity is
 * not linked. Maintains cg_numLocalEntities. */
localEntity_t *CG_AllocLocalEntity(void);
void CG_SpawnScaleFadeSprite(const vec3_t origin, qhandle_t shader, int32_t radius, int32_t duration);
void CG_DrawRotatedQuadPic(const vec4_t color, float x, float y, float width, float height, int32_t rotation, int32_t pivot);
void CG_Trap54DrawStyle4(const vec4_t color, int32_t x, float y, const char *string);
void CG_Trap54DrawStyle5(const vec4_t color, int32_t x, float y, const char *string);
int32_t CG_PlayGearRattleSound(int32_t entityNum, qboolean sprinting, qboolean running);
/* Register-ABI thunk at 0x300435c0: map a weapon index in EAX into the dedicated
 * view-weapon DObj handle band by adding the same 1024 bias used by the adjacent
 * weapon registration/release code. */
enum {
    CG_VIEW_WEAPON_DOBJ_HANDLE_BASE = 1024
};
int32_t CG_WeaponDObjHandle(int32_t weaponIndex);

/*
 * CG_InitLocalEntities (0x3002a9e0, reconstructed:
 * src/client/cgame/effects/cg_initlocalentities.c) — reset the local-entity subsystem at
 * cgame startup: zero the cg_localEntities pool, empty the cg_activeLocalEntities
 * circular list, rebuild the cg_freeLocalEntities free list, and zero
 * cg_numLocalEntities. Called from CG_Init. No args, no return. */
void CG_InitLocalEntities(void);

/*
 * CG_SpawnRailCoreSegment (0x300430a0) — allocate one rail-trail core segment as a
 * fading local entity and initialize it. Called repeatedly by the rail-trail
 * builder at 0x30043190+ (a CG_RailTrail-family routine that marches segments
 * along the rail curve). Behavior proven from the machine code:
 *   - early-out when cg_railTrailTime_vmCvar.integer (0x3045146c) <= 0 (no segment spawned);
 *   - le = CG_AllocLocalEntity() (zeroed);
 *   - le->leType = LE_FADE_RGB (0); le->endTime = cg.time + cg_railTrailTime_vmCvar.integer;
 *     le->lifeRate = 1.0f / cg_railTrailTime_vmCvar.integer;
 *   - le->refEntity.reType = RT_RAIL_CORE; identity axis (axis[i][i]=1);
 *     refEntity.origin = start; refEntity.oldorigin = end;
 *     refEntity.spriteShaderHandle = cg_railCoreShader;
 *     refEntity.shaderTime = cg.time / 1000.0f;
 *   - le->color = { colorRGB[0], colorRGB[1], colorRGB[2], 1.0f } (alpha forced 1).
 *
 * ABI (proven at the call sites 0x30043511.. and here): the two endpoint vec3
 * pointers arrive in registers — `start` in EBX, `end` in EDI — and the RGB
 * color vec3 pointer is the single cdecl stack argument (callers push it and do
 * `add esp,4`; the callee does a plain RET). This register-argument convention is
 * not expressible in portable C without a non-portable attribute the build target
 * does not yet require, so it is documented here and the parameters are ordered
 * start, end, colorRGB.
 *
 * Name is role-derived (rail-core trail segment). The mechanical .mcode name
 * "Pmove" is REJECTED: it was a pure size guess (win size 0xec == sizeof at a
 * Pmove match) and the body proves a local-entity/refEntity rail-core builder,
 * not a player-move frame. Exact original CoD symbol unproven.
 */
void CG_SpawnRailCoreSegment(const vec3_t start, const vec3_t end, const vec3_t colorRGB);

/* CG_RailTrail (0x30043190) -- build a rail-core trail between start/end using a
 * palette selected by colorIndex. Simple palette values emit one segment; value 5
 * emits a closed 16-segment ring; other values emit the 12 edges of the axis-aligned
 * box whose opposite corners are start/end. Register ABI in the DLL is
 * EAX=colorIndex, ECX=start, EDX=end; modeled as an ordinary ordered signature. */
void CG_RailTrail(int32_t colorIndex, const vec3_t start, const vec3_t end);

/*
 * trap_Argv — cgame syscall wrapper that copies console token `n` into `buffer`
 * (NUL-terminated, truncated to `bufferLength`). Signature matches the recovered
 * server trap_Argv(int arg, char *buffer, int bufferLength). Evidence: call site
 * 0x300178c0 pushes (0xd, n=0, buffer, len=0x400); id 0xd is the argv trap. The
 * client analog of the server's trap_Argv (server_name_bank). Defined inline so
 * it lowers to the raw trap call.
 */
static inline void trap_Argv(int32_t n, char *buffer, int32_t bufferLength)
{
    cgame_syscall(CG_ARGV, n, (intptr_t)buffer, bufferLength);
}

/*
 * CG_Argv (0x3002b4d0) — convenience wrapper that copies console token `n` into the
 * shared 1024-byte g_textScratchBuffer and returns a pointer to it, the classic
 * id-Tech `Cmd_Argv`/`CG_Argv` "read argv into a static buffer, hand back the
 * buffer" idiom. The trap length is hardwired to sizeof(g_textScratchBuffer)==1024
 * (0x400). Evidence: FUN_3002b4d0 pushes (0x400, &g_textScratchBuffer, EAX, 0xd)
 * to cgame_syscall (id 0xd == CG_ARGV) then MOV EAX,&g_textScratchBuffer / RET.
 * ABI: the argv index `n` arrives in EAX (register/fastcall-style — every caller,
 * e.g. 0x3003a691/0x3003acde, does `mov $N,%eax` immediately before the call);
 * there are no incoming stack args and the body issues a plain RET. The four dwords
 * it pushes for the syscall are cleaned by its own ADD ESP,0x10. Fully reconstructed
 * in src/client/cgame/module/cg_argv.c. */
char *CG_Argv(int32_t n);

/*
 * CG_VoiceChat_f (0x30017780) — the "vsay" console-command handler (one of the
 * cgame `_f` command callbacks; takes no args, reads tokens via the trap_Arg*
 * syscalls). If the argument count is odd it does nothing. Otherwise, when the
 * local player is a live participant (cg_snap present, ps.pmType != INTERMISSION,
 * and playerStateFlags bit 0x80000 clear) it refuses with the localized
 * "CGAME_NOSPECTATORVOICECHAT" message; for a spectator/intermission player it
 * forwards the chosen voice line to the server as the console command
 * va("cmd vsay %s\n", argv[1]). Named CG_VoiceChat_f (same-module PPC bank) from
 * its non-team "cmd vsay %s\n" (0x300768b4) command string and the
 * CGAME_NOSPECTATORVOICECHAT gate; the .mcode size-guess
 * "PM_InteruptWeaponWithSprintMove" is rejected (no pmove/weapon work here). The
 * team variant "cmd vsay_team %s\n" (0x300768a0) belongs to CG_TeamVoiceChat_f.
 */
void CG_VoiceChat_f(void);

/*
 * cgVoiceChatMessage_t — the small decoded-fields struct CG_ParseVoiceChat
 * (0x3003a410) assembles from the console tokens and hands (by pointer, in ECX) to
 * CG_VoiceChat (0x3003a250). CG_VoiceChat reads the origin vec3 from it (copied
 * to the outgoing cgVoiceChatMsg_t->spriteOrigin); `color` is also passed to
 * CG_VoiceChat as the 4th stack arg and printed as the character after '^' via the
 * format's second %c (0x3003a361/0x3003a384/0x3003a3a2 read stack arg 4). The
 * (mode, voiceOnly, clientNum, color, cmd) shape matches RTCW CG_VoiceChatLocal.
 * Proven from CG_VoiceChat's own body: MOV [EDI]/[EDI+4]/[EDI+8] read the three
 * origin floats at +0x0/+0x4/+0x8. */
typedef struct cgVoiceChatMessage_s {
    vec3_t origin;   /* +0x0: argv[3], argv[6], argv[7] as floats */
    int32_t color;    /* +0xc: argv[5] as int; the '^'+%c color char in the chat line */
} cgVoiceChatMessage_t;

/*
 * CG_VoiceChat (0x3003a250) — the voice-chat display/render routine. RECONSTRUCTED
 * (see src/client/cgame/sound/cg_voicechat.c). Given a decoded incoming
 * voice-chat message it: clamps the speaker `clientNum` to [0,64) (out-of-range -> 0);
 * indexes the 0x4d0-stride clientInfo[] table (0x305e1f34) for that client;
 * early-outs if that slot's infoValid is 0; picks the sound alias + head-icon
 * for the token via CG_PickSoundAlias (0x30039f10) on the axis (team==1) or allies
 * voice-chat table; suppresses non-team (mode!=1) messages when the cg_teamChatsOnly_vmCvar.integer
 * gate (0x3052edac) is set; assembles a cgVoiceChatMsg_t (clientNum, the two picked
 * sound handles, voiceOnly, the Q_strncpyz'd token, and the origin), formats one of
 * three team/quick-chat display strings ("[%s]%s[%s]: %c%c%s" / "(%s)%s(%s): %c%c%s" /
 * "%s %s(%s): %c%c%s" for mode 2/1/other) into its text via Com_sprintf, and dispatches
 * to CG_PlayVoiceChat (0x30039ff0) for the sound/icon/print.
 *
 * SIGNATURE (proven from the body; the provisional arity/types are confirmed but the
 * provisional field NAMES were wrong): fastcall ECX = the cgVoiceChatMessage_t*; five
 * caller-cleaned (`add esp,0x14`) cdecl stack args in order: mode (0/1/2 -> display
 * variant), voiceOnly (stored to cgVoiceChatMsg_t->voiceOnly), clientNum (the speaker,
 * range-clamped and used to index the anim-state table), color (printed as the char
 * after '^' via the format's second %c; RTCW CG_VoiceChatLocal lineage), and
 * voiceChatString (the sound-alias token, picked + copied into msg.token).
 * The .mcode size-match name "PlayerCmd_ClonePlayer" is REJECTED. */
void CG_VoiceChat(cgVoiceChatMessage_t *msg, int32_t mode, int32_t voiceOnly, int32_t clientNum, int32_t color,
                  const char *voiceChatString);

/*
 * CG_ParseVoiceChat (0x3003a410) — client handler for a server-sent voice-chat
 * command. Reads seven console tokens via trap_Argv(n, g_textScratchBuffer, 1024) and
 * Q_atoi's them: argv[1]/argv[2]/argv[5] as ints, argv[3]/argv[6]/argv[7] converted
 * to floats forming a vec3 origin, and argv[4] kept as the voice-chat-string token.
 * When the g_voiceChatCategoryFilter flag (0x30421c0c) is set it suppresses the
 * "insult"/"taunt"/"praise"/"gauntlet" categories (string-compared against
 * kill_insult/taunt/death_insult/kill_gauntlet/praise); otherwise it forwards the
 * decoded fields to CG_VoiceChat (0x3003a250) for display. `mode` (0/1/2) is the sole
 * incoming stack arg, selecting the broadcast/team/target variant (the three call
 * sites in CG_ServerCommand at 0x3003aec8/0x3003aee7/0x3003af06 push 0/1/2). See
 * src/client/cgame/sound/cg_parsevoicechat.c. */
void CG_ParseVoiceChat(int32_t mode);

/*
 * trap_XAnimCreateTree — cgame syscall wrapper (id 134) that creates a new anim-tree
 * instance from the master tree `animTree` and returns its engine handle. One 32-bit
 * argument; the return value is the int the loop stores. Name from the same-module
 * PPC bank (cgame_mp!trap_XAnimCreateTree); see CG_XANIM_CREATE_TREE above for the
 * evidence. Defined inline so it lowers to the raw trap call.
 */
static inline XAnimTree *trap_XAnimCreateTree(XAnim *animTree)
{
    return (XAnimTree *)(intptr_t)cgame_syscall(CG_XANIM_CREATE_TREE, (intptr_t)animTree);
}

/*
 * trap_XAnimGetNotetracks — cgame syscall wrapper (id 0x99) that returns the
 * notetrack list for the animation currently bound on the active DObj. It writes a
 * pointer to the engine-owned xanim_deferred_notify_t array through *outList and returns the
 * record count. Proven from CG_SetGunHandFromNotetracks (0x3001f760): push &listPtr;
 * push 0x99; call cgame_syscall; add esp,8. Provisional role name (no server proto
 * proven); see CG_DOBJ_GET_CLIENT_NOTIFY_LIST above. Defined inline so it lowers to the raw
 * trap call.
 */
static inline int32_t trap_XAnimGetNotetracks(xanim_deferred_notify_t **outList)
{
    return (int32_t)cgame_syscall(CG_DOBJ_GET_CLIENT_NOTIFY_LIST, (intptr_t)outList);
}

/*
 * CG_SetGunHandFromNotetracks (0x3001f760) — for a given client index, scan the
 * notetrack list of the currently-bound animation and record which hand holds the
 * gun into bgs.clientinfo[client]. Ignores out-of-range clients (index < 0 or
 * >= 64 == MAX_CLIENTS). For each notetrack whose kind == 1, an
 * `anim_gunhand = "left"` name sets gunHandLeft = 1 and an `anim_gunhand = "right"`
 * name sets gunHandLeft = 0; either match also sets dobjNeedsUpdate = 1. Provisional role
 * name — the .mcode's size-guessed "PM_BeginWeaponDeploy" is rejected (this is a
 * cgame notetrack/anim-state routine, not a pmove function). */
void CG_SetGunHandFromNotetracks(int clientNum);

/*
 * CG_ProcessWeaponNoteTracks (0x30042c40) — for the local player's currently
 * equipped weapon (cg_predictedPlayerState.currentWeapon), scan the currently-bound
 * animation's notetrack list and play any per-weapon "noteTrackSound{A..D}" sound
 * whose track fired this frame. No-ops when cg_predictedPlayerState.currentWeapon == 0.
 * Otherwise it resolves cg_weaponInfos[currentWeapon] (IMUL idx,0x1c4; ADD base
 * 0x30413580), calls trap_XAnimGetNotetracks(&list) (id 0x99) for the count, and
 * for each notetrack name Q_stricmp-matches it against "noteTrackSoundA/B/C/D"
 * (first match wins, A..D order). On a match it loads the corresponding handle
 * from the weapon record (noteTrackSoundA..D at +0x118..+0x124) and, when nonzero,
 * plays it via CG_PlaySoundAliasByName(&cg_snap->ps.psOrigin, handle,
 * cg_snap->ps.psClientNum). Unlike the sibling CG_SetGunHandFromNotetracks this consumer
 * reads only the notetrack name (+0x00), never the kind word (+0x04).
 *
 * Provisional role name: the .mcode's mechanical "finishSpawningKeyedMover" is a
 * size-match to a game_mp_uo server name and is REJECTED — this is a cgame
 * weapon/notetrack sound emitter, not a mover-spawn routine. Exact CoD symbol
 * unproven. */
void CG_ProcessWeaponNoteTracks(void);

/*
 * CG_AllocAnimTree (0x30016340) — the anim-tree memory allocator callback that
 * CGScr_LoadAnimTrees hands to Scr_PrecacheAnimTrees. It forwards `size` to the
 * engine as cgame_syscall(205, size) and returns the allocated block. Machine code:
 *   mov eax,[esp+4] ; push eax ; push 205 ; call cgame_syscall ; add esp,8 ; ret
 * (cdecl, one int arg, discards nothing; the int32 syscall result is the pointer).
 * Server bank shape: void *GScr_AllocAnimTreeMemory(size_t). Defined inline so it
 * lowers to the raw trap call, matching the tiny callback in the DLL.
 */
static inline void *CGAME_ABI_CDECL CG_AllocAnimTree(size_t size)
{
    return (void *)(intptr_t)cgame_syscall(CG_HUNK_ALLOC_ALIGN, size);
}

/*
 * Scr_{Begin,End}LoadAnimTrees / Scr_PrecacheAnimTrees — the three script anim-tree
 * load traps, issued as direct engine-import calls (not VM syscalls) through the
 * function-pointer slots at 0x300f0978 / 0x300f0980 / 0x300f0984. See those slots in
 * globals.h for the evidence and the client/PPC-bank name derivation. Defined inline
 * so each lowers to the raw indirect call the DLL makes.
 */
static inline void Scr_BeginLoadAnimTrees(void)
{
    cg_scriptImports.beginLoadAnimTrees();
}

static inline void Scr_EndLoadAnimTrees(void)
{
    cg_scriptImports.endLoadAnimTrees();
}

static inline void Scr_PrecacheAnimTrees(scr_anim_tree_alloc_t alloc)
{
    cg_scriptImports.precacheAnimTrees(alloc);
}

/* Direct script-import wrappers. Their identities are fixed by the shared
 * 102-slot Scr_FarHook table recovered independently in the server and engine:
 * slots 80, 81, and 98 are Scr_FindAnimTree, Scr_FindAnim, and
 * Scr_GetAnimsIndex. The former DObj-oriented names were plausible but wrong. */
static inline XAnim *Scr_FindAnimTree(const char *treeName)
{
    return cg_scriptImports.findAnimTree(treeName);
}

static inline void Scr_FindAnim(const char *treeName, const char *animName, scr_anim_t *outAnim)
{
    cg_scriptImports.findAnim(treeName, animName, outAnim);
}

static inline uint32_t Scr_GetAnimsIndex(XAnim *anims)
{
    return cg_scriptImports.getAnimsIndex(anims);
}

/* The CG_R_TEXT_PAINT emitter family at 0x30031940/0x30031a00/0x30031a90
 * receives the same Quake-derived rectDef_t used by CG_OwnerDraw. MOV loads of
 * x/y forward their float bit patterns; they do not establish a second object
 * type or integer union members. Use CG_FloatBits at those scalar boundaries. */

/*
 * trap_R_Text_Paint (0x3003de30) — thin 9-argument cdecl wrapper that forwards its nine
 * stack args unchanged to cgame_syscall(CG_R_TEXT_PAINT, a0..a8) and returns the int32
 * result. The shared fixed-arity entry point used by trap-54 emitters that build
 * the argument vector themselves (e.g. CG_DrawPlayerWeaponName 0x3002ec10). Full
 * body in src/client/cgame/module/trap_r_text_paint.c.
 */
int32_t trap_R_Text_Paint(intptr_t a0, intptr_t a1, intptr_t a2, intptr_t a3, intptr_t a4, intptr_t a5, intptr_t a6, intptr_t a7,
                          intptr_t a8);
/*
 * trap_R_Text_PaintWithCursor (0x3003de90) — recovered ten-word wrapper for
 * cgame syscall 55. Its cursor-character word is sign-extended from byte width,
 * exactly as the original MOVSX at 0x3003deba.
 */
int32_t trap_R_Text_PaintWithCursor(intptr_t xBits, intptr_t yBits, intptr_t font, intptr_t scaleBits, intptr_t color, intptr_t text,
                                    intptr_t cursorPos, intptr_t cursorChar, intptr_t limit, intptr_t textStyle);

/*
 * CG_DrawSpectatorFollowHints (0x3001bd50) — draw the spectator "follow" key-hint
 * HUD: up to three bottom-of-screen text lines telling a following spectator which
 * keys advance/rewind the followed player and which stops following. Gated by the
 * cvar mirror cg_descriptiveText_vmCvar.integer (0x3052f6ac) and by the local
 * playerState flags in cg_snap->ps.playerStateFlags (+0x18):
 *   - bit 0x100000 present in (flags & 0x300000) => "follow next/previous" lines
 *     are shown (label CGAME_FOLLOWNEXTPLAYER on "+attack", CGAME_FOLLOWPREVIOUS-
 *     PLAYER on "toggle cl_run");
 *   - bit 0x200000 => the "stop following" line (CGAME_FOLLOWSTOP) is shown, whose
 *     key is whichever of "toggle cl_run"/"+speed" is currently bound.
 * Each line's text is va(translatedLabel, boundKeyName), drawn white (RGBA 1,1,1,1)
 * at x=240, scale 0.2083 via trap_R_Text_Paint (2D text draw), stacking down y=416/426/436.
 * No arguments, no return. Full body in
 * src/client/cgame/hud/spectator_follow_hints.c.
 */
void CG_DrawSpectatorFollowHints(void);

/*
 * trap_R_Text_Width (0x3003dde0) — thin 4-argument cdecl wrapper that forwards its four
 * stack args to cgame_syscall(CG_R_TEXT_WIDTH, ...) and returns the int32 result
 * (callers FILD it to a float text metric). Proven from the wrapper body: PUSH id
 * 0x34 (52), four dwords, then ADD ESP,0x14. Paired with trap_R_Text_Height in text-draw
 * measurement (see CG_R_TEXT_WIDTH). Provisional caller-observed ABI; superseded by the
 * wrapper's own .mcode reconstruction. Source: uo_cgame_mp_x86.dll 0x3003dde0.
 */
int32_t trap_R_Text_Width(const char *text, int32_t font, int32_t scaleBits, int32_t limit);

/*
 * trap_R_Text_Height (0x3003de10) — thin 2-argument cdecl wrapper that forwards its two
 * stack args to cgame_syscall(CG_R_TEXT_HEIGHT, ...) and returns the int32 result
 * (callers FILD it to a float text/line metric). Proven from the wrapper body:
 * PUSH id 0x35 (53), two dwords, then ADD ESP,0xc. Paired with trap_R_Text_Width (see
 * CG_R_TEXT_HEIGHT). Provisional caller-observed ABI; superseded by the wrapper's own
 * .mcode reconstruction. Source: uo_cgame_mp_x86.dll 0x3003de10.
 */
int32_t trap_R_Text_Height(int32_t a0, int32_t a1);

/* trap_R_SetFog (0x3003e040) — reconstructed seven-dword forwarding wrapper for
 * cgame syscall 68. CG_ParseFog passes the renderer fog payload through it. */
int32_t trap_R_SetFog(int32_t a1, int32_t a2, int32_t a3, int32_t a4, int32_t a5, int32_t a6, int32_t a7);

/*
 * CG_TranslateMessage (0x3002d850) — localize text via syscall 57, then replace the first
 * "[{command}]" marker with "[bound keys]" in the alternating
 * cg_translateMessageBuffers slots. Returns the localized pointer unchanged when there is
 * no complete marker or no binding. Reconstructed in FUN_3002d850_3002da84.c.
 */
const char *CG_TranslateMessage(const char *src, const char *keyOrFormat);

/* CG_ParseFog (0x300384c0) — parse CS_FOGVARS and submit it through the renderer
 * fog trap. Same-module PPC name; reconstructed in FUN_300384c0_3003878b.c. */
void CG_ParseFog(void);

/*
 * CG_BoldGameMessage (0x3002b370) — thin __fastcall wrapper for cgame trap id 3 (CG_BOLD_GAME_MESSAGE).
 * It forwards its single ECX register argument plus the fixed module global at
 * 0x3048c14c (cg_gameBoldMessageWidth_vmCvar.integer) to cgame_syscall(CG_BOLD_GAME_MESSAGE, arg,
 * global) and returns the syscall's int32 result. Proven from the wrapper body:
 * MOV EAX,
 * [0x3048c14c]; PUSH EAX; PUSH ECX; PUSH 3; CALL [0x30085e9c]; ADD ESP,0xc; RET —
 * three dwords, caller-cleaned. Sibling of CG_GameMessage (0x3002b350), identical shape.
 * CoDUOMP.exe's CG_BOLD_GAME_MESSAGE receiver proves the service. Source:
 * uo_cgame_mp_x86.dll 0x3002b370 and CoDUOMP.exe's cgame syscall dispatcher.
 */
int32_t CG_BoldGameMessage(const char *message);

/*
 * trap_MSS_FadeAllSounds (0x3003f030) — thin cdecl wrapper for cgame trap id
 * 0xdb (CG_MSS_FADE_ALL_SOUNDS). Takes a single float and forwards its raw
 * 32-bit word as the trap's first payload dword. Proven from the wrapper body
 * (MOV EAX,[ESP+4]; PUSH ECX; MOV EDX,EAX; PUSH EDX; PUSH 0xdb; MOV [ESP+0x10],EAX;
 * CALL [0x30085e9c]; ADD ESP,0xc; RET) and its sole call site 0x3003b03b, which does
 * FSTP DWORD [ESP] then ADD ESP,4 for the float and explicitly loads ECX from the
 * parsed second command argument. The trap's full shape is
 * (float targetVolume, int durationMsec).
 * Fire-and-forget (void). Source: uo_cgame_mp_x86.dll 0x3003f030.
 */
void trap_MSS_FadeAllSounds(float targetVolume, int32_t durationMsec);

/*
 * CG_PriorityCenterPrint (0x30019050) — queue a center-screen HUD message.
 * ABI: str in [esp+4], y in [esp+8] (float), charWidth in [esp+0xc] (float), priority
 * in EAX (register). If a print is already active (cg_centerPrintTime != 0) and
 * the new priority is lower than the current cg_centerPrintPriority, the call is
 * ignored. Otherwise it records the priority, localizes the text via
 * CG_TranslateMessage(str, "Center Print"), CRT-strncpy's 1023 bytes into the
 * 1024-byte cg_centerPrintString and terminates byte 1023, sets
 * cg_centerPrintTime = cg_time + 2000, stores cg_centerPrintY/CharWidth via target
 * _ftol2 truncation, word-wraps the buffer to <=75 glyphs/line (inserting '\n'
 * at spaces via the CG_SE_READ_CHAR_FROM_STRING glyph iterator), and counts cg_centerPrintLines.
 * The register-passed priority argument is an i386 ABI detail (declared here as a
 * normal parameter). Source: 0x30019050..0x3001918c. */
void CG_PriorityCenterPrint(const char *str, float y, float charWidth, int32_t priority);

/*
 * CG_DrawCenterString (0x300191b0) — paint the queued center-screen HUD message.
 * The draw-side counterpart of CG_PriorityCenterPrint: if cg_centerPrintTime != 0
 * it fades the message via CG_FadeColor(cg_centerPrintTime, cg_centertime_vmCvar.value
 * * 1000 ms) and, while the fade is live, draws each '\n'-delimited line horizontally
 * centered ((640 - width)*0.5) using the text metric/draw traps (CG_R_TEXT_HEIGHT height,
 * CG_R_TEXT_WIDTH width, CG_R_TEXT_PAINT draw) with scale =
 * cg_centerPrintCharWidth/32; a NULL fade
 * (expired) clears cg_centerPrintTime/Priority. No arguments; void return; cdecl RET.
 * The .mcode size-guess "script_method_player_setclientcvar" (a server GSC method)
 * is rejected — this is client HUD draw code with no cvar/script interaction.
 * Source: 0x300191b0..0x30019361. */
void CG_DrawCenterString(void);

/*
 * Q_strlwr (0x3004e6e0) — lowercase a NUL-terminated string in place (applying the
 * CRT locale-aware tolower to each byte) and return the same buffer. Proven from
 * the body: per-char CALL 0x3005b84a (tolower); the string pointer arrives in ECX
 * (register argument) and is returned in EAX. Classic idTech q_shared helper.
 * Source: uo_cgame_mp_x86.dll 0x3004e6e0.
 */
char *Q_strlwr(char *s);

/*
 * CG_LatchOverlaySource (0x3001a5b0) — the "begin/seed" half of the animated
 * usable-entity / crosshair-hint overlay: latch the current cg_snap source fields
 * into the persistent overlay-effect state block (0x3048ae08..), gated by the
 * cgame-ready flag and a nonzero source field. Its only caller is the draw half
 * CG_DrawCursorhint (0x300303a0), which immediately consumes the block. Full body
 * in src/client/cgame/hud/cg_latchoverlaysource.c.
 */
void CG_LatchOverlaySource(void);

/*
 * CG_DrawAreaSystemChat / CG_DrawAreaChat (0x30031940 / 0x30031a00) —
 * byte-for-byte twin emitters of cgame trap 54. Each reads word0 and the float
 * sum f_c+f_4 from `obj`, then issues cgame_syscall(CG_R_TEXT_PAINT, word0,
 * <sumBits>, arg2, arg0, arg1, stringBuffer, 0, 0, 0). The differing string
 * buffers correspond to retail UO owner-draw ids CG_AREA_SYSTEMCHAT and
 * CG_AREA_CHAT. The matching same-module macOS symbols establish the function
 * names. The .mcode size-matched guesses are rejected.
 */
void CG_DrawAreaSystemChat(rectDef_t *obj, intptr_t arg0, intptr_t arg1, intptr_t arg2);
void CG_DrawAreaChat(rectDef_t *obj, intptr_t arg0, intptr_t arg1, intptr_t arg2);
/* CG_DrawAreaTeamChat (0x300319a0) — third family member, selected by
 * CG_AREA_TEAMCHAT; same shape, its own buffer. */
void CG_DrawAreaTeamChat(rectDef_t *obj, intptr_t arg0, intptr_t arg1, intptr_t arg2);
/*
 * CG_DrawObituaryLine (0x30031a90) — trap-52-gated member of the trap-54 emitter
 * family. Draws the scoreboard "fragged by" obituary line, horizontally centered on
 * the string's measured pixel width. Early-outs unless cg_fraggedByName[0] is set,
 * formats va("Fragged by %s", cg_fraggedByName), measures the text width via
 * cgame_syscall(CG_R_TEXT_WIDTH, str, regWord, arg0, 0), and emits the family's fixed
 * 10-slot draw: cgame_syscall(CG_R_TEXT_PAINT, <centerX-width/2 bits>, <(f_c+f_4) bits>,
 * regWord, arg0, arg1, str, 0, 0, arg2) where centerX = obj->x + obj->w*0.5f.
 * The object arrives in ESI and one extra forwarded word in EBX (see the .c for the
 * exact register ABI); three cdecl stack words follow. The .mcode size-matched
 * "BG_GetAnimScriptEvent" guess is REJECTED (no anim-script lookup; it is a centered
 * 2D text draw). Exact source name unresolved; named by proven role.
 */
void CG_DrawObituaryLine(rectDef_t *obj, intptr_t regWord, intptr_t arg0, intptr_t arg1, intptr_t arg2);
/*
 * CG_DrawPlayerBarHealthTitle (0x3002fca0) — fourth family member. Reads bits(obj->x) and
 * bits(obj->y) (both raw dwords, not floats here), runs CG_SafeTranslateString_Internal("cgame",
 * "CGAME_HEALTH") and forwards the translated-string pointer as the trap-54 string
 * buffer, then emits cgame_syscall(CG_R_TEXT_PAINT, word0, word4, arg0, arg1, arg2,
 * translated, 0, 0, arg3). The object arrives in ESI; four cdecl stack words follow.
 * The .mcode size-matched "Cmd_Where_f" guess is rejected (this is not a console
 * command handler).
 */
void CG_DrawPlayerBarHealthTitle(rectDef_t *obj, intptr_t arg0, intptr_t arg1, intptr_t arg2, intptr_t arg3);
/*
 * CG_Draw1stPlace (0x30031b60) — fifth family member, structurally the
 * twin of CG_DrawPlayerBarHealthTitle: object in ESI, four cdecl stack words follow,
 * bits(obj->x) and bits(obj->y) read as raw dwords, and the emitted 10-slot vector is
 * cgame_syscall(CG_R_TEXT_PAINT, word0, word4, arg0, arg1, arg2, <string>, 0, 0, arg3).
 * The difference is the string slot: this member formats a parsed integer config
 * value with va("%2i", value) unless that value is still the -9999 "unset" sentinel,
 * in which case it skips the format call entirely (see the .c for the exact
 * dataflow). The .mcode's size-matched "YawToQuaternion" guess is rejected (no
 * quaternion math, no floating point at all). Object arrives in ESI; four cdecl
 * stack words follow.
 */
void CG_Draw1stPlace(rectDef_t *obj, intptr_t arg0, intptr_t arg1, intptr_t arg2, intptr_t arg3);
/*
 * CG_HudEmitIconOrValue (0x30030f10) — iterator-driven CG_R_TEXT_PAINT HUD emit family
 * member that draws ONE per-client HUD element into a rect. It advances/clamps the
 * shared HUD emit cursor (cg_currentSelectedPlayer_vmCvar.integer), maps it through cg_hudEmitClientTable[]
 * to a per-client bgs.clientinfo[] slot, early-outs if state->infoValid==0,
 * then renders either an icon (hIcon!=0: trap_R_SetColor(color); CG_DrawPic of the
 * icon shader stretched to the whole rect; trap_R_SetColor(NULL)) or the client's
 * cached state->health formatted "%i" and drawn horizontally centered in the
 * rect at y=rect.h+rect.y via trap_R_Text_Width (width) + trap_R_Text_Paint (draw). Non-default
 * register ABI: hIcon in ECX, rect in EBX, then four cdecl stack words
 * (color, drawStyleA, drawStyleB, drawColor). Named by proven role (the .mcode
 * size-match guess "script_func_precacheheadicon" is rejected — it precaches
 * nothing). Full body in src/hud/cg_hudemiticonorvalue.c.
 */
void CG_HudEmitIconOrValue(qhandle_t hIcon, const rectDef_t *rect, int32_t drawParamA, int32_t drawParamB, const vec4_t drawColor,
                           int32_t drawParamD);
/*
 * CG_HudEmitDigits (0x30031300) — HUD number element of the CG_R_TEXT_PAINT emit family.
 * Draws one per-client integer into `rect` either as a single icon shader stretched
 * over the whole rect (hIcon != 0: trap_R_SetColor(color); CG_DrawPic; reset color) or,
 * when no icon is supplied, as a left-to-right run of bitmap digit glyphs from
 * cg_numberShaders[] — one trap_R_DrawStretchPic per character, advancing by
 * Q_rint(charScale*20) with glyph height Q_rint(charScale*32), clamped to 3 glyphs and
 * value range [-99, 999]. The displayed integer is
 * cg_snap->ps.stats[STAT_HEALTH] when the
 * element's bound client (cg_clientNum) is the local player, else the
 * cached bgs.clientinfo[client].health (an empty state draws nothing).
 * Non-default register ABI: hIcon in ECX, color in EDX, then two cdecl stack words
 * (rect pointer, charScale float). The .mcode size-match guess "CG_InterpolatePlayerState"
 * is REJECTED (no player-state interpolation; it issues 2D HUD draw traps). Full body in
 * src/client/cgame/hud/cg_hudemitdigits.c.
 */
void CG_HudEmitDigits(qhandle_t hIcon, const vec4_t color, const rectDef_t *rect, float charScale);
/*
 * CG_DrawFieldWidth (0x30017aa0) — measure the width of a fixed-width numeric
 * HUD field: clamp `value` to what fits in `width` digits (width itself clamped to
 * 1..5, <1 returns 0), format it as "%i", count the characters (capped at width),
 * and return count * charWidth. Uses the same per-digit-magnitude clamps as
 * CG_HudEmitDigits. Non-default register ABI proven from the body: width in ECX,
 * then two cdecl stack words (value, charWidth); returns the summed width in EAX.
 * The .mcode size-match guess "script_method_player_setfatigue" is REJECTED (no
 * player/entity/fatigue access; three args, int return, pure string-width math);
 * exact original symbol remains unresolved. Full body in
 * src/client/cgame/hud/cg_drawfieldwidth.c.
 */
int CG_DrawFieldWidth(int width, int value, int charWidth);
/*
 * CG_DrawField (0x30017bc0) — the DRAW counterpart of CG_DrawFieldWidth
 * (its immediate neighbor 0x30017aa0). Clamps `value` to what fits in `width` digits
 * (width itself clamped 1..5; width < 1 returns 0), formats it as "%i", then blits each
 * character left-to-right as one bitmap glyph from cg_numberShaders[] via
 * trap_R_DrawStretchPic (cgame trap 0x49 == CG_R_DRAWSTRETCHPIC), scaling every coord
 * by cgs_screenXScale/cgs_screenYScale. `justify`==0 right/center-adjusts the start pen
 * (pen -= 2 + glyphs*charWidth); `drawGlyphs`==0 lays out without emitting pics. Returns
 * the justify-adjusted starting X pen. Non-default ABI proven from the body: width in
 * ECX, then seven cdecl stack words (x, y, value, charWidth, charHeight, drawGlyphs,
 * justify); dispatched through a HUD-element method table (no direct call site visible),
 * like the width sibling. The .mcode size-match guess "PM_CmdScale" is REJECTED — the
 * real PM_CmdScale is 0x30008690; this draws a HUD numeric field. Exact name
 * anchored by the same-module Mac CG_DrawField symbol. Full body in
 * src/client/cgame/hud/cg_drawfield.c.
 */
int CG_DrawField(int width, int x, int y, int value, int charWidth, int charHeight, int drawGlyphs, int justify);
/*
 * The -9999 "unset/invalid" sentinel of the parsed integer config value that
 * CG_Draw1stPlace and the score painters test. Proven by CMP EAX,0xffffd8f1 at
 * 0x30031b6d
 * (and the identical gate at 0x30031526). Exact source name of the enclosing
 * subsystem unresolved; named by its proven sentinel role.
 */
enum {
    CG_SCORE_VALUE_UNSET = -9999
};
/*
 * CG_Draw2ndPlace (0x30031bd0) — fifth family member, same 10-slot CG_R_TEXT_PAINT
 * shape as CG_DrawPlayerBarHealthTitle: reads bits(obj->x) and bits(obj->y) (both raw dwords),
 * forwards three caller words, a string buffer, two zero words, then a fourth caller
 * word in the final slot. It differs by (a) gating on the system-info HUD stat
 * cg_hudStat6Value: when that stat equals its -9999 sentinel the whole emit is
 * skipped and the function returns; and (b) deriving the string argument from
 * va("%2i", cg_hudStat6Value) rather than a translated/static buffer. Emits
 * cgame_syscall(CG_R_TEXT_PAINT, word0, word4, arg0, arg1, arg2, va("%2i", stat), 0, 0, arg3).
 * The .mcode size-matched "PitchToQuaternion" guess is REJECTED (no quaternion math,
 * no x87 here; it is a trap-54 emitter).
 */
void CG_Draw2ndPlace(rectDef_t *obj, intptr_t arg0, intptr_t arg1, intptr_t arg2, intptr_t arg3);
/*
 * CG_DrawPlayerLocation (0x30031280) — local-player member of the CG_R_TEXT_PAINT emitter
 * family. It fetches the local player's per-client anim/player state
 * (bgs.clientinfo[cg_snap->ps.psClientNum]); if that state is valid (infoValid
 * != 0) it looks up the current location string via CG_GetTranslatedLocationString and
 * emits cgame_syscall(CG_R_TEXT_PAINT, bits(obj->x), <bits of obj->h + obj->y>, arg0,
 * arg1, arg2, hintString, 0, 0, arg3). The object arrives in ESI; four cdecl stack
 * words follow. The .mcode size-matched "PM_WaterEvents" guess is rejected (this
 * makes a single trap-54 call and no pmove/water bookkeeping).
 */
void CG_DrawPlayerLocation(rectDef_t *obj, intptr_t arg0, intptr_t arg1, intptr_t arg2, intptr_t arg3);
/*
 * CG_EmitLocalTeamBackground (0x300316b0) — local-player member of the same HUD emit
 * family as CG_DrawPlayerLocation. It fetches the local player's per-client anim/player
 * state (bgs.clientinfo[cg_snap->ps.psClientNum]); if that state is valid (infoValid
 * != 0) it draws a team-colored background bar behind the HUD element by calling
 * CG_DrawTeamBackground(ps->team, rect->x, rect->y, rect->w, rect->h, color[3]).
 * The rect/object arrives in ECX (a rectDef_t holding four floats x,y,w,h) and one
 * cdecl stack word — the owner-draw color vector whose +0xc float is the alpha.
 * The .mcode size-matched "CMD_VEH_MakeVehicleUsable" guess is REJECTED: this function
 * makes no vehicle-state write and issues no command; it reads the local player state and
 * forwards a rect + team to CG_DrawTeamBackground (a 2D draw helper). Provisional name by
 * proven role; exact CoD symbol unresolved.
 */
void CG_EmitLocalTeamBackground(rectDef_t *rect, const vec4_t color);
/*
 * CG_DrawSelectedPlayerName (0x30031020) — iterator-driven member of the CG_R_TEXT_PAINT
 * HUD emit family. Advances/clamps the shared HUD emit cursor (cg_currentSelectedPlayer_vmCvar.integer) to
 * [0, cg_hudEmitCount), maps it through cg_hudEmitClientTable[] to a per-client
 * bgs.clientinfo[] index, early-outs if that state's infoValid == 0, then
 * emits cgame_syscall(CG_R_TEXT_PAINT, bits(obj->x), <bits of obj->h + obj->y>, arg0, arg1, arg2,
 * state->name, 0, 0, arg3). Object arrives in ECX; four cdecl stack words follow.
 * Differs from the config-string sibling 0x300311f0 in that the "string" slot is a
 * pointer into the iterated player's state (+0xc) rather than a config string. The .mcode
 * size-matched "trap_XAnimSetCompleteGoalWeightKnobAll" guess is REJECTED (single
 * trap-54 call, no XAnim work). Retail UO and the macOS owner-draw jump table
 * establish the exact name.
 */
void CG_DrawSelectedPlayerName(rectDef_t *obj, intptr_t arg0, intptr_t arg1, intptr_t arg2, intptr_t arg3);
/*
 * CG_DrawSelectedPlayerLocation (0x300311f0) — exact sibling of CG_DrawSelectedPlayerName
 * (0x30031020): shares the HUD emit cursor/table preamble
 * (state = &bgs.clientinfo[cg_hudEmitClientTable[clamp(cg_currentSelectedPlayer_vmCvar.integer)]], early-out
 * when state->infoValid == 0), but DIVERGES in the emitted vector — it forwards
 * bits(obj->x) and resolves the string slot through
 * CG_GetTranslatedLocationString(state->location) instead of pushing state->name.
 * Its body from the float load onward is byte-identical to CG_DrawPlayerLocation
 * (0x30031280). Emits cgame_syscall(CG_R_TEXT_PAINT, bits(obj->x), <bits of obj->h + obj->y>,
 * arg0, arg1, arg2, hintString, 0, 0, arg3). Object arrives in ESI; four cdecl stack words
 * follow. The .mcode size-matched "G_EntLinkToWithOffset" guess is REJECTED (single trap-54
 * call, no entity linking). Retail UO and the macOS owner-draw jump table
 * establish the exact name.
 */
void CG_DrawSelectedPlayerLocation(rectDef_t *obj, intptr_t arg0, intptr_t arg1, intptr_t arg2, intptr_t arg3);
/*
 * CG_DrawRedScore (0x30031510) / CG_DrawBlueScore (0x300315e0) — the two score
 * display members of the CG_R_TEXT_PAINT emitter family. Retail UO assigns their
 * dispatcher ids the exact names CG_RED_SCORE and CG_BLUE_SCORE, and the same-module
 * macOS symbols establish the function names. Each formats one parallel score
 * integer into a 16-byte buffer (Com_sprintf "%i", or "-" when the value holds the
 * -9999 CG_SCORE_VALUE_UNSET sentinel), measures/keys it with
 * cgame_syscall(CG_R_TEXT_WIDTH, buf, arg0, arg1, 0), then emits
 * cgame_syscall(CG_R_TEXT_PAINT, bits((obj->w+obj->x)-(float)trap52), bits(obj->h+obj->y),
 * arg0, arg1, arg2, buf, 0, 0, arg3). The object arrives in EBX (register); four cdecl
 * stack words follow. The two are structural twins differing only in the score slot
 * they read — CG_DrawRedScore reads cg_hudStat5Value and CG_DrawBlueScore reads
 * cg_hudStat6Value (and the blue-score variant additionally carries an MSVC /GS
 * stack-cookie prologue/epilogue). The .mcode size-matched guesses (MenuParse_itemDef /
 * Script_Orbit) are REJECTED — neither parses menus nor does camera math; both are trap-54
 * score emitters.
 */
void CG_DrawRedScore(rectDef_t *obj, intptr_t arg0, intptr_t arg1, intptr_t arg2, intptr_t arg3);
void CG_DrawBlueScore(rectDef_t *obj, intptr_t arg0, intptr_t arg1, intptr_t arg2, intptr_t arg3);
/*
 * CG_DrawGameType (0x30031c50) — member of the CG_R_TEXT_PAINT emitter family that
 * forwards the parsed serverinfo gametype string. Reads bits(obj->x) (raw dword) and the
 * float sum obj->h + obj->y exactly like the area-chat trio, but takes FOUR cdecl
 * stack words (arg3 lands in the trailing slot, like the Translated/Formatted siblings)
 * and pushes the shared cgs_gametype[] serverinfo mirror (0x30447abc) as the string
 * slot. Emits cgame_syscall(CG_R_TEXT_PAINT, bits(obj->x), <bits of obj->h + obj->y>,
 * arg0, arg1, arg2, cgs_gametype, 0, 0, arg3). Object arrives in EAX (register); four
 * cdecl stack words follow (plain RET, caller-cleaned). The .mcode size-matched
 * "script_method_scriptbuiltin_getattachsize" guess is REJECTED — no script/attach
 * work; it is a fixed trap-54 argument vector. The mechanical owner label on 0x30447abc
 * (same getattach first-touch guess) is already rejected in globals.h. Exact source
 * Same-module macOS symbols identify this function as CG_DrawGameType and the
 * adjacent fixed-string accessor at 0x30031c40 as CG_GameTypeString.
 */
const char *CG_GameTypeString(void);
void CG_DrawGameType(rectDef_t *obj, intptr_t arg0, intptr_t arg1, intptr_t arg2, intptr_t arg3);
/*
 * CG_ConfigStringHint (0x300310b0) — PROVISIONAL caller-observed decl (caller-observed
 * ABI only; superseded by this callee's own .mcode reconstruction). A config-string
 * accessor: it adds a fixed reserved base of 53 to the passed index, range-clamps the
 * result to [0, 2048) (indices outside collapse to slot 0), then returns
 * &cg_gameState.stringData[cg_gameState.stringOffsets[index + 53]], falling back to the
 * literal "CGAME_UNKNOWN" when the resolved string is empty. Named by its proven
 * behavior and the nearby hint strings ("CGAME_UNKNOWN"/"KEY_USE"/"Hint String");
 * exact source symbol unresolved. Arg arrives in EAX (register), result in EAX.
 */
const char *CG_ConfigStringHint(int32_t index);

/* consoleCommand_t and cg_consoleCommands[] are declared in globals.h (included
 * above), where the table's storage lives. */

/*
 * CG_ConsoleCommand (0x300178c0) — dispatch a console command issued to the
 * cgame module. Reads argv[0] via trap_Argv, scans cg_consoleCommands for a
 * case-insensitive name match (Q_stricmpn), runs the matched handler if present,
 * and returns qtrue if handled, qfalse otherwise. Name from the same-module PPC
 * bank (cgame_mp CG_ConsoleCommand); the .mcode's size-matched
 * script_func_vectordot guess is rejected.
 */
qboolean CG_ConsoleCommand(void);

/*
 * BG_LoadAnimTreeInstances (0x30005c40) — create a per-entity anim-tree instance
 * from the loaded master "multiplayer" tree for every element of
 * cg_playerAnimTrees[64] and the eight local records at
 * cg_corpseInfo[i].animTree. Named by role; the .mcode's
 * size-matched "script_method_hudelem_destroy" guess is rejected.
 * The declaration and complete common body are owned by bg_animation.h.
 */

/*
 * CGScr_InitAnimTreeParse (0x30002470) — installs the cgame script/anim-tree load
 * context. Called once by CGScr_LoadAnimTrees as
 *   CGScr_InitAnimTreeParse(bgs.animationTable.entries, &loadCtxB, &loadCtxA)
 * and stores those three pointers into the script-VM globals 0x30134cc8 /
 * 0x300a7820 / 0x300a5108 (which the anim-tree parse/load path then reads), also
 * seeding parse state and copying strings via strcpy. The two out-context arguments
 * are the caller's stack locals whose addresses the callee retains for the duration
 * of the load. First arg is the base of the BG animation table (bgs.animationTable.entries,
 * 0x3053a440).
 * Signature is caller-observed: (void *table, void **ctxB, void **ctxA); no cleanup
 * (cdecl, caller reclaims). Provisional; superseded when 0x30002470 is reconstructed. */
void CGScr_InitAnimTreeParse(void *animTreeTable, void **ctxB, void **ctxA);

/*
 * CGScr_LoadAnimTrees (0x30016360) — top-level cgame script anim-tree loader. See
 * src/client/cgame/animation/anim_tree_load.c for the reconstruction. */
void CGScr_LoadAnimTrees(void);

/*
 * CG_EndShellShockSound (0x3003c0f0) — end the shellshock looping sound. Reconstructed
 * from its own .mcode (see functions/FUN_3003c0f0_3003c16a.c). Always issues the
 * sound reset pair trap(220, fullVolumes[10], 0) then
 * trap(221, "generic", 0, 0); then,
 * if a shellshock sound is still active (cg_shellshockSoundEndTime != 0), registers the
 * "shellshock_end_abort" sound via trap(196) and plays it via trap(198, handle, 1023,
 * vec3_origin, 0), clearing cg_shellshockSoundEndTime to 0. Reached from
 * CG_EndShellShock (0x3003c1d0) and from the per-frame shellshock update (0x3003c247).
 * Name from the same-module PPC bank (cgame_mp!CG_EndShellShockSound) by role; the
 * .mcode header's size-matched "Player_GetMethod" guess is rejected.
 */
void CG_EndShellShockSound(void);

/*
 * CG_EndShellShock (0x3003c1d0) — stop the shellshock screen effect and restore the
 * screen to normal. Calls CG_EndShellShockSound, restores mouse sensitivity
 * (cg_shellshockMouseSensitivityScale = 1.0f) and clears the mouse speed limits
 * via trap(246, 0, 0), then zeroes the screen-blur pair (cg_shellshockScreenBlurX/Y =
 * 0) and pushes the cleared blur via trap(0x57, 0). Name from the same-module PPC
 * bank (cgame_mp!CG_EndShellShock) matched by the proven shellshock-reset role; the
 * .mcode's size-matched "BG_IsCrouchingAnim" guess is rejected (this function
 * returns void and issues cgame screen-state traps, not a crouch-anim predicate).
 */
void CG_EndShellShock(void);

/* snapshot_t and the globals cg_snap / cg_effectTime are declared in
 * globals.h (included above), next to their recovered storage. */

/*
 * CG_SnapshotTransitionStage2 (0x30034d40) — second stage of the snapshot
 * transition, reached by tail JMP from CG_InstallSnapshotResetEffects and called
 * directly from the initial-snapshot install path in CG_ProcessSnapshots. Resets the
 * locally-predicted player state to the freshly installed snapshot and clears the
 * per-life transient client view/weapon/HUD/effect state. Reconstructed (see
 * functions/FUN_30034d40_30034eb2.c): clears the initial-snapshot latch
 * (g_data_..._304831bc), memcpy's the 0x1141-dword embedded playerState (cg_snap+0xc)
 * into cg.predictedPlayerState (0x304831c4), caches cg_currentWeaponInfo =
 * bg_weaponInfos[predicted currentWeapon] and cg_weaponSelectTime = cg.time, zeroes the
 * transient global scatter (usable-hint, fade-overlay, damage-flash/kick, view-kick
 * vel+angles, ADS scratch, special-tag scratch, the cg_shakeSources[4] table, the
 * ADS-view-error latch), and re-seeds trap_Cvar_Set for cg_weaponSelect =
 * va("%i", cg_snap->ps.currentWeapon), cl_stance = "0", cl_run = "1". No stack argument,
 * no return value. The size-matched .mcode guess "CG_AddScaleFade" is rejected. */
void CG_SnapshotTransitionStage2(void);

/*
 * CG_InstallSnapshotResetEffects (0x3003c9d0) — install `snap` as cg_snap, mirror
 * its serverTime into the client time bases, free every effect-pool slot, issue
 * cgame trap 0xdb (1.0f), and tail-call CG_SnapshotTransitionStage2. Named by
 * proven role; the size-matched .mcode guess "DebugDumpAnims" is rejected. */
void CG_InstallSnapshotResetEffects(snapshot_t *snap);

/*
 * CG_ReadNextSnapshot (0x3003d220) — read the next engine snapshot into the free one
 * of the two cg_activeSnapshots double-buffer slots and return it, or NULL when no
 * newer snapshot is (yet) available. Reconstructed (see the C artifact): warns via
 * Com_PrintMessage when cg.latestSnapshotNum runs more than 1000 ahead of
 * cg.processedSnapshotNum; then, while processedSnapshotNum < latestSnapshotNum, it
 * INCs processedSnapshotNum and calls trap_GetSnapshot (CG_GET_SNAPSHOT) into the
 * slot not currently held by cg_snap. On a failed read it records a dropped snapshot
 * in the lagometer (the inlined NULL path of CG_AddLagometerSnapshotInfo) and keeps
 * looping; on success it logs the snapshot in the lagometer and returns the slot.
 * Named by the embedded warning string "WARNING: CG_ReadNextSnapshot: way out of
 * range, %i > %i"; the size-matched .mcode guess "KeywordHash_Key" is rejected. */
snapshot_t *CG_ReadNextSnapshot(void);

/*
 * CG_AddLagometerSnapshotInfo (0x30018a40) — record one snapshot in the lagometer
 * snapshot ring cg_lagometer. When `snap` is NULL it stores a dropped-snapshot marker
 * (snapshotSamples[snapshotCount & (LAG_SAMPLES-1)] = -1) and bumps snapshotCount;
 * otherwise it stores snap->ping (snap+0x4) into snapshotSamples and snap->snapFlags
 * (snap+0x0) into snapshotFlags at that index, then bumps snapshotCount. Field access
 * widths proven from the .mcode (all 4-byte). CG_ReadNextSnapshot inlines only the
 * NULL branch and calls this for the success case. Reconstructed (see the C artifact).
 * The size-matched guess "Vector4Scale" is rejected (it walks the lagometer ring with
 * no x87 float ops, not a vector scale). */
void CG_AddLagometerSnapshotInfo(snapshot_t *snap);

/*
 * CG_SetNextSnap (0x3003cc10) — install `snap` as cg_nextSnap, issue the per-frame
 * DObj begin-frame trap (CG_DOBJ_INVALIDATE_SKELS), recompute cg.frameInterpolation for
 * the (cg_snap, snap) pair, and transition all snapshot entities into the current
 * frame. Reconstructed from its own bytes only for the facts CG_ProcessSnapshots
 * depends on: it takes one stack argument (the incoming snapshot_t *), stores it
 * into cg_nextSnap (0x30459164), and returns void (caller-cleaned single dword,
 * ADD ESP,4 at every call site). Provisional caller-observed declaration — arity
 * (1)/return(void) proven at the call sites in CG_ProcessSnapshots; the rest of the
 * body (entity transition / interpolation weight) is to be superseded by its own
 * .mcode reconstruction. The PPC name bank lists a same-module CG_SetNextSnap; the
 * store-cg_nextSnap + DObj-begin-frame + interp behavior matches, adopted by role,
 * not by size. */
void CG_SetNextSnap(snapshot_t *snap);

/*
 * CG_ResetSnapshotEntityEffects (0x3003ca30) — no-argument, void helper called by
 * CG_ProcessSnapshots after each snapshot transition. It reads the current snapshot
 * entity count/list off cg_snap (cg_snap+0x4510 count, cg_snap+0x4518 list) and
 * clears cg_entities[index].currentValid (index*0x288 + 0x3048c8c8),
 * i.e. it re-seeds the per-entity state for the newly transitioned
 * snapshot. Provisional caller-observed declaration: no stack argument and void
 * return are proven at the three call sites in CG_ProcessSnapshots (0x3003d39d,
 * 0x3003d3f6, 0x3003d43e; plain CALL, no cleanup). Role name from its proven body
 * (entity reset keyed by cg_snap's entities); exact CoD source name unresolved,
 * to be superseded by its own .mcode reconstruction. */
void CG_ResetSnapshotEntityEffects(void);

/*
 * CG_SetFrameInterpolation (0x3001f710) — recompute cg.frameInterpolation, the
 * [0,1) lerp weight between cg_snap and cg_nextSnap. It is the standalone form of
 * the same math CG_SetNextSnap inlines: frac = (cg.time - cg_snap->serverTime) /
 * (cg_nextSnap->serverTime - cg_snap->serverTime), stored to cg_frameInterpolation
 * (0x304831a8) and clamped to 0 when the snapshot span is 0 or the result is
 * negative (the FCOMP against the 0.0f pool constant at 0x3007bcec). No argument,
 * void return (RET, 8-byte local frame). Provisional caller-observed declaration:
 * arity(0)/return(void) proven at the CG_ProcessSnapshots call site (0x3003d3af,
 * plain CALL); role name proven by its sole write of cg_frameInterpolation. */
void CG_SetFrameInterpolation(void);
void CG_ProcessSnapshots(void);

/* CG_DrawActiveFrame (0x30042160) — vmMain command 4 frame driver. The first
 * four source arguments are stack-passed by vmMain; demoPlayback arrives in EDX
 * and drawFrame in ECX under the DLL's mixed register ABI. */
void CG_DrawActiveFrame(int32_t serverTime, int32_t stereoView, qboolean demoPlayback, int32_t lockedViewFace, int32_t lockedViewSize,
                        qboolean drawFrame);

void CGAME_ABI_CDECL dllEntry(cgame_syscall_t systemCall);
void CG_OffsetThirdPersonView(void);

/* Core view-refdef builder cluster. CG_CalcViewValues (0x30041a30) dispatches
 * these no-argument helpers. CG_CalcTurretViewValues and
 * CG_OffsetFirstPersonView are caller-observed declarations whose names are
 * supported by their view-origin/angle behavior and the same-module PPC bank;
 * their full signatures are superseded when their own ranges are reconstructed. */
void CG_CalcVrect(void);
void CG_BuildLockedViewRefdef(void);
qboolean CG_CalcViewProjection(void);
void CG_CalcTurretViewValues(void);
void CG_OffsetFirstPersonView(void);
qboolean CG_CalcViewValues(void);

intptr_t CGAME_ABI_CDECL vmMain(int32_t command, intptr_t arg0, intptr_t arg1, intptr_t arg2, intptr_t arg3, intptr_t arg4, intptr_t arg5,
                                intptr_t arg6, intptr_t arg7, intptr_t arg8, intptr_t arg9, intptr_t arg10, intptr_t arg11);
/*
 * trap_R_DrawStretchPic (0x3003e0f0) — cgame trap id 73, the 2D stretch-pic draw.
 * Forwards its nine 32-bit stack slots (x, y, w, h, s1, t1, s2, t2 as float bit
 * patterns, then the shader handle) unchanged to the engine via cgame_syscall
 * with command CG_R_DRAWSTRETCHPIC. The arguments are kept as opaque 32-bit words
 * so the wrapper's bit-exact forwarding through the variadic trap is preserved
 * (typing the coordinates as float would force a double promotion the machine code
 * does not do). Service proven by the CG_DrawPic call site (0x3001caa0). */
int32_t trap_R_DrawStretchPic(int32_t x, int32_t y, int32_t w, int32_t h, int32_t s1, int32_t t1, int32_t s2, int32_t t2, int32_t hShader);

/* NOT_FROM_ORIGINAL_SOURCE: defined float bit-copy adapters for the recovered
 * host build. They preserve the raw dword transfers used by the i386 VM/trap
 * ABI without strict-aliasing violations or an integer/float conversion. */
#if defined(_MSC_VER)
#define CG_RECOVERY_ALWAYS_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define CG_RECOVERY_ALWAYS_INLINE inline __attribute__((always_inline))
#else
#define CG_RECOVERY_ALWAYS_INLINE inline
#endif

static CG_RECOVERY_ALWAYS_INLINE int32_t CG_FloatBits(float f)
{
    int32_t bits;
    memcpy(&bits, &f, sizeof(bits));
    return bits;
}

static CG_RECOVERY_ALWAYS_INLINE float CG_FloatFromBits(uint32_t bits)
{
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

/* NOT_FROM_ORIGINAL_SOURCE: native-host spelling of the retail client's
 * unchecked clientInfo row calculation. The original PE32 instruction forms a
 * modulo-2^32 byte displacement and adds it to the table base. A normal
 * `bgs.clientinfo[index]` expression would make the preserved stock
 * out-of-range path undefined in C; -O0 does not change that language rule.
 * Native rows deliberately use their host sizeof so widened pointer fields
 * remain valid on 64-bit builds. */
static CG_RECOVERY_ALWAYS_INLINE clientInfo_t *cgame_compat_unchecked_clientinfo(clientInfo_t *base, int32_t client)
{
    const uint32_t offsetBits = (uint32_t)client * (uint32_t)sizeof(clientInfo_t);
    const intptr_t displacement = (intptr_t)coduo_int32_from_bits(offsetBits);
    const uintptr_t address = (uintptr_t)(void *)base + (uintptr_t)displacement;
    return (clientInfo_t *)address;
}

/* NOT_FROM_ORIGINAL_SOURCE: native-host spelling for an original target dword
 * table read whose unchecked index is intentionally preserved. The PE32 effective
 * address uses a modulo-2^32 scaled offset; converting that offset back to its
 * signed displacement before native address addition preserves negative/corrupt
 * inputs too, while the integer-address boundary prevents the host compiler from
 * assuming the index is within the declared C array. */
static CG_RECOVERY_ALWAYS_INLINE int32_t cgame_compat_read_target_i32_index(const int32_t *base, int32_t index)
{
    uint32_t offsetBits = (uint32_t)index * 4u;
    intptr_t displacement = (intptr_t)coduo_int32_from_bits(offsetBits);
    uintptr_t address = (uintptr_t)(const void *)base + (uintptr_t)displacement;
    int32_t value;
    memcpy(&value, (const void *)address, sizeof(value));
    return value;
}

#undef CG_RECOVERY_ALWAYS_INLINE

/*
 * The compiler's x87 float->int conversion idiom used pervasively by the
 * fade/debug draw-element cluster (0x3001af10..0x3001b15f): a tiny epsilon (the
 * double 2^-30 = 9.313225746154785e-10, stored at .rdata 0x3007be50) is added to
 * a float before FISTP rounds the sum to the nearest integer under the default
 * x87 rounding mode. The epsilon is far below one ULP of the screen-coordinate
 * constants these callers use, so the results equal the plain integer values; the
 * runtime computation is reproduced faithfully rather than folded to constants.
 * Shared here so the sibling emitters (CG_DrawDebugFadeElement 0x3001b070,
 * CG_DrawFixedFadeElement 0x3001b0f0, CG_DrawScoreboardFadeElement 0x3001afd0)
 * reuse one definition instead of each carrying a divergent file-local copy.
 */
#define CG_FTOL_EPSILON 9.313225746154785e-10 /* double 2^-30, .rdata 0x3007be50 */

static inline int32_t CG_RoundToNearest(float coord)
{
    /* FLD float coord; FADD double eps; FISTP dword -> round-to-nearest int. */
    long double sum = (long double)coord + (long double)(double)CG_FTOL_EPSILON;
    return coduo_x87_fistp_i32(sum);
}

/*
 * CG_DrawPic (0x3001caa0) — draw shader `hShader` as a 2D stretch-pic in the
 * virtual 640x480 UI space. Scales (x,width) by cgs.screenXScale and (y,height) by
 * cgs.screenYScale (the CG_AdjustFrom640 transform, inlined here), fixes the
 * texture coordinates to the full image (s1,t1,s2,t2 = 0,0,1,1), and forwards to
 * trap_R_DrawStretchPic. Same-module PPC bank confirms cgame_mp!CG_DrawPic; the
 * .mcode's size-matched "script_func_getfullclipammo" guess is rejected. */
void CG_DrawPic(float x, float y, float width, float height, qhandle_t hShader);

/*
 * CG_DrawStretchPic (0x3001cb00) — draw shader `hShader` as a 2D stretch-pic in the
 * virtual 640x480 UI space, with caller-supplied texture coordinates. Same as
 * CG_DrawPic except the four texcoords (s1,t1,s2,t2) are passed in rather than pinned
 * to (0,0,1,1): scales (x,width) by cgs.screenXScale and (y,height) by
 * cgs.screenYScale (the CG_AdjustFrom640 transform, inlined here), forwards the raw
 * texcoord dwords unchanged, and hands the 9-dword frame to trap_R_DrawStretchPic
 * (trap 73). The shader handle is a register argument (EAX / __usercall) on the real
 * i386 build — the caller does `mov eax,ebx` immediately before the CALL and cleans
 * only the 8 stack dwords (ADD ESP,0x20); modeled here as the trailing `hShader`
 * parameter. The size-matched `.mcode` guess `PM_GetSlowdownFriction` is rejected:
 * this issues the 2D stretch-pic draw trap and reads cgs.screenXScale/screenYScale,
 * touching no playerState or friction state. */
void CG_DrawStretchPic(float x, float y, float width, float height, float s1, float t1, float s2, float t2, qhandle_t hShader);

/*
 * CG_DrawTeamBackground (0x30017dd0) — draw a solid team-colored 2D background bar.
 * Selects a team color (team 1 = red (1,0,0,alpha), team 2 = blue (0,0,1,alpha);
 * any other team draws nothing), sets the 2D draw color (trap_R_SetColor / trap 72),
 * draws the cgs.media hudColorBar shader (0x3044b6c0) as a full-image stretch-pic
 * scaled from virtual 640x480 to real screen via cgs.screenXScale/screenYScale,
 * then resets the draw color to white. On the i386 __usercall build `team` arrives
 * in EAX and (x,y,w,h,alpha) on the stack; modeled here in argument order with team
 * first. Name from the same-module (cgame_mp) PPC bank (CG_DrawTeamBackground) plus
 * the hudColorBar shader + trap-72/73 draw behavior; the .mcode size guess
 * "CG_InitVote" is rejected. Full reconstruction in
 * src/hud/cg_drawteambackground.c. */
void CG_DrawTeamBackground(int32_t team, float x, float y, float width, float height, float alpha);

/*
 * CG_EmitTrap54Draw (0x3001cf10) — emit one fixed-shape 2D HUD draw through cgame
 * trap 54. Forwards (x, y+14.0f, style=4, scale=1/3, whiteColor[4], handle,
 * size=16.0f, 0, mode=3) to cgame_syscall; floats go through by raw dword. A
 * different trap-54 emitter than the string-buffer family (see CG_R_TEXT_PAINT). The
 * fourth argument occupies whiteColor[3] and is consumed as text alpha.
 * Role name; exact CoD symbol unresolved (trap-54 service not yet identified).
 * The .mcode size-matched guess "LerpAngle" is rejected (this issues a syscall). */
void CG_EmitTrap54Draw(int32_t x, float y, int32_t handle, float alpha);

/*
 * CG_DrawDisconnect (0x30018a90) — draw the "connection interrupted" HUD warning
 * (centered CGAME_CONNECTIONINTERRUPTED text via trap 54, plus the blinking
 * gfx/2d/net.tga icon via CG_DrawPic) when the newest generated usercmd has not been
 * acknowledged by a snapshot in a plausible time window. Gates on
 * trap_GetCurrentCmdNumber (0x53) / trap_GetUserCmd (0x54) vs cg_snap->ps.commandTime
 * and cg_time; blinks the icon on cg_time bit 0x200. No args, void. Reconstructed
 * from its own .mcode (src/client/cgame/hud/cg_drawdisconnect.c); the mechanical
 * size-guess name `script_method_player_setreverb` is rejected (a cgame HUD draw,
 * not a script reverb setter — proven by its net.tga / CGAME_CONNECTIONINTERRUPTED
 * data and the owner=cg_drawdisconnect label the export gave the adjacent bytedirs
 * table). */
void CG_DrawDisconnect(void);

/*
 * CG_DrawSpectatorMessage (0x3001b720) — draw the horizontally-centered
 * "CGAME_SPECTATOR" HUD message. Resolves the localized text via
 * CG_SafeTranslateString_Internal("cgame","CGAME_SPECTATOR"), measures its width through cgame
 * trap 52 (CG_R_TEXT_WIDTH) at scale 1/3, computes x = (640.0f - width) * 0.5f, then
 * draws it in white at y = 443.0f through cgame trap 54 (CG_R_TEXT_PAINT) using the same
 * 10-argument 2D-draw shape as CG_EmitTrap54Draw. Takes no arguments. Role name;
 * exact CoD symbol unresolved. The .mcode size-matched guess
 * "script_method_player_getguid" is rejected (no GUID, not a script method). */
void CG_DrawSpectatorMessage(void);

/*
 * CG_Trap54DrawElement (0x3001cff0) — fixed-arity cgame 2D-draw emitter that
 * issues cgame trap 54 (CG_R_TEXT_PAINT) with one text/element draw frame. It draws a
 * single element at screen x = `position` (a float bit pattern forwarded opaquely),
 * vertical position `yBase + 14.0f`, style 5, scale 1/3, opaque-white color, the
 * `data` string/element pointer, size 8.0f, and a trailing `flags` dword. Two
 * callers build all four args and clean the stack (caller-cleaned/cdecl):
 * CG_DrawViewInfoOverlay (0x3001b2b0) and the sibling at 0x30018090, each drawing
 * three of these per frame at evenly spaced y positions. Full reconstruction lives
 * in src/hud/draw_small_string.c. */
void CG_Trap54DrawElement(int position, float yBase, void *data, int flags);

/*
 * trap_R_SetColor (cgame trap id 72) — set the color modulation applied to the
 * following 2D stretch-pic draws. `rgba` is a pointer to a float[4] (r,g,b,a); a
 * NULL pointer resets the color to opaque white. The single pointer argument is
 * forwarded unchanged. Service proven by the HUD fill/soft-line draw cluster that
 * brackets its draws with trap(72, color) then trap(72, NULL) (CG_FillRect at
 * 0x3001c4e0, the sibling at 0x3001ca20). Reconstructed as the trap-72 wrapper at
 * 0x3003e0d0 (forwards `rgba` and command 0x48 to cgame_syscall); full body in
 * src/module/trap_r_setcolor.c.
 */
void trap_R_SetColor(const float *rgba);

/*
 * CG_FillRect (0x3001c4e0) — fill the virtual-640x480 rectangle (x,y,width,height)
 * with `color` (an r,g,b,a float[4]) by setting the draw color (trap 72), drawing
 * the cgs.media hudSoftLine shader as a stretch-pic scaled to real screen
 * coordinates (screenXScale on x/width, screenYScale on y/height), then resetting
 * the draw color to white. Same-module PPC bank lists cgame_mp!CG_FillRect with this
 * (x,y,w,h,color) shape; the .mcode's size-matched "Scr_SetObjectField" guess is
 * rejected. Note the stretch-pic texcoords here are (s1,t1,s2,t2)=(0,0,0,1), matching
 * the hudSoftLine fill shader rather than CG_DrawPic's full (0,0,1,1). */
void CG_FillRect(float x, float y, float width, float height, const float *color);

/*
 * CG_DrawStatBarWithDecay (0x3002f9d0, provisional role name) — draw a 2D HUD stat
 * bar (fill fraction from the current snapshot's playerState, clamped to [0.5,1.0])
 * plus a time-decaying trailing "ghost" segment, via trap_R_SetColor/
 * trap_R_DrawStretchPic. Non-cdecl register-argument convention proven from its one
 * caller (the HUD-element dispatcher at 0x300324e3): color in ESI (mutated in place),
 * rect[4] in EDI, shader handle on the stack. The .mcode header's size-matched
 * `StopFollowing` name is rejected (this is a 2D draw, not a server player routine).
 */
void CG_DrawStatBarWithDecay(float *color, const rectDef_t *rect, int32_t hShader);

/*
 * CG_DrawFlashDamage (0x3001a8e0) — draw the red full-screen "took damage" flash
 * overlay. No-ops unless a snapshot is installed (cg_snap != NULL) and the flash is
 * still active (cg_damageFlashEndTime > cg_time). While active it fills the whole
 * virtual 640x480 screen (rect (-10,-10)..(640,480), a 10px overscan) via CG_FillRect
 * with color (0.2, 0, 0, alpha): a dark red tint. The alpha ramps with the remaining
 * time: alpha = min(fabs((endTime - cg_time) * (1/500) * cg_damageFlashScale), 5.0)
 * * 0.2 * 0.7, so it fades to zero over the 500 ms window and is capped. Same-module
 * PPC bank lists cgame_mp!CG_DrawFlashDamage; the .mcode's size-matched "DynaSink"
 * guess is rejected (this is a CoD HUD draw, not the Windows DynaSink networking API,
 * and it is named by behavior, not size). */
void CG_DrawFlashDamage(void);

/*
 * CG_DrawDamageDirectionIndicators (0x3001a980) — draw the fading "damage direction" arrow
 * ring, the directional companion of CG_DrawFlashDamage's red flash. No-ops unless a
 * snapshot is installed (cg_snap != NULL). It first anchors the ring on screen: if the
 * ADS/scope overlay is active (CG_CalcAdsOverlayFrac != 0) it requires the ADS anchor
 * gate cg_hudDamageIconInScope_vmCvar.integer and projects the aim point to screen
 * (CG_ProjectDamageDirToScreen), offsetting by the (320,240) screen center; otherwise
 * it anchors at plain screen center (320,240) in virtual 640x480 space. It then walks
 * cg_damageDirIndicators[8]; for each slot still within its lifetime
 * (0 < cg.time - serverTime < duration) it computes alpha = min(2 - 2*elapsed/duration,
 * 1) so each arrow fades out over the back half of its window, and draws a rotated icon
 * quad (width cg_hudDamageIconWidth_vmCvar.value, from cg_hudDamageIconOffset_vmCvar.value down cg_hudDamageIconHeight_vmCvar.value)
 * via CG_DrawTurretTagQuad, rotated by vectoyaw(cg_refdef.viewaxis[0]) - slot->yaw so it
 * points at the attacker relative to the current view. The white draw color
 * (1,1,1,alpha) is installed per-arrow with cgame_syscall(CG_R_SETCOLOR, color). The
 * .mcode size-guess name "CG_EjectWeaponBrass" is REJECTED: no 3D shell/entity is
 * spawned; this is a 2D HUD indicator over cg_damageDir* state. Supersedes the
 * provisional caller-observed name CG_DrawScreenBlend. */
void CG_DrawDamageDirectionIndicators(void);

/*
 * CG_ProjectDamageDirToScreen (0x30019370, role name) — project the module's current
 * aim/impact world point (the cached pair at 0x3048b0b8/0x3048b0bc) to 2D screen
 * coordinates, writing screen X to *outX (EDI) and screen Y to *outY (ESI); both are
 * set to 0 when the point is behind the view plane. Uses the view axis rows
 * (cg_refdef.viewaxis[0] and the following two rows) and the view-projection scale pair
 * (cg_viewProjScaleA/B). Register-ABI: outX in EDI, outY in ESI (both callers LEA two
 * adjacent stack floats). Used by CG_DrawDamageDirectionIndicators to anchor the arrow ring at
 * the projected aim point while aiming down sight. Role name (exact CoD symbol
 * unresolved); the .mcode size-guess "CG_ParseImpactEffects" is a size collision. */
void CG_ProjectDamageDirToScreen(float *outX, float *outY);

/*
 * Per-frame screen/HUD draw dispatcher and its draw-step callees, all reached from
 * CG_Draw2D (0x3001bfe0). Each of these is called with NO source
 * arguments (the call sites are bare `CALL`s with no register/stack argument setup
 * and no post-call cleanup) and no consumed return, so the caller-observed ABI is
 * `void f(void)`. These are PROVISIONAL caller-observed declarations — the arity is
 * proven at this call site, but the exact original CoD symbol names below are
 * role-derived (from each callee's cg_time fades, 1.0f color-vector setup, and
 * draw-trap emission seen in its prologue) and are superseded by each callee's own
 * .mcode reconstruction. The address suffix is retained on the still-opaque helpers
 * whose specific role is not yet proven.
 *
 * CG_Draw2D itself: the cgame per-frame screen builder/drawer. It is
 * called once per rendered frame from the frame entry at 0x3001c440 (which first
 * stores the view origin/angles to 0x30487a90.. and runs the world/scene passes),
 * bails early on two render-suppress flags (g_cgScreenSuppressFlag and
 * cg_lockedViewFace) and on the module-ready gate g_cgScreenReadyState,
 * requires cg_draw2D_vmCvar.integer to be set, then dispatches on the local
 * player's movement state cg_snap->ps.pmType (pmType_t): PM_TYPE_INTERMISSION draws
 * the intermission scoreboard, PM_TYPE_SPECTATOR and the alive/playing states draw
 * their own overlays, and dead states share the common tail. The .mcode header's
 * size-matched guess "Fire_Lead" (win size 0x118 == corpus size 0x118) is REJECTED:
 * this function fires no weapon and computes no bullet lead — it is a HUD/scene
 * draw dispatcher keyed on pmType. The Mac cgame CG_Draw2D body has the identical
 * set of all 14 named direct callees, resolving the source name. The mechanical
 * exporter owner labels near 0x3005d8xx were only first-toucher guesses.
 */
void CG_Draw2D(void);
void CG_TileClear(
    void); /* 0x3001d160: repaint the four letterbox edges outside the cropped 3D view (src/client/cgame/hud/cg_tileclear.c) */
void CG_UpdateScreenFade(void); /* 0x3001ab90: cg_time-driven full-screen fade update */
void CG_DrawScreenFadeOverlay(void); /* 0x3001a7c0: cg_time-driven fade overlay, always run before return on any non-suppressed frame */
void CG_DrawDamageDirectionIndicators(
    void); /* 0x3001a980: draw the fading damage-direction arrow ring (src/client/cgame/hud/cg_drawdamagedirectionindicators.c). Supersedes the provisional caller-observed name CG_DrawScreenBlend: the body proves damage arrows, not a color-vector screen blend. */
qboolean CG_DrawIntermission(
    void); /* 0x3001bd20: forces the scoreboard on for the intermission screen — cgame_syscall(CG_MAP_RESTART_RESET_RENDERER), latches cg_scoreboardShowTime=cg_time, sets cg_scoreboardShowing=qtrue, then TAIL-CALLs CG_DrawScoreboard (its qboolean is this function's return). src/client/cgame/hud/cg_drawintermission.c */
void CG_DrawSpectatorHud(void); /* 0x3001a610: spectator-mode HUD block */
void CG_DrawTeamInfo(
    void); /* 0x30018770: draw the team-chat scroll ring (up to cg_chatHeight_vmCvar.integer lines of teamChatMsgs[], each with a dim hudColorBar background bar and the ^N-colored text, fading out over the cg_chatTime_vmCvar.integer window). src/client/cgame/hud/cg_drawteaminfo.c. Supersedes the size-guess names Reached_BinaryMover and the caller-observed CG_DrawWarmup. */
void CG_DrawCrosshair(void); /* 0x30019cf0: crosshair and weapon-reticle renderer */
void CG_DrawVote(void); /* 0x3001b7d0: complaint/vote HUD */
void CG_VoiceMenuTimeout(void); /* 0x3001ab00: close voiceMenu after 2500 ms */
void CG_DrawHudElems(void); /* 0x3002a4a0: gather+draw active hudElem_t list (calls CG_GetSortedHudElems) */
void CG_DrawWeaponSelect(void); /* 0x30046bb0: animated weapon-selection carousel */
void CG_DrawWeaponSelectKeyHint(const vec4_t color, int32_t slot, float x, float y);
void CG_DrawWeaponIcon3D(void);
void CG_DrawCrosshairNames(
    void); /* 0x3001a610: reconstructed — draws the faded, health-tinted name of the player under the crosshair (was mis-guessed CG_DrawPlayerWeaponNameBack); src/client/cgame/hud/cg_drawcrosshairnames.c */
void CG_DrawMatchTimeout(
    void); /* 0x3001bbd0: reconstructed — draws the screen-centered match-timeout/"CGAME_PAUSED" overlay (was mis-guessed CG_DrawWeaponSelect) */
void CG_DrawLagometer(void); /* 0x30018bc0: frame/snapshot lag graphs + disconnect warning */
void CG_DrawSlidingFadeElement(void); /* 0x3001af10: sliding fade HUD element (trap 0x1a) */
void CG_DrawScoreboardFadeElement(void); /* 0x3001afd0: fixed (320,150) fade HUD element */
void CG_DrawDebugFadeElement(void); /* 0x3001b070: (2,4) debug fade HUD element */
void CG_DrawFixedFadeElement(void); /* 0x3001b0f0: (135,425) fixed fade HUD element */
void CG_DrawTimerHud(void); /* 0x3001b170: trap(6)-timer HUD block */
void CG_DrawInfoScreens(void); /* 0x3001b360: per-frame developer info-overlay
                                      * dispatcher. Reads three cvar integer enable-flags
                                      * in priority order and tail-jumps to the first
                                      * enabled overlay (A=0x3001acc0, B=0x30017e90
                                      * script-VM debug, C=CG_DrawViewInfoOverlay), else
                                      * returns. Reconstructed in
                                      * FUN_3001b360_3001b38b.c. Supersedes the wrong
                                      * size-guessed names Cmd_Fogswitch_f and the earlier
                                      * provisional CG_DrawScoresHud (this issues no draw
                                      * of its own and reads no scores). */
/* Provisional callee decls for CG_DrawInfoScreens's tail-jump targets. arity/types
 * caller-observed as zero-arg void (each is entered by JMP with no arg setup) — verify
 * when each is reconstructed from its own .mcode. */
void CG_DrawSoundOverlay(void); /* 0x3001acc0: overlay-A — the Miles Sound System
                                      * channel debug overlay (trap 222 query + per-channel
                                      * 2D text). Reconstructed in FUN_3001acc0_3001af0e.c;
                                      * supersedes the provisional CG_DrawInfoOverlayA and
                                      * the wrong size-guess Info_SetValueForKey. */
/* CG_EmitTrap54DrawScaled (0x3001ce40) — fixed-arity CG_R_TEXT_PAINT 2D-text/draw emitter
 * with a custom register+stack ABI, reconstructed in FUN_3001ce40_3001cf06.c. Called
 * by CG_DrawSoundOverlay for the header and each channel line. modeFlag!=0 -> mode 3;
 * adjustFlag==0 -> CG_AdjustFrom640 rescale of x/width/y by the screen scales;
 * color NULL -> local white {1,1,1,1}. */
void CG_EmitTrap54DrawScaled(int modeFlag, int adjustFlag, const vec_t *color, float x, float y, void *handle, float width, float height,
                             int32_t extra);
void CG_DrawScriptVmDebugOverlay(void); /* 0x30017e90: overlay-B, script-VM debug
                                         * ("threads: %d" / "vars: %d") */
void CG_DrawViewInfoOverlay(void); /* 0x3001b2b0: overlay-C, draws info text elements
                                      * via CG_Trap54DrawElement */
void CG_DrawWeaponStance(void); /* 0x30018730: weapon/stance HUD indicator */
void CG_DrawSpawnOverlay(void); /* 0x300191b0: just-spawned large overlay frame */
void CG_ResetScreenFadeA(void); /* 0x3001bd50: 1.0f color-vector fade reset A */
void CG_ResetScreenFadeB(void); /* 0x3001bee0: 1.0f color-vector fade reset B, reads cg_snap */

/*
 * CG_TileClearBox (0x3001d0d0) — draw one axis-aligned 2D stretch-pic segment
 * whose texture coordinates come from integer pixel positions / 64 (a 64-texel
 * tiling step, the .rdata constant 0x3c800000 at 0x3007befc). Converts arg0..arg3
 * (int) to float, scales the texcoord-carrying values by 1/64, and forwards a
 * 9-dword frame to trap_R_DrawStretchPic (trap 73). The shader handle is a register
 * argument (EAX / __usercall) on the real i386 build; modeled here as the leading
 * `hShader` parameter. Name resolved: the sole caller CG_TileClear (0x3001d160)
 * matches Quake3 CG_TileClear (full-screen skip + four letterbox-edge tile draws),
 * and the same-module PPC bank lists cgame_mp!CG_TileClearBox as CG_TileClear's
 * helper; the role and the PPC name agree. The earlier role name
 * "CG_DrawTiledPicSegment" and the .mcode size guess "PM_ShouldMakeFootsteps" are
 * both superseded (this issues the 2D draw trap and touches no playerState). Note:
 * the shader dword lands in the trap's t2 positional slot and a float in its hShader
 * slot — an anomaly to reconcile in a trap-73 re-audit, not in these bytes. */
int32_t CG_TileClearBox(int32_t hShader, int32_t arg0, int32_t arg1, int32_t arg2, int32_t arg3);

/*
 * CG_FilledBar flag bits (in EBX at the CG_FilledBar call boundary). All bits are
 * now named from CG_FilledBar's own reconstructed body (FUN_3001c5d0_3001c855.c),
 * which tests each one; exact original source names remain unresolved (named by
 * proven role). Every value below is proven from a TEST at the cited address.
 */
enum {
    /* Bar orientation. If set, the bar's filled length varies along the height
     * (a VERTICAL bar: FillRect height = height*frac); if clear, along the width
     * (a HORIZONTAL bar: FillRect width = width*frac). Proven at 0x3001c74a
     * (TEST BL,0x4). */
    CG_FILLEDBAR_VERTICAL = 0x04,
    /* If set, CG_FilledBar skips the cg_hudAlpha_vmCvar.value multiply applied to the bar
     * colors' alpha. Proven at 0x3001c639 (TEST BL,0x8; JNZ skips the fade block).
     * (The scale is the global HUD fade factor, not a fixed 0.5x.) */
    CG_FILLEDBAR_NO_ALPHA_FADE = 0x08,
    /* If set (and the ECX color pointer is non-NULL), CG_FilledBar draws a full-rect
     * background fill first, using the working fill color overridden from the ECX
     * float[4] instead of the stack border color. Proven at 0x3001c5ef (AND EAX,0x10),
     * the ECX!=NULL guard at 0x3001c61a, and the background FillRect gated at
     * 0x3001c6c9. */
    CG_FILLEDBAR_FILLCOLOR = 0x10,
    /* Anchor the filled span at the far (bottom/right) end: the filled origin is
     * offset by (1-frac)*length before drawing. Proven at 0x3001c74f / 0x3001c7c9
     * (TEST BL,0x1). */
    CG_FILLEDBAR_ANCHOR_END = 0x01,
    /* Anchor the filled span centered: origin offset by (1-frac)*length*0.5. Proven
     * at 0x3001c764 / 0x3001c7de (TEST BL,0x2). Lower priority than ANCHOR_END. */
    CG_FILLEDBAR_ANCHOR_CENTER = 0x02,
    /* When the background fill is drawn (FILLCOLOR set), inset the fill rect's top
     * and bottom by 3 virtual pixels (y+=3, height-=6) instead of the default 1px
     * all-around inset. Proven at 0x3001c6f3 (TEST BL,0x20). */
    CG_FILLEDBAR_INSET_VERT = 0x20,
    /* When the background fill is drawn (FILLCOLOR set), skip the fill-rect inset
     * entirely. Proven at 0x3001c6ee (TEST BL,0x40; JNZ skips both inset paths). */
    CG_FILLEDBAR_NO_INSET = 0x40,
    /* Draw the filled span in a frac-blended color: color = lerp(borderColor, color3,
     * frac), component-wise, instead of the plain border color. Requires the color3
     * (EDX) pointer. Proven at 0x3001c66a (AND ESI,0x100) and the blend at
     * 0x3001c674..0x3001c6c5. */
    CG_FILLEDBAR_BLEND_COLOR3 = 0x100
};

/*
 * CG_FilledBar (0x3001c5d0) — draw a filled/partitioned 2D bar in the virtual
 * 640x480 UI space by issuing one or more CG_FillRect draws (0x3001c4e0). The bar's
 * fill fraction `frac` sizes the filled span of the rect (x,y,width,height);
 * `flags` selects orientation (CG_FILLEDBAR_VERTICAL), an optional background fill
 * (CG_FILLEDBAR_FILLCOLOR), the anchoring of the filled span
 * (CG_FILLEDBAR_ANCHOR_END/ANCHOR_CENTER), the color source
 * (CG_FILLEDBAR_BLEND_COLOR3), and whether the cg_hudAlpha_vmCvar.value alpha fade is
 * applied (CG_FILLEDBAR_NO_ALPHA_FADE). `fillColor` (register ECX) overrides the
 * background-fill color when CG_FILLEDBAR_FILLCOLOR is set; `color3` (register EDX)
 * is the optional blend target for CG_FILLEDBAR_BLEND_COLOR3 and its alpha is faded
 * in place when the fade applies; `borderColor` (stack, dereferenced first) is the
 * base color[4] used for the filled span. ABI now PROVEN from the reconstructed
 * body (FUN_3001c5d0_3001c855.c): flags in EBX, fillColor in ECX, color3 in EDX,
 * then (x, y, width, height, borderColor, frac) on the stack; caller-cleaned
 * (ADD ESP,0x18). Same-module PPC bank lists cgame_mp!CG_FilledBar with this shape.
 * The .mcode's size-matched "CG_CalcMuzzlePoint" guess is rejected: this function
 * draws a 2D bar via CG_FillRect, it computes no muzzle point.
 */
void CG_FilledBar(int flags, const float *fillColor, float *color3, float x, float y, float width, float height, const float *borderColor,
                  float frac);

/*
 * CG_DrawFilledBarStyled (0x3001c860) — draw a styled filled progress bar via
 * CG_FilledBar with fixed colors (gray fill / white border, both alpha 0.3) and
 * flags CG_FILLEDBAR_FILLCOLOR|CG_FILLEDBAR_NO_ALPHA_FADE, forwarding the caller's
 * rect and completion fraction. Named by role from its lone caller (0x3001c120,
 * a loading/download progress screen); exact original symbol unresolved. The
 * .mcode's size-matched "BG_GetSpeed" guess is rejected. */
void CG_DrawFilledBarStyled(float x, float y, float width, float height, float frac);

/*
 * CG_DrawTopBottom (0x3001c980) and CG_DrawSides (0x3001c8e0) — the two thin-bar
 * halves of a HUD rectangle border. Each takes (x, y, width, height, size) in
 * virtual 640x480 space, applies the CG_AdjustFrom640 transform (screenXScale on
 * x/width, screenYScale on y/height), and draws two hudSoftLine stretch-pics: the
 * pair that make up the top+bottom bars (CG_DrawTopBottom) or the left+right bars
 * (CG_DrawSides). The current draw color must already be set by the caller (they do
 * not touch trap_R_SetColor). Same-module PPC bank lists cgame_mp!CG_DrawSides; the
 * split into top/bottom vs sides matches stock Q3 CG_DrawRect.
 * CG_DrawSides is reconstructed (FUN_3001c8e0_3001c97d.c): it draws the left bar at
 * the scaled left edge and the right bar at (x+width-size)*screenXScale, each `size`
 * wide, full scaled height, via two hudSoftLine trap_R_DrawStretchPic calls — the
 * 5-float/void/cdecl ABI is proven there. CG_DrawTopBottom is reconstructed
 * (FUN_3001c980_3001ca1d.c): the mirror of CG_DrawSides on the Y axis — it draws the
 * top bar along the scaled top edge and the bottom bar at (y+height-size)*screenYScale,
 * each spanning the full scaled width and `size` tall, via two hudSoftLine
 * trap_R_DrawStretchPic calls; the same 5-float/void/cdecl ABI is proven there.
 */
void CG_DrawTopBottom(float x, float y, float width, float height, float size);
void CG_DrawSides(float x, float y, float width, float height, float size);

/*
 * CG_DrawRect (0x3001ca20) — draw the outline of the virtual-640x480 rectangle
 * (x,y,width,height) with border thickness `size` in `color` (an r,g,b,a float[4]).
 * Sets the 2D draw color to (color[0], color[1], color[2], cg_hudAlpha_vmCvar.value*color[3])
 * via trap_R_SetColor (the alpha is scaled by the global HUD fade factor; r,g,b are
 * passed through unchanged), draws the top/bottom bars then the left/right bars via
 * CG_DrawTopBottom and CG_DrawSides, then resets the draw color to white with
 * trap_R_SetColor(NULL). This is the sibling of CG_FillRect referenced in the
 * trap_R_SetColor evidence. Same-module PPC bank lists cgame_mp!CG_DrawRect; the
 * .mcode's size-matched "AnglesSubtract" guess is rejected (this function draws a
 * bordered rect, it does not subtract angles).
 */
void CG_DrawRect(float x, float y, float width, float height, float size, const float *color);

/*
 * CG_DrawStretchPicColor (0x30032050) — draw shader `hShader` as a 2D stretch-pic
 * filling the virtual-640x480 rectangle `rect` (a float[4] = {x,y,width,height})
 * in `color` (an r,g,b,a float[4]). Sets the 2D draw color via trap_R_SetColor
 * (trap 72), draws the shader scaled from virtual to real screen coordinates
 * (screenXScale on x/width, screenYScale on y/height) with full-image texcoords
 * (0,0,1,1) via trap_R_DrawStretchPic, then resets the draw color to white with
 * trap_R_SetColor(NULL). The rect-pointer + explicit-color sibling of CG_FillRect
 * (0x3001c4e0). Register-arg ABI: `rect` arrives in EAX, `hShader` and `color`
 * are stack args (proven by the sole caller 0x30032639). The .mcode's size-matched
 * "BG_GetHorizontalBobFactor" guess is rejected (this draws; it is not a bob-factor
 * float helper). Role name from behavior; exact CoD symbol unproven (alternative
 * candidate cgame_mp!Script_SetColor not adopted). */
void CG_DrawStretchPicColor(const rectDef_t *rect, qhandle_t hShader, const float *color);

int32_t trap_R_DrawQuadPic(int32_t arg0, int32_t arg1, int32_t arg2, int32_t arg3, int32_t arg4, int32_t arg5, int32_t arg6, int32_t arg7,
                           int32_t arg8, int32_t arg9);
/* Wrapper for cgame trap id 0x8d. Arguments arg1, arg2 and arg6 are forwarded as
 * 16-bit zero-extended values (the machine code reads them with MOVZX word);
 * the remaining arguments are forwarded as full 32-bit dwords. */
/* Exact same-module wrapper for cgame trap 0x8b (0x3003e780). */
void trap_XAnimClearTreeGoalWeightsStrict(XAnimTree *tree, uint32_t animIndex, float blendTime);

int32_t trap_XAnimSetCompleteGoalWeightKnobAll(XAnimTree *tree, uint32_t anim, uint32_t knob, float weight, float blendTime, float rate,
                                               uint16_t notifyName, qboolean restart);

/* Wrapper for cgame trap id 0x8f (0x3003e890). Forwards
 * (0x8f, arg0, (uint16_t)arg1, arg2, arg3, arg4, (uint16_t)arg5, arg6); the
 * machine code narrows arg1 and arg5 to their low 16 bits (MOVZX word) inside the
 * wrapper before forwarding, so the C interface takes full int32 args and the
 * narrowing is a body-level cast. Structurally identical to trap_XAnimSetCompleteGoalWeight (the
 * adjacent trap 0x90 wrapper); the same XAnim goal-weight trap
 * (CG_XANIM_SET_GOAL_WEIGHT) is also emitted directly by CG_StartWeaponAnim (0x30042ac0).
 * Reconstructed at src/client/cgame/module/trap_xanimsetgoalweight.c. */
int32_t trap_XAnimSetGoalWeight(XAnimTree *tree, uint32_t anim, float weight, float blendTime, float rate, uint16_t notifyName,
                                qboolean restart);

/* Wrapper for cgame trap id 0x90 (0x3003e8e0). Forwards
 * (0x90, arg0, (uint16_t)arg1, arg2, arg3, arg4, (uint16_t)arg5, arg6); the
 * machine code narrows arg1 and arg5 to their low 16 bits (MOVZX word) inside the
 * wrapper before forwarding, so the C interface takes full int32 args and the
 * narrowing is a body-level cast. Reconstructed at
 * src/client/cgame/module/trap_xanimsetcompletegoalweight.c. */
void trap_XAnimSetCompleteGoalWeight(XAnimTree *tree, uint32_t anim, float weight, float blendTime, float rate, uint16_t notifyName,
                                     qboolean restart);

/* Wrapper for cgame trap id 0x95 (0x3003e9d0). Fastcall-style: arg1 arrives in
 * ECX, arg2 is a single caller-cleaned 16-bit stack arg (MOVZX word before the
 * push). Forwards (0x95, arg1, (uint16_t)arg2) to cgame_syscall and returns the
 * int result reinterpreted bit-for-bit as a float in ST(0). CoDUOMP.exe command
 * 149 and the Mac symbol prove this is XAnimGetTime; the sole Windows caller at
 * 0x30003c76 FSTPs the result. */
float trap_XAnimGetTime(XAnimTree *tree, uint16_t animIndex);

/* Wrapper for cgame trap id 0x96 (0x3003e9f0). Fastcall-style: arg1 in ECX, one
 * caller-cleaned 16-bit stack arg (MOVZX word before the push). Forwards
 * (0x96, arg1, (uint16_t)arg2) and returns the int result reinterpreted bit-for-bit
 * as a float in ST(0). CoDUOMP.exe command 150 and the Mac symbol prove this is
 * XAnimGetWeight. Reconstructed in module/trap_xanimgetweight.c. */
float trap_XAnimGetWeight(XAnimTree *tree, uint16_t animIndex);

/* Wrapper for cgame trap id 0xf9 (0x3003d530). No user arguments: pushes only the
 * trap id, calls cgame_syscall, and returns the engine-owned timestamp string.
 * Both consumers push the result as va's `%s` argument; the native declaration
 * therefore retains a 64-bit engine pointer. */
const char *trap_DateTimeStamp(void);

/* Wrapper for cgame trap id 0xf1 (0x3003f2d0). The sole caller parses four
 * floats, passes the first three by address in EDX and the fourth as raw float
 * bits on the stack. */
int32_t trap_FX_SetWind(const vec3_t direction, float intensity);

/* Wrapper for cgame trap id 0xb9 (0x3003ede0). Takes one packed 32-bit stack arg
 * and forwards (0xb9, packed>>16, (uint16_t)packed), returning the int32 result.
 * Reconstructed at functions/FUN_3003ede0_3003edfd.c. */
int32_t trap_XAnimGetNumChildren(uint32_t packed);

/* Wrapper for cgame trap id 0xba (0x3003ee00). Takes one packed 32-bit stack arg
 * plus a register arg (EAX) and forwards (0xba, packed>>16, (uint16_t)packed, extra);
 * the 16-bit trap result (AX) is spliced over the low 16 bits of `packed` and the
 * reconstituted dword is returned. Reconstructed at functions/FUN_3003ee00_3003ee27.c. */
int32_t trap_XAnimGetChildAt(uint32_t packed, int32_t extra);

/*
 * Q_tolower (0x3005b84a) — locale-aware lowercase fold of a single character.
 * A thin wrapper: it fetches the current locale's ctype descriptor and calls the
 * table-driven folder at 0x3005b782, whose ASCII fast path is exactly
 * `if ('A' <= c && c <= 'Z') c += 'a' - 'A';` (0x3005b836). Takes one (signed)
 * char argument on the stack, returns the folded char in AL (modeled as int).
 * Callers (BG_StringHashValue 0x300011b0 and BG_InitWeaponStrings
 * 0x30001500) sign-extend the input byte and consume only the low returned byte.
 * Name is provisional-by-role (standard tolower behavior); exact source symbol
 * unproven. Source: uo_cgame_mp_x86.dll 0x3005b84a..0x3005b86c. Portable callers
 * use coduo_crt_tolower explicitly.
 */

/*
 * coduo_crt_stricmp (0x30069275) — cdecl locale-aware, case-insensitive
 * full-string compare used by the cgame. It is a distinct CRT operation, not
 * the qcommon Q_stricmp wrapper. Portable callers use coduo_crt_stricmp
 * explicitly. Source: uo_cgame_mp_x86.dll 0x30069275.
 */

void CG_WeaponInfoSetString(char *dest, const char *value);

/*
 * ParseConfigStringToStruct (0x3004f1a0) — parse an info-string ("\key\value\...")
 * into a weaponInfo_t-shaped record using the shared parseField_t table. For each
 * of `fieldCount` descriptors it looks up field->key in `info`
 * (Info_ValueForKey); if the value is non-empty it dispatches on field->type,
 * storing the converted value at (weaponInfoBase + field->offset). Types 0..7 are
 * handled inline (string copies, Q_atoi, bool-normalize, atof, seconds->ms via
 * Q_rint); types 8..14 forward to customParser with a bounds check against
 * customTypeLimit, and any other type fatally errors via BG_AnimParseError(ERR_DROP,
 * "Bad field type %i"). Returns qtrue iff every field was processed (the loop
 * completed without a customParser abort). Provisional name matches the
 * same-module PPC symbol BG_ParseWeaponInfoSpecificFieldType by behavior (a
 * weapon-info field parser dispatching on field type), not by size. Nonstandard
 * convention: field table in EDX, fieldCount/weaponInfoBase and remaining args on
 * the stack. The shared implementation normalizes those source arguments to the
 * Mac/Linux order while retaining the proved call behavior. */

/*
 * CG_AllocWeaponInfo (0x3000fe00) — allocate and default-initialize a weaponInfo_t
 * for weapon-definition parsing. Allocates a 0x4bc-byte weaponInfo_t via
 * cgame_syscall(CG_HUNK_ALLOC_LOW_ALIGN, sizeof), registers it in bg_weaponInfos[index],
 * sets weaponIndex = index, sets name = CopyString(""), then walks the field
 * descriptor table (`fields`, `fieldCount` entries) setting every string-typed
 * (type 0) field to the shared empty string. Returns the new weaponInfo_t. The
 * .mcode size-guess name Scr_PlayerKilled is rejected: behavior is weapon-info
 * allocation, not a script kill callback. Provisional role name; the exact
 * original symbol is unproven (no cgame symbol table recovered).
 * Nonstandard convention: fieldCount in EAX, index in ECX, fields on the stack.
 */
weaponInfo_t *CG_AllocWeaponInfo(int32_t fieldCount, int32_t index, const parseField_t *fields);

/*
 * CG_CopyString (0x3000fd90) — duplicate a NUL-terminated string into an
 * engine-allocated buffer. Empty source returns the cg_emptyString singleton
 * (0x300a7e34); otherwise strlen(src), cgame_syscall(CG_HUNK_ALLOC_LOW_ALIGN_EXPLICIT, len+1, 1),
 * then strcpy. The result is stored through *out and also returned. The .mcode
 * size-guess name BG_GetAnimString is rejected: BG_GetAnimString is a two-int
 * animation-table lookup (server_name_bank.txt), not a string-duplicate helper.
 * Provisional role name (this is the general CopyString variant; CG_AllocWeaponInfo
 * uses an inlined 1-byte-copy sibling). Nonstandard convention: src in EDI, out in
 * EBX; plain RET.
 */
char *CG_CopyString(const char *src, char **out);

#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
_Static_assert(offsetof(parseField_t, offset) == 0x04, "parseField_t offset");
_Static_assert(offsetof(parseField_t, type) == 0x08, "parseField_t type");
#endif

/*
 * CG_SettleViewOriginZ (0x3003f9a0) — reconstructed in
 * functions/FUN_3003f9a0_3003f9e5.c. A void(void) helper called at the tail of the
 * view-kick evaluator FUN_3003fb60 (0x3003ff59): it subtracts a time-decaying
 * amount from the view-origin Z (cg_refdef.vieworg[2]) over a fixed 100 ms settle
 * window, weight (100 - elapsed)/100 * cg_weaponChangeViewOffset * 0.01f,
 * using the cg.time stamp cg_weaponChangeViewOffsetTime (re-baselined on a
 * negative delta). Does nothing once the 100 ms window has elapsed. The .mcode
 * size-guess name Cmd_PrevVehSlot_f is rejected (no argc/argv, no Cmd_* calls, no
 * vehicle state); behavioral name, exact CoD symbol unproven.
 */
void CG_SettleViewOriginZ(void);

/*
 * CG_UpdateViewKick (0x3003f9f0) — advance the persistent first-person view-kick
 * spring. Iterates the per-frame elapsed time cg_frametime in <=5ms substeps
 * (dt = min(remaining,5) * 0.001s). For each of the three components of the
 * angular offset cg_viewKickAngles and its velocity cg_viewKickVel:
 *   - integrate a centering acceleration into the velocity that pushes the offset
 *     back toward zero. The acceleration magnitude is 2400.0 when no weapon is
 *     selected (cg_predictedPlayerState.currentWeapon == 0), else the current weapon's
 *     adsViewKickCenterSpeed (cg_predictedPlayerState.adsFraction >= 0.5) or
 *     hipViewKickCenterSpeed (< 0.5);
 *   - integrate velocity into the offset, applying a 0.06 damping factor to the
 *     step when it moves the offset toward center;
 *   - reset both offset and velocity to 0 on a zero crossing or when the offset
 *     lands exactly on 0, and clamp the offset to +/-10.0 degrees (zeroing the
 *     velocity) when it exceeds that magnitude.
 * Returns nothing; operates entirely on the cg_viewKick* globals. The .mcode
 * size-guess name CG_FlameAdjustSpeed is rejected: this touches no flame/effect
 * state — it is the weapon view-kick centering integrator, proven by the
 * weaponInfo_t::{ads,hip}ViewKickCenterSpeed fields (+0x404/+0x44c) it selects and
 * the +/-10-degree angular clamp. Provisional name from that proven role.
 */
void CG_UpdateViewKick(void);

/* gitem_t and the shared bg_itemlist[] definition table are declared in
 * globals.h (included above), next to bg_weaponInfos. */

/* The common BG weapon-table setup passes are declared by bg_weapon.h and
 * implemented in src/bg/bg_weapon_setup.c. */

/*
 * The weapon-info registration subsystem entry point and its worker passes, all
 * driven by InitWeaponInfo (0x30010df0). Signatures are caller-observed from
 * the InitWeaponInfo call sites (all take no arguments and return nothing;
 * plain near CALLs, no stack cleanup); exact CoD symbol names are provisional
 * where noted.
 *
 * BG_ParseWeaponInfoFiles (0x3000fee0): parse a token array of weapon names and populate
 * the bg_weaponInfos[] pointer array / per-weapon records. Called with
 * (const char **argv, int argc). The .mcode size-guess name PM_SetMovementDir is
 * rejected (this function allocates and parses weapon info, no movement math).
 */
void BG_ParseWeaponInfoFiles(const char **argv, int argc);
/*
 * InitWeaponInfo (0x30010df0) — the weapon-info registration entry point.
 * Reconstructed in functions/FUN_30010df0_30010f65.c. Allocates the bg_weaponInfos
 * pointer array via CG_GET_WEAPON_INFO_MEMORY, resets the ammo/clip/shared dedup tables (index 0
 * == "none"), parses the CS_WEAPONS config string into weapon names, registers them
 * via BG_ParseWeaponInfoFiles, then runs the derived-table passes.
 */
void InitWeaponInfo(void);

/*
 * BG_GetTotalAmmoReserve (0x30011800) — shared in
 * src/bg/bg_weapon_inventory.c. The total ammo the player holds
 * usable through weapon `weapon`: it walks bg_weaponInfos[1..bg_numWeapons] and
 * sums, for every weapon the player owns (playerState.weaponBits at ps+0x534) that
 * shares this weapon's shared-ammo pool (weaponInfo_t+0x200), the corresponding
 * playerState.ammo[] (ps+0x134, weaponInfo_t.ammoIndex +0x1e8) and playerState.clips[]
 * (ps+0x334, weaponInfo_t.clipIndex +0x1f0) entries, deduped per ammo/clip index. It
 * has two fast exits (weaponInfo_t.sharedAmmoCapIndex +0x200 < 0) that return a single
 * held ammo[] or clips[] entry directly. Returns the aggregate count in EAX. It is
 * the ADDING twin of BG_GetMaxPickupableAmmo (0x300116b0): same skeleton, but this
 * accumulates held ammo and consults no capacity table.
 *
 * Register ABI (proven from the machine code and the sole HUD ammo-draw call site at
 * 0x30030d14): the weapon index arrives in EAX; the playerState base pointer
 * (&cg_predictedPlayerState, 0x304831c4) is pushed as one cdecl stack arg (caller
 * does `ADD ESP,4`). The .mcode size-guess "BG_GetStackSlotForWeapon" is rejected:
 * that name belongs to the distinct routine at 0x30011590 (see weaponInfo_t.stackable
 * above); this routine at 0x30011800 sums held ammo, it does not resolve a stack slot.
 * `predictedPlayerState` is typed as playerState_t because offsets
 * +0x134/+0x334/+0x534 prove the layout. */

/* cgWeaponInfo_t and its array cg_weaponInfos (0x30413580) are defined in
 * globals.h (included above), next to bg_numWeapons / bg_weaponInfos. */

/*
 * CG_RegisterWeaponInfo — provisional, caller-observed ABI only. FUN_30044890
 * (0x30044890). Called by CG_RefreshWeaponInfosForConfigString for each weapon
 * whose cached name differs from the new config string. The weapon index arrives
 * in ECX (1-based) and the config string is pushed on the stack (caller does
 * `ADD ESP,4` after the call: one 32-bit stack slot, __fastcall-style single
 * register + one stack arg). It re-registers/formats weapon `index` from
 * `configString` into cg_weaponInfos[index] (and consults bg_weaponInfos[index]).
 * MUST be superseded by the callee's own .mcode reconstruction — do not treat
 * this signature as authoritative. */
void CG_RegisterWeaponInfo(int index, const char *configString);

/*
 * CG_SelectWeaponIndex (0x30047390) — commit a weapon selection: snapshot the
 * game time, write the "cg_weaponSelect" cvar to `weapon`, and (unless the
 * request merely re-selects the current weapon's alt-weapon) assert the "cl_run"
 * cvar. Reconstructed in functions/FUN_30047390_300473f6.c.
 *
 * Non-default register ABI (proven from all five call sites, e.g. 0x30047820
 * sets EAX=weaponIndex, ECX=cg_weaponSelect_vmCvar.integer before the plain CALL): the
 * requested weapon index arrives in EAX and the current weapon index in ECX,
 * with no stack arguments. Expressed as (weapon, currentWeapon) to match the
 * reconstructed definition. */
void CG_SelectWeaponIndex(int32_t weapon, int32_t currentWeapon);

/*
 * CG_IsWeaponHeld (0x30047370) — predicate returning whether weapon `weapon` is
 * currently held by the local player, i.e. the value of that weapon's bit in the
 * cg_predictedPlayerState.weaponBits (0x304836f8) ownership bitset (word = weapon>>5, mask =
 * 1u<<(weapon&31)). The one-line factored form of the inline held-bit test the
 * weapon-select scanners perform. Reconstructed in functions/FUN_30047370_3004738d.c.
 *
 * Non-default register ABI: the weapon index arrives in EDX (compiler-chosen
 * register argument in this binary; no ECX/stack input). Expressed as a single
 * int parameter to match the reconstructed definition. */
qboolean CG_IsWeaponHeld(int weapon);

/*
 * CG_SelectFirstWeaponInSlot (0x30047820) and
 * CG_SelectFirstWeaponNotInSlot (0x300478a0) are the two reconstructed
 * leaf scanners used by CG_CycleWeap. Their exact Mac symbols and caller
 * positions are backed by their complete bodies. */
qboolean CG_SelectFirstWeaponInSlot(int32_t forward, int32_t requireAmmo);
qboolean CG_SelectFirstWeaponNotInSlot(int32_t forward, int32_t requireAmmo);

/*
 * CG_CycleWeap (0x30047960) — reconstructed in
 * functions/FUN_30047960_30047bd3.c. Circular weapon-selection scanner: when the
 * current weapon is in a numbered inventory slot it scans neighboring slots first
 * and then the held loose-weapon list; when the current weapon is not slotted, it
 * scans held loose weapons first and then slots. Selections are committed through
 * CG_SelectWeaponIndex; the final reconcile path re-stamps cg_weaponSelect when
 * cg_weaponSelect_vmCvar.integer is no longer held.
 *
 * ABI: two caller-cleaned stack arguments, no register args:
 *   forward      nonzero -> next/forward scan, zero -> previous/backward scan
 *   requireAmmo  nonzero -> require clip+reserve ammo before selecting
 * Callers ignore the return value; the reconstructed source returns void. */
void CG_CycleWeap(int32_t forward, int32_t requireAmmo);

/*
 * CG_OutOfAmmoChange (0x300475f0) — switch the local player away from a weapon
 * that has just run out of ammo to the next weapon that still has ammo.
 * RECONSTRUCTED (src/client/cgame/weapons/cg_outofammochange.c). Takes no
 * arguments; the sole caller (0x30022ef9) invokes it with a bare CALL. Name
 * adopted behaviorally from the same-module (cgame_mp.dll) PPC bank; the .mcode
 * size-guess GScr_PrecacheVehicle is rejected (this is weapon-select, not a
 * script/vehicle precache). */
void CG_OutOfAmmoChange(void);

/*
 * CG_SetSelectedWeapon (0x30022660) — commit an auto/initial weapon selection.
 * Reconstructed in functions/FUN_30022660_300226b9.c. Latches the requested
 * weapon index (cg_lastRequestedWeapon) and cg.time snapshots, then, only when
 * bg_itemlist[weaponIndex].type == IT_WEAPON and no weapon is yet
 * selected (cg_weaponSelect_vmCvar.integer == 0), sets trap_Cvar_Set("cg_weaponSelect",
 * va("%i", bg_itemlist[weaponIndex].weapon)). Distinct from CG_SelectWeaponIndex
 * (0x30047390), which commits unconditionally and also asserts cl_run.
 *
 * Non-default register ABI (proven from the sole caller at 0x30022da1: MOV
 * EAX,EBX before a plain CALL): the weapon index arrives in EAX, no stack args.
 * Provisional role name; exact CoD source name unproven. */
void CG_SetSelectedWeapon(int32_t weaponIndex);


/* Menu_PaintAll is shared by ui_runtime.h. */

/* The common menu feeder controls are shared by ui_runtime.h. */
/* Item_Asset_Paint is shared by ui_runtime.h. */
void Script_FadeIn(itemDef_t *item, char **args);
/* The complete menu-script navigation cluster is shared by ui_runtime.h. */
/* The complete visual-property command cluster is shared by ui_runtime.h. */
/* The cvar/exec/play command family is shared by ui_runtime.h. */
/* The common list/automatic-update/response commands and Script_SetFocus are
 * shared by ui_runtime.h. */

/* The common display mouse/cursor/key dispatch is shared by ui_runtime.h. */

/* Item_Paint is shared by ui_runtime.h. */

/* The complete Item_Text* layout/paint runtime is shared by ui_runtime.h. */

/* The owner-draw item runtime is shared by ui_runtime.h. */

/* LerpColor is shared by ui_runtime.h. */

/* The common item/menu keyword tables and parser dispatch are shared by
 * ui_parse.h. */

/* Item_PostParse and Menu_PostParse are shared by ui_parse.h. */


/*
 * Com_Printf (0x3002b420) — unconditional variadic diagnostic printer, (const
 * char *format, ...), caller-cleaned. Reconstructed from its machine code (see
 * functions/FUN_3002b420_3002b46e.c): snapshots the /GS cookie, formats
 * (format, ...) into a 0x400 stack buffer via vsprintf (0x3005b538), then
 * emits it verbatim through Com_PrintMessage("%s", buffer) (0x3002b2b0 ->
 * cgame_syscall(CG_PRINT, buffer), trap id 0). The adjacent 0x3002b470 is the
 * developer-gated sibling (Com_DPrintf: identical body but guarded on
 * developer_vmCvar.integer [0x3052f8ec] before formatting) and is NOT this symbol.
 * ABI discriminant vs the error sibling Com_Error (0x3002b3d0): this
 * reads the format from arg0 ([ESP+0x408]) and va_starts from [ESP+0x40c], so it
 * has NO leading level argument, whereas Com_Error reads the format one
 * slot further and is (int level, format, ...). The .mcode's size-matched
 * "script_func_isplayernumber" guess is REJECTED (no player/script access here).
 */
void Com_Printf(const char *format, ...);

/*
 * Com_DPrintf (0x3002b470) — developer-gated variadic printer. Reconstructed from
 * its machine code (see functions/FUN_3002b470_3002b4ca.c): snapshots the /GS
 * cookie, reads developer_vmCvar.integer (global 0x3052f8ec) and returns immediately
 * (skipping all formatting and printing) when it is zero; otherwise formats
 * (format, ...) into a 0x400 stack buffer via vsprintf (0x3005b538) and
 * emits it through Com_PrintMessage("%s", buffer) (0x3002b2b0), then runs the
 * __security_check_cookie epilogue. Same (format, ...) caller-cleaned ABI.
 */
void Com_DPrintf(const char *format, ...);

/*
 * Com_PrintMessage (0x3002b2b0) — the low-level variadic print backend that both
 * Com_Printf and Com_DPrintf route through. Proven from its machine code: formats
 * (format, ...) into a 0x400 /GS-guarded stack buffer via vsprintf
 * (0x3005b538), then emits it to the engine via cgame_syscall(0, buffer) through
 * the trap pointer *0x30085e9c (trap id 0 = print), and runs the
 * __security_check_cookie epilogue. Provisional caller-observed ABI; superseded by
 * its own .mcode reconstruction.
 */
void Com_PrintMessage(const char *format, ...);

/* NOTE: 0x30042a30 was provisionally guessed "CG_RefreshWeaponSounds" from the
 * CG_MapRestart call site, but its BODY is reconstructed as CG_StopAllWeaponAnims
 * (FUN_30042a30_30042abe.c) — objdump proves the IMUL 0x1c4 stride + per-node trap-0x8f
 * loop, and the call takes the runtime XAnim tree in EBX. The guessed
 * decl was removed to avoid two names for one address; CG_MapRestart calls
 * CG_StopAllWeaponAnims directly. */

/*
 * CG_ShellShockLoad (0x3003b950) — load and apply a shellshock (.shock)
 * parameter definition file by short name. Reconstructed from its machine code
 * (functions/FUN_3003b950_3003ba04.c): it builds "scripts/<name>.shock" via
 * va (0x3004e8a0), opens it with CG_FS_FOPEN_FILE (mode FS_READ); on a negative
 * length it prints "^1couldn't open '%s'\n" via Com_PrintMessage and returns 0.
 * Otherwise it allocates length+1 bytes (CG_Z_MALLOC_INTERNAL), reads the file
 * (CG_FS_READ), NUL-terminates it, closes it (CG_FS_FCLOSE_FILE), parses the
 * text against the 27 cg_shockParamNames via CG_COM_LOAD_CVARS_FROM_BUFFER, frees the buffer
 * (CG_Z_FREE_INTERNAL), then applies CG_CVAR_UPDATE to each of the 27 cg_shockParamTargets,
 * and returns the CG_COM_LOAD_CVARS_FROM_BUFFER parse result.
 *
 * Non-default register ABI: the name pointer arrives in EAX (the caller does
 * LEA EAX,[name] then a plain CALL; the callee's first PUSH EAX forwards it to
 * va with no stack argument). Expressed as a plain const char * parameter.
 * Returns int (the parse result; 0 on open failure). The size-matched bank name
 * CG_DrawSelectedPlayerName is rejected (contradicted by the "scripts/%s.shock"
 * path string and the cg_shock_* cvar name table).
 */
int CG_ShellShockLoad(const char *name);

/*
 * shellshock_t (124 bytes) — the resolved parameter block for a screen/blur/
 * sound "shellshock" post-effect. Proven a 124-byte object three ways: the
 * console-command trigger CG_ShellShock_f (0x300174b0) treats 0x30448624 as the
 * base of one such block; the scene reader (0x30042160) indexes a table of these
 * with element stride 0x7c (`base + index*0x7c`); and the resolver
 * CG_SetShellShockParams (0x3003ba10) writes fields at offsets up to +0x78, with
 * the next distinct .data global sitting exactly at 0x30448624 + 0x7c.
 *
 * The leading blur fields and the sound block through +0x64 are decoded by
 * CG_ShellShockCalcVibrate and CG_UpdateShellShockSound. The mouse-input tail
 * (offsets +0x68 .. +0x78) is decoded directly by the resolver:
 * enable, fade time, sensitivity scale, maximum pitch speed, and maximum yaw
 * speed. The camera update (0x3003c230) uses many earlier float fields
 * (+0x34 .. +0x64); the blur update (0x3003c630) reads a separate object. Those are
 * not decoded here — only the fields a reconstructed consumer proves are named; the
 * remaining bytes stay reserved. */
typedef struct shellshock_s {
    int32_t blurDivisor; /* +0x00: screen-blur ramp divisor */
    float blurRate; /* +0x04: screen-blur phase rate */
    float blurScale; /* +0x08: screen-blur displacement scale */
    int32_t screenBlendFadeTime; /* +0x0c: screen-blend fade time, ms */
    int32_t screenBlendTime; /* +0x10: screen-blend hold time, ms */
    qboolean soundEnabled; /* +0x14: nonzero => apply sound/reverb effects */
    int32_t soundFadeInTime; /* +0x18: sound fade-in time, ms */
    int32_t soundFadeOutTime; /* +0x1c: sound fade-out time, ms */
    float soundWetLevel; /* +0x20: room/reverb wet level */
    char soundRoomType[16]; /* +0x24: room-type name */
    float soundVolume[SND_ALIAS_CHANNEL_COUNT]; /* +0x34..+0x5b */
    int32_t soundModEndDelay; /* +0x5c: modifier end delay, ms */
    int32_t soundLoopFadeTime; /* +0x60: loop crossfade time, ms */
    int32_t soundLoopEndDelay; /* +0x64: loop end delay, ms */
    qboolean mouseEnabled; /* +0x68: cg_shock_mouse != 0 */
    int32_t mouseFadeTime; /* +0x6c: mouse attenuation fade time, ms */
    float mouseSensitivityScale; /* +0x70 */
    float mouseMaxPitchSpeed; /* +0x74 */
    float mouseMaxYawSpeed; /* +0x78 */
} shellshock_t;
_Static_assert(sizeof(shellshock_t) == 0x7c, "shellshock_t is 124 bytes (0x30448624 .. 0x304486a0)");
_Static_assert(offsetof(shellshock_t, mouseEnabled) == 0x68, "shellshock_t.mouseEnabled @ +0x68");
_Static_assert(offsetof(shellshock_t, mouseFadeTime) == 0x6c, "shellshock_t.mouseFadeTime @ +0x6c");
_Static_assert(offsetof(shellshock_t, mouseSensitivityScale) == 0x70, "shellshock_t.mouseSensitivityScale @ +0x70");
_Static_assert(offsetof(shellshock_t, mouseMaxPitchSpeed) == 0x74, "shellshock_t.mouseMaxPitchSpeed @ +0x74");
_Static_assert(offsetof(shellshock_t, mouseMaxYawSpeed) == 0x78, "shellshock_t.mouseMaxYawSpeed @ +0x78");
_Static_assert(offsetof(shellshock_t, soundEnabled) == 0x14, "shellshock_t.soundEnabled @ +0x14");
_Static_assert(offsetof(shellshock_t, soundRoomType) == 0x24, "shellshock_t.soundRoomType @ +0x24");
_Static_assert(offsetof(shellshock_t, soundVolume) == 0x34, "shellshock_t.soundVolume @ +0x34");
_Static_assert(offsetof(shellshock_t, soundLoopEndDelay) == 0x64, "shellshock_t.soundLoopEndDelay @ +0x64");

/*
 * cg_consoleShellShock (0x30448624, .data) — the single shellshock_t parameter
 * block filled by the "cg_shellshock" console command CG_ShellShock_f
 * (0x300174b0) via CG_SetShellShockParams, then handed to the scene reader
 * (0x30042160) as the active manual shellshock. Storage lives in globals.c.
 * Supersedes the mechanical g_data_concatargs_30448624 (wrong owner/type). */
extern shellshock_t cg_consoleShellShock;

/*
 * CG_SetShellShockParams (0x3003ba10) — resolve the loaded cg_shock_* cvars into
 * a shellshock_t parameter block. Caller-observed ABI: the destination pointer
 * arrives in ESI (CG_ShellShock_f does `MOV ESI,0x30448624` then a plain CALL);
 * the callee writes rounded ms/int fields into [ESI+0x0c .. ESI+0x78] from the
 * cvar float values (each via the seconds*1000 -> int round idiom). Expressed as
 * a plain shellshock_t * parameter. The size-matched bank name SetMoverState is
 * rejected (no mover/entity state here; it reads the cg_shock_* cvar floats and
 * fills the shellshock block). Provisional caller-observed decl — superseded by
 * its own .mcode reconstruction. Register-in-ESI ABI recorded here; expressed as
 * a normal call at the source level. */
void CG_SetShellShockParams(shellshock_t *out);

/*
 * cg_shellShocks[15] (0x304486a0, .data) — config-string indices 1..15. The
 * registration loop derives an index of 1 on its first pass and continues while
 * index < 16, advancing the destination by 0x7c each time; index zero uses the
 * separate cg_consoleShellShock block.
 * for the CS_SHELLSHOCKS config-string block (stride 0x7c). Filled by CG_RegisterGraphics
 * (0x3002ba50) via CG_ShellShockLoad + CG_SetShellShockParams. Declared here (not in
 * globals.h) because shellshock_t is completed in this header; storage in globals.c.
 */
extern shellshock_t cg_shellShocks[15];

/*
 * CG_RegisterGraphics (0x3002ba50) — precache the cgame render/effect media for a
 * map load: HUD/lagometer/hint/stance/objective/headicon/checkbox/backtile/flare
 * shaders (trap_R_RegisterShader), the 2D bitmap-number font, tank/jeep tread and
 * flesh impact effects (trap CG_FX_REGISTER_EFFECT), and the per-map config-string
 * precache of CS_MODELS models, CS_EFFECTS effects, and CS_SHELLSHOCKS shellshocks
 * plus the inline "*N" brush models. Also drives CG_RegisterItems and a second
 * media batch (CG_RegisterGraphics2, 0x30037e90). Prints "Fx System Initialization"
 * banners and periodic "LOADING... %s" progress via the inlined loading-screen
 * updater. No args, no return. /GS-protected. The .mcode size-guess name
 * PM_UpdateViewAngles is REJECTED — this is a cgame media-registration routine, not
 * a pmove view-angle clamp. Resolved by behavior + the registered asset strings.
 */
void CG_RegisterGraphics(void);

/*
 * CG_RegisterGraphics2 (0x30037e90) — provisional, caller-observed. A second cgame
 * media-registration batch invoked once by CG_RegisterGraphics after the inline-model
 * precache. Registers additional 2D shaders ("black", "white", ...) via
 * trap_R_RegisterShader(name, 5) and pumps the loading screen. No args, no return
 * (bare RET). Exact original symbol unproven; named by proven role. Superseded by its
 * own .mcode reconstruction.
 */
void CG_RegisterGraphics2(void);

/*
 * CG_EndShellShockMouse (0x3003c170) — restore the shellshock mouse-sensitivity
 * scale to 1.0f and clear the engine's pitch/yaw speed limits through trap 246.
 * The same-module PPC symbol bank supplies the exact source name. */
void CG_EndShellShockMouse(void);

/*
 * CG_UpdateShellShockMouse (0x3003c530) — advance the shellshock mouse
 * sensitivity envelope and submit maximum pitch/yaw speeds to trap 246. The
 * same-module PPC symbol bank supplies the exact source name. */
void CG_UpdateShellShockMouse(shellshock_t *params, int32_t startTime, int32_t endTime);

/*
 * CG_UpdateShellShockSound (0x3003c230) — advance the shellshock sound/reverb
 * for the current frame. Provisional, caller-observed ABI: the params pointer arrives
 * in EAX (`MOV ESI,EAX`; the callee reads shellshock_t fields +0x14, +0x1c, +0x5c and
 * others via FILD/FIDIV), `elapsed` is the first stack arg and `duration` the second
 * (dispatcher 0x3003c750 does PUSH duration; PUSH elapsed; MOV EAX,params). If the
 * gating field (+0x14) is zero it tail-calls CG_EndShellShockSound and returns. Named
 * by proven role (the sibling of CG_UpdateShellShockMouse/…ScreenBlur);
 * exact source symbol unproven. Register-in-EAX ABI recorded here; expressed as a
 * normal call. Superseded by its own .mcode reconstruction. */
void CG_UpdateShellShockSound(shellshock_t *params, int32_t elapsed, int32_t duration);

/*
 * CG_ShellShockCalcVibrate (0x3003c630) — advance the shellshock screen-blur
 * displacement pair cg_shellshockScreenBlurX/Y (0x3048bff0/0x3048bff4) for the current
 * frame. ABI proven at its sole call site 0x3003c778: EAX=duration, EDX=params,
 * [ESP+0x18]=elapsed. */
void CG_ShellShockCalcVibrate(int32_t duration, const shellshock_t *params, int32_t elapsed);

/*
 * CG_UpdateShellShock (0x3003c750) — per-frame driver for the whole cgame shellshock
 * post-effect. Computes elapsed = cg_time - startTime; if the effect has not started
 * (startTime == 0) or the clock ran backwards (elapsed < 0) it tail-calls
 * CG_EndShellShock, otherwise it fans out the three sub-updates (camera, screen-blend,
 * screen-blur) and issues cgame_syscall(CG_SET_SHELLSHOCK_SCREEN_BLUR, elapsed < duration) with the
 * "still active" flag. Non-default register ABI proven at the sole call site
 * 0x30042541: startTime in EAX, params (shellshock_t*) in EBX, duration in EDI; no
 * stack args. Expressed as a normal 3-arg call. See
 * src/client/cgame/effects/cg_updateshellshock.c. NAMING CONFLICT: globals.c notes also
 * label the caller leg 0x300424e0 CG_UpdateShellShock — distinct function; this address
 * keeps the name its reconstructed siblings (0x3003c530/0x3003c630) already bound. */
void CG_UpdateShellShock(int32_t startTime, shellshock_t *params, int32_t duration);

/*
 * atof (0x3005b969) — parse a leading numeric substring of a C string into a
 * double. Machine code: skips leading whitespace (per-char classify via
 * 0x3005e907), then runs the shared string->double conversion (0x3005e750 /
 * 0x3005e6ac) and returns the result in ST(0) (loaded from [result+0x10]).
 * Single `const char *` argument (EBP+0x8), double return. This is the CRT
 * atof; the size-matched bank name GetLeanFraction is rejected (no lean math —
 * it is a whitespace-skipping numeric parse). Provisional caller-observed decl —
 * superseded by its own .mcode reconstruction. */
double atof(const char *s);


/*
 * CG_PlayFxOnTag (0x30022780) — play a tag-bound effect described by an
 * effect config string. Register-argument ABI (self in EAX, self->fxId in ECX, but
 * the dispatcher passes fxId separately so it is a plain second parameter). Reads
 * config string CS_FX + fxId, whose first two bytes are decimal digits
 * selecting cg_effectDefs[(str[0]-'0')*10 + str[1]-'0']. Windows folds the 528-entry
 * ASCII bias into its indexed-load displacement; the Mac body performs both
 * subtractions explicitly. The remaining bytes (str + 2) are the tag name. Resolves the tag
 * on self->objId via CG_RESOLVE_TAG; on success plays that handle at
 * self->origin through CG_PLAY_EFFECT_ON_TAG, passing NULL dir and the address of a
 * stack copy of self->objId as the trailing out/in parameter. The
 * out-of-range config index path reuses CG_ConfigString's inlined
 * Com_ErrorMessage("CG_ConfigString: bad index: %i", n). Role name (exact CoD
 * symbol unproven); the .mcode's size-matched "G_DObjSetLocalTag" guess is rejected
 * (this is cgame effect-play through config strings, not a server DObj tag setter).
 */
void CG_PlayFxOnTag(centity_t *self, int fxId);

/*
 * CG_AddLoopedEntitySound (0x30021860) — register and (re)start an entity's pair of
 * looping sounds for the current frame, with an interpolated cyclic "phase".
 *
 * Guards (any failing => nothing emitted): self->fxId (+0xa4) and self->surfaceType
 * (+0xdc) must both be non-zero (both are looped-sound alias indices); and during
 * intermission (cg_snap->ps.pmType == PM_TYPE_INTERMISSION) an entity flagged as
 * unbound (self->legsSound == 0x3ff) is skipped.
 *
 * For each alias index it forms cfgIndex = CS_SOUNDS + index (0x295), reuses
 * CG_ConfigString's inlined [0, MAX_CONFIGSTRINGS) bounds check + Com_ErrorMessage
 * ("CG_ConfigString: bad index: %i") + &cg_gameState.stringData[offsets[cfgIndex]]
 * lookup, then registers that name via cgame_syscall(CG_COM_PICK_SOUND_ALIAS, name,
 * &self->origin) -> handle. If BOTH handles are non-zero it lerps the sound phase
 * between loopSoundPhaseOld (+0x6c) and loopSoundPhaseNew (+0x160) by
 * cg_frameInterpolation and starts the pair via trap_MSS_PlayBlendedSoundAliases(handle1, handle2, phase,
 * self->objId). Role name (exact CoD symbol unproven); the .mcode's size-matched
 * "CheckMatchTimeout" guess (from the wrong DLL, game_mp_uo) is rejected.
 */
void CG_AddLoopedEntitySound(centity_t *self);

/*
 * CG_EjectWeaponBrass (0x30047be0) -- eject the current weapon's shell-casing ("brass")
 * effect from the firing entity's "tag_brass" bone.
 *
 * thiscall: `self` (the firing centity_t) in ECX; the single cdecl stack arg
 * `eventId` is the weapon-event id (CG_EntityEvent event 0xa8 path, and the
 * CG_FireWeapon / 0x30023690 fire paths). Early-outs when the brass master switch
 * global cg_brass_vmCvar.integer (0x3044f3cc) is 0, when self->eType >= 0x10 (not a normal
 * entity), or when self->weaponModel is 0. If weaponModel exceeds bg_numWeapons
 * (0x30134cd4) it reuses CG_FireWeapon's out-of-range error
 * ("CG_FireWeapon: ent->weapon > BG_GetNumWeapons()").
 *
 * The eject effect handle is chosen from cg_weaponInfos[weaponModel]: field +0x170
 * when eventId == 0xa6 (the alt-brass event), otherwise field +0x16c; a zero
 * handle skips. The tag is resolved on the firing object id via CG_RESOLVE_TAG
 * (trap 0xe3) -- for the local player's own first-person view (cg_snap
 * PSF_PLAYER_ENTITY_MASK set and self->objId == cg_snap->ps.psClientNum) the object
 * id becomes weaponModel + 0x400 (the view-weapon DObj), otherwise self->objId --
 * then the effect is played on the resolved tag via CG_PLAY_EFFECT_ON_TAG (trap
 * 0xe9). Role name from the proven "tag_brass" literal and the CG_FireWeapon
 * error string; the .mcode's size-matched "script_method_scriptbuiltin_detach"
 * guess is rejected (this is cgame brass-ejection effect play, not a script
 * builtin). The earlier caller-observed "CG_ItemPickupEvent" guess is likewise
 * superseded.
 */
void CG_EjectWeaponBrass(entityState_t *self, int eventId);

/*
 * CG_EntityEffects (0x3001e7f0) — add a client entity's per-frame audio/visual
 * ambient effects: its looping sound and its dynamic light. Argument in EAX
 * (cent, a centity_t). If currentState.clientSound is nonzero it
 * emits a looping sound named by config string (clientSound + CS_SOUNDS): the
 * sound is placed at lerpOrigin + cg_inlineModelMidpoints[modelindex] when
 * currentState.solid == SOLID_BMODEL,
 * otherwise at the raw lerpOrigin, and is started via CG_PlaySoundAliasByName with the
 * entity number (currentState.number). If currentState.constantLight is nonzero it
 * unpacks that packed RGBA into an RGB color and intensity and adds a dynamic light
 * via CG_R_ADD_LIGHT_TO_SCENE. Role name (classic Q3/CoD CG_EntityEffects behavior);
 * the .mcode's size-matched "BG_TakePlayerWeapon" guess is rejected — this is cgame
 * render/audio code (config strings, sound/light traps, float color math), not a BG
 * playerState weapon routine.
 *
 * NAMING CONFLICT (flagged, not silently resolved): CG_EntityEffects reads the vec3
 * at cent+0x208 as the entity's emission origin (sound + light position). The shared
 * centity_t models +0x208 as `lerpOrigin` (written by CG_CalcEntityLerpPositions
 * 0x30021d30 and copied as an origin by CG_GetEntityOriginAxis 0x3002adb0), while the
 * centity_t view names the same +0x208 `origin`. Whether +0x208 is a single
 * lerpOrigin reused as angles, or two separate fields, is unresolved; this function
 * proves the +0x208 vec3 is consumed here as a world position.
 */
void CG_EntityEffects(centity_t *cent);

/*
 * CG_PlayFx (0x30022720) — play the effect selected by self->fxId at self->origin.
 * Validates 0 < fxId < 80, looks up the engine handle cg_effectDefs[fxId], and
 * fires it through the play trap: with a direction (CG_PLAY_EFFECT_ORIENTED) when
 * `dir` is non-NULL, otherwise at the origin only (CG_PLAY_EFFECT_ORIGIN). An
 * out-of-range fxId prints the "invalid effect id" diagnostic and does nothing.
 * Register-argument ABI (self in EDX, dir in ECX); named by the error string it
 * emits ("ERROR: CG_PlayFx called with invalid effect id %i"). The .mcode's
 * size-matched `irand` guess is rejected.
 */
void CG_PlayFx(centity_t *self, const vec_t *dir);

/*
 * CG_PlaySpinningViewEffect (0x300164b0) -- play the effect selected by
 * `effectId` at a point 256 units in front of the current view origin, oriented
 * by a continuously-animated spin angle. Reads the global spin angle
 * cg_refdefViewAngles[1] and builds a Z-axis rotation matrix from it via
 * YawVectors; the effect origin is cg_refdef.vieworg + 256*forward. Fires
 * cg_effectDefs[effectId] through CG_PLAY_EFFECT_ON_TAG (trap 0xe9) with that
 * origin, the 3x3 orientation, and a trailing 0. Role name (exact CoD symbol
 * unproven); the .mcode's size-matched "BG_IndexForString" guess is rejected --
 * this does no token/string lookup, it emits an oriented effect. See its own
 * .c file for the full machine-code derivation.
 */
void CG_PlaySpinningViewEffect(int effectId);

/*
 * CG_EntityEvent (0x30022810) -- run the visual/audio side effects for one entity
 * event id on a centity/localEntity. Big range-dispatch over the event id
 * (0x1..0x18, 0x18..0x2f, 0x46..0x5d, ... ) that plays sounds/effects, reading the
 * subject entity fields (currentState.eventParm at +0xa4, origin at +0x208, etc.);
 * indexes per-entity slots with the 0x288-byte stride array at 0x3048c6f8. This is
 * exactly the cgame CG_EntityEvent dispatcher (same-module PPC bank names it and
 * the id-range/entity-stride shape confirms it). Non-default register ABI proven
 * from CG_CheckPlayerstateEvents (0x30034f0f..0x30034f1c): the centity `self` is
 * passed in ECX, the event id in EAX, and one flag word (1) on the stack; the
 * callee ends in a plain RET so the caller cleans that one slot. Provisional
 * caller-observed declaration; superseded by its own .mcode reconstruction.
 */
void CG_EntityEvent(centity_t *self, int32_t event, int32_t predicted);
void CG_Obituary(centity_t *self);

/*
 * CG_EntityPreEvent (0x30023690) -- RECONSTRUCTED
 * (functions/FUN_30023690_3002388f.c). The DObj / corpse-model analogue of
 * CG_EntityEvent: dispatch one event id for the model sub-entity embedded at
 * cent->corpseModelInfo (+0xf4). Big jump-table dispatch (index table at
 * 0x300238b8, target table at 0x30023890, base id 0xa3) over the event id that
 * plays sounds/effects reading the model record's fields (surfType/poseType +0x88,
 * weaponIndex +0xcc, emitter params +0xd0/+0xd4, vehicleEntityNum +0x74, shake
 * params +0x68/+0x6c/+0x70, ... ). Register/stack ABI proven from both call sites
 * in CG_CheckPreEvents (0x30023a1b / 0x30023abd): the parent cent arrives
 * in EAX (0x3002369d MOV EDI,EAX; then LEA ESI,[EDI+0xf4]), the event id is the
 * first caller-cleaned stack arg (0x30023695 MOV EBP,[ESP+0x28] before the 2
 * pushes), and the event parm is the second stack arg (0x30023810 reads [ESP+0x34]
 * after the 4 pushes). Callee ends in a plain RET (caller cleans two slots).
 *
 * NAME RESOLVED: the function's own embedded debug-trace strings name it --
 * "CG_EntityPreEvent:ZERO EVENT\n" (.rdata 0x30077358) and
 * "ent:%3i  preevent:%3i CG_EntityPreEvent:%s\n" (.rdata 0x3007732c). This
 * supersedes the earlier role name CG_ModelEntityEvent and the mechanical
 * "CG_DrawField" size guess (both rejected). */
void CG_EntityPreEvent(centity_t *cent, int32_t event, int32_t eventParm);

/*
 * DObj/corpse model-event effect helpers CG_EntityPreEvent dispatches to. Each
 * signature is proven from the dispatcher and the recovered callee body. */

/* CG_EjectWeaponBrass (0x30047be0) also accepts the entityState-shaped model
 * record. At 0x30023853 it arrives in ECX with the event id in the one
 * caller-cleaned stack slot; the callee reads eType +0x4 and weapon +0xcc. */

/* CG_FireWeapon (0x30047d20) -- per-mode weapon effect (muzzle/brass/shell).
 * Register/stack ABI (0x30023796/0x300237a2/...): a flags word (carrying bit 0x80) in
 * EAX, and (cent, model, event, mode) as four caller-cleaned stack args pushed
 * cent,model,event,mode right-to-left (so mode is pushed first). Provisional role name. */
void CG_FireWeapon(uint32_t flags, centity_t *cent, entityState_t *model, int32_t event, int32_t mode);
void CG_PlayFxOnWeaponTag(qboolean selectViewDObj, int32_t weaponIndex, int32_t model, const vec3_t effectOrigin, const char *tagName,
                          int32_t drawTagModel);
void CG_FakeTrajectoryEffects(int32_t entityNum, int32_t weaponIndex, const char *tagName);

/* CG_ModelEventFireWeapon (0x30049060) -- the weapon-fire / tracer spawn for the model
 * event. Register ABI (0x30023786): event id in EAX, model->poseType (+0x88) in ECX,
 * model->weapon (+0xcc) in EDI, &cent->lerpOrigin (+0x208) in EBX, and
 * model->vehicleEntityNum (+0x74) as one caller-cleaned stack arg. Provisional role
 * name. */
void CG_ModelEventFireWeapon(int32_t event, uint32_t poseType, uint32_t weaponIndex, vec3_t lerpOrigin, int32_t vehicleEntityNum);

/* CG_BulletHitEvent (0x30048e60) -- bullet/tracer trail spawner between two
 * byte-dir vectors. Eight caller-cleaned stack args (0x20 cleanup), pushed
 * right-to-left so the C order is (vehicleEntityNum, &cent->lerpOrigin, dirA, dirB,
 * weaponIndex, poseType, emitterParam1, effectSelect). dirA/dirB are the two ByteToDir
 * outputs; effectSelect = emitterParam0 ? (emitterParam0 & 7) : 0. Provisional role
 * name. */
/* Its declaration is in the recovered weapon-effects section above. */


/*
 * Com_Compress (0x3004d550) — id-Tech in-place comment stripper. Walks `data`,
 * removing `//` line comments and slash-star block comments (preserving embedded
 * newlines inside block comments so line-number tracking stays intact), copying
 * every other byte — including bare `\r`/`\n` and lone `/` — down in place. NUL-
 * terminates the compacted output and returns its new length. Register ABI: the
 * single `char *data` argument arrives in ECX; plain `RET` (no stack args). The
 * size-matched .mcode name `ItemParse_maxPaintChars` is REJECTED — this is a raw
 * byte-level comment stripper, not a menu-item field parser. Name from the server
 * bank (game_functions.h: int Com_Compress(char *data)); behavior matches exactly.
 */
int Com_Compress(char *data);

/*
 * CG_RegisterSurfaceTypeSounds (0x3002b4f0) — build one canonical alias name per
 * surface type (23 types) for a base alias name, filling a caller-provided pointer
 * table. For each surface type i in 0..22 it resolves the surface-type name string
 * via CG_SURFACE_TYPE_TO_NAME(i), composes "<baseName>_<surfaceName>" with sprintf into a
 * scratch buffer, resolves that composed name via CG_COM_SOUND_ALIAS_STRING,
 * and stores the canonical pointer in table[i]. Register-argument ABI: EDI = table,
 * EBX = baseName; the caller at 0x3002b560/0x3002b7a0
 * sets EDI/EBX before each call (e.g. baseName "grenade_bounce"/"rocket_explode",
 * table 0x3044bbd4/...). The tables are consumed at 0x30023257/0x30023417 indexed
 * by a surface-type field (entity+0x88). Named by proven behavior; the exact asset
 * result is the engine-owned canonical alias-name pointer. The
 * size-matched .mcode name `G_EntDetachAll` is rejected (no entity-detach behavior;
 * this is a per-surface asset-registration loop).
 */
void CG_RegisterSurfaceTypeSounds(const char **table, const char *baseName);

/*
 * effectDef_t — one 64-byte per-surface effect-definition entry. The registrar
 * CG_RegisterEffectDefSurfaces (0x3001dcc0) walks an array of these (one entry per
 * surface type, stride 0x40) and only reads the two leading marker bytes; the whole
 * entry is otherwise handed by pointer to the register-effect trap CG_FX_REGISTER_EFFECT,
 * which consumes the filename. An empty filename denotes no definition; for that
 * case only, byte +0x01 is overloaded as a nonzero "optional" marker that
 * suppresses the missing-surface warning. */
typedef struct effectDef_s {
    char name[64];
} effectDef_t;

/*
 * fxTest command state — a stored effect name that is re-registered and replayed
 * at a fixed world origin on a throttle. Defined in globals.c and zero-initialized
 * in the DLL. CG_FxTest writes it; CG_UpdatePeriodicEffect consumes it.
 */
extern char cg_periodicEffectName[MAX_QPATH]; /* 0x3048b004: name -> CG_FX_REGISTER_EFFECT */
extern vec3_t cg_periodicEffectOrigin; /* 0x3048b044: origin -> CG_PLAY_EFFECT_ORIGIN */
extern int32_t cg_periodicEffectLastTime; /* 0x3048b050: cg.time of last emit */
extern int32_t cg_periodicEffectInterval; /* 0x3048b054: min ms between emits (>=1 gate) */

/*
 * CG_UpdatePeriodicEffect (0x30042110) — throttled periodic effect emitter. If the
 * interval is armed (cg_periodicEffectInterval >= 1) and enough time has elapsed
 * (cg.time > cg_periodicEffectLastTime + cg_periodicEffectInterval), register the
 * stored effect name via CG_FX_REGISTER_EFFECT and play the returned handle at the
 * stored origin via CG_PLAY_EFFECT_ORIGIN, then stamp cg_periodicEffectLastTime =
 * cg.time. Otherwise it is a no-op. Takes/returns nothing (bare RET, no args). The
 * size-matched .mcode name `PM_GetEffectiveStance` is REJECTED: no playerstate,
 * stance, or bg_pmove logic exists here — the body is purely an effect-emit
 * throttle over the trap dispatcher. */
void CG_UpdatePeriodicEffect(void);

/*
 * CG_RegisterEffectDefSurfaces (0x3001dcc0) - register the engine effect handles for
 * every surface type of one effect type, and return how many surface slots were
 * missing a definition. For each of the 24 surface types i (entry stride 0x40):
 *   - if defs[i].name[0] != 0, register the definition via CG_FX_REGISTER_EFFECT (register
 *     effect from def) and store the returned handle in handles[i];
 *   - else store handle 0, and - unless the empty-name optional marker is set or i is the last
 *     surface index (23) - emit the diagnostic "no entry for effect type '%s' on
 *     surface type '%s'\n" (effectTypeName, CG_SURFACE_TYPE_TO_NAME(i) surface name) via
 *     Com_PrintMessage and count it.
 * Returns the number of warnings emitted. Register-argument ABI (proven from the
 * caller loop at 0x3001e335): EAX = defs (the effectDef_t[24] table), stack arg0 =
 * effectTypeName, stack arg1 = handles (the qhandle_t[24] output). The caller sums
 * the returns across all effect types. The size-matched .mcode name
 * `script_func_isplayer` is REJECTED (no script/player behavior; this is a
 * per-surface effect-registration loop). */
int32_t CG_RegisterEffectDefSurfaces(const effectDef_t *defs, const char *effectTypeName, qhandle_t *handles);

/* CG_IMPACT_EFFECT_TYPES (22) and CG_IMPACT_SURFACE_TYPES (24) — the dimensions of
 * the shared cg_impactEffects[22][24] handle table — are defined in globals.h
 * (next to that array), which is included above. */

/*
 * compare_impact_files (0x3001df90) — qsort comparator for the impact-effect CSV
 * file list. Proven from its machine code: it loads *(const char **)a and
 * *(const char **)b and tail-jumps the locale-aware CRT comparison at
 * 0x30069275, i.e. `return coduo_crt_stricmp(*(const char **)a,
 * *(const char **)b)`. Name from the
 * same-module PPC bank (cgame_mp.dll compare_impact_files). Provisional
 * caller-observed decl; superseded by its own .mcode reconstruction. */
int32_t compare_impact_files(const void *a, const void *b);

/*
 * CG_ParseImpactEffects (0x3001dd30) — parse one impact-effect CSV file's text and
 * fill the per-effect-type effectDef_t tables. Caller-observed ABI from
 * CG_RegisterImpactEffects (0x3001e2bd): EAX register argument = text (the loaded,
 * NUL-terminated CSV buffer); stack args (path, count, names, defTables); returns
 * a `char *` error message (via va(...)) on a malformed row, or NULL on success.
 * The caller reports the returned string through Com_ErrorMessage. It tokenizes the
 * comma-separated rows against the effect-type name table (matching column 1 via
 * Q_stricmp), issues the per-name register-effect trap CG_REGISTER_NAMED_EFFECT (0xc8), and
 * writes the effectDef_t entries; the error strings are "unknown effect type ...",
 * "missing/unknown surface type ...", and "effect filename ... is too long". Name
 * from the same-module PPC bank (cgame_mp.dll CG_ParseImpactEffects). Provisional
 * caller-observed decl; superseded by its own .mcode reconstruction. The EAX
 * register-passed `text` argument is expressed here as the first parameter; its
 * register mapping is documented in the caller's .c file. */
char *CG_ParseImpactEffects(const char *path, char *text, int32_t effectTypeCount, const char *const *effectTypeNames,
                            effectDef_t *defTables);

/*
 * CG_RegisterImpactEffects (0x3001dfb0) — reconstructed; see
 * src/client/cgame/effects/cg_registerimpacteffects.c. Enumerates every "fx/[*].csv"
 * impact-effect definition file, parses each one into the 22 per-effect-type
 * effectDef_t[24] tables, then registers the engine effect handles for all 24
 * surface types of all 22 effect types and reports the count of missing entries.
 * The .mcode's size-matched guess "G_TryPushingEntity" (from game_mp.dll) is
 * REJECTED: this function opens fx CSV files via CG_FS_GETFILELIST, calls va,
 * qsort, the Com parse-session API, and CG_RegisterEffectDefSurfaces — no
 * entity-push/physics behavior. Name from the same-module PPC bank
 * (cgame_mp.dll CG_RegisterImpactEffects) and this proven behavior. */
void CG_RegisterImpactEffects(void);

/*
 * BG_GetEmptySlotForWeapon (0x30011520) — return the inventory slot a weapon should
 * occupy given the player's current slot occupancy, or 0 (NONE) when there is no free
 * acceptable slot. Register ABI (compiler __fastcall, proven from both call sites):
 * weapon in ECX, ps in EDX, result in EAX. The canonical C contract and shared
 * body are in src/bg/bg_weapon_inventory.c; a second caller is the usable-entity
 * hint drawer CG_DrawCursorhint (0x300303a0), which passes the pickup weapon index and
 * &cg.predictedPlayerState (0x304831c4). */

/*
 * Com_Error (0x3002b3d0) — the cgame variadic fatal-error reporter:
 * (int level, const char *format, ...), caller-cleaned. Proven from its machine
 * code: snapshots the /GS cookie, formats (format, ...) into a 0x400 stack buffer
 * via vsprintf (0x3005b538), then emits it via Com_ErrorMessage("%s", buffer)
 * (0x3002b300), which invokes the engine CG_ERROR syscall (trap id 1 through
 * *0x30085e9c). The `level` argument is the Quake3 error code (ERR_DROP == 1).
 * Its signature and PPC symbol are the canonical
 * `void Com_Error(errorParm_t code, const char *format, ...)`. The format-only animation
 * parser reporter at 0x30001200 is BG_AnimParseError. The complete shared
 * errorParm_t domain is declared in q_shared_types.h.
 */
void Com_Error(errorParm_t level, const char *format, ...);

/*
 * Q_strstr (0x3005b5c0) - locate the first occurrence of substring `needle` in
 * `haystack`; returns a pointer into `haystack` at the match, or NULL if absent
 * (returns `haystack` when `needle` is empty). cdecl, caller-cleaned
 * (haystack, needle) - proven by the BG_ParseCommands call `push ".wav"; push snd;
 * call; add esp,8; test eax,eax`. Standard strstr behavior recovered from the body
 * (two-char anchor scan then full-substring confirm). Name provisional-by-role
 * (Q_-prefixed idTech string helper); superseded by 0x3005b5c0's own .mcode.
 */
char *Q_strstr(const char *haystack, const char *needle);

/*
 * BG animation-parser scratch globals shared between BG_ParseCommands (0x30001e90)
 * and its caller BG_AnimParseAnimScriptCommands (0x30002470). The caller resolves a
 * movement-group / event keyword to an index via BG_IndexForString and stashes it here so
 * BG_ParseCommands can, after building each command, stamp the referenced animation
 * table entry's flags. Both use the shared 32-bit animation enum domains; only these
 * two functions touch them (verified by xref). The matching game parser supplies the
 * names retained here.
 *
 *  bgAnimParseCurrentAnimGroup (0x3008c4b8): the animations/statechanges movement
 *      group, an index into the bg_indexed_string table at 0x30082080 (or
 *      ANIM_MT_UNUSED). Used as a stateFlags bit position; ANIM_MT_CLIMBUP and
 *      ANIM_MT_CLIMBDOWN additionally set the vertical-motion flag.
 *  bgAnimParseCurrentEvent (0x3008bf34): the events-list event type, an index into the
 *      bg_indexed_string table at 0x30082118, or -1 when not in an events block.
 */
extern bg_anim_move_type_t bgAnimParseCurrentAnimGroup; /* 0x3008c4b8 */
extern bg_anim_event_t bgAnimParseCurrentEvent; /* 0x3008bf34 */

/*
 * Animation-script command discriminants used by BG_ParseCommands (0x30001e90) when
 * scanning a script's condition list and dispatching parsed body-part slots. Named
 * by proven role; exact source names unresolved.
 */
/* BG_ParseCommands uses the shared ANIM_BP_*, ANIM_MT_*, and ANIM_COND_*
 * domains.  The Windows cgame and game bodies agree on all compared values. */

/*
 * BG_ParseCommands (0x30001e90) - parse the `{ ... }` command body of one animation-
 * script entry. Reconstructed; see functions/FUN_30001e90_30002466.c.
 *
 * bg_anim_script_t and bg_static_animation_t are provided by the shared BG
 * animation-type boundary.
 */
void BG_ParseCommands(char **text_pp, bg_anim_script_t *script, bg_static_animation_t *animations);

/*
 * BG_ParseConditions (0x30001cd0) — reconstructed; see FUN_30001cd0_30001e84.c.
 * Parses the condition clauses that qualify one animation script/command and fills
 * the script's condition array; always returns qtrue. Custom register ABI (proven
 * at the BG_AnimParseAnimScript call sites 0x30002a5b / 0x30002d73): the script base
 * is passed in ESI, the text cursor as a single caller-cleaned stack dword (&data_p).
 * Modeled here as a normal C signature; the ESI/stack split is a codegen detail.
 * Server bank: `qboolean BG_ParseConditions(char **parse, bg_anim_script_t *script)`.
 */
qboolean BG_ParseConditions(char **text_pp, bg_anim_script_t *script);

/*
 * SkipWhitespace (0x3004d510) — advance past leading whitespace and count newlines
 * for the Com script tokenizer. Name from the same-module cgame_mp.dll PPC bank
 * (SkipWhitespace) and the behavior proven by objdump of 0x3004d510: it scans a
 * text cursor while the current char is <= ' ' (signed byte compare), incrementing
 * com_parseSession->line (+0x400) and setting *hasNewLines on each '\n', and
 * returns the pointer to the first char > ' ', or NULL at the terminating '\0'.
 * The client passes the text cursor in EAX and &hasNewLines in ESI (register ABI);
 * modeled here with the canonical Quake3/CoD source signature. Called by Com_Parse
 * (0x3004d6b0) and expected by other Com parser routines. Provisional
 * caller-observed ABI — superseded by its own .mcode reconstruction.
 */
char *SkipWhitespace(char *data, qboolean *hasNewLines);

/*
 * CG_Asset_Parse (0x3002cb40) — parse a single "assetGlobalDef { ... }" block
 * from a PC parser source. Caller-observed from CG_ParseMenu (0x3002d110):
 * the PC source handle is passed in ECX (this-call) and a second value (the menu
 * asset load mode, ESI) on the stack; it reads tokens via trap_PC_ReadToken and
 * matches "}" (0x30072764) / global asset keywords (e.g. 0x30077d80). Returns
 * nonzero on success, 0 on parse failure. Provisional caller-observed ABI and
 * role name (the assetGlobalDef branch); superseded by its own .mcode
 * reconstruction. Name corroborated by the same-module PPC symbol bank.
 */
qboolean CG_Asset_Parse(int32_t sourceHandle, int32_t loadMode);

/*
 * CG_ParseMenu (0x3002d110) — reconstructed; see
 * src/client/cgame/ui/cg_parsemenu.c. Opens the menu source file named
 * by `filename` through trap_PC_LoadSource (CG_PC_LOAD_SOURCE), falling back to
 * "ui_mp/testhud.menu" (0x30077c8c) when the primary open returns a null handle,
 * then reads tokens with trap_PC_ReadToken and dispatches the top-level keywords
 * "assetGlobalDef" (0x30077c7c) -> Menus_ParseAsset and "menudef" (0x30077c74) ->
 * Menu_New, until the closing '}' or end-of-source, and frees the source with
 * trap_PC_FreeSource. `loadMode` is an incoming register argument (ESI) forwarded
 * unchanged to Menu_New/Menus_ParseAsset as the asset load mode; it is NOT the
 * PC source handle (the source handle is opened locally). The function has no
 * meaningful return value (the sole caller CG_Load_Menu ignores it); modeled as
 * void. Name from the same-module PPC bank (cgame_mp.dll) and this behavior. The
 * .mcode's size-matched "CG_HudElemShaderHeight" guess is rejected.
 */
void CG_ParseMenu(int32_t loadMode, const char *filename);

/*
 * CG_Load_Menu (0x3002d200) — reconstructed; see
 * src/client/cgame/ui/cg_load_menu.c. Parses a brace-delimited list of
 * menu filenames — `{ "file1" "file2" ... }` — from the shared Com parse cursor
 * *parse, calling CG_ParseMenu on each filename token until the closing `}`.
 * Returns qtrue when the block closes cleanly on `}`, qfalse on a missing opening
 * `{`, end-of-text, or an empty token. Name from the same-module PPC bank
 * (cgame_mp.dll CG_Load_Menu) and the callgraph: its caller (CG_LoadMenus, 0x3002d3c0)
 * matches the "loadmenu" keyword and then invokes this. The .mcode's size-matched
 * "ItemParse_origin" guess is REJECTED (that is a single-keyword vec-parse handler;
 * this is a brace-block filename-list loop that dispatches CG_ParseMenu).
 * The incoming EAX register argument is the scalar asset load mode forwarded
 * unchanged to CG_ParseMenu; the sole caller supplies the value 5.
 */
qboolean CG_Load_Menu(int32_t loadMode, char **parse);

/*
 * CG_LoadMenus (0x3002d2d0) — reconstructed; see
 * src/client/cgame/ui/cg_loadmenus.c. Load and register all menus listed
 * in a menu-list file. It times itself with CG_MILLISECONDS (engine milliseconds), opens
 * the file named by `menuFile` via CG_FS_FOPEN_FILE, falling back to the default
 * "ui_mp/hud.txt" (with a CG_ERROR "menu file not found ... using default", then
 * "unable to continue!" if the default is also missing), rejects files >= 4096
 * bytes ("menu file too large"), reads the text into cg_menuListText[4096],
 * NUL-terminates and Com_Compress()es it, resets menuCount to 0, then walks the
 * top-level tokens with Com_Parse and dispatches each "loadmenu { ... }" block to
 * CG_Load_Menu, finally printing "UI menu load time = %d milli seconds". `loadMode`
 * is a stack argument (the caller FUN_3002da90 pushes the literal 5) forwarded
 * unchanged to CG_Load_Menu; `menuFile` arrives in the EDI register. Name from the
 * same-module PPC bank (cgame_mp.dll CG_LoadMenus) and this proven behavior; the
 * .mcode's size-matched "BG_SetupWeaponAlts" guess is REJECTED (no weapon-record
 * access — this is the ui_shared menu-list loader). Custom register ABI (EDI =
 * menuFile) documented in the .c. */
void CG_LoadMenus(int32_t loadMode, const char *menuFile);
int CG_ShellShockSave(const char *name);

/*
 * CG_UIDisplayContextInit (0x3002da90) — install every function-pointer/asset slot
 * of the static ui_shared display context (g_uiDCInstance, 0x30421f60) from its
 * fixed cgame service addresses, publish it as the global DC (0x30134d2c), reset
 * the registered-menu count, then load the HUD menu list named by cg_hudFiles
 * (default "ui_mp/hud.txt"). The .mcode size-guess name PM_Weapon_CheckForDeployBreakdown
 * (win 0x258 == matched 0x258) is REJECTED (no ADS-lerp math; this is the DC
 * bootstrap). src/client/cgame/ui/cg_uidisplaycontextinit.c */
void CG_UIDisplayContextInit(void);

/* The BG animation / player-DObj context types (pmove_t,
 * playerState_t) and the pointer global pm are defined in
 * globals.h (included above) so both the globals storage unit and the consumers
 * here can see them. */

/* The complete Q3-derived window/menu/item/type-data family is shared by
 * cgame and UI through ui_menu_types.h. */

/* ITEM_CVAR_* constants are shared with itemDef_t. */

/* The common parser keyword handlers are shared by ui_parse.h. */

/* itemDef_t has pointer fields before these offsets, so the layout is only
 * ABI-exact at 32-bit pointer width (the target DLL); guard the asserts. */
#if UINTPTR_MAX == 0xFFFFFFFFu
_Static_assert(offsetof(itemDef_t, window.background) == 0xb4, "itemDef_t.window.background must sit at +0xb4");
_Static_assert(offsetof(itemDef_t, textRect) == 0xb8, "itemDef_t.textRect must sit at +0xb8");
_Static_assert(offsetof(itemDef_t, type) == 0xc8, "itemDef_t.type must sit at +0xc8");
_Static_assert(offsetof(itemDef_t, typeValidated) == 0xcc, "itemDef_t.typeValidated must sit at +0xcc");
_Static_assert(offsetof(itemDef_t, alignment) == 0xd0, "itemDef_t.alignment must sit at +0xd0");
_Static_assert(offsetof(itemDef_t, font) == 0xd4, "itemDef_t.font must sit at +0xd4");
_Static_assert(offsetof(itemDef_t, textStyle) == 0xe8, "itemDef_t.textStyle must sit at +0xe8");
_Static_assert(offsetof(itemDef_t, text) == 0xec, "itemDef_t.text must sit at +0xec");
_Static_assert(offsetof(itemDef_t, parent) == 0xf0, "itemDef_t.parent must sit at +0xf0");
_Static_assert(offsetof(itemDef_t, mouseEnterText) == 0xf8, "itemDef_t.mouseEnterText must sit at +0xf8");
_Static_assert(offsetof(itemDef_t, mouseExitText) == 0xfc, "itemDef_t.mouseExitText must sit at +0xfc");
_Static_assert(offsetof(itemDef_t, mouseEnter) == 0x100, "itemDef_t.mouseEnter must sit at +0x100");
_Static_assert(offsetof(itemDef_t, mouseExit) == 0x104, "itemDef_t.mouseExit must sit at +0x104");
_Static_assert(offsetof(itemDef_t, action) == 0x108, "itemDef_t.action must sit at +0x108");
_Static_assert(offsetof(itemDef_t, accept) == 0x10c, "itemDef_t.accept must sit at +0x10c");
_Static_assert(offsetof(itemDef_t, onFocus) == 0x110, "itemDef_t.onFocus must sit at +0x110");
_Static_assert(offsetof(itemDef_t, leaveFocus) == 0x114, "itemDef_t.leaveFocus must sit at +0x114");
_Static_assert(offsetof(itemDef_t, cvar) == 0x118, "itemDef_t.cvar must sit at +0x118");
_Static_assert(offsetof(itemDef_t, cvarFlags) == 0x124, "itemDef_t.cvarFlags must sit at +0x124");
_Static_assert(offsetof(itemDef_t, focusSound) == 0x128, "itemDef_t.focusSound must sit at +0x128");
_Static_assert(offsetof(itemDef_t, numColors) == 0x12c, "itemDef_t.numColors must sit at +0x12c");
_Static_assert(offsetof(itemDef_t, colorRanges) == 0x130, "itemDef_t.colorRanges must sit at +0x130");
_Static_assert(offsetof(itemDef_t, colorRangeType) == 0x248, "itemDef_t.colorRangeType must sit at +0x248");
_Static_assert(offsetof(itemDef_t, special) == 0x24c, "itemDef_t.special must sit at +0x24c");
_Static_assert(offsetof(itemDef_t, loadMode) == 0x258, "itemDef_t.loadMode must sit at +0x258");
#endif

/* The complete Item_Multi_* runtime is shared by ui_runtime.h. */

/* The complete UI memory/string-pool API is shared by ui_memory.h. */

/* editFieldDef_t is part of the shared item type-data family. */

/* Item_TextField_HandleKey/Paint are shared by ui_runtime.h. */

/* Script_SetFocus and the list/response command cluster are shared by
 * ui_runtime.h; their paired PE32 provenance is retained with the bodies. */

/* The visual-property command cluster is shared by ui_runtime.h; its paired
 * PE32 and supporting Mac provenance is retained with the bodies. */

/* Item_YesNo_* is shared by ui_runtime.h. */

/* Item_RunScript and its complete command table are shared by ui_runtime.h. */

/* Item_Action is shared by ui_runtime.h. */

/*
 * Item_ListBox_MaxScroll (0x30052b90) — greatest valid top-of-list scroll index
 * for a listbox item: feeder element count minus the number of elements that fit
 * in the item's visible extent, clamped at 0. Requires item->type ==
 * ITEM_TYPE_LISTBOX (else prints "^1Menu Error: Expecting type: ITEM_TYPE_LISTBOX"
 * via Com_Printf and, with no listBoxDef, returns 0). count comes from
 * DC->feederCount(item->special); the divisor is elementWidth or elementHeight
 * chosen by WINDOW_HORIZONTAL. Same-module PPC bank name (cgame_mp.dll
 * Item_ListBox_MaxScroll); behavior matches. Custom register ABI: item is passed
 * in EDI (see .c file); expressed here as a normal C signature.
 */

/*
 * Item_ListBox_ThumbPosition (0x30052c00) — pixel coordinate of the scrollbar
 * thumb for a listbox item, truncated to an int via Q_rint. Requires item->type
 * == ITEM_TYPE_LISTBOX (else prints the same "^1Menu Error: Expecting type:
 * ITEM_TYPE_LISTBOX" as Item_ListBox_MaxScroll and, with no listBoxDef, returns
 * 0). Maps the top-of-list index (listBoxDef->startPos) linearly across the
 * item's inner scroll track: track length is the item's extent minus
 * SCROLLBAR_SIZE*2 + 2 + SCROLLBAR_SIZE, divided by Item_ListBox_MaxScroll(); the
 * result is offset by the track origin (rect coord + SCROLLBAR_SIZE + 1). Uses
 * rect.w/rect.x for WINDOW_HORIZONTAL, else rect.h/rect.y. Same-module PPC bank
 * name (cgame_mp.dll Item_ListBox_ThumbPosition); behavior matches. Custom
 * register ABI: item is passed in EAX (see .c file); expressed as a normal C
 * signature. Note the returned value differs from canonical Q3 ui_shared.c, which
 * returns float: this UO client rounds to int and subtracts one extra
 * SCROLLBAR_SIZE from the track length (recorded as a divergence in the .c file).
 */

/*
 * Item_ListBox_ThumbDrawPosition (0x30052cd0) — where to DRAW the scrollbar
 * thumb this frame. If this item is currently captured for a drag
 * (captureItem == item, 0x30134d28) and the mouse cursor lies within the
 * scrollbar drag track, it returns the live cursor coordinate minus half the
 * scrollbar size, so the thumb tracks the pointer; otherwise it tail-calls
 * Item_ListBox_ThumbPosition(item) to return the position implied by the scroll
 * index. Uses DC->cursorx (+0xf4) for WINDOW_HORIZONTAL items, else
 * DC->cursory (+0xf8). Same-module PPC bank name (cgame_mp.dll
 * Item_ListBox_ThumbDrawPosition); mechanical .mcode label Menu_OrbitItemByName
 * was a size-only guess and is rejected. Custom register ABI: item is passed in
 * ESI (see .c file); expressed here as a normal C signature.
 */

/* The complete Item_Slider_* runtime is shared by ui_runtime.h. */

/* The complete Item_Bind_* runtime is shared by ui_runtime.h. */

/*
 * Item_ListBox_OverLB (0x30052ed0) — hit-test a point (x,y) against the scrollbar
 * regions of a listbox item and return which region the cursor is over, as one of
 * the WINDOW_LB_* flag bits (0 if the cursor is not over any scrollbar region).
 * Requires item->typeValidated (+0xcc) == ITEM_TYPE_LISTBOX (else prints the same
 * "^1Menu Error: Expecting type: ITEM_TYPE_LISTBOX" via Com_Printf and returns 0)
 * and a non-NULL listBoxDef (else returns 0). Tests a sequence of SCROLLBAR_SIZE-
 * wide sub-rectangles (arrows, thumb, page-up/down track) via Rect_ContainsPoint,
 * splitting the track around Item_ListBox_ThumbPosition. Behavioral name from the
 * WINDOW_LB_* bits it returns and its call from Item_ListBox_MouseEnter; matches
 * Q3 ui_shared.c Item_ListBox_OverLB. The .mcode's size-matched
 * "Scr_ParseGameTypeList" guess is rejected — this reads no script and returns
 * region flags. ABI: item in ESI (register), x/y on the stack, caller-cleaned;
 * returns the region-flag word in EAX. Provisional caller-observed record;
 * arity/types re-derived from Item_ListBox_MouseEnter's call site, superseded by
 * its own .mcode reconstruction. */

/*
 * Item_ListBox_MouseEnter (0x30053110) — the listbox branch of Item_MouseEnter:
 * called when the cursor enters a listbox item, it refreshes which scrollbar region
 * (WINDOW_LB_*) the cursor is over and, if the cursor is over the list body rather
 * than the scrollbar, computes and stores the hovered element index into
 * listBoxDef->cursorPos. Requires item->typeValidated == ITEM_TYPE_LISTBOX and a
 * non-NULL listBoxDef. Behavioral name from its role and its sole caller
 * Item_MouseEnter (0x30053260, `item->type == ITEM_TYPE_LISTBOX` branch); matches
 * the Q3 ui_shared.c listbox mouse-enter helper. ABI: item in EAX (register), x/y
 * on the stack, caller-cleaned (ADD ESP,8). */

/*
 * Entity controller handlers dispatched by CG_DoControllers (0x30021fe0) on
 * part->eType. Their Mac symbols and Windows roles are correlated. All three
 * query the entity's DObj via syscall 0xa5 and receive the four-word DObj bone
 * selection bitset used by the rot/trans-index gates.
 *
 *  - CG_Player_DoControllers (0x30021fa0): scales part->modelIndex(+0x94) by 0x4d0 into a
 *    per-model table (0x305e1f34) and, on a live entry, calls 0x30005730 — a model
 *    player controller record.
 *  - CG_mg42_DoControllers (0x3001e9f0): gates on DObj-state globals (bit test on
 *    0x30483248, compare against 0x30483780) before acting.
 *  - The type-12/type-13 target at 0x30020540 was previously called
 *    HandleAnimPart from this dispatch context. Its own body proves it is actually
 *    CG_Vehicle_DoControllers: it consumes the full centity vehicle overlay,
 *    sets turret/wheel tags, traces all six wheel bones, and emits tread effects.
 */
void CG_Player_DoControllers(centity_t *part, uint32_t *partBits);
void CG_mg42_DoControllers(centity_t *part, uint32_t *partBits);
void CG_Vehicle_DoControllers(centity_t *vehicle, uint32_t *partBits);

/*
 * CG_DoControllers (0x30021fe0) — dispatch one entity to its player, MG42, or
 * vehicle DObj controller. The Mac body has the identical three controller
 * callees. See the function's .c for the full ABI and switch-table derivation.
 */
void CG_DoControllers(centity_t *part, uint32_t *partBits);

/*
 * CG_DObjCalcBone (0x30022080) — ensure one DObj bone is current. If
 * CreateSkelForBone reports work remains, fetch its four-word hierarchy bitset,
 * calculate animation, run the entity controllers, and calculate the skeleton.
 * The exact name comes from the Mac cgame symbol and the operation sequence
 * matches the server's G_DObjCalcBone.
 */
void CG_DObjCalcBone(DObj *self, int32_t boneIndex, centity_t *part);

/*
 * CG_DObjCalcPose (0x30022040) — renderer callback exported under this exact Mac
 * symbol. It receives the renderer's DObj and requested four-word part bitset;
 * if CreateSkelForBones reports work remains it calculates animation, runs the
 * owning entity's controllers, and calculates the skeleton.
 */
void CG_DObjCalcPose(centity_t *owner, DObj *obj, uint32_t *partBits);

/*
 * CG_DObjGetWorldTagMatrix (0x3001fdf0, provisional-by-role) — build the
 * world-space bone/tag matrix for a client entity's DObj skeleton. Steps
 * (proven from the machine code):
 *   1. handle = cgame_syscall(CG_DOBJ_GET_BONE_INDEX, self, tagName); if handle < 0,
 *      return qfalse (no DObj) leaving `out` untouched.
 *   2. CG_DObjCalcBone(self, handle, entity) — calculate the requested bone
 *      hierarchy and run the entity's DObj controllers.
 *   3. bone = cgame_syscall(CG_DOBJ_GET_BONE_MATRICES, self, 0) + (handle << 6):
 *      the entity's per-bone matrix table entry (stride 0x40). If that pointer is
 *      NULL, return qfalse.
 *   4. Build a local placement matrix from the entity: AngleVectors(entity.
 *      lerpAngles, forward, right, up); rows = {forward, -right, up,
 *      entity.lerpOrigin(origin)}.
 *   5. CG_ComposeBoneMatrix(bone, localMatrix, out) to write the world matrix,
 *      then return qtrue.
 * Shares its prologue and DObj-handle logic with the lighter sibling at
 * 0x3001fda0 (which returns the bone-matrix pointer directly instead of composing
 * a world matrix). The .mcode size-guess name "script_method_player_playlocalsound"
 * is rejected: there is no sound work; this is DObj bone-matrix / AngleVectors math.
 * Register-argument ABI (custom regparm): `self` (DObj context) arrives in ECX;
 * `tagName` (the named bone/tag to resolve, e.g. "tag_flash") arrives in EAX;
 * `entity` (a centity_t*) is the first stack arg; `out` (float[16]) is the
 * second stack arg. Returns qboolean in EAX (1 on success, 0 on no-DObj). Modeled
 * as ordered parameters; no calling-convention attribute (syntax-only build).
 *
 * tagName WIDENING (per hardened provisional-decl rule): step 1 above is a TWO-arg
 * trap(0xb2, self, tagName) whose tagName is passed in EAX (callee 0x3001fdf8:
 * PUSH EAX / PUSH ESI(self) / PUSH 0xb2 / CALL cgame_syscall). Every call site
 * loads a tag-name string into EAX right before the CALL (this function 0x30049157
 * MOV EAX,"tag_flash"; CG_CalcMuzzlePoint 0x30048ca7 MOV EAX,EBX=weaponName). The
 * earlier decl omitted this EAX argument, and the CG_CalcMuzzlePoint reconstruction
 * mislabeled the EAX load as a "dead compiler artifact"; the machine code proves it
 * is the tag-name argument. Signature widened and callers updated. Other
 * arity/types verified at each call site.
 */
qboolean CG_DObjGetWorldTagMatrix(DObj *self, const char *tagName, centity_t *entity, DObjSkelMat *out);

/*
 * CG_DObjGetEntityBoneMatrix (0x3001fda0, provisional-by-role) — the lighter
 * sibling of CG_DObjGetWorldTagMatrix. Resolves a NAMED tag on `self`
 * (trap 0xb2 with a tagName in EAX; callers pass "tag_weapon" / "tag_aim"),
 * calculates that bone's DObj hierarchy via CG_DObjCalcBone, then
 * returns the pointer to the bone's raw 4x4 matrix in the engine table
 * (trap 0xa0 table base + handle<<6). Returns NULL when the tag is absent
 * (negative bone handle). Unlike the 0x3001fdf0 sibling it does NOT compose a
 * placement matrix into an out buffer and has NO NULL guard on the table result.
 * Register-argument ABI (custom regparm): `self` in ECX, `tagName` in EAX,
 * `part` (the owning entity record) the sole stack arg; modeled as ordered
 * parameters, no calling-convention attribute (syntax-only build).
 */
DObjSkelMat *CG_DObjGetEntityBoneMatrix(DObj *self, const char *tagName, centity_t *part);

/* crt_ftol_round (0x3006be3c) — MERGED: this MSVC CRT `_ftol2` helper is declared
 * once, canonically, as `Q_rint` above (see that decl for the true truncation
 * semantics). The former duplicate `crt_ftol_round` decl was removed and its
 * callers repointed to Q_rint. */

/*
 * CG_PlayerTurretPositionAndBlend (0x30033b70) — position and orient a player who is
 * manning a vehicle turret, by blending the player's aim along the turret weapon's
 * DObj bone tree, then writing the blended barrel angles back into the player's
 * centity lerpOrigin/lerpAngles. Reconstruction: FUN_30033b70_300343d4.c.
 *
 * Takes the PLAYER centity by pointer (its sole stack argument at [ESP+4]); returns
 * void. Proven a centity_t* (not the corpse entityState_t): the body
 * dereferences +0x208 (lerpOrigin) and +0x214 (lerpAngles), which lie beyond
 * the 0xf4-byte entityState_t. It reads player->vehicleEntityNum (+0x74),
 * player->clientNum (+0x94, the bgs.clientinfo[] row), and player->lerpOrigin/
 * lerpAngles as the source aim; the turret vehicle is cg_entities[+0x74].
 */
void CG_PlayerTurretPositionAndBlend(centity_t *player);

/*
 * CG_GetRiderTagName (0x30008190) — map a rider/seat slot index (0..6) to the
 * model-attach tag name string ("*unused*", "tag_player", "tag_secondary_player",
 * "tag_passenger", "tag_passenger2".."tag_passenger4"). Register-argument ABI: the
 * index arrives in EAX (`MOV EAX,[ESP+EAX*4]`); no bounds check (callers mask & 7).
 * Reconstructed at its own .c; promoted to the shared header because
 * CG_PlayerVehiclePositionAndBlend (0x30032fe0) is a second caller. See the RIDER_TAG_*
 * enum in that .c for the slot constants.
 */
const char *CG_GetRiderTagName(int index);

/*
 * CG_GetVehicleViewPosOriginTag (0x300407c0, provisional-by-role) — map a vehicle
 * view/seat mode to the model-attach tag string used to anchor the vehicle camera.
 * Register-argument ABI: the mode arrives in EAX; the body computes (mode-2) and, if
 * that is <= 4 (unsigned), jumps through a 5-entry table, else returns the default.
 * Proven mapping (from the jump table at 0x300407f4 and the RET stubs):
 *   2 -> "tag_secondary_gun" (0x300771b8)
 *   3 -> "tag_passenger"     (0x30072d40)
 *   4 -> "tag_passenger2"    (0x30072d30)
 *   5 -> "tag_passenger3"    (0x30072d20)
 *   6 -> "tag_passenger4"    (0x30072d10)
 *   default -> "tag_turret"  (0x300771ec)
 * Sole observed caller is CG_CalcVehicleViewPos (0x300409c8, 0x30040dca); promoted to
 * the shared header. Provisional name (behavior-proven role); no calling-convention
 * attribute (syntax-only build). arity/types verified at the call sites. */
const char *CG_GetVehicleViewPosOriginTag(int32_t viewMode);

/*
 * BG_GetVehiclePosOffset (0x300081e0) — return a pointer to the per-position
 * position-offset vec3 for a rider being placed on a vehicle tag, selected by the
 * vehicle type (EAX) and the seat slot (ECX). Full body reconstructed (a nested
 * switch on the two register arguments returning one of four const vec3 pointers):
 *   ARTILLERY              -> &bgVehicleArtilleryPositionOffset {0,-24,0}
 *   TANK && position==2    -> &bgVehicleTankPosition2Offset {10,5,-39}
 *   TANK && position==1    -> &bgVehicleTankPosition1Offset {0,0,-20}
 *   default (any other)     -> &vec3_origin               {0,0,0}   (0x30071f58)
 * The sole caller (0x30033aa9) consumes the result as a float pointer (FMUL float
 * ptr [EAX]) to scale a placement basis vector, so the return type is const float *.
 * Register-argument ABI (EAX=vehicle type, ECX=seat slot), modeled with ordered
 * parameters; no calling-convention attribute (syntax-only build). The mechanical
 * size-guess name "Scr_LocalizationError" is rejected: this issues no error, it is a
 * seat-offset table lookup. The shared declaration and implementation are in
 * bg_vehicle.h and src/bg/bg_vehicle.c. */

/*
 * CG_DObjGetSpecialTagWorldMatrix (0x3001fec0, provisional-by-role) — third
 * sibling of the DObj bone-world-matrix family. Resolves a NAMED tag on `self`
 * (trap 0xb2 with a tagName), calculates that bone's DObj hierarchy (inlined
 * CreateSkelForBone/GetHierarchyBits/CalcAnim/CalcSkel, no controller call), then
 * composes the FIXED global cg_specialTagPlacement (orientation_t at 0x3048b0e4)
 * into the engine bone matrix (trap 0xa0 table + handle<<6) via
 * CG_ComposeBoneMatrix, writing the world matrix to `out`. Returns qtrue on
 * success, qfalse when the tag is absent (negative bone handle). Differs from the
 * 0x3001fdf0 sibling by using the fixed placement instead of per-entity angles and
 * by omitting the NULL bone-matrix guard. Register-argument ABI: `self` in EDI,
 * `tagName` in EAX, `out` the sole stack arg; modeled as ordered parameters (no
 * calling-convention attribute for the syntax-only build). The .mcode size-guess
 * name "PM_BeginReloadLoop" is rejected (no pmove/reload work). See the function's
 * .c for the full per-instruction derivation.
 */
qboolean CG_DObjGetSpecialTagWorldMatrix(struct DObj_s *self, const char *tagName, DObjSkelMat *out);

/* cg_corpseInfo (0x3044cb00) — per-corpse client animation/info table,
 * stride 0x4d0 (clientInfo_t), indexed by (corpse entity number - 0x40). Three
 * consumers use this base identically (0x30021e0a, CG_AddPlayerCorpseEntity at
 * 0x30034708, and 0x3003c8e6): each does `SUB idx,0x40; IMUL idx,idx,0x4d0; ADD
 * base`. Storage in globals.c (superseding the mechanical scalar symbol whose
 * owner label bg_calculateweaponangles was a wrong first-touch artifact).
 *
 * The retail eight-record count is proven jointly by the +0x4c4 field walk at
 * 0x3044cfc4..0x3044f644 and the next independent datum at 0x3044f18c.
 * The model state number carries that corpse entity number into the render
 * path. The shared PLAYER_CLONE_ENTITYNUM_BASE follows MAX_CLIENTS, while
 * PLAYER_CLONE_COUNT retains the retail count and lets coordinated
 * client/server mods configure a larger pool. */
extern clientInfo_t cg_corpseInfo[PLAYER_CLONE_COUNT];

/*
 * centity_t.corpseModelInfo is a second entityState_t stored at cent+0xf4, not
 * a distinct DObj-only record. CG_AddPlayerCorpseEntity forms that address and
 * the consumers use the canonical entity-state offsets through +0xe8: number,
 * eType, eFlags, pos, the event ring, weapon, legsAnim/torsoAnim, and
 * fTorsoHeight. The full +0xf4-byte view also agrees with nextState, which shares
 * the same storage. Keeping a separate 0xec-byte overlay duplicated the common
 * layout and assigned cgame-only names to transmitted entity-state lanes.
 */

/*
 * CG_ResetPlayerEntity (0x30034880) — (re)initialise a player DObj model's per-bone
 * anim tags and its legs/torso/lean swing-angle state to the client's current view
 * angles. The sole caller (0x3003c7c0) writes the fresh view angles into the client's
 * clientInfo_t (+0x3e8/+0x3ec/+0x3f0) and then calls this with the model-info
 * entity-state record as its one cdecl stack argument.
 *
 * When the model is drawable (renderEntity->eFlags & EF_DEAD == 0) it
 * issues four DObj bone-tag traps on the client's DObj model handle
 * (clientInfo_t.animTree, +0x4c4) — trap 0x8a for the root bone, then trap
 * 0x90 for the torso, legs and turning bones — and resets the legs/torso/lean swing
 * fields (memset each 0x30-byte block, then seed the yaw/lean angles from viewYaw /
 * viewPitch). When NODRAW is set the whole block is skipped. Finally, if
 * cg_debugposition_vmCvar.integer is nonzero, it prints a "ResetPlayerEntity yaw=" trace.
 *
 * The .mcode size-guess name "CG_InterpolateEntityPosition" is REJECTED: there is no
 * snapshot/nextSnapshot interpolation, no BG_EvaluateTrajectory, and no
 * cg_frameInterpolation — the body only emits bone traps and resets swing state.
 * Named by its proven debug string "ResetPlayerEntity" plus its reset behaviour. */
void CG_ResetPlayerEntity(entityState_t *renderEntity);

/*
 * CG_RegisterModel (0x3003d940) — register a render model by name and return its
 * qhandle_t (0 => load failed). RECONSTRUCTED from its own .mcode
 * (FUN_3003d940_3003db5f.c). It is the twin of CG_RegisterShader (0x3003db80): both
 * are byte-identical wrappers that, while a client snapshot is not yet installed
 * (cg_snap==NULL) and no redraw is already in progress (cg_updateScreenActive==0),
 * pump the connect/loading screen once (clear the cl_serverload* cvars, draw the map
 * levelshot full-screen, draw the com_expectedhunkusage progress bar, force a present
 * via trap_UpdateScreen) and then forward (name, category) to the register trap and
 * return its handle. Call sites push the category first and name second, proving the
 * cdecl source order `(const char *name, int category)`. Callers use category 6 for
 * icon/view models and 7 for world models. */
qhandle_t CG_RegisterModel(const char *name, int category);

/*
 * CG_BuildCorpseDObjModels (0x300058f0) — (re)build the DObj model set for a corpse
 * model-part and commit it to the engine. Register-argument client ABI proven from
 * the caller CG_AddPlayerCorpseEntity (0x300346c0 / 0x30034830):
 *   EBX = &cg_corpseInfo[modelPartIndex-0x40],
 *   EDX = the DObj handle from CG_DOBJ_GET_HANDLE (trap 0xa5),
 *   stack arg0 = renderEntity (cent->corpseModelInfo, cent+0xf4),
 *   stack arg1 = &generationOut (cent+0x284): a byte the function writes the record's
 *                build generation into (and short-circuits on when unchanged).
 * The mechanical size-guess "G_FreeVehicle" is REJECTED: the machine code registers
 * models, wraps them into DObj objects, and issues the DObj model-set traps; it does
 * not free a vehicle. Provisional decl; superseded by this file's own reconstruction. */
void CG_BuildCorpseDObjModels(clientInfo_t *info, intptr_t dobjHandle, entityState_t *renderEntity, uint8_t *generationOut);

/*
 * Player-animation update passes and lerp-frame helpers. All are reconstructed
 * and use the register-argument client ABI these processors are compiled with.
 *
 *  - DObjModelPart_PreUpdateA (0x30004550): RECONSTRUCTED as BG_PlayerAngles
 *    (functions/FUN_30004550_30004859.c). The .mcode "this in EDI + render entity"
 *    shape held, but the i386 proves EDI is the shared clientInfo_t and the stack
 *    arg is an entityState_t. The former partial DObj-model-effect overlay has
 *    been removed because CG_TransitionEntity proves the whole 0x4d0 row is copied.
 *  - BG_AnimPlayerConditions (0x30004860): RECONSTRUCTED
 *    (functions/FUN_30004860_30004d21.c). The caller-observed "DObjModelPart_PreUpdateB"
 *    guess is REJECTED: the machine code proves this is the anim-condition snapshotter
 *    (its own fatal string "\x15BG_AnimPlayerConditions: Vehicle type unknown: %i" at
 *    0x30071594, and it fills bgs.clientinfo[es->clientNum].conditionWords[*] exactly like the
 *    sibling BG_AnimUpdatePlayerStateConditions 0x300035f0). EDI is the entityState_t
 *    (es) and the __cdecl stack arg is the clientInfo_t (anim; only its viewPitch
 *    +0x3e8 is read). Declared after entityState_t below.
 *  - BG_PlayerAnimation_VerifyAnim (0x300042c0): RECONSTRUCTED
 *    (animation/bg_playeranimation_verifyanim.c). Takes an emitter in ESI and an
 *    XAnimTree pointer as the sole cdecl stack arg. Queries the current animation's
 *    weight via trap 0x96 (CG_XANIM_GET_WEIGHT, animTree, animNumber16); when that
 *    weight == 0.0f it resets the emitter's +0x10/+0x14/+0x18 fields
 *    (animationWord=0, animationOffset=0, blendTime=150). The 0x96 (150) is the
 *    value WRITTEN to +0x18 on reset,
 *    not an age threshold that is compared against.
 *  - BG_RunLerpFrameRate (0x30004050): RECONSTRUCTED (animation/bg_runlerpframerate.c).
 *    Register-argument client ABI: emitter in EAX, `self` (clientInfo_t) in
 *    ECX, renderEntity in EDI, and the per-emitter param
 *    (renderEntity->legsAnim/torsoAnim)
 *    as the sole __cdecl stack arg. Recomputes the emitter's per-frame move-speed
 *    blend fraction from the entity's motion and pushes the anim/blend rate to the
 *    engine (trap 0x91) via BG_SetNewAnimation. The prior 3-arg caller-observed
 *    decl omitted the EDI renderEntity argument (the sole caller sets EDI=EBP before
 *    each call); superseded here by the proven 4-arg signature.
 */
/*
 * entityState_t is the CoD network entity-state record (a fixed 0xf4-byte record).
 * It is defined ONCE, canonically, in globals.h (the base header, so the snapshot
 * entities[] array and these anim consumers share the one layout). BG_PlayerAngles
 * (0x30004550) reads this same record as its render entity — the fields it proves
 * (eFlags +0x08, eventParm +0x88, clientNum +0x94, weapon +0xcc, legsAnim +0x10 payload
 * at +0xd0) are named there.
 */

/* BG_CanItemBeGrabbed (0x30005e00) is declared with the other item-pickup helpers
 * near CG_TouchItem (its caller) later in this header; that decl is now the
 * callee's own reconstruction (functions/FUN_30005e00_30005f13.c), which supersedes
 * the earlier caller-observed placeholder. */

/*
 * BG_PlayerAnimation (0x30005860) — per-part update for a DObj model
 * effect record: run the two pre-update passes, time out both emitters, reset the
 * effect flag pair when the flags (masked with ~0x200) are clear, then advance
 * both emitters from the render entity's +0xd0/+0xd4 params. The Mac body has
 * the identical six-call structure, resolving the source name; the .mcode
 * size-guess "VectorNormalize2D" is rejected. */
/*
 * CG_DObjSetLocalTag (0x3001fd00) resolves tagName through trap 0xb2, marks the
 * resulting bone in the caller's four-word partBits through trap 0xa2, and only then calls
 * CG_DObjSetLocalTagInternal to write angles/origin. CG_DObjSetControlTagAngles
 * (0x3001fd50) is the trap-0xa3 control-tag sibling and supplies vec3_origin.
 * Both return false if name resolution or marking fails. Their i386 register ABI
 * carries self/tagName/angles in EDI/EAX/EBX; partBits is a stack pointer, not an
 * integer rot/trans index. */
qboolean CG_DObjSetLocalTag(DObj *self, const char *tagName, const uint32_t partBits[4], const vec3_t angles, const vec3_t origin);
qboolean CG_DObjSetControlTagAngles(DObj *self, const char *tagName, const uint32_t partBits[4], const vec3_t angles);

/*
 * CG_DObjCalcBoneGeneric (0x300220e0) — the entity-index form of
 * CG_DObjCalcBone. It resolves the DObj from the entity index, calculates the
 * requested bone hierarchy through the same four-word part bitset, and dispatches
 * controllers only for indices below MAX_GENTITIES. Exact name from the Mac
 * cgame symbol immediately preceding CG_DObjCalcBone.
 */
void CG_DObjCalcBoneGeneric(int32_t index, int32_t boneIndex);

/* ---------------------------------------------------------------------------
 * playerState_t event ring (Quake3 / CoD BG).
 *
 * The player state records recent predictable events in two parallel fixed-size
 * ring buffers indexed by a monotonically incrementing counter masked into
 * range: events[eventIndex & (MAX_PS_EVENTS-1)] and the matching eventParms[].
 *
 * Layout corroborated against the recovered server playerState_s
 * (src/server/game/recovered_game.h; the isolated bank copy in
 * cgame_mp/inputs/server_structs/ is identical). The client machine code
 * (0x30006550) proves +0x88 (write cursor), events[4] at +0x8c, and the parms
 * block at +0x9c, which matches the single server playerState_s layout exactly —
 * so the server field names are adopted: +0x88 = eventIndex (Q3-conventionally
 * "eventSequence"), +0x8c = events[4], +0x9c = predictableEventParms. Only the
 * fields this helper touches are modeled; the leading bytes are reserved to hold
 * the offsets. The full server struct has many more named fields to fill in as
 * their consumers are reconstructed. Client machine code remains the authority if
 * a future function proves a client/server layout difference.
 * ------------------------------------------------------------------------- */
/*
 * entityEvent_t event ids (the discriminant stored in playerState.events[] and
 * entityState.event). The complete shared domain is entityEvent_t.
 * CG_TouchItem (0x30035680) appends event id 148 (0x94) with the item modelindex
 * as the parm, i.e. the item-pickup predictable event.
 */

/*
 * Weapon raise/rechamber predictable events. PM_Weapon_CheckForRechamber
 * (0x300124b0) appends EV_EJECT_BRASS (168) when it aborts an in-progress
 * bolt-action raise (weaponState == WEAPON_STATE_RECHAMBERING and interrupt allowed) and
 * EV_RECHAMBER_WEAPON (167) when it starts a new rechamber. Both are appended with
 * parm 0. Values adopted from the recovered server entityEvent enum
 * (EV_RECHAMBER_WEAPON = 167 / EV_EJECT_BRASS = 168), whose numeric mapping
 * matches the client's inline ring writes exactly.
 */

/*
 * EV_PULLBACK_WEAPON (162, 0xa2) — predictable event appended to the playerState
 * event ring by PM_Weapon_StartFiring (0x30013f20) on the grenade-fire setup path
 * (weaponType == WEAPTYPE_GRENADE), with parm 0, when the current weapon's
 * clip is loaded and no grenade cook-off is already in progress. Exact source
 * entityEvent identity is confirmed by the shared event-name tables.
 */

/*
 * Predictable events appended (with parm 0) to the playerState event ring by
 * PM_Weapon (0x30014710) on its grenade cook-off "special time" (grenadeTimeLeft,
 * ps+0x34) handling path:
 *   - EV_GRENADE_SPOON (182 / 0xb6): appended on the attack-edge step, after
 *     grenadeTimeLeft is decremented by 10 while it is still at/above the weapon's
 *     specialTimeThreshold (0x30014814..0x30014846). Modeled by role.
 *   - EV_FIRE_WEAPON (163 / 0xa3): appended when the cook-off counter has run
 *     out (grenadeTimeLeft <= 50, clamped up to 50 first, 0x30014874..0x300148af).
 *   - EV_GRENADE_SUICIDE (210 / 0xd2): additionally appended on that same run-out
 *     path when the current weapon's missileSplashDamage flag (weaponInfo_t +0x37c)
 *     is set (0x300148b5..0x300148e8) -- the grenade is launched rather than only
 *     detonating in hand.
 * Names and values match the independently recovered entityEvent_t enum.
 */

/* playerState_t layout guards (playerState_t has no pointer fields, but keep the
 * established 32-bit guard so offsets are only checked at the target pointer width). */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
_Static_assert(offsetof(playerState_t, weaponTime) == 0x2c, "playerState weaponTime @ +0x2c");
_Static_assert(offsetof(playerState_t, leanFraction) == 0x44, "playerState leanFraction @ +0x44");
_Static_assert(offsetof(playerState_t, weaponDelay) == 0x30, "playerState weaponDelay @ +0x30");
_Static_assert(offsetof(playerState_t, legsTimer) == 0x70, "playerState legsTimer @ +0x70");
_Static_assert(offsetof(playerState_t, legsAnim) == 0x74, "playerState legsAnim @ +0x74");
_Static_assert(offsetof(playerState_t, torsoTimer) == 0x78, "playerState torsoTimer @ +0x78");
_Static_assert(offsetof(playerState_t, torsoAnim) == 0x7c, "playerState torsoAnim @ +0x7c");
_Static_assert(offsetof(playerState_t, currentWeapon) == 0xd8, "playerState currentWeapon @ +0xd8");
_Static_assert(offsetof(playerState_t, weaponState) == 0xdc, "playerState weaponState @ +0xdc");
_Static_assert(offsetof(playerState_t, damageEvent) == 0x10c, "playerState damageEvent @ +0x10c");
_Static_assert(offsetof(playerState_t, damageCount) == 0x118, "playerState damageCount @ +0x118");
_Static_assert(offsetof(playerState_t, stats[STAT_HEALTH]) == 0x11c, "playerState health @ +0x11c");
_Static_assert(offsetof(playerState_t, stats[STAT_MAX_HEALTH]) == 0x124, "playerState maxHealth @ +0x124");
_Static_assert(offsetof(playerState_t, ammo) == 0x134, "playerState ammo @ +0x134");
_Static_assert(offsetof(playerState_t, clips) == 0x334, "playerState clips @ +0x334");
_Static_assert(offsetof(playerState_t, weaponRechamberBits) == 0x54c, "playerState weaponRechamberBits @ +0x54c");
_Static_assert(offsetof(playerState_t, deltaAngles) == 0x4c, "playerState deltaAngles @ +0x4c");
_Static_assert(offsetof(playerState_t, groundEntityNum) == 0x58, "playerState groundEntityNum @ +0x58");
_Static_assert(offsetof(playerState_t, viewModelIndex) == 0xe4, "playerState viewModelIndex @ +0xe4");
_Static_assert(offsetof(playerState_t, viewAngles) == 0xe8, "playerState viewAngles @ +0xe8");
_Static_assert(offsetof(playerState_t, stats[STAT_DEAD_YAW]) == 0x120, "playerState deathYaw @ +0x120");
_Static_assert(offsetof(playerState_t, playerMaxs) == 0x568, "playerState playerMaxs @ +0x568");
_Static_assert(offsetof(playerState_t, proneViewHeight) == 0x574, "playerState proneViewHeight @ +0x574");
_Static_assert(offsetof(playerState_t, proneDirection) == 0x5a4, "playerState proneDirection @ +0x5a4");
_Static_assert(offsetof(playerState_t, proneTorsoPitch) == 0x5ac, "playerState proneTorsoPitch @ +0x5ac");
_Static_assert(offsetof(playerState_t, torsoHeight) == 0x608, "playerState torsoHeight @ +0x608");
_Static_assert(offsetof(playerState_t, aimSpreadScale) == 0x628, "playerState aimSpreadScale @ +0x628");
_Static_assert(offsetof(playerState_t, objectives) == 0x638, "playerState objectives @ +0x638");
_Static_assert(offsetof(playerState_t, hudCurrent) == 0x7f8, "playerState hudCurrent @ +0x7f8");
_Static_assert(offsetof(playerState_t, hudArchival) == 0x267c, "playerState hudArchival @ +0x267c");
_Static_assert(offsetof(playerState_t, deltaTime) == 0x4500, "playerState tail dword @ +0x4500");
_Static_assert(sizeof(playerState_t) == 0x4504, "playerState_t is the full 0x4504-byte playerState block");
#endif

/*
 * CG_CheckPlayerstateEvents (0x30034ec0) -- fire the client-predicted entity
 * events present in the new player state `ps` but not the old `ops`, so the local
 * player sees its own events without waiting for the server snapshot. Diffs the two
 * playerState event rings (eventIndex/events[]/eventParms[], MAX_PS_EVENTS==4),
 * dispatches each newly-appeared event through CG_EntityEvent on the static
 * cg_predictedEventEntity, and records it into cg_predictedEvents[]/
 * cg_predictedEventSequence. See functions/FUN_30034ec0_30034f4b.c. Register ABI:
 * `ps` in EBX, `ops` a single stack arg (caller-cleaned).
 */
void CG_CheckPlayerstateEvents(playerState_t *ps, playerState_t *ops);
void CG_CheckChangedPredictableEvents(playerState_t *ps);
void CG_UpdatePlayerDObj(centity_t *cent);

/*
 * CG_DamageFeedback (0x30034ac0) -- kick the view / paint the directional damage
 * blend when the local player is hit. Given the damage direction as separate
 * yaw/pitch BAMS angles and the damage magnitude, it computes the on-screen
 * damage-indicator direction and intensity and latches the view-kick state. Fed
 * by CG_TransitionPlayerState from playerState_t damageYaw/damagePitch/damageCount.
 * Same-module PPC symbol CG_DamageFeedback. See functions/FUN_30034ac0_30034d32.c.
 */
void CG_DamageFeedback(int32_t yaw, int32_t pitch, int32_t damage);

/*
 * CG_TransitionPlayerState (0x30034fe0) -- per-frame reconciliation of the new
 * player state `ps` against the previous one `ops`: fire the directional damage
 * feedback on a new damage event, run the local-sound checks, and replay the
 * predicted playerState events. Same-module PPC symbol CG_TransitionPlayerState;
 * the .mcode size-guess "Item_GetModelDef" is REJECTED (no item/model logic). See
 * functions/FUN_30034fe0_30035024.c. Register ABI: `ps` in EAX, `ops` in ESI.
 */
void CG_TransitionPlayerState(playerState_t *ps, playerState_t *ops);

/*
 * BG_TakePlayerWeapon (0x30011290) — remove one weapon from a player's inventory.
 * Shared reconstruction: src/bg/bg_weapon_inventory.c. Returns early with 0
 * if the weapon's owned bit in ps->weaponBits[] (+0x534) is not set. Otherwise:
 * asks BG_IsPlayerWeaponInSlot (0x30011460) which inventory slot the weapon occupies;
 * if it occupies one and the weapon is stackable (weaponInfo_t::stackable +0x88), scans
 * owned weapons 1..bg_numWeapons (0x30134cd4) for another one still in that slot and
 * reassigns ps->weaponSlots[] (+0x544) to it, else zeroes the slot entry; clears the
 * weapon's owned bit; then walks the weapon's altWeapon chain (weaponInfo_t +0x36c),
 * clearing each alt-weapon owned bit still set. Returns 1 on that main path. Callers
 * such as PM_RemoveEmptyClipOnlyWeapon (0x30013a00) ignore the result.
 *
 * Non-default register ABI, proven from the call at 0x30013a55 (no stack setup):
 * ps in ECX, weapon in EAX. The name is adopted from the server bank
 * (BG_TakePlayerWeapon(playerState_t *ps, int weapon), game_functions.h); the
 * server proto is stack-cdecl, whereas this client build passes both in registers
 * — recorded as a client/server ABI divergence. (The .mcode size-guess name
 * PM_Weapon_PrintWeaponAnim is rejected: this touches no anim string/print path.)
 */

/* BG_GivePlayerWeapon (0x30011160) — grant a non-turret/non-player weapon,
 * assign its inventory slot when available, and grant its alt-weapon chain.
 * Reconstructed from the repaired complete switch body. Register ABI in the DLL:
 * ps in EAX, weapon in ECX. */

/*
 * BG_IsPlayerWeaponInSlot (0x30011460) — query which inventory slot currently holds a
 * given weapon in the player's loadout (read-only sibling of BG_SetPlayerWeaponForSlot).
 * Returns 0 if the weapon is not held or not in a tracked slot, 1/2 for the primary
 * slots, or weaponInfo_t::slot (>=2) for the matched slot. The shared body is in
 * src/bg/bg_weapon_inventory.c; promoted here because
 * BG_TakePlayerWeapon (0x30011290) is a second caller. Register ABI: ps in EDI,
 * `weapon` and `checkAlt` on the stack (caller-cleaned). The exact name is
 * independently present in the Mac and Linux game-module symbol banks.
 */

/*
 * BG_GetStackSlotForWeapon (0x30011590) — decide which inventory slot a stackable
 * weapon should be placed into, given the player's current slot occupancy and a
 * preferred-slot hint. Returns the chosen weaponSlot_t (1..7), or 0 (NONE) if the
 * weapon is not itself stackable or cannot stack into any acceptable slot.
 * Shared reconstruction: src/bg/bg_weapon_inventory.c; promoted here
 * because CG_SelectFirstWeaponNotInSlot (0x300478a0) is a second
 * caller. Register ABI (compiler-chosen fastcall-style): weapon in ECX, preferred
 * slot in EAX, ps in ESI, caller-cleaned (plain RET), result in EAX. Expressed as
 * by the canonical shared signature (ps, weapon, preferredSlot). Name adopted
 * from the recovered server BG_GetStackSlotForWeapon (the .mcode size-guess is
 * rejected in the definition file).
 */

/*
 * BG_SetPlayerWeaponForSlot (0x300113f0) — try to place weapon `weapon` into the
 * player's inventory slot `slot`, returning qtrue on success, qfalse if the
 * placement is not allowed. Name and signature adopted from the recovered server
 * BG_SetPlayerWeaponForSlot(playerState_t *client, int slot, int weapon); the
 * .mcode header's size-matched BG_GetVehiclePosTag guess is rejected (that is a
 * one-arg const char* accessor with no slot/bitset logic).
 *
 * Non-default register ABI (compiler-chosen; the caller at 0x30012dfc sets these
 * in registers with no stack args and cleans nothing for this call): ps in EDI,
 * slot in ESI, weapon in EAX. Expressed as a normal C function taking
 * (ps, slot, weapon) — the recovered source shape.
 */

/*
 * The single entityStateFlags (+0x84) bit that BG_CalculateWeaponAngles
 * (0x30015920) tests (TEST [ps+0x84], 0x100000) to decide whether the angle offset
 * needs the vehicle-gunner-pose gate (vehicleType == 1 && vehiclePosition == 3). It is
 * one of the bits inside EF_RESTRICTED_MASK (0x106000 == 0x100000|0x4000|0x2000):
 * the angle path keys on only this bit while the bob path keys on the whole mask. The
 * exact source name for this bit is unresolved; named by its proven role.
 */
/*
 * BG_CalculateWeaponPosition_Sway (0x30015ca0) — the shared weapon-sway core.
 * Its complete Windows cgame/game and Linux game bodies are reconstructed in
 * src/bg/bg_weapon_sway.c. Given the player's current sway
 * scale, it blends the active weapon's hip sway envelope (bg_weaponInfos[ps->
 * currentWeapon] fields +0x2e4..+0x2f8) toward its ADS sway envelope (+0x300..
 * +0x318) by ps->adsFraction (ps+0xe0) — gated when adsEnabled (+0x328) is 0 —
 * then produces the animated sway output vectors, folding in the previous view
 * angles (ps->viewangles at ps+0xe8..0xf0).
 *
 * This callee uses a MIXED register/stack ABI (not plain cdecl): the three stack
 * pushes are (previous_view_angles:vec3*, scale:float, frametime:int), while the other
 * object pointers arrive in registers — ESI = const playerState_t*, EDI =
 * out_angles:vec2*, EAX = out_position:vec3*. Its three cgame callers
 * (0x3004098a, 0x300417b3, 0x30044c10) all set ESI/EDI/EAX and push the same three
 * stack slots. The source-level prototype records all six logical parameters even
 * though the original Windows i386 compiler assigned the first three below to registers.
 * Caller cleans the three pushed dwords (the callee's `ret` does not pop args ->
 * cdecl-on-stack for the pushed slots). The Linux game body at RVA 0x392b2
 * receives the same six source parameters through its stack ABI, independently
 * confirming their order and types. The Mac symbol bank supplies the canonical
 * BG_CalculateWeaponPosition_Sway name.
 */
/*
 * CG_WeaponSway_ApplyShellShock (0x30044c10) — the cgame-side weapon-sway entry
 * point that computes the current shell-shock sway scale for the predicted
 * player's active weapon and drives the sway core BG_CalculateWeaponPosition_Sway.
 * It reads the active weapon (bg_weaponInfos[cg_predictedPlayerState.currentWeapon]) and
 * the shell-shock time window (cg_shellShockStartTime/EndTime, duration from
 * cg_shellShockState->duration), eases a smoothstep envelope over that window and
 * scales from 1.0f toward the weapon's swayShellShockScale (weaponInfo_t+0x2fc);
 * outside/after the window the scale is exactly 1.0f. Name provisional by proven
 * role (matches the mechanical owner label bg_smoothweaponswayvalue and the
 * same-module PPC BG_CalculateWeaponPosition_Sway family); exact source name
 * unresolved. Reconstructed in functions/FUN_30044c10_30044cbe.c.
 */
void CG_WeaponSway_ApplyShellShock(void);

/*
 * bgAnimConditionTypes — global table mapping a condition `type` to how its state
 * is matched. Each 8-byte entry is { mode, values } (server
 * bg_anim_condition_type_s). BG_EvaluateConditions (0x30002ee0) reads only the
 * `mode` selector (entry +0x00) to choose the compare style:
 *   ANIM_CONDMODE_BITMASK (0): match if any bit of value[] overlaps the player's
 *                              two condition-state words.
 *   ANIM_CONDMODE_EQUAL   (1): match if value[0] equals the player's state word.
 *   otherwise               : condition is treated as satisfied.
 * Backed at 0x30082388; matches the recovered server table and complete DLL
 * initializer. `values` points to a bg_indexed_string_t value-name table.
 */
/* Eleven condition entries, matching the server table and DLL initializer. */

/*
 * bgAnimConditionTypeStrings — parallel table of condition *type* names, one
 * 8-byte bg_indexed_string_t per bgAnimConditionTypes entry. BG_ParseConditions
 * (0x30001cd0) passes its base as the table argument to BG_IndexForString to map a
 * script token ("WEAPONS", ...) to a condition-type index, then uses that index
 * into bgAnimConditionTypes for the match mode/value table. Backed at 0x30082328;
 * entry 0's name is the .rdata string 0x30072a38 ("WEAPONS"). It occupies 0x60
 * bytes through the NULL/-1 sentinel immediately before bgAnimConditionTypes at
 * 0x30082388. The mechanical export fragment captured only entry 0's truncated
 * name pointer as a uint32_t; this typed declaration supersedes it.
 */
extern bg_indexed_string_t bgAnimConditionTypeStrings[];

/*
 * weaponStrings — bgAnimConditionTypes[ANIM_COND_WEAPON].values, the
 * value table for the WEAPON anim-condition type. A fixed 128-entry
 * bg_indexed_string_t array (0x3053a040 .data, exactly 0x400 bytes; the next
 * distinct .data symbol is at 0x3053a440). Rebuilt by
 * BG_InitWeaponStrings (0x30001500): index 0 = {"none", hash},
 * indices 1..bg_numWeapons = { bg_weaponInfos[i]->pickupName, BG_StringHashValue }.
 * A trailing zeroed (NULL-name) entry terminates the table for BG_IndexForString.
 * Storage in globals.c; supersedes the mechanical g_data_cmd_veh_freevehicle_*
 * 0x3053a040/0x3053a044 fragments.
 */
extern bg_indexed_string_t weaponStrings[MAX_WEAPONS];

/*
 * BG_InitWeaponStrings (0x30001500) — clear and repopulate
 * weaponStrings from the registered weapon list so an anim-script
 * "weapon" condition token can be resolved to a weapon index. Standard cdecl,
 * no arguments. Source: uo_cgame_mp_x86.dll 0x30001500..0x30001591.
 */
void BG_InitWeaponStrings(void);

extern bgs_t bgs;

/*
 * NOT_FROM_ORIGINAL_SOURCE: native compatibility accessors for the original
 * i386 animation-entry pointer word. The i386 build retains that exact pointer
 * word; wider builds use a table-relative offset so the fixed 0x30-byte
 * legs/torso slot geometry does not truncate a host pointer.
 */
static inline bg_static_animation_t *cgame_compat_anim_entry_from_word(uint32_t animationWord)
{
    if (animationWord == 0) {
        return NULL;
    }

#if UINTPTR_MAX == UINT32_MAX
    return (bg_static_animation_t *)(uintptr_t)animationWord;
#else
    return (bg_static_animation_t *)(void *)((uint8_t *)&bgs.animationTable + animationWord);
#endif
}

/*
 * NOT_FROM_ORIGINAL_SOURCE: native compatibility setter paired with
 * cgame_compat_anim_entry_from_word.
 */
static inline uint32_t cgame_compat_anim_entry_to_word(const bg_static_animation_t *animation)
{
    if (animation == NULL) {
        return 0;
    }

#if UINTPTR_MAX == UINT32_MAX
    return (uint32_t)(uintptr_t)animation;
#else
    return (uint32_t)((const uint8_t *)animation - (const uint8_t *)&bgs.animationTable);
#endif
}

/*
 * Predictable player-state event ids pushed into playerState_t.events[] by the
 * aim-down-sight weapon-state transitions: PM_BeginWeaponDeploy (0x30012980) pushes
 * EV_DEPLOY_WEAPON (0xa0) when it enters weaponState WEAPON_STATE_DEPLOYING
 * (12), and PM_BeginWeaponBreakingdown (0x30012ab0) pushes
 * EV_BREAKDOWN_WEAPON (0xa1) when it enters WEAPON_STATE_BREAKING_DOWN (13).
 * Both values are written as bare immediates and named by the shared event-name
 * tables. */

/*
 * Predictable player-state event ids pushed into playerState_t.events[] by the
 * weapon-change state machine PM_BeginWeaponChange (0x30012bc0):
 *   - EV_WEAPON_ALT (0x9f): pushed on an alt-weapon switch (the next weapon
 *     is the current weapon's altWeapon), together with PM_StartWeaponAnim(0xf).
 *   - EV_PUTAWAY_WEAPON (0x9e): pushed on an ordinary put-away/change with a
 *     ready clip, together with PM_StartWeaponAnim(0x9).
 *   - EV_REMOVE_WEAPON_ATTACHMENTS (0xca): pushed by the no-change path (next weapon 0 /
 *     current weapon has no ready clip), with the change value as its parm.
 * All three are written as bare immediates and agree with the shared event-name
 * tables. */

/*
 * Predictable player-state event ids pushed into playerState_t.events[] by the
 * reload-begin paths:
 *   - EV_RELOAD       (0x99): PM_SetReloadingState (0x30012740) pushes it on an
 *     ordinary (partial) reload, together with the reload-pose weaponAnim
 *     (PM_WEAPON_ANIM_RELOAD, 0xb) merged inline.
 *   - EV_RELOAD_FROM_EMPTY (0x9a): PM_SetReloadingState pushes it on an empty-clip reload of
 *     a bullet weapon, together with PM_StartWeaponAnim(PM_WEAPON_ANIM_RELOAD_EMPTY,
 *     0xc).
 *   - EV_RELOAD_START (0x9b): PM_BeginWeaponReload (0x30012860) pushes it when it
 *     begins a segmented/interruptible reload (weaponState -> WEAPON_STATE_RELOAD_START),
 *     together with PM_StartWeaponAnim(PM_WEAPON_ANIM_RELOAD_START, 0xd).
 * All are written as bare immediates and agree with the shared event-name
 * tables. (These numeric values 0x99/0x9a also
 * appear as unrelated cgame trap ids CG_DOBJ_GET_CLIENT_NOTIFY_LIST/CG_DOBJ_CALC_ANIM;
 * here they are playerState event ids, a distinct namespace.) EV_RELOAD_END
 * (0x9c) is emitted by PM_Weapon_FinishReload when a segmented reload enters
 * its wind-down phase. */

/* Shared animation playback, command execution, event selection, and accessors are declared by bg_animation.h. */

/* Cgame-owned services installed in the shared displayContextDef_t record. */
void trap_R_RegisterFont(const char *name, int32_t pointSize, fontInfo_t *font, intptr_t loadMode);
int32_t trap_R_RegisterShaderNoMip(const char *name, int32_t loadMode);
int32_t CG_OwnerDrawWidth(int32_t ownerDraw, int32_t font, float scale);
int32_t CG_PlayCinematic(const char *name, float x, float y, float w, float h);
void CG_StopCinematic(int32_t handle);
void CG_DrawCinematic(int32_t handle, float x, float y, float w, float h);
void CG_RunCinematicFrame(int32_t handle);
const char *trap_SE_TranslateReference(const char *reference);
void trap_R_ClearScene(void);
void trap_R_RenderScene(const refdef_t *refdef);
void trap_R_ModelBounds(int32_t model, vec3_t mins, vec3_t maxs);
void trap_Key_GetBindingBuf(int32_t keynum, char *buffer, int32_t bufferSize);
void trap_Key_SetBinding(int32_t keynum, const char *binding);
void trap_Key_KeynumToStringBuf(int32_t keynum, char *buffer, int32_t bufferSize);

/*
 * NOT_FROM_ORIGINAL_SOURCE: semantic-float/native-register-ABI adapters for
 * original opaque-dword renderer trap wrappers on non-i386 hosts.
 */
void OpenCoDUO_UI_DrawTextAdapter(float x, float y, int32_t font, float scale, const vec4_t color, const char *text, float fixedAdvance,
                                  int32_t limit, int32_t textStyle);
void OpenCoDUO_UI_DrawStretchPicAdapter(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t shaderHandle);
int32_t OpenCoDUO_UI_TextWidthAdapter(const char *text, int32_t font, float scale, int32_t limit);
int32_t OpenCoDUO_UI_TextHeightAdapter(int32_t font, float scale);
void OpenCoDUO_UI_DrawTextWithCursorAdapter(float x, float y, int32_t font, float scale, const vec4_t color, const char *text,
                                            int32_t cursorPos, int8_t cursorChar, int32_t limit, int32_t textStyle);

void CG_OwnerDraw(float x, float y, float w, float h, float textX, float textY, int32_t ownerDraw, int32_t ownerDrawFlags,
                  int32_t alignment, float special, int32_t font, float textScale, vec4_t color, int32_t background, int32_t textStyle);
float CG_OwnerDrawValue(int32_t ownerDraw, int32_t colorRangeType);
void CG_Fade_f(void);
void CG_DrawPlayerAmmoValue(int32_t viewMode, const rectDef_t *rect, int32_t font, int32_t scaleBits, const vec4_t color,
                            int32_t textStyle);
void CG_DrawPlayerAmmoBackdrop(int32_t wantVehicleView, const rectDef_t *rect, const float *color, qhandle_t hShader);
void CG_DrawPlayerStance(const rectDef_t *iconRect, const float *colorVec, int32_t textArgA, int32_t textArgB, int32_t textArgC);
void CG_DrawCursorhint(rectDef_t *hintRect, int32_t fontContext, int32_t scaleBits, const vec4_t color, int32_t textStyle);
void CG_DrawPlayerWeaponName(const vec3_t color, rectDef_t *obj, int32_t regWord, int32_t arg0, int32_t arg1);
void CG_DrawPlayerWeaponNameBack(const vec3_t color, rectDef_t *rect, int32_t metricA, int32_t metricB, qhandle_t hShader);
void CG_DrawPlayerWeaponModeIcon(int mode, const rectDef_t *rect, const float *color);
void CG_DrawHudSlidePicColor(const rectDef_t *rect, qhandle_t hShader, const float *color);
void CG_DrawJeepBody(const rectDef_t *rect, int32_t hShader, const float *color);
void CG_DrawTankBody(const rectDef_t *rect, int32_t stateFilter, int32_t hShader, const float *color);


/* The complete common displayContextDef_t record and callback ABI live in
 * ui_display_context_types.h. */

/* String_Init, String_Hash, and String_Alloc are shared by ui_memory.h. */

/*
 * BG_AllowPlayerWeaponAtVehiclePos (0x30008210): RECONSTRUCTED -- see
 * src/client/cgame/prediction/bg_allowplayerweaponatvehiclepos.c. A small predicate over a player's
 * vehicle state: a weapon/change action is allowed only in the gunner pose, i.e.
 * returns 1 iff (vehicleType == 1 && vehiclePos == 3), else 0. Every caller feeds
 * it the adjacent playerState_t vehicle fields ps->vehicleType (+0x618) and
 * ps->vehiclePosition (+0x614): PM_Weapon_FinishWeaponChange (0x30012e70),
 * PM_Weapon_FireWeapon (0x300142a0), and two further cgame vehicle gates at
 * 0x30040620 and 0x300465f0.
 *
 * Name RESOLVED to the shared BG_* predicate from the server bank
 *   int BG_AllowPlayerWeaponAtVehiclePos(int vehicleType, int vehiclePos)
 * (game_functions.h; present in both cgame_mp.dll and game_mp.dll), proven by the
 * exact call-site dataflow -- NOT a size match. Supersedes the earlier provisional
 * placeholder PM_Weapon_VehiclePositionAllowsChange and adopts the server param
 * order (vehicleType first). ABI is fastcall-style: vehicleType in ECX, vehiclePos
 * on the stack ([ESP+4]); returns int in EAX. The shared declaration and
 * implementation are in bg_vehicle.h and src/bg/bg_vehicle.c.
 */

/* CG_CalcVehicleViewValues (0x30040580): derive the local player's view/refdef angles
 * while riding a vehicle/turret by attaching to the vehicle centity's "tag_player" DObj
 * bone, converting that world orientation to Euler angles, smoothing it against the
 * previous frame (cg_vehicleViewPrevAxis), applying the engine's SHORT2ANGLE-scaled angle
 * deltas, subtracting cg_adsViewErrorAngles, and committing pitch/yaw/roll into
 * cg_predictedPlayerState.viewAngles. Angle-producing sibling of CG_CalcVehicleViewPos (0x30040810).
 * void(void). src/client/cgame/vehicles/cg_calcvehicleviewvalues.c */
void CG_CalcVehicleViewValues(void);

/* CG_CalcVehicleViewPos (0x30040810): build the local vehicle/turret camera origin
 * from DObj seat/view tags, weapon sway, current refdef angles, and a final boxed
 * world trace. Reconstructed in functions/FUN_30040810_30041541.c. */
void CG_CalcVehicleViewPos(void);

/*
 * Predictable player-state event ids pushed into playerState_t.events[] by the
 * fire-commit path in PM_Weapon_FireWeapon (0x300142a0), chosen by whether the shot
 * emptied the clip:
 *   - EV_FIRE_WEAPON (0xa3): pushed when, after using ammo, the clip is NOT empty
 *     (PM_WeaponClipEmpty == qfalse) -- an ordinary shot.
 *   - EV_FIRE_WEAPON_LASTSHOT (0xa6): pushed when the shot emptied the clip
 *     (PM_WeaponClipEmpty == qtrue) -- the last round.
 * Both are written as bare immediates with a 0 eventParm and agree with the
 * shared event-name tables. (0xa3 also
 * appears as the unrelated cgame trap id CG_DOBJ_SET_CONTROL_ROT_TRANS_INDEX; here
 * these are playerState event ids, a distinct namespace.)
 */

/*
 * ConvertQuatToMat (0x3004b7c0) — in-place convert a quaternion to a 3x3 rotation
 * matrix. Reads the 4-float quaternion [x,y,z,w] at the buffer and overwrites the
 * same buffer with 9 floats (row-major 3x3): with s = 2/|q|^2,
 *   m00 = 1-(yy+zz)  m01 = xy+zw   m02 = xz-yw
 *   m10 = xy-zw      m11 = 1-(xx+zz) m12 = yz+xw
 *   m20 = xz+yw      m21 = yz-xw   m22 = 1-(xx+yy)
 * where xx=s*x^2, xy=s*x*y, etc. Only when |q|^2 == 0 does it store the
 * identity matrix; unordered values take the arithmetic path. Proven from the
 * x87 body (FLD 0.0f 0x3007bcec / FUCOMPP /
 * TEST AH,0x44 / JNP zero-guard; scale = 2.0f 0x3007bce4 / |q|^2; diagonals via
 * FSUBR 1.0f 0x3007bce0). Matches the VERIFIED server bank common_math.c:0x3d3cf
 * ConvertQuatToMat(float *quat); the globals.h owner label "convertquattomat"
 * corroborates. Register ABI: quaternion/matrix pointer in ECX (in-place, same
 * buffer read then written); no stack args (plain RET). Reconstruction:
 * the common source boundary is declared in q_math.h. The .mcode's size-matched
 * "CG_DrawFlashFade" guess is rejected (no trap, no 2D draw — pure
 * quaternion->matrix float math).
 */

/*
 * CG_DObjSetLocalTagInternal (0x3001fbb0) — write a DObj local-tag rot/trans slot
 * from Euler angles and an origin. Fetches the entity's rot/trans matrix array via
 * cgame_syscall(CG_DOBJ_GET_ROT_TRANS_ARRAY, self) and selects entry
 * `mat = &base[rotTransIndex]` (stride 0x20 DObjAnimMat). When `angles` is non-NULL
 * it builds three per-axis quaternions q_y(angles[1]) about Z, q_x(angles[0])
 * about X.y, q_z(angles[2]) about X.x (each {axis*sin, cos} with the angle scaled
 * by the exact pi/360 half-angle constant at 0x3007be78) and stores
 * mat->quat = (q1 * q2) * q3 via two
 * QuatMultiply calls; when `angles` is NULL it stores the identity quat {0,0,0,1}.
 * mat->trans is then {0, origin[0], origin[1], origin[2]}. Register ABI:
 * EAX=self, ECX=rotTransIndex, EBX=angles, plus one stack arg = origin. No return
 * value used. Callers: BG_Player_DoControllers (0x30005730) and the tag setters
 * at 0x3001eb41 / 0x3001fd1e / 0x30020708. Name from the same-module PPC bank
 * (cgame_mp CG_DObjSetLocalTagInternal); the .mcode's size-matched "G_FreeEntity"
 * guess is rejected (the body is an angles->quaternion tag write, not entity free).
 */
void CG_DObjSetLocalTagInternal(void *self, int rotTransIndex, const vec3_t angles, const vec3_t origin);

/*
 * VectorNormalize (0x30049700) — shared Quake3/CoD 3D in-place vector normalize
 * (the 3D sibling of VectorNormalize2D). Computes
 * length = sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]); if length != 0, scales
 * v[0], v[1] and v[2] by (1.0f / length) in place; returns the original 3D
 * length in either case (the length is left in ST0 on both return paths).
 * Proven from the recovered server signature (game_functions.h:
 * float VectorNormalize(float *v)) and the machine code (v[0]/v[1]/v[2] read
 * and written through ESI). The .mcode's size-matched "HudElem_GetMethod" guess
 * is rejected (matched only by byte size 0x7a == 0x7a, which the naming rules
 * forbid; HudElem_GetMethod is a method-string lookup with a different shape).
 *
 * Register-argument ABI: v arrives in ESI (caller-set), no stack args, `RET`
 * with no immediate; modeled here as a single pointer parameter.
 */
float VectorNormalize(vec3_t v);

/*
 * PlaneFromPoints (0x3004c690) — id-Tech/CoD common_math helper and the true
 * PlaneFromPoints sibling of NormalFromPoints above. Builds the plane through the
 * three points a, b, c (a is the shared vertex):
 *     d1 = b - a; d2 = c - a;
 *     CrossProduct(d2, d1, plane);          // plane[0..2] = un-normalized normal
 *     if (VectorNormalize(plane) == 0) return qfalse;   // degenerate/collinear
 *     plane[3] = DotProduct(a, plane); return qtrue;
 * Unlike NormalFromPoints it does NOT pre-normalize the edge vectors, it DOES
 * write plane[3] (the plane distance), and it returns a qboolean — exactly the
 * traits the NormalFromPoints note lists as identifying a real PlaneFromPoints, so
 * the corpus name is adopted here. See functions/FUN_3004c690_3004c748.c for full
 * instruction provenance. The .mcode's size-matched "VectorDistance" guess (win
 * size 0xb8 == 0xb8) is REJECTED: this is a cross product + plane distance, not a
 * distance sqrt.
 *
 * ABI (Ghidra __usercall): a in EDI, b in ECX, c in EAX, plane out in EDX (copied
 * to ESI); ESI saved/restored, ESP restored by ADD ESP,0x18, bare RET. Modeled as
 * the standard PlaneFromPoints(plane, a, b, c); the register split is an ABI
 * detail. Signature matches server_name_bank.txt (common_math.c:
 * int PlaneFromPoints(float *plane, const float *a, const float *b, const float *c)).
 */
int32_t PlaneFromPoints(vec4_t plane, const vec3_t a, const vec3_t b, const vec3_t c);

/*
 * Reconstructed pmove/BG step callees invoked by PmoveSingle (0x3000e050) and other
 * movers. Each has an accepted .mcode reconstruction (file cited); declared here so
 * every caller shares one signature. All take no arguments and read pm /
 * pml.weaponInfo from the globals.
 */

/*
 * ConstrainVectorTowardForward (0x300063e0) — name provisional-by-ROLE; the exact
 * CoD source symbol is unresolved. The .mcode's size-guessed "PM_Weapon_StartFiring"
 * is rejected (matched only by byte size 0x16f≈0x170, which the naming rules forbid;
 * the code has no weapon/firing state, only vec3 math).
 * Bends a working direction toward a forward axis until it lies inside an angular
 * cone: forward is forwardDir if length(forwardDir) >= 1.0 else (0,0,1); the working
 * vector starts at normalize(-reference); threshold = (forwardDir[2] > 0.8f)?0.7f:0.3f;
 * while dot(working, forward) < threshold the working vector is stepped by 0.5f*forward
 * and renormalized; the result is written to out. Register-argument ABI:
 * forwardDir=EDI, reference=ECX, out=EBX, `RET` with no immediate. See
 * src/animation/constrainvectortowardforward.c for the full instruction provenance.
 */
void ConstrainVectorTowardForward(const vec3_t forwardDir, const vec3_t reference, vec3_t out);

/*
 * VectorNormalize2 (0x30049920) — shared Quake3/CoD 3D vector normalize that
 * writes into a SEPARATE output vector (the src->dst sibling of VectorNormalize,
 * which normalizes in place). Computes
 * length = sqrt(in[0]*in[0] + in[1]*in[1] + in[2]*in[2]); if length != 0.0f,
 * writes out[i] = (1.0f / length) * in[i]; otherwise zeroes out[0..2].
 * Proven from the recovered server signature (game_functions.h:
 * float VectorNormalize2(const float *in, float *out)) and the machine code:
 * `in` is read through EDI (in[0]/in[1]/in[2]), `out` written through ESI, the
 * length compare is against the shared .rdata 0.0f (0x3007bcec) via the same
 * FLD/FLD/FLD ST1/FUCOMPP/FNSTSW/TEST AH,0x44/JNP idiom, and the reciprocal is
 * 1.0f (0x3007bce0) FDIV'd by length then multiplied per component (not a
 * per-component divide). The .mcode's size-matched "Rect_Parse" guess is rejected
 * (matched only by byte size 0x88 == 0x88, which the naming rules forbid;
 * Rect_Parse is a UI string/number token parser with an entirely different
 * shape). On the normalize path `length` is left in ST0 at RET and can serve as
 * a float return value, matching the server's float return; the zero path loads
 * no float, so the returned length is only meaningful when length != 0.
 *
 * Register-argument ABI: `out` arrives in ESI and `in` in EDI (caller-set), no
 * stack args, `RET` with no immediate; modeled here as two pointer parameters.
 */
float VectorNormalize2(const vec3_t in, vec3_t out);

/*
 * VectorNormalizeFast (0x30049890) — shared Quake3/CoD 3D in-place vector
 * normalize using the fast inverse-square-root (no sqrt call). Computes
 * ilength = Q_rsqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]) via the classic bit-hack
 * magic constant 0x5f3759df plus one Newton-Raphson iteration
 * (y = y * (1.5f - 0.5f*sumsq*y*y)), then scales v[0], v[1], v[2] by that
 * inverse length in place. Returns void (no length is produced). Proven from the
 * recovered server signature (common_math.c: void VectorNormalizeFast(float *v))
 * and the machine code (the 0x5f3759df magic, the 0.5f rdata const at 0x3007bce8,
 * the 1.5f rdata const at 0x3007be70, FSUBR for 1.5 - t, and v[0]/v[1]/v[2]
 * read+written through EAX). The .mcode's size-matched
 * "script_method_scriptbuiltin_settoparc" guess is rejected (matched only by byte
 * size 0x85 == 0x85, which the naming rules forbid; this is a pure float math
 * routine with no script/entity access).
 *
 * Register-argument ABI: v arrives in EAX (caller-set), no stack args, `RET` with
 * no immediate; modeled here as a single pointer parameter. No calling-convention
 * attribute is added because the syntax-only build does not require one.
 */
void VectorNormalizeFast(vec3_t v);

/*
 * sqrt helper (0x3006bee0) — statically-linked MSVC CRT square-root wrapper.
 * NOT client source (STATIC_LINKAGE msvc_crt): it takes its argument already on
 * the x87 stack (ST0), spills it as a double, and tail-calls the CRT `_CIsqrt`
 * chain, leaving sqrt(arg) in ST0. Its own .mcode's size-matched
 * "WeaponSlotsNotValid" guess is bogus. Declared here as a caller-observed
 * Portable recovered callers use the standard sqrtf spelling directly.
 */

/*
 * CG_pow (0x3006bb20) — statically-linked MSVC x87 `pow` intrinsic (`_CIpow`).
 * NOT client source (STATIC_LINKAGE msvc_crt): it takes base on ST(1) and exponent
 * on ST(0), leaving pow(base, exponent) on ST(0). Declared as a caller-observed
 * provisional (matching the sqrt_f pattern) so flame/physics callers name their
 * pow call instead of a bare FUN_ address. Argument order proven from the two
 * CG_AddFlameChunks (0x300272b0) call sites, which load base then push the exponent
 * constant last (ST0). Portable recovered callers use standard pow directly.
 */

/*
 * CG_CrtFloor (0x3005bcd0) — statically-linked MSVC CRT `floor` implementation.
 * NOT client source (STATIC_LINKAGE msvc_crt); large runtime body at
 * 0x3005bcd0..0x3005ef9a. Caller sites push one double on the stack (ADD ESP,8
 * after the call) and consume the floored double result on the x87 stack.
 * Portable recovered callers use standard floor directly.
 */

/*
 * crt_atan2f — MSVC CRT float atan2 intrinsic dispatcher (_CIatan2) reached via
 * the thunk at 0x3006beca (it loads the "atan2" descriptor into EDX and
 * tail-jumps into the shared FP library entry at 0x3006d400). NOT client source
 * (STATIC_LINKAGE msvc_crt); provisional caller-observed declaration until the
 * thunk's own .mcode is marked. The intrinsic takes its two arguments already on
 * the x87 stack, ST(1) = numerator and ST(0) = denominator, and returns
 * atan2(numerator, denominator) in radians. Portable recovered callers use
 * standard atan2f directly.
 */

/*
 * CG_FadeColor (0x3001d200) — shared Quake3/CoD "fade a HUD element out over
 * time" helper. Given the millisecond timestamp an event started (startMsec)
 * and the total lifetime of the fade (totalMsec), returns a pointer to a static
 * vec4_t RGBA color (cg_fadeColor, globals.h) whose RGB is white (1,1,1) and
 * whose alpha ramps from 1.0 down to 0.0 over the last CG_FADE_TIME (100) ms,
 * or NULL if the effect has not started (startMsec == 0) or has fully expired
 * (cg.time - startMsec >= totalMsec). This client build additionally scales the
 * returned alpha by the global cg_hudAlpha_vmCvar.value (color[3] = scale * frac),
 * which stock Quake3 CG_FadeColor does not do.
 *
 * The .mcode's size-matched "PM_AddTouchEnt" guess is rejected: PM_AddTouchEnt
 * is pmove touch-list bookkeeping (void, no x87), whereas this reads cg.time,
 * does an elapsed-time fade computation, and returns a color pointer — matching
 * the cgame_mp.dll PPC CG_FadeColor and the many callers in the HUD/overlay
 * draw cluster. Corroborated by CG_LatchOverlaySource (0x3001a5b0), which
 * latches (cg.time, duration) as this function's (startMsec, totalMsec).
 *
 * Register-argument ABI (custom regparm): startMsec arrives in EDX and totalMsec
 * in ECX (caller-set, e.g. 0x3001af46: MOV EDX,startMsec / MOV ECX,100), no
 * stack args, `RET` with no immediate. Modeled here as two int parameters in
 * source order; no calling-convention attribute is added because the syntax-only
 * build does not require one.
 */
enum {
    CG_FADE_TIME = 100
}; /* ms over which the fade alpha ramps 1.0 -> 0.0 */
vec_t *CG_FadeColor(int32_t startMsec, int32_t totalMsec);

/*
 * CG_DrawScoreboardHeader (0x300361d0) — provisional, caller-observed declaration
 * (superseded by its own .mcode reconstruction). Draws the scoreboard's
 * background/header block, scaled by the fade-alpha argument the top-level
 * CG_DrawScoreboard passes it (the callee FLDs it from [ESP+0x70] and multiplies
 * it into the header colors, then issues the 2D-draw traps 0x48/0x59). One 32-bit
 * float argument passed by value (cdecl; caller cleans it). Name is provisional
 * (role-proven: it is the first of the two scoreboard drawers CG_DrawScoreboard
 * invokes); the exact original symbol is unconfirmed.
 */
void CG_DrawScoreboardHeader(float fadeAlpha);

/*
 * CG_DrawScoreboardBody (0x30037b50) — provisional, caller-observed declaration
 * (superseded by its own .mcode reconstruction). Draws the scoreboard client rows
 * and columns for the frame: computes the panel height via CG_ScoreboardHeight
 * (0x30036e50) and emits the per-column text/pics, again scaled by the fade-alpha
 * argument. One 32-bit float argument passed by value (cdecl; caller cleans it).
 * Name is provisional (role-proven: it is the second scoreboard drawer
 * CG_DrawScoreboard invokes, and CG_ScoreboardHeight's own comment already refers
 * to this block as the "CG_DrawScoreboard body"); exact original symbol unconfirmed.
 */
void CG_DrawScoreboardBody(float fadeAlpha);

/*
 * cgScoreboardDrawCtx_t — the RGBA draw color the scoreboard body passes through
 * all scoreboard sub-drawers. The body initializes RGB to 1.0 and alpha to the
 * current scoreboard fade. CG_DrawObjectiveInfo reads all four fields;
 * the team/list drawers consume alpha.
 */
typedef struct cgScoreboardDrawCtx_s {
    vec4_t color; /* +0x00: shared scoreboard RGBA */
} cgScoreboardDrawCtx_t;
_Static_assert(sizeof(cgScoreboardDrawCtx_t) == 0x10, "cgScoreboardDrawCtx_t is one vec4");

/*
 * CG_DrawScoreboardTeamHeader (0x30037090) — draw one scoreboard team-section
 * header: the localized "<team name> (N players)" banner line and, for the
 * AXIS/ALLIES sections, a per-team totals row (cg_scoreboardTeamScores in the
 * score column, cg_scoreboardTeamPings in the ping column). Returns the Y at which the
 * section's client rows begin (y + 24.0f). Invoked four times per frame by
 * CG_DrawScoreboardBody (0x30037b50), once per team section.
 *
 * ABI proven from those four call sites (0x30037c49/c8f/cda/d28): drawCtx, y,
 * boardWidth, bannerHeight are four cdecl stack slots; `team` arrives in EBX and
 * the running line counter (`int *`) in EDX; the result is returned on the x87
 * stack. Register args are modeled as ordered trailing parameters — no
 * calling-convention attribute is added because the syntax-only build does not
 * need one. Reconstructed at 0x30037090
 * (src/client/cgame/hud/cg_drawscoreboardteamheader.c). The .mcode mechanical pre-hint
 * "G_Damage" is rejected: that is a server damage routine, unrelated to this
 * cgame scoreboard drawer. Name provisional by proven role; exact CoD symbol
 * unproven from the allowed inputs.
 */
float CG_DrawScoreboardTeamHeader(const cgScoreboardDrawCtx_t *drawCtx, float y, float boardWidth, float bannerHeight, int team,
                                  int *lineCounter);

/* cgScore_t (the collected scoreboard row, stride 0x18) is defined in globals.h
 * next to its backing array cg_scoreboardEntries[]. */

/*
 * CG_DrawScoreboard_GetTeamColor (0x30036f20) — provisional, caller-observed
 * declaration (superseded by its own .mcode reconstruction). Given a team
 * selector it writes an RGB color into the caller-supplied vec3 (register ABI:
 * the color-out pointer arrives in ESI; the team selector is the single stack
 * argument at [ESP+0x408]). It selects/parses a cvar-backed color string for the
 * team, then clamps each component into [0,1]. Named by proven role and the
 * same-module PPC bank (CG_DrawScoreboard_GetTeamColor); the color-out register
 * arg is modelled as an ordinary leading parameter for the syntax-only build.
 */
void CG_DrawScoreboard_GetTeamColor(int team, vec3_t colorOut);

/*
 * CG_DrawClientScore (0x30037420) — draws one scoreboard client row: optional
 * local/alternating background, status icon, name, ping, deaths, and score across
 * cg_scoreboardColumns[]. It clips against the scoreboard scroll/bottom state and
 * returns y + 12.0f only for a live visible client. The sole caller pushes five
 * cdecl slots (color, y, entry, boardWidth, alternateShade); the running counter
 * pointer arrives in EDX and is modeled as the final ordered parameter. Name is
 * proven by the scoreboard call graph and corroborated by the same-module PPC
 * bank. Reconstructed in FUN_30037420_300377fd.c.
 */
float CG_DrawClientScore(const vec_t *color, float y, const cgScore_t *entry, float boardWidth, int alternateShade, int *counter);

/*
 * CG_DrawScoreboard_ScoresList (0x30037810) — draws every collected scoreboard
 * row belonging to one team section and returns the advanced y position. Called
 * once per section by CG_DrawScoreboardBody (0x30037b50). See the function file
 * for the full evidence trace.
 */
float CG_DrawScoreboard_ScoresList(const cgScoreboardDrawCtx_t *drawCtx, float y, int team, float rowScale, int *rowCounter);

/*
 * CG_DrawScoreboard_ListColumnHeaders (0x30036d60) — draws the scoreboard's
 * localized column-header row (score/deaths/ping) at startY, advancing an X cursor
 * across the column table, and returns the baseline just below the headers
 * (startY + 14.0f). Reconstructed at src/client/cgame/hud/cg_drawscoreboard_listcolumnheaders.c.
 * ABI proven from the sole caller (CG_DrawScoreboardBody at 0x30037bde): startY
 * and the board pixel width are two cdecl float stack args, and the {1,1,1,alpha}
 * draw-color pointer arrives in EBX (modeled as a trailing int32 word, forwarded
 * verbatim to the trap-54 text draws). Float x87 return.
 */
float CG_DrawScoreboard_ListColumnHeaders(float startY, float widthScale, const vec4_t color);

/*
 * CG_DrawObjectiveInfo (0x30036900) — draw the scoreboard's localized,
 * width-wrapped cg_objectiveText band and its separator rule, returning the next
 * Y coordinate on the x87 stack. When no snapshot exists it also services the
 * synchronous loading-screen redraw before drawing the rule. The two cdecl args
 * are the scoreboard draw-color context and the starting Y coordinate (52.0f at
 * the sole caller); caller cleans 8 bytes. Name proven by behavior and scoreboard
 * call graph. Reconstructed in FUN_30036900_30036d5e.c.
 */
float CG_DrawObjectiveInfo(const cgScoreboardDrawCtx_t *drawCtx, float y);

/*
 * CG_DrawScoreboard_ScrollIndicators (0x300378b0) — reconstructed. When the
 * collected scoreboard list overflows the visible area, this draws the vertical
 * scrollbar down the right edge: a dimmed "black" track (trap_R_DrawStretchPic), a
 * dimmer "white" proportional thumb, and up/down arrow+key glyphs (CG_DrawPic) at
 * the ends of the track. It registers each shader on demand via
 * cgame_syscall(CG_R_REGISTERSHADER, name, 5), preceded by a CG_DrawInformation(0)
 * loading pump, and sets the 2D draw color per element via trap_R_SetColor.
 *
 * ABI (re-derived from this function's own bytes; the earlier 3-arg provisional
 * missed the EBX register arg). FOUR inputs:
 *   - color  : ESI register arg — pointer to the {r,g,b,alpha} draw color vec4
 *              (reads color[0..2] and color[3]=alpha). Register arg, never saved.
 *   - topY   : first cdecl stack arg ([R0+4], the caller's ECX). The header-baseline
 *              Y where the track top / up-arrow sit.
 *   - lineCount        : second cdecl stack arg ([R0+8], the caller's EDI). Total
 *              collected line count; the caller cleans these two (ADD ESP,8).
 *   - visibleLineCount : EBX register arg (the caller's running line cursor). Never
 *              saved/restored, confirming an incoming register arg. Drives the thumb
 *              height and the down-arrow visibility.
 * No return (bare RET after ADD ESP,0x1c). The two register args (color in ESI,
 * visibleLineCount in EBX) are modelled as ordinary parameters for the syntax-only
 * build, mirroring the sibling CG_DrawScoreboard_ListColumnHeaders EBX-colorPtr
 * convention.
 */
void CG_DrawScoreboard_ScrollIndicators(const vec_t *color, float topY, int lineCount, int visibleLineCount);

/*
 * CG_DrawScoreboard (0x30037d90) — top-level multiplayer scoreboard draw. Returns
 * qfalse (and draws nothing) when the draw-inhibit gate cl_paused_vmCvar.integer is
 * set or the scoreboard-enable gate timescale_vmCvar.integer is clear. When
 * the scoreboard is showing (cg_scoreboardShowing) it draws at full alpha; when it
 * is hiding it fades via CG_FadeColor(cg_scoreboardShowTime, 100) and returns
 * qfalse once the fade has fully expired (emptying cg_fraggedByName). It also
 * re-requests the "score" info from the server at most once every 2000 ms via
 * cgame_syscall(CG_SEND_CLIENT_COMMAND, "score"), then draws the header and body.
 * Takes no arguments; returns qboolean (EAX 0/1).
 */
qboolean CG_DrawScoreboard(void);

/*
 * CG_GetEffectOriginAxis (0x3002ae70) — script/VM builtin (dispatched from the
 * cgame builtin table at 0x3002af..) that indexes cg_entities and returns its
 * placement: cg_entities[index].lerpOrigin (+0x208) is copied to *outOrigin,
 * and an orientation axis is derived from lerpAngles (+0x214). The axis
 * is built AnglesToAxis-
 * style: AngleVectors(angles, forward=outAxis[0], right=tmp, up=outAxis[2]) then
 * outAxis[1] = -tmp (the "left" basis row, 0.0f - right computed via FLD 0.0 /
 * FSUB). Name is provisional-by-role; the exact source name is not proven (the
 * .mcode's size-matched "Menu_SetupKeywordHash" guess is rejected — this does
 * effect-pool math and vector orientation, nothing menu/hash related).
 *
 * Register-argument ABI (custom regparm, from the sole caller 0x3002b085): index
 * in EAX, outOrigin in ECX, outAxis in EDX; no stack args, `RET`, returns void.
 * Modeled here as ordered parameters; no calling-convention attribute is added
 * because the syntax-only build does not require one.
 */
void CG_GetEffectOriginAxis(int32_t effectIndex, vec3_t outOrigin, axis_t outAxis);

/*
 * CG_ScrollScoreboardDown (0x30037e60) — scroll the scoreboard list down by one
 * step. Reads only globals: if the scoreboard-active companion flag
 * (cg_scoreboardOverflowed, 0x3048a564) is nonzero it computes
 * cg_scoreboardScrollPos + cg_scoreboardScrollStep_vmCvar.integer, clamps that to
 * (cg_scoreboardNumClients - 1), and stores it back. Takes no arguments
 * and returns void (`RET`, no stack cleanup). Name adopted by proven behavior
 * from the cgame_mp bank (CG_ScrollScoreboardDown); the sibling
 * CG_ScrollScoreboardUp step is inlined directly inside CG_KeyEvent in this
 * client build rather than called. Reconstructed from its own .mcode
 * (0x30037e60); see src/client/cgame/hud/cg_scrollscoreboarddown.c.
 */
void CG_ScrollScoreboardDown(void);

/*
 * CG_KeyEvent (0x30032780) — cgame VM key-event handler (dispatched from vmMain
 * at command index 7, called as handler(key, down)). Acts only on key-press
 * (down != 0) while the scoreboard is showing, and consumes only the scoreboard
 * scroll keys. Returns qtrue when the key was a handled scoreboard scroll key,
 * qfalse otherwise (including key-release and while not showing). Name is cgame VM
 * key-event slot; provisional (the bank's trivial 4-byte CG_KeyEvent is a
 * different stub and no scoreboard-specific key-handler name is in the banks).
 */
qboolean CG_KeyEvent(int32_t key, qboolean down);

/*
 * The canonical hudElem_t is the animated 2D-coordinate value node evaluated by the
 * parallel sibling helpers CG_HudElemX (component 0, stored to item+0x00) and
 * CG_HudElemY (component 1, stored to
 * item+0x04). Both are invoked from FUN_30029c00 with EAX = the node (EBP) and
 * ESI = the cgAlignedDrawItem being built. The two helpers are byte-for-byte
 * parallel and differ only by a +4 stride on the per-component fields, which
 * proves those fields are paired X/Y values:
 *   +0x04/+0x08  x/y             settled/goal endpoints (int -> float)
 *   +0x14/+0x18  alignX/alignY   owner-relative adjustment selectors
 *   +0x4c/+0x50  moveFromX/Y     previous interpolation endpoints (int -> float)
 * The interpolation window is shared across both components:
 *   +0x54        moveStartTime   cg_time base of the interpolation
 *   +0x58        moveTime        interpolation length in ms (signed; <=0 => settled)
 * The former cgCoordValueNode_t partial overlay was redundant and has been removed.
 */
/*
 * Coordinate value-node evaluators. Their owner register is the actual
 * cgAlignedDrawItem built by CG_GetHudElemInfo: component 0 subtracts
 * item->width (+0x08), component 1 subtracts item->height (+0x0c), and the
 * caller stores their results to item->x/item->y (+0x00/+0x04). Compiler
 * register-argument helpers (EAX = node, ESI = item; no stack args, plain RET).
 */
long double CG_HudElemX(const hudElem_t *node, const cgAlignedDrawItem *item);
long double CG_HudElemY(const hudElem_t *node, const cgAlignedDrawItem *item);

/*
 * hudElemType_t and hudElem_t (the single HUD element descriptor, +0x00 type,
 * +0x6c float sortKey, stride 0x7c) are defined in the shared
 * player_state_types.h boundary included by globals.h. The embedded playerState
 * HUD arrays are therefore complete here.
 */

/*
 * CG_GetSortedHudElems (0x3002a440) — reconstructed; see
 * src/client/cgame/hud/cg_getsortedhudelems.c. Gathers the active HUD elements
 * (type != 0) from the local playerState's two hud arrays (cg_snap->ps.hudArchival
 * then cg_snap->ps.hudCurrent, each PLAYERSTATE_HUD_ELEM_COUNT entries) into
 * `sortedList` as an array of hudElem_t pointers, qsorts them ascending by
 * hudElem_t.sortKey (+0x6c),
 * and returns the number gathered. ABI: the list pointer arrives in EDX; count
 * returned in EAX. Called only by CG_DrawHudElems (0x3002a4a0), which then draws
 * each returned element via CG_DrawSingleHudElem (0x3002a310). The .mcode size-guess
 * name `BG_WeaponTrackValue` is rejected (see the .c). */
int CG_GetSortedHudElems(hudElem_t **sortedList);

/*
 * CG_GetHudElemTime (0x30029780) — return a HUD element's timer value in
 * milliseconds, clamped to >= 0. Reads the element's timer-mode discriminant at
 * +0x00 (a jump table over hudElemType_t values 4..9, the timer/clock types) and
 * its stored reference time at +0x5c (timerValue), and computes remaining time
 * (timerValue - cg_time, countdown types 4/6/8) or elapsed time (cg_time -
 * timerValue, count-up types 5/7/9) relative to cg_time; a computed negative
 * result is clamped to 0. Countdown types add a rounding bias (+999 ms for the
 * whole-second timer, +99 ms for the tenths timer). Register ABI proven from
 * 0x30029780's own bytes and the sibling timer-string call sites: the element
 * pointer arrives in ECX, return in EAX; plain `RET`, no stack args. Exact
 * original symbol unresolved (role name; the PPC bank names the string wrappers
 * CG_HudElemTimerString / CG_HudElemTenthsTimerString but not this shared getter).
 */
int32_t CG_GetHudElemTime(const struct hudElem_s *elem);

/*
 * CG_HudElemTimerString (0x300297f0) — format a HUD element's timer value (from
 * CG_GetHudElemTime) as "h:mm:ss" when hours are nonzero, else "m:ss", via
 * the `va` ring-buffer formatter. Element pointer arrives in ECX. Name proven by
 * behavior + call graph and the PPC cgame_mp.dll bank (CG_HudElemTimerString).
 */
const char *CG_HudElemTimerString(const struct hudElem_s *elem);

/*
 * CG_HudElemTenthsTimerString (0x30029870) — format a HUD element's timer value
 * (from CG_GetHudElemTime, in milliseconds) as "h:mm:ss.t" when hours are
 * nonzero, else "m:ss.t" (t = tenths of a second), via the `va` ring-buffer
 * formatter. Identical to CG_HudElemTimerString except it keeps one decimal digit:
 * it first divides the millisecond value by 100 (centiseconds) and decomposes into
 * hours (/36000), minutes (/600), seconds (/10) and tenths (%10). Element pointer
 * arrives in ECX (__thiscall). Name proven by behavior + call graph (sibling of
 * CG_HudElemTimerString, shares the CG_GetHudElemTime getter) and the PPC
 * cgame_mp.dll bank (CG_HudElemTenthsTimerString).
 */
const char *CG_HudElemTenthsTimerString(const struct hudElem_s *elem);

/* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
void ProjectPointOnPlane(vec3_t dst, const vec3_t point, const vec3_t normal);

/*
 * ADJUDICATED (was cg_adsAnimState_t): there is NO separate aim-down-sight
 * zoom/FOV animation struct. The three helpers once modelled here as ADS-anim
 * code — CG_FlameGetSizeRate (0x30023b70), CG_AdvanceFlameChunkSize (0x30025c60),
 * and CG_UpdateFlameChunk (0x30023c30) — all operate on a flameChunk_t. Every
 * offset the "ADS" model reserved is a defined flameChunk_t field: +0x2c=kind,
 * +0x34=ownerInfoIndex, +0x48=spawnTime, +0x50=endTime, +0x5c=startSpeedBits,
 * +0x60=smokeDensityRate, +0x64=sizeRate, +0x68=spawnTimeCopy, +0x94=driftSpeed,
 * +0xa0=deadFlag, +0xa4=birthTime, +0xb8=soundAmpRate, +0xe4=radius. Every caller
 * (0x30023d3a, 0x30025c73, 0x30025faa, 0x30027825, 0x30027c42, and the four sites
 * in CG_FireFlameChunks) passes a flame chunk in EAX/ESI, and all live in the
 * flame subsystem cluster. The struct, its offset asserts, and the ADS-named decls
 * are therefore removed; the flameChunk_t definition and the flame-role decls
 * (CG_FlameGetSizeRate / CG_AdvanceFlameChunkSize / CG_UpdateFlameChunk) below are
 * the single source of truth.
 */

/* Preserve this recovered boundary's validated input, state, and compatibility invariants. */

struct centity_s {
    entityState_t currentState; /* +0x000..+0x0f3 */

    /* +0xf4: dual-view region. CG_AddPlayerCorpseEntity (0x300346c0) forms
     * &cent->corpseModelInfo and reads it as the corpse DObj model-info sub-object,
     * while CG_TransitionEntity (0x3003c7c0) and its caller (0x3003d020) treat the same
     * bytes as the entity's nextState (the incoming snapshot entityState copied in here,
     * then evaluated/dispatched at transition). Both views span +0xf4..+0x1e7. */
    union {
        entityState_t corpseModelInfo; /* +0xf4: corpse/DObj entity-state view */
        entityState_t nextState; /* +0xf4: incoming snapshot state */
    };
    qboolean currentValid; /* +0x1e8: current snapshot state is valid. CG_ResetEntity
                               * copies nextState to currentState and writes 1 here; snapshot
                               * teardown paths write 0. DObj/tag consumers test this flag
                               * before asking the engine for the entity's DObj handle. */
    qboolean weaponEffectActive; /* +0x1ec: ordinary weapon-fire effect latch */
    int32_t previousEvent; /* +0x1f0: highest event sequence CG_CheckEvents (0x300238e0)
                               * has already dispatched for this entity. Doubles as a
                               * one-shot "fired" flag for pure ET_EVENTS entities (set to 1
                               * so the event plays exactly once). Client centity_t
                               * previousEvent; provisional exact source name. */
    int32_t modelPreviousEvent; /* +0x1f4: highest event sequence CG_CheckPreEvents
                               * (0x300239e0) has already dispatched for this entity's corpse-model
                               * sub-entity (corpseModelInfo at +0xf4). Doubles as the one-shot
                               * "fired" flag for a pure model event entity (eType > ET_EVENTS: set
                               * to 1 so the encoded event plays once). Parallel to previousEvent
                               * (+0x1f0) but for the model sub-entity's event ring. Provisional
                               * exact source name. Proven at 0x300239ec (MOV [ESI+0x1f4]). */
    int32_t laserEffectStarted; /* +0x1f8: one-shot latch so the laser/muzzle tag effect
                               * is played exactly once. CG_Missile (0x3001edb0) plays the
                               * tag_origin effect only when this is 0, then sets it to 1
                               * after a successful CG_RESOLVE_TAG. Provisional field name
                               * (exact centity_t source name unproven). */
    int32_t miscTime; /* +0x1fc: a per-entity client-time (ms) timestamp. The vehicle
                               * render handler CG_AddCEntity_Vehicle (0x30021660) derives the
                               * DObj model's shaderTime (age in seconds) as
                               * (cg.time - miscTime) * 0.001f. CG_Vehicle_DoControllers
                               * (0x30020540) sets it to -1 when currentState.time < 0;
                               * otherwise it lerps currentState.time to nextState.time in
                               * seconds and converts the result back to integer milliseconds.
                               * Provisional field name (exact centity_t source name unproven). */
    int32_t gasFireTime; /* +0x200: cg.time of the latest gas-weapon fire */
    int32_t flashSoundLifetime; /* +0x204: remaining lifetime (ms) of this entity's
                               * muzzle-flash sound emission. CG_WeaponUpdateLoopingSound
                               * (0x300490b0) decrements it by cg_frametime each frame
                               * (only while it is > 0); while it stays > 0 the looping
                               * fire sound (cgWeaponInfo.flashLoopSound, +0x128) is
                               * played, and on the frame it reaches <= 0 the tail sound
                               * (cgWeaponInfo.flashTailSound, +0x12c) plays once. A value
                               * <= 0 on entry skips the whole emitter. Provisional field
                               * name (exact centity_t source name unproven). */
    vec3_t lerpOrigin; /* +0x208: canonical centity_t interpolated origin. */
    vec3_t lerpAngles; /* +0x214: canonical centity_t interpolated angles. */
    vec3_t smoothedWeaponAngles; /* +0x220: view-weapon angles smoothed across frames.
                               * The refEntity builder at 0x3001e380 seeds this from
                               * lerpOrigin on first use (sentinel 0.5 per component)
                               * and zeroes it when the EF flag is clear. Provisional name. */
    int32_t loopedFxNextTime; /* +0x22c: cg.time (ms) at which this looped emitter should next
                               * spawn its effect. CG_AddCEntity_LoopedFx (0x30021a30) clamps it
                               * up to cg.time when it has fallen behind, and otherwise advances
                               * it by loopedFxInterval (+0x6c) until it is within one interval
                               * of cg.time, returning without playing when the interval has not
                               * yet elapsed. Provisional field name. */
    int32_t voiceChatIcon; /* +0x230: registered head-icon shader handle shown while
                               * this client is voice-chatting. Written by CG_PlayVoiceChat
                               * (0x30039ff0) from cgs_voiceChatIcon (the "headiconVoiceChat"
                               * shader handle at 0x3044b6cc). Provisional field name; role
                               * proven by the writer, exact centity_t source name unproven. */
    int32_t voiceChatTime; /* +0x234: cg.time deadline until which voiceChatIcon is shown.
                               * CG_PlayVoiceChat sets it to cg.time + duration, or
                               * cg.time + 2*duration when the icon is the default voice-chat
                               * icon. Provisional field name. */
    qboolean predictionCollisionActive; /* +0x238: temporary predicted-player
                               * collision participation flag. The predictor sets it
                               * around Pmove for the contacted ET_VEHICLE entity and
                               * clears it immediately afterward. */
    vec3_t vehicleWheelLastOrigin[6]; /* +0x23c..+0x283: one persistent world-space
                               * contact origin per vehicle wheel. The six-wheel
                               * DObj pass at 0x30020540 advances this pointer by
                               * 0x0c with the wheel-tag table, initializes an all-zero
                               * slot from the current contact, and otherwise emits
                               * tread dust after it moves more than 32 and no more
                               * than 200 units. */
    /* +0x284: corpse tag-state sub-object. CG_AddPlayerCorpseEntity (0x300346c0)
     * forms &cent->corpseTagState and passes it to CG_UpdateCorpseModelPartState
     * (0x300058f0), which reads its leading byte ([edx]) while (re)binding the
     * corpse skeleton. Modeled opaque (only its address and leading byte are
     * observed here). Provisional field name. */
    uint8_t corpseTagState[4]; /* +0x284 */
}; /* struct centity_s; typedef to centity_t is the forward decl above */
_Static_assert(sizeof(struct centity_s) == 0x288, "centity_t stride 0x288");
_Static_assert(offsetof(struct centity_s, currentState.eType) == 0x4, "centity eType +0x4");
_Static_assert(offsetof(struct centity_s, currentState.eFlags) == 0x8, "centity eFlags +0x8");
_Static_assert(offsetof(struct centity_s, currentState.pos) == 0x0c, "centity currentState.pos +0x0c");
_Static_assert(offsetof(struct centity_s, currentState.origin) == 0x18, "centity currentState.origin +0x18");
_Static_assert(offsetof(struct centity_s, currentState.apos) == 0x30, "centity apos +0x30");
_Static_assert(offsetof(struct centity_s, currentState.time) == 0x54, "centity time +0x54");
_Static_assert(offsetof(struct centity_s, currentState.iconFadeEndTime) == 0x54, "centity iconFadeEndTime +0x54");
_Static_assert(offsetof(struct centity_s, currentState.origin2) == 0x5c, "centity origin2 +0x5c");
_Static_assert(offsetof(struct centity_s, currentState.angles2) == 0x68, "centity angles2 +0x68");
_Static_assert(offsetof(struct centity_s, currentState.iconBaseYaw) == 0xd8, "centity iconBaseYaw +0xd8");
_Static_assert(offsetof(struct centity_s, currentState.leanf) == 0xd8, "centity leanf +0xd8");
_Static_assert(offsetof(struct centity_s, currentValid) == 0x1e8, "centity currentValid +0x1e8");
_Static_assert(offsetof(struct centity_s, weaponEffectActive) == 0x1ec, "centity weaponEffectActive +0x1ec");
_Static_assert(offsetof(struct centity_s, currentState.vehicleEntityNum) == 0x74, "centity vehicleEntityNum +0x74");
_Static_assert(offsetof(struct centity_s, currentState.compassBlipIndex) == 0x78, "centity compassBlipIndex +0x78");
_Static_assert(offsetof(struct centity_s, currentState.stateFilter) == 0x88, "centity stateFilter +0x88");
_Static_assert(offsetof(struct centity_s, currentState.surfType) == 0x88, "centity surfType +0x88");
_Static_assert(offsetof(struct centity_s, currentState.modelIndex) == 0x90, "centity modelIndex +0x90");
_Static_assert(offsetof(struct centity_s, currentState.clientNum) == 0x94, "centity clientNum +0x94");
_Static_assert(offsetof(struct centity_s, currentState.iHeadIcon) == 0x98, "centity iHeadIcon +0x98");
_Static_assert(offsetof(struct centity_s, currentState.headIconTeam) == 0x9c, "centity headIconTeam +0x9c");
_Static_assert(offsetof(struct centity_s, currentState.eventParm) == 0xa4, "centity eventParm +0xa4");
_Static_assert(offsetof(struct centity_s, currentState.eventSequence) == 0xa8, "centity eventSequence +0xa8");
_Static_assert(offsetof(struct centity_s, currentState.events) == 0xac, "centity events +0xac");
_Static_assert(offsetof(struct centity_s, currentState.eventParms) == 0xbc, "centity eventParms +0xbc");
_Static_assert(offsetof(struct centity_s, currentState.weapon) == 0xcc, "centity weapon +0xcc");
_Static_assert(offsetof(struct centity_s, currentState.scale) == 0xdc, "centity scale +0xdc");
_Static_assert(offsetof(struct centity_s, previousEvent) == 0x1f0, "centity previousEvent +0x1f0");
_Static_assert(offsetof(struct centity_s, currentState.hudTagMask) == 0xe4, "centity hudTagMask +0xe4");
_Static_assert(offsetof(struct centity_s, currentState.animMovetype) == 0xe4, "centity animMovetype +0xe4");
_Static_assert(offsetof(struct centity_s, currentState.turretOverheatState) == 0xe4, "centity turretOverheatState +0xe4");
_Static_assert(offsetof(struct centity_s, lerpOrigin) == 0x208, "centity lerpOrigin +0x208");
_Static_assert(offsetof(struct centity_s, lerpAngles) == 0x214, "centity lerpAngles +0x214");
_Static_assert(offsetof(struct centity_s, smoothedWeaponAngles) == 0x220, "centity smoothedWeaponAngles +0x220");
_Static_assert(offsetof(struct centity_s, corpseModelInfo) == 0xf4, "centity corpseModelInfo +0xf4");
_Static_assert(offsetof(struct centity_s, modelPreviousEvent) == 0x1f4, "centity modelPreviousEvent +0x1f4");
_Static_assert(offsetof(struct centity_s, gasFireTime) == 0x200, "centity gasFireTime +0x200");
_Static_assert(offsetof(struct centity_s, voiceChatIcon) == 0x230, "centity voiceChatIcon +0x230");
_Static_assert(offsetof(struct centity_s, voiceChatTime) == 0x234, "centity voiceChatTime +0x234");
_Static_assert(offsetof(struct centity_s, vehicleWheelLastOrigin) == 0x23c, "centity vehicle wheel origins +0x23c");
_Static_assert(offsetof(struct centity_s, corpseTagState) == 0x284, "centity corpseTagState +0x284");

/* CG_CheckEvents (0x300238e0) — fire the entity events that have been queued on one
 * client entity since it was last checked. For a pure event entity (eType > ET_EVENTS)
 * it dispatches the encoded event once; otherwise it walks the (previousEvent,
 * eventSequence] span of the currentState event ring, calling CG_EntityEvent for each.
 * Register-in-ESI ABI; declared here so the client-entity update path can reuse it. */
void CG_CheckEvents(centity_t *cent);

/* CG_CheckPreEvents (0x300239e0) — the corpse-model analogue of CG_CheckEvents:
 * fire the events queued on a client entity's embedded model sub-entity
 * (cent->corpseModelInfo at +0xf4) since it was last checked. Same algorithm as
 * CG_CheckEvents but over the model record's own entityState-shaped ring
 * (eType +0xf8, previousEvent +0x1f4, eventSequence +0x19c, events[]/eventParms[]
 * +0x1a0/+0x1b0, eventParm +0x198) and dispatching through CG_EntityPreEvent
 * (0x30023690) instead of CG_EntityEvent. Register-in-ESI ABI. See its own .c file
 * for the full machine-code derivation. Name is a role name paralleling the proven
 * CG_CheckEvents; the mechanical "CMD_VEH_SetTurretTargetEnt" size guess is rejected. */
void CG_CheckPreEvents(centity_t *cent);

/* Sentinel each component of centity_t.smoothedWeaponAngles carries before it
 * has been seeded; the builder at 0x3001e380 seeds from lerpOrigin only while all
 * three components still equal this value. Proven 0.0f: 0x3001e380 does
 * `fld DWORD PTR ds:0x3007bcec` / `fucompp` against +0x220/+0x224/+0x228, and
 * .rdata 0x3007bcec = 00 00 00 00 = 0.0f (the adjacent 0x3007bce8 = 0.5f). A prior
 * pass misread this constant as 0.5f; corrected here. */
#define CG_WEAPON_ANGLE_SMOOTH_UNSET (0.0f)

/*
 * cgLerpAngleBlock_t — the interpolated "view angles" sub-block that
 * CG_CalcEntityLerpPositions (0x30021d30) writes at +0x3e0 of a 0x4d0-stride
 * per-entity state record. Both stride-0x4d0 tables that function targets carry
 * this identical block at +0x3e0: bgs.clientinfo[] (player entities; the field
 * names here match clientInfo_t.leanAmount/leanFraction/viewPitch/viewYaw/
 * +0x3f0 viewRoll) and cg_corpseInfo[] (player-corpse entities). The
 * .mcode computes ONE base pointer (EAX) then stores these five dwords at fixed
 * offsets regardless of which table, so the two paths share this overlay. Field
 * roles proven only as raw dword copies out of the centity by this function; exact
 * CoD source names adopted from the corroborating clientInfo_t layout. */
typedef struct cgLerpAngleBlock_s {
    float leanAmount; /* +0x00 (record +0x3e0): from currentState +0x6c */
    float leanFraction; /* +0x04 (record +0x3e4): from currentState +0xd8 */
    float viewPitch; /* +0x08 (record +0x3e8): lerpAngles[0] (+0x214) */
    float viewYaw; /* +0x0c (record +0x3ec): currentState +0x218 */
    float viewRoll; /* +0x10 (record +0x3f0): currentState +0x21c */
} cgLerpAngleBlock_t;
_Static_assert(offsetof(cgLerpAngleBlock_t, viewRoll) == 0x10, "cgLerpAngleBlock_t viewRoll +0x10");

/* Base of cg_entities[] indexed by CG_CalcEntityLerpPositions for the eType-9
 * (ET_SOUND_BLEND) proxy copy: element stride 0x288 (centity_t), base
 * 0x3048c6e0 (the established cg_entities view). See globals.h. */

/*
 * CG_CalcEntityLerpPositions (0x30021d30) — RECONSTRUCTED (see
 * src/client/cgame/animation/cg_calcentitylerppositions.c). Compute one client entity's
 * interpolated render/weapon angles into its centity +0x208 lerpOrigin /
 * +0x214 lerpAngles block, and mirror the resulting view angles into the
 * entity's per-client/per-corpse 0x4d0-stride state record.
 *
 * Behavior proven from the machine code:
 *   - eType == ET_SOUND_BLEND(9) with vehicleEntityNum(+0x74) != 0x3ff: copy the
 *     six-dword +0x208..+0x21c angle block straight from cg_entities[vehicleEntityNum]
 *     (proxy/attach entity) and return.
 *   - otherwise, on currentState.pos.trType(+0xc): TR_INTERPOLATE(1), or
 *     TR_LINEAR_STOP(3) with currentState.number(+0x0) < 0x40 (below MAX_CLIENTS),
 *     tail-call the sibling smoothing routine 0x30021bb0 and return.
 *   - otherwise: BG_EvaluateTrajectory(&currentState.pos, cg_time, lerpOrigin)
 *     and BG_EvaluateTrajectory(&currentState.apos, cg_time, lerpAngles);
 *     then for eType ET_PLAYER(1) write bgs.clientinfo[otherEntityNum(+0x94)],
 *     for ET_PLAYER_CORPSE(2) write cg_corpseInfo[number(+0x0)-0x40], the
 *     shared cgLerpAngleBlock_t at +0x3e0 (also zeroing lerpAngles[0] and
 *     currentState +0x21c). Finally, unless the entity is cg_predictedEventEntity,
 *     mover-lag-adjust lerpOrigin via CG_AdjustPositionForMover using the entity's
 *     mover num (+0x7c), from cg_snap->serverTime to cg_time.
 *
 * One caller-cleaned int32 stack arg (the centity pointer); plain RET. The .mcode
 * size-guess "BG_CalculateWeaponAngles" is REJECTED (this is the centity angle-lerp
 * updater, not the shared BG weapon-position math). The Mac call fingerprint
 * resolves CG_CalcEntityLerpPositions as the canonical symbol. This supersedes
 * the earlier duplicate name "CG_UpdateEntityDObjRenderState".
 */
void CG_CalcEntityLerpPositions(centity_t *entity);

/*
 * CG_InterpolateEntityPosition (0x30021bb0) — RECONSTRUCTED. The sibling
 * angle-lerp path CG_CalcEntityLerpPositions (0x30021d30) tail-calls for entities
 * whose position trajectory is TR_INTERPOLATE, or TR_LINEAR_STOP below MAX_CLIENTS.
 * It evaluates the entity's currentState.pos/apos trajectories at cg_snap->serverTime
 * AND its nextState.pos/apos trajectories at cg_nextSnap->serverTime, then blends the
 * two evaluations by cg_frameInterpolation: a plain lerp for the position
 * (lerpOrigin, +0x208) and a short-way LerpAngle (0x3004bd00) per component for the
 * angles (lerpAngles, +0x214). When nextState.eType == ET_PLAYER it also
 * publishes the LerpAngle-blended lean scalars into bgs.clientinfo[
 * nextState.otherEntityNum]'s cgLerpAngleBlock_t (+0x3e0) and zeroes lerpAngles
 * [0]/[2]. ABI proven: ESI = centity (register arg), no stack args, plain RET.
 * The Mac body has the identical BG_EvaluateTrajectory/LerpAngle callset.
 * Source: uo_cgame_mp_x86.dll 0x30021bb0..0x30021d2a. */
void CG_InterpolateEntityPosition(centity_t *entity /* ESI */);

/*
 * CG_DrawTankPositionStatus (0x30031f70) — the mask-gated rotated-tag member
 * of the 0x30031cb0..0x30032042 HUD-tag draw family. Draws one HUD "tag" (a 2D shader
 * quad) for cg_entities[cg_predictedPlayerState.viewLockedEntityNum], sliding in horizontally and rotated by
 * AngleSubtract(cg_refdefViewAngles[1], entity->lerpAngles[1]) exactly like the
 * rotated-tag sibling (0x30031d50), but adds a per-entity bitmask gate: the entity's
 * hudTagMask (+0xe4) must have the (1 << bitIndex) bit set, where bitIndex is a
 * caller-supplied argument. ABI: rect in EDI (float[4]), stateFilter in ECX (register
 * args); three caller-cleaned cdecl stack args hShader, color, bitIndex. The .mcode
 * size-guess "Script_ExecOnCvarFloatValue" is REJECTED (no cvar/script work; this is a
 * gated 2D rotated-pic draw). The Mac body has the same entity-lerp, angle,
 * color, and rotated-picture sequence, resolving the source name.
 */
void CG_DrawTankPositionStatus(const rectDef_t *rect, int32_t stateFilter, int32_t hShader, const float *color, int32_t bitIndex);

/*
 * CG_DrawTurretTagQuad (0x3001ccf0, role name) — RECONSTRUCTED; see
 * src/client/cgame/hud/cg_drawturrettagquad.c. Renders a rotated, screen-scaled
 * 2D quad via cgame_syscall(CG_R_DRAW_ROTATED_QUAD, &verts[8], shaderParams, hShader).
 * Register-ABI: the four {x,y} corner offsets are passed in EDX as `const float *`
 * (callers LEA a stack float[8]); the remaining five args are caller-cleaned cdecl
 * stack slots: (float x, float y, const float *shaderParams, float angleDegrees,
 * int32_t hShader). Internally it converts angleDegrees->radians (M_PI/180 split
 * across two FMULs), FSINCOSes it, screen-scales the rotation basis and the center
 * by cgs.screenXScale/screenYScale, transforms the four corner offsets through the
 * caller's 2x4 matrix, and submits the resulting eight-float quad. Bare RET (the
 * function cleans its own 0x44 frame + four syscall pushes; the caller cleans the
 * five stack args). Both known callers (0x3001aae0 in FUN_3001a980_3001aafe and
 * 0x30031f55 in the turret-tag drawer 0x30031e20) match this ABI; the name is a
 * role name from the turret-tag caller, not a proven CoD symbol. */
void CG_DrawTurretTagQuad(float *cornerOffsets, float x, float y, const float *shaderParams, float angleDegrees, int32_t hShader);

/*
 * CG_DrawTankBarrel (0x30031e20) — the world-tag member of the
 * 0x30031cb0..0x30032042 HUD-tag draw family. Unlike the fixed-rect siblings
 * (0x30031cb0/0x30031d50/0x30031f70), which draw at a rect corner, this one resolves
 * the currently-processed entity's "tag_turret" DObj bone world matrix
 * (CG_DObjGetWorldTagMatrix), converts that matrix to Euler angles
 * (Axis4ToAngles), and draws a rotated icon quad there
 * (CG_DrawTurretTagQuad -> trap 0x4c) with the static cg_turretTagShaderParams
 * descriptor. Same entry gates as the siblings: entityStateFlags bit
 * EF_IN_VEHICLE set and EF_VEHICLE_ALLOW_WEAPON clear, entity->eType == 12
 * (ET_VEHICLE), and a stateFilter match; additionally the entity must have a live
 * DObj handle (trap 0xa5 returns nonzero) and the tag lookup must succeed.
 * ABI: rect in EDI (float[4]), stateFilter in ECX (register args); two caller-cleaned
 * cdecl stack args hShader (arg0) and color (arg1). The .mcode size-guess
 * "Item_ValidateTypeData" is REJECTED (no item/type validation; this is a vehicle
 * turret HUD-tag draw). The Mac DObj/world-tag/rotated-quad sequence resolves the
 * source name. */
void CG_DrawTankBarrel(const rectDef_t *rect, int32_t stateFilter, int32_t hShader, const float *color);

qboolean CG_DObjGetBoneBoundsWireframe(DObj *self, const char *tagName, vec3_t out[24]);
qboolean CG_DObjGetWorldBoneBoundsWireframe(DObj *dobj, centity_t *cent, const char *tagName, vec3_t points[24]);

/*
 * CG_SetupWeaponLightingOrigin (0x3001e380) — fills refEntity.lightingOrigin from the
 * entity's smoothed view-weapon angles and OR-s in RF_LIGHTING_ORIGIN. Caller-observed
 * ABI (custom regparm): the centity `this` is in ECX, the out refEntity in EDX; no stack
 * args (plain RET). Reconstructed at src/client/cgame/render/cg_setupweaponlightingorigin.c;
 * declared here so CG_General (0x3001e430) can reuse it. */
void CG_SetupWeaponLightingOrigin(centity_t *ent, refEntity_t *re);

/*
 * CG_RefreshEntityDObjAnimTree (0x30021ea0) — (re)bind the per-entity DObj anim tree
 * for the client entity identified by ESI = cent->entityNum. Caller-observed ABI: the
 * entity number is passed in ESI (register), plus two stack args. It is caller-cleaned
 * (plain RET): CG_General defers the cleanup, folding these two args' 8 bytes into the
 * `ADD ESP,0x10` after the following trap 0xa5 call (0x3001e47e), rather than issuing an
 * `ADD ESP,8` right after the CALL at 0x3001e469. From CG_General the first stack arg is
 * cent->eType and the second is cg_gameModels[cent->modelIndex]. Internally it queries the DObj handle
 * (CG_DOBJ_GET_HANDLE), wraps the model (CG_DOBJ_WRAP_MODEL), compares/releases the
 * previously registered DObj-info in the per-entity tables cg_dObjInfoKeys /
 * cg_dObjInfoHandles (0x30487af8/0x30488af4, indexed by entityNum), builds the DObj
 * model set (CG_CLIENT_DOBJ_CREATE), and for eType ET_TURRET (11) constructs the
 * MG42 weapon anim tree via CG_CreateMG42WeaponAnimTree (0x3001e960) and instantiates
 * it (CG_XANIM_CREATE_TREE, trap 134 == 0x86). Reconstructed at
 * src/client/cgame/animation/cg_refreshentitydobjanimtree.c; the register+stack ABI is documented
 * in that file (entityNum in ESI, eType/animTreeParam as two caller-cleaned stack
 * args), not encoded as a calling-convention attribute, for the syntax-only build.
 * The .mcode size-guess "PlaneFromPoints" is rejected: it manages per-entity DObj
 * anim-tree registration with integer engine syscalls and contains NO floating point
 * (no cross product / normalize / dot), not plane geometry. */
/* entityNum arrives in ESI (register); eType/animTreeParam are the two caller-cleaned
 * stack args. Returns the entity's DObj handle (0 on the no-handle early-outs). */
intptr_t CG_RefreshEntityDObjAnimTree(int32_t entityNum, int32_t eType, int32_t animTreeParam);

/*
 * CG_General (0x3001e430) — the eType-0 handler of the CG_AddCEntity dispatch
 * (jump table at 0x30022228, reached via 0x30022170; the caller at 0x3001f6f0 lerps
 * the entity's weapon angles then dispatches on cent->eType < 16). Builds and submits
 * one animated DObj model render entity for `cent`. Caller-observed ABI: the centity
 * pointer is passed in EBX (register); no stack args, /GS-protected frame (snapshots
 * __security_cookie at frame+cookie slot, verifies via __security_check_cookie on
 * exit). Reconstructed at src/client/cgame/entities/general_entity.c. */
void CG_General(centity_t *cent /* EBX */);

/*
 * CG_Mover (0x3001f120) — the ET_MOVER (eType == 5) handler of the CG_AddCEntity
 * dispatch (jump table at 0x30022228, arm [5] via the thunk at 0x300221c9; cent
 * passed as one caller-cleaned 32-bit stack arg). Builds and submits one oriented
 * mover refEntity_t: axis = AnglesToAxis(cent->lerpAngles), origin ==
 * oldorigin == cent->lerpOrigin, renderfx = RF_NOSHADOW. When
 * cent->solid == SOLID_BMODEL it draws a static
 * inline/brush model (reType 0, hModel = cg_inlineModelHandles[modelindex]);
 * otherwise a DObj-animated model (RT_MODEL, dobjHandle from
 * cgame_syscall(CG_DOBJ_GET_HANDLE, entityNum), owner = cent). Early-outs on the
 * eFlags 0x80 no-draw bit, and on a missing DObj handle. /GS-protected frame.
 * Reconstructed at src/client/cgame/entities/cg_mover.c. The .mcode
 * size-guess "MatrixMultiply43" is rejected (this is an eType render handler, no
 * matrix multiply). */
void CG_Mover(centity_t *cent);

/*
 * CG_Item (0x3001e680) — the CG_AddCEntity item-entity handler, sibling of
 * CG_General (dispatched from the same eType jump table via 0x30022170; this is the
 * arm at 0x300221b7). Register-ABI: centity in EBX; /GS-protected frame. Draws a
 * world/dropped item: bounds-checks cent->itemIndex (< 134, else "Bad item index %i
 * on entity"), skips EF_NODRAW entities, lazily registers the item's visuals via
 * CG_RegisterItemVisuals over cg_items (itemInfo_t), then builds and submits one
 * RT_MODEL refEntity_t with trap_R_AddRefEntityToScene. The .mcode size-guess name
 * "SP_func_bobbing" is REJECTED (client render handler, not a server spawn func).
 * Reconstructed at src/client/cgame/entities/item_entity.c. */
void CG_Item(centity_t *cent /* EBX */);

/* Source: uo_cgame_mp_x86.dll 0x3048c6e0 (.data). One client centity array with
 * MAX_GENTITIES elements and the machine-proven 0x288-byte centity_t stride. */
extern centity_t cg_entities[MAX_GENTITIES];

/* NOT_FROM_ORIGINAL_SOURCE: native-host spelling of the retail client's
 * unchecked cg_entities row calculation. The original PE32 IMUL produces a
 * modulo-2^32 byte displacement. Keeping the address calculation at an explicit
 * integer boundary avoids false in-range C array provenance while preserving the
 * original absent-bound behavior; it does not make an invalid access safe. */
#if defined(_MSC_VER)
#define CGENTITY_COMPAT_ALWAYS_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define CGENTITY_COMPAT_ALWAYS_INLINE inline __attribute__((always_inline))
#else
#define CGENTITY_COMPAT_ALWAYS_INLINE inline
#endif
static CGENTITY_COMPAT_ALWAYS_INLINE centity_t *cgame_compat_unchecked_cgentity(int32_t index)
{
    const uint32_t offsetBits = (uint32_t)index * (uint32_t)sizeof(cg_entities[0]);
    const intptr_t displacement = (intptr_t)coduo_int32_from_bits(offsetBits);
    const uintptr_t address = (uintptr_t)(void *)&cg_entities[0] + (uintptr_t)displacement;
    return (centity_t *)address;
}
#undef CGENTITY_COMPAT_ALWAYS_INLINE

/* cg_specialTagPlacement (orientation_t @ 0x3048b0e4) is declared in globals.h. */

/*
 * CG_DrawRotatedPic (0x3001cb60) — draw a 2D shader quad (x, y, w, h) rotated by
 * `angle` (degrees; 0 = axis-aligned; converted internally to radians via *M_PI*(1/180)
 * before FSINCOS) via cgame trap 76 (0x4c). Caller-observed from
 * the HUD-tag family: 0x30031cb0 issues it with angle 0 and an animation-slid x; the
 * text/name siblings pass a nonzero angle. It reads all six args as consecutive stack
 * dwords (SUB ESP,0x44; args at +0x48..+0x5c) — the first four as floats, then the
 * angle float, then the shader/handle dword — builds a rotated quad with FSINCOS and
 * calls cgame_syscall(76, ...). Caller-cleaned cdecl; six 32-bit stack args. Engine
 * service is a rotated 2D pic draw (trap 76); the exact CoD symbol is unproven. The
 * .mcode size-guess "BG_AnimationIndexForString" is REJECTED (no string, no index; pure
 * float rotation + a 2D-draw syscall). Superseded by its own .mcode reconstruction.
 */
void CG_DrawRotatedPic(float x, float y, float w, float h, float angle, int32_t hShader);

/*
 * CG_AddCEntity per-eType render/update handlers. All are caller-observed only
 * from the CG_AddCEntity (0x30022170) jump-table dispatch and take the
 * centity_t in a register (the dispatcher never uses a stack
 * ABI except for the three PUSH-arg cases noted below). Names are provisional,
 * assigned by the ROLE proven from the dispatch — the handler invoked for a
 * given currentState.eType — and are superseded by each callee's own .mcode
 * reconstruction. Every .mcode size-guess name for these (PM_NoclipMove,
 * CG_AdjustPositionForMover, CG_LoadShellShockCvars, SP_func_bobbing,
 * G_GetNonPVSTankInfo, MatrixMultiply43, G_SetFixedLink, CG_DrawDisconnect,
 * PM_Weapon_CheckFiringAmmo, Scr_Vehicle_Pain, CheckMatchTimeout, String_Init)
 * is REJECTED: each is a pure size match ("matched size") contradicted by the
 * dispatch role. Register-arg convention noted per handler where the dispatcher
 * proves it. */
void CG_AddCEntity_General(centity_t *cent); /* eType 0; EAX=cent (0x3001e430) */
void CG_AddCEntity_Player(centity_t *cent); /* eType 1; ECX=cent (0x300343e0) */
void CG_AddCEntity_PlayerCorpse(centity_t *cent); /* eType 2; stack arg (0x300346c0) */
void CG_AddCEntity_Item(centity_t *cent); /* eType 3; EAX=cent (0x3001e680) */
void CG_AddCEntity_Missile(centity_t *cent); /* eType 4; EAX=cent (0x3001edb0) */
void CG_Portal(centity_t *cent); /* eType 6; ESI=cent (0x3001f470) */
void CG_ScriptMover(centity_t *cent); /* eType 8; stack arg (0x3001f260) */
void CG_AddCEntity_SoundBlend(centity_t *cent); /* eType 9; EAX=cent (0x30021860) */
void CG_AddCEntity_LoopedFx(centity_t *cent); /* eType 10; EAX=cent (0x30021a30) */
void CG_AddCEntity_Turret(centity_t *cent); /* eType 11; EAX=cent (0x3001eca0) */
void CG_AddCEntity_Vehicle(centity_t *cent); /* eType 12,13; EAX=cent (0x30021660) */
void CG_VehicleOwnerIcon(centity_t *cent); /* eType 15; ESI=cent (0x30021540) */

/*
 * CG_AddHudHeadIconSprite (0x300213c0, provisional role name) — build one rotating
 * HUD head-icon refEntity and submit it to the render scene. Called by
 * CG_VehicleOwnerIcon (0x30021540) once per pulse phase with:
 *   cent          — the owning client entity (its origin has been copied into the
 *                   cent->lerpOrigin[+0x208] block, which this helper reads as the
 *                   sprite world origin: FLD [cent+0x208], FADD [cent+0x210], ...);
 *   material      — the "gfx/hud/headicon" qhandle_t (in EBX);
 *   yaw           — the sprite's integer rotation (degrees) for this pulse;
 *   drawFlag      — a 0/nonzero selector (0 here) gating an extra offset term;
 *   secondaryAngle— a second integer angle (180 here);
 *   alphaScale    — the trapezoidal pulse alpha (pow(fade,1.5)) for this pulse.
 * cdecl / caller-cleaned (RET; caller does ADD ESP,0x18 for the six 4-byte args).
 * Internally it computes a 2D screen offset, zeroes a stack refEntity_t, packs a
 * white RGBA with alphaScale-derived alpha via Q_rint, and issues trap id 0x3d
 * (trap_R_AddRefEntityToScene) through *0x30085e9c. Caller-observed ABI only;
 * superseded by its own .mcode reconstruction (FUN_300213c0_30021531). The .mcode
 * size-guess name is rejected — this draws a HUD sprite, it is no weapon/PM step. */
void CG_AddHudHeadIconSprite(centity_t *cent, qhandle_t material, int32_t yaw, int32_t drawFlag, int32_t secondaryAngle, float alphaScale);

/*
 * CG_CalcEntityLerpOrigin (0x3001e7f0, provisional role name) — the per-entity
 * setup CG_AddCEntity runs on every centity before the eType dispatch (called
 * with EAX=cent). Its own .mcode reads currentState-adjacent fields (+0x84,
 * +0xa0, +0x8c) and the entity origin block (+0x208), does float trajectory math
 * against a per-index table (0x304495e8), and raises two Com_ErrorMessage bounds
 * diagnostics. Caller-observed only; the exact CoD symbol is unresolved and the
 * .mcode size-guess "BG_TakePlayerWeapon" is REJECTED (no weapon-slot / player
 * state work). Superseded by its own .mcode reconstruction. */
void CG_CalcEntityLerpOrigin(centity_t *cent);

/*
 * CG_AddCEntity (0x30022170) — add one client entity (centity_t)
 * to the current render/sound frame, dispatching on currentState.eType through the
 * 16-entry jump table. Reconstructed in functions/FUN_30022170_30022228.c; the
 * dispatcher takes `cent` in EAX (register ABI). Declared here so the per-frame
 * driver CG_AddPacketEntities (0x3001f810) can call it on each snapshot entity and
 * on cg_predictedEventEntity. See the .c for the full proof. */
void CG_AddCEntity(centity_t *cent);

/*
 * CG_TransitionEntity (0x3003c7c0) — RECONSTRUCTED (see
 * src/client/cgame/state/cg_transitionentity.c). Commit one cg_entities[] centity_t
 * from its incoming snapshot state to its current render state: currentState =
 * nextState (0xf4-byte copy), reset smoothedWeaponAngles / the current-valid flag /
 * laserEffectStarted, evaluate nextState.pos and nextState.apos at cg.time into
 * lerpOrigin/lerpAngles, then dispatch on nextState.eType — ET_PLAYER reseeds
 * the client's clientInfo_t view angles and calls CG_ResetPlayerEntity;
 * ET_PLAYER_CORPSE copies the client's anim-state row into cg_corpseInfo[]
 * (with an optional DObj clone via CG_XANIM_CLONE_ANIM_TREE); other types just seed the event
 * latches. One caller-cleaned int32 stack arg (the centity), void return. Called by the
 * snapshot-transition driver at 0x3003d020. The .mcode size-guess "CG_PlayerShadow" is
 * REJECTED (real CG_PlayerShadow is 0x30032c20; this does no shadow projection).
 */
void CG_TransitionEntity(centity_t *cent);

/* NOTE: 0x30021d30 was formerly declared twice — once as CG_CalcEntityLerpPositions
 * (from the HUD-tag draw callers) and once here as CG_UpdateEntityDObjRenderState (from
 * the CG_AddPacketEntities callers). They are the SAME function. Consolidated on the
 * reconstruction (src/client/cgame/animation/cg_calcentitylerppositions.c) to CG_CalcEntityLerpPositions above; the
 * CG_AddPacketEntities call sites were updated to that name. The "DObjRenderState" role
 * is unproven — the body does no DObj access — so it is dropped. */

/*
 * CG_AddPacketEntities (0x3001f810) — reconstructed in
 * functions/FUN_3001f810_3001fb71.c. The cgame per-frame entity add pass: advance
 * every snapshot entity's DObj animation, rebuild the local player's predicted-event
 * centity, then add every snapshot entity (and the predicted-event entity) to the
 * render scene via CG_AddCEntity. No arguments; void. */
void CG_AddPacketEntities(void);

/*
 * CG_UpdateHudSpinAngle (0x3001d3a0, provisional role name) — advance the
 * time-animated spin angle of the rotating HUD element (cg_hudSpinAngle,
 * 0x3048b5cc) toward a target angle, via a critically-damped angular spring
 * integrated in <=5 ms substeps.
 *
 * PROVEN FROM ITS OWN .mcode (supersedes the earlier caller-observed guess): the
 * body takes NO arguments — although its sole caller CG_DrawSpinningPic
 * (0x3002f910) pre-stores three/four floats in the outgoing stack frame, this
 * function never reads any [ESP + arg] slot (verified exhaustively; all stack
 * accesses are its own locals). It reads globals only: cg_refdefViewAngles[1]
 * (0x30487acc) and cg_hudSpinBaseTime (0x3048b5c8) to form the target angle,
 * cg_time (0x304831b0), cg_hudSpinPrevTime (0x30134ce0) and cg_hudSpinVel
 * (0x3048b5d0) as running state, and writes cg_hudSpinAngle. The return value in
 * EAX is undefined garbage and is ignored by the caller; declared void. Bare RET
 * (no callee stack cleanup). The .mcode size-guess "PM_BeginWeaponChange" is
 * REJECTED: this touches no pmove/weapon-change state and produces a HUD rotation
 * angle from cg_time. The earlier 4-float caller-observed signature is corrected
 * here to () to match the machine code.
 */
void CG_UpdateHudSpinAngle(void);

/*
 * CG_DrawSpinningPic (0x3002f910, provisional role name) — HUD draw-command
 * handler that draws one HUD-scaled 2D pic (`rect` = {x,y,w,h}, shader `hShader`)
 * rotated by a time-animated spin angle, modulated by `color`. Called by the
 * cgame HUD-command dispatcher (0x300320e0, sole call site 0x3003249f). Scales
 * the rect into virtual-screen space with cg_hudCompassSize_vmCvar.value (0x3048c4a8) and
 * fixed anchor constants, advances/reads the spin angle via CG_UpdateHudSpinAngle
 * / cg_hudSpinAngle, and draws it via trap_R_SetColor(color) / CG_DrawRotatedPic
 * / trap_R_SetColor(NULL). ABI (proven from the call site): EAX = pointer to the
 * rect float[4]; two caller-cleaned cdecl stack args hShader then color. The
 * .mcode size-guess "PlayerCmd_isOnGround" is REJECTED: this issues 2D-draw cgame
 * traps and reads no player command/ground state. Provisional role name (no cgame
 * symbol table recovered).
 */
void CG_DrawSpinningPic(const rectDef_t *rect, qhandle_t hShader, const float *color);

/*
 * CG_CreateMG42WeaponAnimTree (0x3001e960) — build the "MG42" weapon XAnim tree
 * for the weapon owned by the given effect slot and return its engine tree
 * handle (0 if creation failed). Looks up bg_weaponInfos[slot->weaponIndex],
 * then issues the XAnim tree-construction traps: trap(132,"MG42",3) creates the
 * master tree, trap(135, tree, 0,"root",1,2,0) attaches its root node, and two
 * (trap(131,boneId); trap(133, tree, slot, boneId)) pairs register the weapon's
 * two anim bone ids (weaponInfo_t +0x1c, +0x24). The MG42 name is compiled in and
 * the sole caller (0x30021f27) guards on the weapon type == 11; the caller feeds
 * the returned handle to CG_XANIM_CREATE_TREE (134) to instantiate it. Role name
 * (MG42 + XAnim tree construction); the mechanical .mcode header name
 * "ObjectiveStateIndexFromString" was a size-only guess and is rejected — this
 * function parses no string and returns a tree handle, not a state index. */
intptr_t CG_CreateMG42WeaponAnimTree(centity_t *slot);

/*
 * CG_SpawnFlameChunkOnBone (0x30023d50, provisional name) — allocate a flame
 * chunk and attach it to a bone of a client's model, then set up its trajectory
 * and lifetime. Called once per limb bone by CG_StartFlameDamageEffect. The
 * entity record (whose state begins at +0x1e8) is passed in
 * EAX; the five stack args follow (caller-cleaned cdecl, RET with no immediate).
 * Proven behavior in the callee: it calls the slot allocator (0x30025600) and on
 * failure prints "Out of flame chunks\n" (0x300777b8), resolves the bone name to a
 * bone/tag index via cgame_syscall(0xa5)/(0xb2), reads a time global (0x300ab718)
 * via FILD to stamp the chunk, and stores a TR_LINEAR_STOP trajectory (type 3).
 * `pos` is an optional explicit spawn position (NULL => use the bone tag).
 * Caller-observed provisional ABI; superseded by its own .mcode.
 *
 * The .mcode size-matched guess "SpectatorClientEndFrame" is REJECTED: this is the
 * flame-chunk spawner (alloc + "Out of flame chunks" + bone-tag trajectory), not a
 * spectator end-of-frame routine.
 */
void CG_SpawnFlameChunkOnBone(centity_t *slot, const vec3_t pos, const char *boneName, int32_t durationMsec, float startSpeed,
                              int32_t count);

/*
 * flameChunk_t (0x150 / 336 bytes) — one node of the client flame-chunk pool
 * cg_flameChunks (FLAME_CHUNK_COUNT of these; see globals.h). Each node lives on
 * two intrusive doubly-linked lists simultaneously:
 *   - the free/active list via next (+0x00) / prev (+0x04): CG_ClearFlameChunks
 *     (0x30025570) threads every node onto cg_freeFlameChunks; CG_SpawnFlameChunk
 *     (0x30025600) pops from cg_freeFlameChunks and pushes onto cg_activeFlameChunks;
 *     CG_FreeFlameChunk (0x300256e0) unlinks and returns the node to the free list.
 *   - the secondary list via listNext (+0x0c) / listPrev (+0x10), headed by
 *     cg_flameChunkList (0x300d9750); CG_FreeFlameChunk recursively frees the
 *     child chain rooted at parent (+0x08) first.
 * Field roles below are the ones the pool init/spawn/free reconstructions prove;
 * the remaining bytes hold flame render/physics state not yet reconstructed. The
 * numeric-offset padding fields are placeholders, not final source names. */
enum {
    /* node.listMarker (+0x24) nonzero value SETLE-derived in CG_ClearFlameChunks;
     * exact source name unresolved. */
    FLAME_CHUNK_LIST_ACTIVE = 1
};
typedef struct flameChunk_s {
    struct flameChunk_s *next; /* +0x00: free/active list forward link */
    struct flameChunk_s *prev; /* +0x04: free/active list back link */
    struct flameChunk_s *parent; /* +0x08: child chain root (freed first) */
    struct flameChunk_s *listNext; /* +0x0c: secondary list forward link */
    struct flameChunk_s *listPrev; /* +0x10: secondary list back link */
    uint32_t ownerSentinel; /* +0x14: owner/instance sentinel. CG_AddFlameSpriteToScene
                                     * (0x300268e0) treats -1 (0xffffffff) as "unset/no owner":
                                     * for a kind==2 chunk it skips the rand-gated damage-trace
                                     * block unless ownerSentinel == -1 (CMP [f+0x14],-1). Exact name
                                     * unresolved. */
    uint32_t emitScatterIndex; /* +0x18: rand()-scattered emit index (Q_rint of a
                                     * randomized value); seeded by CG_EmitPlayerFlameChunks
                                     * and CG_MoveFlameChunk. Exact name unresolved. */
    uint32_t unresolvedField_1c; /* +0x1c: written/reset but never read by any recovered
                                     * client consumer. This is not an abiGap because the store
                                     * is real; the field's semantic role remains unknowable. */
    uint32_t liveFlag; /* +0x20: set to 1 by CG_SpawnFlameChunk (spawned/live flag; exact name unresolved) */
    uint32_t listMarker; /* +0x24: secondary-list membership marker */
    uint32_t emitArgFlag; /* +0x28: emit `arg4` flag stamped at emit by
                                     * CG_EmitPlayerFlameChunks (0x3002496c / 0x30025295);
                                     * exact source name unresolved. */
    int32_t kind; /* +0x2c: per-chunk mode/kind. CG_UpdateFlamethrowerSounds
                                     * (0x30029210) skips a chunk's sound update when
                                     * kind == 2, and takes the sound-envelope
                                     * decay branch when kind == 3; exact source
                                     * enum name unresolved. CG_AddFlameSpriteToScene
                                     * (0x300268e0) dispatches its whole sprite build on
                                     * this: kind==3 returns immediately (no sprite),
                                     * kind==1 returns after the flame-time update,
                                     * kind==2 gates the damage-trace path (and needs
                                     * ownerSentinel==-1), kind==5 selects the smoke-color
                                     * path, all others use the solid-white fire color.
                                     * So the tested values are {1,2,3,5}; provisional
                                     * flame-chunk-kind enum. */
    uint32_t overrideMaterial; /* +0x30: zeroed at spawn. CG_AddFlameSpriteToScene
                                     * (0x300268e0), on the argFlag==1 (world) sprite path,
                                     * reads it as an OPTIONAL render-material override
                                     * (MOV EBP,[f+0x30]; if nonzero it is passed straight
                                     * to trap_R_AddPolyToScene as the poly shader, else the
                                     * fire-material table cg_flameFireMaterials[i] is used).
                                     * Provisional role name unresolved (an int32 qhandle_t). */
    int32_t ownerInfoIndex; /* +0x34: flame-info/sound index (the "owner" or
                                     * per-limb index into cg_flameInfo[] (stride 0xb8)
                                     * and the stride-12 flame-sound-loop table). Used
                                     * as an array index throughout CG_UpdateFlamethrowerSounds
                                     * (0x30029210). Exact source name unresolved. */
    /* +0x38..+0x117 flame render/physics state. The named sub-fields below are the
     * ones CG_MergeFlameChunks (0x300257e0) copies from the trailing chunk (f2) into
     * the leading chunk (f1) when it fuses two consecutive flame chunks; the exact
     * source names are unresolved, so they carry their offsets. Gaps stay reserved. */
    int32_t ownerClientNum; /* +0x38: a second copy of ownerInfoIndex stamped at spawn by
                                     * CG_SpawnFlameChunkOnBone (0x30023d50: MOV [EBX+0x38],EAX
                                     * where EAX=ownerInfoIndex). CG_AddFlameSpriteToScene
                                     * (0x300268e0) uses it as the OWNER CLIENT NUMBER: it is
                                     * passed as arg0 to CG_FlameDamage (0x300265c0)
                                     * and compared against cg_snap->ps.psClientNum (+0xe0) to decide
                                     * whether this flame belongs to the local player (the
                                     * first-person view-relative billboard-basis path). So
                                     * +0x34/+0x38 are the flame's owner-entity/limb index.
                                     * Exact source name unresolved. */
    int32_t centFlags; /* +0x3c: cent[+0xcc] copied at emit by
                                     * CG_EmitPlayerFlameChunks (0x30025108 / 0x30025416);
                                     * exact source name unresolved. */
    int32_t boneHandle; /* +0x40: DObj bone/tag handle+1 that this chunk is attached
                                     * to (0 = free chunk, not bone-attached). CG_SpawnFlameChunkOnBone
                                     * stores CG_DObjGetBoneIndex(...)+1 here; CG_ComputeFlameChunkOrigin
                                     * (0x30025990) uses boneHandle-1 to index the engine bone-matrix
                                     * table. Provisional name; exact source name unresolved. */
    uint8_t padding044[4]; /* ABI_AUDITED_PADDING: aligns the following double at +0x48. */
    double spawnTime; /* +0x48: chunk spawn timestamp (double, flame-clock units);
                                     * elapsed-since-spawn = (cg_flameTime - spawnTime)*0.001 in
                                     * CG_ComputeFlameChunkOrigin. Merged from f2. */
    double endTime; /* +0x50: chunk end/expire timestamp (double). Merged from f2. */
    float startSpeed; /* +0x58: the spawn `startSpeed` argument (a float), copied at
                                     * spawn by CG_SpawnFlameChunkOnBone (0x30023dbf) into +0x58,
                                     * +0x5c and +0xe4 from the same source dword. Retyped from the
                                     * mechanical reserved gap; exact source name unresolved. */
    uint32_t startSpeedBits; /* +0x5c: merged from f2. Also written at spawn by
                                     * CG_SpawnFlameChunkOnBone (0x30023dc8) with the raw 32-bit
                                     * bits of the `startSpeed` float (same dword as +0x58/+0xe4);
                                     * kept uint32_t because CG_UpdateFlamethrowerSounds (0x30029210)
                                     * reads it as an int gate (compared == 1). Dual role: holds
                                     * float bits but is consumed as an int gate. */
    float smokeDensityRate; /* +0x60: smoke-density / opacity rate (single). Merged from
                                     * f2. CG_AddFlameSpriteToScene (0x300268e0), on the smoke
                                     * path (kind==5), reads it (FLD [f+0x60]) and multiplies
                                     * by 0.007147 (~1/140) into the smoke sprite's alpha/size
                                     * curve. Retyped from the merge-placeholder uint32_t to float;
                                     * provisional role name unresolved. */
    float sizeRate; /* +0x64: per-chunk size/expansion rate cache
                                     * (single). CG_FlameDropDrip (0x30027ad0)
                                     * and CG_FireFlameChunks (0x30027d10) store
                                     * CG_FlameGetSizeRate(chunk) (optionally scaled)
                                     * here; the emitter (0x30024050) writes
                                     * startSpeedBits * alpha * 0.00060024 into it.
                                     * Retyped from the mechanical uint32_t placeholder
                                     * to float; exact source name unresolved. */
    double spawnTimeCopy; /* +0x68: chunk timestamp (double); stamped with
                                     * (double)cg_flameTime at spawn and chunkEndTime at emit.
                                     * Merged from f2. Exact source name unresolved. */
    vec3_t localPos; /* +0x70/74/78: chunk local position (bone-relative when
                                     * attached; world otherwise). Read as singles by the
                                     * per-frame position update CG_ComputeFlameChunkOrigin
                                     * (0x30025990: FLD/FMUL [EBX+0x70..78]); merged from f2. */
    uint32_t padding07C; /* ABI_AUDITED_PADDING: aligns driftStartTime at +0x80. */
    double driftStartTime; /* +0x80: chunk drift-start timestamp (double, flame-clock units).
                                     * The free-chunk extrapolation in CG_ComputeFlameChunkOrigin
                                     * (0x30025990) uses elapsed = (cg_flameTime - driftStartTime)*0.001.
                                     * Merged from f2. */
    vec3_t driftDir; /* +0x88/8c/90: drift/velocity direction (a vec3).
                                     * CG_ComputeFlameChunkOrigin (0x30025990) multiplies it by
                                     * elapsed*driftSpeed to extrapolate the free (no-bone) chunk.
                                     * Merged from f2. */
    float driftSpeed; /* +0x94: chunk drift speed (single). Scales the free-chunk
                                     * extrapolation and drives the (speed*0.001111)^2 size term
                                     * in CG_ComputeFlameChunkOrigin (0x30025990). Merged from f2. */
    float radiusBaseA; /* +0x98: sprite radius/size base A (single). Merged from f2.
                                     * CG_AddFlameSpriteToScene (0x300268e0) selects it when the
                                     * caller's world/first-person flag arg == 1 (FLD [f+0x98]),
                                     * rounds it via Q_rint, and uses it as the half-extent of the
                                     * flame billboard quad. Provisional name unresolved. */
    float radiusBaseB; /* +0x9c: sprite radius/size base B (single). Merged from f2.
                                     * The alternate half-extent CG_AddFlameSpriteToScene picks
                                     * when the flag arg != 1 (FLD [f+0x9c]). Provisional. */
    int32_t deadFlag; /* +0xa0: dead/inactive flag. CG_AddFlameChunks
                                     * (0x300272b0) skips the per-chunk render/merge
                                     * body when deadFlag != 0. */
    int32_t birthTime; /* +0xa4: chunk birth/emit time (ms), read via
                                     * FILD as an int and compared against the
                                     * per-flame reference time in CG_AddFlameChunks. */
    float expansionRate; /* +0xa8: chunk expansion/radius rate (single). Read via FLD
                                     * by CG_ComputeFlameChunkOrigin (0x30025990): scaled by -1.5
                                     * for the drift term and multiplied into the free-chunk z
                                     * turbulence. Merged from f2. */
    vec3_t axisDir; /* +0xac/b0/b4: direction/basis row (a vec3), read by
                                     * CG_AddFlameChunks for the chunk-axis dot; written at
                                     * emit by CG_EmitPlayerFlameChunks (0x30024ce8) as a
                                     * copy of the drift dir (driftDir). */
    float soundAmpRate; /* +0xb8: per-chunk sound-amplitude rate; used as a
                                     * float multiplier when CG_UpdateFlamethrowerSounds
                                     * (0x30029210) accumulates the flame-sound-loop
                                     * envelope for this chunk's index. Exact source
                                     * name unresolved. */
    float alpha; /* +0xbc: initialized to 1.0f at emit by
                                     * CG_EmitPlayerFlameChunks (0x30024da8 / 0x30025476);
                                     * exact source name unresolved. */
    vec3_t posCopy; /* +0xc0/c4/c8: chunk position copy (a vec3); the emitter
                                     * (0x30024d83 / 0x300253f6) stamps the accum0 position
                                     * triple here as well as into localPos. */
    uint8_t padding0CC[4]; /* ABI_AUDITED_PADDING: aligns endTimeCopy at +0xd0. */
    double endTimeCopy; /* +0xd0: a chunk timestamp (double); the emitter stamps
                                     * chunkEndTime here (0x30024d89 / 0x300253f0). */
    vec3_t worldPos; /* +0xd8/dc/e0: chunk world position (a vec3). Merged from f2.
                                     * CG_AddFlameSpriteToScene (0x300268e0) reads it as
                                     * the sprite CENTER world position: it subtracts
                                     * cg_refdef.vieworg and VectorNormalize()s the difference to
                                     * build the billboard facing, uses it as the quad center, and
                                     * on exit stores the triple into cg_flameLastSpritePos
                                     * (0x300ab738/73c/740).
                                     * Retyped from the mechanical merge-placeholder uint32_t. */
    float radius; /* +0xe4: chunk radius / expansion magnitude (single).
                                     * CG_MergeFlameChunks (0x300257e0) uses it as the merge
                                     * discriminant (f2 copied into f1 only when
                                     * f2.radius > f1.radius). CG_AddFlameSpriteToScene
                                     * (0x300268e0) passes it as arg1 to CG_FlameDamage
                                     * (0x300265c0) and halves it (radius*0.5) as a size term. */
    float lifeFraction; /* +0xe8: chunk life/fade fraction (single, ~[0,1]). Merged
                                     * from f2. CG_AddFlameChunks (0x300272b0) derives it from the
                                     * chunk's elapsed-vs-lifetime ratio and passes it (and a curve
                                     * of it) to CG_AddFlameSpriteToScene (0x300268e0), which
                                     * compares it against 0.9f for the flicker/damage-trace gate
                                     * and against other thresholds for the color envelope.
                                     * Retyped from the merge-placeholder uint32_t to float. */
    uint8_t padding0EC[4]; /* ABI_AUDITED_PADDING: aligns endTimeCopy2 at +0xf0. */
    double endTimeCopy2; /* +0xf0: a chunk timestamp (double); the emitter stamps
                                     * chunkEndTime here (0x30025181 / 0x300254a4). */
    double endTimeCopy3; /* +0xf8: merged from f2 (double); the emitter stamps
                                     * chunkEndTime here too (0x30025187 / 0x300254aa). */
    vec3_t emitBasis; /* +0x100/104/108: emit basis-2 row (a vec3); the emitter stamps
                                     * the accum2 direction triple here (0x30025156). */
    vec3_t emitOrigin; /* +0x10c/110/114: emit origin (a vec3); the emitter stamps the
                                     * emitOrigin triple here (0x30025195 / 0x300254ba). */
    vec3_t centerOffset; /* +0x118/11c/120: sprite center world-offset (a vec3). Zeroed at
                                     * spawn. CG_AddFlameSpriteToScene (0x300268e0) reads it
                                     * as an additional offset applied to the
                                     * sprite center: when all three are 0 the offset block is
                                     * skipped (the common case); otherwise centerOffset[2] (via
                                     * fabs()*20.0) biases the center z. Retyped from the mechanical
                                     * zeroed-at-spawn uint32_t; provisional role name unresolved. */
    int32_t damageFrameStamp; /* +0x124: per-chunk "damage-frame processed" stamp
                                     * (cg_flameTime). CG_MoveFlameChunk (0x30025da0)
                                     * gates the once-per-chunk flame-damage-source spawn on
                                     * damageFrameStamp == 0 and, after handling it, stamps
                                     * damageFrameStamp = cg_flameTime so it does not re-run. Retyped
                                     * from the mechanical reserved gap; exact source name
                                     * unresolved. */
    int32_t emitCounter; /* +0x128: emit counter (cgFlameInfo emitCounter) stamped at emit by
                                     * CG_EmitPlayerFlameChunks (0x30024990 / 0x300252ae);
                                     * exact source name unresolved. */
    uint8_t padding12C[4]; /* ABI_AUDITED_PADDING: aligns lifeStartTime at +0x130. */
    double lifeStartTime; /* +0x130: min-time/lifetime (double); f2's value
                                     * overrides f1's only when f2==0 and f1!=0. Stamped at spawn
                                     * by CG_SpawnFlameChunkOnBone (0x30023fa9) with (double)cg_flameTime. */
    double lifeRate; /* +0x138: chunk life-rate (double). CG_SpawnFlameChunkOnBone
                                     * (0x30023fa3) stores (2*count) / (endTime - spawnTime) here —
                                     * the reciprocal of the chunk's lifetime span scaled by the
                                     * spawn `count`. Retyped from the mechanical reserved gap; exact
                                     * source name unresolved. */
    double lifeStartTime2; /* +0x140: min-time/lifetime (double); same
                                     * zero-override rule as lifeStartTime */
    float spawnScale; /* +0x148: initialized to 1.0f at spawn (scale/alpha; exact name unresolved) */
    uint8_t padding14C[4]; /* ABI_AUDITED_PADDING: tail-aligns the 0x150-byte record. */
} flameChunk_t;
/* The link fields are 4-byte pointers only at the 32-bit target; the layout guards
 * below are meaningful there and are compiled only when pointers are 4 bytes (the
 * i386 validation target). On a 64-bit host the 8-byte pointers shift the offsets,
 * so the asserts are skipped rather than falsely failing. */
#if UINTPTR_MAX == 0xFFFFFFFFu
_Static_assert(sizeof(flameChunk_t) == 0x150, "flameChunk_t must be 336 bytes");
_Static_assert(offsetof(flameChunk_t, next) == 0x00, "next @ +0x00");
_Static_assert(offsetof(flameChunk_t, prev) == 0x04, "prev @ +0x04");
_Static_assert(offsetof(flameChunk_t, parent) == 0x08, "parent @ +0x08");
_Static_assert(offsetof(flameChunk_t, listNext) == 0x0c, "listNext @ +0x0c");
_Static_assert(offsetof(flameChunk_t, listPrev) == 0x10, "listPrev @ +0x10");
_Static_assert(offsetof(flameChunk_t, unresolvedField_1c) == 0x1c, "unresolvedField_1c @ +0x1c");
_Static_assert(offsetof(flameChunk_t, liveFlag) == 0x20, "liveFlag @ +0x20");
_Static_assert(offsetof(flameChunk_t, listMarker) == 0x24, "listMarker @ +0x24");
_Static_assert(offsetof(flameChunk_t, kind) == 0x2c, "kind @ +0x2c");
_Static_assert(offsetof(flameChunk_t, overrideMaterial) == 0x30, "overrideMaterial @ +0x30");
_Static_assert(offsetof(flameChunk_t, ownerInfoIndex) == 0x34, "ownerInfoIndex @ +0x34");
_Static_assert(offsetof(flameChunk_t, ownerClientNum) == 0x38, "ownerClientNum @ +0x38");
_Static_assert(offsetof(flameChunk_t, boneHandle) == 0x40, "boneHandle @ +0x40");
_Static_assert(offsetof(flameChunk_t, soundAmpRate) == 0xb8, "soundAmpRate @ +0xb8");
_Static_assert(offsetof(flameChunk_t, spawnTime) == 0x48, "spawnTime @ +0x48");
_Static_assert(offsetof(flameChunk_t, endTime) == 0x50, "endTime @ +0x50");
_Static_assert(offsetof(flameChunk_t, startSpeed) == 0x58, "startSpeed @ +0x58");
_Static_assert(offsetof(flameChunk_t, startSpeedBits) == 0x5c, "startSpeedBits @ +0x5c");
_Static_assert(offsetof(flameChunk_t, smokeDensityRate) == 0x60, "smokeDensityRate @ +0x60");
_Static_assert(offsetof(flameChunk_t, spawnTimeCopy) == 0x68, "spawnTimeCopy @ +0x68");
_Static_assert(offsetof(flameChunk_t, localPos) == 0x70, "localPos @ +0x70");
_Static_assert(offsetof(flameChunk_t, driftStartTime) == 0x80, "driftStartTime @ +0x80");
_Static_assert(offsetof(flameChunk_t, driftDir) == 0x88, "driftDir @ +0x88");
_Static_assert(offsetof(flameChunk_t, driftSpeed) == 0x94, "driftSpeed @ +0x94");
_Static_assert(offsetof(flameChunk_t, radiusBaseA) == 0x98, "radiusBaseA @ +0x98");
_Static_assert(offsetof(flameChunk_t, radiusBaseB) == 0x9c, "radiusBaseB @ +0x9c");
_Static_assert(offsetof(flameChunk_t, deadFlag) == 0xa0, "deadFlag @ +0xa0");
_Static_assert(offsetof(flameChunk_t, birthTime) == 0xa4, "birthTime @ +0xa4");
_Static_assert(offsetof(flameChunk_t, expansionRate) == 0xa8, "expansionRate @ +0xa8");
_Static_assert(offsetof(flameChunk_t, emitArgFlag) == 0x28, "emitArgFlag @ +0x28");
_Static_assert(offsetof(flameChunk_t, centFlags) == 0x3c, "centFlags @ +0x3c");
_Static_assert(offsetof(flameChunk_t, axisDir) == 0xac, "axisDir @ +0xac");
_Static_assert(offsetof(flameChunk_t, alpha) == 0xbc, "alpha @ +0xbc");
_Static_assert(offsetof(flameChunk_t, posCopy) == 0xc0, "posCopy @ +0xc0");
_Static_assert(offsetof(flameChunk_t, endTimeCopy) == 0xd0, "endTimeCopy @ +0xd0");
_Static_assert(offsetof(flameChunk_t, endTimeCopy2) == 0xf0, "endTimeCopy2 @ +0xf0");
_Static_assert(offsetof(flameChunk_t, emitBasis) == 0x100, "emitBasis @ +0x100");
_Static_assert(offsetof(flameChunk_t, emitOrigin) == 0x10c, "emitOrigin @ +0x10c");
_Static_assert(offsetof(flameChunk_t, damageFrameStamp) == 0x124, "damageFrameStamp @ +0x124");
_Static_assert(offsetof(flameChunk_t, emitCounter) == 0x128, "emitCounter @ +0x128");
_Static_assert(offsetof(flameChunk_t, worldPos) == 0xd8, "worldPos @ +0xd8");
_Static_assert(offsetof(flameChunk_t, radius) == 0xe4, "radius @ +0xe4");
_Static_assert(offsetof(flameChunk_t, lifeFraction) == 0xe8, "lifeFraction @ +0xe8");
_Static_assert(offsetof(flameChunk_t, endTimeCopy3) == 0xf8, "endTimeCopy3 @ +0xf8");
_Static_assert(offsetof(flameChunk_t, lifeStartTime) == 0x130, "lifeStartTime @ +0x130");
_Static_assert(offsetof(flameChunk_t, lifeRate) == 0x138, "lifeRate @ +0x138");
_Static_assert(offsetof(flameChunk_t, lifeStartTime2) == 0x140, "lifeStartTime2 @ +0x140");
_Static_assert(offsetof(flameChunk_t, centerOffset) == 0x118, "centerOffset @ +0x118");
_Static_assert(offsetof(flameChunk_t, spawnScale) == 0x148, "spawnScale @ +0x148");
#endif

/*
 * CG_ComputeFlameChunkOrigin (0x30025990, provisional role name) — per-frame update of a
 * flame chunk's world position (and, for free chunks, a vertical turbulence term). Called
 * from CG_SpawnFlameChunkOnBone (0x30023faf) with cg_flameTime and &out. Two paths, on
 * flameChunk boneHandle (+0x40):
 *   - boneHandle != 0 (bone-attached): resolve the owning entity's DObj (trap 0xa5),
 *     calculate its bone hierarchy (CG_DObjCalcBone), fetch the entity bone-matrix
 *     table (trap 0xa0), build a local placement matrix from the entity's lerpOrigin /
 *     lerpAngles (AnglesToAxisNegRight), compose it into the bone matrix
 *     (CG_ComposeBoneMatrix), and transform the chunk's local offset (field_70/74/78) into
 *     world space: out = translation + row0*field_70 + row1*field_74 + row2*field_78.
 *   - boneHandle == 0 (free chunk): out = position(field_70/74/78) extrapolated by
 *     elapsed*field_94*(field_88/8c/90); out[2] additionally gets a size/turbulence term
 *     built from a clamped (0.65*(field_94*0.001111)^2 + 0.35*(1 - field_e4*0.003448)) in
 *     [0,1] and the two elapsed-time deltas.
 * Register-argument ABI: the flame chunk `f` arrives in EBX; `cg_flameTime` (int) and the
 * output vec3 pointer are the two caller-cleaned stack args. Modeled as ordered parameters;
 * no calling-convention attribute (syntax-only build). The .mcode size-guess "ClientConnect"
 * is REJECTED: this is cgame flame-chunk position math (DObj bone-matrix transform + x87
 * drift), not a server client-connect handler. Reconstructed at
 * src/client/cgame/effects/cg_computeflamechunkorigin.c. */
void CG_ComputeFlameChunkOrigin(flameChunk_t *f, int32_t cg_flameTime, vec3_t out);

/* ==========================================================================
 * ADJUDICATED (was a NAMING CONFLICT): the three helpers below, at 0x30023b70,
 * 0x30025c60 and 0x30023c30, were once ALSO declared as CG_ADSAnim_ComputeRate /
 * CG_ADSAnim_AdvanceZoom / CG_PlayADSAnim taking a cg_adsAnimState_t*. The machine
 * code proves those addresses operate on a flameChunk_t, not an ADS-anim struct:
 * 0x30023b70 reads kind (+0x2c), ownerInfoIndex (+0x34, compared against
 * cg_snap->ps.psClientNum), driftSpeed (+0x94), soundAmpRate (+0xb8), startSpeedBits
 * (+0x5c) and the double timestamps spawnTime/endTime (+0x48/+0x50); 0x30025c60
 * advances the chunk's radius (+0xe4) toward startSpeedBits (+0x5c) using that rate
 * and stamps spawnTimeCopy (+0x68); 0x30023c30 advances driftSpeed (+0x94),
 * recomputes startSpeedBits and sizeRate (+0x64), and shares the flame clock. Every
 * caller passes a flame chunk in EAX/ESI and lives in the flame subsystem;
 * CG_FireFlameChunks (0x30027d10) calls all three with flameChunk_t*. The ADS
 * struct and decls have been removed — these are the single source of truth.
 * ==========================================================================
 *
 * CG_FlameGetSizeRate (0x30023b70) — compute a flame chunk's per-frame size/expansion
 * advance rate. `this` (the flame chunk) arrives in ESI (register-argument helper);
 * float result returned on ST(0). Body reads kind/ownerInfoIndex/driftSpeed/soundAmpRate
 * and the chunk's double life span (endTime - spawnTime) and mixes cg_snap->ps.psClientNum. */
/* long double return: 0x30023b70 ends FADDP; ADD ESP; RET with no store, so it
 * returns raw st(0). CG_AdvanceFlameChunkSize FMULPs it and CG_FireFlameChunks
 * FMULs it by 14.5f, both before any rounding (Class 7). The five callers that
 * store the result straight to a float slot are unaffected (float widens
 * exactly, then their store rounds once). */
long double CG_FlameGetSizeRate(flameChunk_t *f);

/* CG_AdvanceFlameChunkSize (0x30025c60) — advance a flame chunk's radius
 * (+0xe4) toward its target startSpeedBits (+0x5c) at the rate from CG_FlameGetSizeRate,
 * clamping to the target and to 290.0f, and record spawnTimeCopy (+0x68). `this`
 * arrives in EAX; one caller-cleaned int32 stack arg = the current flame time
 * (Q_rint of (double)field_130 at the flame call sites). */
void CG_AdvanceFlameChunkSize(flameChunk_t *f, int32_t flameTime);

/* CG_UpdateFlameChunk (0x30023c30 == FUN_30023c30_30023d44, provisional role name) —
 * advance one flame chunk one frame. Accumulate the drift timer driftSpeed (+0x94)
 * by the per-frame delta `dt` (held at >= 30.0 while nonzero); while kind (+0x2c) < 2,
 * recompute the size target startSpeedBits (+0x5c) from soundAmpRate/smokeDensityRate
 * (clamped to 290.0); then, for the active local chunk (deadFlag == 0), write the
 * per-frame size-advance rate sizeRate (+0x64) either directly (startSpeedBits/1666)
 * while the chunk's start delay (birthTime vs cg_flameTime - spawnTime) is pending, or
 * via CG_FlameGetSizeRate once it has elapsed. `this` arrives in EAX; `dt` is one
 * caller-cleaned float stack arg; no value returned. Sole caller is
 * CG_MoveFlameChunk (0x30025faa), which passes the chunk and -x. */
void CG_UpdateFlameChunk(flameChunk_t *f, float dt);

/* CG_FlameDropDrip (0x30027ad0) — spawn a fresh flame chunk
 * (CG_SpawnFlameChunk(NULL)), clone `parent`'s
 * state into it (preserving the new node's own list links), jitter its drift
 * direction, and seed its life span / size-rate from the two float parameters `a`
 * (radius/speed base) and `b` (drift gate). Reconstructed at
 * src/client/cgame/effects/cg_flamedropdrip.c. The Mac body has the identical three
 * named direct callees, resolving the source name.
 *
 * DECL CORRECTION (machine wins over prior caller-observed decl):
 *   1. RETURN TYPE — the body ends with `MOV EAX,EBX` (EAX = the new chunk) on the
 *      success path and `XOR EAX,EAX` (NULL) on the spawn-failure path, so this
 *      returns flameChunk_t*, NOT void. The prior `void` decl was caller-observed
 *      from a call site that ignored EAX.
 *   2. The two floats are `a = [esp+0x170]` (first float arg) and
 *      `b = [esp+0x174]` (second float arg); ABI is plain cdecl (SUB ESP frame,
 *      plain RET, caller cleans). */
flameChunk_t *CG_FlameDropDrip(flameChunk_t *parent, float a, float b);

/* CG_MoveFlameChunk (0x30025da0)
 * — per-frame finalize/submit of a single flame chunk: reads the chunk's field_94,
 * field_bc, field_f8, its owner cgFlameInfo[field_34].field_40 (+0x300ab790) and
 * various render state, and advances/submits the chunk for the given flame time.
 * Caller-observed ABI (proven at the CG_FireFlameChunks call sites 0x3002800c /
 * 0x30028ee6 / 0x30028f22 / 0x3002909b): two caller-cleaned stack args pushed in
 * reverse — (flameChunk_t *chunk, int32_t flameTime). NOTE: this is NOT the pool
 * merge helper CG_MergeFlameChunks (0x300257e0); the PHASE-2 interface spec labeled
 * 0x30025da0 "CG_MergeFlameChunks" but the machine code is a distinct per-chunk
 * finalize. Superseded by its own .mcode. */
void CG_MoveFlameChunk(flameChunk_t *chunk, int32_t flameTime);

/*
 * Flame-chunk pool management. Provisional decls; each is superseded by its own
 * .mcode reconstruction.
 *   - CG_ClearFlameChunks (0x30025570): reset the pool to empty and rebuild the
 *     free list (this function).
 *   - CG_SpawnFlameChunk (0x30025600): pop a free node, link it active, return it
 *     (NULL when the pool is exhausted).
 *   - CG_FreeFlameChunk (0x300256e0): recursively free a node and its children.
 *   - CG_InitFlameChunks (0x300279d0): allocate the pool via cgame trap 0xc0 then
 *     call CG_ClearFlameChunks. */
void CG_ClearFlameChunks(void);
/* CG_SpawnFlameChunk takes its `parent` argument in ESI (register-passed by every
 * caller, which does XOR ESI,ESI for a root chunk or MOV ESI,<chunk> to chain a
 * child before the CALL); it returns the new node in EAX (NULL when the free list
 * is empty). When parent is non-NULL the new node inherits the parent's place on
 * the secondary cg_flameChunkList (the parent is unlinked from that list and the
 * new node is spliced in at its head). Modelled here as an ordinary parameter. */
flameChunk_t *CG_SpawnFlameChunk(flameChunk_t *parent);
void CG_FreeFlameChunk(flameChunk_t *chunk);
/* CG_MergeFlameChunks (0x300257e0): fuse the trailing flame chunk f2 into the
 * leading chunk f1 (f2 must immediately follow f1 on the parent forward chain:
 * f1->parent == f2, asserted via Com_ErrorMessage), copy f2's render/physics state
 * into f1 under a discriminant, then free f2. Args: EDI=f1 (into), ESI=f2 (from),
 * both register-passed by the caller; RET (no immediate) but no args on the stack —
 * pure register-argument entry (the PUSH ECX/POP ECX bracket the body). */
void CG_MergeFlameChunks(flameChunk_t *f1, flameChunk_t *f2);
/* CG_InitFlameChunks (0x300279d0, this reconstruction): allocate the flame-chunk pool
 * (cgame trap 0xc0) and register the flame material/effect assets, then reset the
 * pool via CG_ClearFlameChunks. Void, no args, caller-cleaned frame. */
void CG_InitFlameChunks(void);

/* --- Flame callee declarations consolidated from the reconstructed bodies (body-proven,
 * not caller-guessed). These carry authority for the deferred flame giants
 * (CG_EmitPlayerFlameChunks 0x30024050, CG_FireFlameChunks 0x30027d10); a later
 * reconstruction may override a signature but must state why. --- */

/* CG_FlameDamage (0x300265c0, reconstructed FUN_300265c0_300268d7): traces from
 * a flame chunk's world position toward nearby entities and applies flame damage/marks.
 * Signature proven from its own reconstructed body. */
void CG_FlameDamage(const vec3_t flamePos, int32_t ownerClientNum, float radiusBase, const flameChunk_t *chunk);

/* CG_AddFlameChunks (0x300272b0, reconstructed FUN_300272b0_3002783f): per-owner-chunk
 * frame update — advances/merges the chunk's children and submits their sprites via
 * CG_AddFlameToScene. Body-proven signature: takes the owner (root) chunk. */
void CG_AddFlameChunks(flameChunk_t *ownerChunk);

/* CG_AddFlameToScene (0x300268e0 == FUN_300268e0_300272ac, alias CG_AddFlameSpriteToScene):
 * build one camera-facing flame/smoke billboard quad for a flameChunk_t and submit it via
 * trap_R_AddPolyToScene (CG_R_ADDPOLYTOSCENE=0x40). Reconstructed from the dense x87
 * body; the call shape is also proven from CG_AddFlameChunks at 0x30027819. */
void CG_AddFlameToScene(flameChunk_t *chunk, float animationFraction, float alpha, int32_t finalFrame);

/* CG_SpawnFlameChunkOnBone (0x30023d50) is already declared above (near line 13583) with a
 * better-evidenced signature: void CG_SpawnFlameChunkOnBone(centity_t*, const vec3_t,
 * const char *boneName, int32_t durationMsec, float startSpeed, int32_t count). Not
 * re-declared here — that decl stands (its own body FUN_30023d50 is still unreconstructed). */

/*
 * cgFlameInfo_t — one element of the per-owner flame-info region cg_flameInfo
 * (base 0x300ab750, stride FLAME_INFO_SIZE == 0xb8, FLAME_INFO_COUNT == 1024
 * elements). Only the fields CG_UpdateFlamethrowerSounds (0x30029210) proves are named;
 * the rest of the 184-byte element is opaque render/emit state. The exporter had
 * carved several interior dwords of this region into separate g_data_* symbols
 * (0x300ab750/790/794/7ac/7b0/7b4); those aliases are the fields modelled here.
 * Exact source field names unresolved — provisional by role.
 *
 * This is the owning element type of cg_flameInfo. Index is the flame chunk's
 * field_34 (a per-owner/per-limb index).
 */
typedef struct cgFlameInfo_s {
    int32_t clientFrame; /* +0x00: cg.clientFrame stamp of the last emitter
                                  * update. Current/recent checks compare it against
                                  * cg_clientFrame-1; CG_EmitPlayerFlameChunks writes
                                  * the current frame here on exit (0x30025550). */
    vec3_t prevDir; /* +0x04/+0x08/+0x0c: the emitter's previous-frame aim
                                     * direction (a vec3). CG_EmitPlayerFlameChunks
                                     * (0x30024050) reads it at 0x30024131/0x30024177 as the
                                     * angle-delta base (originDir - prevDir) for the emit
                                     * rate limiter, again at 0x300245c3 as the AngleVectors#2
                                     * input, and writes the current dir back on exit
                                     * (0x300254f3). Provisional name; exact source unresolved. */
    vec3_t prevEmitOrigin; /* +0x10/+0x14/+0x18: the emitter's previous-frame
                                     * emit origin (a vec3). Read at 0x300243a5 as the vDir
                                     * spawn-delta base (emitOrigin - prevEmitOrigin) and at
                                     * 0x300245fc as the rowEmit lerp base; written from the
                                     * current emitOrigin on exit (0x30025508). Provisional
                                     * name; exact source unresolved. */
    vec3_t prevSpawnVelA; /* +0x1c/+0x20/+0x24: previous-frame spawn velocity vec3
                                     * (a copy of vDir). Written on exit (0x3002551d) but not
                                     * read by this function. Provisional name. */
    vec3_t prevSpawnVel; /* +0x28/+0x2c/+0x30: previous-frame spawn velocity vec3
                                     * (the second copy of vDir). Read at 0x3002460f as the
                                     * accum1 lerp base (angInfo2 reuse) and written from the
                                     * current vDir on exit (0x30025523). Provisional name;
                                     * exact source unresolved. */
    vec3_t emitDir; /* +0x34/+0x38/+0x3c: the emitter's previous-frame
                                     * spawn direction (a vec3). CG_EmitPlayerFlameChunks
                                     * (0x30024050) reads it at 0x30024216.. as the accum2
                                     * lerp base (infoDir2) and writes the current dir back
                                     * on exit. Provisional name; exact source unresolved. */
    flameChunk_t *ownerChunk; /* +0x40: the flame chunk that "owns" this info.
                                     * CG_MoveFlameChunk (0x30025da0) loads it via
                                     * [idx*0xb8 + 0x300ab790], null-checks it, and reads its
                                     * emitBasis[3] and emitOrigin[3]. Retyped from the mechanical int32
                                     * placeholder to flameChunk_t*; exact source name unresolved. */
    int32_t lastUpdateTime; /* +0x44: last emitter update stamp (set to the
                                     * flame-time flameTime/EBP at 0x3002957f). */
    uint8_t emitterState48[4]; /* ABI_AUDITED_OPAQUE: unconsumed flame-emitter state. */
    int32_t emitSeedGate; /* +0x4c: emit-index seed threshold (flame-clock units).
                                     * CG_EmitPlayerFlameChunks (0x30024050) compares it
                                     * against the per-iteration radius seed (0x30024c28/35)
                                     * to gate the rand-scattered emit-index block and stores
                                     * the seed back here (0x30024c76). Provisional name;
                                     * exact source unresolved. */
    uint8_t emitterState50[4]; /* ABI_AUDITED_OPAQUE: unconsumed flame-emitter state. */
    int32_t lastEmitTime; /* +0x54: last-emit timestamp (flame-clock ms).
                                     * CG_EmitPlayerFlameChunks gates a non-owner emitter
                                     * on lastEmitTime >= cg_flameTime - 150 (0x3002431b);
                                     * CG_UpdateFlamethrowerSounds also touches +0x54. Provisional
                                     * name; exact source unresolved. */
    int32_t emitAccumTime; /* +0x58: accumulated emit time (ms). CG_EmitPlayerFlameChunks
                                     * (0x30024050) adds cg_frametime to it each non-terminal
                                     * emit (0x30024562) and resets it to 0 on the terminal
                                     * path (0x30025222). Provisional name; exact source
                                     * unresolved. */
    int32_t soundPathFlag; /* +0x5c: nonzero (== 1) gates the primary vs
                                     * secondary sound-envelope path. CG_EmitPlayerFlameChunks
                                     * compares it against arg3 and stamps arg3 into it. */
    int32_t activeFlag; /* +0x60: nonzero flag; the final decay loop skips
                                     * an element whose activeFlag is 0. CG_EmitPlayerFlameChunks
                                     * also writes arg3 here on exit (0x300254ec). */
    int32_t soundActiveFlag; /* +0x64: nonzero => this info's sound is already
                                     * active; CG_UpdateFlamethrowerSounds skips the chunk. */
    uint8_t soundState68[4]; /* ABI_AUDITED_OPAQUE: unconsumed flame sound state. */
    int32_t activeUntil; /* +0x6c: cg_flameTime through which this owner's
                                    * flame can apply damage. */
    uint8_t ownerRuntimeState[28]; /* ABI_AUDITED_OPAQUE: per-owner flame runtime state. */
    int32_t emitRandSeed; /* +0x8c: per-emit random seed. CG_EmitPlayerFlameChunks
                                     * (0x30024050) stores the per-iteration rand() radius
                                     * seed (EBX) here at 0x30024c20; the retry sub-loop
                                     * bumps it. Provisional name; exact source unresolved. */
    int32_t emitCounter; /* +0x90: monotonic emit counter. CG_EmitPlayerFlameChunks
                                     * (0x30024050) copies it into chunk->emitCounter at each
                                     * chunk spawn (0x30024990) and increments it on the
                                     * terminal path (0x30025251). Provisional name; exact
                                     * source unresolved. */
    vec3_t lastFlamePos; /* +0x94/+0x98/+0x9c: the world position of the flame
                                     * chunk that last updated this owner's flame-damage source.
                                     * CG_MoveFlameChunk (0x30025da0) reads it
                                     * (VectorDistance vs the chunk world origin, gated < 32.0f)
                                     * and writes the chunk's worldPos[3] back here when the
                                     * source is latched. Was three mechanical g_data_* dwords
                                     * (0x300ab7e4/e8/ec). Provisional name; exact source
                                     * unresolved. */
    int32_t lastFlameStamp; /* +0xa0: cg_flameTime at which lastFlamePos was recorded.
                                     * CG_MoveFlameChunk gates the source-update window
                                     * on it (compared against cg_flameTime and cg_flameTime-6000).
                                     * Was mechanical g_data_* dword 0x300ab7f0. Provisional. */
    uint32_t damageActive; /* +0xa4: nonzero while the client is burning */
    int32_t lastPainTime; /* +0xa8: cg_time of last pain notification */
    int32_t painCounter; /* +0xac: caller-supplied pain event id */
    double lastFlameTime; /* +0xb0: last emit's flame-clock time as a double.
                                     * CG_EmitPlayerFlameChunks stores (double)cg_flameTime
                                     * here at the end of each loop iteration (0x300250ee).
                                     * Provisional name; exact source unresolved. */
} cgFlameInfo_t;
extern cgFlameInfo_t cg_flameInfo[FLAME_INFO_COUNT];
#if UINTPTR_MAX == 0xFFFFFFFFu
_Static_assert(sizeof(cgFlameInfo_t) == 0xb8, "cgFlameInfo_t must be 184 bytes");
_Static_assert(offsetof(cgFlameInfo_t, prevDir) == 0x04, "cgFlameInfo prevDir @ +0x04");
_Static_assert(offsetof(cgFlameInfo_t, prevEmitOrigin) == 0x10, "cgFlameInfo prevEmitOrigin @ +0x10");
_Static_assert(offsetof(cgFlameInfo_t, prevSpawnVelA) == 0x1c, "cgFlameInfo prevSpawnVelA @ +0x1c");
_Static_assert(offsetof(cgFlameInfo_t, prevSpawnVel) == 0x28, "cgFlameInfo prevSpawnVel @ +0x28");
_Static_assert(offsetof(cgFlameInfo_t, emitDir) == 0x34, "cgFlameInfo emitDir @ +0x34");
_Static_assert(offsetof(cgFlameInfo_t, emitSeedGate) == 0x4c, "cgFlameInfo emitSeedGate @ +0x4c");
_Static_assert(offsetof(cgFlameInfo_t, lastEmitTime) == 0x54, "cgFlameInfo lastEmitTime @ +0x54");
_Static_assert(offsetof(cgFlameInfo_t, emitAccumTime) == 0x58, "cgFlameInfo emitAccumTime @ +0x58");
_Static_assert(offsetof(cgFlameInfo_t, emitCounter) == 0x90, "cgFlameInfo emitCounter @ +0x90");
_Static_assert(offsetof(cgFlameInfo_t, lastFlameTime) == 0xb0, "cgFlameInfo lastFlameTime @ +0xb0");
_Static_assert(offsetof(cgFlameInfo_t, ownerChunk) == 0x40, "cgFlameInfo ownerChunk @ +0x40");
_Static_assert(offsetof(cgFlameInfo_t, lastFlamePos) == 0x94, "cgFlameInfo lastFlamePos @ +0x94");
_Static_assert(offsetof(cgFlameInfo_t, lastFlameStamp) == 0xa0, "cgFlameInfo lastFlameStamp @ +0xa0");
_Static_assert(offsetof(cgFlameInfo_t, lastUpdateTime) == 0x44, "cgFlameInfo lastUpdateTime @ +0x44");
_Static_assert(offsetof(cgFlameInfo_t, soundPathFlag) == 0x5c, "cgFlameInfo soundPathFlag @ +0x5c");
_Static_assert(offsetof(cgFlameInfo_t, activeFlag) == 0x60, "cgFlameInfo activeFlag @ +0x60");
_Static_assert(offsetof(cgFlameInfo_t, soundActiveFlag) == 0x64, "cgFlameInfo soundActiveFlag @ +0x64");
_Static_assert(offsetof(cgFlameInfo_t, activeUntil) == 0x6c, "cgFlameInfo activeUntil @ +0x6c");
_Static_assert(offsetof(cgFlameInfo_t, emitRandSeed) == 0x8c, "cgFlameInfo emitRandSeed @ +0x8c");
_Static_assert(offsetof(cgFlameInfo_t, damageActive) == 0xa4, "cgFlameInfo damageActive @ +0xa4");
_Static_assert(offsetof(cgFlameInfo_t, lastPainTime) == 0xa8, "cgFlameInfo lastPainTime @ +0xa8");
_Static_assert(offsetof(cgFlameInfo_t, painCounter) == 0xac, "cgFlameInfo painCounter @ +0xac");
#endif

/*
 * cgFlameSoundLoop_t — one element of the flame sound-loop envelope table (base
 * 0x300a8718, stride 12 bytes = 3 dwords), indexed by the flame chunk's field_34.
 * CG_UpdateFlamethrowerSounds (0x30029210) accumulates and clamps two amplitude envelopes
 * per index and stamps the per-frame owner. Exact source name unresolved.
 *
 * The full definition lives in globals.h (which is parsed first and needs the
 * complete type for the extern array cg_flameSoundLoops[1024]); it is reused here,
 * not redefined, to avoid a duplicate struct-tag definition.
 */

/*
 * CG_UpdateFlamethrowerSounds (0x30029210, this reconstruction) — per-frame flame/burning
 * sound-loop update. Walks the secondary flame-chunk list (cg_flameChunkList via
 * flameChunk_t.listNext) accumulating a per-index looping-sound amplitude envelope
 * for each active chunk, plays the "fl_catch_fire_lo"/"fl_catch_fire_hi" looping
 * catch-fire sounds through the cgame sound traps for chunks whose burn state has
 * elapsed, then decays every table envelope toward zero. Void, no args; caller-
 * cleaned frame (RET, no immediate). The mechanical pre-hint name matches this
 * behavior; renamed to CG_UpdateFlamethrowerSounds because the loop also drives ambient
 * catch-fire (player-burning) sounds, not only the flamethrower weapon.
 *
 * cg_clientFrame (0x30459140) is the per-frame stamp consumed here as
 * `clientFrame - 1`; declared in globals.h (its canonical home).
 *
 * CG_PlayFlameChunkSound (0x30025990, provisional) — emit/position one chunk's
 * flame sound. Register+stack ABI: the chunk is in EBX at the call site (the walk
 * register), plus two stack pushes: the flameChunk pointer and a &float parameter.
 * Modelled by its observed call shape; superseded by its own .mcode. */
void CG_UpdateFlamethrowerSounds(void);
int32_t CG_PlayFlameChunkSound(flameChunk_t *chunk, int32_t arg, float *param);

/*
 * CG_RegisterMaterial (0x3003db80) — register a render material by name and
 * return its qhandle_t. CG_InitFlameChunks calls it once per flame sprite frame with
 * (name, 4); it forwards to cgame_syscall(CG_REGISTER_MATERIAL, name, 4) and returns
 * the resulting handle. The name (buffer) arrives in ECX/EDX at the call site and the
 * `param` (a constant 4) on the stack; both flame call sites clean the args
 * (caller-cleaned). The callee also performs one-time flame-material warm-up work
 * guarded by an internal flag before issuing the registration; that side of the body
 * is out of scope here. Provisional name by role/behavior; the .mcode size-guess name
 * `Weapon_Melee` is rejected (this registers a material, it fires no weapon).
 * Superseded by its own .mcode reconstruction.
 *
 * NAMING NOTE: "Flame" is a first-caller misnomer. This is the generic material/
 * shader register — CG_RegisterItemVisuals (0x30044ac0) also calls it as (hudIcon, 5)
 * to register an item HUD icon shader, and CG_HeadIcon (0x30021540) as (headicon, 5).
 * The generic role is proven; CG_RegisterMaterial is the resolved symbol used by
 * every caller. */
qhandle_t CG_RegisterMaterial(const char *name, int param);

/*
 * CG_ScoreboardHeight (0x30036e50) — compute the total pixel height of the
 * multiplayer scoreboard for the rows collected this frame, and return the
 * number of drawn lines through *lineCount. The single argument (a pointer to
 * the line counter) is passed in EAX by the caller (the CG_DrawScoreboard body
 * at 0x30037b98 does `lea eax,[esp+0x10]` with no push); the accumulated height
 * is returned on the x87 stack (ST0). Provisional name (behavior-proven); the
 * exact original symbol is unconfirmed.
 */
float CG_ScoreboardHeight(int32_t *lineCount);

/* CG_CM_BOX_TRACE..41 return the shared 48-byte trace_t from
 * q_collision_types.h. The cgame wrappers patch entityNum at +0x28 and copy
 * all 12 i386 dwords. Earlier
 * reconstruction incorrectly introduced a second trace type with aliases over
 * normal and contents; server and engine layouts confirm those bytes are the
 * ordinary trace normal, contents, material, and hit-metadata fields. */

/* CG_Trace stamps trace_t.entityNum with ENTITYNUM_NONE when the collision trace
 * reaches the far end and ENTITYNUM_WORLD when it hits the world. */

/*
 * CG_BuildSolidList (0x30035030) — reconstructed in
 * functions/FUN_30035030_300350c2.c. Rebuild the per-frame collision lists from
 * cg_nextSnap. ET_ITEM entities enter cg_triggerEntities[]; other entities with a
 * nonzero nextState.solid enter cg_solidEntities[]. Non-solid inline brush models
 * are excluded. The name is confirmed by the same-module Mac symbol bank. */
void CG_BuildSolidList(void);

/*
 * CG_TouchTriggerPrediction (0x30035710) — reconstructed in
 * functions/FUN_30035710_300357c2.c. Predict item touches and brush-trigger
 * intersections using cg_triggerEntities[]. The name is confirmed by the
 * same-module Mac symbol bank. */
void CG_TouchTriggerPrediction(void);

/*
 * CG_PointContents (0x30035420) — combine world point contents with transformed
 * inline-model contents from cg_solidEntities[], excluding passEntityNum, then
 * apply contentMask. Name confirmed by the same-module Mac symbol bank.
 */
int32_t CG_PointContents(const vec3_t point, int32_t passEntityNum, int32_t contentMask);

/*
 * CG_ClipMoveToEntities (0x300350d0) — reconstructed in
 * functions/FUN_300350d0_3003530e.c. Clip a movement trace against the entities
 * in cg_solidEntities[], using inline brush models or temporary box/capsule models.
 * Name confirmed by the same-module Mac symbol bank. */
void CG_ClipMoveToEntities(const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int32_t excludeId, int32_t flagsMask,
                           int32_t useVariant, trace_t *out);

/*
 * CG_Trace (0x30035310) — thin cgame wrapper that runs a trace/
 * collision trap (CG_CM_BOX_TRACE / 0x26) into a local trace_t, tags the
 * result's entityNum field by whether the trace reached the far end
 * (fraction == 1.0f), runs CG_ClipMoveToEntities over solid centities, then
 * copies the 48-byte result out to *out.
 *
 * Custom register ABI (proven from the call sites, e.g. 0x30020e29): EAX =
 * contentMask, ECX = end, EBX = maxs, plus four cdecl stack dwords
 * (out, start, mins, passEntityNum); the function ends with a plain RET (caller
 * cleans the stack). Provisional name (behavior-proven role); the exact source symbol is
 * unconfirmed, and the .mcode-assigned "G_UpdateHudElemsToClients" is REJECTED
 * (that is a server G_* name; this is client cgame collision code).
 */
void CG_Trace(int32_t contentMask, const vec3_t end, const vec3_t maxs, trace_t *out, const vec3_t start, const vec3_t mins,
              int32_t passEntityNum);

/*
 * CG_ScanForCrosshairEntity (0x3001a4d0) — reconstructed in
 * functions/FUN_3001a4d0_3001a5ab.c. Trace a "shot" ray from the camera origin
 * (cg_refdef.vieworg) 8192 units along the current aim direction and, if it hits a
 * hittable entity below MAX_CLIENTS that isn't the local player, latch it as the
 * crosshair target (cg_crosshairEntNum / cg_crosshairEntTime = cg_time). Vehicles
 * redirect to their occupant (currentState.vehicleEntityNum). Consumed by the
 * crosshair-name HUD drawer at 0x3001a604. Takes no args, returns nothing.
 * The .mcode size-guess "CG_DrawScoreboard" is REJECTED: the real CG_DrawScoreboard
 * is at 0x30037d90; this function issues a trace and writes the crosshair globals.
 */
void CG_ScanForCrosshairEntity(void);

/*
 * CG_TraceCapsule (0x30035390) — the capsule-trace twin of CG_Trace,
 * used as a callback (its address is pushed as a function pointer to the iterator
 * 0x3000c8e0 by the enumerator at 0x300354b0), so all of its arguments arrive on
 * the stack rather than in registers. It runs the collision trap CG_CM_CAPSULE_TRACE
 * (0x28) into a local trace_t, tags the result's entityNum field by whether
 * the trace reached the far end (fraction == 1.0f), runs the solid-entity helper
 * CG_ClipMoveToEntities over it, then copies the 48-byte result out to *out.
 *
 * It differs from CG_Trace only in two proven ways:
 *   - it issues trap CG_CM_CAPSULE_TRACE (0x28) rather than CG_CM_BOX_TRACE (0x26);
 *   - it passes 1 (not 0) in CG_ClipMoveToEntities's next-to-last argument (the
 *     parameter the caller-observed decl names `zero`; it is really a flag, and
 *     this twin proves it can be 1).
 *
 * All seven inputs are cdecl stack arguments (plain RET, caller-cleaned); EBX/EBP/
 * ESI/EDI are callee-saved. Name is provisional-by-role; the mechanical .mcode
 * guess "Scr_Vehicle_Use" is REJECTED (that is a server Scr_* symbol, while this is
 * client cgame collision code that calls through cgame_syscall and the solid-entity
 * helper CG_ClipMoveToEntities).
 */
void CG_TraceCapsule(trace_t *out, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int32_t passEntityNum,
                     int32_t contentMask);

/*
 * CG_FlamethrowerTrace (0x30025cd0) — reconstructed in
 * functions/FUN_30025cd0_30025d94.c. A trace helper in the flame-chunk cluster
 * (its callers are the flame-fire paths at 0x30025da0 and 0x30027d10 / the
 * provisional CG_FireFlameChunks). It first runs the ordinary world trace/
 * projection CG_Trace (0x30035310) into the caller's `out` buffer, then
 * — unless the traced entity IS the local player (entityNum == cg_snap->ps.psClientNum)
 * — refines the result against the local player's own body: it issues the setup
 * trap CG_CM_TEMP_CAPSULE_MODEL (0x2a, CONTENTS_BODY, cg_snap+0x568, cg_snap+0x574) and feeds
 * its handle into the body trace CG_CM_TRANSFORMED_CAPSULE_TRACE (0x29) writing a local
 * trace_t. If that body trace was all-solid (result.allsolid != 0) OR came back nearer
 * than the world trace (body.fraction < out->fraction), it stamps
 * body.entityNum = (uint16_t)cg_snap->ps.psClientNum and copies the 48-byte body result
 * over *out. Otherwise the world result is left in place.
 *
 * Custom register ABI (proven from the call sites at 0x300260fa / 0x3002828c):
 *   EAX = contentMask (int)
 *   ECX = maxs (const vec3_t)
 *   EDX = mins (const vec3_t)
 *   [ESP+4]  out       (trace_t *, final destination)
 *   [ESP+8]  start     (const vec3_t)
 *   [ESP+0xc] end      (const vec3_t)
 *   [ESP+0x10] entityNum (int)       -> compared to cg_snap->ps.psClientNum; also
 *                                       CG_Trace passEntityNum
 * The function ends with a plain RET, so the four stack dwords are caller-cleaned
 * (cdecl for the stack portion). EBX/EBP/ESI/EDI are callee-saved and restored.
 *
 * Name: adopted from the cgame_mp PPC bank (CG_FlamethrowerTrace) on the strength
 * of the flame-cluster call graph; the .mcode size-guess "BG_SetPlayerWeaponForSlot"
 * is REJECTED (no per-slot weapon write — this issues collision-trace traps and
 * copies a 48-byte trace result). Provisional; exact server ABI not fully proven.
 */
void CG_FlamethrowerTrace(int32_t contentMask, const vec3_t maxs, const vec3_t mins, trace_t *out, const vec3_t start, const vec3_t end,
                          int32_t entityNum);


/* cg_flameDamageTrace (the 0x300a85b8 module-static flame-damage-source trace)
 * is declared as trace_t in globals.h. */

/* Flame surface-kind gate (CG_MoveFlameChunk masks cgFlameDamageTrace.flags
 * with FLAME_SURF_KIND_MASK and rejects three specific surface kinds before allowing
 * a new flame-damage source to spawn). Exact SURF_* source names unresolved; named by
 * their masked values. */
#define FLAME_SURF_KIND_MASK 0x01f00000u
#define FLAME_SURF_KIND_1300 0x01300000u
#define FLAME_SURF_KIND_1400 0x01400000u
#define FLAME_SURF_KIND_0C00 0x00c00000u
/* CG_FlamethrowerTrace contentMask used by CG_MoveFlameChunk's damage probe. */
#define FLAME_DAMAGE_TRACE_CONTENTMASK 0x02810011u

/*
 * PM_trace (0x30008280) — pmove collision trace wrapper. Runs the pmove trace
 * callback (pm->trace, +0x104) once; if the trace started solid
 * inside a body brush (results->startsolid && (results->contents &
 * CONTENTS_BODY)), it records that body via PM_AddTouchEnt(results->entityNum)
 * and re-runs the trace with CONTENTS_BODY cleared from both the traceType
 * argument and the shared pmove trace mask (pm->traceMask, +0x34).
 * This is the standard Quake3/CoD PM_trace; name corroborated by the cgame_mp
 * PPC bank and by the exact bg_pmove / trace_t field access. The .mcode-assigned
 * size-guess "BG_GetWeaponIndexForName" is REJECTED: no string/name lookup and
 * no weapon-index math — this is a pmove trace helper.
 *
 * Custom register ABI (proven from the callers, e.g. 0x3000c239 / 0x3000a4f7):
 * EAX = traceType (the trace callback's last argument / content-mask), EBX =
 * results (trace_t *), plus five cdecl stack dwords (start, mins, maxs, end,
 * passEntityNum); callers clean 0x14 bytes after the call, and the function ends
 * in a plain RET. Modeled here as plain C parameters; no calling-convention
 * attribute is added because the syntax-only build does not need one.
 */

/*
 * PM_ClipVelocity (0x30008390) - the standard Quake3/CoD velocity-vs-plane clip:
 *     out = in - overbounce * (in . normal) * normal
 * The sign of the dot selects FMUL vs FDIV of the reflected scale (0x300083ac
 * FCOM 0.0f; TEST AH,0x5 / JP). Register ABI proven at every reconstructed call
 * site (e.g. PM_SlideMove 0x3000e9d8, PM_StepSlideMove): EDX = in (const vec3 *),
 * ECX = normal (const vec3 *), ESI = out (vec3 *); the overbounce scalar is one
 * pushed stack dword. Modeled here as plain cdecl params (register split is an ABI
 * detail; the syntax-only build needs no calling-convention attribute). Reconstructed
 * from 0x30008390's own .mcode in functions/FUN_30008390_300083d8.c. */

/*
 * Footstep / movement predictable-event classification, proven by PM_FootstepEvent
 * (0x3000b950). The surface-type field of a downward footstep trace is
 * (trace.surfaceFlags >> 20) & 0x1f; the emitted footstep event is that surface
 * type (or the default 13 when the trace reaches the far end / has surface type 0)
 * plus the EV_FOOTSTEP_RUN_DEFAULT event base. The footstep trace clears
 * CONTENTS_BODY|the low water-contents bit from the pmove trace mask
 * (~0x02010000). The four "water" paths write existing entityEvent_t values
 * directly into the player-state event ring. Values and masks match the
 * recovered server movement.c exactly; the canonical event names are declared
 * in entity_event_types.h.
 */

/*
 * PM_AddEvent (0x30008310) — append a predictable event to the current pmove
 * player-state event ring: when event != 0, writes
 * pm->ps->events[eventIndex & (MAX_PS_EVENTS-1)] = (uint8_t)event,
 * clears the paired predictableEventParms slot to 0, and increments eventIndex.
 * event arrives in ECX (custom register ABI, plain RET). Name adopted from the
 * server movement.c symbol (server_name_bank.txt: void PM_AddEvent(int event)),
 * whose behavior matches this body. The .mcode size-guess
 * "compare_weaponfile_names" is REJECTED: this is event-ring bookkeeping, not a
 * string compare. Provisional caller-observed decl; superseded by its own .mcode.
 */

/*
 * cg_shakeSource_t — one active camera-shake ("earthquake") source, a 0x24-byte
 * entry in the client's fixed 4-slot shake table at 0x3048b52c (stride 0x24,
 * end 0x3048b5bc). Proven from the shake trio:
 *   - the add path at 0x3001b420 populates a stack copy of this struct (startMsec
 *     from cg_time, amplitude, duration, radius, origin) then REP MOVSD's it into
 *     a free table slot (9 dwords == 0x24 bytes);
 *   - the per-source evaluator at 0x3001b390 (CG_EvaluateCameraShakeSource) reads
 *     startMsec/+0x00, amplitude/+0x04, duration/+0x08, radius/+0x0c, origin/+0x10
 *     and writes scaledAmplitude/+0x1c and timeFalloff/+0x20;
 *   - the aggregate path at 0x3001b550 (CG_CalcViewShake) walks the table, calls the
 *     evaluator, takes the max surviving scaledAmplitude (entry+0x1c) and drives the
 *     shaken view origin/angles from it.
 * Field offsets are all proven by those three functions' machine code. The names
 * radius/amplitude are role-derived (exact original member names not proved).
 */
typedef struct cg_shakeSource_s {
    int32_t startMsec; /* +0x00: cg_time when this shake began */
    float amplitude; /* +0x04: base shake magnitude */
    float duration; /* +0x08: total lifetime in ms */
    float radius; /* +0x0c: spatial falloff radius (distance divisor) */
    vec3_t origin; /* +0x10: world position of the shake source */
    float scaledAmplitude; /* +0x1c: OUT — amplitude scaled by time*distance falloff */
    float timeFalloff; /* +0x20: OUT — the time-only falloff factor */
} cg_shakeSource_t;

/*
 * CG_EvaluateCameraShakeSource (0x3001b390) — evaluate a single active camera
 * shake source for the current frame. Returns qtrue and fills source->{
 * scaledAmplitude, timeFalloff} while the source is still live, else returns
 * qfalse (expired, or elapsed time outside [0, duration)). Register-arg helper:
 * the source pointer arrives in ESI (client thiscall-style ABI). The distance
 * falloff uses VectorDistance(cg_refdef.vieworg, source->origin). The .mcode
 * size-guess "PM_BeginWeaponBreakingdown" is REJECTED — this reads cg_time and
 * the view origin and computes shake falloff, which is cgame camera-shake logic,
 * not a pmove weapon routine.
 */
qboolean CG_EvaluateCameraShakeSource(cg_shakeSource_t *source);

/*
 * cg_shakeSources[4] (0x3048b52c, .data) — the client's fixed 4-slot active
 * camera-shake table (stride 0x24 == sizeof(cg_shakeSource_t), end 0x3048b5bc).
 * Storage in globals.c; supersedes the mechanical g_data_*_viewki_* dword
 * captures at 0x3048b52c/0x3048b548/0x3048b56c/0x3048b590/0x3048b5b4. Written by
 * CG_AddCameraShake (0x3001b420) and consumed by CG_EvaluateCameraShakeSource
 * (0x3001b390) / the aggregate walker (0x3001b550). */
extern cg_shakeSource_t cg_shakeSources[4];

/*
 * CG_AddCameraShake (0x3001b420) — register a new camera-shake ("earthquake")
 * source at a world position. Assigned name script_method_scriptbuiltin_viewkick
 * from the .mcode is REJECTED: this is not a view-kick (no playerState viewangle
 * math); it seeds a cg_shakeSource_t into the 4-slot cg_shakeSources table.
 *
 * Client register/stack ABI (proven at all three call sites, each cleans 0xc of
 * stack => 3 stack args + ECX): the world origin vec3 arrives in ECX; the three
 * stack args are (amplitude float, duration int, radius float). The body FILDs
 * `duration` to float when storing it into the shake source. Early-out when
 * `amplitude <= 0.0f` (FCOMP against 0.0f at .rdata 0x3007bcec).
 *
 * The new source is placed into the first free slot (startMsec > cg_time, i.e.
 * never-used/future, or elapsed >= duration i.e. expired); if all four are live,
 * it replaces the slot with the smallest scaledAmplitude (+0x1c) — but only when
 * that minimum is below the evaluated local scratch threshold. The original
 * discards evaluator failure and can read an unwritten threshold; the recovered
 * source rejects that failure before entering slot selection.
 *
 * `origin` is read-only here (only *origin, origin[1], origin[2] are loaded);
 * kept as vec3_t (decays to float*). Provisional caller-observed ABI; the arg
 * types are re-derived from this function's own machine code. */
void CG_AddCameraShake(const vec3_t origin, float amplitude, int32_t duration, float radius);

/*
 * CG_CalcViewShake (0x3001b550) — apply the aggregate camera shake to the view for
 * the current frame. Walks cg_shakeSources[4] via CG_EvaluateCameraShakeSource, takes
 * the strongest surviving source's scaledAmplitude (also merging cg_shakeExternAmplitude
 * @0x3048b5c0), clamps it to 1.0, then (a) jitters cg_refdef.vieworg by three rand()-based
 * offsets scaled by amplitude*0.8, and (b) advances the effect spin-angle triple
 * (cg_refdefViewAngles[0]/cg_refdefViewAngles[1]/cg_refdefViewAngles[2]) with three
 * sinusoidal sway terms driven by cg_time and the random phase cg_shakeSpinPhase
 * (@0x3048b5bc). On frames with no active shake it only re-rolls cg_shakeSpinPhase.
 *
 * The .mcode size-guess name "Item_Slider_Paint" is REJECTED (real Item_Slider_Paint
 * is 0x30056c80); role-derived from the camera-shake call graph. cdecl, no source args
 * (operates on client globals). */
void CG_CalcViewShake(void);


/* Shared trajectory_types.h supplies the complete trajectory_t embedded by
 * localEntity_t at +0x18 and evaluated by CG_AddMovingTracer. */

/*
 * CG_AdjustPositionForMover (0x3001f5c0) - compensate a world point for the motion
 * of a mover entity between two client times. Given a point `in`, a mover entity
 * number, and a from/to time pair, it evaluates the mover's position trajectory
 * (currentState.pos) at both times and adds the delta to `in`, writing `out`; it
 * also evaluates the angle trajectory (currentState.apos) delta into the optional
 * `angleDelta` vec3 (skipped when NULL). The mover is only compensated when its
 * currentState.eType (cent+0x4) is ET_MOVER(5) or ET_SCRIPTMOVER(8); for any other
 * eType, or a moverNum outside [1, MAX_GENTITIES-2], `out` is just a copy of `in`.
 *
 * Register ABI proven by the three call sites (0x30021e73/0x30035b3c/0x30036014):
 * EAX=moverNum, EDI=in, ESI=out; three cdecl stack dwords fromTime, toTime,
 * angleDelta (RET; caller `add esp,0xc`). angleDelta is zeroed on entry when
 * non-NULL, then overwritten with the apos delta only in the mover branch.
 *
 * The .mcode size-guess name "GetKeyBindingLocalizedString" is REJECTED: the body
 * is pure vec3 trajectory-delta math with no string/key handling. Provenance:
 * Source uo_cgame_mp_x86.dll 0x3001f5c0..0x3001f6ef.
 */
void CG_AdjustPositionForMover(const vec3_t in /* EDI */, int32_t moverNum /* EAX */, int32_t fromTime, int32_t toTime,
                               vec3_t out /* ESI */, vec3_t angleDelta);


/*
 * CG_TrajectoryPointInBounds (0x30005d70) - evaluate an entity's position
 * trajectory at atTime and test whether a reference point lies within an
 * axis-aligned box centred on that position: x,y in [-36,+36], z in [-88,+18].
 * Returns qtrue only when all three axes are inside; qfalse otherwise. Custom
 * REGISTER ABI (proven from the sole caller 0x30035680): ECX = entity, ESI =
 * reference-point base, EAX = atTime (forwarded untouched to BG_EvaluateTrajectory).
 * Plain RET, no stack args. The .mcode size-guess "PM_StartWeaponAnim" is
 * REJECTED: this function starts no animation and touches no weapon state; it is a
 * trajectory bounding-box containment test. Provisional-by-role name (an
 * interaction/pickup proximity gate); exact source name unresolved.
 * Source: uo_cgame_mp_x86.dll 0x30005d70..0x30005df7.
 *
 * NAMING NOTE (from CG_TouchItem 0x30035680, its sole caller): the reference base
 * is cg.predictedPlayerState and the box (x,y +/-36, z -88..+18) is the Q3/CoD
 * item-pickup bounds; the very next gate calls BG_CanItemBeGrabbed. Together these
 * identify this routine as **BG_PlayerTouchesItem(playerState, entityState, time)**.
 * Left under the provisional name here to avoid rewriting the already-accepted
 * 0x30005d70 C artifact; adopt BG_PlayerTouchesItem when that file is revisited.
 */
qboolean CG_TrajectoryPointInBounds(const centity_t *entity, const playerState_t *ps, int32_t atTime);

/*
 * BG_CanItemBeGrabbed (0x30005e00) - decide whether the item entity `es` can be
 * picked up by the player owning `ps`. Shared BG helper (server items.c
 * BG_CanItemBeGrabbed). RECONSTRUCTED from its own machine code in
 * functions/FUN_30005e00_30005f13.c; this decl supersedes the earlier
 * caller-observed placeholder (which typed the item as centity_t and
 * named the flag canTake). Identified by its two diagnostic strings Com_Error'd
 * from the machine code: "BG_CanItemBeGrabbed: index out of range" (0x30072dfc,
 * item modelindex not in [1,134)) and "BG_CanItemBeGrabbed: IT_BAD" (0x30072ddc,
 * default/invalid item type). Reads es->modelindex (+0x8c) to index the item table
 * (bg_itemlist, base 0x300827a0, stride 0x30) and dispatches on the item type
 * (itemType_t); the weapon branch tests ps->weaponBits (+0x534). Returns
 * qtrue/qfalse in AL. Custom register ABI: es(entityState) in ECX, ps in EAX, and
 * one dword `atStreamPos` on the stack (caller-cleaned, plain RET). Source:
 * 0x30005e00..0x30005f13. The .mcode size-guess "BoxDistSqrdExceeds" is rejected.
 */

/*
 * CG_TouchItem (0x30035680) - client-side predicted item pickup. When item
 * prediction is enabled (cg_predictItems_vmCvar.integer), and the predicted player origin is
 * within the pickup box of an item entity, and the item has not already been
 * touched this frame, and BG_CanItemBeGrabbed allows it, hide the item locally
 * (eFlags |= EF_NODRAW) and append an EV_ITEM_PICKUP predictable event (carrying the
 * item modelindex) to cg.predictedPlayerState so the local player sees the pickup a
 * frame early. Register ABI: the centity is passed in EDI. The .mcode size-guess
 * name "AxisCopy" is REJECTED (this copies no matrix; it gates on globals and calls
 * two BG helpers). Source: uo_cgame_mp_x86.dll 0x30035680..0x3003570e.
 */
void CG_TouchItem(centity_t *cent);

/*
 * cgAlignedDrawItem — the larger 2D-draw descriptor consumed by
 * CG_DrawHudElemString (0x30029f70). Unlike the small rectDef_t, this object
 * carries a fixed placement rectangle plus a trailing 4-dword block. The caller at
 * 0x3002a310 builds one on a 0x2044-byte /GS frame: `item` occupies the low 0x40
 * bytes (F+4..F+0x44) and a SEPARATE 0x2000-byte scratch buffer follows it (passed
 * to CG_GetHudElemInfo as arg 3, NOT part of the item). The item is 0x40
 * bytes exactly. Layout is machine-proven; exact source type name unresolved.
 *   +0x00 x          : horizontal draw coordinate, forwarded through the syscall
 *                      as raw float bits; CG_DrawSingleHudElem advances it by
 *                      labelWidth after drawing label
 *   +0x04 y          : vertical draw coordinate
 *   +0x08 width      : complete label/content or label/shader width
 *   +0x0c height     : font or animated shader height
 *   +0x10 label      : translated label string
 *   +0x14 labelWidth : measured label width, added to x before drawing text
 *   +0x18 text       : translated/formatted content string
 *   +0x1c textWidth  : measured content width
 *   +0x20 font      : renderer font selector (0, 4, or 5)
 *   +0x24 fontScale : text scale, forwarded through the syscall as float bits
 *   +0x28 fontHeight: selected font height, also the shader-dimension fallback
 *   +0x2c fontWidth : fixed glyph width (0, 16, or 8); zero selects proportional
 *                     renderer measurement
 *   +0x30 color     : vec4_t RGBA draw color. CG_GetHudElemInfo (0x30029c00)
 *                     FSTPs four floats here; CG_DrawHudElemShader forwards
 *                     &color[0] to trap_R_SetColor and reads color[2] (+0x38) as a
 *                     raw dword. Fills the item to its proven 0x40-byte size.
 */
typedef struct cgAlignedDrawItem {
    union {
        float x; /* +0x00: horizontal draw coordinate */
        int32_t xBits; /* +0x00: syscall/raw-dword view */
    };
    float y; /* +0x04 */
    float width; /* +0x08 */
    float height; /* +0x0c */
    char *label; /* +0x10 translated label string */
    union {
        float labelWidth; /* +0x14 advances x before content */
        int32_t labelWidthBits; /* +0x14 raw-dword clear view */
    };
    char *text; /* +0x18 translated/formatted content */
    union {
        float textWidth; /* +0x1c measured content width */
        int32_t textWidthBits; /* +0x1c raw-dword clear view */
    };
    int32_t font; /* +0x20: renderer font selector */
    union {
        float fontScale; /* +0x24 */
        int32_t fontScaleBits; /* +0x24: syscall view */
    };
    float fontHeight; /* +0x28 */
    union {
        float fontWidth; /* +0x2c */
        int32_t fontWidthBits; /* +0x2c: syscall view */
    };
    /* +0x30: RGBA draw color. CG_GetHudElemInfo (0x30029c00) FSTPs four
     * floats into +0x30/+0x34/+0x38/+0x3c; CG_DrawHudElemShader forwards
     * &color[0] to trap_R_SetColor (a vec4 color arg) and reads color[2] (+0x38)
     * as a raw dword. Fills the item to its proven 0x40-byte frame. */
    vec4_t color; /* +0x30..+0x3f */
} cgAlignedDrawItem;
/* Contains char* fields (label/text), so the 0x40 size only holds at 32-bit
 * pointer width (the target ABI); guard the size assert like the offset ones. */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
_Static_assert(sizeof(cgAlignedDrawItem) == 0x40, "cgAlignedDrawItem is 0x40 bytes");
#endif
/* CG_DrawHudElemString (0x30029f70) uses hudElem_t.alignY (+0x18) to place a
 * prepared 2D draw item before emitting it through CG_R_TEXT_PAINT. */
void CG_DrawHudElemString(cgAlignedDrawItem *item, hudElem_t *elem, const char *string);

/*
 * CG_DrawHudElemShader (0x3002a1d0) — SHADER-type HUD element renderer. Builds a
 * placement from the aligned draw item (ECX) and the hud element (EDI/this),
 * reading elem->materialIndex (+0x38), elem->alignY (+0x18, alignment selector),
 * elem->shaderRightTexcoord (+0x70) and elem->shaderBottomTexcoord (+0x74), and
 * emits the shader via cgame_syscall(0x48). Register ABI proven from CG_DrawSingleHudElem's
 * dispatch (0x3002a3c6): draw item in ECX, hud element in EDI. Provisional
 * caller-observed decl; the callee's own .mcode reconstruction is authoritative.
 * Name anchored by the same-module PPC bank (CG_DrawHudElemShader) and the matching
 * shader-field access. Source: 0x3002a1d0..0x3002a309.
 */
void CG_DrawHudElemShader(cgAlignedDrawItem *item, struct hudElem_s *elem);

/*
 * CG_DrawHudElemClock (0x3002a000) — analog CLOCK / timer HUD element renderer.
 * Looks up the shader config-string for elem->materialIndex (+0x38), registers it
 * as the clock FACE and the same name + literal "Needle" suffix as the rotating
 * HAND, converts CG_GetHudElemTime(elem) into a rotation angle (period from
 * elem->rotationPeriodMs +0x60, or a fixed 60000 ms/rev default), then draws the
 * face via CG_DrawPic and the hand via CG_DrawRotatedPic, bracketed by
 * trap_R_SetColor(item->color). Register ABI proven from CG_DrawSingleHudElem's dispatch
 * (0x3002a3a7): draw item in ECX, hud element in EDX. Name anchored by the
 * same-module PPC bank (CG_DrawHudElemClock) and the "Needle" hand shader. Fully
 * reconstructed; source 0x3002a000..0x3002a1c4.
 */
void CG_DrawHudElemClock(cgAlignedDrawItem *item, struct hudElem_s *elem);

/*
 * CG_GetHudElemInfo (0x30029c00) — populate a cgAlignedDrawItem from a hud
 * element ready for drawing. Register/stack ABI proven from CG_DrawSingleHudElem's call
 * (0x3002a338): item pointer in EAX; three cdecl stack words follow (hud element,
 * a 0x2000-byte scratch string buffer, and its 0x2000 length), caller-cleaned
 * (ADD ESP,0xc). The Mac body shares the four timer/string-formatting callees,
 * resolving the source name. Source: 0x30029c00..0x30029efa.
 */
void CG_GetHudElemInfo(cgAlignedDrawItem *item, struct hudElem_s *elem, char *scratch, int32_t scratchLen);

/*
 * CG_DrawSingleHudElem (0x3002a310) — draw one HUD element. See the C artifact for the
 * full behavior/evidence. `this` (the hud element) arrives in ECX. Source:
 * 0x3002a310..0x3002a3e3.
 */
void CG_DrawSingleHudElem(struct hudElem_s *elem);


/*
 * PerpendicularVector (0x3004a3d0) - id-Tech common_math helper: write into `dst`
 * a unit vector perpendicular to `src`. It picks the minor axis of `src` (smallest
 * absolute component), builds the corresponding cardinal unit vector, projects it
 * onto the plane perpendicular to `src` via ProjectPointOnPlane, then normalizes
 * the result. Caller-observed register ABI (proven from RotateAroundDirection's
 * call site and the callee body; superseded by its own .mcode reconstruction):
 * `src` in EDI, `dst` in EDX; plain `RET`, no stack args. Expressed here with the
 * recovered server signature (game_functions.h:
 * void PerpendicularVector(float *dst, const float *src)). The .mcode's
 * size-matched "script_func_getbrushmodelcenter" guess is REJECTED (matched only
 * by byte size 0x9a==0x9a, which the naming rules forbid); this body is pure vector
 * math with no script/entity access.
 */
void PerpendicularVector(vec3_t dst, const vec3_t src);
void MakeNormalVectors(const vec3_t forward, vec3_t right, vec3_t up);

/*
 * CG_ConsolidateHudElemText (0x30029b70) — expand the single "%s" in a HUD
 * element's label with its content string, writing at most maxlen-1 characters
 * plus a NUL into `out`, then adopt the expanded buffer as the displayed text
 * and clear the label. The original also adds the two width lanes before the
 * caller immediately measures and overwrites them.
 *
 * Custom register ABI: `self` in EDI, `maxlen` in EDX, `out` on the stack
 * (RET with caller cleanup of the one pushed argument). Name from the
 * same-module Mac PEF traceback symbol.
 */
void CG_ConsolidateHudElemText(cgAlignedDrawItem *self, int maxlen, char *out);

/*
 * markPoly_t (0x174 / 372 bytes) — one node of the cgame decal/mark pool
 * cg_markPolys (MAX_MARK_POLYS of these; see globals.h). Marks are transient
 * projected decals (bullet holes, blast scorches) aged out by time. Each node
 * lives on an intrusive circular doubly-linked "active" list threaded through
 * prevMark (+0x00) / nextMark (+0x04), plus a singly-linked free list threaded
 * through nextMark:
 *   - CG_InitMarkPolys (0x3002e400): zero the whole pool, empty the active list
 *     (cg_activeMarkPolys sentinel points at itself), and thread all
 *     MAX_MARK_POLYS nodes onto cg_freeMarkPolys via nextMark.
 *   - CG_AllocMark (0x3002e490): pop a node off cg_freeMarkPolys; if the free
 *     list is empty, reclaim the run of oldest active marks that share the same
 *     markTime (+0x08) as the current oldest, unlinking each and pushing it back
 *     onto the free list, then pop. Zero the node and append it at the newest
 *     end of cg_activeMarkPolys. Returns the node.
 *   - CG_ImpactMark (0x3002e520): the caller that fills the freshly allocated
 *     node's render fields (markTime at +0x08, duration, color, verts at +0x30..).
 *   - CG_AddMarks (0x3002e8c0): walks the active list, fades/expires marks by
 *     time, and re-emits their polys.
 * Only the three fields CG_AllocMark / CG_InitMarkPolys prove are named; the
 * remaining bytes (start time, duration, color, alpha, vertex array at +0x30,
 * numVerts at +0x28, ...) are set by CG_ImpactMark and read by CG_AddMarks and
 * are left as opaque reserved storage until those functions are reconstructed. */
/* markPoly_t is defined canonically in globals.h (included above), because
 * globals.h is parsed first and its cg_markPolys[] / cg_activeMarkPolys object
 * declarations need the complete type. The field commentary and offset guards
 * live there. */

/*
 * CG_InitMarkPolys (0x3002e400) — reset the cgame mark-poly (decal) pool at
 * cgame startup: zero all MAX_MARK_POLYS nodes of cg_markPolys, empty the active
 * list (cg_activeMarkPolys sentinel links point at itself), point cg_freeMarkPolys
 * at cg_markPolys[0], and thread every node onto the free list via nextMark.
 * Called from CG_Init alongside CG_InitLocalEntities. Void in/out. */
void CG_InitMarkPolys(void);

/*
 * CG_AllocMark (0x3002e490) — allocate a mark poly from the free list, reclaiming
 * the oldest active marks if the free list is exhausted. Returns a zeroed node
 * already appended to the newest end of the cg_activeMarkPolys list.
 *
 * The .mcode size-matched guess "Com_ScriptError" is REJECTED: this is the
 * mark-poly pool allocator (free-list pop / oldest-mark reclamation via the
 * cg_freeMarkPolys and cg_activeMarkPolys lists at 0x303b5d20 / 0x30412d40), not
 * a script diagnostic. The "CG_FreeLocalEntity: not active" text passed to
 * Com_ErrorMessage in the reclaim path is a copy-pasted list-integrity assert
 * shared with the local-entity free routine and does not name this pool. */
markPoly_t *CG_AllocMark(void);

/*
 * CG_ImpactMark (0x3002e520) — project a decal (bullet hole, blast scorch) onto
 * world surfaces at `origin` along the surface `dir`, and either draw it as a
 * temporary poly this frame or store it as a persistent mark. Behavior proven from
 * the i386 machine code:
 *   1. Gate on cg_marks_vmCvar.integer (return if 0) and cg_suppressMarksGate
 *      (return if nonzero). Clamp a negative `markLifeTime` to 20000 ms.
 *   2. Normalize `dir`, build a perpendicular axis via PerpendicularVector and
 *      RotatePointAroundVector(perp, dir, axis, orientation), and form a 4-corner
 *      quad of side 2*radius centered on `origin` in that basis.
 *   3. trap_CM_MarkFragments (CG_CM_MARKFRAGMENTS) to clip the quad against world
 *      surfaces into fragments + projected verts.
 *   4. Pack (red,green,blue,alpha)*255 -> Q_rint -> one RGBA dword per vertex.
 *   5. For each fragment (numPoints clamped to 10): set every vert's color; then
 *      if `temporary`, trap_R_AddPolyToScene (CG_R_ADDPOLYTOSCENE) immediately;
 *      else CG_AllocMark and fill the node (markTime=cg_time, markShader, alphaFade,
 *      colors, numVerts, verts[], duration=markLifeTime) for CG_AddMarks to draw.
 * Register ABI: `dir` arrives in ECX; all other parameters are cdecl stack args
 * (the caller unwinds; the huge frame is chkstk-probed). The .mcode size-guess
 * "VEH_UnlinkPlayer" is REJECTED: the body is the cgame decal/mark projector, not
 * a player-unlink routine (that guess matched by byte size only, which the naming
 * rules forbid).
 */
void CG_ImpactMark(qhandle_t markShader, const vec3_t origin, const vec3_t dir, float orientation, float red, float green, float blue,
                   float alpha, qboolean alphaFade, float radius, qboolean temporary, int32_t markLifeTime);

/*
 * rand (0x3005b879) - the statically-linked MSVC CRT pseudo-random generator,
 * returning a value in [0, 32767]. This is C-runtime code, not CoD client
 * source, so it is not reconstructed here. Portable recovered callers use
 * coduo_crt_rand explicitly for that numeric domain. The .mcode size-matched
 * guess "SP_light" is rejected: this is rand(), not an entity spawn function.
 */

/*
 * CG_PickSoundAlias (0x30039f10) - client voice-chat alias picker. Scans the passed
 * cgVoiceChatTable_t for the entry
 * whose name case-insensitively matches `name`, then chooses one of that entry's
 * `variantCount` variants at random (rand()/32768 * variantCount, rounded via
 * Q_rint) and returns the chosen variant's data through the three output
 * pointers. Returns qtrue when a matching alias was found and picked, qfalse
 * otherwise. The .mcode size-matched guess "Menu_Init" is rejected: this reads
 * a 0x1244-strided sound-alias table, calls rand()/Q_rint, and mirrors the
 * engine trap_Com_PickSoundAlias(name, out) role - it is not a menu initializer.
 *
 * ABI note: the alias-table base is passed in EBX (an implicit register
 * parameter set by the caller at 0x3003a292/0x3003a2a2); the four stack
 * arguments are __cdecl (caller cleans 0x10 bytes). */
qboolean CG_PickSoundAlias(cgVoiceChatTable_t *table, const char *name, const char **outSoundName, qhandle_t *outIcon,
                           const char **outText);

/*
 * CG_FindVoiceChatFileIndex (0x30039d80): reconstructed -- see
 * src/client/cgame/sound/cg_findvoicechatfileindex.c. Opens the voice-chat command file
 * named `fileName` (CG_FS_FOPEN_FILE), rejects a missing handle ("voice chat file
 * not found: %s\n") or an oversized file (>= 0x4000, "^1voice chat file too large:
 * %s is %i, max allowed is %i\n"), reads up to 0x4000 bytes into a stack buffer,
 * Com_Parse()s the first token, and returns the index (0..7) of the
 * cg_voiceChatTables[] block whose leading name case-insensitively matches that
 * token, or -1 on any failure.
 *
 * ABI: `fileName` arrives in ECX (fastcall-ish register parameter — the callee's
 * `MOV EDI,ECX` at 0x30039d9f forwards it with no stack argument). The .mcode
 * size-match guess "BG_CalculateWeaponPosition_DamageKick" is rejected: there is no
 * weapon-position math here — it is va/Q_stricmpn/Com_Parse string work over the
 * voice-chat tables. */
int32_t CG_FindVoiceChatFileIndex(const char *fileName);

/*
 * windowDef_t.flags fade bits, proven by Menu_FadeItemByName (0x30051790): the
 * fadeOut branch does `flags = (flags & ~0x40) | 0x24` and the fadeIn branch does
 * `flags = (flags & ~0x20) | 0x44`, i.e. it sets exactly one of bit 0x20 / bit
 * 0x40 (mutually exclusive) together with WINDOW_VISIBLE (0x4). This is the Q3
 * ui_shared.c Menu_FadeItemByName idiom, where 0x20 = WINDOW_FADINGOUT and
 * 0x40 = WINDOW_FADINGIN.
 *
 * The exact UI/cgame Menu_HandleMouseMove twins independently resolve 0x1 as
 * WINDOW_MOUSEOVER and test this 0x20 bit before re-entering a fading-out item.
 */
/* WINDOW_FADINGOUT and WINDOW_FADINGIN are shared UI-window constants. */

/*
 * Menu_ItemsMatchingGroup (0x30051180) counts the items in `menu` whose
 * name/group matches `name`. The compiler passes menu in EAX and name in EBX;
 * the shared semantic interface remains (menu, name).
 */
/* Menu matching declarations are shared in ui_runtime.h. */

/*
 * CG_CalcAdsOverlayFrac (0x30019520) - compute the ADS scope/overlay display
 * fraction for the current predicted weapon into *outFrac and return whether the
 * overlay is active this frame (fraction > 0.01). RECONSTRUCTED; see
 * functions/FUN_30019520_300195da.c. Register ABI: outFrac pointer in ECX (modeled
 * as a normal parameter); AL/EAX -> qboolean.
 */
qboolean CG_CalcAdsOverlayFrac(float *outFrac);

/* CG_DrawWeapReticle (0x300195e0) — draw the active ADS overlay/reticle and
 * return the complementary ordinary-crosshair alpha. */
long double CG_DrawWeapReticle(void);

/*
 * CG_PredictPlayerState_Internal (0x30035800) - replay the buffered usercmd window
 * through Pmove, update prediction error/mover compensation, and transition the
 * resulting player state. Name is matched by the same-module PPC bank (0x860-byte
 * body adjacent to CG_PredictPlayerState and CG_EntityType).
 */
void CG_PredictPlayerState_Internal(void);

void CG_InterpolatePlayerState(qboolean grabAngles);

/* CG_EntityType (0x300357d0): pmove callback returning the entity type for an
 * entity number. Same-module PPC name and exact 0x28-byte adjacency match. */
int32_t CG_EntityType(int32_t entityNum);

/*
 * CG_PredictPlayerState (0x30036070) - reconstructed; see
 * functions/FUN_30036070_300361c7.c. Refreshes the tracked camera/view entity slot
 * (cg_entities[cg_adsViewErrorEntityNum]) angle block from the requested view origin
 * and its evaluated angle trajectory, then, while the player is scoped (cg_snap
 * serverFlags bit 0x80000) and the ADS overlay is active, advances the two ADS
 * view-error (idle aim-wander) angle accumulators by a random-magnitude,
 * random-phase step. The mechanical size-guess name "ScriptEnt_MoveAxis" (a
 * server GSC entity mover) is rejected. No args, void.
 */
void CG_PredictPlayerState(void);

/* ===========================================================================
 * CG_Player (0x300343e0) callee ABI records — provisional, caller-observed only,
 * superseded by each callee's own .mcode reconstruction. Register-argument client
 * ABI; arity/types re-derived from the CG_Player call sites and UNPROVEN beyond what
 * those sites show.
 * =========================================================================== */

/* corpseModelInfo.eFlags (+0x08) is the shared entity eFlags word.
 * CG_Player proves tests of EF_DEAD, EF_NODRAW, EF_FORCED_STANCE_MASK,
 * EF_FIRING, and EF_IN_VEHICLE at this boundary. */

/*  - CG_AddPlayerMountedModel (0x30033b70): PUSH cent; CALL; ADD ESP,4 (one __cdecl
 *    stack arg). Runs when EF_FORCED_STANCE_MASK is set; reads cent+0x74 and
 *    indexes bgs.clientinfo. arity/types UNPROVEN beyond the single cent arg.
 *  - CG_AddPlayerViewWeapon (0x30032fe0): EBX=cent (register arg; MOV EBX,ESI at
 *    0x30034493), no stack args; returns qboolean in EAX (nonzero => handled, skip the
 *    normal model submit). Reads cent+0x74/+0x88/+0x94. arity/types UNPROVEN.
 *  - CG_AddPlayerWaterShadow (0x30032da0, prior size-match guess CG_AddPlayerHeatEffect):
 *    ESI=cent (register arg), no stack args, result unused. Now fully reconstructed in
 *    src/client/cgame/entities/cg_addplayerwatershadow.c — when the shadow-mode global
 *    (0x3044fdec) is set it point-contents-probes 24u below (must be CONTENTS_WATER) and
 *    32u above (must be air) the entity origin, traces the water surface, and submits a
 *    64x64 white markShadow(fade) quad on that surface via trap_R_AddPolyToScene. The
 *    provisional name "HeatEffect" is REJECTED (no heat/matrix math; it draws a water-line
 *    decal). Reads cent+0x208 (lerpOrigin). */
/*  - CG_PlayerShadow (0x30032c20, prior caller-observed provisional name
 *    CG_AddPlayerModelEffect): EAX=shadowPlane (float* out), ESI=cent (register
 *    args); returns qboolean. Now fully reconstructed in
 *    src/client/cgame/entities/cg_playershadow.c — it traces straight down from
 *    the entity origin and, in blob-shadow mode, projects a markShadow decal faded
 *    by camera distance, writing the ground shadow plane Z to *shadowPlane. It
 *    writes ONLY *shadowPlane (the scratch's +0x00 float); it does NOT write the
 *    scratch +0x04 entityNum the caller reads back at 0x30034614 (that field's
 *    writer is elsewhere / still unresolved for the CG_Player caller). */
void CG_AddPlayerMountedModel(centity_t *cent);
qboolean CG_AddPlayerViewWeapon(centity_t *cent);
void CG_AddPlayerWaterShadow(centity_t *cent);
/* CG_Player's per-model-effect scratch: +0x00 doubles as the CG_PlayerShadow out
 * float (shadowPlane) and +0x04 as an entityNum the caller compares to
 * cg_snap->ps.psClientNum. CG_PlayerShadow writes only +0x00. */
/* (A former cgPlayerModelEffectScratch_t two-field struct here was a mis-model:
 * the +0x04 slot next to CG_Player's shadowPlane local is that function's own
 * spilled animStateIndex, written at 0x30034417 — not a struct field.) */
qboolean CG_PlayerShadow(float *shadowPlane, centity_t *cent);

/* CG_AddPlayerWeapon (0x30045ca0) — add one held/view weapon to a player render
 * entity. The register argument is the parent refEntity placement; stack args are
 * the optional playerState (NULL for a world player, non-NULL for the local view
 * weapon), owning centity, view-weapon flag, and the view-origin pullback scalar.
 * This supersedes the stale CG_AttachDObjParts role name: the body registers the
 * centity weapon, builds its DObj/refEntity, handles flash/flame effects, and submits
 * it. The two CG_Player callers pass (ps=NULL, cent, qtrue, 0.0f); CG_AddViewWeapon
 * passes the predicted ps/entity and its -19.0f mounted pullback when active. */
void CG_AddPlayerWeapon(refEntity_t *parent, playerState_t *ps, centity_t *cent, qboolean viewWeapon, float viewOriginOffset);
void CG_WeaponUpdateLoopingSound(centity_t *cent);

/* CG_EmitPlayerFlameChunks (0x30024050) — emits flamethrower flame chunks for a player
 * whose held weapon is WEAPTYPE_GAS. NOT the per-chunk processor at 0x300272b0
 * (which owns the CG_AddFlameChunks name); the globals.h note attributing that name to
 * 0x300240a3 concerns THIS function's interior and is not adopted as its name. CG_Player
 * call site (0x300346a0): EAX register arg + five __cdecl stack dwords:
 * CG_EmitPlayerFlameChunks(viewAngles(EAX), cent, flashOrigin, spread=1.8f, a3=1, a4=0).
 * `viewAngles` (EAX) is the emitter's pitch/yaw/roll triple — the CG_Player caller
 * seeds it from animState->viewPitch/viewYaw/viewRoll and the body does
 * AngleNormalize180 math on its components; `flashOrigin` is the tag_flash
 * bone WORLD POSITION (CG_Player passes &tagFlashMatrix[12], the matrix translation
 * row; the body uses it as the emit-origin base). An earlier decl had these two
 * roles swapped (originDir/viewAngles) from a call-site ESP misread. Indexes
 * cg_flameInfo (0x300ab750, stride 0xb8) by cent[0] (the emitter owner clientNum).
 * Partial reconstruction in progress at
 * src/client/cgame/effects/cg_emitplayerflamechunks.c (region-split; loop body deferred). */
void CG_EmitPlayerFlameChunks(vec3_t viewAngles, centity_t *cent, vec3_t flashOrigin, float spread, int32_t a3, int32_t a4);


#endif /* CLIENT_RECOVERED_H */
