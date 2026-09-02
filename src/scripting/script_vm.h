#ifndef SHARED_SCRIPT_VM_H
#define SHARED_SCRIPT_VM_H

#include "qcommon/script_runtime_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint16_t VM_Execute(VariableValue *stackTop, script_codepos_t codePos,
                    uint16_t thread, uint16_t currentObject,
                    VariableValue *stackBase);

#ifdef __cplusplus
}
#endif

#endif
