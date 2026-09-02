#ifndef SHARED_SERVER_DOBJ_H
#define SHARED_SERVER_DOBJ_H

#include "animation/dobj.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void SV_DObjDumpInfo(int32_t entityNum);
qboolean SV_DObjCreateSkelForBone(int32_t entityNum, int32_t boneIndex);
qboolean SV_DObjCreateSkelForBones(int32_t entityNum, const uint32_t *partBits);
qboolean SV_DObjUpdateServerTime(int32_t entityNum, float serverTime, qboolean notify);
void SV_DObjInitServerTime(int32_t entityNum, float serverTime);
void SV_DObjGetHierarchyBits(int32_t entityNum, int32_t boneIndex, uint32_t *partBits);
void SV_DObjCalcAnim(int32_t entityNum, const uint32_t *partBits);
void SV_DObjCalcSkel(int32_t entityNum, const uint32_t *partBits);
int32_t SV_DObjNumBones(int32_t entityNum);
int32_t SV_DObjGetBoneIndex(int32_t entityNum, const char *tagName);
DObjSkelMat *SV_DObjGetMatrixArray(int32_t entityNum);
void SV_DObjDisplayAnim(int32_t entityNum);
DObjAnimMat *SV_DObjGetRotTransArray(int32_t entityNum);
qboolean SV_DObjSetRotTransIndex(int32_t entityNum, const uint8_t *partBits, int32_t boneIndex);
qboolean SV_DObjSetControlRotTransIndex(int32_t entityNum, const uint8_t *partBits, int32_t boneIndex);
void SV_DObjGetBounds(int32_t entityNum, vec3_t mins, vec3_t maxs);
XAnimTree *SV_DObjGetTree(int32_t entityNum);

#ifdef __cplusplus
}
#endif

#endif
