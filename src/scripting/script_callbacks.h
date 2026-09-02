#ifndef SCRIPT_CALLBACKS_H
#define SCRIPT_CALLBACKS_H

#include "qcommon/script_runtime_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern script_vm_callback_slot_t
    script_importCallbacks[SCRIPT_IMPORT_CALLBACK_COUNT];
extern script_vm_callback_slot_t
    script_exportCallbacks[SCRIPT_EXPORT_CALLBACK_COUNT];

script_vm_callback_slot_t *CODUO_SCRIPT_CDECL Scr_NearHook(
    const script_vm_callback_slot_t *gameCallbacks);
script_function_callback_t CODUO_SCRIPT_CDECL
Scr_GetFunction(const char **name, int32_t *developerOnly);
script_method_callback_t CODUO_SCRIPT_CDECL
Scr_GetMethod(const char **name, int32_t *developerOnly);
void CODUO_SCRIPT_CDECL Scr_SetObjectField(int32_t classNum,
                                           int32_t objectNum,
                                           int32_t fieldIndex);
void CODUO_SCRIPT_CDECL Scr_GetObjectField(int32_t classNum,
                                           int32_t objectNum,
                                           int32_t fieldIndex);
void *Scr_LoadRead(uint32_t size);

#ifdef __cplusplus
}
#endif

#endif
