#include "../client_recovered.h"
#include "../globals.h"

#include <stdint.h>

// Source: uo_cgame_mp_x86.dll 0x3002b560..0x3002ba42
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002b560_3002ba42.mcode
//
// CG_RegisterSounds — load the two team voice-chat definition files, report
// their memory cost, then register the fixed cgame sound aliases and the
// per-surface alias families. The same-module PPC name and the complete asset
// behavior agree. The former PlayerCmd_finishPlayerDamage assignment was only a
// size match and is rejected: this function has no player/entity arguments.

void CG_RegisterSounds(void)
{
    int32_t memoryBefore;
    int32_t memoryAfter;

    memoryBefore = coduo_int32_from_bits(
        (uint32_t)cgame_syscall(CG_MEMORY_REMAINING));
    CG_ParseVoiceChats("mp/axis_chat.voice", &cg_voiceChatTables[0],
                       CG_MAX_VOICE_CHATS);
    CG_ParseVoiceChats("mp/allies_chat.voice", &cg_voiceChatTables[1],
                       CG_MAX_VOICE_CHATS);
    memoryAfter = coduo_int32_from_bits(
        (uint32_t)cgame_syscall(CG_MEMORY_REMAINING));
    Com_PrintMessage("voice chat memory size = %d\n",
                     coduo_int32_from_bits((uint32_t)memoryBefore -
                                      (uint32_t)memoryAfter));

    cgs.media.soundMpAnnounceGTwoMinutes = trap_Com_SoundAliasString("mp_announce_g_twominutes");
    cgs.media.soundMpAnnounceATwoMinutes = trap_Com_SoundAliasString("mp_announce_a_twominutes");
    cgs.media.soundMpAnnounceGThirtySeconds = trap_Com_SoundAliasString("mp_announce_g_thirtyseconds");
    cgs.media.soundMpAnnounceAThirtySeconds = trap_Com_SoundAliasString("mp_announce_a_thirtyseconds");
    cgs.media.soundPlayerGib = trap_Com_SoundAliasString("player_gib");
    cgs.media.soundPlayerGibBounce = trap_Com_SoundAliasString("player_gib_bounce");
    cgs.media.soundFlameBarrelBounce = trap_Com_SoundAliasString("flamebarrel_bounce");
    cgs.media.soundOutOfAmmo = trap_Com_SoundAliasString("player_out_of_ammo");
    cgs.media.talkSound = trap_Com_SoundAliasString("player_talk");
    cgs.media.soundLandDamage = trap_Com_SoundAliasString("land_damage");
    cgs.media.watrInSound = trap_Com_SoundAliasString("player_water_in");
    cgs.media.watrOutSound = trap_Com_SoundAliasString("player_water_out");
    cgs.media.soundGrenadePulse[0] = trap_Com_SoundAliasString("player_grenade_pulse_0");
    cgs.media.soundGrenadePulse[1] = trap_Com_SoundAliasString("player_grenade_pulse_1");
    cgs.media.soundGrenadePulse[2] = trap_Com_SoundAliasString("player_grenade_pulse_2");
    cgs.media.soundGrenadePulse[3] = trap_Com_SoundAliasString("player_grenade_pulse_3");
    cgs.media.soundDebrisBounce = trap_Com_SoundAliasString("debris_bounce");
    cgs.media.soundDebrisHitPlayer = trap_Com_SoundAliasString("debris_hit_player");
    cgs.media.flameFireSound = trap_Com_SoundAliasString("flamethrower_fire");
    cgs.media.flameStartSound = trap_Com_SoundAliasString("flamethrower_start");
    cgs.media.flameStreamSound = trap_Com_SoundAliasString("flamethrower_stream");
    cgs.media.soundPlayerBoneBounce = trap_Com_SoundAliasString("player_bone_bounce");
    cgs.media.flameCooldownSound = trap_Com_SoundAliasString("flamethrower_cooldown");
    cgs.media.soundSatchelBounce = trap_Com_SoundAliasString("satchel_bounce");

    CG_RegisterSurfaceTypeSounds(cgs.media.grenadeBounceSurfaceSounds, "grenade_bounce");
    CG_RegisterSurfaceTypeSounds(cgs.media.grenadeExplodeSurfaceSounds, "grenade_explode");
    CG_RegisterSurfaceTypeSounds(cgs.media.rocketExplodeSurfaceSounds, "rocket_explode");
    CG_RegisterSurfaceTypeSounds(cgs.media.mortarExplodeSurfaceSounds, "mortar_explode");
    CG_RegisterSurfaceTypeSounds(cgs.media.artilleryExplodeSurfaceSounds, "artillery_explode");
    CG_RegisterSurfaceTypeSounds(cgs.media.tankExplodeSurfaceSounds, "tank_explode");
    CG_RegisterSurfaceTypeSounds(cgs.media.bulletSmallSurfaceSounds, "bullet_small");
    CG_RegisterSurfaceTypeSounds(cgs.media.bulletLargeSurfaceSounds, "bullet_large");
    CG_RegisterSurfaceTypeSounds(cgs.media.stepSprintSurfaceSounds, "step_sprint");
    CG_RegisterSurfaceTypeSounds(cgs.media.stepRunSurfaceSounds, "step_run");
    CG_RegisterSurfaceTypeSounds(cgs.media.stepWalkSurfaceSounds, "step_walk");
    CG_RegisterSurfaceTypeSounds(cgs.media.stepProneSurfaceSounds, "step_prone");
    /* 0x3002b859..0x3002b863: MOV EDI,0x3044c138; MOV EBX,0x30078604 ("land"). */
    CG_RegisterSurfaceTypeSounds(cgs.media.landSurfaceSounds, "land");

    cgs.media.shellFlashSounds[0] = 0;
    cgs.media.shellFlashSounds[2] = 0;
    cgs.media.shellFlashSounds[6] = 0;
    cgs.media.shellFlashSounds[5] = trap_Com_SoundAliasString("shell_flash");
    cgs.media.shellFlashSounds[4] = trap_Com_SoundAliasString("shell_flash");

    cgs.media.barrageIncomingSounds[0] = 0;
    cgs.media.barrageIncomingSounds[2] = 0;
    cgs.media.barrageIncomingSounds[6] = 0;
    cgs.media.barrageIncomingSounds[5] = trap_Com_SoundAliasString("barrage_incoming");
    cgs.media.barrageIncomingSounds[4] = trap_Com_SoundAliasString("barrage_incoming");

    cgs.media.soundGearRattleSprint = trap_Com_SoundAliasString("gear_rattle_sprint");
    cgs.media.soundGearRattleRun = trap_Com_SoundAliasString("gear_rattle_run");
    cgs.media.soundGearRattleWalk = trap_Com_SoundAliasString("gear_rattle_walk");
    cgs.media.soundMovementFoliage = trap_Com_SoundAliasString("movement_foliage");
    cgs.media.soundWhizby = trap_Com_SoundAliasString("whizby");
    cgs.media.soundFatigueBreath = trap_Com_SoundAliasString("fatigue_breath");
    cgs.media.soundSprintBreathLast = trap_Com_SoundAliasString("sprint_breath_last");
    cgs.media.soundUsGrenadeLever = trap_Com_SoundAliasString("us_grenade_lever");
    cgs.media.soundMgOverheat = trap_Com_SoundAliasString("mg_overheat");
    cgs.media.soundMgOverheatVehicle = trap_Com_SoundAliasString("mg_overheat_vehicle");
    cgs.media.soundMeleeSwingLarge = trap_Com_SoundAliasString("melee_swing_large");
    cgs.media.soundMeleeSwingSmall = trap_Com_SoundAliasString("melee_swing_small");
    cgs.media.soundMeleeHit = trap_Com_SoundAliasString("melee_hit");
    cgs.media.soundGameMessage = trap_Com_SoundAliasString("game_message");
    cgs.media.soundObjectiveComplete = trap_Com_SoundAliasString("objective_complete");
    cgs.media.soundSpotlightSpark = trap_Com_SoundAliasString("spotlight_spark");
}
