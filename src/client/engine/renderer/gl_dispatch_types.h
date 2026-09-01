#ifndef CODUOMP_RENDERER_GL_DISPATCH_TYPES_H
#define CODUOMP_RENDERER_GL_DISPATCH_TYPES_H

#include <stddef.h>
#include <stdint.h>

#if defined(_MSC_VER)
#define RENDERER_GL_API_CALL __stdcall
#elif defined(_WIN32) && defined(__i386__)
#define RENDERER_GL_API_CALL __attribute__((stdcall))
#else
#define RENDERER_GL_API_CALL
#endif

typedef void (RENDERER_GL_API_CALL *renderer_gl_alpha_func_t)(
    uint32_t func, float reference);
typedef void (RENDERER_GL_API_CALL *renderer_gl_begin_func_t)(uint32_t mode);
typedef void (RENDERER_GL_API_CALL *renderer_gl_bind_texture_func_t)(
    uint32_t target, uint32_t texture);
typedef void (RENDERER_GL_API_CALL *renderer_gl_bind_program_arb_func_t)(
    uint32_t target, uint32_t program);
typedef void (RENDERER_GL_API_CALL *renderer_gl_bind_fragment_shader_ati_func_t)(
    uint32_t shader);
typedef void (RENDERER_GL_API_CALL *renderer_gl_bind_buffer_arb_func_t)(
    uint32_t target, uint32_t buffer);
typedef void (RENDERER_GL_API_CALL *renderer_gl_buffer_data_arb_func_t)(
    uint32_t target, intptr_t size, const void *data, uint32_t usage);
typedef void *(RENDERER_GL_API_CALL *renderer_gl_map_buffer_arb_func_t)(
    uint32_t target, uint32_t access);
typedef uint8_t (RENDERER_GL_API_CALL *renderer_gl_unmap_buffer_arb_func_t)(
    uint32_t target);
typedef void (RENDERER_GL_API_CALL *renderer_gl_blend_func_t)(
    uint32_t sourceFactor, uint32_t destinationFactor);
typedef void (RENDERER_GL_API_CALL *renderer_gl_call_list_func_t)(
    uint32_t list);
typedef void (RENDERER_GL_API_CALL *renderer_gl_clear_func_t)(uint32_t mask);
typedef void (RENDERER_GL_API_CALL *renderer_gl_clear_depth_func_t)(
    double depth);
typedef void (RENDERER_GL_API_CALL *renderer_gl_clear_stencil_func_t)(
    int32_t stencil);
typedef void (RENDERER_GL_API_CALL *renderer_gl_color4f_func_t)(
    float red, float green, float blue, float alpha);
typedef void (RENDERER_GL_API_CALL *renderer_gl_color4fv_func_t)(
    const float *color);
typedef void (RENDERER_GL_API_CALL *renderer_gl_color_pointer_func_t)(
    int32_t size, uint32_t type, int32_t stride, const void *pointer);
typedef void (RENDERER_GL_API_CALL *renderer_gl_cull_face_func_t)(
    uint32_t mode);
typedef void (RENDERER_GL_API_CALL *renderer_gl_depth_func_t)(uint32_t func);
typedef void (RENDERER_GL_API_CALL *renderer_gl_depth_mask_func_t)(
    uint8_t enabled);
typedef void (RENDERER_GL_API_CALL *renderer_gl_depth_range_func_t)(
    double nearValue, double farValue);
typedef void (RENDERER_GL_API_CALL *renderer_gl_capability_func_t)(
    uint32_t capability);
typedef void (RENDERER_GL_API_CALL *renderer_gl_draw_elements_func_t)(
    uint32_t mode, int32_t count, uint32_t type, const void *indices);
typedef void (RENDERER_GL_API_CALL *renderer_gl_draw_range_elements_func_t)(
    uint32_t mode, uint32_t start, uint32_t end, int32_t count,
    uint32_t type, const void *indices);
typedef void (RENDERER_GL_API_CALL *renderer_gl_draw_buffer_func_t)(
    uint32_t buffer);
typedef void (RENDERER_GL_API_CALL *renderer_gl_hint_func_t)(
    uint32_t target, uint32_t mode);
typedef void (RENDERER_GL_API_CALL *renderer_gl_fogf_func_t)(
    uint32_t parameter, float value);
typedef void (RENDERER_GL_API_CALL *renderer_gl_fogfv_func_t)(
    uint32_t parameter, const float *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_fogi_func_t)(
    uint32_t parameter, int32_t value);
typedef void (RENDERER_GL_API_CALL *renderer_gl_polygon_mode_func_t)(
    uint32_t face, uint32_t mode);
typedef void (RENDERER_GL_API_CALL *renderer_gl_scissor_func_t)(
    int32_t x, int32_t y, int32_t width, int32_t height);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_coord_pointer_func_t)(
    int32_t size, uint32_t type, int32_t stride, const void *pointer);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_envf_func_t)(
    uint32_t target, uint32_t parameter, float value);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_envi_func_t)(
    uint32_t target, uint32_t parameter, int32_t value);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_parameterf_func_t)(
    uint32_t target, uint32_t parameter, float value);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_parameteri_func_t)(
    uint32_t target, uint32_t parameter, int32_t value);
typedef void (RENDERER_GL_API_CALL *renderer_gl_normal_pointer_func_t)(
    uint32_t type, int32_t stride, const void *pointer);
typedef void (RENDERER_GL_API_CALL *renderer_gl_vertex_pointer_func_t)(
    int32_t size, uint32_t type, int32_t stride, const void *pointer);
typedef void (RENDERER_GL_API_CALL *renderer_gl_viewport_func_t)(
    int32_t x, int32_t y, int32_t width, int32_t height);
typedef void (RENDERER_GL_API_CALL *renderer_gl_set_fence_nv_func_t)(
    uint32_t fence, uint32_t condition);
typedef void (RENDERER_GL_API_CALL *renderer_gl_finish_fence_nv_func_t)(
    uint32_t fence);
typedef void (RENDERER_GL_API_CALL *renderer_gl_active_texture_arb_func_t)(
    uint32_t texture);
typedef void (RENDERER_GL_API_CALL *renderer_gl_multi_tex_coord2f_arb_func_t)(
    uint32_t target, float s, float t);
typedef void (RENDERER_GL_API_CALL *renderer_gl_lock_arrays_ext_func_t)(
    int32_t first, int32_t count);
typedef void (RENDERER_GL_API_CALL *renderer_gl_void_func_t)(void);
typedef void (RENDERER_GL_API_CALL *renderer_gl_pn_trianglesi_ati_func_t)(
    uint32_t parameter, int32_t value);
typedef void (RENDERER_GL_API_CALL *renderer_gl_pn_trianglesf_ati_func_t)(
    uint32_t parameter, float value);
typedef void (RENDERER_GL_API_CALL *renderer_gl_compressed_tex_image_3d_func_t)(
    uint32_t target, int32_t level, uint32_t internalFormat, int32_t width,
    int32_t height, int32_t depth, int32_t border, int32_t imageSize,
    const void *data);
typedef void (RENDERER_GL_API_CALL *renderer_gl_compressed_tex_image_2d_func_t)(
    uint32_t target, int32_t level, uint32_t internalFormat, int32_t width,
    int32_t height, int32_t border, int32_t imageSize, const void *data);
typedef void (RENDERER_GL_API_CALL *renderer_gl_compressed_tex_image_1d_func_t)(
    uint32_t target, int32_t level, uint32_t internalFormat, int32_t width,
    int32_t border, int32_t imageSize, const void *data);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_compressed_tex_sub_image_3d_func_t)(
    uint32_t target, int32_t level, int32_t xOffset, int32_t yOffset,
    int32_t zOffset, int32_t width, int32_t height, int32_t depth,
    uint32_t format, int32_t imageSize, const void *data);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_compressed_tex_sub_image_2d_func_t)(
    uint32_t target, int32_t level, int32_t xOffset, int32_t yOffset,
    int32_t width, int32_t height, uint32_t format, int32_t imageSize,
    const void *data);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_compressed_tex_sub_image_1d_func_t)(
    uint32_t target, int32_t level, int32_t xOffset, int32_t width,
    uint32_t format, int32_t imageSize, const void *data);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_get_compressed_tex_image_func_t)(
    uint32_t target, int32_t level, void *image);
typedef void (RENDERER_GL_API_CALL *renderer_gl_delete_buffers_arb_func_t)(
    int32_t count, const uint32_t *buffers);
typedef void (RENDERER_GL_API_CALL *renderer_gl_gen_buffers_arb_func_t)(
    int32_t count, uint32_t *buffers);
typedef uint8_t (RENDERER_GL_API_CALL *renderer_gl_is_buffer_arb_func_t)(
    uint32_t buffer);
typedef void (RENDERER_GL_API_CALL *renderer_gl_buffer_sub_data_arb_func_t)(
    uint32_t target, intptr_t offset, intptr_t size, const void *data);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_get_buffer_sub_data_arb_func_t)(
    uint32_t target, intptr_t offset, intptr_t size, void *data);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_get_buffer_parameteriv_arb_func_t)(
    uint32_t target, uint32_t parameter, int32_t *values);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_get_buffer_pointerv_arb_func_t)(
    uint32_t target, uint32_t parameter, void **pointer);
typedef uint32_t (RENDERER_GL_API_CALL
                  *renderer_gl_new_object_buffer_ati_func_t)(
    int32_t size, const void *data, uint32_t usage);
typedef uint8_t (RENDERER_GL_API_CALL
                 *renderer_gl_is_object_buffer_ati_func_t)(uint32_t buffer);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_update_object_buffer_ati_func_t)(
    uint32_t buffer, uint32_t offset, int32_t size, const void *data,
    uint32_t preserveMode);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_get_object_bufferfv_ati_func_t)(
    uint32_t buffer, uint32_t parameter, float *values);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_get_object_bufferiv_ati_func_t)(
    uint32_t buffer, uint32_t parameter, int32_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_free_object_buffer_ati_func_t)(
    uint32_t buffer);
typedef void (RENDERER_GL_API_CALL *renderer_gl_array_object_ati_func_t)(
    uint32_t array, int32_t size, uint32_t type, int32_t stride,
    uint32_t buffer, uint32_t offset);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_get_array_objectfv_ati_func_t)(
    uint32_t array, uint32_t parameter, float *values);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_get_array_objectiv_ati_func_t)(
    uint32_t array, uint32_t parameter, int32_t *values);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_variant_array_object_ati_func_t)(
    uint32_t id, uint32_t type, int32_t stride, uint32_t buffer,
    uint32_t offset);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_get_variant_array_objectfv_ati_func_t)(
    uint32_t id, uint32_t parameter, float *values);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_get_variant_array_objectiv_ati_func_t)(
    uint32_t id, uint32_t parameter, int32_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_element_pointer_ati_func_t)(
    uint32_t type, const void *pointer);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_draw_element_array_ati_func_t)(
    uint32_t mode, int32_t count);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_draw_range_element_array_ati_func_t)(
    uint32_t mode, uint32_t start, uint32_t end, int32_t count);
typedef void (RENDERER_GL_API_CALL *renderer_gl_vertex_array_range_nv_func_t)(
    int32_t length, const void *pointer);
typedef void *(RENDERER_GL_API_CALL
               *renderer_gl_allocate_memory_nv_func_t)(
    int32_t size, float readFrequency, float writeFrequency, float priority);
typedef void (RENDERER_GL_API_CALL *renderer_gl_free_memory_nv_func_t)(
    void *memory);
typedef void (RENDERER_GL_API_CALL *renderer_gl_delete_fences_nv_func_t)(
    int32_t count, const uint32_t *fences);
typedef void (RENDERER_GL_API_CALL *renderer_gl_gen_fences_nv_func_t)(
    int32_t count, uint32_t *fences);
typedef uint8_t (RENDERER_GL_API_CALL *renderer_gl_fence_test_nv_func_t)(
    uint32_t fence);
typedef void (RENDERER_GL_API_CALL *renderer_gl_get_fenceiv_nv_func_t)(
    uint32_t fence, uint32_t parameter, int32_t *values);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_combiner_parameterfv_nv_func_t)(
    uint32_t parameter, const float *values);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_combiner_parameterf_nv_func_t)(
    uint32_t parameter, float value);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_combiner_parameteriv_nv_func_t)(
    uint32_t parameter, const int32_t *values);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_combiner_parameteri_nv_func_t)(
    uint32_t parameter, int32_t value);
typedef void (RENDERER_GL_API_CALL *renderer_gl_combiner_input_nv_func_t)(
    uint32_t stage, uint32_t portion, uint32_t variable, uint32_t input,
    uint32_t mapping, uint32_t componentUsage);
typedef void (RENDERER_GL_API_CALL *renderer_gl_combiner_output_nv_func_t)(
    uint32_t stage, uint32_t portion, uint32_t abOutput, uint32_t cdOutput,
    uint32_t sumOutput, uint32_t scale, uint32_t bias,
    uint8_t abDotProduct, uint8_t cdDotProduct, uint8_t muxSum);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_final_combiner_input_nv_func_t)(
    uint32_t variable, uint32_t input, uint32_t mapping,
    uint32_t componentUsage);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_get_combiner_input_parameterfv_nv_func_t)(
    uint32_t stage, uint32_t portion, uint32_t variable, uint32_t parameter,
    float *values);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_get_combiner_input_parameteriv_nv_func_t)(
    uint32_t stage, uint32_t portion, uint32_t variable, uint32_t parameter,
    int32_t *values);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_get_combiner_output_parameterfv_nv_func_t)(
    uint32_t stage, uint32_t portion, uint32_t parameter, float *values);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_get_combiner_output_parameteriv_nv_func_t)(
    uint32_t stage, uint32_t portion, uint32_t parameter, int32_t *values);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_get_final_combiner_input_parameterfv_nv_func_t)(
    uint32_t variable, uint32_t parameter, float *values);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_get_final_combiner_input_parameteriv_nv_func_t)(
    uint32_t variable, uint32_t parameter, int32_t *values);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_combiner_stage_parameterfv_nv_func_t)(
    uint32_t stage, uint32_t parameter, const float *values);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_get_combiner_stage_parameterfv_nv_func_t)(
    uint32_t stage, uint32_t parameter, float *values);
typedef uint32_t (RENDERER_GL_API_CALL
                  *renderer_gl_gen_fragment_shaders_ati_func_t)(
    uint32_t range);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_fragment_shader_texcoord_ati_func_t)(
    uint32_t destination, uint32_t coordinate, uint32_t swizzle);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_color_fragment_op1_ati_func_t)(
    uint32_t operation, uint32_t destination, uint32_t destinationMask,
    uint32_t destinationModifier, uint32_t argument1,
    uint32_t argument1Replication, uint32_t argument1Modifier);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_color_fragment_op2_ati_func_t)(
    uint32_t operation, uint32_t destination, uint32_t destinationMask,
    uint32_t destinationModifier, uint32_t argument1,
    uint32_t argument1Replication, uint32_t argument1Modifier,
    uint32_t argument2, uint32_t argument2Replication,
    uint32_t argument2Modifier);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_color_fragment_op3_ati_func_t)(
    uint32_t operation, uint32_t destination, uint32_t destinationMask,
    uint32_t destinationModifier, uint32_t argument1,
    uint32_t argument1Replication, uint32_t argument1Modifier,
    uint32_t argument2, uint32_t argument2Replication,
    uint32_t argument2Modifier, uint32_t argument3,
    uint32_t argument3Replication, uint32_t argument3Modifier);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_alpha_fragment_op1_ati_func_t)(
    uint32_t operation, uint32_t destination, uint32_t destinationModifier,
    uint32_t argument1, uint32_t argument1Replication,
    uint32_t argument1Modifier);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_alpha_fragment_op2_ati_func_t)(
    uint32_t operation, uint32_t destination, uint32_t destinationModifier,
    uint32_t argument1, uint32_t argument1Replication,
    uint32_t argument1Modifier, uint32_t argument2,
    uint32_t argument2Replication, uint32_t argument2Modifier);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_alpha_fragment_op3_ati_func_t)(
    uint32_t operation, uint32_t destination, uint32_t destinationModifier,
    uint32_t argument1, uint32_t argument1Replication,
    uint32_t argument1Modifier, uint32_t argument2,
    uint32_t argument2Replication, uint32_t argument2Modifier,
    uint32_t argument3, uint32_t argument3Replication,
    uint32_t argument3Modifier);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_set_fragment_shader_constant_ati_func_t)(
    uint32_t destination, const float *value);
typedef void (RENDERER_GL_API_CALL *renderer_gl_vertex_attrib1s_arb_func_t)(
    uint32_t index, int16_t x);
typedef void (RENDERER_GL_API_CALL *renderer_gl_vertex_attrib1f_arb_func_t)(
    uint32_t index, float x);
typedef void (RENDERER_GL_API_CALL *renderer_gl_vertex_attrib1d_arb_func_t)(
    uint32_t index, double x);
typedef void (RENDERER_GL_API_CALL *renderer_gl_vertex_attrib2s_arb_func_t)(
    uint32_t index, int16_t x, int16_t y);
typedef void (RENDERER_GL_API_CALL *renderer_gl_vertex_attrib2f_arb_func_t)(
    uint32_t index, float x, float y);
typedef void (RENDERER_GL_API_CALL *renderer_gl_vertex_attrib2d_arb_func_t)(
    uint32_t index, double x, double y);
typedef void (RENDERER_GL_API_CALL *renderer_gl_vertex_attrib3s_arb_func_t)(
    uint32_t index, int16_t x, int16_t y, int16_t z);
typedef void (RENDERER_GL_API_CALL *renderer_gl_vertex_attrib3f_arb_func_t)(
    uint32_t index, float x, float y, float z);
typedef void (RENDERER_GL_API_CALL *renderer_gl_vertex_attrib3d_arb_func_t)(
    uint32_t index, double x, double y, double z);
typedef void (RENDERER_GL_API_CALL *renderer_gl_vertex_attrib4s_arb_func_t)(
    uint32_t index, int16_t x, int16_t y, int16_t z, int16_t w);
typedef void (RENDERER_GL_API_CALL *renderer_gl_vertex_attrib4f_arb_func_t)(
    uint32_t index, float x, float y, float z, float w);
typedef void (RENDERER_GL_API_CALL *renderer_gl_vertex_attrib4d_arb_func_t)(
    uint32_t index, double x, double y, double z, double w);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_vertex_attrib4nub_arb_func_t)(
    uint32_t index, uint8_t x, uint8_t y, uint8_t z, uint8_t w);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_vertex_attrib_i8v_arb_func_t)(
    uint32_t index, const int8_t *values);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_vertex_attrib_i16v_arb_func_t)(
    uint32_t index, const int16_t *values);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_vertex_attrib_i32v_arb_func_t)(
    uint32_t index, const int32_t *values);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_vertex_attrib_u8v_arb_func_t)(
    uint32_t index, const uint8_t *values);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_vertex_attrib_u16v_arb_func_t)(
    uint32_t index, const uint16_t *values);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_vertex_attrib_u32v_arb_func_t)(
    uint32_t index, const uint32_t *values);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_vertex_attrib_fv_arb_func_t)(
    uint32_t index, const float *values);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_vertex_attrib_dv_arb_func_t)(
    uint32_t index, const double *values);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_vertex_attrib_pointer_arb_func_t)(
    uint32_t index, int32_t size, uint32_t type, uint8_t normalized,
    int32_t stride, const void *pointer);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_vertex_attrib_array_arb_func_t)(uint32_t index);
typedef void (RENDERER_GL_API_CALL *renderer_gl_program_string_arb_func_t)(
    uint32_t target, uint32_t format, int32_t length, const void *string);
typedef void (RENDERER_GL_API_CALL *renderer_gl_delete_programs_arb_func_t)(
    int32_t count, const uint32_t *programs);
typedef void (RENDERER_GL_API_CALL *renderer_gl_gen_programs_arb_func_t)(
    int32_t count, uint32_t *programs);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_program_parameter4f_arb_func_t)(
    uint32_t target, uint32_t index, float x, float y, float z, float w);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_program_parameter4d_arb_func_t)(
    uint32_t target, uint32_t index,
    double x, double y, double z, double w);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_program_parameterfv_arb_func_t)(
    uint32_t target, uint32_t index, const float *values);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_program_parameterdv_arb_func_t)(
    uint32_t target, uint32_t index, const double *values);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_get_program_parameterfv_arb_func_t)(
    uint32_t target, uint32_t index, float *values);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_get_program_parameterdv_arb_func_t)(
    uint32_t target, uint32_t index, double *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_get_programiv_arb_func_t)(
    uint32_t target, uint32_t parameter, int32_t *values);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_get_program_string_arb_func_t)(
    uint32_t target, uint32_t parameter, void *string);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_get_vertex_attribdv_arb_func_t)(
    uint32_t index, uint32_t parameter, double *values);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_get_vertex_attribfv_arb_func_t)(
    uint32_t index, uint32_t parameter, float *values);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_get_vertex_attribiv_arb_func_t)(
    uint32_t index, uint32_t parameter, int32_t *values);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_get_vertex_attrib_pointerv_arb_func_t)(
    uint32_t index, uint32_t parameter, void **pointer);
typedef uint8_t (RENDERER_GL_API_CALL *renderer_gl_is_program_arb_func_t)(
    uint32_t program);
typedef void (RENDERER_GL_API_CALL *renderer_gl_accum_func_t)(
    uint32_t operation, float value);
typedef uint8_t (RENDERER_GL_API_CALL
                 *renderer_gl_are_textures_resident_func_t)(
    int32_t count, const uint32_t *textures, uint8_t *residences);
typedef void (RENDERER_GL_API_CALL *renderer_gl_array_element_func_t)(
    int32_t index);
typedef void (RENDERER_GL_API_CALL *renderer_gl_bitmap_func_t)(
    int32_t width, int32_t height, float xOrigin, float yOrigin,
    float xMove, float yMove, const uint8_t *bitmap);
typedef void (RENDERER_GL_API_CALL *renderer_gl_call_lists_func_t)(
    int32_t count, uint32_t type, const void *lists);
typedef void (RENDERER_GL_API_CALL *renderer_gl_clear_color4_func_t)(
    float red, float green, float blue, float alpha);
typedef void (RENDERER_GL_API_CALL *renderer_gl_clear_index_func_t)(
    float index);
typedef void (RENDERER_GL_API_CALL *renderer_gl_clip_plane_func_t)(
    uint32_t plane, const double *equation);
typedef void (RENDERER_GL_API_CALL *renderer_gl_color3b_func_t)(
    int8_t red, int8_t green, int8_t blue);
typedef void (RENDERER_GL_API_CALL *renderer_gl_color3bv_func_t)(
    const int8_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_color3d_func_t)(
    double red, double green, double blue);
typedef void (RENDERER_GL_API_CALL *renderer_gl_color3dv_func_t)(
    const double *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_color3f_func_t)(
    float red, float green, float blue);
typedef void (RENDERER_GL_API_CALL *renderer_gl_color3fv_func_t)(
    const float *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_color3i_func_t)(
    int32_t red, int32_t green, int32_t blue);
typedef void (RENDERER_GL_API_CALL *renderer_gl_color3iv_func_t)(
    const int32_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_color3s_func_t)(
    int16_t red, int16_t green, int16_t blue);
typedef void (RENDERER_GL_API_CALL *renderer_gl_color3sv_func_t)(
    const int16_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_color3ub_func_t)(
    uint8_t red, uint8_t green, uint8_t blue);
typedef void (RENDERER_GL_API_CALL *renderer_gl_color3ubv_func_t)(
    const uint8_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_color3ui_func_t)(
    uint32_t red, uint32_t green, uint32_t blue);
typedef void (RENDERER_GL_API_CALL *renderer_gl_color3uiv_func_t)(
    const uint32_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_color3us_func_t)(
    uint16_t red, uint16_t green, uint16_t blue);
typedef void (RENDERER_GL_API_CALL *renderer_gl_color3usv_func_t)(
    const uint16_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_color4b_func_t)(
    int8_t red, int8_t green, int8_t blue, int8_t alpha);
typedef void (RENDERER_GL_API_CALL *renderer_gl_color4bv_func_t)(
    const int8_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_color4d_func_t)(
    double red, double green, double blue, double alpha);
typedef void (RENDERER_GL_API_CALL *renderer_gl_color4dv_func_t)(
    const double *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_color4i_func_t)(
    int32_t red, int32_t green, int32_t blue, int32_t alpha);
typedef void (RENDERER_GL_API_CALL *renderer_gl_color4iv_func_t)(
    const int32_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_color4s_func_t)(
    int16_t red, int16_t green, int16_t blue, int16_t alpha);
typedef void (RENDERER_GL_API_CALL *renderer_gl_color4sv_func_t)(
    const int16_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_color4ub_func_t)(
    uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha);
typedef void (RENDERER_GL_API_CALL *renderer_gl_color4ubv_func_t)(
    const uint8_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_color4ui_func_t)(
    uint32_t red, uint32_t green, uint32_t blue, uint32_t alpha);
typedef void (RENDERER_GL_API_CALL *renderer_gl_color4uiv_func_t)(
    const uint32_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_color4us_func_t)(
    uint16_t red, uint16_t green, uint16_t blue, uint16_t alpha);
typedef void (RENDERER_GL_API_CALL *renderer_gl_color4usv_func_t)(
    const uint16_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_color_mask_func_t)(
    uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha);
typedef void (RENDERER_GL_API_CALL *renderer_gl_color_material_func_t)(
    uint32_t face, uint32_t mode);
typedef void (RENDERER_GL_API_CALL *renderer_gl_copy_pixels_func_t)(
    int32_t x, int32_t y, int32_t width, int32_t height, uint32_t type);
typedef void (RENDERER_GL_API_CALL *renderer_gl_copy_tex_image_1d_func_t)(
    uint32_t target, int32_t level, uint32_t internalFormat,
    int32_t x, int32_t y, int32_t width, int32_t border);
typedef void (RENDERER_GL_API_CALL *renderer_gl_copy_tex_image_2d_func_t)(
    uint32_t target, int32_t level, uint32_t internalFormat,
    int32_t x, int32_t y, int32_t width, int32_t height, int32_t border);
typedef void (RENDERER_GL_API_CALL *renderer_gl_copy_tex_sub_image_1d_func_t)(
    uint32_t target, int32_t level, int32_t xOffset,
    int32_t x, int32_t y, int32_t width);
typedef void (RENDERER_GL_API_CALL *renderer_gl_copy_tex_sub_image_2d_func_t)(
    uint32_t target, int32_t level, int32_t xOffset, int32_t yOffset,
    int32_t x, int32_t y, int32_t width, int32_t height);
typedef void (RENDERER_GL_API_CALL *renderer_gl_delete_lists_func_t)(
    uint32_t list, int32_t range);
typedef void (RENDERER_GL_API_CALL *renderer_gl_delete_textures_func_t)(
    int32_t count, const uint32_t *textures);
typedef void (RENDERER_GL_API_CALL *renderer_gl_draw_arrays_func_t)(
    uint32_t mode, int32_t first, int32_t count);
typedef void (RENDERER_GL_API_CALL *renderer_gl_draw_pixels_func_t)(
    int32_t width, int32_t height, uint32_t format, uint32_t type,
    const void *pixels);
typedef void (RENDERER_GL_API_CALL *renderer_gl_edge_flag_func_t)(
    uint8_t flag);
typedef void (RENDERER_GL_API_CALL *renderer_gl_edge_flag_pointer_func_t)(
    int32_t stride, const void *pointer);
typedef void (RENDERER_GL_API_CALL *renderer_gl_edge_flagv_func_t)(
    const uint8_t *flag);
typedef void (RENDERER_GL_API_CALL *renderer_gl_eval_coord1d_func_t)(double u);
typedef void (RENDERER_GL_API_CALL *renderer_gl_eval_coord1dv_func_t)(
    const double *u);
typedef void (RENDERER_GL_API_CALL *renderer_gl_eval_coord1f_func_t)(float u);
typedef void (RENDERER_GL_API_CALL *renderer_gl_eval_coord1fv_func_t)(
    const float *u);
typedef void (RENDERER_GL_API_CALL *renderer_gl_eval_coord2d_func_t)(
    double u, double v);
typedef void (RENDERER_GL_API_CALL *renderer_gl_eval_coord2dv_func_t)(
    const double *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_eval_coord2f_func_t)(
    float u, float v);
typedef void (RENDERER_GL_API_CALL *renderer_gl_eval_coord2fv_func_t)(
    const float *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_eval_mesh1_func_t)(
    uint32_t mode, int32_t i1, int32_t i2);
typedef void (RENDERER_GL_API_CALL *renderer_gl_eval_mesh2_func_t)(
    uint32_t mode, int32_t i1, int32_t i2, int32_t j1, int32_t j2);
typedef void (RENDERER_GL_API_CALL *renderer_gl_eval_point1_func_t)(int32_t i);
typedef void (RENDERER_GL_API_CALL *renderer_gl_eval_point2_func_t)(
    int32_t i, int32_t j);
typedef void (RENDERER_GL_API_CALL *renderer_gl_feedback_buffer_func_t)(
    int32_t size, uint32_t type, float *buffer);
typedef void (RENDERER_GL_API_CALL *renderer_gl_fogiv_func_t)(
    uint32_t parameter, const int32_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_front_face_func_t)(
    uint32_t mode);
typedef void (RENDERER_GL_API_CALL *renderer_gl_frustum_func_t)(
    double left, double right, double bottom, double top,
    double nearValue, double farValue);
typedef uint32_t (RENDERER_GL_API_CALL *renderer_gl_gen_lists_func_t)(
    int32_t range);
typedef void (RENDERER_GL_API_CALL *renderer_gl_gen_textures_func_t)(
    int32_t count, uint32_t *textures);
typedef void (RENDERER_GL_API_CALL *renderer_gl_get_booleanv_func_t)(
    uint32_t parameter, uint8_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_get_clip_plane_func_t)(
    uint32_t plane, double *equation);
typedef void (RENDERER_GL_API_CALL *renderer_gl_get_doublev_func_t)(
    uint32_t parameter, double *values);
typedef uint32_t (RENDERER_GL_API_CALL *renderer_gl_get_error_func_t)(void);
typedef void (RENDERER_GL_API_CALL *renderer_gl_get_floatv_func_t)(
    uint32_t parameter, float *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_get_integerv_func_t)(
    uint32_t parameter, int32_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_get_lightfv_func_t)(
    uint32_t light, uint32_t parameter, float *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_get_lightiv_func_t)(
    uint32_t light, uint32_t parameter, int32_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_get_mapdv_func_t)(
    uint32_t target, uint32_t query, double *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_get_mapfv_func_t)(
    uint32_t target, uint32_t query, float *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_get_mapiv_func_t)(
    uint32_t target, uint32_t query, int32_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_get_materialfv_func_t)(
    uint32_t face, uint32_t parameter, float *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_get_materialiv_func_t)(
    uint32_t face, uint32_t parameter, int32_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_get_pixel_mapfv_func_t)(
    uint32_t map, float *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_get_pixel_mapuiv_func_t)(
    uint32_t map, uint32_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_get_pixel_mapusv_func_t)(
    uint32_t map, uint16_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_get_pointerv_func_t)(
    uint32_t parameter, void **value);
typedef void (RENDERER_GL_API_CALL *renderer_gl_get_polygon_stipple_func_t)(
    uint8_t *mask);
typedef const uint8_t *(RENDERER_GL_API_CALL *renderer_gl_get_string_func_t)(
    uint32_t name);
typedef void (RENDERER_GL_API_CALL *renderer_gl_get_tex_envfv_func_t)(
    uint32_t target, uint32_t parameter, float *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_get_tex_enviv_func_t)(
    uint32_t target, uint32_t parameter, int32_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_get_tex_gendv_func_t)(
    uint32_t coordinate, uint32_t parameter, double *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_get_tex_genfv_func_t)(
    uint32_t coordinate, uint32_t parameter, float *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_get_tex_geniv_func_t)(
    uint32_t coordinate, uint32_t parameter, int32_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_get_tex_image_func_t)(
    uint32_t target, int32_t level, uint32_t format, uint32_t type,
    void *pixels);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_get_tex_level_parameterfv_func_t)(
    uint32_t target, int32_t level, uint32_t parameter, float *values);
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_get_tex_level_parameteriv_func_t)(
    uint32_t target, int32_t level, uint32_t parameter, int32_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_get_tex_parameterfv_func_t)(
    uint32_t target, uint32_t parameter, float *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_get_tex_parameteriv_func_t)(
    uint32_t target, uint32_t parameter, int32_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_index_mask_func_t)(
    uint32_t mask);
typedef void (RENDERER_GL_API_CALL *renderer_gl_index_pointer_func_t)(
    uint32_t type, int32_t stride, const void *pointer);
typedef void (RENDERER_GL_API_CALL *renderer_gl_indexd_func_t)(double index);
typedef void (RENDERER_GL_API_CALL *renderer_gl_indexdv_func_t)(
    const double *index);
typedef void (RENDERER_GL_API_CALL *renderer_gl_indexf_func_t)(float index);
typedef void (RENDERER_GL_API_CALL *renderer_gl_indexfv_func_t)(
    const float *index);
typedef void (RENDERER_GL_API_CALL *renderer_gl_indexi_func_t)(int32_t index);
typedef void (RENDERER_GL_API_CALL *renderer_gl_indexiv_func_t)(
    const int32_t *index);
typedef void (RENDERER_GL_API_CALL *renderer_gl_indexs_func_t)(int16_t index);
typedef void (RENDERER_GL_API_CALL *renderer_gl_indexsv_func_t)(
    const int16_t *index);
typedef void (RENDERER_GL_API_CALL *renderer_gl_indexub_func_t)(uint8_t index);
typedef void (RENDERER_GL_API_CALL *renderer_gl_indexubv_func_t)(
    const uint8_t *index);
typedef void (RENDERER_GL_API_CALL *renderer_gl_interleaved_arrays_func_t)(
    uint32_t format, int32_t stride, const void *pointer);
typedef uint8_t (RENDERER_GL_API_CALL *renderer_gl_is_enabled_func_t)(
    uint32_t capability);
typedef uint8_t (RENDERER_GL_API_CALL *renderer_gl_is_list_func_t)(
    uint32_t list);
typedef uint8_t (RENDERER_GL_API_CALL *renderer_gl_is_texture_func_t)(
    uint32_t texture);
typedef void (RENDERER_GL_API_CALL *renderer_gl_light_modelf_func_t)(
    uint32_t parameter, float value);
typedef void (RENDERER_GL_API_CALL *renderer_gl_light_modelfv_func_t)(
    uint32_t parameter, const float *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_light_modeli_func_t)(
    uint32_t parameter, int32_t value);
typedef void (RENDERER_GL_API_CALL *renderer_gl_light_modeliv_func_t)(
    uint32_t parameter, const int32_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_lightf_func_t)(
    uint32_t light, uint32_t parameter, float value);
typedef void (RENDERER_GL_API_CALL *renderer_gl_lightfv_func_t)(
    uint32_t light, uint32_t parameter, const float *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_lighti_func_t)(
    uint32_t light, uint32_t parameter, int32_t value);
typedef void (RENDERER_GL_API_CALL *renderer_gl_lightiv_func_t)(
    uint32_t light, uint32_t parameter, const int32_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_line_stipple_func_t)(
    int32_t factor, uint16_t pattern);
typedef void (RENDERER_GL_API_CALL *renderer_gl_line_width_func_t)(float width);
typedef void (RENDERER_GL_API_CALL *renderer_gl_list_base_func_t)(
    uint32_t base);
typedef void (RENDERER_GL_API_CALL *renderer_gl_load_matrixd_func_t)(
    const double *matrix);
typedef void (RENDERER_GL_API_CALL *renderer_gl_load_matrixf_func_t)(
    const float *matrix);
typedef void (RENDERER_GL_API_CALL *renderer_gl_load_name_func_t)(
    uint32_t name);
typedef void (RENDERER_GL_API_CALL *renderer_gl_logic_op_func_t)(
    uint32_t operation);
typedef void (RENDERER_GL_API_CALL *renderer_gl_map1d_func_t)(
    uint32_t target, double u1, double u2, int32_t stride, int32_t order,
    const double *points);
typedef void (RENDERER_GL_API_CALL *renderer_gl_map1f_func_t)(
    uint32_t target, float u1, float u2, int32_t stride, int32_t order,
    const float *points);
typedef void (RENDERER_GL_API_CALL *renderer_gl_map2d_func_t)(
    uint32_t target, double u1, double u2, int32_t uStride, int32_t uOrder,
    double v1, double v2, int32_t vStride, int32_t vOrder,
    const double *points);
typedef void (RENDERER_GL_API_CALL *renderer_gl_map2f_func_t)(
    uint32_t target, float u1, float u2, int32_t uStride, int32_t uOrder,
    float v1, float v2, int32_t vStride, int32_t vOrder,
    const float *points);
typedef void (RENDERER_GL_API_CALL *renderer_gl_map_grid1d_func_t)(
    int32_t count, double u1, double u2);
typedef void (RENDERER_GL_API_CALL *renderer_gl_map_grid1f_func_t)(
    int32_t count, float u1, float u2);
typedef void (RENDERER_GL_API_CALL *renderer_gl_map_grid2d_func_t)(
    int32_t uCount, double u1, double u2,
    int32_t vCount, double v1, double v2);
typedef void (RENDERER_GL_API_CALL *renderer_gl_map_grid2f_func_t)(
    int32_t uCount, float u1, float u2,
    int32_t vCount, float v1, float v2);
typedef void (RENDERER_GL_API_CALL *renderer_gl_materialf_func_t)(
    uint32_t face, uint32_t parameter, float value);
typedef void (RENDERER_GL_API_CALL *renderer_gl_materialfv_func_t)(
    uint32_t face, uint32_t parameter, const float *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_materiali_func_t)(
    uint32_t face, uint32_t parameter, int32_t value);
typedef void (RENDERER_GL_API_CALL *renderer_gl_materialiv_func_t)(
    uint32_t face, uint32_t parameter, const int32_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_matrix_mode_func_t)(
    uint32_t mode);
typedef void (RENDERER_GL_API_CALL *renderer_gl_mult_matrixd_func_t)(
    const double *matrix);
typedef void (RENDERER_GL_API_CALL *renderer_gl_mult_matrixf_func_t)(
    const float *matrix);
typedef void (RENDERER_GL_API_CALL *renderer_gl_new_list_func_t)(
    uint32_t list, uint32_t mode);
typedef void (RENDERER_GL_API_CALL *renderer_gl_normal3b_func_t)(
    int8_t x, int8_t y, int8_t z);
typedef void (RENDERER_GL_API_CALL *renderer_gl_normal3bv_func_t)(
    const int8_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_normal3d_func_t)(
    double x, double y, double z);
typedef void (RENDERER_GL_API_CALL *renderer_gl_normal3dv_func_t)(
    const double *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_normal3f_func_t)(
    float x, float y, float z);
typedef void (RENDERER_GL_API_CALL *renderer_gl_normal3fv_func_t)(
    const float *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_normal3i_func_t)(
    int32_t x, int32_t y, int32_t z);
typedef void (RENDERER_GL_API_CALL *renderer_gl_normal3iv_func_t)(
    const int32_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_normal3s_func_t)(
    int16_t x, int16_t y, int16_t z);
typedef void (RENDERER_GL_API_CALL *renderer_gl_normal3sv_func_t)(
    const int16_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_ortho_func_t)(
    double left, double right, double bottom, double top,
    double nearValue, double farValue);
typedef void (RENDERER_GL_API_CALL *renderer_gl_pass_through_func_t)(
    float token);
typedef void (RENDERER_GL_API_CALL *renderer_gl_pixel_mapfv_func_t)(
    uint32_t map, int32_t mapSize, const float *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_pixel_mapuiv_func_t)(
    uint32_t map, int32_t mapSize, const uint32_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_pixel_mapusv_func_t)(
    uint32_t map, int32_t mapSize, const uint16_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_pixel_storef_func_t)(
    uint32_t parameter, float value);
typedef void (RENDERER_GL_API_CALL *renderer_gl_pixel_storei_func_t)(
    uint32_t parameter, int32_t value);
typedef void (RENDERER_GL_API_CALL *renderer_gl_pixel_transferf_func_t)(
    uint32_t parameter, float value);
typedef void (RENDERER_GL_API_CALL *renderer_gl_pixel_transferi_func_t)(
    uint32_t parameter, int32_t value);
typedef void (RENDERER_GL_API_CALL *renderer_gl_pixel_zoom_func_t)(
    float xFactor, float yFactor);
typedef void (RENDERER_GL_API_CALL *renderer_gl_point_size_func_t)(
    float size);
typedef void (RENDERER_GL_API_CALL *renderer_gl_polygon_offset_func_t)(
    float factor, float units);
typedef void (RENDERER_GL_API_CALL *renderer_gl_polygon_stipple_func_t)(
    const uint8_t *mask);
typedef void (RENDERER_GL_API_CALL *renderer_gl_prioritize_textures_func_t)(
    int32_t count, const uint32_t *textures, const float *priorities);
typedef void (RENDERER_GL_API_CALL *renderer_gl_attrib_mask_func_t)(
    uint32_t mask);
typedef void (RENDERER_GL_API_CALL *renderer_gl_push_name_func_t)(
    uint32_t name);
typedef void (RENDERER_GL_API_CALL *renderer_gl_raster_pos2d_func_t)(
    double x, double y);
typedef void (RENDERER_GL_API_CALL *renderer_gl_raster_pos2dv_func_t)(
    const double *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_raster_pos2f_func_t)(
    float x, float y);
typedef void (RENDERER_GL_API_CALL *renderer_gl_raster_pos2fv_func_t)(
    const float *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_raster_pos2i_func_t)(
    int32_t x, int32_t y);
typedef void (RENDERER_GL_API_CALL *renderer_gl_raster_pos2iv_func_t)(
    const int32_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_raster_pos2s_func_t)(
    int16_t x, int16_t y);
typedef void (RENDERER_GL_API_CALL *renderer_gl_raster_pos2sv_func_t)(
    const int16_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_raster_pos3d_func_t)(
    double x, double y, double z);
typedef void (RENDERER_GL_API_CALL *renderer_gl_raster_pos3dv_func_t)(
    const double *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_raster_pos3f_func_t)(
    float x, float y, float z);
typedef void (RENDERER_GL_API_CALL *renderer_gl_raster_pos3fv_func_t)(
    const float *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_raster_pos3i_func_t)(
    int32_t x, int32_t y, int32_t z);
typedef void (RENDERER_GL_API_CALL *renderer_gl_raster_pos3iv_func_t)(
    const int32_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_raster_pos3s_func_t)(
    int16_t x, int16_t y, int16_t z);
typedef void (RENDERER_GL_API_CALL *renderer_gl_raster_pos3sv_func_t)(
    const int16_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_raster_pos4d_func_t)(
    double x, double y, double z, double w);
typedef void (RENDERER_GL_API_CALL *renderer_gl_raster_pos4dv_func_t)(
    const double *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_raster_pos4f_func_t)(
    float x, float y, float z, float w);
typedef void (RENDERER_GL_API_CALL *renderer_gl_raster_pos4fv_func_t)(
    const float *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_raster_pos4i_func_t)(
    int32_t x, int32_t y, int32_t z, int32_t w);
typedef void (RENDERER_GL_API_CALL *renderer_gl_raster_pos4iv_func_t)(
    const int32_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_raster_pos4s_func_t)(
    int16_t x, int16_t y, int16_t z, int16_t w);
typedef void (RENDERER_GL_API_CALL *renderer_gl_raster_pos4sv_func_t)(
    const int16_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_read_buffer_func_t)(
    uint32_t mode);
typedef void (RENDERER_GL_API_CALL *renderer_gl_read_pixels_func_t)(
    int32_t x, int32_t y, int32_t width, int32_t height,
    uint32_t format, uint32_t type, void *pixels);
typedef void (RENDERER_GL_API_CALL *renderer_gl_rectd_func_t)(
    double x1, double y1, double x2, double y2);
typedef void (RENDERER_GL_API_CALL *renderer_gl_rectdv_func_t)(
    const double *vertex1, const double *vertex2);
typedef void (RENDERER_GL_API_CALL *renderer_gl_rectf_func_t)(
    float x1, float y1, float x2, float y2);
typedef void (RENDERER_GL_API_CALL *renderer_gl_rectfv_func_t)(
    const float *vertex1, const float *vertex2);
typedef void (RENDERER_GL_API_CALL *renderer_gl_recti_func_t)(
    int32_t x1, int32_t y1, int32_t x2, int32_t y2);
typedef void (RENDERER_GL_API_CALL *renderer_gl_rectiv_func_t)(
    const int32_t *vertex1, const int32_t *vertex2);
typedef void (RENDERER_GL_API_CALL *renderer_gl_rects_func_t)(
    int16_t x1, int16_t y1, int16_t x2, int16_t y2);
typedef void (RENDERER_GL_API_CALL *renderer_gl_rectsv_func_t)(
    const int16_t *vertex1, const int16_t *vertex2);
typedef int32_t (RENDERER_GL_API_CALL *renderer_gl_render_mode_func_t)(
    uint32_t mode);
typedef void (RENDERER_GL_API_CALL *renderer_gl_rotated_func_t)(
    double angle, double x, double y, double z);
typedef void (RENDERER_GL_API_CALL *renderer_gl_rotatef_func_t)(
    float angle, float x, float y, float z);
typedef void (RENDERER_GL_API_CALL *renderer_gl_scaled_func_t)(
    double x, double y, double z);
typedef void (RENDERER_GL_API_CALL *renderer_gl_scalef_func_t)(
    float x, float y, float z);
typedef void (RENDERER_GL_API_CALL *renderer_gl_select_buffer_func_t)(
    int32_t size, uint32_t *buffer);
typedef void (RENDERER_GL_API_CALL *renderer_gl_shade_model_func_t)(
    uint32_t mode);
typedef void (RENDERER_GL_API_CALL *renderer_gl_stencil_func_t)(
    uint32_t func, int32_t reference, uint32_t mask);
typedef void (RENDERER_GL_API_CALL *renderer_gl_stencil_mask_func_t)(
    uint32_t mask);
typedef void (RENDERER_GL_API_CALL *renderer_gl_stencil_op_func_t)(
    uint32_t stencilFail, uint32_t depthFail, uint32_t depthPass);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_coord1d_func_t)(double s);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_coord1dv_func_t)(
    const double *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_coord1f_func_t)(float s);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_coord1fv_func_t)(
    const float *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_coord1i_func_t)(int32_t s);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_coord1iv_func_t)(
    const int32_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_coord1s_func_t)(int16_t s);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_coord1sv_func_t)(
    const int16_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_coord2d_func_t)(
    double s, double t);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_coord2dv_func_t)(
    const double *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_coord2f_func_t)(
    float s, float t);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_coord2fv_func_t)(
    const float *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_coord2i_func_t)(
    int32_t s, int32_t t);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_coord2iv_func_t)(
    const int32_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_coord2s_func_t)(
    int16_t s, int16_t t);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_coord2sv_func_t)(
    const int16_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_coord3d_func_t)(
    double s, double t, double r);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_coord3dv_func_t)(
    const double *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_coord3f_func_t)(
    float s, float t, float r);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_coord3fv_func_t)(
    const float *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_coord3i_func_t)(
    int32_t s, int32_t t, int32_t r);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_coord3iv_func_t)(
    const int32_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_coord3s_func_t)(
    int16_t s, int16_t t, int16_t r);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_coord3sv_func_t)(
    const int16_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_coord4d_func_t)(
    double s, double t, double r, double q);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_coord4dv_func_t)(
    const double *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_coord4f_func_t)(
    float s, float t, float r, float q);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_coord4fv_func_t)(
    const float *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_coord4i_func_t)(
    int32_t s, int32_t t, int32_t r, int32_t q);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_coord4iv_func_t)(
    const int32_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_coord4s_func_t)(
    int16_t s, int16_t t, int16_t r, int16_t q);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_coord4sv_func_t)(
    const int16_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_envfv_func_t)(
    uint32_t target, uint32_t parameter, const float *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_enviv_func_t)(
    uint32_t target, uint32_t parameter, const int32_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_gend_func_t)(
    uint32_t coordinate, uint32_t parameter, double value);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_gendv_func_t)(
    uint32_t coordinate, uint32_t parameter, const double *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_genf_func_t)(
    uint32_t coordinate, uint32_t parameter, float value);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_genfv_func_t)(
    uint32_t coordinate, uint32_t parameter, const float *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_geni_func_t)(
    uint32_t coordinate, uint32_t parameter, int32_t value);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_geniv_func_t)(
    uint32_t coordinate, uint32_t parameter, const int32_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_image_1d_func_t)(
    uint32_t target, int32_t level, int32_t internalFormat, int32_t width,
    int32_t border, uint32_t format, uint32_t type, const void *pixels);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_image_2d_func_t)(
    uint32_t target, int32_t level, int32_t internalFormat, int32_t width,
    int32_t height, int32_t border, uint32_t format, uint32_t type,
    const void *pixels);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_parameterfv_func_t)(
    uint32_t target, uint32_t parameter, const float *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_parameteriv_func_t)(
    uint32_t target, uint32_t parameter, const int32_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_sub_image_1d_func_t)(
    uint32_t target, int32_t level, int32_t xOffset, int32_t width,
    uint32_t format, uint32_t type, const void *pixels);
typedef void (RENDERER_GL_API_CALL *renderer_gl_tex_sub_image_2d_func_t)(
    uint32_t target, int32_t level, int32_t xOffset, int32_t yOffset,
    int32_t width, int32_t height, uint32_t format, uint32_t type,
    const void *pixels);
typedef void (RENDERER_GL_API_CALL *renderer_gl_translated_func_t)(
    double x, double y, double z);
typedef void (RENDERER_GL_API_CALL *renderer_gl_translatef_func_t)(
    float x, float y, float z);
typedef void (RENDERER_GL_API_CALL *renderer_gl_vertex2d_func_t)(
    double x, double y);
typedef void (RENDERER_GL_API_CALL *renderer_gl_vertex2dv_func_t)(
    const double *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_vertex2f_func_t)(
    float x, float y);
typedef void (RENDERER_GL_API_CALL *renderer_gl_vertex2fv_func_t)(
    const float *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_vertex2i_func_t)(
    int32_t x, int32_t y);
typedef void (RENDERER_GL_API_CALL *renderer_gl_vertex2iv_func_t)(
    const int32_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_vertex2s_func_t)(
    int16_t x, int16_t y);
typedef void (RENDERER_GL_API_CALL *renderer_gl_vertex2sv_func_t)(
    const int16_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_vertex3d_func_t)(
    double x, double y, double z);
typedef void (RENDERER_GL_API_CALL *renderer_gl_vertex3dv_func_t)(
    const double *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_vertex3f_func_t)(
    float x, float y, float z);
typedef void (RENDERER_GL_API_CALL *renderer_gl_vertex3fv_func_t)(
    const float *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_vertex3i_func_t)(
    int32_t x, int32_t y, int32_t z);
typedef void (RENDERER_GL_API_CALL *renderer_gl_vertex3iv_func_t)(
    const int32_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_vertex3s_func_t)(
    int16_t x, int16_t y, int16_t z);
typedef void (RENDERER_GL_API_CALL *renderer_gl_vertex3sv_func_t)(
    const int16_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_vertex4d_func_t)(
    double x, double y, double z, double w);
typedef void (RENDERER_GL_API_CALL *renderer_gl_vertex4dv_func_t)(
    const double *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_vertex4f_func_t)(
    float x, float y, float z, float w);
typedef void (RENDERER_GL_API_CALL *renderer_gl_vertex4fv_func_t)(
    const float *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_vertex4i_func_t)(
    int32_t x, int32_t y, int32_t z, int32_t w);
typedef void (RENDERER_GL_API_CALL *renderer_gl_vertex4iv_func_t)(
    const int32_t *values);
typedef void (RENDERER_GL_API_CALL *renderer_gl_vertex4s_func_t)(
    int16_t x, int16_t y, int16_t z, int16_t w);
typedef void (RENDERER_GL_API_CALL *renderer_gl_vertex4sv_func_t)(
    const int16_t *values);

#endif
