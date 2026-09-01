#ifndef CODUOMP_SCRIPT_RUNTIME_H
#define CODUOMP_SCRIPT_RUNTIME_H

#include "../system_fatal.h"

#include <stddef.h>
#include <stdint.h>

#include "qcommon/file_data.h"
#include "scripting/script_anim.h"
#include "scripting/script_callbacks.h"
#include "scripting/script_code_emit.h"
#include "scripting/script_compile_developer.h"
#include "scripting/script_compile_load.h"
#include "scripting/script_compile_statements.h"
#include "scripting/script_memory.h"
#include "scripting/script_notify.h"
#include "scripting/script_error_reporting.h"
#include "scripting/script_import_fields.h"
#include "qcommon/script_runtime_types.h"
#include "scripting/script_runtime_state.h"
#include "scripting/script_serialization.h"
#include "scripting/script_source_positions.h"
#include "scripting/script_string.h"
#include "scripting/script_temp_memory.h"
#include "scripting/script_thread.h"
#include "qcommon/script_types.h"
#include "scripting/script_usage.h"
#include "scripting/script_value.h"
#include "scripting/script_variable.h"
#include "scripting/script_vm.h"
#include "animation/xanim.h"

#include "qcommon/hunk.h"
#include "../q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

void ScriptVariable_Release(uint16_t objectId);
int32_t Scr_GetNumScriptThreads(void);

#ifdef __cplusplus
}
#endif

#endif
