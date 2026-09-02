// Source: uo_cgame_mp_x86.dll 0x30001e90..0x30002466
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30001e90_30002466.mcode
//
// BG_ParseCommands(char **text, bg_anim_script_t *script,
//                  bg_static_animation_t *animations)
//
// Parse the animation-script command list that follows a script entry's condition
// header. It is one master loop that keeps reading commands until the token stream
// is exhausted or a "}" closes the block. Each command names up to two animations
// (a torso slot and a legs slot, selected by bodyPartSlot 0/1); each slot resolves a
// body-part keyword and an animation name, then accepts the inline parameters
// "duration N", "turretanim", and "blendtime N". A trailing pass reads whole-command
// parameters, currently just "sound <alias>". The parsed data is written into the
// next script->commands[] slot, and — when running in-game rather than while loading
// an anim tree (g_data_g_testentityposition_300a7820 == NULL) — usage flags and
// durations are stamped onto the referenced bg_static_animation_t entries.
//
// Return value: the machine code has a single epilogue (0x30002461) reached only
// from the "empty token" / "}" cases at the top of the master loop; EAX is not set
// there, so the function returns garbage and the sole caller
// (BG_AnimParseAnimScriptCommands 0x30002470) ignores it (CALL; ADD ESP,0xc; JMP).
// Modeled as void.
//
// NAME ADJUDICATION: the .mcode header's mechanical name "CG_DrawFPS" is a pure
// size-match guess (win 0x5d6 ~ corpus 0x5d0) and is REJECTED — no FPS/HUD drawing
// exists here. Identity is proven by this body's own diagnostics, e.g.
// "BG_ParseCommands: exceeded maximum number of animations (%i)" (0x30072724),
// "BG_ParseCommands: expected animation" (0x300726fc), and the keywords
// "duration"/"turretanim"/"blendtime"/"sound" it Q_stricmpn's against. Corroborated
// by the server animation.c BG_ParseCommands. The two mechanical globals whose
// exporter owner was "cg_drawfps" (0x3008bf34/0x3008c4b8) were the first-toucher
// artifact of THIS (misnamed) function and are resolved to
// bgAnimParseCurrentEvent / bgAnimParseCurrentAnimGroup.
//
// ---------------------------------------------------------------------------
// Argument shape (proven at the call site 0x30002b1b, which pushes 3 dwords and
// ADD ESP,0xc):
//   text       = char **               [ESP+0x14]  the Com parser text cursor
//   script     = bg_anim_script_t *    [ESP+0x18]  destination script (+0x88 count)
//   animations = bg_static_animation_t *[ESP+0x1c] static animation-table entries base
// ([ESP+0x1c] and [ESP+0x20] read the SAME arg: the +0x20 read at 0x300022a3 occurs
// with one extra PUSH ESI live on the stack.)
//
// EBP = the body-part slot (0 = first anim on the line, 1 = second). EBX = the
// &script->commands[commandCount] cursor, established on the slot-0 pass.
//
// Token-fetch inlining: the machine code inlines the unget fast-path (restore
// com_parseSession->savedParse/savedLine, clear ungetToken) then CALL 0x3004d6b0
// (Com_Parse) with allowLineBreaks = (EBP==0 ? 1 : 0). Source equivalents are
// Com_Parse(text) for a fresh command's leading token and
// Com_ParseOnLine(text) for same-line tokens; the restore blocks are the compiler
// inlining those unget-aware wrappers around Com_Parse.
//
// Q_stricmpn ABI at every compare site: EAX=99999 (unbounded => Q_stricmp),
// EDX=token, ECX=keyword; zero result == match.
//
// bg_anim_script_command_t (0x10): bodyPart[2] +0x00, animIndex[2] +0x04,
//   duration[2] +0x08, soundAliasName +0x0c (per-slot writes use [EBX+EBP*2]).
// bg_static_animation_t (0x5c): blendTime +0x40, moveSpeed +0x44, duration +0x48,
//   flags +0x50, stateFlags +0x54 (indexed animations[idx]).

#include "bg_animation.h"
#include "bg_animation_services.h"
#if defined(WINDOWS_BEHAVIOR)
#include "compat/crt/msvc_compat.h"
#elif defined(LINUX_BEHAVIOR)
#include <stdlib.h>
#else
#error "BG_ParseCommands requires a target behavior"
#endif
#include "qcommon/com_parse.h"
#include "qcommon/q_string.h"

/*
 * A script command may reference at most this many animation slots before
 * "exceeded maximum number of animations" is reported (0x30001f4d compares
 * script->commandCount against 8). It is a soft warning: the count keeps growing.
 */
/* Fixed blendTime (ms) stamped on an event-type-2 entry (0x300020ef stores 0x1e). */
enum { ANIM_EVENT2_BLENDTIME = 30 };

/* BG_ANIM_ENTRY_TURRET — bg_static_animation_t.flags (+0x50) bit set when a
 * "turretanim" parameter is applied (0x30002235 OR ...,4). Promoted to
 * client_recovered.h (second consumer: CG_PlayerVehiclePositionAndBlend 0x30032fe0). */

void BG_ParseCommands(char **text, bg_anim_script_t *script,
                      bg_static_animation_t *animations)
{
    bg_anim_script_command_t *command = (bg_anim_script_command_t *)0;
    int16_t animIndex = 0;      /* command.animIndex[slot] of the current slot */
    int32_t bodyPartSlot = 0;   /* EBP */

    for (;;) {
        /* 0x30001ea0/0x30001f11: fresh command allows line breaks; a continued slot
         * stays on the current line. */
        char *token = (bodyPartSlot == 0) ? Com_Parse(text)
                                          : Com_ParseOnLine(text);

        /* 0x30001f1b/0x30001f23: empty / end-of-text ends the whole command block. */
        if (token == (char *)0 || token[0] == '\0') {
            return;
        }

        /* 0x30001f2c: a bare "}" closes the block; unget it and stop (0x3000244f). */
        if (Q_stricmpn(token, "}", 99999) == 0) {
            size_t tokenLength = 0;
            while (token[tokenLength] != '\0') {
                tokenLength++;
            }
            *text -= tokenLength;
            return;
        }

        /* Slot 0 allocates the next command and bumps the count, warning past the
         * soft maximum (0x30001f45..0x30001f7e). */
        if (bodyPartSlot == 0) {
        if (script->commandCount >= BG_ANIM_MAX_SCRIPT_COMMANDS) {
                BG_AnimParseError("BG_ParseCommands: exceeded maximum number of "
                          "animations (%i)", BG_ANIM_MAX_SCRIPT_COMMANDS);
            }
            {
                int32_t commandIndex = script->commandCount;
                command = &script->commands[commandIndex];
                script->commandCount =
                    coduo_int32_from_bits((uint32_t)commandIndex + 1u);
            }
            /* 0x30001f7e MOV [EBX],0 zeroes the dword at command+0 = both bodyPart
             * words (bodyPart[0] +0x00, bodyPart[1] +0x02). soundAliasName (+0x0c) is
             * untouched at allocation and only written later at 0x3000242e; a prior
             * pass cleared soundAliasName here instead of the bodyPart slots. */
            command->bodyPart[0] = 0;
            command->bodyPart[1] = 0;
        }

        /* 0x30001f84: resolve the body-part keyword to an index in the
         * animBodyPartsStr table, stored (word) in bodyPart[slot]. */
        int16_t animPartIndex =
            (int16_t)BG_IndexForString(token, animBodyPartsStr,
                                       qtrue);
        command->bodyPart[bodyPartSlot] = animPartIndex;

        /* 0x30001f9c: a non-positive index means this token was not a body part —
         * unget it and fall straight into the trailing sound loop (0x30002309). */
        if (animPartIndex <= 0) {
            /* fall through to the trailing sound loop below. */
            size_t tokenLength = 0;
            while (token[tokenLength] != '\0') {
                tokenLength++;
            }
            *text -= tokenLength;
        } else {
            /* 0x30001fa2: read and resolve the animation name for this slot. */
            char *animToken = Com_ParseOnLine(text);
            if (animToken == (char *)0 || animToken[0] == '\0') {
                BG_AnimParseError("BG_ParseCommands: expected animation");
            }
            animIndex = (int16_t)BG_AnimationIndexForString(animToken);
            command->animIndex[bodyPartSlot] = animIndex;
            /* 0x30002017: default the slot duration to the entry's base duration
             * (low word of animations[animIndex].duration at +0x48). */
            command->duration[bodyPartSlot] = (int16_t)animations[animIndex].duration;

            /* In-game marking pass (0x30002020): only when NOT loading an anim tree. */
            if (bgRuntimeAnimations == NULL) {
                bg_anim_move_type_t animGroup = bgAnimParseCurrentAnimGroup;

                /* Skip when there is no channel or this slot is torso-only
                 * (0x30002032 / 0x3000203a). */
                if (animGroup != ANIM_MT_UNUSED &&
                    command->bodyPart[bodyPartSlot] != ANIM_BP_TORSO) {
                    /* 0x30002045: mark this channel bit on the ANIMATION entry.
                     * The OR at 0x3000204c uses EAX = &animations[animIndex] set at
                     * 0x30002014 (LEA EAX,[ECX+EDX], ECX = animIndex*0x5c) and never
                     * recomputed; the sibling moveSpeed test below likewise indexes by
                     * animIndex. A prior pass indexed by animPartIndex. */
                    animations[animIndex].stateFlags |=
                        (uint32_t)(1u << ((uint32_t)animGroup & 31u));

                    /* 0x3000204f: climb-up / climb-down additionally flag the
                     * animated entry as moving when it has a nonzero moveSpeed. */
                    if (animGroup == ANIM_MT_CLIMBUP ||
                        animGroup == ANIM_MT_CLIMBDOWN) {
                        bg_static_animation_t *ae = &animations[animIndex];
                        if (ae->moveSpeed != 0) {
                            ae->flags |= BG_ANIM_ENTRY_VERTICAL_MOTION;
                        }
                    }

                    /* 0x3000206f: consult the first strafing (type 10) condition in
                     * the owning script; its value 1/2 sets the matching flag bit. */
                    for (int32_t ci = 0; ci < script->conditionCount;
                         ci = coduo_int32_from_bits((uint32_t)ci + 1u)) {
                        if (script->conditions[ci].type !=
                            ANIM_COND_STRAFING) {
                            continue;
                        }
                        int32_t moveValue =
                            script->conditions[ci].value[0];
                        if (moveValue == 1) {
                            animations[animIndex].flags |= BG_ANIM_ENTRY_STRAFE_LEFT;
                        } else if (moveValue == 2) {
                            animations[animIndex].flags |= BG_ANIM_ENTRY_STRAFE_RIGHT;
                        }
                        break;
                    }
                }

                /* 0x300020c8: event-type marking, independent of the body-part gate. */
                bg_anim_event_t eventType = bgAnimParseCurrentEvent;
                if (eventType == 2) {
                    animations[animIndex].flags |= BG_ANIM_ENTRY_FIRE_WEAPON;
                    animations[animIndex].blendTime = ANIM_EVENT2_BLENDTIME;
                } else if (eventType == 1) {
                    animations[animIndex].moveSpeed = 0;
                    animations[animIndex].flags |= BG_ANIM_ENTRY_DEATH;
                }
            }

            /* --------------------------------------------------------------
             * Inline-parameter loop (0x30002119): duration / turretanim /
             * blendtime, until an unknown token (ungotten) or end of line.
             * -------------------------------------------------------------- */
            for (;;) {
                char *param = Com_ParseOnLine(text);

                /* 0x300022c3: end of line — push the empty on-line result back
                 * before entering the trailing whole-command parameter loop.
                 * The inlined original performs all of Com_UngetToken's effects:
                 * it checks ungetToken, copies com_lastTokenStart to
                 * com_tokenStart, and sets ungetToken. */
                if (param == (char *)0 || param[0] == '\0') {
                    Com_UngetToken();
                    break;
                }

                /* 0x30002182: "duration <int>" */
                if (Q_stricmpn(param, "duration", 99999) == 0) {
                    char *value = Com_ParseOnLine(text);
                    if (value == (char *)0 || value[0] == '\0') {
                        BG_AnimParseError("BG_ParseCommands: expected duration value");
                    }
                    /* 0x300021fd: store Q_atoi(value) as the slot duration word. */
#if defined(WINDOWS_BEHAVIOR)
                    command->duration[bodyPartSlot] =
                        (int16_t)coduo_crt_atoi(value);
#else
                    command->duration[bodyPartSlot] =
                        (int16_t)atoi(value);
#endif
                    continue;
                }

                /* 0x30002207: "turretanim" */
                if (Q_stricmpn(param, "turretanim", 99999) == 0) {
                    if (bgRuntimeAnimations == NULL) {
                        /* 0x30002235: flag the animated entry as a turret anim. */
                        animations[animIndex].flags |= BG_ANIM_ENTRY_TURRET;
                    }
                    /* 0x30002238: only valid on the "both" body part (sentinel 3). */
                    if (command->bodyPart[bodyPartSlot] != ANIM_BP_BOTH) {
                        BG_AnimParseError("BG_ParseCommands: Turret animations can only be "
                                  "played on the 'both' body part");
                    }
                    continue;
                }

                /* 0x30002255: "blendtime <int>" */
                if (Q_stricmpn(param, "blendtime", 99999) == 0) {
                    char *value = Com_ParseOnLine(text);
                    if (value == (char *)0 || value[0] == '\0') {
                        BG_AnimParseError("BG_ParseCommands: expected blendtime value");
                    }
                    /* 0x3000228b: only stamped in-game (else the value is dropped). */
                    if (bgRuntimeAnimations == NULL) {
#if defined(WINDOWS_BEHAVIOR)
                        animations[animIndex].blendTime =
                            coduo_crt_atoi(value);
#else
                        animations[animIndex].blendTime = atoi(value);
#endif
                    }
                    continue;
                }

                /* 0x300022b6: anything else — unget it so the trailing sound loop can
                 * see it, then leave the inline-parameter loop. */
                Com_UngetToken();
                break;
            }

            /* 0x300022f4: after the parameter loop, advance to the next slot unless
             * this slot was the terminator sentinel (3) or slot 1 is done. */
            if (command->bodyPart[bodyPartSlot] != ANIM_BP_BOTH &&
                bodyPartSlot < 1) {
                bodyPartSlot = coduo_int32_from_bits((uint32_t)bodyPartSlot + 1u);
                continue; /* 0x30002301: back to the master loop for slot 1 */
            }
            /* otherwise fall into the trailing sound loop (0x30002325). */
        }

        /* ------------------------------------------------------------------
         * Trailing whole-command parameter loop (0x30002325): reads tokens with
         * the unget-aware on-line parser and handles "sound <alias>". An empty token
         * restarts the master loop at slot 0 (0x30001e9c XOR EBP,EBP).
         * ------------------------------------------------------------------ */
        for (;;) {
            char *param = Com_ParseOnLine(text);
            if (param == (char *)0 || param[0] == '\0') {
                break; /* 0x3000237c/0x30002385: restart master loop with slot 0 */
            }

            /* 0x3000238b: "sound <alias>" */
            if (Q_stricmpn(param, "sound", 99999) == 0) {
                char *sound = Com_ParseOnLine(text);
                if (sound == (char *)0 || sound[0] == '\0') {
                    BG_AnimParseError("BG_ParseCommands: expected sound");
                }
                /* 0x30002400: reject raw .wav references (sound scripts only). */
                if (strstr(sound, ".wav") != (char *)0) {
                    BG_AnimParseError("BG_ParseCommands: wav files not supported, only "
                              "sound scripts");
                }
                /* 0x3000242e: store the registered alias handle. */
                {
                    /* 0x30002419: load the import block after the extension
                     * check and any parse-error callback. */
                    const bgs_t *arena =
                        (const bgs_t *)(const void *)bgAnimStaticTable;
                    command->soundAliasName = arena->soundAliasCallback(sound);
                }
                continue;
            }

            /* 0x30002436: unknown whole-command parameter. */
            BG_AnimParseError("BG_ParseCommands: unknown parameter '%s'", param);
        }

        /* Master loop restarts at slot 0 (0x30001e9c). */
        bodyPartSlot = 0;
    }
}
