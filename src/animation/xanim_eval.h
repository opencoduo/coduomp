#ifndef SHARED_ANIMATION_XANIM_EVAL_H
#define SHARED_ANIMATION_XANIM_EVAL_H

#include "dobj.h"
#include "xanim.h"
#include "xanim_compat.h"

extern int32_t xanim_evalPartCount;
extern uint32_t xanim_evalPartBits[DOBJ_PART_BITSET_WORD_COUNT];
extern uint32_t xanim_evalSkipBits[DOBJ_PART_BITSET_WORD_COUNT];
extern uint8_t xanim_evalLeafOutputMode;
extern int32_t xanim_evalPoolWeightSelector;

void TransformToQuatRefFrame(const vec2_t rotation, vec2_t vector);
void XAnimCalcDeltaParts(XAnimParts *record,
                         vec2_t rotation, vec3_t move, float time);
void XAnimCalcRelDeltaParts(XAnimParts *record, float weight,
                            float *delta,
                            float startTime, float endTime);
void XAnimCalcAbsDeltaParts(XAnimParts *record, float weight,
                            float *delta, float time);
void XAnimCalcDeltaTree(XAnimTree *tree, uint32_t animIndex,
                        float weight, float *delta,
                        qboolean clear, qboolean normalize);
void XAnimCalcDelta(XAnimTree *tree, uint32_t animIndex,
                    vec2_t rotationDelta, vec3_t moveDelta,
                    int32_t weightSelector);
void XAnimCalcAbsDelta(XAnimTree *tree, uint32_t animIndex,
                       vec2_t rotationDelta, vec3_t moveDelta);
void XAnimGetRelDelta(XAnim *tree, uint32_t animIndex,
                      vec2_t rotationDelta, vec3_t moveDelta,
                      float startTime, float endTime);
void XAnimGetAbsDelta(XAnim *tree, uint32_t animIndex,
                      vec2_t rotationDelta, vec3_t moveDelta,
                      float time);
void XAnimCalcPartsSmallIndices(XAnimParts *record,
                                const uint8_t *partRemap, float time,
                                float weight, DObjAnimMat *parts);
void XAnimCalcPartsLargeIndices(XAnimParts *record,
                                const uint8_t *partRemap, float time,
                                float weight, DObjAnimMat *parts);
void XAnimCalcNonLoopEnd(XAnimParts *record,
                         const uint8_t *partRemap,
                         float weight, DObjAnimMat *parts);
void XAnimCalcData(fileData_t *entry,
                   XAnimToXModel *partRemap,
                   float weight, DObjAnimMat *parts,
                   float time);
void XAnimCalc(uint32_t animIndex, float weight,
               DObjAnimMat *parts, qboolean clear,
               qboolean normalize);
void DObjCalcAnim(DObj *obj, const uint32_t *partBits);

#endif
