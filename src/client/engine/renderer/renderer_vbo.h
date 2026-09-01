#ifndef CODUOMP_RENDERER_VBO_H
#define CODUOMP_RENDERER_VBO_H

#include <stddef.h>
#include <stdint.h>

enum {
    R_STATIC_VERTEX_MEMORY_ALIGNMENT = 32
};

/* CoDUOMP.exe stores exactly one dword in this lane. Backend selection makes
 * that same lane an NV/client-memory address, an ARB buffer-object name, or an
 * ATI object-buffer name. Native 64-bit builds intentionally widen only the
 * pointer alternative. */
typedef union renderer_static_vertex_memory_base_u {
    uint8_t *address;
    uint32_t glBuffer;
    uint32_t atiObjectBuffer;
} renderer_static_vertex_memory_base_t;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(renderer_static_vertex_memory_base_t) == 0x4,
               "renderer_static_vertex_memory_base_t original alignment");
_Static_assert(offsetof(renderer_static_vertex_memory_base_t, address) == 0x0,
               "renderer_static_vertex_memory_base_t address offset");
_Static_assert(sizeof(((renderer_static_vertex_memory_base_t *)0)->address) ==
                   0x4,
               "renderer_static_vertex_memory_base_t address extent");
_Static_assert(offsetof(renderer_static_vertex_memory_base_t, glBuffer) == 0x0,
               "renderer_static_vertex_memory_base_t glBuffer offset");
_Static_assert(sizeof(((renderer_static_vertex_memory_base_t *)0)->glBuffer) ==
                   0x4,
               "renderer_static_vertex_memory_base_t glBuffer extent");
_Static_assert(offsetof(renderer_static_vertex_memory_base_t,
                        atiObjectBuffer) == 0x0,
               "renderer_static_vertex_memory_base_t atiObjectBuffer offset");
_Static_assert(sizeof(((renderer_static_vertex_memory_base_t *)0)
                          ->atiObjectBuffer) == 0x4,
               "renderer_static_vertex_memory_base_t atiObjectBuffer extent");
_Static_assert(sizeof(renderer_static_vertex_memory_base_t) == 0x4,
               "renderer_static_vertex_memory_base_t original size");
#endif

typedef enum renderer_static_vertex_memory_source_e {
    R_STATIC_VERTEX_MEMORY_NONE = 0,
    R_STATIC_VERTEX_MEMORY_PRIMARY = 1,
    R_STATIC_VERTEX_MEMORY_SECONDARY = 2,
    R_STATIC_VERTEX_MEMORY_HUNK = 3
} renderer_static_vertex_memory_source_t;

typedef enum renderer_vbo_refresh_components_e {
    R_VBO_REFRESH_VERTICES = 1,
    R_VBO_REFRESH_INDEXES = 2,
    R_VBO_REFRESH_ALL = 3
} renderer_vbo_refresh_components_t;

#ifdef __cplusplus
extern "C" {
#endif

uint32_t R_CreateBufferARB(uint32_t target, size_t size, const void *data,
                           uint32_t usage);
void R_DeleteBuffersARB(void);
renderer_static_vertex_memory_source_t R_AllocMemoryNV(
    renderer_static_vertex_memory_source_t firstSource, size_t size,
    uint8_t **memory);
renderer_static_vertex_memory_source_t R_AllocMemoryATI(
    renderer_static_vertex_memory_source_t firstSource, size_t size,
    size_t *offset);

#ifdef __cplusplus
}
#endif

#endif
