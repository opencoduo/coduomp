#ifndef CODUOMP_FX_BOLT_H
#define CODUOMP_FX_BOLT_H

#include <stdint.h>

#include "qcommon/fx_types.h"
#include "../q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fx_cull_plane_s {
    vec3_t normal;
    float distance;
} fx_cull_plane_t;

typedef struct cfx_bolt_frame_s cfx_bolt_frame_t;
typedef struct fx_archive_s fx_archive_t;

typedef struct cfx_bolt_frame_ptr_s {
    cfx_bolt_frame_t *frame;
} cfx_bolt_frame_ptr_t;

struct cfx_bolt_frame_s {
    int32_t referenceCount;
    /* CL_DObjInvalidateSkels changes the shared key whenever the client
     * skeleton cache is invalidated. Bone orientations are reused only while
     * the key matches. */
    int32_t lastSkeletonCacheKey;
    orientation_t orientation;
    cfx_bolt_frame_t *next;
    sfx_bolt_info_t boltInfo;
};

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(fx_cull_plane_t) == 4, "i386 FX cull-plane alignment changed");
_Static_assert(offsetof(fx_cull_plane_t, normal) == 0x00, "i386 FX cull-plane normal moved");
_Static_assert(sizeof(((fx_cull_plane_t *)0)->normal) == 0x0c, "i386 FX cull-plane normal extent changed");
_Static_assert(offsetof(fx_cull_plane_t, distance) == 0x0c, "i386 FX cull-plane distance moved");
_Static_assert(sizeof(fx_cull_plane_t) == 0x10, "i386 FX cull-plane size changed");
_Static_assert(_Alignof(cfx_bolt_frame_ptr_t) == 4, "i386 bolt-frame pointer wrapper alignment changed");
_Static_assert(offsetof(cfx_bolt_frame_ptr_t, frame) == 0x00, "i386 bolt-frame pointer moved");
_Static_assert(sizeof(cfx_bolt_frame_ptr_t) == 0x04, "i386 bolt-frame pointer wrapper size changed");
_Static_assert(_Alignof(cfx_bolt_frame_t) == 4, "i386 bolt-frame alignment changed");
_Static_assert(offsetof(cfx_bolt_frame_t, referenceCount) == 0x00, "i386 bolt-frame reference count moved");
_Static_assert(offsetof(cfx_bolt_frame_t, lastSkeletonCacheKey) == 0x04, "i386 bolt-frame skeleton key moved");
_Static_assert(offsetof(cfx_bolt_frame_t, orientation) == 0x08, "i386 bolt-frame orientation moved");
_Static_assert(offsetof(cfx_bolt_frame_t, next) == 0x38, "i386 CFxBoltFrame list-link offset changed");
_Static_assert(offsetof(cfx_bolt_frame_t, boltInfo) == 0x3c, "i386 CFxBoltFrame info offset changed");
_Static_assert(sizeof(cfx_bolt_frame_t) == 0x44, "i386 CFxBoltFrame size changed");
#endif

extern cfx_bolt_frame_t *fxBoltFrames;
/* SFxHelper timing prefix at 0x038b5010.  AdjustTime proves the first four
 * roles: raw caller time, integrated FX time, previous integrated time, and
 * the clamped current-frame delta.  The final word is archive-only. */
extern int32_t fxLastTime;
extern int32_t fxCurrentTime;
extern int32_t fxPreviousTime;
extern int32_t fxFrameTime;
extern int32_t fxArchivedTimingState;
/* 0x038b5024 and 0x038b5030: camera origin followed by the culling planes
 * built for the current FX frame. CFlash uses the first plane's normal as the
 * camera-forward direction; sphere/cylinder culling walks the same array. */
extern vec3_t fxViewOrigin;
extern fx_cull_plane_t fxCullPlanes[];
extern int32_t fxCullPlaneCount; /* 0x038b5090 */
extern vec3_t fx_windDirection;  /* 0x038b5094 */

cfx_bolt_frame_t *CFxBoltFrame_Construct(cfx_bolt_frame_t *frame, const sfx_bolt_info_t *boltInfo);
cfx_bolt_frame_t *CFxBoltFrame_Acquire(const sfx_bolt_info_t *boltInfo);
void CFxBoltFrame_Release(cfx_bolt_frame_t *frame);
orientation_t *CFxBoltFrame_GetOrientation(cfx_bolt_frame_t *frame);
void CFxBoltFramePtr_Archive(cfx_bolt_frame_ptr_t *framePtr, fx_archive_t *archive);
void CFxBoltFramePtr_Destroy(cfx_bolt_frame_ptr_t *framePtr);
qboolean FX_GetBoneOrientation(const sfx_bolt_info_t *boltInfo, orientation_t *orientation);
#ifdef __cplusplus
}
#endif

#endif
