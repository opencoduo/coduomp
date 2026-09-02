#include "backend.h"

#include "gl_api.h"
#include "gl_state.h"
#include "renderer_cvars.h"
#include "qcommon/hunk.h"
#include "../platform/crt_boundary.h"

enum {
    R_DEFAULT_NV_AGP_MEMORY_BYTES = 13 * 1024 * 1024
};

/* Source: CoDUOMP.exe 0x004c89a0..0x004c8a16.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c89a0_004c8a16.mcode; its body is
 * also inlined byte-for-byte into R_OptimizeRigidXSurfaceNV at 0x0049f918.
 * Name and source-level boundary: same-module Mac R_AllocMemoryNV and its call
 * graph. The strict-less-than pool test is intentional: an allocation ending
 * exactly at either limit falls through to the next source. */
renderer_static_vertex_memory_source_t R_AllocMemoryNV(
    renderer_static_vertex_memory_source_t firstSource, size_t size,
    uint8_t **memory)
{
    /* ADD/AND at 0x004c89a0..0x004c89a3 and each LEA pool end retain
     * wrapping 32-bit arithmetic; JGE interprets each end and limit signed. */
    const uint32_t alignedSize =
        ((uint32_t)size + (R_STATIC_VERTEX_MEMORY_ALIGNMENT - 1U)) &
        ~(uint32_t)(R_STATIC_VERTEX_MEMORY_ALIGNMENT - 1U);

    if (firstSource == R_STATIC_VERTEX_MEMORY_PRIMARY) {
        const uint32_t used =
            (uint32_t)tr.staticVertexMemoryPrimaryUsed;
        const uint32_t end = used + alignedSize;

        if ((int32_t)end <
            (int32_t)(uint32_t)tr.staticVertexMemoryPrimaryLimit) {
            *memory = tr.staticVertexMemoryPrimary.address +
                      used;
            tr.staticVertexMemoryPrimaryUsed = (size_t)end;
            return R_STATIC_VERTEX_MEMORY_PRIMARY;
        }
        firstSource = R_STATIC_VERTEX_MEMORY_SECONDARY;
    }

    if (firstSource == R_STATIC_VERTEX_MEMORY_SECONDARY) {
        const uint32_t used =
            (uint32_t)tr.staticVertexMemorySecondaryUsed;
        const uint32_t end = used + alignedSize;

        if ((int32_t)end <
            (int32_t)(uint32_t)tr.staticVertexMemorySecondaryLimit) {
            *memory = tr.staticVertexMemorySecondary.address +
                      used;
            tr.staticVertexMemorySecondaryUsed = (size_t)end;
            return R_STATIC_VERTEX_MEMORY_SECONDARY;
        }
    }

    *memory = Hunk_AllocAlignInternal((size_t)alignedSize,
                                     R_STATIC_VERTEX_MEMORY_ALIGNMENT);
    return R_STATIC_VERTEX_MEMORY_HUNK;
}

/* Source: CoDUOMP.exe 0x004c8a20..0x004c8a8d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c8a20_004c8a8d.mcode; its body is
 * also inlined into R_OptimizeRigidXSurfaceATI at 0x0049fa41. Name and source-level
 * boundary: same-module Mac R_AllocMemoryATI and its call graph. Unlike the NV
 * allocator, ATI accepts an allocation ending exactly at the pool limit and
 * has no hunk fallback. */
renderer_static_vertex_memory_source_t R_AllocMemoryATI(
    renderer_static_vertex_memory_source_t firstSource, size_t size,
    size_t *offset)
{
    /* ADD/AND at 0x004c8a20..0x004c8a24 and each LEA pool end retain
     * wrapping 32-bit arithmetic; JG interprets each end and limit signed. */
    const uint32_t alignedSize =
        ((uint32_t)size + (R_STATIC_VERTEX_MEMORY_ALIGNMENT - 1U)) &
        ~(uint32_t)(R_STATIC_VERTEX_MEMORY_ALIGNMENT - 1U);

    if (firstSource == R_STATIC_VERTEX_MEMORY_PRIMARY) {
        const uint32_t used =
            (uint32_t)tr.staticVertexMemoryPrimaryUsed;
        const uint32_t end = used + alignedSize;

        if ((int32_t)end <=
            (int32_t)(uint32_t)tr.staticVertexMemoryPrimaryLimit) {
            *offset = (size_t)used;
            tr.staticVertexMemoryPrimaryUsed = (size_t)end;
            return R_STATIC_VERTEX_MEMORY_PRIMARY;
        }
        firstSource = R_STATIC_VERTEX_MEMORY_SECONDARY;
    }

    if (firstSource == R_STATIC_VERTEX_MEMORY_SECONDARY) {
        const uint32_t used =
            (uint32_t)tr.staticVertexMemorySecondaryUsed;
        const uint32_t end = used + alignedSize;

        if ((int32_t)end <=
            (int32_t)(uint32_t)tr.staticVertexMemorySecondaryLimit) {
            *offset = (size_t)used;
            tr.staticVertexMemorySecondaryUsed = (size_t)end;
            return R_STATIC_VERTEX_MEMORY_SECONDARY;
        }
    }

    return R_STATIC_VERTEX_MEMORY_NONE;
}

/* Source: CoDUOMP.exe 0x004c8a90..0x004c8f12.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c8a90_004c8f12.mcode.
 * Name and source-level boundary: same-module Mac symbol R_InitAllocators.
 *
 * The legacy messages intentionally receive each cvar float promoted to
 * double even though their original format strings use "%i". That mismatch
 * is present in the executable and is retained rather than silently repaired. */
void R_InitAllocators(void)
{
    float agpMegabytes;
    float videoMegabytes = 0.0f;
    float backendMegabytes;
    qboolean secondaryWasPreallocated = qfalse;

    tr.defaultStorageMode = R_STATIC_VERTEX_MEMORY_HUNK;
    tr.stageIteratorFunc = RB_StageIteratorGeneric;

    if (r_optimize->integer == 0)
        return;

    tr.staticVertexMemorySecondary.address = NULL;
    tr.staticVertexMemorySecondaryLimit = 0;
    tr.staticVertexMemorySecondaryUsed = 0;
    tr.staticVertexMemoryPrimary.address = NULL;
    tr.staticVertexMemoryPrimaryLimit = 0;
    tr.staticVertexMemoryPrimaryUsed = 0;

    if (r_mem_manual->integer != 0) {
        agpMegabytes = r_mem_agp->value;
        videoMegabytes = r_mem_video->value;
        backendMegabytes = r_mem_backend->value;
    } else {
        backendMegabytes = 1.0f;

        if (glConfig.vertexArrayRangeMode != R_VERTEX_ARRAY_RANGE_NONE) {
            agpMegabytes = 13.0f;
            tr.staticVertexMemorySecondaryLimit =
                R_DEFAULT_NV_AGP_MEMORY_BYTES;
            tr.staticVertexMemorySecondary.address = qglAllocateMemoryNV(
                (int32_t)R_DEFAULT_NV_AGP_MEMORY_BYTES,
                0.0f, 0.0f, 0.5f);

            if (tr.staticVertexMemorySecondary.address != NULL) {
                secondaryWasPreallocated = qtrue;
            } else {
                ri.Printf(
                    R_PRINT_ALL,
                    "^3No AGP memory available for video card to use with "
                    "NV_vertex_array_range.\n"
                    "^3This is usually because the motherboard has no AGP "
                    "drivers installed,\n"
                    "^3forcing the video card to use PCI mode.\n");
                agpMegabytes = 0.0f;
                videoMegabytes = 13.0f;
                tr.staticVertexMemorySecondaryLimit = 0;
            }
        } else {
            agpMegabytes = 1.0f;
            videoMegabytes = 12.0f;
        }
    }

    if (backendMegabytes < 1.0f)
        backendMegabytes = 1.0f;
    else if (backendMegabytes > 32.0f)
        backendMegabytes = 32.0f;

    if (!secondaryWasPreallocated && videoMegabytes > 0.0f) {
        const uint32_t negativeBytes = (uint32_t)(int32_t)(
            videoMegabytes * -1048576.0f);
        const uint32_t alignedBytes =
            (31U - negativeBytes) & ~(uint32_t)31U;

        tr.staticVertexMemoryPrimaryLimit = alignedBytes;

        if (glConfig.vertexArrayRangeMode != R_VERTEX_ARRAY_RANGE_NONE) {
            tr.staticVertexMemoryPrimary.address = qglAllocateMemoryNV(
                (int32_t)alignedBytes, 0.0f, 0.0f, 1.0f);
            if (tr.staticVertexMemoryPrimary.address != NULL) {
                ri.Printf(R_PRINT_ALL,
                          "Allocated %i MB of video memory for vertex data\n",
                          (double)videoMegabytes);
            } else {
                ri.Printf(
                    R_PRINT_WARNING,
                    "Failed to allocate %i MB of video memory for vertex data\n",
                    (double)videoMegabytes);
                tr.staticVertexMemoryPrimaryLimit = 0;
            }
        } else if (glConfig.vertexArrayObjectATIAvailable) {
            tr.staticVertexMemoryPrimary.atiObjectBuffer =
                qglNewObjectBufferATI((int32_t)alignedBytes, NULL,
                                      GL_STATIC_ATI);
            if (tr.staticVertexMemoryPrimary.atiObjectBuffer != 0) {
                ri.Printf(R_PRINT_ALL,
                          "Allocated %i MB of static buffers\n",
                          (double)videoMegabytes);
            } else {
                ri.Printf(R_PRINT_WARNING,
                          "Failed to allocate %i MB of static buffers\n",
                          (double)videoMegabytes);
                tr.staticVertexMemoryPrimaryLimit = 0;
            }
        } else {
            tr.staticVertexMemoryPrimaryLimit = 0;
        }

        tr.staticVertexMemoryPrimaryUsed = 0;
    }

    if (agpMegabytes > 0.0f) {
        const uint32_t negativeBytes = (uint32_t)(int32_t)(
            agpMegabytes * -1048576.0f);
        const uint32_t alignedBytes =
            (31U - negativeBytes) & ~(uint32_t)31U;

        tr.staticVertexMemorySecondaryLimit = alignedBytes;

        if (glConfig.vertexArrayRangeMode != R_VERTEX_ARRAY_RANGE_NONE) {
            if (tr.staticVertexMemorySecondary.address == NULL) {
                tr.staticVertexMemorySecondary.address = qglAllocateMemoryNV(
                    (int32_t)alignedBytes, 0.0f, 0.0f, 0.5f);
            }

            if (tr.staticVertexMemorySecondary.address != NULL) {
                ri.Printf(R_PRINT_ALL,
                          "Allocated %i MB of AGP memory for vertex data\n",
                          (double)agpMegabytes);
            } else {
                ri.Printf(
                    R_PRINT_WARNING,
                    "Failed to allocate %i MB of AGP memory for vertex data\n",
                    (double)agpMegabytes);
                tr.staticVertexMemorySecondaryLimit = 0;
            }
        } else if (glConfig.vertexArrayObjectATIAvailable) {
            tr.staticVertexMemorySecondary.atiObjectBuffer =
                qglNewObjectBufferATI((int32_t)alignedBytes, NULL,
                                      GL_DYNAMIC_ATI);
            if (tr.staticVertexMemorySecondary.atiObjectBuffer != 0) {
                ri.Printf(R_PRINT_ALL,
                          "Allocated %i MB of dynamic buffers\n",
                          (double)agpMegabytes);
            } else {
                ri.Printf(R_PRINT_WARNING,
                          "Failed to allocate %i MB of dynamic buffers\n",
                          (double)agpMegabytes);
                tr.staticVertexMemorySecondaryLimit = 0;
            }
        } else {
            tr.staticVertexMemorySecondaryLimit = 0;
        }

        tr.staticVertexMemorySecondaryUsed = 0;
    }

    if (glConfig.vertexBufferObjectAvailable) {
        backEnd.dynamicBuffer.storage.glBuffer = 0;
        backEnd.dynamicBuffer.currentOffset = 0;
        backEnd.dynamicBuffer.capacity = 0;

        if (r_optimizeBackend->integer == 0)
            return;

        tr.stageIteratorFunc = RB_StageIteratorGenericARB;
        if (tr.vboStreamDraw)
            return;

        const uint32_t negativeBytes = (uint32_t)(int32_t)(
            backendMegabytes * -1048576.0f);
        const uint32_t alignedBytes =
            (31U - negativeBytes) & ~(uint32_t)31U;
        backEnd.dynamicBuffer.capacity = (int32_t)alignedBytes;

        if (backEnd.dynamicBuffer.capacity < 0) {
            backEnd.dynamicBuffer.capacity = 0;
            return;
        }
        if (backEnd.dynamicBuffer.capacity <= 0)
            return;

        backEnd.dynamicBuffer.storage.glBuffer = R_CreateBufferARB(
            GL_ARRAY_BUFFER_ARB, (size_t)backEnd.dynamicBuffer.capacity,
            NULL, GL_DYNAMIC_DRAW_ARB);
        return;
    }

    {
        const uint32_t negativeBytes = (uint32_t)(int32_t)(
            backendMegabytes * -1048576.0f);
        const uint32_t alignedBytes =
            (31U - negativeBytes) & ~(uint32_t)31U;
        backEnd.dynamicBuffer.capacity = (int32_t)alignedBytes;
    }
    backEnd.dynamicBuffer.currentOffset = 0;

    if (backEnd.dynamicBuffer.capacity < 0 ||
        r_optimizeBackend->integer == 0) {
        backEnd.dynamicBuffer.capacity = 0;
        return;
    }
    if (backEnd.dynamicBuffer.capacity <= 0)
        return;

    if (tr.staticVertexMemorySecondaryLimit != 0) {
        if ((size_t)backEnd.dynamicBuffer.capacity >
            tr.staticVertexMemorySecondaryLimit) {
            backEnd.dynamicBuffer.capacity =
                (int32_t)tr.staticVertexMemorySecondaryLimit;
        }

        tr.staticVertexMemorySecondaryUsed =
            (size_t)backEnd.dynamicBuffer.capacity;
        if (glConfig.vertexArrayRangeMode != R_VERTEX_ARRAY_RANGE_NONE) {
            backEnd.dynamicBuffer.storage.address =
                tr.staticVertexMemorySecondary.address;
        } else {
            backEnd.dynamicBuffer.storage.atiObjectBuffer =
                tr.staticVertexMemorySecondary.atiObjectBuffer;
        }

        if (glConfig.vertexArrayRangeMode != R_VERTEX_ARRAY_RANGE_NONE &&
            glConfig.fenceNVAvailable) {
            tr.defaultStorageMode = R_STATIC_VERTEX_MEMORY_SECONDARY;
            tr.stageIteratorFunc = RB_StageIteratorGenericNV;
            return;
        }
    } else if (tr.staticVertexMemoryPrimaryLimit != 0) {
        if ((size_t)backEnd.dynamicBuffer.capacity >
            tr.staticVertexMemoryPrimaryLimit) {
            backEnd.dynamicBuffer.capacity =
                (int32_t)tr.staticVertexMemoryPrimaryLimit;
        }

        tr.staticVertexMemoryPrimaryUsed =
            (size_t)backEnd.dynamicBuffer.capacity;
        if (glConfig.vertexArrayRangeMode != R_VERTEX_ARRAY_RANGE_NONE) {
            backEnd.dynamicBuffer.storage.address =
                tr.staticVertexMemoryPrimary.address;
        } else {
            backEnd.dynamicBuffer.storage.atiObjectBuffer =
                tr.staticVertexMemoryPrimary.atiObjectBuffer;
        }

        if (glConfig.vertexArrayRangeMode != R_VERTEX_ARRAY_RANGE_NONE &&
            glConfig.fenceNVAvailable) {
            tr.defaultStorageMode = R_STATIC_VERTEX_MEMORY_PRIMARY;
            tr.stageIteratorFunc = RB_StageIteratorGenericNV;
            return;
        }
    } else {
        return;
    }

    if (glConfig.vertexArrayObjectATIAvailable)
        tr.stageIteratorFunc = RB_StageIteratorGenericATI;
}

/* Source: CoDUOMP.exe 0x004eb860..0x004eb8a5.
 * Name: exact same-module Mac symbol RB_SelectStorageATI. Static ATI object
 * buffers install their own array objects; only the dynamic tessellation
 * storage needs the client-array pointers restored here. */
void RB_SelectStorageATI(renderer_static_vertex_memory_source_t storageMode)
{
    if (storageMode != R_STATIC_VERTEX_MEMORY_HUNK)
        return;

    qglVertexPointer(tess.vertexComponentCount, GL_FLOAT, 0, tess.xyz);
    qglNormalPointer(GL_FLOAT, (int32_t)sizeof(tess.stageNormals[0]),
                     tess.stageNormals);
    qglTexCoordPointer(2, GL_FLOAT, 0, tess.activeTexCoords[0]);
}

/* Source: CoDUOMP.exe 0x0051d630..0x0051d6ef.
 * Name: exact same-module Mac symbol RB_SelectStorageNV. Windows machine code
 * proves the extension enable/disable direction, the primary/secondary range
 * selection, and the four dynamic-array pointers below. */
void RB_SelectStorageNV(renderer_static_vertex_memory_source_t storageMode)
{
    if (glState.currentStorageMode == R_STATIC_VERTEX_MEMORY_HUNK) {
        qglEnableClientState(GL_VERTEX_ARRAY_RANGE_NV);
    } else if (storageMode == R_STATIC_VERTEX_MEMORY_HUNK) {
        qglDisableClientState(
            glConfig.vertexArrayRangeMode == R_VERTEX_ARRAY_RANGE_NV
                ? GL_VERTEX_ARRAY_RANGE_NV
                : GL_VERTEX_ARRAY_RANGE_WITHOUT_FLUSH_NV);
    }

    if (storageMode == R_STATIC_VERTEX_MEMORY_PRIMARY) {
        qglVertexArrayRangeNV((int32_t)tr.staticVertexMemoryPrimaryLimit,
                              tr.staticVertexMemoryPrimary.address);
    } else if (storageMode == R_STATIC_VERTEX_MEMORY_SECONDARY) {
        qglVertexArrayRangeNV((int32_t)tr.staticVertexMemorySecondaryLimit,
                              tr.staticVertexMemorySecondary.address);
    } else {
        qglVertexPointer(tess.vertexComponentCount, GL_FLOAT, 0, tess.xyz);
        qglNormalPointer(GL_FLOAT, 0, tess.stageNormals);
        qglTexCoordPointer(2, GL_FLOAT, 0, tess.activeTexCoords[0]);
        qglColorPointer(4, GL_UNSIGNED_BYTE, 0, tess.stageVertexColors);
    }
}

/* Source: CoDUOMP.exe 0x0051d5d0..0x0051d62e.
 * Name: exact same-module Mac symbol RB_FinishFenceNV. Each real allocation
 * carries a fence whose name is its monotonically increasing reclaim sequence;
 * wrap-padding records use a negative offset and therefore need no GL wait. */
void RB_FinishFenceNV(void)
{
    const uint32_t sequence = backEnd.dynamicBuffer.reclaimSequence;
    renderer_dynamic_buffer_allocation_t *allocation =
        &backEnd.dynamicBuffer.allocations[
            sequence & (R_DYNAMIC_BUFFER_ALLOCATION_COUNT - 1U)];

    if (allocation->offset >= 0)
        qglFinishFenceNV(sequence);

    backEnd.dynamicBuffer.freeBytes += allocation->size;
    ++backEnd.dynamicBuffer.reclaimSequence;
}

/* Source: CoDUOMP.exe 0x0051d7f0..0x0051d801, also inlined at the end of
 * RB_SingleStageGenericNV (0x0051e167..0x0051e178). No corresponding Mac
 * traceback symbol survived; the role is completely proved by the GL call
 * and the shared allocation-sequence operand. */
void RB_SetFenceNV(void)
{
    qglSetFenceNV(backEnd.dynamicBuffer.allocationSequence,
                  GL_ALL_COMPLETED_NV);
}

/* Source: CoDUOMP.exe 0x0051d6f0..0x0051d7e7.
 * Name: exact same-module Mac symbol RB_GetBuffersNV. Windows LTCG carries
 * size in EAX; the maintained interface uses an ordinary portable argument.
 * The allocator rounds to the original 32-byte boundary and records unused
 * bytes at the end of the circular buffer as a fence-free padding entry. */
uint8_t *RB_GetBuffersNV(int32_t size)
{
    const int32_t alignedSize = (int32_t)(
        ((uint32_t)size + (R_STATIC_VERTEX_MEMORY_ALIGNMENT - 1U)) &
        ~(R_STATIC_VERTEX_MEMORY_ALIGNMENT - 1U));
    uint32_t sequence = ++backEnd.dynamicBuffer.allocationSequence;
    uint32_t allocationIndex =
        sequence & (R_DYNAMIC_BUFFER_ALLOCATION_COUNT - 1U);

    if (sequence == backEnd.dynamicBuffer.reclaimSequence +
                        R_DYNAMIC_BUFFER_ALLOCATION_COUNT) {
        RB_FinishFenceNV();
    }

    const int32_t allocationEnd = (int32_t)(
        (uint32_t)backEnd.dynamicBuffer.currentOffset +
        (uint32_t)alignedSize);
    if (allocationEnd > backEnd.dynamicBuffer.capacity) {
        const int32_t trailingBytes = (int32_t)(
            (uint32_t)backEnd.dynamicBuffer.capacity -
            (uint32_t)backEnd.dynamicBuffer.currentOffset);

        if (trailingBytes != 0) {
            renderer_dynamic_buffer_allocation_t *padding =
                &backEnd.dynamicBuffer.allocations[allocationIndex];
            padding->offset = -1;
            padding->size = trailingBytes;
            backEnd.dynamicBuffer.freeBytes = (int32_t)(
                (uint32_t)backEnd.dynamicBuffer.freeBytes -
                (uint32_t)trailingBytes);

            sequence = ++backEnd.dynamicBuffer.allocationSequence;
            allocationIndex =
                sequence & (R_DYNAMIC_BUFFER_ALLOCATION_COUNT - 1U);
            if (sequence == backEnd.dynamicBuffer.reclaimSequence +
                                R_DYNAMIC_BUFFER_ALLOCATION_COUNT) {
                RB_FinishFenceNV();
            }
        }

        backEnd.dynamicBuffer.currentOffset = 0;
    }

    while (alignedSize > backEnd.dynamicBuffer.freeBytes)
        RB_FinishFenceNV();

    renderer_dynamic_buffer_allocation_t *allocation =
        &backEnd.dynamicBuffer.allocations[allocationIndex];
    allocation->offset = backEnd.dynamicBuffer.currentOffset;
    allocation->size = alignedSize;
    backEnd.dynamicBuffer.freeBytes = (int32_t)(
        (uint32_t)backEnd.dynamicBuffer.freeBytes -
        (uint32_t)alignedSize);
    backEnd.dynamicBuffer.currentOffset = (int32_t)(
        (uint32_t)backEnd.dynamicBuffer.currentOffset +
        (uint32_t)alignedSize);

    return backEnd.dynamicBuffer.storage.address + allocation->offset;
}

/* Source: CoDUOMP.exe 0x004eb8b0..0x004eb8e8. This complete function was
 * absent from Ghidra's function table and was recovered from the executable
 * gap inventory after direct boundary inspection. Name: exact same-module Mac
 * symbol RB_SelectStorage. */
void RB_SelectStorage(renderer_static_vertex_memory_source_t storageMode)
{
    if (storageMode == glState.currentStorageMode) {
        return;
    }

    if (glConfig.vertexArrayRangeMode != R_VERTEX_ARRAY_RANGE_NONE) {
        RB_SelectStorageNV(storageMode);
    } else if (glConfig.vertexArrayObjectATIAvailable != qfalse) {
        RB_SelectStorageATI(storageMode);
    }

    glState.currentStorageMode = storageMode;
}

/* Source: CoDUOMP.exe 0x004c8f20..0x004c9077.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c8f20_004c9077.mcode.
 * Name and source-level boundary: same-module Mac symbol
 * R_ShutdownAllocators. */
void R_ShutdownAllocators(void)
{
    if (glState.currentStorageMode != R_STATIC_VERTEX_MEMORY_HUNK) {
        if (glConfig.vertexArrayRangeMode != R_VERTEX_ARRAY_RANGE_NONE) {
            if (glConfig.vertexArrayRangeMode == R_VERTEX_ARRAY_RANGE_NV)
                qglDisableClientState(GL_VERTEX_ARRAY_RANGE_NV);
            else
                qglDisableClientState(
                    GL_VERTEX_ARRAY_RANGE_WITHOUT_FLUSH_NV);

            qglVertexPointer(tess.vertexComponentCount, GL_FLOAT, 0, tess.xyz);
            qglNormalPointer(GL_FLOAT, 0, tess.stageNormals);
            qglTexCoordPointer(2, GL_FLOAT, 0, tess.activeTexCoords[0]);
            qglColorPointer(4, GL_UNSIGNED_BYTE, 0,
                            tess.stageVertexColors);
        } else if (glConfig.vertexArrayObjectATIAvailable) {
            qglVertexPointer(tess.vertexComponentCount, GL_FLOAT, 0, tess.xyz);
            qglNormalPointer(GL_FLOAT, (int32_t)sizeof(vec3_t),
                             tess.stageNormals);
            qglTexCoordPointer(2, GL_FLOAT, 0, tess.activeTexCoords[0]);
        }

        glState.currentStorageMode = R_STATIC_VERTEX_MEMORY_HUNK;
    }

    if (glConfig.vertexArrayRangeMode != R_VERTEX_ARRAY_RANGE_NONE) {
        if (tr.staticVertexMemorySecondary.address != NULL) {
            qglFreeMemoryNV(tr.staticVertexMemorySecondary.address);
            tr.staticVertexMemorySecondary.address = NULL;
            tr.staticVertexMemorySecondaryLimit = 0;
        }

        if (tr.staticVertexMemoryPrimary.address != NULL) {
            qglFreeMemoryNV(tr.staticVertexMemoryPrimary.address);
            tr.staticVertexMemoryPrimary.address = NULL;
            tr.staticVertexMemoryPrimaryLimit = 0;
        }
        return;
    }

    if (glConfig.vertexArrayObjectATIAvailable) {
        if (tr.staticVertexMemorySecondary.atiObjectBuffer != 0) {
            qglFreeObjectBufferATI(
                tr.staticVertexMemorySecondary.atiObjectBuffer);
            tr.staticVertexMemorySecondary.atiObjectBuffer = 0;
            tr.staticVertexMemorySecondaryLimit = 0;
        }

        if (tr.staticVertexMemoryPrimary.atiObjectBuffer != 0) {
            qglFreeObjectBufferATI(
                tr.staticVertexMemoryPrimary.atiObjectBuffer);
            tr.staticVertexMemoryPrimary.atiObjectBuffer = 0;
            tr.staticVertexMemoryPrimaryLimit = 0;
        }
    }
}

/* Source: CoDUOMP.exe 0x004c9080..0x004c9133, repaired from the executable
 * gap after R_ShutdownAllocators.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c9080_004c9133.mcode.
 * Name: the R_Register command binding for "r_meminfo" and the same-module
 * Mac symbol R_MemInfo_f. */
void R_MemInfo_f(void)
{
    const double bytesToMegabytes =
        (double)0.00000095367431640625f; /* exact original 0x35800000 = 2^-20 */
    double percentUsed = 0.0;

    if (tr.staticVertexMemorySecondaryLimit != 0) {
        percentUsed =
            (double)(int32_t)tr.staticVertexMemorySecondaryUsed * 100.0 /
            (double)(int32_t)tr.staticVertexMemorySecondaryLimit;
    }
    ri.Printf(R_PRINT_ALL, "AGP:   %.2f / %.2f MB (%.1f%%)\n",
              (double)(int32_t)tr.staticVertexMemorySecondaryUsed *
                  bytesToMegabytes,
              (double)(int32_t)tr.staticVertexMemorySecondaryLimit *
                  bytesToMegabytes,
              percentUsed);

    percentUsed = 0.0;
    if (tr.staticVertexMemoryPrimaryLimit != 0) {
        percentUsed =
            (double)(int32_t)tr.staticVertexMemoryPrimaryUsed * 100.0 /
            (double)(int32_t)tr.staticVertexMemoryPrimaryLimit;
    }
    ri.Printf(R_PRINT_ALL, "Video: %.2f / %.2f MB (%.1f%%)\n",
              (double)(int32_t)tr.staticVertexMemoryPrimaryUsed *
                  bytesToMegabytes,
              (double)(int32_t)tr.staticVertexMemoryPrimaryLimit *
                  bytesToMegabytes,
              percentUsed);
}

/* Source: CoDUOMP.exe 0x004c8820..0x004c888c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c8820_004c888c.mcode.
 * Name: same-module Mac symbol R_CreateBufferARB.
 *
 * The original does not call glGenBuffersARB: legacy ARB buffer objects are
 * created by binding a previously unused nonzero name. Choosing a name above
 * both renderer serials prevents a front-end allocation from reusing a name
 * still represented by the render command currently executing. */
uint32_t R_CreateBufferARB(uint32_t target, size_t size, const void *data,
                           uint32_t usage)
{
    uint32_t serial = (uint32_t)tr.dynamicBufferFrameSerial;

    if ((int32_t)serial < tr.dynamicBufferMaxFrameSerial)
        serial = (uint32_t)tr.dynamicBufferMaxFrameSerial;
    /* INC EAX at 0x004c8832 has defined modulo-2^32 behavior. */
    serial += 1u;
    tr.dynamicBufferFrameSerial = (int32_t)serial;

    const uint32_t buffer = serial;
    qglBindBufferARB(target, buffer);
    (void)qglGetError();
    qglBufferDataARB(target, size, data, usage);
    const uint32_t error = qglGetError();
    qglBindBufferARB(target, 0);

    if (error != GL_NO_ERROR) {
        qglDeleteBuffersARB(1, &buffer);
        return 0;
    }

    return buffer;
}

/* Source: CoDUOMP.exe 0x004c8890..0x004c8992.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c8890_004c8992.mcode.
 * Name: same-module Mac symbol R_DeleteBuffersARB.
 *
 * Buffer names form the dense interval [1, max(serials)]. The pointer resets
 * match the exact fixed-function array descriptors installed by the original
 * before it deletes that interval. */
void R_DeleteBuffersARB(void)
{
    if (glConfig.vertexBufferObjectAvailable == qfalse)
        return;

    qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
    qglBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);

    for (int32_t textureUnit = glConfig.maxActiveTextures - 1;
         textureUnit >= 0;
         --textureUnit) {
        GL_SelectTexture(textureUnit);
        qglTexCoordPointer(2, GL_FLOAT, 0, NULL);
    }

    qglNormalPointer(GL_FLOAT, 0, NULL);
    qglColorPointer(4, GL_UNSIGNED_BYTE, 0, NULL);
    qglVertexPointer(3, GL_FLOAT, 0, NULL);

    int32_t lastBuffer = tr.dynamicBufferMaxFrameSerial;
    if (lastBuffer < tr.dynamicBufferFrameSerial)
        lastBuffer = tr.dynamicBufferFrameSerial;

    for (uint32_t buffer = 1;
         (int32_t)buffer <= lastBuffer;
         ++buffer) {
        qglDeleteBuffersARB(1, &buffer);
    }

    tr.dynamicBufferFrameSerial = 0;
    tr.dynamicBufferMaxFrameSerial = 0;
}

/* Source: CoDUOMP.exe 0x004c39e0..0x004c3b0b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c39e0_004c3b0b.mcode.
 * Name and source-level renderer helper calls: exact same-module Mac symbol
 * R_VboRefresh_f and its call graph. The command is registered as
 * "r_vbo_refresh" at 0x004c4dc2..0x004c4dd7 and its usage string proves the
 * seven accepted component names. Ghidra omitted the complete body between
 * two INT3 padding runs; its entry gates, closed control flow, RET, and
 * registration pointer prove the repaired boundary. */
void R_VboRefresh_f(void)
{
    static const char usage[] =
        "usage: r_vbo_refresh [list], where list is one or more of world, "
        "smodels, sverts, sindexes, xmodels, xverts, and xindexes\n";
    const int32_t argumentCount = cmd_argc;
    int32_t argumentIndex;

    if (tr.world == NULL || !glConfig.vertexBufferObjectAvailable)
        return;

    for (argumentIndex = 1; argumentIndex < argumentCount;
         ++argumentIndex) {
        const char *argument =
            argumentIndex < cmd_argc ? cmd_argv[argumentIndex] : "";

        if (coduo_crt_stricmp(argument, "world") == 0) {
            R_RefreshOptimizedWorldSurfaces_ARB();
        } else if (coduo_crt_stricmp(argument, "smodels") == 0) {
            R_RefreshStaticModels_ARB(R_VBO_REFRESH_ALL);
        } else if (coduo_crt_stricmp(argument, "sverts") == 0) {
            R_RefreshStaticModels_ARB(R_VBO_REFRESH_VERTICES);
        } else if (coduo_crt_stricmp(argument, "sindexes") == 0) {
            R_RefreshStaticModels_ARB(R_VBO_REFRESH_INDEXES);
        } else if (coduo_crt_stricmp(argument, "xmodels") == 0) {
            R_RefreshXModels_ARB(R_VBO_REFRESH_ALL);
        } else if (coduo_crt_stricmp(argument, "xverts") == 0) {
            R_RefreshXModels_ARB(R_VBO_REFRESH_VERTICES);
        } else if (coduo_crt_stricmp(argument, "xindexes") == 0) {
            R_RefreshXModels_ARB(R_VBO_REFRESH_INDEXES);
        } else {
            break;
        }
    }

    if (argumentCount == 1 || argumentIndex < argumentCount)
        ri.Printf(R_PRINT_ALL, usage);
}
