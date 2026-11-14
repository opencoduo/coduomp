// Source: uo_cgame_mp_x86.dll 0x300226c0..0x30022709
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300226c0_30022709.mcode

#include "../client_recovered.h"

int32_t CG_PlayGearRattleSound(int32_t entityNum, qboolean sprinting,
                               qboolean running)
{
    const char *soundName;
    if (sprinting) {
        soundName = cgs.media.soundGearRattleSprint;
    } else if (running) {
        soundName = cgs.media.soundGearRattleRun;
    } else {
        soundName = cgs.media.soundGearRattleWalk;
    }
    return CG_PlaySoundAliasByName(
        entityNum, &cg_entities[entityNum].currentState.pos.trBase, soundName);
}
