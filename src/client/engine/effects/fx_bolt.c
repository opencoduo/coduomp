#include "fx_bolt.h"

#include "fx_archive.h"
#include "fx_memory.h"

enum {
    FX_BOLT_UNBOUND = -1
};

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the inlined
 * CFxBoltFramePtr copy assignment at 0x004a0bb1..0x004a0bd1. */
static void coduomp_fx_bolt_assign_frame_ptr(cfx_bolt_frame_ptr_t *destination, const cfx_bolt_frame_ptr_t *source)
{
    if (destination->frame == source->frame) {
        return;
    }
    if (destination->frame != NULL) {
        CFxBoltFrame_Release(destination->frame);
        destination->frame = NULL;
    }
    if (source->frame != NULL) {
        ++source->frame->referenceCount;
        destination->frame = source->frame;
    }
}

/* Source: CoDUOMP.exe 0x004a0b30..0x004a0b56.
 * Name and parameter type: exact same-module Mac symbol
 * CFxBoltFrame::CFxBoltFrame(SFxBoltInfo const *). The Windows standalone
 * constructor returns its object address in EAX. */
cfx_bolt_frame_t *CFxBoltFrame_Construct(cfx_bolt_frame_t *frame, const sfx_bolt_info_t *boltInfo)
{
    frame->referenceCount = 0;
    frame->lastSkeletonCacheKey = 0;
    frame->boltInfo = *boltInfo;
    frame->next = fxBoltFrames;
    fxBoltFrames = frame;
    return frame;
}

/* Source: CoDUOMP.exe 0x004a0a40..0x004a0aae.
 * Name: same-module Mac symbol CFxBoltFrame::Acquire. */
cfx_bolt_frame_t *CFxBoltFrame_Acquire(const sfx_bolt_info_t *boltInfo)
{
    cfx_bolt_frame_t *frame = fxBoltFrames;
    while (frame != NULL) {
        if (frame->boltInfo.entityNum == boltInfo->entityNum && frame->boltInfo.boneIndex == boltInfo->boneIndex) {
            ++frame->referenceCount;
            return frame;
        }
        frame = frame->next;
    }

    frame = FxMem_AllocBoltFrame(&fxBoltFrameAllocator, sizeof(*frame));
    if (frame != NULL) {
        frame = CFxBoltFrame_Construct(frame, boltInfo);
    }
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if (frame == NULL) {
        return NULL;
    }
    ++frame->referenceCount;
    return frame;
}

/* Source: CoDUOMP.exe 0x004a0ab0..0x004a0aea.
 * Name: same-module Mac symbol CFxBoltFrame::Release. */
void CFxBoltFrame_Release(cfx_bolt_frame_t *frame)
{
    --frame->referenceCount;
    if (frame->referenceCount != 0) {
        return;
    }

    cfx_bolt_frame_t **link = &fxBoltFrames;
    while (*link != NULL && *link != frame) {
        link = &(*link)->next;
    }
    if (*link == frame) {
        *link = frame->next;
    }
    FxMem_FreeBoltFrame(&fxBoltFrameAllocator, frame);
}

/* Source: CoDUOMP.exe 0x004a0af0..0x004a0b2b.
 * Name: same-module Mac symbol CFxBoltFrame::GetOrientation. */
orientation_t *CFxBoltFrame_GetOrientation(cfx_bolt_frame_t *frame)
{
    if (frame->boltInfo.entityNum < 0) {
        return NULL;
    }
    if (frame->lastSkeletonCacheKey != dobj_skelCacheKey) {
        frame->lastSkeletonCacheKey = dobj_skelCacheKey;
        if (FX_GetBoneOrientation(&frame->boltInfo, &frame->orientation) == qfalse) {
            frame->boltInfo.entityNum = FX_BOLT_UNBOUND;
            frame->boltInfo.boneIndex = FX_BOLT_UNBOUND;
            return NULL;
        }
    }
    return &frame->orientation;
}

/* Source: CoDUOMP.exe 0x004a0b60..0x004a0c48.
 * Name: same-module Mac symbol CFxBoltFramePtr::Archive. */
void CFxBoltFramePtr_Archive(cfx_bolt_frame_ptr_t *framePtr, fx_archive_t *archive)
{
    if (archive->loading != 0) {
        sfx_bolt_info_t boltInfo;
        boltInfo.entityNum = CFxArchive_ReadInt(archive);
        if (boltInfo.entityNum >= 0) {
            boltInfo.boneIndex = CFxArchive_ReadInt(archive);
            cfx_bolt_frame_ptr_t loaded = {CFxBoltFrame_Acquire(&boltInfo)};
            coduomp_fx_bolt_assign_frame_ptr(framePtr, &loaded);
            CFxBoltFramePtr_Destroy(&loaded);
        } else if (framePtr->frame != NULL) {
            /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
            CFxBoltFrame_Release(framePtr->frame);
            framePtr->frame = NULL;
        }
        return;
    }

    if (framePtr->frame == NULL) {
        CFxArchive_WriteInt(archive, FX_BOLT_UNBOUND);
        return;
    }
    CFxArchive_WriteInt(archive, framePtr->frame->boltInfo.entityNum);
    CFxArchive_WriteInt(archive, framePtr->frame->boltInfo.boneIndex);
}

/* Source: CoDUOMP.exe 0x004a07f0..0x004a07fb.
 * Name: same-module Mac symbol CFxBoltFramePtr::~CFxBoltFramePtr. */
void CFxBoltFramePtr_Destroy(cfx_bolt_frame_ptr_t *framePtr)
{
    if (framePtr->frame != NULL) {
        CFxBoltFrame_Release(framePtr->frame);
    }
}
