// Source: uo_cgame_mp_x86.dll 0x30005730..0x3000585d
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30005730_3000585d.mcode
//
// BG_Player_DoControllers (0x30005730) -- per-frame update of a DObj model
// part's spine control-tag angles and its tag_origin placement.
//
// For each of the six spine control bones (cg_spineControlTagNames:
// back_low, back_mid, back_up, neck, head, pelvis) it:
//   1. lerps the record's stored angles for that bone toward a freshly computed
//      target (BG_LerpAngles, per-axis maxStep clamp; step = cg_effectFrameTime *
//      0.36), then
//   2. resolves the bone index by name (trap CG_DOBJ_GET_BONE_INDEX) and, if the
//      engine reports it a live control tag (trap CG_DOBJ_SET_CONTROL_ROT_TRANS_INDEX
//      returns nonzero), pushes the smoothed angles to the engine as that bone's
//      local control-tag orientation (CG_DObjSetLocalTagInternal, with an identity origin
//      offset read from vec3_origin).
// Then it does the same for the "tag_origin" tag: BG_LerpAngles on the origin-tag
// angles (step * 0.36) and BG_LerpOffset on the origin-tag offset (Euclidean, scale
// = cg_effectFrameTime * 0.1), gated by trap CG_DOBJ_SET_ROT_TRANS_INDEX, then
// CG_DObjSetLocalTagInternal with the record's own lerped offset as the origin.
//
// The eight target vec3s (six spine + origin-tag angles + origin-tag offset) are
// computed once up front by BG_Player_DoControllersInternal (0x30004da0) into a local
// buffer; this function only lerps toward them and forwards to the engine.
//
// Naming: the .mcode size-guess "NormalToLatLong" (pure 0x12d==0x12d window match)
// is REJECTED -- there is no lat/long normal math here; this drives BG_LerpAngles /
// BG_LerpOffset over a stateful record and issues DObj tag traps. The Mac
// BG_Player_DoControllers has the same internal-target, lerp, control-tag, and
// local-tag call sequence, resolving this name. Its thin per-tag helper is the
// separate CG_DObjSetControlTagAngles function at 0x3001fd50.
//
// The Windows game body at 0x20005560 and cgame body at 0x30005730 have the
// same target generation, frame scales, six-tag walk, and tag_origin tail.
// Linux game RVA 0x1f2fe retains the same sequence through its entity-owned
// DObj helpers.
//
// The original cgame i386 register ABI is proven from the sole caller
// CG_Player_DoControllers at 0x30021fa0:
//   EAX (incoming)  = part      -- the model-part/render context; forwarded to
//                                  BG_Player_DoControllersInternal as its `part` arg (the
//                                  processor pushes it, uninitialized-here EAX, as
//                                  that helper's stack argument at 0x3000573a).
//   EDI (incoming)  = dobjHandle-- the DObj handle (caller's trap 0xa5 result) that
//                                  every tag trap and CG_DObjSetLocalTag act on.
//   [ESP+0x74]=arg2 = record    -- the 0x4d0-stride clientInfo_t entry (EBX).
//   [ESP+0x78]=arg1 = partBits -- the DObj selection bitset forwarded to the
//                                  set-tag traps as their middle argument.
// Recovered source normalizes those values to (dobjOwner, entity, partBits,
// record). The target service header alone maps dobjOwner to the cgame handle
// traps or the game entity helpers; the shared function does not fork by module.

#include "bg_animation.h"
#include "bg_animation_services.h"

#include "compat/coduo_x87emu.h"

#include <stdint.h>

/* Windows cgame stores this table at 0x300823e0..0x300823f4 and Windows game
 * stores the same six relocated strings at 0x200833e0..0x200833f4. The Linux
 * game table has the same order. The following 0x300823f8 datum is independent,
 * not a seventh entry. */
const char *const BG_ControllerTagNames[CLIENT_INFO_SPINE_CONTROL_COUNT] = {"back_low", "back_mid", "back_up", "neck", "head", "pelvis"};

static const char BG_ControllerOriginTagName[] = "tag_origin";

/* NOT_FROM_ORIGINAL_SOURCE: preserve FILD/FMUL/FSTP for the two frame-time
 * scales on hosts without native x87. */
static float bg_compat_controller_frame_scale(float scale)
{
#if EMULATE_X87
    return x87f_store_f32(x87f_mul(x87f_load_i32(bg_compat_animation_frame_time()), x87f_load_f32(scale)));
#else
    return (float)((long double)bg_compat_animation_frame_time() * (long double)scale);
#endif
}

void BG_Player_DoControllers(void *dobjOwner, const entityState_t *entity, uint32_t *partBits, clientInfo_t *record)
{
    /*
     * 0x3000573a..0x30005741: compute all eight target angle/offset vec3s once into
     * a local buffer. LEA ESI,[ESP+0x18] is the out-buffer pointer (ESI reg arg);
     * EAX=record (0x3000573f MOV EAX,EBX); the incoming EAX (part) is the pushed
     * stack argument. out[0..5] = the six spine target angles, out[6] = origin-tag
     * target angles, out[7] = origin-tag target offset.
     */
    vec3_t targets[8];
    BG_Player_DoControllersInternal(record, entity, targets);

    /*
     * 0x30005746..0x30005766: angleStep = (float)cg_effectFrameTime * 0.36f -- the
     * per-axis BG_LerpAngles clamp used for every spine bone below.
     *   FILD [cg_effectFrameTime] ; FMUL [0x3007c06c == 0.36000001f] ; FSTP local.
     */
    float angleStep = bg_compat_controller_frame_scale(0.36000001f);

    /*
     * 0x30005770..0x300057da: for each of the six spine control bones. EBX walks
     * record->controllerAngles[i] (0x30005760 ADD EBX,0x408, then +0xc/iter);
     * the target advances through targets[i] (the ESI buffer, +0xc/iter); EBP walks
     * cg_spineControlTagNames[i] (0x30005757 MOV EBP,0x300823e0, +4/iter, until
     * 0x300823f8). ESI is the DObj handle result register, reused as a scratch loop
     * temp for the local-buffer pointer in the original; expressed here by index.
     */
    for (int32_t i = 0; i < CLIENT_INFO_SPINE_CONTROL_COUNT; ++i) {
        /*
         * 0x30005774..0x3000577b: smooth this bone's stored angles toward the target,
         * clamped per axis by angleStep. BG_LerpAngles(target, maxStep, current):
         * ECX=current=record->controllerAngles[i], EDX=target=targets[i],
         * [ESP+4]=maxStep=angleStep.
         */
        BG_LerpAngles(targets[i], angleStep, record->controllerAngles[i]);

        /*
         * 0x30005780..0x30005797: resolve the bone index by name. ESI = boneIndex =
         * trap(CG_DOBJ_GET_BONE_INDEX, dobjHandle, cg_spineControlTagNames[i]).
         * A negative result (JL) means the tag is absent -> skip this bone.
         */
        bg_compat_controller_set_control_tag_angles(dobjOwner, partBits, BG_ControllerTagNames[i], record->controllerAngles[i]);
    }

    /*
     * 0x300057dc..0x300057f1: the tag_origin angles. ESI reloaded to record
     * (0x300057dc), EBX = &record->localTagAngles (LEA [ESI+0x450]). Reuse the same
     * angleStep (ECX = [ESP+0x10], the stored step) as the per-axis clamp.
     * BG_LerpAngles(targets[6], angleStep, record->localTagAngles).
     */
    BG_LerpAngles(targets[6], angleStep, record->localTagAngles);

    /*
     * 0x300057f6..0x30005811: the tag_origin offset. offsetScale =
     * (float)cg_effectFrameTime * 0.1f (FILD ; FMUL [0x3007bf6c == 0.1f]).
     * BG_LerpOffset(target=targets[7], scale=offsetScale,
     * current=record->localTagOffset (EBP=&[ESI+0x45c])) -- Euclidean move-toward.
     */
    float offsetScale = bg_compat_controller_frame_scale(0.10000000f);
    BG_LerpOffset(targets[7], offsetScale, record->localTagOffset);

    /*
     * 0x30005816..0x3000582e: resolve the tag_origin index by name.
     * ESI = boneIndex = trap(CG_DOBJ_GET_BONE_INDEX, dobjHandle, "tag_origin").
     * The name string is 0x30071588 (cg_originTagName). Negative -> done.
     */
    bg_compat_controller_set_local_tag(dobjOwner, partBits, BG_ControllerOriginTagName, record->localTagOffset, record->localTagAngles);
}
