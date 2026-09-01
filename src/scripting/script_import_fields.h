#ifndef SHARED_SCRIPT_IMPORT_FIELDS_H
#define SHARED_SCRIPT_IMPORT_FIELDS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern uint8_t *script_importFieldBuffer;

uint16_t Scr_FindField(const char *name, int32_t *typeOut);
void Scr_AddFields(const char *path, const char *extension);

#ifdef __cplusplus
}
#endif

#endif
