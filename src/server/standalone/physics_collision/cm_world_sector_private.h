#ifndef CODUO_CM_WORLD_SECTOR_PRIVATE_H
#define CODUO_CM_WORLD_SECTOR_PRIVATE_H

#include <stddef.h>
#include <stdint.h>

#include "server/standalone/bindings/coduo_engine_structs.h"
#include "collision/collision_leaf_queries.h"
#include "collision/collision_map_load.h"
#include "collision/collision_area.h"
#include "collision/collision_point_contents.h"
#include "collision/collision_queries.h"
#include "collision/collision_box_trace.h"
#include "collision/collision_entity_traversal.h"
#include "collision/collision_server_entity.h"
#include "collision/collision_server_trace.h"
#include "collision/collision_static_model_trace.h"
#include "collision/collision_trace_bounds.h"
#include "collision/collision_world_sector.h"
#include "server/engine/server_game_data.h"
#include "qcommon/vm_runtime.h"
#include "animation/xmodel.h"

extern vm_t *sv_gameVM;

void Com_DPrintf(const char *format, ...);
#endif
