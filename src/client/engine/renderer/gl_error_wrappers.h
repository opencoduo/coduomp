#ifndef CODUOMP_RENDERER_GL_ERROR_WRAPPERS_H
#define CODUOMP_RENDERER_GL_ERROR_WRAPPERS_H

#include <stdint.h>

#include "gl_api.h"

#ifdef __cplusplus
extern "C" {
#endif

void RENDERER_GL_API_CALL GL_CheckedMultiTexCoord2fARB(uint32_t target, float s, float t);
void RENDERER_GL_API_CALL GL_CheckedActiveTextureARB(uint32_t texture);
void RENDERER_GL_API_CALL GL_CheckedClientActiveTextureARB(uint32_t texture);
void RENDERER_GL_API_CALL GL_CheckedLockArraysEXT(int32_t first, int32_t count);
void RENDERER_GL_API_CALL GL_CheckedUnlockArraysEXT(void);
void RENDERER_GL_API_CALL GL_CheckedPNTrianglesiATI(uint32_t parameter, int32_t value);
void RENDERER_GL_API_CALL GL_CheckedPNTrianglesfATI(uint32_t parameter, float value);
void RENDERER_GL_API_CALL GL_CheckedDrawRangeElementsEXT(uint32_t mode, uint32_t start, uint32_t end, int32_t count, uint32_t type,
                                                         const void *indices);
void RENDERER_GL_API_CALL GL_CheckedCompressedTexImage3DARB(uint32_t target, int32_t level, uint32_t internalFormat, int32_t width,
                                                            int32_t height, int32_t depth, int32_t border, int32_t imageSize,
                                                            const void *data);
void RENDERER_GL_API_CALL GL_CheckedCompressedTexImage2DARB(uint32_t target, int32_t level, uint32_t internalFormat, int32_t width,
                                                            int32_t height, int32_t border, int32_t imageSize, const void *data);
void RENDERER_GL_API_CALL GL_CheckedCompressedTexImage1DARB(uint32_t target, int32_t level, uint32_t internalFormat, int32_t width,
                                                            int32_t border, int32_t imageSize, const void *data);
void RENDERER_GL_API_CALL GL_CheckedCompressedTexSubImage3DARB(uint32_t target, int32_t level, int32_t xOffset, int32_t yOffset,
                                                               int32_t zOffset, int32_t width, int32_t height, int32_t depth,
                                                               uint32_t format, int32_t imageSize, const void *data);
void RENDERER_GL_API_CALL GL_CheckedCompressedTexSubImage2DARB(uint32_t target, int32_t level, int32_t xOffset, int32_t yOffset,
                                                               int32_t width, int32_t height, uint32_t format, int32_t imageSize,
                                                               const void *data);
void RENDERER_GL_API_CALL GL_CheckedCompressedTexSubImage1DARB(uint32_t target, int32_t level, int32_t xOffset, int32_t width,
                                                               uint32_t format, int32_t imageSize, const void *data);
void RENDERER_GL_API_CALL GL_CheckedGetCompressedTexImageARB(uint32_t target, int32_t level, void *image);
void RENDERER_GL_API_CALL GL_CheckedBindBufferARB(uint32_t target, uint32_t buffer);
void RENDERER_GL_API_CALL GL_CheckedDeleteBuffersARB(int32_t count, const uint32_t *buffers);
void RENDERER_GL_API_CALL GL_CheckedGenBuffersARB(int32_t count, uint32_t *buffers);
uint8_t RENDERER_GL_API_CALL GL_CheckedIsBufferARB(uint32_t buffer);
void RENDERER_GL_API_CALL GL_CheckedBufferDataARB(uint32_t target, intptr_t size, const void *data, uint32_t usage);
void RENDERER_GL_API_CALL GL_CheckedBufferSubDataARB(uint32_t target, intptr_t offset, intptr_t size, const void *data);
void RENDERER_GL_API_CALL GL_CheckedGetBufferSubDataARB(uint32_t target, intptr_t offset, intptr_t size, void *data);
void *RENDERER_GL_API_CALL GL_CheckedMapBufferARB(uint32_t target, uint32_t access);
uint8_t RENDERER_GL_API_CALL GL_CheckedUnmapBufferARB(uint32_t target);
void RENDERER_GL_API_CALL GL_CheckedGetBufferParameterivARB(uint32_t target, uint32_t parameter, int32_t *values);
void RENDERER_GL_API_CALL GL_CheckedGetBufferPointervARB(uint32_t target, uint32_t parameter, void **pointer);
uint32_t RENDERER_GL_API_CALL GL_CheckedNewObjectBufferATI(int32_t size, const void *data, uint32_t usage);
uint8_t RENDERER_GL_API_CALL GL_CheckedIsObjectBufferATI(uint32_t buffer);
void RENDERER_GL_API_CALL GL_CheckedUpdateObjectBufferATI(uint32_t buffer, uint32_t offset, int32_t size, const void *data,
                                                          uint32_t preserveMode);
void RENDERER_GL_API_CALL GL_CheckedGetObjectBufferfvATI(uint32_t buffer, uint32_t parameter, float *values);
void RENDERER_GL_API_CALL GL_CheckedGetObjectBufferivATI(uint32_t buffer, uint32_t parameter, int32_t *values);
void RENDERER_GL_API_CALL GL_CheckedFreeObjectBufferATI(uint32_t buffer);
void RENDERER_GL_API_CALL GL_CheckedArrayObjectATI(uint32_t array, int32_t size, uint32_t type, int32_t stride, uint32_t buffer,
                                                   uint32_t offset);
void RENDERER_GL_API_CALL GL_CheckedGetArrayObjectfvATI(uint32_t array, uint32_t parameter, float *values);
void RENDERER_GL_API_CALL GL_CheckedGetArrayObjectivATI(uint32_t array, uint32_t parameter, int32_t *values);
void RENDERER_GL_API_CALL GL_CheckedVariantArrayObjectATI(uint32_t id, uint32_t type, int32_t stride, uint32_t buffer, uint32_t offset);
void RENDERER_GL_API_CALL GL_CheckedGetVariantArrayObjectfvATI(uint32_t id, uint32_t parameter, float *values);
void RENDERER_GL_API_CALL GL_CheckedGetVariantArrayObjectivATI(uint32_t id, uint32_t parameter, int32_t *values);
void RENDERER_GL_API_CALL GL_CheckedElementPointerATI(uint32_t type, const void *pointer);
void RENDERER_GL_API_CALL GL_CheckedDrawElementArrayATI(uint32_t mode, int32_t count);
void RENDERER_GL_API_CALL GL_CheckedDrawRangeElementArrayATI(uint32_t mode, uint32_t start, uint32_t end, int32_t count);
void RENDERER_GL_API_CALL GL_CheckedFlushVertexArrayRangeNV(void);
void RENDERER_GL_API_CALL GL_CheckedVertexArrayRangeNV(int32_t length, const void *pointer);
void *RENDERER_GL_API_CALL GL_CheckedAllocateMemoryNV(int32_t size, float readFrequency, float writeFrequency, float priority);
void RENDERER_GL_API_CALL GL_CheckedFreeMemoryNV(void *memory);
void RENDERER_GL_API_CALL GL_CheckedDeleteFencesNV(int32_t count, const uint32_t *fences);
void RENDERER_GL_API_CALL GL_CheckedGenFencesNV(int32_t count, uint32_t *fences);
uint8_t RENDERER_GL_API_CALL GL_CheckedIsFenceNV(uint32_t fence);
uint8_t RENDERER_GL_API_CALL GL_CheckedTestFenceNV(uint32_t fence);
void RENDERER_GL_API_CALL GL_CheckedGetFenceivNV(uint32_t fence, uint32_t parameter, int32_t *values);
void RENDERER_GL_API_CALL GL_CheckedFinishFenceNV(uint32_t fence);
void RENDERER_GL_API_CALL GL_CheckedSetFenceNV(uint32_t fence, uint32_t condition);
void RENDERER_GL_API_CALL GL_CheckedCombinerParameterfvNV(uint32_t parameter, const float *values);
void RENDERER_GL_API_CALL GL_CheckedCombinerParameterfNV(uint32_t parameter, float value);
void RENDERER_GL_API_CALL GL_CheckedCombinerParameterivNV(uint32_t parameter, const int32_t *values);
void RENDERER_GL_API_CALL GL_CheckedCombinerParameteriNV(uint32_t parameter, int32_t value);
void RENDERER_GL_API_CALL GL_CheckedCombinerInputNV(uint32_t stage, uint32_t portion, uint32_t variable, uint32_t input, uint32_t mapping,
                                                    uint32_t componentUsage);
void RENDERER_GL_API_CALL GL_CheckedCombinerOutputNV(uint32_t stage, uint32_t portion, uint32_t abOutput, uint32_t cdOutput,
                                                     uint32_t sumOutput, uint32_t scale, uint32_t bias, uint8_t abDotProduct,
                                                     uint8_t cdDotProduct, uint8_t muxSum);
void RENDERER_GL_API_CALL GL_CheckedFinalCombinerInputNV(uint32_t variable, uint32_t input, uint32_t mapping, uint32_t componentUsage);
void RENDERER_GL_API_CALL GL_CheckedGetCombinerInputParameterfvNV(uint32_t stage, uint32_t portion, uint32_t variable, uint32_t parameter,
                                                                  float *values);
void RENDERER_GL_API_CALL GL_CheckedGetCombinerInputParameterivNV(uint32_t stage, uint32_t portion, uint32_t variable, uint32_t parameter,
                                                                  int32_t *values);
void RENDERER_GL_API_CALL GL_CheckedGetCombinerOutputParameterfvNV(uint32_t stage, uint32_t portion, uint32_t parameter, float *values);
void RENDERER_GL_API_CALL GL_CheckedGetCombinerOutputParameterivNV(uint32_t stage, uint32_t portion, uint32_t parameter, int32_t *values);
void RENDERER_GL_API_CALL GL_CheckedGetFinalCombinerInputParameterfvNV(uint32_t variable, uint32_t parameter, float *values);
void RENDERER_GL_API_CALL GL_CheckedGetFinalCombinerInputParameterivNV(uint32_t variable, uint32_t parameter, int32_t *values);
void RENDERER_GL_API_CALL GL_CheckedCombinerStageParameterfvNV(uint32_t stage, uint32_t parameter, const float *values);
void RENDERER_GL_API_CALL GL_CheckedGetCombinerStageParameterfvNV(uint32_t stage, uint32_t parameter, float *values);
uint32_t RENDERER_GL_API_CALL GL_CheckedGenFragmentShadersATI(uint32_t range);
void RENDERER_GL_API_CALL GL_CheckedBindFragmentShaderATI(uint32_t shader);
void RENDERER_GL_API_CALL GL_CheckedDeleteFragmentShaderATI(uint32_t shader);
void RENDERER_GL_API_CALL GL_CheckedBeginFragmentShaderATI(void);
void RENDERER_GL_API_CALL GL_CheckedEndFragmentShaderATI(void);
void RENDERER_GL_API_CALL GL_CheckedPassTexCoordATI(uint32_t destination, uint32_t coordinate, uint32_t swizzle);
void RENDERER_GL_API_CALL GL_CheckedSampleMapATI(uint32_t destination, uint32_t interpolation, uint32_t swizzle);
void RENDERER_GL_API_CALL GL_CheckedColorFragmentOp1ATI(uint32_t operation, uint32_t destination, uint32_t destinationMask,
                                                        uint32_t destinationModifier, uint32_t argument1, uint32_t argument1Replication,
                                                        uint32_t argument1Modifier);
void RENDERER_GL_API_CALL GL_CheckedColorFragmentOp2ATI(uint32_t operation, uint32_t destination, uint32_t destinationMask,
                                                        uint32_t destinationModifier, uint32_t argument1, uint32_t argument1Replication,
                                                        uint32_t argument1Modifier, uint32_t argument2, uint32_t argument2Replication,
                                                        uint32_t argument2Modifier);
void RENDERER_GL_API_CALL GL_CheckedColorFragmentOp3ATI(uint32_t operation, uint32_t destination, uint32_t destinationMask,
                                                        uint32_t destinationModifier, uint32_t argument1, uint32_t argument1Replication,
                                                        uint32_t argument1Modifier, uint32_t argument2, uint32_t argument2Replication,
                                                        uint32_t argument2Modifier, uint32_t argument3, uint32_t argument3Replication,
                                                        uint32_t argument3Modifier);
void RENDERER_GL_API_CALL GL_CheckedAlphaFragmentOp1ATI(uint32_t operation, uint32_t destination, uint32_t destinationModifier,
                                                        uint32_t argument1, uint32_t argument1Replication, uint32_t argument1Modifier);
void RENDERER_GL_API_CALL GL_CheckedAlphaFragmentOp2ATI(uint32_t operation, uint32_t destination, uint32_t destinationModifier,
                                                        uint32_t argument1, uint32_t argument1Replication, uint32_t argument1Modifier,
                                                        uint32_t argument2, uint32_t argument2Replication, uint32_t argument2Modifier);
void RENDERER_GL_API_CALL GL_CheckedAlphaFragmentOp3ATI(uint32_t operation, uint32_t destination, uint32_t destinationModifier,
                                                        uint32_t argument1, uint32_t argument1Replication, uint32_t argument1Modifier,
                                                        uint32_t argument2, uint32_t argument2Replication, uint32_t argument2Modifier,
                                                        uint32_t argument3, uint32_t argument3Replication, uint32_t argument3Modifier);
void RENDERER_GL_API_CALL GL_CheckedSetFragmentShaderConstantATI(uint32_t destination, const float *value);
void RENDERER_GL_API_CALL GL_CheckedVertexAttrib1sARB(uint32_t index, int16_t x);
void RENDERER_GL_API_CALL GL_CheckedVertexAttrib1fARB(uint32_t index, float x);
void RENDERER_GL_API_CALL GL_CheckedVertexAttrib1dARB(uint32_t index, double x);
void RENDERER_GL_API_CALL GL_CheckedVertexAttrib2sARB(uint32_t index, int16_t x, int16_t y);
void RENDERER_GL_API_CALL GL_CheckedVertexAttrib2fARB(uint32_t index, float x, float y);
void RENDERER_GL_API_CALL GL_CheckedVertexAttrib2dARB(uint32_t index, double x, double y);
void RENDERER_GL_API_CALL GL_CheckedVertexAttrib3sARB(uint32_t index, int16_t x, int16_t y, int16_t z);
void RENDERER_GL_API_CALL GL_CheckedVertexAttrib3fARB(uint32_t index, float x, float y, float z);
void RENDERER_GL_API_CALL GL_CheckedVertexAttrib3dARB(uint32_t index, double x, double y, double z);
void RENDERER_GL_API_CALL GL_CheckedVertexAttrib4sARB(uint32_t index, int16_t x, int16_t y, int16_t z, int16_t w);
void RENDERER_GL_API_CALL GL_CheckedVertexAttrib4fARB(uint32_t index, float x, float y, float z, float w);
void RENDERER_GL_API_CALL GL_CheckedVertexAttrib4dARB(uint32_t index, double x, double y, double z, double w);
void RENDERER_GL_API_CALL GL_CheckedVertexAttrib4NubARB(uint32_t index, uint8_t x, uint8_t y, uint8_t z, uint8_t w);
void RENDERER_GL_API_CALL GL_CheckedVertexAttrib1svARB(uint32_t index, const int16_t *values);
void RENDERER_GL_API_CALL GL_CheckedVertexAttrib1fvARB(uint32_t index, const float *values);
void RENDERER_GL_API_CALL GL_CheckedVertexAttrib1dvARB(uint32_t index, const double *values);
void RENDERER_GL_API_CALL GL_CheckedVertexAttrib2svARB(uint32_t index, const int16_t *values);
void RENDERER_GL_API_CALL GL_CheckedVertexAttrib2fvARB(uint32_t index, const float *values);
void RENDERER_GL_API_CALL GL_CheckedVertexAttrib2dvARB(uint32_t index, const double *values);
void RENDERER_GL_API_CALL GL_CheckedVertexAttrib3svARB(uint32_t index, const int16_t *values);
void RENDERER_GL_API_CALL GL_CheckedVertexAttrib3fvARB(uint32_t index, const float *values);
void RENDERER_GL_API_CALL GL_CheckedVertexAttrib3dvARB(uint32_t index, const double *values);
void RENDERER_GL_API_CALL GL_CheckedVertexAttrib4bvARB(uint32_t index, const int8_t *values);
void RENDERER_GL_API_CALL GL_CheckedVertexAttrib4svARB(uint32_t index, const int16_t *values);
void RENDERER_GL_API_CALL GL_CheckedVertexAttrib4ivARB(uint32_t index, const int32_t *values);
void RENDERER_GL_API_CALL GL_CheckedVertexAttrib4ubvARB(uint32_t index, const uint8_t *values);
void RENDERER_GL_API_CALL GL_CheckedVertexAttrib4usvARB(uint32_t index, const uint16_t *values);
void RENDERER_GL_API_CALL GL_CheckedVertexAttrib4uivARB(uint32_t index, const uint32_t *values);
void RENDERER_GL_API_CALL GL_CheckedVertexAttrib4fvARB(uint32_t index, const float *values);
void RENDERER_GL_API_CALL GL_CheckedVertexAttrib4dvARB(uint32_t index, const double *values);
void RENDERER_GL_API_CALL GL_CheckedVertexAttrib4NbvARB(uint32_t index, const int8_t *values);
void RENDERER_GL_API_CALL GL_CheckedVertexAttrib4NsvARB(uint32_t index, const int16_t *values);
void RENDERER_GL_API_CALL GL_CheckedVertexAttrib4NivARB(uint32_t index, const int32_t *values);
void RENDERER_GL_API_CALL GL_CheckedVertexAttrib4NubvARB(uint32_t index, const uint8_t *values);
void RENDERER_GL_API_CALL GL_CheckedVertexAttrib4NusvARB(uint32_t index, const uint16_t *values);
void RENDERER_GL_API_CALL GL_CheckedVertexAttrib4NuivARB(uint32_t index, const uint32_t *values);
void RENDERER_GL_API_CALL GL_CheckedVertexAttribPointerARB(uint32_t index, int32_t size, uint32_t type, uint8_t normalized, int32_t stride,
                                                           const void *pointer);
void RENDERER_GL_API_CALL GL_CheckedEnableVertexAttribArrayARB(uint32_t index);
void RENDERER_GL_API_CALL GL_CheckedDisableVertexAttribArrayARB(uint32_t index);
void RENDERER_GL_API_CALL GL_CheckedProgramStringARB(uint32_t target, uint32_t format, int32_t length, const void *string);
void RENDERER_GL_API_CALL GL_CheckedBindProgramARB(uint32_t target, uint32_t program);
void RENDERER_GL_API_CALL GL_CheckedDeleteProgramsARB(int32_t count, const uint32_t *programs);
void RENDERER_GL_API_CALL GL_CheckedGenProgramsARB(int32_t count, uint32_t *programs);
void RENDERER_GL_API_CALL GL_CheckedProgramEnvParameter4fARB(uint32_t target, uint32_t index, float x, float y, float z, float w);
void RENDERER_GL_API_CALL GL_CheckedProgramEnvParameter4dARB(uint32_t target, uint32_t index, double x, double y, double z, double w);
void RENDERER_GL_API_CALL GL_CheckedProgramEnvParameter4fvARB(uint32_t target, uint32_t index, const float *values);
void RENDERER_GL_API_CALL GL_CheckedProgramEnvParameter4dvARB(uint32_t target, uint32_t index, const double *values);
void RENDERER_GL_API_CALL GL_CheckedProgramLocalParameter4fARB(uint32_t target, uint32_t index, float x, float y, float z, float w);
void RENDERER_GL_API_CALL GL_CheckedProgramLocalParameter4dARB(uint32_t target, uint32_t index, double x, double y, double z, double w);
void RENDERER_GL_API_CALL GL_CheckedProgramLocalParameter4fvARB(uint32_t target, uint32_t index, const float *values);
void RENDERER_GL_API_CALL GL_CheckedProgramLocalParameter4dvARB(uint32_t target, uint32_t index, const double *values);
void RENDERER_GL_API_CALL GL_CheckedGetProgramEnvParameterfvARB(uint32_t target, uint32_t index, float *values);
void RENDERER_GL_API_CALL GL_CheckedGetProgramEnvParameterdvARB(uint32_t target, uint32_t index, double *values);
void RENDERER_GL_API_CALL GL_CheckedGetProgramLocalParameterfvARB(uint32_t target, uint32_t index, float *values);
void RENDERER_GL_API_CALL GL_CheckedGetProgramLocalParameterdvARB(uint32_t target, uint32_t index, double *values);
void RENDERER_GL_API_CALL GL_CheckedGetProgramivARB(uint32_t target, uint32_t parameter, int32_t *values);
void RENDERER_GL_API_CALL GL_CheckedGetProgramStringARB(uint32_t target, uint32_t parameter, void *string);
void RENDERER_GL_API_CALL GL_CheckedGetVertexAttribdvARB(uint32_t index, uint32_t parameter, double *values);
void RENDERER_GL_API_CALL GL_CheckedGetVertexAttribfvARB(uint32_t index, uint32_t parameter, float *values);
void RENDERER_GL_API_CALL GL_CheckedGetVertexAttribivARB(uint32_t index, uint32_t parameter, int32_t *values);
void RENDERER_GL_API_CALL GL_CheckedGetVertexAttribPointervARB(uint32_t index, uint32_t parameter, void **pointer);
uint8_t RENDERER_GL_API_CALL GL_CheckedIsProgramARB(uint32_t program);
void RENDERER_GL_API_CALL GL_CheckedAccum(uint32_t operation, float value);
void RENDERER_GL_API_CALL GL_CheckedAlphaFunc(uint32_t function, float reference);
uint8_t RENDERER_GL_API_CALL GL_CheckedAreTexturesResident(int32_t count, const uint32_t *textures, uint8_t *residences);
void RENDERER_GL_API_CALL GL_CheckedArrayElement(int32_t index);
void RENDERER_GL_API_CALL GL_CheckedBegin(uint32_t mode);
void RENDERER_GL_API_CALL GL_CheckedBindTexture(uint32_t target, uint32_t texture);
void RENDERER_GL_API_CALL GL_CheckedBitmap(int32_t width, int32_t height, float xOrigin, float yOrigin, float xMove, float yMove,
                                           const uint8_t *bitmap);
void RENDERER_GL_API_CALL GL_CheckedBlendFunc(uint32_t sourceFactor, uint32_t destinationFactor);
void RENDERER_GL_API_CALL GL_CheckedCallList(uint32_t list);
void RENDERER_GL_API_CALL GL_CheckedCallLists(int32_t count, uint32_t type, const void *lists);
void RENDERER_GL_API_CALL GL_CheckedClear(uint32_t mask);
void RENDERER_GL_API_CALL GL_CheckedClearAccum(float red, float green, float blue, float alpha);
void RENDERER_GL_API_CALL GL_CheckedClearColor(float red, float green, float blue, float alpha);
void RENDERER_GL_API_CALL GL_CheckedClearDepth(double depth);
void RENDERER_GL_API_CALL GL_CheckedClearIndex(float index);
void RENDERER_GL_API_CALL GL_CheckedClearStencil(int32_t stencil);
void RENDERER_GL_API_CALL GL_CheckedClipPlane(uint32_t plane, const double *equation);
void RENDERER_GL_API_CALL GL_CheckedColor3b(int8_t red, int8_t green, int8_t blue);
void RENDERER_GL_API_CALL GL_CheckedColor3bv(const int8_t *values);
void RENDERER_GL_API_CALL GL_CheckedColor3d(double red, double green, double blue);
void RENDERER_GL_API_CALL GL_CheckedColor3dv(const double *values);
void RENDERER_GL_API_CALL GL_CheckedColor3f(float red, float green, float blue);
void RENDERER_GL_API_CALL GL_CheckedColor3fv(const float *values);
void RENDERER_GL_API_CALL GL_CheckedColor3i(int32_t red, int32_t green, int32_t blue);
void RENDERER_GL_API_CALL GL_CheckedColor3iv(const int32_t *values);
void RENDERER_GL_API_CALL GL_CheckedColor3s(int16_t red, int16_t green, int16_t blue);
void RENDERER_GL_API_CALL GL_CheckedColor3sv(const int16_t *values);
void RENDERER_GL_API_CALL GL_CheckedColor3ub(uint8_t red, uint8_t green, uint8_t blue);
void RENDERER_GL_API_CALL GL_CheckedColor3ubv(const uint8_t *values);
void RENDERER_GL_API_CALL GL_CheckedColor3ui(uint32_t red, uint32_t green, uint32_t blue);
void RENDERER_GL_API_CALL GL_CheckedColor3uiv(const uint32_t *values);
void RENDERER_GL_API_CALL GL_CheckedColor3us(uint16_t red, uint16_t green, uint16_t blue);
void RENDERER_GL_API_CALL GL_CheckedColor3usv(const uint16_t *values);
void RENDERER_GL_API_CALL GL_CheckedColor4b(int8_t red, int8_t green, int8_t blue, int8_t alpha);
void RENDERER_GL_API_CALL GL_CheckedColor4bv(const int8_t *values);
void RENDERER_GL_API_CALL GL_CheckedColor4d(double red, double green, double blue, double alpha);
void RENDERER_GL_API_CALL GL_CheckedColor4dv(const double *values);
void RENDERER_GL_API_CALL GL_CheckedColor4f(float red, float green, float blue, float alpha);
void RENDERER_GL_API_CALL GL_CheckedColor4fv(const float *values);
void RENDERER_GL_API_CALL GL_CheckedColor4i(int32_t red, int32_t green, int32_t blue, int32_t alpha);
void RENDERER_GL_API_CALL GL_CheckedColor4iv(const int32_t *values);
void RENDERER_GL_API_CALL GL_CheckedColor4s(int16_t red, int16_t green, int16_t blue, int16_t alpha);
void RENDERER_GL_API_CALL GL_CheckedColor4sv(const int16_t *values);
void RENDERER_GL_API_CALL GL_CheckedColor4ub(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha);
void RENDERER_GL_API_CALL GL_CheckedColor4ubv(const uint8_t *values);
void RENDERER_GL_API_CALL GL_CheckedColor4ui(uint32_t red, uint32_t green, uint32_t blue, uint32_t alpha);
void RENDERER_GL_API_CALL GL_CheckedColor4uiv(const uint32_t *values);
void RENDERER_GL_API_CALL GL_CheckedColor4us(uint16_t red, uint16_t green, uint16_t blue, uint16_t alpha);
void RENDERER_GL_API_CALL GL_CheckedColor4usv(const uint16_t *values);
void RENDERER_GL_API_CALL GL_CheckedColorMask(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha);
void RENDERER_GL_API_CALL GL_CheckedColorMaterial(uint32_t face, uint32_t mode);
void RENDERER_GL_API_CALL GL_CheckedColorPointer(int32_t size, uint32_t type, int32_t stride, const void *pointer);
void RENDERER_GL_API_CALL GL_CheckedCopyPixels(int32_t x, int32_t y, int32_t width, int32_t height, uint32_t type);
void RENDERER_GL_API_CALL GL_CheckedCopyTexImage1D(uint32_t target, int32_t level, uint32_t internalFormat, int32_t x, int32_t y,
                                                   int32_t width, int32_t border);
void RENDERER_GL_API_CALL GL_CheckedCopyTexImage2D(uint32_t target, int32_t level, uint32_t internalFormat, int32_t x, int32_t y,
                                                   int32_t width, int32_t height, int32_t border);
void RENDERER_GL_API_CALL GL_CheckedCopyTexSubImage1D(uint32_t target, int32_t level, int32_t xOffset, int32_t x, int32_t y, int32_t width);
void RENDERER_GL_API_CALL GL_CheckedCopyTexSubImage2D(uint32_t target, int32_t level, int32_t xOffset, int32_t yOffset, int32_t x,
                                                      int32_t y, int32_t width, int32_t height);
void RENDERER_GL_API_CALL GL_CheckedCullFace(uint32_t mode);
void RENDERER_GL_API_CALL GL_CheckedDeleteLists(uint32_t list, int32_t range);
void RENDERER_GL_API_CALL GL_CheckedDeleteTextures(int32_t count, const uint32_t *textures);
void RENDERER_GL_API_CALL GL_CheckedDepthFunc(uint32_t function);
void RENDERER_GL_API_CALL GL_CheckedDepthMask(uint8_t enabled);
void RENDERER_GL_API_CALL GL_CheckedDepthRange(double nearValue, double farValue);
void RENDERER_GL_API_CALL GL_CheckedDisable(uint32_t capability);
void RENDERER_GL_API_CALL GL_CheckedDisableClientState(uint32_t array);
void RENDERER_GL_API_CALL GL_CheckedDrawArrays(uint32_t mode, int32_t first, int32_t count);
void RENDERER_GL_API_CALL GL_CheckedDrawBuffer(uint32_t buffer);
void RENDERER_GL_API_CALL GL_CheckedDrawElements(uint32_t mode, int32_t count, uint32_t type, const void *indices);
void RENDERER_GL_API_CALL GL_CheckedDrawPixels(int32_t width, int32_t height, uint32_t format, uint32_t type, const void *pixels);
void RENDERER_GL_API_CALL GL_CheckedEdgeFlag(uint8_t flag);
void RENDERER_GL_API_CALL GL_CheckedEdgeFlagPointer(int32_t stride, const void *pointer);
void RENDERER_GL_API_CALL GL_CheckedEdgeFlagv(const uint8_t *flag);
void RENDERER_GL_API_CALL GL_CheckedEnable(uint32_t capability);
void RENDERER_GL_API_CALL GL_CheckedEnableClientState(uint32_t array);
void RENDERER_GL_API_CALL GL_CheckedEnd(void);
void RENDERER_GL_API_CALL GL_CheckedEndList(void);
void RENDERER_GL_API_CALL GL_CheckedEvalCoord1d(double u);
void RENDERER_GL_API_CALL GL_CheckedEvalCoord1dv(const double *u);
void RENDERER_GL_API_CALL GL_CheckedEvalCoord1f(float u);
void RENDERER_GL_API_CALL GL_CheckedEvalCoord1fv(const float *u);
void RENDERER_GL_API_CALL GL_CheckedEvalCoord2d(double u, double v);
void RENDERER_GL_API_CALL GL_CheckedEvalCoord2dv(const double *values);
void RENDERER_GL_API_CALL GL_CheckedEvalCoord2f(float u, float v);
void RENDERER_GL_API_CALL GL_CheckedEvalCoord2fv(const float *values);
void RENDERER_GL_API_CALL GL_CheckedEvalMesh1(uint32_t mode, int32_t i1, int32_t i2);
void RENDERER_GL_API_CALL GL_CheckedEvalMesh2(uint32_t mode, int32_t i1, int32_t i2, int32_t j1, int32_t j2);
void RENDERER_GL_API_CALL GL_CheckedEvalPoint1(int32_t i);
void RENDERER_GL_API_CALL GL_CheckedEvalPoint2(int32_t i, int32_t j);
void RENDERER_GL_API_CALL GL_CheckedFeedbackBuffer(int32_t size, uint32_t type, float *buffer);
void RENDERER_GL_API_CALL GL_CheckedFinish(void);
void RENDERER_GL_API_CALL GL_CheckedFlush(void);
void RENDERER_GL_API_CALL GL_CheckedFogf(uint32_t parameter, float value);
void RENDERER_GL_API_CALL GL_CheckedFogfv(uint32_t parameter, const float *values);
void RENDERER_GL_API_CALL GL_CheckedFogi(uint32_t parameter, int32_t value);
void RENDERER_GL_API_CALL GL_CheckedFogiv(uint32_t parameter, const int32_t *values);
void RENDERER_GL_API_CALL GL_CheckedFrontFace(uint32_t mode);
void RENDERER_GL_API_CALL GL_CheckedFrustum(double left, double right, double bottom, double top, double nearValue, double farValue);
uint32_t RENDERER_GL_API_CALL GL_CheckedGenLists(int32_t range);
void RENDERER_GL_API_CALL GL_CheckedGenTextures(int32_t count, uint32_t *textures);
void RENDERER_GL_API_CALL GL_CheckedGetBooleanv(uint32_t parameter, uint8_t *values);
void RENDERER_GL_API_CALL GL_CheckedGetClipPlane(uint32_t plane, double *equation);
void RENDERER_GL_API_CALL GL_CheckedGetDoublev(uint32_t parameter, double *values);
uint32_t RENDERER_GL_API_CALL GL_CheckedGetError(void);
void RENDERER_GL_API_CALL GL_CheckedGetFloatv(uint32_t parameter, float *values);
void RENDERER_GL_API_CALL GL_CheckedGetIntegerv(uint32_t parameter, int32_t *values);
void RENDERER_GL_API_CALL GL_CheckedGetLightfv(uint32_t light, uint32_t parameter, float *values);
void RENDERER_GL_API_CALL GL_CheckedGetLightiv(uint32_t light, uint32_t parameter, int32_t *values);
void RENDERER_GL_API_CALL GL_CheckedGetMapdv(uint32_t target, uint32_t query, double *values);
void RENDERER_GL_API_CALL GL_CheckedGetMapfv(uint32_t target, uint32_t query, float *values);
void RENDERER_GL_API_CALL GL_CheckedGetMapiv(uint32_t target, uint32_t query, int32_t *values);
void RENDERER_GL_API_CALL GL_CheckedGetMaterialfv(uint32_t face, uint32_t parameter, float *values);
void RENDERER_GL_API_CALL GL_CheckedGetMaterialiv(uint32_t face, uint32_t parameter, int32_t *values);
void RENDERER_GL_API_CALL GL_CheckedGetPixelMapfv(uint32_t map, float *values);
void RENDERER_GL_API_CALL GL_CheckedGetPixelMapuiv(uint32_t map, uint32_t *values);
void RENDERER_GL_API_CALL GL_CheckedGetPixelMapusv(uint32_t map, uint16_t *values);
void RENDERER_GL_API_CALL GL_CheckedGetPointerv(uint32_t parameter, void **value);
void RENDERER_GL_API_CALL GL_CheckedGetPolygonStipple(uint8_t *mask);
const uint8_t *RENDERER_GL_API_CALL GL_CheckedGetString(uint32_t name);
void RENDERER_GL_API_CALL GL_CheckedGetTexEnvfv(uint32_t target, uint32_t parameter, float *values);
void RENDERER_GL_API_CALL GL_CheckedGetTexEnviv(uint32_t target, uint32_t parameter, int32_t *values);
void RENDERER_GL_API_CALL GL_CheckedGetTexGendv(uint32_t coordinate, uint32_t parameter, double *values);
void RENDERER_GL_API_CALL GL_CheckedGetTexGenfv(uint32_t coordinate, uint32_t parameter, float *values);
void RENDERER_GL_API_CALL GL_CheckedGetTexGeniv(uint32_t coordinate, uint32_t parameter, int32_t *values);
void RENDERER_GL_API_CALL GL_CheckedGetTexImage(uint32_t target, int32_t level, uint32_t format, uint32_t type, void *pixels);
void RENDERER_GL_API_CALL GL_CheckedGetTexLevelParameterfv(uint32_t target, int32_t level, uint32_t parameter, float *values);
void RENDERER_GL_API_CALL GL_CheckedGetTexLevelParameteriv(uint32_t target, int32_t level, uint32_t parameter, int32_t *values);
void RENDERER_GL_API_CALL GL_CheckedGetTexParameterfv(uint32_t target, uint32_t parameter, float *values);
void RENDERER_GL_API_CALL GL_CheckedGetTexParameteriv(uint32_t target, uint32_t parameter, int32_t *values);
void RENDERER_GL_API_CALL GL_CheckedHint(uint32_t target, uint32_t mode);
void RENDERER_GL_API_CALL GL_CheckedIndexMask(uint32_t mask);
void RENDERER_GL_API_CALL GL_CheckedIndexPointer(uint32_t type, int32_t stride, const void *pointer);
void RENDERER_GL_API_CALL GL_CheckedIndexd(double index);
void RENDERER_GL_API_CALL GL_CheckedIndexdv(const double *index);
void RENDERER_GL_API_CALL GL_CheckedIndexf(float index);
void RENDERER_GL_API_CALL GL_CheckedIndexfv(const float *index);
void RENDERER_GL_API_CALL GL_CheckedIndexi(int32_t index);
void RENDERER_GL_API_CALL GL_CheckedIndexiv(const int32_t *index);
void RENDERER_GL_API_CALL GL_CheckedIndexs(int16_t index);
void RENDERER_GL_API_CALL GL_CheckedIndexsv(const int16_t *index);
void RENDERER_GL_API_CALL GL_CheckedIndexub(uint8_t index);
void RENDERER_GL_API_CALL GL_CheckedIndexubv(const uint8_t *index);
void RENDERER_GL_API_CALL GL_CheckedInitNames(void);
void RENDERER_GL_API_CALL GL_CheckedInterleavedArrays(uint32_t format, int32_t stride, const void *pointer);
uint8_t RENDERER_GL_API_CALL GL_CheckedIsEnabled(uint32_t capability);
uint8_t RENDERER_GL_API_CALL GL_CheckedIsList(uint32_t list);
uint8_t RENDERER_GL_API_CALL GL_CheckedIsTexture(uint32_t texture);
void RENDERER_GL_API_CALL GL_CheckedLightModelf(uint32_t parameter, float value);
void RENDERER_GL_API_CALL GL_CheckedLightModelfv(uint32_t parameter, const float *values);
void RENDERER_GL_API_CALL GL_CheckedLightModeli(uint32_t parameter, int32_t value);
void RENDERER_GL_API_CALL GL_CheckedLightModeliv(uint32_t parameter, const int32_t *values);
void RENDERER_GL_API_CALL GL_CheckedLightf(uint32_t light, uint32_t parameter, float value);
void RENDERER_GL_API_CALL GL_CheckedLightfv(uint32_t light, uint32_t parameter, const float *values);
void RENDERER_GL_API_CALL GL_CheckedLighti(uint32_t light, uint32_t parameter, int32_t value);
void RENDERER_GL_API_CALL GL_CheckedLightiv(uint32_t light, uint32_t parameter, const int32_t *values);
void RENDERER_GL_API_CALL GL_CheckedLineStipple(int32_t factor, uint16_t pattern);
void RENDERER_GL_API_CALL GL_CheckedLineWidth(float width);
void RENDERER_GL_API_CALL GL_CheckedListBase(uint32_t base);
void RENDERER_GL_API_CALL GL_CheckedLoadIdentity(void);
void RENDERER_GL_API_CALL GL_CheckedLoadMatrixd(const double *matrix);
void RENDERER_GL_API_CALL GL_CheckedLoadMatrixf(const float *matrix);
void RENDERER_GL_API_CALL GL_CheckedLoadName(uint32_t name);
void RENDERER_GL_API_CALL GL_CheckedLogicOp(uint32_t operation);
void RENDERER_GL_API_CALL GL_CheckedMap1d(uint32_t target, double u1, double u2, int32_t stride, int32_t order, const double *points);
void RENDERER_GL_API_CALL GL_CheckedMap1f(uint32_t target, float u1, float u2, int32_t stride, int32_t order, const float *points);
void RENDERER_GL_API_CALL GL_CheckedMap2d(uint32_t target, double u1, double u2, int32_t uStride, int32_t uOrder, double v1, double v2,
                                          int32_t vStride, int32_t vOrder, const double *points);
void RENDERER_GL_API_CALL GL_CheckedMap2f(uint32_t target, float u1, float u2, int32_t uStride, int32_t uOrder, float v1, float v2,
                                          int32_t vStride, int32_t vOrder, const float *points);
void RENDERER_GL_API_CALL GL_CheckedMapGrid1d(int32_t count, double u1, double u2);
void RENDERER_GL_API_CALL GL_CheckedMapGrid1f(int32_t count, float u1, float u2);
void RENDERER_GL_API_CALL GL_CheckedMapGrid2d(int32_t uCount, double u1, double u2, int32_t vCount, double v1, double v2);
void RENDERER_GL_API_CALL GL_CheckedMapGrid2f(int32_t uCount, float u1, float u2, int32_t vCount, float v1, float v2);
void RENDERER_GL_API_CALL GL_CheckedMaterialf(uint32_t face, uint32_t parameter, float value);
void RENDERER_GL_API_CALL GL_CheckedMaterialfv(uint32_t face, uint32_t parameter, const float *values);
void RENDERER_GL_API_CALL GL_CheckedMateriali(uint32_t face, uint32_t parameter, int32_t value);
void RENDERER_GL_API_CALL GL_CheckedMaterialiv(uint32_t face, uint32_t parameter, const int32_t *values);
void RENDERER_GL_API_CALL GL_CheckedMatrixMode(uint32_t mode);
void RENDERER_GL_API_CALL GL_CheckedMultMatrixd(const double *matrix);
void RENDERER_GL_API_CALL GL_CheckedMultMatrixf(const float *matrix);
void RENDERER_GL_API_CALL GL_CheckedNewList(uint32_t list, uint32_t mode);
void RENDERER_GL_API_CALL GL_CheckedNormal3b(int8_t x, int8_t y, int8_t z);
void RENDERER_GL_API_CALL GL_CheckedNormal3bv(const int8_t *values);
void RENDERER_GL_API_CALL GL_CheckedNormal3d(double x, double y, double z);
void RENDERER_GL_API_CALL GL_CheckedNormal3dv(const double *values);
void RENDERER_GL_API_CALL GL_CheckedNormal3f(float x, float y, float z);
void RENDERER_GL_API_CALL GL_CheckedNormal3fv(const float *values);
void RENDERER_GL_API_CALL GL_CheckedNormal3i(int32_t x, int32_t y, int32_t z);
void RENDERER_GL_API_CALL GL_CheckedNormal3iv(const int32_t *values);
void RENDERER_GL_API_CALL GL_CheckedNormal3s(int16_t x, int16_t y, int16_t z);
void RENDERER_GL_API_CALL GL_CheckedNormal3sv(const int16_t *values);
void RENDERER_GL_API_CALL GL_CheckedNormalPointer(uint32_t type, int32_t stride, const void *pointer);
void RENDERER_GL_API_CALL GL_CheckedOrtho(double left, double right, double bottom, double top, double nearValue, double farValue);
void RENDERER_GL_API_CALL GL_CheckedPassThrough(float token);
void RENDERER_GL_API_CALL GL_CheckedPixelMapfv(uint32_t map, int32_t mapSize, const float *values);
void RENDERER_GL_API_CALL GL_CheckedPixelMapuiv(uint32_t map, int32_t mapSize, const uint32_t *values);
void RENDERER_GL_API_CALL GL_CheckedPixelMapusv(uint32_t map, int32_t mapSize, const uint16_t *values);
void RENDERER_GL_API_CALL GL_CheckedPixelStoref(uint32_t parameter, float value);
void RENDERER_GL_API_CALL GL_CheckedPixelStorei(uint32_t parameter, int32_t value);
void RENDERER_GL_API_CALL GL_CheckedPixelTransferf(uint32_t parameter, float value);
void RENDERER_GL_API_CALL GL_CheckedPixelTransferi(uint32_t parameter, int32_t value);
void RENDERER_GL_API_CALL GL_CheckedPixelZoom(float xFactor, float yFactor);
void RENDERER_GL_API_CALL GL_CheckedPointSize(float size);
void RENDERER_GL_API_CALL GL_CheckedPolygonMode(uint32_t face, uint32_t mode);
void RENDERER_GL_API_CALL GL_CheckedPolygonOffset(float factor, float units);
void RENDERER_GL_API_CALL GL_CheckedPolygonStipple(const uint8_t *mask);
void RENDERER_GL_API_CALL GL_CheckedPopAttrib(void);
void RENDERER_GL_API_CALL GL_CheckedPopClientAttrib(void);
void RENDERER_GL_API_CALL GL_CheckedPopMatrix(void);
void RENDERER_GL_API_CALL GL_CheckedPopName(void);
void RENDERER_GL_API_CALL GL_CheckedPrioritizeTextures(int32_t count, const uint32_t *textures, const float *priorities);
void RENDERER_GL_API_CALL GL_CheckedPushAttrib(uint32_t mask);
void RENDERER_GL_API_CALL GL_CheckedPushClientAttrib(uint32_t mask);
void RENDERER_GL_API_CALL GL_CheckedPushMatrix(void);
void RENDERER_GL_API_CALL GL_CheckedPushName(uint32_t name);
void RENDERER_GL_API_CALL GL_CheckedRasterPos2d(double x, double y);
void RENDERER_GL_API_CALL GL_CheckedRasterPos2dv(const double *values);
void RENDERER_GL_API_CALL GL_CheckedRasterPos2f(float x, float y);
void RENDERER_GL_API_CALL GL_CheckedRasterPos2fv(const float *values);
void RENDERER_GL_API_CALL GL_CheckedRasterPos2i(int32_t x, int32_t y);
void RENDERER_GL_API_CALL GL_CheckedRasterPos2iv(const int32_t *values);
void RENDERER_GL_API_CALL GL_CheckedRasterPos2s(int16_t x, int16_t y);
void RENDERER_GL_API_CALL GL_CheckedRasterPos2sv(const int16_t *values);
void RENDERER_GL_API_CALL GL_CheckedRasterPos3d(double x, double y, double z);
void RENDERER_GL_API_CALL GL_CheckedRasterPos3dv(const double *values);
void RENDERER_GL_API_CALL GL_CheckedRasterPos3f(float x, float y, float z);
void RENDERER_GL_API_CALL GL_CheckedRasterPos3fv(const float *values);
void RENDERER_GL_API_CALL GL_CheckedRasterPos3i(int32_t x, int32_t y, int32_t z);
void RENDERER_GL_API_CALL GL_CheckedRasterPos3iv(const int32_t *values);
void RENDERER_GL_API_CALL GL_CheckedRasterPos3s(int16_t x, int16_t y, int16_t z);
void RENDERER_GL_API_CALL GL_CheckedRasterPos3sv(const int16_t *values);
void RENDERER_GL_API_CALL GL_CheckedRasterPos4d(double x, double y, double z, double w);
void RENDERER_GL_API_CALL GL_CheckedRasterPos4dv(const double *values);
void RENDERER_GL_API_CALL GL_CheckedRasterPos4f(float x, float y, float z, float w);
void RENDERER_GL_API_CALL GL_CheckedRasterPos4fv(const float *values);
void RENDERER_GL_API_CALL GL_CheckedRasterPos4i(int32_t x, int32_t y, int32_t z, int32_t w);
void RENDERER_GL_API_CALL GL_CheckedRasterPos4iv(const int32_t *values);
void RENDERER_GL_API_CALL GL_CheckedRasterPos4s(int16_t x, int16_t y, int16_t z, int16_t w);
void RENDERER_GL_API_CALL GL_CheckedRasterPos4sv(const int16_t *values);
void RENDERER_GL_API_CALL GL_CheckedReadBuffer(uint32_t mode);
void RENDERER_GL_API_CALL GL_CheckedReadPixels(int32_t x, int32_t y, int32_t width, int32_t height, uint32_t format, uint32_t type,
                                               void *pixels);
void RENDERER_GL_API_CALL GL_CheckedRectd(double x1, double y1, double x2, double y2);
void RENDERER_GL_API_CALL GL_CheckedRectdv(const double *vertex1, const double *vertex2);
void RENDERER_GL_API_CALL GL_CheckedRectf(float x1, float y1, float x2, float y2);
void RENDERER_GL_API_CALL GL_CheckedRectfv(const float *vertex1, const float *vertex2);
void RENDERER_GL_API_CALL GL_CheckedRecti(int32_t x1, int32_t y1, int32_t x2, int32_t y2);
void RENDERER_GL_API_CALL GL_CheckedRectiv(const int32_t *vertex1, const int32_t *vertex2);
void RENDERER_GL_API_CALL GL_CheckedRects(int16_t x1, int16_t y1, int16_t x2, int16_t y2);
void RENDERER_GL_API_CALL GL_CheckedRectsv(const int16_t *vertex1, const int16_t *vertex2);
int32_t RENDERER_GL_API_CALL GL_CheckedRenderMode(uint32_t mode);
void RENDERER_GL_API_CALL GL_CheckedRotated(double angle, double x, double y, double z);
void RENDERER_GL_API_CALL GL_CheckedRotatef(float angle, float x, float y, float z);
void RENDERER_GL_API_CALL GL_CheckedScaled(double x, double y, double z);
void RENDERER_GL_API_CALL GL_CheckedScalef(float x, float y, float z);
void RENDERER_GL_API_CALL GL_CheckedScissor(int32_t x, int32_t y, int32_t width, int32_t height);
void RENDERER_GL_API_CALL GL_CheckedSelectBuffer(int32_t size, uint32_t *buffer);
void RENDERER_GL_API_CALL GL_CheckedShadeModel(uint32_t mode);
void RENDERER_GL_API_CALL GL_CheckedStencilFunc(uint32_t function, int32_t reference, uint32_t mask);
void RENDERER_GL_API_CALL GL_CheckedStencilMask(uint32_t mask);
void RENDERER_GL_API_CALL GL_CheckedStencilOp(uint32_t stencilFail, uint32_t depthFail, uint32_t depthPass);
void RENDERER_GL_API_CALL GL_CheckedTexCoord1d(double s);
void RENDERER_GL_API_CALL GL_CheckedTexCoord1dv(const double *values);
void RENDERER_GL_API_CALL GL_CheckedTexCoord1f(float s);
void RENDERER_GL_API_CALL GL_CheckedTexCoord1fv(const float *values);
void RENDERER_GL_API_CALL GL_CheckedTexCoord1i(int32_t s);
void RENDERER_GL_API_CALL GL_CheckedTexCoord1iv(const int32_t *values);
void RENDERER_GL_API_CALL GL_CheckedTexCoord1s(int16_t s);
void RENDERER_GL_API_CALL GL_CheckedTexCoord1sv(const int16_t *values);
void RENDERER_GL_API_CALL GL_CheckedTexCoord2d(double s, double t);
void RENDERER_GL_API_CALL GL_CheckedTexCoord2dv(const double *values);
void RENDERER_GL_API_CALL GL_CheckedTexCoord2f(float s, float t);
void RENDERER_GL_API_CALL GL_CheckedTexCoord2fv(const float *values);
void RENDERER_GL_API_CALL GL_CheckedTexCoord2i(int32_t s, int32_t t);
void RENDERER_GL_API_CALL GL_CheckedTexCoord2iv(const int32_t *values);
void RENDERER_GL_API_CALL GL_CheckedTexCoord2s(int16_t s, int16_t t);
void RENDERER_GL_API_CALL GL_CheckedTexCoord2sv(const int16_t *values);
void RENDERER_GL_API_CALL GL_CheckedTexCoord3d(double s, double t, double r);
void RENDERER_GL_API_CALL GL_CheckedTexCoord3dv(const double *values);
void RENDERER_GL_API_CALL GL_CheckedTexCoord3f(float s, float t, float r);
void RENDERER_GL_API_CALL GL_CheckedTexCoord3fv(const float *values);
void RENDERER_GL_API_CALL GL_CheckedTexCoord3i(int32_t s, int32_t t, int32_t r);
void RENDERER_GL_API_CALL GL_CheckedTexCoord3iv(const int32_t *values);
void RENDERER_GL_API_CALL GL_CheckedTexCoord3s(int16_t s, int16_t t, int16_t r);
void RENDERER_GL_API_CALL GL_CheckedTexCoord3sv(const int16_t *values);
void RENDERER_GL_API_CALL GL_CheckedTexCoord4d(double s, double t, double r, double q);
void RENDERER_GL_API_CALL GL_CheckedTexCoord4dv(const double *values);
void RENDERER_GL_API_CALL GL_CheckedTexCoord4f(float s, float t, float r, float q);
void RENDERER_GL_API_CALL GL_CheckedTexCoord4fv(const float *values);
void RENDERER_GL_API_CALL GL_CheckedTexCoord4i(int32_t s, int32_t t, int32_t r, int32_t q);
void RENDERER_GL_API_CALL GL_CheckedTexCoord4iv(const int32_t *values);
void RENDERER_GL_API_CALL GL_CheckedTexCoord4s(int16_t s, int16_t t, int16_t r, int16_t q);
void RENDERER_GL_API_CALL GL_CheckedTexCoord4sv(const int16_t *values);
void RENDERER_GL_API_CALL GL_CheckedTexCoordPointer(int32_t size, uint32_t type, int32_t stride, const void *pointer);
void RENDERER_GL_API_CALL GL_CheckedTexEnvf(uint32_t target, uint32_t parameter, float value);
void RENDERER_GL_API_CALL GL_CheckedTexEnvfv(uint32_t target, uint32_t parameter, const float *values);
void RENDERER_GL_API_CALL GL_CheckedTexEnvi(uint32_t target, uint32_t parameter, int32_t value);
void RENDERER_GL_API_CALL GL_CheckedTexEnviv(uint32_t target, uint32_t parameter, const int32_t *values);
void RENDERER_GL_API_CALL GL_CheckedTexGend(uint32_t coordinate, uint32_t parameter, double value);
void RENDERER_GL_API_CALL GL_CheckedTexGendv(uint32_t coordinate, uint32_t parameter, const double *values);
void RENDERER_GL_API_CALL GL_CheckedTexGenf(uint32_t coordinate, uint32_t parameter, float value);
void RENDERER_GL_API_CALL GL_CheckedTexGenfv(uint32_t coordinate, uint32_t parameter, const float *values);
void RENDERER_GL_API_CALL GL_CheckedTexGeni(uint32_t coordinate, uint32_t parameter, int32_t value);
void RENDERER_GL_API_CALL GL_CheckedTexGeniv(uint32_t coordinate, uint32_t parameter, const int32_t *values);
void RENDERER_GL_API_CALL GL_CheckedTexImage1D(uint32_t target, int32_t level, int32_t internalFormat, int32_t width, int32_t border,
                                               uint32_t format, uint32_t type, const void *pixels);
void RENDERER_GL_API_CALL GL_CheckedTexImage2D(uint32_t target, int32_t level, int32_t internalFormat, int32_t width, int32_t height,
                                               int32_t border, uint32_t format, uint32_t type, const void *pixels);
void RENDERER_GL_API_CALL GL_CheckedTexParameterf(uint32_t target, uint32_t parameter, float value);
void RENDERER_GL_API_CALL GL_CheckedTexParameterfv(uint32_t target, uint32_t parameter, const float *values);
void RENDERER_GL_API_CALL GL_CheckedTexParameteri(uint32_t target, uint32_t parameter, int32_t value);
void RENDERER_GL_API_CALL GL_CheckedTexParameteriv(uint32_t target, uint32_t parameter, const int32_t *values);
void RENDERER_GL_API_CALL GL_CheckedTexSubImage1D(uint32_t target, int32_t level, int32_t xOffset, int32_t width, uint32_t format,
                                                  uint32_t type, const void *pixels);
void RENDERER_GL_API_CALL GL_CheckedTexSubImage2D(uint32_t target, int32_t level, int32_t xOffset, int32_t yOffset, int32_t width,
                                                  int32_t height, uint32_t format, uint32_t type, const void *pixels);
void RENDERER_GL_API_CALL GL_CheckedTranslated(double x, double y, double z);
void RENDERER_GL_API_CALL GL_CheckedTranslatef(float x, float y, float z);
void RENDERER_GL_API_CALL GL_CheckedVertex2d(double x, double y);
void RENDERER_GL_API_CALL GL_CheckedVertex2dv(const double *values);
void RENDERER_GL_API_CALL GL_CheckedVertex2f(float x, float y);
void RENDERER_GL_API_CALL GL_CheckedVertex2fv(const float *values);
void RENDERER_GL_API_CALL GL_CheckedVertex2i(int32_t x, int32_t y);
void RENDERER_GL_API_CALL GL_CheckedVertex2iv(const int32_t *values);
void RENDERER_GL_API_CALL GL_CheckedVertex2s(int16_t x, int16_t y);
void RENDERER_GL_API_CALL GL_CheckedVertex2sv(const int16_t *values);
void RENDERER_GL_API_CALL GL_CheckedVertex3d(double x, double y, double z);
void RENDERER_GL_API_CALL GL_CheckedVertex3dv(const double *values);
void RENDERER_GL_API_CALL GL_CheckedVertex3f(float x, float y, float z);
void RENDERER_GL_API_CALL GL_CheckedVertex3fv(const float *values);
void RENDERER_GL_API_CALL GL_CheckedVertex3i(int32_t x, int32_t y, int32_t z);
void RENDERER_GL_API_CALL GL_CheckedVertex3iv(const int32_t *values);
void RENDERER_GL_API_CALL GL_CheckedVertex3s(int16_t x, int16_t y, int16_t z);
void RENDERER_GL_API_CALL GL_CheckedVertex3sv(const int16_t *values);
void RENDERER_GL_API_CALL GL_CheckedVertex4d(double x, double y, double z, double w);
void RENDERER_GL_API_CALL GL_CheckedVertex4dv(const double *values);
void RENDERER_GL_API_CALL GL_CheckedVertex4f(float x, float y, float z, float w);
void RENDERER_GL_API_CALL GL_CheckedVertex4fv(const float *values);
void RENDERER_GL_API_CALL GL_CheckedVertex4i(int32_t x, int32_t y, int32_t z, int32_t w);
void RENDERER_GL_API_CALL GL_CheckedVertex4iv(const int32_t *values);
void RENDERER_GL_API_CALL GL_CheckedVertex4s(int16_t x, int16_t y, int16_t z, int16_t w);
void RENDERER_GL_API_CALL GL_CheckedVertex4sv(const int16_t *values);
void RENDERER_GL_API_CALL GL_CheckedVertexPointer(int32_t size, uint32_t type, int32_t stride, const void *pointer);
void RENDERER_GL_API_CALL GL_CheckedViewport(int32_t x, int32_t y, int32_t width, int32_t height);

#ifdef __cplusplus
}
#endif

#endif
