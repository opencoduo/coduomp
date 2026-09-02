// Source: uo_cgame_mp_x86.dll 0x30039d30..0x30039d77
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30039d30_30039d77.mcode

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

void CG_LoadVoiceChats(void)
{
    int32_t memoryBefore = coduo_int32_from_bits(
        (uint32_t)cgame_syscall(CG_MEMORY_REMAINING));

    CG_ParseVoiceChats("mp/axis_chat.voice",
                       &cg_voiceChatTables[0],
                       CG_MAX_VOICE_CHATS);
    CG_ParseVoiceChats("mp/allies_chat.voice",
                       &cg_voiceChatTables[1],
                       CG_MAX_VOICE_CHATS);

    int32_t memoryAfter = coduo_int32_from_bits(
        (uint32_t)cgame_syscall(CG_MEMORY_REMAINING));
    Com_PrintMessage("voice chat memory size = %d\n",
                     coduo_int32_from_bits((uint32_t)memoryBefore -
                                      (uint32_t)memoryAfter));
}
