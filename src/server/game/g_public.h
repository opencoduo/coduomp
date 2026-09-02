#ifndef G_PUBLIC_H
#define G_PUBLIC_H

#include <stdint.h>
#include "qcommon/game_module_abi_types.h"
#include "recovered_game.h"
#include "g_syscalls.h"

/*
 * Module export prototypes.
 * These functions are exported to the engine via the VM interface.
 */
extern intptr_t vmMain(int32_t command, intptr_t arg0, intptr_t arg1,
                       intptr_t arg2, intptr_t arg3, intptr_t arg4,
                       intptr_t arg5, intptr_t arg6, intptr_t arg7,
                       intptr_t arg8, intptr_t arg9, intptr_t arg10,
                       intptr_t arg11);
extern void dllEntry(game_syscall_t syscallPtr);

/*
 * Game lifecycle functions called by vmMain.
 */
extern void G_InitGame(int levelTime, int randomSeed, int restart,
                       int cvarRestartGate);
extern void G_ShutdownGame(int restart);
extern char *ClientConnect(int clientNum, uint16_t persistentValue);
extern void ClientBegin(int clientNum);
extern void ClientUserinfoChanged(int clientNum);
extern void ClientDisconnect(int clientNum);
extern void ClientCommand(int clientNum);
extern void ClientThink(int clientNum);
extern qboolean GetFollowPlayerState(int clientNum, void *playerState);
extern void G_RunFrame(int levelTime);
extern void G_UpdateCvars(void);
extern int ConsoleCommand(void);
extern void G_DObjCalcPose(gentity_t *ent);

#endif /* G_PUBLIC_H */
