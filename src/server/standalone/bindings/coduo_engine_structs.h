#ifndef CODUO_ENGINE_STRUCTS_H
#define CODUO_ENGINE_STRUCTS_H

/*
 * Compatibility include umbrella for recovered engine translation units.
 * Concrete type definitions and their ABI/layout assertions live together in
 * the owning shared headers below; this file must not duplicate either.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Several engine-private headers use the C11 spelling for layout checks after
 * including this umbrella.  GCC C++ requires the corresponding C++ keyword. */
#if defined(__cplusplus) && !defined(_Static_assert)
#define _Static_assert static_assert
#endif

#include "coduo_engine_abi.h"
#include "qcommon/client_state_types.h"
#include "qcommon/collision_map_types.h"
#include "qcommon/collision_trace_work_types.h"
#include "qcommon/command_types.h"
#include "qcommon/console_field_types.h"
#include "qcommon/dobj_types.h"
#include "qcommon/entity_state_types.h"
#include "qcommon/filesystem_types.h"
#include "qcommon/game_state_types.h"
#include "qcommon/hunk_types.h"
#include "qcommon/huffman.h"
#include "qcommon/net_field_types.h"
#include "qcommon/net_types.h"
#include "qcommon/player_state_types.h"
#include "qcommon/precompiler_types.h"
#include "qcommon/q_checksum.h"
#include "qcommon/q_command.h"
#include "qcommon/q_endian.h"
#include "math/q_matrix_types.h"
#include "qcommon/qcommon_limits.h"
#include "qcommon/qcommon_runtime_types.h"
#include "qcommon/q_shared_types.h"
#include "qcommon/qtime_types.h"
#include "qcommon/script_runtime_types.h"
#include "qcommon/script_types.h"
#include "qcommon/server_punkbuster_types.h"
#include "qcommon/server_runtime_types.h"
#include "qcommon/server_types.h"
#include "qcommon/sound_types.h"
#include "qcommon/system_event_types.h"
#include "qcommon/trajectory_types.h"
#include "qcommon/vm_types.h"
#include "qcommon/xanim_types.h"
#include "qcommon/xmodel_types.h"

#endif
