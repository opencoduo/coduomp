#include "bg_animation.h"

#include <stddef.h>

#define BG_INDEXED(name) { (name), BG_INDEXED_STRING_HASH_UNSET }
#define BG_INDEXED_END { NULL, BG_INDEXED_STRING_HASH_UNSET }

/*
 * The initialized tables agree byte-for-byte between the authoritative
 * Windows cgame/game modules and between the supporting Mac cgame/game
 * modules. Linux game carries the same ordered strings and condition-mode/
 * value-table mapping. The paired Windows addresses below retain the exact
 * provenance of each shared object.
 */
/* uo_cgame_mp_x86.dll 0x3053a040; uo_game_mp_x86.dll 0x20358760. */
bg_indexed_string_t weaponStrings[MAX_WEAPONS];

/* 0x30082054 / 0x20083054. */
bg_indexed_string_t animStateStr[] = {
    BG_INDEXED("RELAXED"), BG_INDEXED("QUERY"), BG_INDEXED("ALERT"),
    BG_INDEXED("COMBAT"), BG_INDEXED_END
};

/* 0x30082080 / 0x20083080. */
bg_indexed_string_t bgAnimGroupStrings[] = {
    BG_INDEXED("** UNUSED **"), BG_INDEXED("IDLE"), BG_INDEXED("IDLECR"),
    BG_INDEXED("IDLEPRONE"), BG_INDEXED("WALK"), BG_INDEXED("WALKBK"),
    BG_INDEXED("WALKCR"), BG_INDEXED("WALKCRBK"),
    BG_INDEXED("WALKPRONE"), BG_INDEXED("WALKPRONEBK"),
    BG_INDEXED("RUN"), BG_INDEXED("RUNBK"), BG_INDEXED("RUNCR"),
    BG_INDEXED("RUNCRBK"), BG_INDEXED("TURNRIGHT"),
    BG_INDEXED("TURNLEFT"), BG_INDEXED("CLIMBUP"),
    BG_INDEXED("CLIMBDOWN"), BG_INDEXED_END
};

/* 0x30082118 / 0x20083118. */
bg_indexed_string_t bgAnimEventStrings[] = {
    BG_INDEXED("PAIN"), BG_INDEXED("DEATH"), BG_INDEXED("FIREWEAPON"),
    BG_INDEXED("JUMP"), BG_INDEXED("JUMPBK"), BG_INDEXED("LAND"),
    BG_INDEXED("DROPWEAPON"), BG_INDEXED("RAISEWEAPON"),
    BG_INDEXED("CLIMBMOUNT"), BG_INDEXED("CLIMBDISMOUNT"),
    BG_INDEXED("RELOAD"), BG_INDEXED("CROUCH_TO_PRONE"),
    BG_INDEXED("PRONE_TO_CROUCH"), BG_INDEXED("MELEEATTACK"),
    BG_INDEXED("LMG_DEPLOY"), BG_INDEXED("LMG_BREAKDOWN"), BG_INDEXED_END
};

/* 0x300821a0 / 0x200831a0. */
bg_indexed_string_t animBodyPartsStr[] = {
    BG_INDEXED("** UNUSED **"), BG_INDEXED("LEGS"),
    BG_INDEXED("TORSO"), BG_INDEXED("BOTH"), BG_INDEXED_END
};

/* 0x300821f8 / 0x200831f8. */
bg_indexed_string_t animMountedStr[] = {
    BG_INDEXED("** UNUSED **"), BG_INDEXED("MG42"),
    BG_INDEXED("vehDriver"), BG_INDEXED("vehGunner"),
    BG_INDEXED("vehPassenger1"), BG_INDEXED("vehPassenger2"),
    BG_INDEXED("vehPassenger3"), BG_INDEXED("vehPassenger4"),
    BG_INDEXED_END
};

/* 0x30082240 / 0x20083240. */
bg_indexed_string_t animVehicleMotionStr[] = {
    BG_INDEXED("** UNUSED **"), BG_INDEXED("IDLE"),
    BG_INDEXED("FORWARD"), BG_INDEXED("REVERSING"), BG_INDEXED_END
};

/* 0x30082268 / 0x20083268. */
bg_indexed_string_t animVehicleStr[] = {
    BG_INDEXED("** UNUSED **"), BG_INDEXED("TANK"), BG_INDEXED("JEEP"),
    BG_INDEXED("FLAK88"), BG_INDEXED_END
};

/* 0x30082290 / 0x20083290. */
bg_indexed_string_t animWeaponClassStr[] = {
    BG_INDEXED("RIFLE"), BG_INDEXED("MG"), BG_INDEXED("SMG"),
    BG_INDEXED("LMG"), BG_INDEXED("PISTOL"), BG_INDEXED("GRENADE"),
    BG_INDEXED("ROCKETLAUNCHER"), BG_INDEXED("TURRET"),
    BG_INDEXED("SPOTTER"), BG_INDEXED("NON-PLAYER"),
    BG_INDEXED("FLAMETHROWER"), BG_INDEXED_END
};

/* 0x300822f0 / 0x200832f0. */
bg_indexed_string_t animWeaponPositionStr[] = {
    BG_INDEXED("HIP"), BG_INDEXED("ADS"), BG_INDEXED_END
};

/* 0x30082308 / 0x20083308. */
bg_indexed_string_t animStrafeStateStr[] = {
    BG_INDEXED("NOT"), BG_INDEXED("LEFT"), BG_INDEXED("RIGHT"),
    BG_INDEXED_END
};

/* 0x30082328 / 0x20083328. */
bg_indexed_string_t bgAnimConditionTypeStrings[] = {
    BG_INDEXED("WEAPONS"), BG_INDEXED("WEAPONCLASS"),
    BG_INDEXED("MOUNTED"), BG_INDEXED("VEHICLE"),
    BG_INDEXED("VEHICLE_MOTION"), BG_INDEXED("MOVETYPE"),
    BG_INDEXED("UNDERHAND"), BG_INDEXED("CROUCHING"),
    BG_INDEXED("FIRING"), BG_INDEXED("WEAPON_POSITION"),
    BG_INDEXED("STRAFING"), BG_INDEXED_END
};

/* 0x300823f8 / 0x200833f8. */
bg_indexed_string_t bgAnimParseSectionStrings[] = {
    BG_INDEXED("defines"), BG_INDEXED("animations"),
    BG_INDEXED("canned_animations"), BG_INDEXED("statechanges"),
    BG_INDEXED("events"), BG_INDEXED_END
};

/* 0x30082388 / 0x20083388. */
bg_anim_condition_type_t bgAnimConditionTypes[] = {
    { ANIM_CONDMODE_BITMASK, weaponStrings },
    { ANIM_CONDMODE_BITMASK, animWeaponClassStr },
    { ANIM_CONDMODE_EQUAL, animMountedStr },
    { ANIM_CONDMODE_EQUAL, animVehicleStr },
    { ANIM_CONDMODE_EQUAL, animVehicleMotionStr },
    { ANIM_CONDMODE_BITMASK, bgAnimGroupStrings },
    { ANIM_CONDMODE_EQUAL, NULL },
    { ANIM_CONDMODE_EQUAL, NULL },
    { ANIM_CONDMODE_EQUAL, NULL },
    { ANIM_CONDMODE_EQUAL, animWeaponPositionStr },
    { ANIM_CONDMODE_EQUAL, animStrafeStateStr }
};

bg_anim_move_type_t bgAnimParseCurrentAnimGroup = ANIM_MT_UNUSED;
bg_anim_event_t bgAnimParseCurrentEvent = ANIM_EVENT_PAIN;
bg_indexed_string_t bgAnimConditionAliases[
    ANIM_COND_COUNT * BG_ANIM_CONDITION_VALUE_COUNT];
bg_condition_bits_t bgAnimConditionAliasBits[
    ANIM_COND_COUNT * BG_ANIM_CONDITION_VALUE_COUNT];
int32_t bgAnimConditionAliasCounts[ANIM_COND_COUNT];
char bgAnimConditionAliasStringBuffer[
    BG_ANIM_CONDITION_ALIAS_STRING_BUFFER_SIZE];
int32_t bgAnimConditionAliasStringUsed;
char bgAnimScriptFileBuffer[BG_ANIM_SCRIPT_FILE_BUFFER_SIZE];
int32_t bgAnimScriptLoaded;
const char *bgPlayerAnimScriptPath = "mp/playeranim.script";

#undef BG_INDEXED_END
#undef BG_INDEXED
