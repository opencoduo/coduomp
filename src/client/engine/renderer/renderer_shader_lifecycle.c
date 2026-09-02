#include "backend.h"

#include "gl_api.h"
#include "gl_state.h"

#include <string.h>

uint32_t rendererCurrentFragmentShader;
renderer_vertex_program_t *rendererCurrentVertexProgram;
renderer_vertex_program_t rendererVertexPrograms[R_MAX_VERTEX_PROGRAMS];
int32_t rendererVertexProgramCount;

/* Source: CoDUOMP.exe 0x005171f0..0x0051733f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005171f0_00517340.mcode.
 * Name and ordinary one-argument signature: exact same-module Mac symbol
 * R_LoadVertexProgram. Windows instructions prove the 128-entry cap, vp10
 * pathname, 68-byte registry stride, sequential ARB program names, upload and
 * error-query order, failed-program deletion, file-buffer ownership, and the
 * count increment only after a successful load. */
renderer_vertex_program_t *R_LoadVertexProgram(const char *name)
{
    const char *path;
    void *fileBuffer;
    int32_t fileSize;
    renderer_vertex_program_t *program;

    if (rendererVertexProgramCount == R_MAX_VERTEX_PROGRAMS) {
        ri.Printf(R_PRINT_WARNING, "WARNING: tried to load more than %i unique vertex programs in a single map\n", R_MAX_VERTEX_PROGRAMS);
        return NULL;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if (strlen(name) >= R_VERTEX_PROGRAM_NAME_SIZE) {
        ri.Printf(R_PRINT_WARNING, "WARNING: vertex program name '%s' is too long\n", name);
        return NULL;
    }

    path = va("scripts/%s.vp10", name);
    fileSize = ri.FS_ReadFile(path, &fileBuffer);
    if (fileSize < 0) {
        ri.Printf(R_PRINT_WARNING, "WARNING: couldn't open vertex program '%s'\n", path);
        return NULL;
    }

    program = &rendererVertexPrograms[rendererVertexProgramCount];
    program->glProgramName = (uint32_t)rendererVertexProgramCount + 1u;
    strcpy(program->name, name);

    if (glConfig.vertexProgramAvailable != qfalse) {
        int32_t errorPosition;

        qglBindProgramARB(GL_VERTEX_PROGRAM_ARB, program->glProgramName);
        qglProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, fileSize, fileBuffer);
        qglBindProgramARB(GL_VERTEX_PROGRAM_ARB, 0);
        qglGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &errorPosition);
        if (errorPosition >= 0) {
            /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
            ri.Printf(R_PRINT_WARNING, "WARNING: shader '%s': error in vertex program '%s' at char %i: %s\n", rendererParsedShader.name,
                      path, errorPosition, qglGetString(GL_PROGRAM_ERROR_STRING_ARB));
            qglDeleteProgramsARB(1, &program->glProgramName);
            ri.FS_FreeFile(fileBuffer);
            return NULL;
        }
    }

    ri.FS_FreeFile(fileBuffer);
    ++rendererVertexProgramCount;
    return program;
}

/* Source: CoDUOMP.exe 0x00517340..0x00517386.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00517340_00517387.mcode.
 * Name and signature: exact same-module Mac symbol R_FindVertexProgram.
 * Windows proves the case-insensitive linear lookup and load-on-miss path. */
renderer_vertex_program_t *R_FindVertexProgram(const char *name)
{
    int32_t programIndex;

    for (programIndex = 0; programIndex < rendererVertexProgramCount; ++programIndex) {
        if (Q_stricmp(name, rendererVertexPrograms[programIndex].name) == 0)
            return &rendererVertexPrograms[programIndex];
    }

    return R_LoadVertexProgram(name);
}

/* Source: CoDUOMP.exe 0x005173a0..0x00517400.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005173a0_00517401.mcode.
 * Name and source behavior: exact same-module Mac symbol
 * R_DeleteVertexPrograms. Windows proves the capability gate, program-zero
 * binding, current-program cache reset, one-at-a-time deletion, and final
 * registry-count reset. */
void R_DeleteVertexPrograms(void)
{
    int32_t programIndex;

    if (glConfig.vertexProgramAvailable != qfalse) {
        qglBindProgramARB(GL_VERTEX_PROGRAM_ARB, 0);
        rendererCurrentVertexProgram = NULL;

        for (programIndex = 0; programIndex < rendererVertexProgramCount; ++programIndex) {
            qglDeleteProgramsARB(1, &rendererVertexPrograms[programIndex].glProgramName);
        }
    }

    rendererVertexProgramCount = 0;
}

/* Source: CoDUOMP.exe 0x00517390..0x00517394, recovered from an exporter
 * gap. Name and tail-call behavior: exact same-module Mac symbol
 * R_InitVertexPrograms. Programs are rebuilt lazily after the reset. */
void R_InitVertexPrograms(void)
{
    R_DeleteVertexPrograms();
}

/* Source: CoDUOMP.exe 0x004eae50..0x004eae65.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004eae50_004eae66.mcode.
 * Name: exact same-module Mac symbol GL_BindFragmentShaderATI. Repeated
 * Windows inline instances additionally prove the cached-binding comparison,
 * GL call, and cache update; R_DeleteFragmentShaders contains the shader-zero
 * instance at 0x004c5089..0x004c50a3. */
void GL_BindFragmentShaderATI(uint32_t shader)
{
    if (shader == rendererCurrentFragmentShader)
        return;

    qglBindFragmentShaderATI(shader);
    rendererCurrentFragmentShader = shader;
}

/* Source: CoDUOMP.exe 0x004c5080..0x004c50d9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c5080_004c50da.mcode.
 * Name and the inlined binding helper are corroborated by the same-module Mac
 * symbols. The Windows loader at 0x004f635d proves the glConfig capability
 * gate and the GL entry-point identities. */
void R_DeleteFragmentShaders(void)
{
    if (glConfig.fragmentShaderATIAvailable == qfalse)
        return;

    GL_BindFragmentShaderATI(0);

    for (int32_t shader = 1; shader <= tr.fragmentShaderCount; ++shader) {
        qglDeleteFragmentShaderATI((uint32_t)shader);
    }

    tr.fragmentShaderCount = 0;
    rendererCurrentFragmentShader = 0;
}
