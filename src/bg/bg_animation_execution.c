#include "bg_animation.h"
#include "bg_animation_services.h"
#include "compat/coduo_int32_bits.h"
#include "compat/crt/random_compat.h"

#include <stddef.h>
#include <stdint.h>

enum {
    BG_ANIM_TIMER_PAD_MS = 50,
    BG_ANIM_RESTART_MIN_TIMER_MS = 50
};

/*
 * The authoritative Windows cgame/game bodies are instruction-identical at
 * 0x30002f90/0x20002f70. Linux game BG_PlayAnim at RVA 0x0001bfb9 retains the
 * same timer gates, animation-toggle operation, and legs-only success result.
 */
int32_t BG_PlayAnim(playerState_t *player, uint32_t animationIndex,
                    uint32_t bodyPart, int32_t duration, qboolean setTimer,
                    qboolean restartSame, qboolean force)
{
    qboolean legsStarted = qfalse;

    if (duration == 0) {
        duration = coduo_int32_from_bits(
            (uint32_t)bgAnimStaticTable->entries[animationIndex].duration +
            BG_ANIM_TIMER_PAD_MS);
    }

    if (bodyPart == ANIM_BP_LEGS || bodyPart == ANIM_BP_BOTH) {
        if (player->legsTimer < BG_ANIM_RESTART_MIN_TIMER_MS || force) {
            const uint32_t legsAnim = (uint32_t)player->legsAnim;

            if (!restartSame ||
                (legsAnim & ~ANIM_TOGGLEBIT) != animationIndex) {
                player->legsAnim = (int32_t)(
                    ((legsAnim & ANIM_TOGGLEBIT) ^ ANIM_TOGGLEBIT) |
                    animationIndex);
                legsStarted = qtrue;
                if (setTimer) {
                    player->legsTimer = duration;
                }
            } else if (setTimer &&
                       (bgAnimStaticTable->entries[animationIndex].flags &
                        BG_ANIM_ENTRY_LOOPED) != 0) {
                player->legsTimer = duration;
            }
        }

        if (bodyPart == ANIM_BP_BOTH) {
            animationIndex = 0;
        }
    }

    if (bodyPart == ANIM_BP_TORSO || bodyPart == ANIM_BP_BOTH) {
        if (player->torsoTimer < BG_ANIM_RESTART_MIN_TIMER_MS || force) {
            const uint32_t torsoAnim = (uint32_t)player->torsoAnim;

            if (!restartSame ||
                (torsoAnim & ~ANIM_TOGGLEBIT) != animationIndex) {
                player->torsoAnim = (int32_t)(
                    ((torsoAnim & ANIM_TOGGLEBIT) ^ ANIM_TOGGLEBIT) |
                    animationIndex);
                if (setTimer) {
                    player->torsoTimer = duration;
                }
            } else if (setTimer &&
                       (bgAnimStaticTable->entries[animationIndex].flags &
                        BG_ANIM_ENTRY_LOOPED) != 0) {
                player->torsoTimer = duration;
            }
        }
    }

    return legsStarted ? duration : -1;
}

/* Windows cgame/game 0x30003080/0x20003060; Linux game RVA 0x0001c151. */
int32_t BG_PlayAnimName(playerState_t *player, const char *animationName,
                        uint32_t bodyPart, qboolean setTimer,
                        qboolean restartSame, qboolean force)
{
    return BG_PlayAnim(player,
                       (uint32_t)BG_AnimationIndexForString(animationName),
                       bodyPart, 0, setTimer, restartSame, force);
}

/*
 * Windows retains BG_AnimScriptAnimation for this command runner at
 * 0x300030a0/0x20003080. Linux emits the same operation graph at RVA
 * 0x0001c1a9 under the swapped symbol BG_ExecuteCommand.
 */
int32_t BG_AnimScriptAnimation(const bg_anim_script_command_t *command,
                               playerState_t *player, qboolean setTimer,
                               qboolean restartSame, qboolean force)
{
    int32_t duration = -1;
    qboolean legsStarted = qfalse;

    if (command->bodyPart[0] != ANIM_BP_UNUSED) {
        int32_t result;

        duration = (int32_t)command->duration[0] + BG_ANIM_TIMER_PAD_MS;
        result = BG_PlayAnim(player, (uint32_t)(int32_t)command->animIndex[0],
                             (uint32_t)(int32_t)command->bodyPart[0], duration,
                             setTimer, restartSame, force);
        if (command->bodyPart[0] == ANIM_BP_LEGS ||
            command->bodyPart[0] == ANIM_BP_BOTH) {
            legsStarted = result >= 0;
        }
    }

    if (command->bodyPart[1] != ANIM_BP_UNUSED) {
        int32_t result;

        duration = (int32_t)command->duration[0] + BG_ANIM_TIMER_PAD_MS;
        result = BG_PlayAnim(player, (uint32_t)(int32_t)command->animIndex[1],
                             (uint32_t)(int32_t)command->bodyPart[1], duration,
                             setTimer, restartSame, force);
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (command->bodyPart[0] == ANIM_BP_LEGS ||
            command->bodyPart[0] == ANIM_BP_BOTH) {
            legsStarted = result >= 0;
        }
    }

    if (command->soundAliasName != NULL) {
        bgs_t *animationState = (bgs_t *)(void *)bgAnimStaticTable;
        animationState->soundEventCallback(player->psClientNum,
                                           command->soundAliasName);
    }

    return legsStarted ? duration : -1;
}

/*
 * Windows retains BG_ExecuteCommand for this selector at
 * 0x300031d0/0x200031b0. Linux emits the same operation graph at RVA
 * 0x0001c377 under the swapped symbol BG_AnimScriptAnimation.
 */
int32_t BG_ExecuteCommand(playerState_t *player, int32_t stateIndex,
                          int32_t animGroup, qboolean restartSame)
{
    bg_anim_script_t *script = NULL;
    const int32_t clientNum = player->psClientNum;

    if (player->pmType >= PM_TYPE_DEAD) {
        return -1;
    }

    while (script == NULL && stateIndex >= 0) {
        bg_anim_script_list_t *scriptList =
            &bgAnimStaticTable->animations[stateIndex][animGroup];

        if (scriptList->count != 0) {
            script = BG_FirstValidItem(clientNum, scriptList);
        }
        if (script == NULL) {
            stateIndex = coduo_int32_from_bits((uint32_t)stateIndex - 1u);
        }
    }

    if (script == NULL || script->commandCount == 0) {
        return -1;
    }

    BG_UpdateConditionValue(clientNum, ANIM_COND_MOVETYPE, animGroup, qtrue);
    return BG_AnimScriptAnimation(
               &script->commands[clientNum % script->commandCount], player,
               qfalse, restartSame, qfalse) != -1;
}

/*
 * NOT_FROM_ORIGINAL_SOURCE: the original event lists occupy one contiguous
 * sixteen-list region split across two named source arrays. This typed helper
 * preserves that valid event-domain mapping without indexing past either C
 * array.
 */
static bg_anim_script_list_t *bg_compat_anim_event_list(
    bg_static_animation_table_t *table, bg_anim_event_t event)
{
    if (event < ANIM_EVENT_RELOAD) {
        return &table->events[event];
    }
    return &table->scriptLists[event - ANIM_EVENT_RELOAD];
}

/* Windows cgame/game 0x30003360/0x20003340; Linux game RVA 0x0001c4e3. */
int32_t BG_AnimScriptStateChange(playerState_t *player, int32_t fromState,
                                 int32_t toState)
{
    bg_anim_script_list_t *scriptList;
    bg_anim_script_t *script;

    if (player->pmType >= PM_TYPE_DEAD) {
        return -1;
    }

    scriptList = &bgAnimStaticTable->statechanges[toState][fromState];
    if (scriptList->count == 0) {
        return -1;
    }

    script = BG_FirstValidItem(player->psClientNum, scriptList);
    if (script == NULL || script->commandCount == 0) {
        return -1;
    }

    return BG_AnimScriptAnimation(
        &script->commands[coduo_server_randrange(0, script->commandCount)],
        player,
        qtrue, qfalse, qfalse);
}

/* Windows cgame/game 0x300033e0/0x200033c0; Linux game RVA 0x0001c5e3. */
int32_t BG_AnimScriptEvent(playerState_t *player, bg_anim_event_t event,
                           qboolean restartSame, qboolean force)
{
    bg_anim_script_list_t *scriptList;
    bg_anim_script_t *script;

    if (event != ANIM_EVENT_DEATH &&
        player->pmType >= PM_TYPE_DEAD) {
        return -1;
    }

    scriptList = bg_compat_anim_event_list(bgAnimStaticTable, event);
    if (scriptList->count == 0) {
        return -1;
    }

    script = BG_FirstValidItem(player->psClientNum, scriptList);
    if (script == NULL || script->commandCount == 0) {
        return -1;
    }

    return BG_AnimScriptAnimation(
        &script->commands[coduo_server_randrange(0, script->commandCount)],
        player,
        qtrue, restartSame, force);
}

/*
 * Windows cgame/game 0x30003460/0x20003440 and Linux game RVA 0x0001c6d5.
 * Linux's unused leading client number is absent from both Windows interfaces
 * and all maintained callers, so the common source exposes the live index only.
 */
char *BG_GetAnimString(uint32_t animationIndex)
{
    if (animationIndex >= (uint32_t)bgAnimStaticTable->entryCount) {
        BG_AnimParseError("BG_GetAnimString: anim index is out of range");
    }
    return bgAnimStaticTable->entries[animationIndex].name;
}

/* Windows cgame/game 0x30003550/0x20003530; Linux game RVA 0x0001c85a. */
int32_t BG_GetAnimScriptEvent(playerState_t *player, bg_anim_event_t event)
{
    bg_anim_script_list_t *scriptList;
    bg_anim_script_t *script;

    if (event != ANIM_EVENT_DEATH &&
        player->pmType >= PM_TYPE_DEAD) {
        return -1;
    }

    scriptList = bg_compat_anim_event_list(bgAnimStaticTable, event);
    if (scriptList->count == 0) {
        return -1;
    }

    script = BG_FirstValidItem(player->psClientNum, scriptList);
    if (script == NULL || script->commandCount == 0) {
        return -1;
    }

    return script
        ->commands[coduo_server_randrange(0, script->commandCount)]
        .animIndex[0];
}

/* Windows cgame/game 0x300035c0/0x200035a0; Linux game RVA 0x0001c928.
 * Linux proves the otherwise-unused clientNum leading argument. */
bg_static_animation_t *BG_GetAnimationForIndex(int32_t clientNum,
                                                uint32_t animationIndex)
{
    (void)clientNum;
    if (animationIndex >= (uint32_t)bgAnimStaticTable->entryCount) {
        bg_compat_get_animation_index_error();
    }
    return &bgAnimStaticTable->entries[animationIndex];
}
