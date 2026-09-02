#include "server_game_lifecycle.h"

#include "collision/collision_queries.h"
#include "qcommon/com_event_queue.h"
#include "qcommon/game_module_abi_types.h"
#include "qcommon/hunk.h"
#include "qcommon/q_cvar.h"
#include "scripting/script_callbacks.h"
#include "server_game_lifecycle_services.h"
#include "server_game_syscalls.h"
#include "qcommon/server_runtime_types.h"
#include "sound/alias/sound_alias.h"
#include "qcommon/vm_runtime.h"
#include "animation/xanim.h"

#include <stddef.h>
#include <stdint.h>

enum {
    SERVER_GAME_CVAR_DUMP_CHANNEL = 4
};

extern serverHeader_t sv;
extern serverStatic_t svs;
extern vm_t *sv_gameVM;
extern char *sv_entityParsePoint;
extern cvar_t *sv_maxclients;
extern cvar_t *dedicated;

void Com_Error(errorParm_t code, const char *format, ...);

/*
 * Complete game-VM lifecycle shared by the Windows client/listen server and
 * Linux dedicated engine:
 *
 *                                  Windows client       Linux dedicated
 * SV_ResetEntityParsePoint         0x0045d810           0x0808ee92
 * SV_ShutdownGameProgs             0x0045ea60           0x08090b57
 * SV_InitGameVM                    0x0045eb50           0x08090bbb
 * SV_RestartGameProgs              0x0045ebf0           0x08090c91
 * SV_InitGameProgs                 0x0045ec50           0x08090cce
 * SV_GameCommand                   0x0045ecc0           0x08090d65
 *
 * The Linux binary has no retained names for the last four functions; the
 * exact Windows/Mac names are canonical. Direct comparison proves the same
 * VM commands, arguments, callback exchange, client-gentity reset, hunk
 * marks, sound-bank lifetime, state gate, and return values. The only target
 * edge inside the cluster is the loading hook surrounding GAME_INIT, supplied
 * by each engine through server_game_lifecycle_services.h.
 */

void SV_ResetEntityParsePoint(void)
{
    sv_entityParsePoint = CM_EntityString();
}

void SV_ShutdownGameProgs(void)
{
    Com_UnloadSoundAliases(SND_ALIAS_BANK_GAME);

    if (sv_gameVM == NULL) {
        return;
    }

    XAnimSetUser(XANIM_USER_SERVER);
    (void)VM_Call(sv_gameVM, GAME_SHUTDOWN, qfalse, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    Hunk_ClearToMarkLow();
    VM_Free(sv_gameVM);
    sv_gameVM = NULL;
}

void SV_InitGameVM(qboolean restart, qboolean cvarRestartGate)
{
    SV_ResetEntityParsePoint();

    script_vm_callback_slot_t *const engineCallbacks = Scr_NearHook(NULL);
    const script_vm_callback_slot_t *const gameCallbacks = (const script_vm_callback_slot_t *)VM_Call(
        sv_gameVM, GAME_SCRIPT_FAR_HOOK, (intptr_t)engineCallbacks, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    (void)Scr_NearHook(gameCallbacks);

    SERVER_GAME_LOADING_KEEPALIVE();
    (void)VM_Call(sv_gameVM, GAME_INIT, svs.time, Com_Milliseconds(), restart, cvarRestartGate, 0, 0, 0, 0, 0, 0, 0, 0);
    SERVER_GAME_LOADING_KEEPALIVE();

    for (int32_t clientNum = 0; clientNum < sv_maxclients->integer; ++clientNum) {
        svs.clients[clientNum].gentity = NULL;
    }

    if (dedicated->integer != 0) {
        Cvar_DumpToChannel(SERVER_GAME_CVAR_DUMP_CHANNEL);
    }
}

void SV_RestartGameProgs(qboolean cvarRestartGate)
{
    (void)VM_Call(sv_gameVM, GAME_SHUTDOWN, qtrue, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    Hunk_ClearToMarkLow();
    SV_InitGameVM(qtrue, cvarRestartGate);
}

void SV_InitGameProgs(qboolean cvarRestartGate)
{
    sv_gameVM = VM_Create("game", SV_GameSystemCalls);
    if (sv_gameVM == NULL) {
        Com_Error(ERR_FATAL, "\x15"
                             "VM_Create on game failed");
    }

    const intptr_t apiVersion = VM_Call(sv_gameVM, GAME_GET_API_VERSION, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    if (apiVersion != GAME_API_VERSION) {
        Com_Error(ERR_FATAL,
                  "\x15"
                  "game is version %d, expected %d",
                  (int32_t)apiVersion, GAME_API_VERSION);
    }

    Hunk_SetMarkLow();
    SV_InitGameVM(qfalse, cvarRestartGate);
}

qboolean SV_GameCommand(void)
{
    if (sv.state != SS_GAME) {
        return qfalse;
    }

    XAnimSetUser(XANIM_USER_SERVER);
    return (qboolean)VM_Call(sv_gameVM, GAME_CONSOLE_COMMAND, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}
