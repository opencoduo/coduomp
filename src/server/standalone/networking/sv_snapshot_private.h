#ifndef CODUO_SV_SNAPSHOT_PRIVATE_H
#define CODUO_SV_SNAPSHOT_PRIVATE_H

#include "server/standalone/bindings/coduo_engine_structs.h"
#include "server/engine/server_commands.h"
#include "server/engine/server_game_data.h"
#include "server/engine/server_snapshot_archive.h"
#include "server/engine/server_snapshot_send.h"
#include "qcommon/vm_runtime.h"

extern serverStatic_t svs;
extern serverHeader_t sv;
extern cvar_t *sv_fps;
extern cvar_t *sv_maxclients;
extern cvar_t *sv_maxRate;
extern cvar_t *sv_padPackets;
extern vm_t *sv_gameVM;

#endif
