#ifndef CODUOMP_RENDERER_GL_DEBUG_H
#define CODUOMP_RENDERER_GL_DEBUG_H

#include <stdio.h>
#include <stdint.h>

#include "gl_api.h"

/* The debug wrappers are installed in front of the actual platform OpenGL
 * entry points. The logging controller owns the stream lifetime. */
extern FILE *rendererGlLogFile;
#define QGL_GL_ENTRY(type_, name_) extern type_ rendererGl##name_##Driver;
#include "qgl_gl_entries.h"
#undef QGL_GL_ENTRY

#ifdef __cplusplus
extern "C" {
#endif

void RENDERER_GL_API_CALL GL_LogAlphaFunc(uint32_t func, float reference);
void RENDERER_GL_API_CALL GL_LogBegin(uint32_t mode);
void RENDERER_GL_API_CALL GL_LogBindTexture(uint32_t target, uint32_t texture);
void RENDERER_GL_API_CALL GL_LogBindProgramARB(uint32_t target, uint32_t program);
void RENDERER_GL_API_CALL GL_LogBindFragmentShaderATI(uint32_t shader);
void RENDERER_GL_API_CALL GL_LogBindBufferARB(uint32_t target, uint32_t buffer);
void RENDERER_GL_API_CALL GL_LogBufferDataARB(uint32_t target, intptr_t size, const void *data, uint32_t usage);
void *RENDERER_GL_API_CALL GL_LogMapBufferARB(uint32_t target, uint32_t access);
uint8_t RENDERER_GL_API_CALL GL_LogUnmapBufferARB(uint32_t target);
void RENDERER_GL_API_CALL GL_LogBlendFunc(uint32_t sourceFactor, uint32_t destinationFactor);
void RENDERER_GL_API_CALL GL_LogCallList(uint32_t list);
void RENDERER_GL_API_CALL GL_LogClear(uint32_t mask);
void RENDERER_GL_API_CALL GL_LogClearDepth(double depth);
void RENDERER_GL_API_CALL GL_LogClearStencil(int32_t stencil);
void RENDERER_GL_API_CALL GL_LogColor4f(float red, float green, float blue, float alpha);
void RENDERER_GL_API_CALL GL_LogColor4fv(const float *color);
void RENDERER_GL_API_CALL GL_LogColorPointer(int32_t size, uint32_t type, int32_t stride, const void *pointer);
void RENDERER_GL_API_CALL GL_LogCullFace(uint32_t mode);
void RENDERER_GL_API_CALL GL_LogDepthFunc(uint32_t func);
void RENDERER_GL_API_CALL GL_LogDepthMask(uint8_t enabled);
void RENDERER_GL_API_CALL GL_LogDepthRange(double nearValue, double farValue);
void RENDERER_GL_API_CALL GL_LogDisable(uint32_t capability);
void RENDERER_GL_API_CALL GL_LogDisableClientState(uint32_t capability);
void RENDERER_GL_API_CALL GL_LogEnable(uint32_t capability);
void RENDERER_GL_API_CALL GL_LogDrawElements(uint32_t mode, int32_t count, uint32_t type, const void *indices);
void RENDERER_GL_API_CALL GL_LogDrawRangeElementsEXT(uint32_t mode, uint32_t start, uint32_t end, int32_t count, uint32_t type,
                                                     const void *indices);
void RENDERER_GL_API_CALL GL_LogDrawBuffer(uint32_t buffer);
void RENDERER_GL_API_CALL GL_LogEnableClientState(uint32_t capability);
void RENDERER_GL_API_CALL GL_LogHint(uint32_t target, uint32_t mode);
const char *GL_FogParameterToString(uint32_t parameter);
void RENDERER_GL_API_CALL GL_LogFogf(uint32_t parameter, float value);
void RENDERER_GL_API_CALL GL_LogFogfv(uint32_t parameter, const float *values);
void RENDERER_GL_API_CALL GL_LogFogi(uint32_t parameter, int32_t value);
void RENDERER_GL_API_CALL GL_LogPolygonMode(uint32_t face, uint32_t mode);
void RENDERER_GL_API_CALL GL_LogScissor(int32_t x, int32_t y, int32_t width, int32_t height);
void RENDERER_GL_API_CALL GL_LogTexCoordPointer(int32_t size, uint32_t type, int32_t stride, const void *pointer);
void RENDERER_GL_API_CALL GL_LogTexEnvf(uint32_t target, uint32_t parameter, float value);
void RENDERER_GL_API_CALL GL_LogTexEnvi(uint32_t target, uint32_t parameter, int32_t value);
void RENDERER_GL_API_CALL GL_LogTexParameterf(uint32_t target, uint32_t parameter, float value);
void RENDERER_GL_API_CALL GL_LogTexParameteri(uint32_t target, uint32_t parameter, int32_t value);
void RENDERER_GL_API_CALL GL_LogNormalPointer(uint32_t type, int32_t stride, const void *pointer);
void RENDERER_GL_API_CALL GL_LogVertexPointer(int32_t size, uint32_t type, int32_t stride, const void *pointer);
void RENDERER_GL_API_CALL GL_LogViewport(int32_t x, int32_t y, int32_t width, int32_t height);
void RENDERER_GL_API_CALL GL_LogSetFenceNV(uint32_t fence, uint32_t condition);
void RENDERER_GL_API_CALL GL_LogFinishFenceNV(uint32_t fence);
void RENDERER_GL_API_CALL GL_LogActiveTextureARB(uint32_t texture);
void RENDERER_GL_API_CALL GL_LogClientActiveTextureARB(uint32_t texture);
void RENDERER_GL_API_CALL GL_LogMultiTexCoord2fARB(uint32_t target, float s, float t);
void RENDERER_GL_API_CALL GL_LogLockArraysEXT(int32_t first, int32_t count);
void RENDERER_GL_API_CALL GL_LogUnlockArraysEXT(void);
void RENDERER_GL_API_CALL GL_LogPNTrianglesiATI(uint32_t parameter, int32_t value);
void RENDERER_GL_API_CALL GL_LogPNTrianglesfATI(uint32_t parameter, float value);
void RENDERER_GL_API_CALL GL_LogCompressedTexImage3DARB(uint32_t target, int32_t level, uint32_t internalFormat, int32_t width,
                                                        int32_t height, int32_t depth, int32_t border, int32_t imageSize, const void *data);
void RENDERER_GL_API_CALL GL_LogCompressedTexImage2DARB(uint32_t target, int32_t level, uint32_t internalFormat, int32_t width,
                                                        int32_t height, int32_t border, int32_t imageSize, const void *data);
void RENDERER_GL_API_CALL GL_LogCompressedTexImage1DARB(uint32_t target, int32_t level, uint32_t internalFormat, int32_t width,
                                                        int32_t border, int32_t imageSize, const void *data);
void RENDERER_GL_API_CALL GL_LogCompressedTexSubImage3DARB(uint32_t target, int32_t level, int32_t xOffset, int32_t yOffset,
                                                           int32_t zOffset, int32_t width, int32_t height, int32_t depth, uint32_t format,
                                                           int32_t imageSize, const void *data);
void RENDERER_GL_API_CALL GL_LogCompressedTexSubImage2DARB(uint32_t target, int32_t level, int32_t xOffset, int32_t yOffset, int32_t width,
                                                           int32_t height, uint32_t format, int32_t imageSize, const void *data);
void RENDERER_GL_API_CALL GL_LogCompressedTexSubImage1DARB(uint32_t target, int32_t level, int32_t xOffset, int32_t width, uint32_t format,
                                                           int32_t imageSize, const void *data);
void RENDERER_GL_API_CALL GL_LogGetCompressedTexImageARB(uint32_t target, int32_t level, void *image);
void RENDERER_GL_API_CALL GL_LogDeleteBuffersARB(int32_t count, const uint32_t *buffers);
void RENDERER_GL_API_CALL GL_LogGenBuffersARB(int32_t count, uint32_t *buffers);
uint8_t RENDERER_GL_API_CALL GL_LogIsBufferARB(uint32_t buffer);
void RENDERER_GL_API_CALL GL_LogBufferSubDataARB(uint32_t target, intptr_t offset, intptr_t size, const void *data);
void RENDERER_GL_API_CALL GL_LogGetBufferSubDataARB(uint32_t target, intptr_t offset, intptr_t size, void *data);
void RENDERER_GL_API_CALL GL_LogGetBufferParameterivARB(uint32_t target, uint32_t parameter, int32_t *values);
void RENDERER_GL_API_CALL GL_LogGetBufferPointervARB(uint32_t target, uint32_t parameter, void **pointer);
uint32_t RENDERER_GL_API_CALL GL_LogNewObjectBufferATI(int32_t size, const void *data, uint32_t usage);
uint8_t RENDERER_GL_API_CALL GL_LogIsObjectBufferATI(uint32_t buffer);
void RENDERER_GL_API_CALL GL_LogUpdateObjectBufferATI(uint32_t buffer, uint32_t offset, int32_t size, const void *data,
                                                      uint32_t preserveMode);
void RENDERER_GL_API_CALL GL_LogGetObjectBufferfvATI(uint32_t buffer, uint32_t parameter, float *values);
void RENDERER_GL_API_CALL GL_LogGetObjectBufferivATI(uint32_t buffer, uint32_t parameter, int32_t *values);
void RENDERER_GL_API_CALL GL_LogFreeObjectBufferATI(uint32_t buffer);
void RENDERER_GL_API_CALL GL_LogArrayObjectATI(uint32_t array, int32_t size, uint32_t type, int32_t stride, uint32_t buffer,
                                               uint32_t offset);
void RENDERER_GL_API_CALL GL_LogGetArrayObjectfvATI(uint32_t array, uint32_t parameter, float *values);
void RENDERER_GL_API_CALL GL_LogGetArrayObjectivATI(uint32_t array, uint32_t parameter, int32_t *values);
void RENDERER_GL_API_CALL GL_LogVariantArrayObjectATI(uint32_t id, uint32_t type, int32_t stride, uint32_t buffer, uint32_t offset);
void RENDERER_GL_API_CALL GL_LogGetVariantArrayObjectfvATI(uint32_t id, uint32_t parameter, float *values);
void RENDERER_GL_API_CALL GL_LogGetVariantArrayObjectivATI(uint32_t id, uint32_t parameter, int32_t *values);
void RENDERER_GL_API_CALL GL_LogElementPointerATI(uint32_t type, const void *pointer);
void RENDERER_GL_API_CALL GL_LogDrawElementArrayATI(uint32_t mode, int32_t count);
void RENDERER_GL_API_CALL GL_LogDrawRangeElementArrayATI(uint32_t mode, uint32_t start, uint32_t end, int32_t count);
void RENDERER_GL_API_CALL GL_LogFlushVertexArrayRangeNV(void);
void RENDERER_GL_API_CALL GL_LogVertexArrayRangeNV(int32_t length, const void *pointer);
void *RENDERER_GL_API_CALL GL_LogAllocateMemoryNV(int32_t size, float readFrequency, float writeFrequency, float priority);
void RENDERER_GL_API_CALL GL_LogFreeMemoryNV(void *memory);
void RENDERER_GL_API_CALL GL_LogDeleteFencesNV(int32_t count, const uint32_t *fences);
void RENDERER_GL_API_CALL GL_LogGenFencesNV(int32_t count, uint32_t *fences);
uint8_t RENDERER_GL_API_CALL GL_LogIsFenceNV(uint32_t fence);
uint8_t RENDERER_GL_API_CALL GL_LogTestFenceNV(uint32_t fence);
void RENDERER_GL_API_CALL GL_LogGetFenceivNV(uint32_t fence, uint32_t parameter, int32_t *values);
void RENDERER_GL_API_CALL GL_LogCombinerParameterfvNV(uint32_t parameter, const float *values);
void RENDERER_GL_API_CALL GL_LogCombinerParameterfNV(uint32_t parameter, float value);
void RENDERER_GL_API_CALL GL_LogCombinerParameterivNV(uint32_t parameter, const int32_t *values);
void RENDERER_GL_API_CALL GL_LogCombinerParameteriNV(uint32_t parameter, int32_t value);
void RENDERER_GL_API_CALL GL_LogCombinerInputNV(uint32_t stage, uint32_t portion, uint32_t variable, uint32_t input, uint32_t mapping,
                                                uint32_t componentUsage);
void RENDERER_GL_API_CALL GL_LogCombinerOutputNV(uint32_t stage, uint32_t portion, uint32_t abOutput, uint32_t cdOutput, uint32_t sumOutput,
                                                 uint32_t scale, uint32_t bias, uint8_t abDotProduct, uint8_t cdDotProduct, uint8_t muxSum);
void RENDERER_GL_API_CALL GL_LogFinalCombinerInputNV(uint32_t variable, uint32_t input, uint32_t mapping, uint32_t componentUsage);
void RENDERER_GL_API_CALL GL_LogGetCombinerInputParameterfvNV(uint32_t stage, uint32_t portion, uint32_t variable, uint32_t parameter,
                                                              float *values);
void RENDERER_GL_API_CALL GL_LogGetCombinerInputParameterivNV(uint32_t stage, uint32_t portion, uint32_t variable, uint32_t parameter,
                                                              int32_t *values);
void RENDERER_GL_API_CALL GL_LogGetCombinerOutputParameterfvNV(uint32_t stage, uint32_t portion, uint32_t parameter, float *values);
void RENDERER_GL_API_CALL GL_LogGetCombinerOutputParameterivNV(uint32_t stage, uint32_t portion, uint32_t parameter, int32_t *values);
void RENDERER_GL_API_CALL GL_LogGetFinalCombinerInputParameterfvNV(uint32_t variable, uint32_t parameter, float *values);
void RENDERER_GL_API_CALL GL_LogGetFinalCombinerInputParameterivNV(uint32_t variable, uint32_t parameter, int32_t *values);
void RENDERER_GL_API_CALL GL_LogCombinerStageParameterfvNV(uint32_t stage, uint32_t parameter, const float *values);
void RENDERER_GL_API_CALL GL_LogGetCombinerStageParameterfvNV(uint32_t stage, uint32_t parameter, float *values);
uint32_t RENDERER_GL_API_CALL GL_LogGenFragmentShadersATI(uint32_t range);
void RENDERER_GL_API_CALL GL_LogDeleteFragmentShaderATI(uint32_t shader);
void RENDERER_GL_API_CALL GL_LogBeginFragmentShaderATI(void);
void RENDERER_GL_API_CALL GL_LogEndFragmentShaderATI(void);
void RENDERER_GL_API_CALL GL_LogPassTexCoordATI(uint32_t destination, uint32_t coordinate, uint32_t swizzle);
void RENDERER_GL_API_CALL GL_LogSampleMapATI(uint32_t destination, uint32_t interpolation, uint32_t swizzle);
void RENDERER_GL_API_CALL GL_LogColorFragmentOp1ATI(uint32_t operation, uint32_t destination, uint32_t destinationMask,
                                                    uint32_t destinationModifier, uint32_t argument1, uint32_t argument1Replication,
                                                    uint32_t argument1Modifier);
void RENDERER_GL_API_CALL GL_LogColorFragmentOp2ATI(uint32_t operation, uint32_t destination, uint32_t destinationMask,
                                                    uint32_t destinationModifier, uint32_t argument1, uint32_t argument1Replication,
                                                    uint32_t argument1Modifier, uint32_t argument2, uint32_t argument2Replication,
                                                    uint32_t argument2Modifier);
void RENDERER_GL_API_CALL GL_LogColorFragmentOp3ATI(uint32_t operation, uint32_t destination, uint32_t destinationMask,
                                                    uint32_t destinationModifier, uint32_t argument1, uint32_t argument1Replication,
                                                    uint32_t argument1Modifier, uint32_t argument2, uint32_t argument2Replication,
                                                    uint32_t argument2Modifier, uint32_t argument3, uint32_t argument3Replication,
                                                    uint32_t argument3Modifier);
void RENDERER_GL_API_CALL GL_LogAlphaFragmentOp1ATI(uint32_t operation, uint32_t destination, uint32_t destinationModifier,
                                                    uint32_t argument1, uint32_t argument1Replication, uint32_t argument1Modifier);
void RENDERER_GL_API_CALL GL_LogAlphaFragmentOp2ATI(uint32_t operation, uint32_t destination, uint32_t destinationModifier,
                                                    uint32_t argument1, uint32_t argument1Replication, uint32_t argument1Modifier,
                                                    uint32_t argument2, uint32_t argument2Replication, uint32_t argument2Modifier);
void RENDERER_GL_API_CALL GL_LogAlphaFragmentOp3ATI(uint32_t operation, uint32_t destination, uint32_t destinationModifier,
                                                    uint32_t argument1, uint32_t argument1Replication, uint32_t argument1Modifier,
                                                    uint32_t argument2, uint32_t argument2Replication, uint32_t argument2Modifier,
                                                    uint32_t argument3, uint32_t argument3Replication, uint32_t argument3Modifier);
void RENDERER_GL_API_CALL GL_LogSetFragmentShaderConstantATI(uint32_t destination, const float *value);
void RENDERER_GL_API_CALL GL_LogVertexAttrib1sARB(uint32_t index, int16_t x);
void RENDERER_GL_API_CALL GL_LogVertexAttrib1fARB(uint32_t index, float x);
void RENDERER_GL_API_CALL GL_LogVertexAttrib1dARB(uint32_t index, double x);
void RENDERER_GL_API_CALL GL_LogVertexAttrib2sARB(uint32_t index, int16_t x, int16_t y);
void RENDERER_GL_API_CALL GL_LogVertexAttrib2fARB(uint32_t index, float x, float y);
void RENDERER_GL_API_CALL GL_LogVertexAttrib2dARB(uint32_t index, double x, double y);
void RENDERER_GL_API_CALL GL_LogVertexAttrib3sARB(uint32_t index, int16_t x, int16_t y, int16_t z);
void RENDERER_GL_API_CALL GL_LogVertexAttrib3fARB(uint32_t index, float x, float y, float z);
void RENDERER_GL_API_CALL GL_LogVertexAttrib3dARB(uint32_t index, double x, double y, double z);
void RENDERER_GL_API_CALL GL_LogVertexAttrib4sARB(uint32_t index, int16_t x, int16_t y, int16_t z, int16_t w);
void RENDERER_GL_API_CALL GL_LogVertexAttrib4fARB(uint32_t index, float x, float y, float z, float w);
void RENDERER_GL_API_CALL GL_LogVertexAttrib4dARB(uint32_t index, double x, double y, double z, double w);
void RENDERER_GL_API_CALL GL_LogVertexAttrib4NubARB(uint32_t index, uint8_t x, uint8_t y, uint8_t z, uint8_t w);
void RENDERER_GL_API_CALL GL_LogVertexAttrib1svARB(uint32_t index, const int16_t *values);
void RENDERER_GL_API_CALL GL_LogVertexAttrib1fvARB(uint32_t index, const float *values);
void RENDERER_GL_API_CALL GL_LogVertexAttrib1dvARB(uint32_t index, const double *values);
void RENDERER_GL_API_CALL GL_LogVertexAttrib2svARB(uint32_t index, const int16_t *values);
void RENDERER_GL_API_CALL GL_LogVertexAttrib2fvARB(uint32_t index, const float *values);
void RENDERER_GL_API_CALL GL_LogVertexAttrib2dvARB(uint32_t index, const double *values);
void RENDERER_GL_API_CALL GL_LogVertexAttrib3svARB(uint32_t index, const int16_t *values);
void RENDERER_GL_API_CALL GL_LogVertexAttrib3fvARB(uint32_t index, const float *values);
void RENDERER_GL_API_CALL GL_LogVertexAttrib3dvARB(uint32_t index, const double *values);
void RENDERER_GL_API_CALL GL_LogVertexAttrib4bvARB(uint32_t index, const int8_t *values);
void RENDERER_GL_API_CALL GL_LogVertexAttrib4svARB(uint32_t index, const int16_t *values);
void RENDERER_GL_API_CALL GL_LogVertexAttrib4ivARB(uint32_t index, const int32_t *values);
void RENDERER_GL_API_CALL GL_LogVertexAttrib4ubvARB(uint32_t index, const uint8_t *values);
void RENDERER_GL_API_CALL GL_LogVertexAttrib4usvARB(uint32_t index, const uint16_t *values);
void RENDERER_GL_API_CALL GL_LogVertexAttrib4uivARB(uint32_t index, const uint32_t *values);
void RENDERER_GL_API_CALL GL_LogVertexAttrib4fvARB(uint32_t index, const float *values);
void RENDERER_GL_API_CALL GL_LogVertexAttrib4dvARB(uint32_t index, const double *values);
void RENDERER_GL_API_CALL GL_LogVertexAttrib4NbvARB(uint32_t index, const int8_t *values);
void RENDERER_GL_API_CALL GL_LogVertexAttrib4NsvARB(uint32_t index, const int16_t *values);
void RENDERER_GL_API_CALL GL_LogVertexAttrib4NivARB(uint32_t index, const int32_t *values);
void RENDERER_GL_API_CALL GL_LogVertexAttrib4NubvARB(uint32_t index, const uint8_t *values);
void RENDERER_GL_API_CALL GL_LogVertexAttrib4NusvARB(uint32_t index, const uint16_t *values);
void RENDERER_GL_API_CALL GL_LogVertexAttrib4NuivARB(uint32_t index, const uint32_t *values);
void RENDERER_GL_API_CALL GL_LogVertexAttribPointerARB(uint32_t index, int32_t size, uint32_t type, uint8_t normalized, int32_t stride,
                                                       const void *pointer);
void RENDERER_GL_API_CALL GL_LogEnableVertexAttribArrayARB(uint32_t index);
void RENDERER_GL_API_CALL GL_LogDisableVertexAttribArrayARB(uint32_t index);
void RENDERER_GL_API_CALL GL_LogProgramStringARB(uint32_t target, uint32_t format, int32_t length, const void *string);
void RENDERER_GL_API_CALL GL_LogDeleteProgramsARB(int32_t count, const uint32_t *programs);
void RENDERER_GL_API_CALL GL_LogGenProgramsARB(int32_t count, uint32_t *programs);
void RENDERER_GL_API_CALL GL_LogProgramEnvParameter4fARB(uint32_t target, uint32_t index, float x, float y, float z, float w);
void RENDERER_GL_API_CALL GL_LogProgramEnvParameter4dARB(uint32_t target, uint32_t index, double x, double y, double z, double w);
void RENDERER_GL_API_CALL GL_LogProgramEnvParameter4fvARB(uint32_t target, uint32_t index, const float *values);
void RENDERER_GL_API_CALL GL_LogProgramEnvParameter4dvARB(uint32_t target, uint32_t index, const double *values);
void RENDERER_GL_API_CALL GL_LogProgramLocalParameter4fARB(uint32_t target, uint32_t index, float x, float y, float z, float w);
void RENDERER_GL_API_CALL GL_LogProgramLocalParameter4dARB(uint32_t target, uint32_t index, double x, double y, double z, double w);
void RENDERER_GL_API_CALL GL_LogProgramLocalParameter4fvARB(uint32_t target, uint32_t index, const float *values);
void RENDERER_GL_API_CALL GL_LogProgramLocalParameter4dvARB(uint32_t target, uint32_t index, const double *values);
void RENDERER_GL_API_CALL GL_LogGetProgramEnvParameterfvARB(uint32_t target, uint32_t index, float *values);
void RENDERER_GL_API_CALL GL_LogGetProgramEnvParameterdvARB(uint32_t target, uint32_t index, double *values);
void RENDERER_GL_API_CALL GL_LogGetProgramLocalParameterfvARB(uint32_t target, uint32_t index, float *values);
void RENDERER_GL_API_CALL GL_LogGetProgramLocalParameterdvARB(uint32_t target, uint32_t index, double *values);
void RENDERER_GL_API_CALL GL_LogGetProgramivARB(uint32_t target, uint32_t parameter, int32_t *values);
void RENDERER_GL_API_CALL GL_LogGetProgramStringARB(uint32_t target, uint32_t parameter, void *string);
void RENDERER_GL_API_CALL GL_LogGetVertexAttribdvARB(uint32_t index, uint32_t parameter, double *values);
void RENDERER_GL_API_CALL GL_LogGetVertexAttribfvARB(uint32_t index, uint32_t parameter, float *values);
void RENDERER_GL_API_CALL GL_LogGetVertexAttribivARB(uint32_t index, uint32_t parameter, int32_t *values);
void RENDERER_GL_API_CALL GL_LogGetVertexAttribPointervARB(uint32_t index, uint32_t parameter, void **pointer);
uint8_t RENDERER_GL_API_CALL GL_LogIsProgramARB(uint32_t program);
void RENDERER_GL_API_CALL GL_LogAccum(uint32_t operation, float value);
uint8_t RENDERER_GL_API_CALL GL_LogAreTexturesResident(int32_t count, const uint32_t *textures, uint8_t *residences);
void RENDERER_GL_API_CALL GL_LogArrayElement(int32_t index);
void RENDERER_GL_API_CALL GL_LogBitmap(int32_t width, int32_t height, float xOrigin, float yOrigin, float xMove, float yMove,
                                       const uint8_t *bitmap);
void RENDERER_GL_API_CALL GL_LogCallLists(int32_t count, uint32_t type, const void *lists);
void RENDERER_GL_API_CALL GL_LogClearAccum(float red, float green, float blue, float alpha);
void RENDERER_GL_API_CALL GL_LogClearColor(float red, float green, float blue, float alpha);
void RENDERER_GL_API_CALL GL_LogClearIndex(float index);
void RENDERER_GL_API_CALL GL_LogClipPlane(uint32_t plane, const double *equation);
void RENDERER_GL_API_CALL GL_LogColor3b(int8_t red, int8_t green, int8_t blue);
void RENDERER_GL_API_CALL GL_LogColor3bv(const int8_t *values);
void RENDERER_GL_API_CALL GL_LogColor3d(double red, double green, double blue);
void RENDERER_GL_API_CALL GL_LogColor3dv(const double *values);
void RENDERER_GL_API_CALL GL_LogColor3f(float red, float green, float blue);
void RENDERER_GL_API_CALL GL_LogColor3fv(const float *values);
void RENDERER_GL_API_CALL GL_LogColor3i(int32_t red, int32_t green, int32_t blue);
void RENDERER_GL_API_CALL GL_LogColor3iv(const int32_t *values);
void RENDERER_GL_API_CALL GL_LogColor3s(int16_t red, int16_t green, int16_t blue);
void RENDERER_GL_API_CALL GL_LogColor3sv(const int16_t *values);
void RENDERER_GL_API_CALL GL_LogColor3ub(uint8_t red, uint8_t green, uint8_t blue);
void RENDERER_GL_API_CALL GL_LogColor3ubv(const uint8_t *values);
void RENDERER_GL_API_CALL GL_LogColor3ui(uint32_t red, uint32_t green, uint32_t blue);
void RENDERER_GL_API_CALL GL_LogColor3uiv(const uint32_t *values);
void RENDERER_GL_API_CALL GL_LogColor3us(uint16_t red, uint16_t green, uint16_t blue);
void RENDERER_GL_API_CALL GL_LogColor3usv(const uint16_t *values);
void RENDERER_GL_API_CALL GL_LogColor4b(int8_t red, int8_t green, int8_t blue, int8_t alpha);
void RENDERER_GL_API_CALL GL_LogColor4bv(const int8_t *values);
void RENDERER_GL_API_CALL GL_LogColor4d(double red, double green, double blue, double alpha);
void RENDERER_GL_API_CALL GL_LogColor4dv(const double *values);
void RENDERER_GL_API_CALL GL_LogColor4i(int32_t red, int32_t green, int32_t blue, int32_t alpha);
void RENDERER_GL_API_CALL GL_LogColor4iv(const int32_t *values);
void RENDERER_GL_API_CALL GL_LogColor4s(int16_t red, int16_t green, int16_t blue, int16_t alpha);
void RENDERER_GL_API_CALL GL_LogColor4sv(const int16_t *values);
void RENDERER_GL_API_CALL GL_LogColor4ub(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha);
void RENDERER_GL_API_CALL GL_LogColor4ubv(const uint8_t *values);
void RENDERER_GL_API_CALL GL_LogColor4ui(uint32_t red, uint32_t green, uint32_t blue, uint32_t alpha);
void RENDERER_GL_API_CALL GL_LogColor4uiv(const uint32_t *values);
void RENDERER_GL_API_CALL GL_LogColor4us(uint16_t red, uint16_t green, uint16_t blue, uint16_t alpha);
void RENDERER_GL_API_CALL GL_LogColor4usv(const uint16_t *values);
void RENDERER_GL_API_CALL GL_LogColorMask(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha);
void RENDERER_GL_API_CALL GL_LogColorMaterial(uint32_t face, uint32_t mode);
void RENDERER_GL_API_CALL GL_LogCopyPixels(int32_t x, int32_t y, int32_t width, int32_t height, uint32_t type);
void RENDERER_GL_API_CALL GL_LogCopyTexImage1D(uint32_t target, int32_t level, uint32_t internalFormat, int32_t x, int32_t y, int32_t width,
                                               int32_t border);
void RENDERER_GL_API_CALL GL_LogCopyTexImage2D(uint32_t target, int32_t level, uint32_t internalFormat, int32_t x, int32_t y, int32_t width,
                                               int32_t height, int32_t border);
void RENDERER_GL_API_CALL GL_LogCopyTexSubImage1D(uint32_t target, int32_t level, int32_t xOffset, int32_t x, int32_t y, int32_t width);
void RENDERER_GL_API_CALL GL_LogCopyTexSubImage2D(uint32_t target, int32_t level, int32_t xOffset, int32_t yOffset, int32_t x, int32_t y,
                                                  int32_t width, int32_t height);
void RENDERER_GL_API_CALL GL_LogDeleteLists(uint32_t list, int32_t range);
void RENDERER_GL_API_CALL GL_LogDeleteTextures(int32_t count, const uint32_t *textures);
void RENDERER_GL_API_CALL GL_LogDrawArrays(uint32_t mode, int32_t first, int32_t count);
void RENDERER_GL_API_CALL GL_LogDrawPixels(int32_t width, int32_t height, uint32_t format, uint32_t type, const void *pixels);
void RENDERER_GL_API_CALL GL_LogEdgeFlag(uint8_t flag);
void RENDERER_GL_API_CALL GL_LogEdgeFlagPointer(int32_t stride, const void *pointer);
void RENDERER_GL_API_CALL GL_LogEdgeFlagv(const uint8_t *flag);
void RENDERER_GL_API_CALL GL_LogEnd(void);
void RENDERER_GL_API_CALL GL_LogEndList(void);
void RENDERER_GL_API_CALL GL_LogEvalCoord1d(double u);
void RENDERER_GL_API_CALL GL_LogEvalCoord1dv(const double *u);
void RENDERER_GL_API_CALL GL_LogEvalCoord1f(float u);
void RENDERER_GL_API_CALL GL_LogEvalCoord1fv(const float *u);
void RENDERER_GL_API_CALL GL_LogEvalCoord2d(double u, double v);
void RENDERER_GL_API_CALL GL_LogEvalCoord2dv(const double *values);
void RENDERER_GL_API_CALL GL_LogEvalCoord2f(float u, float v);
void RENDERER_GL_API_CALL GL_LogEvalCoord2fv(const float *values);
void RENDERER_GL_API_CALL GL_LogEvalMesh1(uint32_t mode, int32_t i1, int32_t i2);
void RENDERER_GL_API_CALL GL_LogEvalMesh2(uint32_t mode, int32_t i1, int32_t i2, int32_t j1, int32_t j2);
void RENDERER_GL_API_CALL GL_LogEvalPoint1(int32_t i);
void RENDERER_GL_API_CALL GL_LogEvalPoint2(int32_t i, int32_t j);
void RENDERER_GL_API_CALL GL_LogFeedbackBuffer(int32_t size, uint32_t type, float *buffer);
void RENDERER_GL_API_CALL GL_LogFinish(void);
void RENDERER_GL_API_CALL GL_LogFlush(void);
void RENDERER_GL_API_CALL GL_LogFogiv(uint32_t parameter, const int32_t *values);
void RENDERER_GL_API_CALL GL_LogFrontFace(uint32_t mode);
void RENDERER_GL_API_CALL GL_LogFrustum(double left, double right, double bottom, double top, double nearValue, double farValue);
uint32_t RENDERER_GL_API_CALL GL_LogGenLists(int32_t range);
void RENDERER_GL_API_CALL GL_LogGenTextures(int32_t count, uint32_t *textures);
void RENDERER_GL_API_CALL GL_LogGetBooleanv(uint32_t parameter, uint8_t *values);
void RENDERER_GL_API_CALL GL_LogGetClipPlane(uint32_t plane, double *equation);
void RENDERER_GL_API_CALL GL_LogGetDoublev(uint32_t parameter, double *values);
uint32_t RENDERER_GL_API_CALL GL_LogGetError(void);
void RENDERER_GL_API_CALL GL_LogGetFloatv(uint32_t parameter, float *values);
void RENDERER_GL_API_CALL GL_LogGetIntegerv(uint32_t parameter, int32_t *values);
void RENDERER_GL_API_CALL GL_LogGetLightfv(uint32_t light, uint32_t parameter, float *values);
void RENDERER_GL_API_CALL GL_LogGetLightiv(uint32_t light, uint32_t parameter, int32_t *values);
void RENDERER_GL_API_CALL GL_LogGetMapdv(uint32_t target, uint32_t query, double *values);
void RENDERER_GL_API_CALL GL_LogGetMapfv(uint32_t target, uint32_t query, float *values);
void RENDERER_GL_API_CALL GL_LogGetMapiv(uint32_t target, uint32_t query, int32_t *values);
void RENDERER_GL_API_CALL GL_LogGetMaterialfv(uint32_t face, uint32_t parameter, float *values);
void RENDERER_GL_API_CALL GL_LogGetMaterialiv(uint32_t face, uint32_t parameter, int32_t *values);
void RENDERER_GL_API_CALL GL_LogGetPixelMapfv(uint32_t map, float *values);
void RENDERER_GL_API_CALL GL_LogGetPixelMapuiv(uint32_t map, uint32_t *values);
void RENDERER_GL_API_CALL GL_LogGetPixelMapusv(uint32_t map, uint16_t *values);
void RENDERER_GL_API_CALL GL_LogGetPointerv(uint32_t parameter, void **value);
void RENDERER_GL_API_CALL GL_LogGetPolygonStipple(uint8_t *mask);
const uint8_t *RENDERER_GL_API_CALL GL_LogGetString(uint32_t name);
void RENDERER_GL_API_CALL GL_LogGetTexEnvfv(uint32_t target, uint32_t parameter, float *values);
void RENDERER_GL_API_CALL GL_LogGetTexEnviv(uint32_t target, uint32_t parameter, int32_t *values);
void RENDERER_GL_API_CALL GL_LogGetTexGendv(uint32_t coordinate, uint32_t parameter, double *values);
void RENDERER_GL_API_CALL GL_LogGetTexGenfv(uint32_t coordinate, uint32_t parameter, float *values);
void RENDERER_GL_API_CALL GL_LogGetTexGeniv(uint32_t coordinate, uint32_t parameter, int32_t *values);
void RENDERER_GL_API_CALL GL_LogGetTexImage(uint32_t target, int32_t level, uint32_t format, uint32_t type, void *pixels);
void RENDERER_GL_API_CALL GL_LogGetTexLevelParameterfv(uint32_t target, int32_t level, uint32_t parameter, float *values);
void RENDERER_GL_API_CALL GL_LogGetTexLevelParameteriv(uint32_t target, int32_t level, uint32_t parameter, int32_t *values);
void RENDERER_GL_API_CALL GL_LogGetTexParameterfv(uint32_t target, uint32_t parameter, float *values);
void RENDERER_GL_API_CALL GL_LogGetTexParameteriv(uint32_t target, uint32_t parameter, int32_t *values);
void RENDERER_GL_API_CALL GL_LogIndexMask(uint32_t mask);
void RENDERER_GL_API_CALL GL_LogIndexPointer(uint32_t type, int32_t stride, const void *pointer);
void RENDERER_GL_API_CALL GL_LogIndexd(double index);
void RENDERER_GL_API_CALL GL_LogIndexdv(const double *index);
void RENDERER_GL_API_CALL GL_LogIndexf(float index);
void RENDERER_GL_API_CALL GL_LogIndexfv(const float *index);
void RENDERER_GL_API_CALL GL_LogIndexi(int32_t index);
void RENDERER_GL_API_CALL GL_LogIndexiv(const int32_t *index);
void RENDERER_GL_API_CALL GL_LogIndexs(int16_t index);
void RENDERER_GL_API_CALL GL_LogIndexsv(const int16_t *index);
void RENDERER_GL_API_CALL GL_LogIndexub(uint8_t index);
void RENDERER_GL_API_CALL GL_LogIndexubv(const uint8_t *index);
void RENDERER_GL_API_CALL GL_LogInitNames(void);
void RENDERER_GL_API_CALL GL_LogInterleavedArrays(uint32_t format, int32_t stride, const void *pointer);
uint8_t RENDERER_GL_API_CALL GL_LogIsEnabled(uint32_t capability);
uint8_t RENDERER_GL_API_CALL GL_LogIsList(uint32_t list);
uint8_t RENDERER_GL_API_CALL GL_LogIsTexture(uint32_t texture);
void RENDERER_GL_API_CALL GL_LogLightModelf(uint32_t parameter, float value);
void RENDERER_GL_API_CALL GL_LogLightModelfv(uint32_t parameter, const float *values);
void RENDERER_GL_API_CALL GL_LogLightModeli(uint32_t parameter, int32_t value);
void RENDERER_GL_API_CALL GL_LogLightModeliv(uint32_t parameter, const int32_t *values);
void RENDERER_GL_API_CALL GL_LogLightf(uint32_t light, uint32_t parameter, float value);
void RENDERER_GL_API_CALL GL_LogLightfv(uint32_t light, uint32_t parameter, const float *values);
void RENDERER_GL_API_CALL GL_LogLighti(uint32_t light, uint32_t parameter, int32_t value);
void RENDERER_GL_API_CALL GL_LogLightiv(uint32_t light, uint32_t parameter, const int32_t *values);
void RENDERER_GL_API_CALL GL_LogLineStipple(int32_t factor, uint16_t pattern);
void RENDERER_GL_API_CALL GL_LogLineWidth(float width);
void RENDERER_GL_API_CALL GL_LogListBase(uint32_t base);
void RENDERER_GL_API_CALL GL_LogLoadIdentity(void);
void RENDERER_GL_API_CALL GL_LogLoadMatrixd(const double *matrix);
void RENDERER_GL_API_CALL GL_LogLoadMatrixf(const float *matrix);
void RENDERER_GL_API_CALL GL_LogLoadName(uint32_t name);
void RENDERER_GL_API_CALL GL_LogLogicOp(uint32_t operation);
void RENDERER_GL_API_CALL GL_LogMap1d(uint32_t target, double u1, double u2, int32_t stride, int32_t order, const double *points);
void RENDERER_GL_API_CALL GL_LogMap1f(uint32_t target, float u1, float u2, int32_t stride, int32_t order, const float *points);
void RENDERER_GL_API_CALL GL_LogMap2d(uint32_t target, double u1, double u2, int32_t uStride, int32_t uOrder, double v1, double v2,
                                      int32_t vStride, int32_t vOrder, const double *points);
void RENDERER_GL_API_CALL GL_LogMap2f(uint32_t target, float u1, float u2, int32_t uStride, int32_t uOrder, float v1, float v2,
                                      int32_t vStride, int32_t vOrder, const float *points);
void RENDERER_GL_API_CALL GL_LogMapGrid1d(int32_t count, double u1, double u2);
void RENDERER_GL_API_CALL GL_LogMapGrid1f(int32_t count, float u1, float u2);
void RENDERER_GL_API_CALL GL_LogMapGrid2d(int32_t uCount, double u1, double u2, int32_t vCount, double v1, double v2);
void RENDERER_GL_API_CALL GL_LogMapGrid2f(int32_t uCount, float u1, float u2, int32_t vCount, float v1, float v2);
void RENDERER_GL_API_CALL GL_LogMaterialf(uint32_t face, uint32_t parameter, float value);
void RENDERER_GL_API_CALL GL_LogMaterialfv(uint32_t face, uint32_t parameter, const float *values);
void RENDERER_GL_API_CALL GL_LogMateriali(uint32_t face, uint32_t parameter, int32_t value);
void RENDERER_GL_API_CALL GL_LogMaterialiv(uint32_t face, uint32_t parameter, const int32_t *values);
void RENDERER_GL_API_CALL GL_LogMatrixMode(uint32_t mode);
void RENDERER_GL_API_CALL GL_LogMultMatrixd(const double *matrix);
void RENDERER_GL_API_CALL GL_LogMultMatrixf(const float *matrix);
void RENDERER_GL_API_CALL GL_LogNewList(uint32_t list, uint32_t mode);
void RENDERER_GL_API_CALL GL_LogNormal3b(int8_t x, int8_t y, int8_t z);
void RENDERER_GL_API_CALL GL_LogNormal3bv(const int8_t *values);
void RENDERER_GL_API_CALL GL_LogNormal3d(double x, double y, double z);
void RENDERER_GL_API_CALL GL_LogNormal3dv(const double *values);
void RENDERER_GL_API_CALL GL_LogNormal3f(float x, float y, float z);
void RENDERER_GL_API_CALL GL_LogNormal3fv(const float *values);
void RENDERER_GL_API_CALL GL_LogNormal3i(int32_t x, int32_t y, int32_t z);
void RENDERER_GL_API_CALL GL_LogNormal3iv(const int32_t *values);
void RENDERER_GL_API_CALL GL_LogNormal3s(int16_t x, int16_t y, int16_t z);
void RENDERER_GL_API_CALL GL_LogNormal3sv(const int16_t *values);
void RENDERER_GL_API_CALL GL_LogOrtho(double left, double right, double bottom, double top, double nearValue, double farValue);
void RENDERER_GL_API_CALL GL_LogPassThrough(float token);
void RENDERER_GL_API_CALL GL_LogPixelMapfv(uint32_t map, int32_t mapSize, const float *values);
void RENDERER_GL_API_CALL GL_LogPixelMapuiv(uint32_t map, int32_t mapSize, const uint32_t *values);
void RENDERER_GL_API_CALL GL_LogPixelMapusv(uint32_t map, int32_t mapSize, const uint16_t *values);
void RENDERER_GL_API_CALL GL_LogPixelStoref(uint32_t parameter, float value);
void RENDERER_GL_API_CALL GL_LogPixelStorei(uint32_t parameter, int32_t value);
void RENDERER_GL_API_CALL GL_LogPixelTransferf(uint32_t parameter, float value);
void RENDERER_GL_API_CALL GL_LogPixelTransferi(uint32_t parameter, int32_t value);
void RENDERER_GL_API_CALL GL_LogPixelZoom(float xFactor, float yFactor);
void RENDERER_GL_API_CALL GL_LogPointSize(float size);
void RENDERER_GL_API_CALL GL_LogPolygonOffset(float factor, float units);
void RENDERER_GL_API_CALL GL_LogPolygonStipple(const uint8_t *mask);
void RENDERER_GL_API_CALL GL_LogPopAttrib(void);
void RENDERER_GL_API_CALL GL_LogPopClientAttrib(void);
void RENDERER_GL_API_CALL GL_LogPopMatrix(void);
void RENDERER_GL_API_CALL GL_LogPopName(void);
void RENDERER_GL_API_CALL GL_LogPrioritizeTextures(int32_t count, const uint32_t *textures, const float *priorities);
void RENDERER_GL_API_CALL GL_LogPushAttrib(uint32_t mask);
void RENDERER_GL_API_CALL GL_LogPushClientAttrib(uint32_t mask);
void RENDERER_GL_API_CALL GL_LogPushMatrix(void);
void RENDERER_GL_API_CALL GL_LogPushName(uint32_t name);
void RENDERER_GL_API_CALL GL_LogRasterPos2d(double x, double y);
void RENDERER_GL_API_CALL GL_LogRasterPos2dv(const double *values);
void RENDERER_GL_API_CALL GL_LogRasterPos2f(float x, float y);
void RENDERER_GL_API_CALL GL_LogRasterPos2fv(const float *values);
void RENDERER_GL_API_CALL GL_LogRasterPos2i(int32_t x, int32_t y);
void RENDERER_GL_API_CALL GL_LogRasterPos2iv(const int32_t *values);
void RENDERER_GL_API_CALL GL_LogRasterPos2s(int16_t x, int16_t y);
void RENDERER_GL_API_CALL GL_LogRasterPos2sv(const int16_t *values);
void RENDERER_GL_API_CALL GL_LogRasterPos3d(double x, double y, double z);
void RENDERER_GL_API_CALL GL_LogRasterPos3dv(const double *values);
void RENDERER_GL_API_CALL GL_LogRasterPos3f(float x, float y, float z);
void RENDERER_GL_API_CALL GL_LogRasterPos3fv(const float *values);
void RENDERER_GL_API_CALL GL_LogRasterPos3i(int32_t x, int32_t y, int32_t z);
void RENDERER_GL_API_CALL GL_LogRasterPos3iv(const int32_t *values);
void RENDERER_GL_API_CALL GL_LogRasterPos3s(int16_t x, int16_t y, int16_t z);
void RENDERER_GL_API_CALL GL_LogRasterPos3sv(const int16_t *values);
void RENDERER_GL_API_CALL GL_LogRasterPos4d(double x, double y, double z, double w);
void RENDERER_GL_API_CALL GL_LogRasterPos4dv(const double *values);
void RENDERER_GL_API_CALL GL_LogRasterPos4f(float x, float y, float z, float w);
void RENDERER_GL_API_CALL GL_LogRasterPos4fv(const float *values);
void RENDERER_GL_API_CALL GL_LogRasterPos4i(int32_t x, int32_t y, int32_t z, int32_t w);
void RENDERER_GL_API_CALL GL_LogRasterPos4iv(const int32_t *values);
void RENDERER_GL_API_CALL GL_LogRasterPos4s(int16_t x, int16_t y, int16_t z, int16_t w);
void RENDERER_GL_API_CALL GL_LogRasterPos4sv(const int16_t *values);
void RENDERER_GL_API_CALL GL_LogReadBuffer(uint32_t mode);
void RENDERER_GL_API_CALL GL_LogReadPixels(int32_t x, int32_t y, int32_t width, int32_t height, uint32_t format, uint32_t type,
                                           void *pixels);
void RENDERER_GL_API_CALL GL_LogRectd(double x1, double y1, double x2, double y2);
void RENDERER_GL_API_CALL GL_LogRectdv(const double *vertex1, const double *vertex2);
void RENDERER_GL_API_CALL GL_LogRectf(float x1, float y1, float x2, float y2);
void RENDERER_GL_API_CALL GL_LogRectfv(const float *vertex1, const float *vertex2);
void RENDERER_GL_API_CALL GL_LogRecti(int32_t x1, int32_t y1, int32_t x2, int32_t y2);
void RENDERER_GL_API_CALL GL_LogRectiv(const int32_t *vertex1, const int32_t *vertex2);
void RENDERER_GL_API_CALL GL_LogRects(int16_t x1, int16_t y1, int16_t x2, int16_t y2);
void RENDERER_GL_API_CALL GL_LogRectsv(const int16_t *vertex1, const int16_t *vertex2);
int32_t RENDERER_GL_API_CALL GL_LogRenderMode(uint32_t mode);
void RENDERER_GL_API_CALL GL_LogRotated(double angle, double x, double y, double z);
void RENDERER_GL_API_CALL GL_LogRotatef(float angle, float x, float y, float z);
void RENDERER_GL_API_CALL GL_LogScaled(double x, double y, double z);
void RENDERER_GL_API_CALL GL_LogScalef(float x, float y, float z);
void RENDERER_GL_API_CALL GL_LogSelectBuffer(int32_t size, uint32_t *buffer);
void RENDERER_GL_API_CALL GL_LogShadeModel(uint32_t mode);
void RENDERER_GL_API_CALL GL_LogStencilFunc(uint32_t func, int32_t reference, uint32_t mask);
void RENDERER_GL_API_CALL GL_LogStencilMask(uint32_t mask);
void RENDERER_GL_API_CALL GL_LogStencilOp(uint32_t stencilFail, uint32_t depthFail, uint32_t depthPass);
void RENDERER_GL_API_CALL GL_LogTexCoord1d(double s);
void RENDERER_GL_API_CALL GL_LogTexCoord1dv(const double *values);
void RENDERER_GL_API_CALL GL_LogTexCoord1f(float s);
void RENDERER_GL_API_CALL GL_LogTexCoord1fv(const float *values);
void RENDERER_GL_API_CALL GL_LogTexCoord1i(int32_t s);
void RENDERER_GL_API_CALL GL_LogTexCoord1iv(const int32_t *values);
void RENDERER_GL_API_CALL GL_LogTexCoord1s(int16_t s);
void RENDERER_GL_API_CALL GL_LogTexCoord1sv(const int16_t *values);
void RENDERER_GL_API_CALL GL_LogTexCoord2d(double s, double t);
void RENDERER_GL_API_CALL GL_LogTexCoord2dv(const double *values);
void RENDERER_GL_API_CALL GL_LogTexCoord2f(float s, float t);
void RENDERER_GL_API_CALL GL_LogTexCoord2fv(const float *values);
void RENDERER_GL_API_CALL GL_LogTexCoord2i(int32_t s, int32_t t);
void RENDERER_GL_API_CALL GL_LogTexCoord2iv(const int32_t *values);
void RENDERER_GL_API_CALL GL_LogTexCoord2s(int16_t s, int16_t t);
void RENDERER_GL_API_CALL GL_LogTexCoord2sv(const int16_t *values);
void RENDERER_GL_API_CALL GL_LogTexCoord3d(double s, double t, double r);
void RENDERER_GL_API_CALL GL_LogTexCoord3dv(const double *values);
void RENDERER_GL_API_CALL GL_LogTexCoord3f(float s, float t, float r);
void RENDERER_GL_API_CALL GL_LogTexCoord3fv(const float *values);
void RENDERER_GL_API_CALL GL_LogTexCoord3i(int32_t s, int32_t t, int32_t r);
void RENDERER_GL_API_CALL GL_LogTexCoord3iv(const int32_t *values);
void RENDERER_GL_API_CALL GL_LogTexCoord3s(int16_t s, int16_t t, int16_t r);
void RENDERER_GL_API_CALL GL_LogTexCoord3sv(const int16_t *values);
void RENDERER_GL_API_CALL GL_LogTexCoord4d(double s, double t, double r, double q);
void RENDERER_GL_API_CALL GL_LogTexCoord4dv(const double *values);
void RENDERER_GL_API_CALL GL_LogTexCoord4f(float s, float t, float r, float q);
void RENDERER_GL_API_CALL GL_LogTexCoord4fv(const float *values);
void RENDERER_GL_API_CALL GL_LogTexCoord4i(int32_t s, int32_t t, int32_t r, int32_t q);
void RENDERER_GL_API_CALL GL_LogTexCoord4iv(const int32_t *values);
void RENDERER_GL_API_CALL GL_LogTexCoord4s(int16_t s, int16_t t, int16_t r, int16_t q);
void RENDERER_GL_API_CALL GL_LogTexCoord4sv(const int16_t *values);
void RENDERER_GL_API_CALL GL_LogTexEnvfv(uint32_t target, uint32_t parameter, const float *values);
void RENDERER_GL_API_CALL GL_LogTexEnviv(uint32_t target, uint32_t parameter, const int32_t *values);
void RENDERER_GL_API_CALL GL_LogTexGend(uint32_t coordinate, uint32_t parameter, double value);
void RENDERER_GL_API_CALL GL_LogTexGendv(uint32_t coordinate, uint32_t parameter, const double *values);
void RENDERER_GL_API_CALL GL_LogTexGenf(uint32_t coordinate, uint32_t parameter, float value);
void RENDERER_GL_API_CALL GL_LogTexGenfv(uint32_t coordinate, uint32_t parameter, const float *values);
void RENDERER_GL_API_CALL GL_LogTexGeni(uint32_t coordinate, uint32_t parameter, int32_t value);
void RENDERER_GL_API_CALL GL_LogTexGeniv(uint32_t coordinate, uint32_t parameter, const int32_t *values);
void RENDERER_GL_API_CALL GL_LogTexImage1D(uint32_t target, int32_t level, int32_t internalFormat, int32_t width, int32_t border,
                                           uint32_t format, uint32_t type, const void *pixels);
void RENDERER_GL_API_CALL GL_LogTexImage2D(uint32_t target, int32_t level, int32_t internalFormat, int32_t width, int32_t height,
                                           int32_t border, uint32_t format, uint32_t type, const void *pixels);
void RENDERER_GL_API_CALL GL_LogTexParameterfv(uint32_t target, uint32_t parameter, const float *values);
void RENDERER_GL_API_CALL GL_LogTexParameteriv(uint32_t target, uint32_t parameter, const int32_t *values);
void RENDERER_GL_API_CALL GL_LogTexSubImage1D(uint32_t target, int32_t level, int32_t xOffset, int32_t width, uint32_t format,
                                              uint32_t type, const void *pixels);
void RENDERER_GL_API_CALL GL_LogTexSubImage2D(uint32_t target, int32_t level, int32_t xOffset, int32_t yOffset, int32_t width,
                                              int32_t height, uint32_t format, uint32_t type, const void *pixels);
void RENDERER_GL_API_CALL GL_LogTranslated(double x, double y, double z);
void RENDERER_GL_API_CALL GL_LogTranslatef(float x, float y, float z);
void RENDERER_GL_API_CALL GL_LogVertex2d(double x, double y);
void RENDERER_GL_API_CALL GL_LogVertex2dv(const double *values);
void RENDERER_GL_API_CALL GL_LogVertex2f(float x, float y);
void RENDERER_GL_API_CALL GL_LogVertex2fv(const float *values);
void RENDERER_GL_API_CALL GL_LogVertex2i(int32_t x, int32_t y);
void RENDERER_GL_API_CALL GL_LogVertex2iv(const int32_t *values);
void RENDERER_GL_API_CALL GL_LogVertex2s(int16_t x, int16_t y);
void RENDERER_GL_API_CALL GL_LogVertex2sv(const int16_t *values);
void RENDERER_GL_API_CALL GL_LogVertex3d(double x, double y, double z);
void RENDERER_GL_API_CALL GL_LogVertex3dv(const double *values);
void RENDERER_GL_API_CALL GL_LogVertex3f(float x, float y, float z);
void RENDERER_GL_API_CALL GL_LogVertex3fv(const float *values);
void RENDERER_GL_API_CALL GL_LogVertex3i(int32_t x, int32_t y, int32_t z);
void RENDERER_GL_API_CALL GL_LogVertex3iv(const int32_t *values);
void RENDERER_GL_API_CALL GL_LogVertex3s(int16_t x, int16_t y, int16_t z);
void RENDERER_GL_API_CALL GL_LogVertex3sv(const int16_t *values);
void RENDERER_GL_API_CALL GL_LogVertex4d(double x, double y, double z, double w);
void RENDERER_GL_API_CALL GL_LogVertex4dv(const double *values);
void RENDERER_GL_API_CALL GL_LogVertex4f(float x, float y, float z, float w);
void RENDERER_GL_API_CALL GL_LogVertex4fv(const float *values);
void RENDERER_GL_API_CALL GL_LogVertex4i(int32_t x, int32_t y, int32_t z, int32_t w);
void RENDERER_GL_API_CALL GL_LogVertex4iv(const int32_t *values);
void RENDERER_GL_API_CALL GL_LogVertex4s(int16_t x, int16_t y, int16_t z, int16_t w);
void RENDERER_GL_API_CALL GL_LogVertex4sv(const int16_t *values);

#ifdef __cplusplus
}
#endif

#endif
