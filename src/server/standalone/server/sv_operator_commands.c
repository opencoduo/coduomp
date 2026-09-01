#include "server/standalone/bindings/coduo_engine_structs.h"
#include "compat/coduo_x87emu.h" /* defines EMULATE_X87; x87 shim when it is 1 */
#include "compat/coduo_int32_bits.h"
#include "qcommon/vm_runtime.h"
#include "../animation/xanim_private.h"
#include "qcommon/q_command.h"
#include "../core_cvar/cvar_private.h"
#include "../core_info/info_private.h"
#include "../core_math/core_math_private.h"
#include "../core_runtime/core_runtime_private.h"
#include "../filesystem/fs_private.h"
#include "../networking/netchan_private.h"
#include "scripting/script_memory.h"
#include "../scripting/script_runtime_private.h"
#include "scripting/script_variable.h"
#include "server_private.h"
#include "sv_connect_authorize_private.h"
#include "server/engine/server_game_lifecycle.h"
#include "sv_globals_private.h"
#include "sv_init_shutdown_private.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void SV_PrintPunkBusterMessage(const char *left, const char *right)
{
    Com_Printf("%s: %s\n", left, right);
}
