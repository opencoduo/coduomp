#ifndef CODUO_CM_TRACE_CORE_PRIVATE_H
#define CODUO_CM_TRACE_CORE_PRIVATE_H

#include <stddef.h>
#include <stdint.h>

#include "../core_memory/core_memory_private.h"
#include "server/standalone/bindings/coduo_engine_structs.h"
#include "collision/collision_brush_traces.h"
#include "collision/collision_capsule_traces.h"
#include "collision/collision_queries.h"
#include "collision/collision_geometry.h"
#include "collision/collision_box_trace.h"
#include "collision/collision_trace_bounds.h"
#include "collision/collision_tree_traces.h"
#include "collision/collision_trace_entry.h"
#include "collision/collision_patch_dispatch.h"
#include "collision/collision_leaf_traces.h"
#include "collision/collision_map_load.h"

float VectorNormalize2(const vec3_t in, vec3_t out);


float CM_TraceFloatAbs(float value);

#endif
