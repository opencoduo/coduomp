#include "gl_state.h"

#include "backend.h"
#include "gl_api.h"

/* Source: CoDUOMP.exe 0x004be1c0..0x004be497.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004be1c0_004be498.mcode.
 * Name: same-module Mac symbol GL_ClientState. */
void GL_ClientState(uint32_t stateBits)
{
    uint32_t diff = stateBits ^ glState.clientStateBits;
    int32_t textureUnit;

    if (diff == 0)
        return;

    for (textureUnit = 0; textureUnit < R_MAX_TEXTURE_UNITS; ++textureUnit) {
        const uint32_t textureCoordinateBit = 1u << textureUnit;

        if ((diff & textureCoordinateBit) == 0)
            continue;

        if (glState.currentClientTmu != textureUnit) {
            qglClientActiveTextureARB(GL_TEXTURE0_ARB + (uint32_t)textureUnit);
        }

        if ((stateBits & textureCoordinateBit) != 0)
            qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
        else
            qglDisableClientState(GL_TEXTURE_COORD_ARRAY);

        if (glState.currentClientTmu != textureUnit) {
            qglClientActiveTextureARB(GL_TEXTURE0_ARB + (uint32_t)glState.currentClientTmu);
        }
    }

    if ((diff & GLS_CLIENT_COLOR_ARRAY) != 0) {
        if ((stateBits & GLS_CLIENT_COLOR_ARRAY) != 0)
            qglEnableClientState(GL_COLOR_ARRAY);
        else
            qglDisableClientState(GL_COLOR_ARRAY);
    }

    if ((diff & GLS_CLIENT_NORMAL_ARRAY) != 0) {
        if ((stateBits & GLS_CLIENT_NORMAL_ARRAY) != 0)
            qglEnableClientState(GL_NORMAL_ARRAY);
        else
            qglDisableClientState(GL_NORMAL_ARRAY);
    }

    if ((diff & GLS_CLIENT_VERTEX_ARRAY) != 0) {
        if ((stateBits & GLS_CLIENT_VERTEX_ARRAY) != 0)
            qglEnableClientState(GL_VERTEX_ARRAY);
        else
            qglDisableClientState(GL_VERTEX_ARRAY);
    }

    glState.clientStateBits = stateBits;
}

/* Source: CoDUOMP.exe 0x004be4a0..0x004be4d1.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004be4a0_004be4d2.mcode.
 * Name: same-module Mac symbol GL_DrawElements. The Windows optimizer carries
 * count in EAX; the maintained source uses a normal portable signature. */
void GL_DrawElements(uint32_t mode, int32_t count, uint32_t type, const void *indices)
{
    /* The original ADD/INC counters wrap in 32 bits. */
    backEnd.pc.drawnIndexCount = (int32_t)((uint32_t)backEnd.pc.drawnIndexCount + (uint32_t)count);
    backEnd.pc.drawCallCount = (int32_t)((uint32_t)backEnd.pc.drawCallCount + 1u);
    qglDrawElements(mode, count, type, indices);
}

/* Source: CoDUOMP.exe 0x004be4e0..0x004be545.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004be4e0_004be546.mcode.
 * Name: same-module Mac symbol GL_DrawRangeElements. */
void GL_DrawRangeElements(uint32_t mode, uint32_t start, uint32_t end, int32_t count, uint32_t type, const void *indices)
{
    /* The original ADD/INC counters wrap in 32 bits. */
    backEnd.pc.drawnIndexCount = (int32_t)((uint32_t)backEnd.pc.drawnIndexCount + (uint32_t)count);
    backEnd.pc.drawCallCount = (int32_t)((uint32_t)backEnd.pc.drawCallCount + 1u);

    if (qglDrawRangeElementsEXT != NULL && end != 0) {
        /* Callers supply an exclusive vertex bound; OpenGL defines `end` as
         * inclusive. Convert the bound at the API boundary. */
        qglDrawRangeElementsEXT(mode, start, end - 1u, count, type, indices);
    } else {
        qglDrawElements(mode, count, type, indices);
    }
}

/* Source: CoDUOMP.exe 0x004be550..0x004be577.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004be550_004be578.mcode.
 * Name: same-module Mac symbol GL_DrawElementArrayATI. */
void GL_DrawElementArrayATI(uint32_t mode, int32_t count)
{
    /* The original ADD/INC counters wrap in 32 bits. */
    backEnd.pc.drawnIndexCount = (int32_t)((uint32_t)backEnd.pc.drawnIndexCount + (uint32_t)count);
    backEnd.pc.drawCallCount = (int32_t)((uint32_t)backEnd.pc.drawCallCount + 1u);
    qglDrawElementArrayATI(mode, count);
}
