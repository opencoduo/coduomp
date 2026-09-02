#ifndef SHARED_ANIMATION_XANIM_ASSET_LOAD_H
#define SHARED_ANIMATION_XANIM_ASSET_LOAD_H

#include <stddef.h>
#include <stdint.h>

#include "xanim.h"

typedef void *(*xanim_asset_alloc_fn)(size_t size);

extern uint16_t xanim_rootTreeHandle;

void XAnimBeginLoadFiles(void);
void XAnimLoadPendingFiles(xanim_asset_alloc_fn alloc);
void XAnimFreeMemory(fileData_t *entry);
void XAnimLoadFile(const char *animName, xanim_asset_alloc_fn alloc);
void ReadQuat(const int16_t packed[3], xanim_int16_vec4_t *out);
void ReadQuat2(const int16_t packed[1], xanim_int16_vec2_t *out);

const uint8_t *ReadNoteTracks(const char *animName, const uint8_t *cursor, size_t remaining, XAnimParts *record,
                              xanim_asset_alloc_fn alloc);
uint16_t XAnimSetModel(XAnimEntry *entry, XModel **models, int32_t modelCount);

#endif
