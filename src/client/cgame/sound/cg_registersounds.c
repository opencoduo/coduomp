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

    memoryBefore = coduo_int32_from_bits((uint32_t)cgame_syscall(CG_MEMORY_REMAINING));
    CG_ParseVoiceChats("mp/axis_chat.voice", &cg_voiceChatTables[0], CG_MAX_VOICE_CHATS);
    CG_ParseVoiceChats("mp/allies_chat.voice", &cg_voiceChatTables[1], CG_MAX_VOICE_CHATS);
    memoryAfter = coduo_int32_from_bits((uint32_t)cgame_syscall(CG_MEMORY_REMAINING));
    Com_PrintMessage("voice chat memory size = %d\n", coduo_int32_from_bits((uint32_t)memoryBefore - (uint32_t)memoryAfter));

    cg_soundMpAnnounceGTwoMinutes = trap_Com_SoundAliasString("mp_announce_g_twominutes");
    cg_soundMpAnnounceATwoMinutes = trap_Com_SoundAliasString("mp_announce_a_twominutes");
    cg_soundMpAnnounceGThirtySeconds = trap_Com_SoundAliasString("mp_announce_g_thirtyseconds");
    cg_soundMpAnnounceAThirtySeconds = trap_Com_SoundAliasString("mp_announce_a_thirtyseconds");
    cg_soundPlayerGib = trap_Com_SoundAliasString("player_gib");
    cg_soundPlayerGibBounce = trap_Com_SoundAliasString("player_gib_bounce");
    cg_soundFlameBarrelBounce = trap_Com_SoundAliasString("flamebarrel_bounce");
    cg_soundOutOfAmmo = trap_Com_SoundAliasString("player_out_of_ammo");
    cgs_media_playerTalkSound = trap_Com_SoundAliasString("player_talk");
    cg_soundLandDamage = trap_Com_SoundAliasString("land_damage");
    cg_soundPlayerWaterIn = trap_Com_SoundAliasString("player_water_in");
    cg_soundPlayerWaterOut = trap_Com_SoundAliasString("player_water_out");
    cg_soundGrenadePulse[0] = trap_Com_SoundAliasString("player_grenade_pulse_0");
    cg_soundGrenadePulse[1] = trap_Com_SoundAliasString("player_grenade_pulse_1");
    cg_soundGrenadePulse[2] = trap_Com_SoundAliasString("player_grenade_pulse_2");
    cg_soundGrenadePulse[3] = trap_Com_SoundAliasString("player_grenade_pulse_3");
    cg_soundDebrisBounce = trap_Com_SoundAliasString("debris_bounce");
    cg_soundDebrisHitPlayer = trap_Com_SoundAliasString("debris_hit_player");
    cg_flameFireSound = trap_Com_SoundAliasString("flamethrower_fire");
    cg_flameStartSound = trap_Com_SoundAliasString("flamethrower_start");
    cg_flameStreamSound = trap_Com_SoundAliasString("flamethrower_stream");
    cg_soundPlayerBoneBounce = trap_Com_SoundAliasString("player_bone_bounce");
    cg_flameCooldownSound = trap_Com_SoundAliasString("flamethrower_cooldown");
    cg_soundSatchelBounce = trap_Com_SoundAliasString("satchel_bounce");

    CG_RegisterSurfaceTypeSounds(cg_grenadeBounceSurfaceSounds, "grenade_bounce");
    CG_RegisterSurfaceTypeSounds(cg_grenadeExplodeSurfaceSounds, "grenade_explode");
    CG_RegisterSurfaceTypeSounds(cg_rocketExplodeSurfaceSounds, "rocket_explode");
    CG_RegisterSurfaceTypeSounds(cg_mortarExplodeSurfaceSounds, "mortar_explode");
    CG_RegisterSurfaceTypeSounds(cg_artilleryExplodeSurfaceSounds, "artillery_explode");
    CG_RegisterSurfaceTypeSounds(cg_tankExplodeSurfaceSounds, "tank_explode");
    CG_RegisterSurfaceTypeSounds(cg_bulletSmallSurfaceSounds, "bullet_small");
    CG_RegisterSurfaceTypeSounds(cg_bulletLargeSurfaceSounds, "bullet_large");
    CG_RegisterSurfaceTypeSounds(cg_stepSprintSurfaceSounds, "step_sprint");
    CG_RegisterSurfaceTypeSounds(cg_stepRunSurfaceSounds, "step_run");
    CG_RegisterSurfaceTypeSounds(cg_stepWalkSurfaceSounds, "step_walk");
    CG_RegisterSurfaceTypeSounds(cg_stepProneSurfaceSounds, "step_prone");
    CG_RegisterSurfaceTypeSounds(cg_shellFlashSurfaceSounds, "shell_flash");

    cg_shellFlashSounds[0] = 0;
    cg_shellFlashSounds[2] = 0;
    cg_shellFlashSounds[6] = 0;
    cg_shellFlashSounds[5] = trap_Com_SoundAliasString("shell_flash");
    cg_shellFlashSounds[4] = trap_Com_SoundAliasString("shell_flash");

    cg_barrageIncomingSounds[0] = 0;
    cg_barrageIncomingSounds[2] = 0;
    cg_barrageIncomingSounds[6] = 0;
    cg_barrageIncomingSounds[5] = trap_Com_SoundAliasString("barrage_incoming");
    cg_barrageIncomingSounds[4] = trap_Com_SoundAliasString("barrage_incoming");

    cg_soundGearRattleSprint = trap_Com_SoundAliasString("gear_rattle_sprint");
    cg_soundGearRattleRun = trap_Com_SoundAliasString("gear_rattle_run");
    cg_soundGearRattleWalk = trap_Com_SoundAliasString("gear_rattle_walk");
    cg_soundMovementFoliage = trap_Com_SoundAliasString("movement_foliage");
    cg_soundWhizby = trap_Com_SoundAliasString("whizby");
    cg_soundFatigueBreath = trap_Com_SoundAliasString("fatigue_breath");
    cg_soundSprintBreathLast = trap_Com_SoundAliasString("sprint_breath_last");
    cg_soundUsGrenadeLever = trap_Com_SoundAliasString("us_grenade_lever");
    cg_soundMgOverheat = trap_Com_SoundAliasString("mg_overheat");
    cg_soundMgOverheatVehicle = trap_Com_SoundAliasString("mg_overheat_vehicle");
    cg_soundMeleeSwingLarge = trap_Com_SoundAliasString("melee_swing_large");
    cg_soundMeleeSwingSmall = trap_Com_SoundAliasString("melee_swing_small");
    cg_soundMeleeHit = trap_Com_SoundAliasString("melee_hit");
    cg_soundGameMessage = trap_Com_SoundAliasString("game_message");
    cg_soundObjectiveComplete = trap_Com_SoundAliasString("objective_complete");
    cg_soundSpotlightSpark = trap_Com_SoundAliasString("spotlight_spark");
}
