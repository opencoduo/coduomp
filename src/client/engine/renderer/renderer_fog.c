#include "backend.h"

#include "gl_api.h"
#include "gl_state.h"

#include <string.h>

/* Original storage: fog count 0x04884dd4, nine 0x40-byte fog records at
 * 0x04884de0, and the selected fog index at 0x0389feac. RE_SaveFogState's
 * 0x240-byte copy proves that the storage is nine complete fog records; index
 * 8 is interpreted specially by R_SetFog rather than populated normally. */
int32_t rendererFogCount;
renderer_fog_t rendererFogs[R_FOG_SLOT_COUNT];
int32_t rendererCurrentFogIndex;

/* Source: CoDUOMP.exe 0x004e2dd0..0x004e2f4e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e2dd0_004e2f4e.mcode.
 * Name: same-module Mac symbol R_Fog. The Windows LTCG body receives fog in
 * ESI; this maintained signature restores its ordinary source-level argument. */
void R_Fog(renderer_fog_t *fog)
{
    float end;

    if (r_fog->integer == 0 || fog->registered == qfalse) {
        R_FogOff();
        return;
    }

    if (fog->density == 0.0f)
        fog->density = 1.0f;
    if (fog->hint == 0)
        fog->hint = GL_DONT_CARE;
    if (fog->mode == 0)
        fog->mode = GL_LINEAR;

    R_FogOn();

    if (fog->mode != glState.fogMode) {
        qglFogi(GL_FOG_MODE, fog->mode);
        glState.fogMode = fog->mode;
    }

    glState.fogColor[0] = fog->color[0];
    glState.fogColor[1] = fog->color[1];
    glState.fogColor[2] = fog->color[2];
    glState.fogColor[3] = fog->color[3];
    R_SetFogColor();

    if (fog->density != glState.fogDensity) {
        qglFogf(GL_FOG_DENSITY, fog->density);
        glState.fogDensity = fog->density;
    }
    if (fog->hint != glState.fogHint) {
        qglHint(GL_FOG_HINT, fog->hint);
        glState.fogHint = fog->hint;
    }
    if (fog->start != glState.fogStart) {
        qglFogf(GL_FOG_START, fog->start);
        glState.fogStart = fog->start;
    }

    end = fog->end;
    if (r_zfar->value != 0.0f)
        end = r_zfar->value;
    if (end != glState.fogEnd) {
        qglFogf(GL_FOG_END, end);
        glState.fogEnd = end;
    }
}

/* Source: CoDUOMP.exe 0x004e2f50..0x004e2f72.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e2f50_004e2f72.mcode.
 * Name: same-module Mac symbol R_FogOff. */
void R_FogOff(void)
{
    if ((glState.glStateBits & GLS_FOG) == 0)
        return;

    qglDisable(GL_FOG);
    glState.glStateBits &= ~GLS_FOG;
}

/* Source: CoDUOMP.exe 0x004e2f80..0x004e2fce.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e2f80_004e2fce.mcode.
 * Name: same-module Mac symbol R_FogOn. */
void R_FogOn(void)
{
    if (backEnd.projection2D != qfalse ||
        (glState.glStateBits & GLS_FOG) != 0 || r_fog->integer == 0) {
        return;
    }

    if ((backEnd.refdef.rdflags & RDF_SKYBOX_PORTAL) != 0) {
        if (rendererFogs[R_FOG_PORTAL_VIEW].registered == qfalse)
            return;
    } else if (rendererCurrentFogIndex == 0) {
        return;
    }

    qglEnable(GL_FOG);
    glState.glStateBits |= GLS_FOG;
}

/* Source: CoDUOMP.exe 0x004ecb60..0x004ecc05.
 * Name: exact same-module Mac symbol RB_SetIteratorFog.  The function gives
 * the active skybox and portal views their dedicated fog slots, otherwise it
 * applies the current world fog. */
void RB_SetIteratorFog(void)
{
    renderer_fog_t *fog;

    if ((backEnd.refdef.rdflags & RDF_NOWORLDMODEL) != 0) {
        R_FogOff();
        return;
    }

    if ((backEnd.refdef.rdflags & RDF_DRAWING_SKYBOX) != 0) {
        fog = &rendererFogs[R_FOG_SKYBOX_VIEW];
        if (fog->registered != qfalse)
            R_Fog(fog);
        else
            R_FogOff();
        return;
    }

    if (rendererFogCount != 0 &&
        (backEnd.refdef.rdflags & RDF_SKYBOX_PORTAL) != 0) {
        fog = &rendererFogs[R_FOG_PORTAL_VIEW];
        if (fog->registered != qfalse)
            R_Fog(fog);
        else
            R_FogOff();
        return;
    }

    if (rendererCurrentFogIndex > 0)
        R_Fog(&rendererFogs[R_FOG_WORLD_VIEW]);
    else
        R_FogOff();
}

/* Source: CoDUOMP.exe 0x004e2fd0..0x004e304c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e2fd0_004e304c.mcode.
 * Name: same-module Mac symbol R_SetFogColor. */
void R_SetFogColor(void)
{
    vec4_t color;

    if (rendererCurrentFogIndex == 0)
        return;

    if ((glState.glStateBits & GLS_DSTBLEND_BITS) == GLS_DSTBLEND_ONE) {
        color[0] = 0.0f;
        color[1] = 0.0f;
        color[2] = 0.0f;
    } else {
        color[0] = glState.fogColor[0] * tr.identityLight;
        color[1] = glState.fogColor[1] * tr.identityLight;
        color[2] = glState.fogColor[2] * tr.identityLight;
    }
    color[3] = glState.fogColor[3];
    qglFogfv(GL_FOG_COLOR, color);
}

/* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
void R_SetFog(int32_t fogIndex, int32_t fogStart, int32_t fogEnd,
              float red, float green, float blue, float density)
{
    renderer_fog_t *fog;

    if (fogIndex != R_FOG_SWITCH_COMMAND) {
        fog = &rendererFogs[fogIndex];
        if (fogStart == 0 && fogEnd == 0) {
            fog->registered = qfalse;
            return;
        }

        fog->start = (float)fogStart;
        fog->end = (float)fogEnd;
        fog->color[0] = red;
        fog->color[1] = green;
        fog->color[2] = blue;
        fog->color[3] = 1.0f;
        fog->clearScreen = qfalse;
        fog->drawSky = qtrue;

        /* The x87 comparison selects exponential fog for values below 1.0
         * and for unordered values; spelling the inverse preserves NaN. */
        if (!(density >= 1.0f)) {
            fog->mode = GL_EXP;
            fog->density = density;
        } else {
            fog->mode = GL_LINEAR;
            fog->density = 1.0f;
        }

        fog->registered = qtrue;
        fog->hint = GL_DONT_CARE;
        return;
    }

    if (fogStart == R_FOG_RESET_SLOT) {
        if (rendererFogs[R_FOG_WORLD_VIEW].registered != qfalse) {
            rendererFogs[R_FOG_TRANSITION_FROM] =
                rendererFogs[R_FOG_WORLD_VIEW];
        }
        memset(&rendererFogs[R_FOG_RESET_SLOT], 0,
               sizeof(rendererFogs[R_FOG_RESET_SLOT]));
        memset(&rendererFogs[R_FOG_TRANSITION_TO], 0,
               sizeof(rendererFogs[R_FOG_TRANSITION_TO]));
        rendererCurrentFogIndex = 0;
        return;
    }

    fog = &rendererFogs[fogStart];
    if (fog->registered != qtrue)
        return;

    rendererCurrentFogIndex = fogStart;
    if (rendererFogs[R_FOG_WORLD_VIEW].registered != qfalse) {
        rendererFogs[R_FOG_TRANSITION_FROM] =
            rendererFogs[R_FOG_WORLD_VIEW];
    } else {
        rendererFogs[R_FOG_TRANSITION_FROM] = *fog;
    }
    rendererFogs[R_FOG_TRANSITION_TO] = *fog;

    if (fogEnd == 0) {
        rendererFogs[R_FOG_TRANSITION_TO].dirty = qtrue;
        rendererFogs[R_FOG_WORLD_VIEW].dirty = qtrue;
        rendererFogs[R_FOG_TRANSITION_TO].startTime = 0;
        rendererFogs[R_FOG_TRANSITION_TO].finishTime = 0;
    } else {
        rendererFogs[R_FOG_TRANSITION_TO].startTime =
            tr.refdef.time;
        rendererFogs[R_FOG_TRANSITION_TO].finishTime =
            tr.refdef.time + fogEnd;
    }
}

/* Source: CoDUOMP.exe 0x004e31f0..0x004e3239.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e31f0_004e3239.mcode.
 * Name: same-module Mac symbol RE_SaveFogState. */
int32_t RE_SaveFogState(void *buffer, uint32_t bufferSize)
{
    renderer_fog_saved_state_t *state = buffer;

    if (bufferSize < sizeof(*state)) {
        ri.Error(ERR_DROP,
                 "couldn't save fog settings (%i bytes available, %i bytes needed)\n",
                 (int32_t)bufferSize, (int32_t)sizeof(*state));
    }

    memcpy(state->fogs, rendererFogs, sizeof(state->fogs));
    state->currentFogIndex = rendererCurrentFogIndex;
    return (int32_t)sizeof(*state);
}

/* Source: CoDUOMP.exe 0x004e3240..0x004e3281.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e3240_004e3281.mcode.
 * Name: same-module Mac symbol RE_RestoreFogState. */
int32_t RE_RestoreFogState(const void *buffer, uint32_t bufferSize)
{
    const renderer_fog_saved_state_t *state = buffer;

    if (bufferSize < sizeof(*state)) {
        ri.Error(ERR_DROP,
                 "couldn't restore fog settings (savegame is probably corrupt or an old version)\n");
    }

    memcpy(rendererFogs, state->fogs, sizeof(state->fogs));
    rendererCurrentFogIndex = state->currentFogIndex;
    return (int32_t)sizeof(*state);
}

/* Source: CoDUOMP.exe 0x004e4200..0x004e4460.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e4200_004e4460.mcode.
 * Name: same-module Mac symbol R_SetFrameFog. Fog slots 6 and 7 are the
 * transition endpoints and slot 5 is the per-frame result. Mode changes
 * between exponential and linear fog are applied without interpolating their
 * incompatible parameterizations. */
void R_SetFrameFog(void)
{
    renderer_fog_t *current = &rendererFogs[R_FOG_WORLD_VIEW];
    renderer_fog_t *from = &rendererFogs[R_FOG_TRANSITION_FROM];
    renderer_fog_t *to = &rendererFogs[R_FOG_TRANSITION_TO];
    const int32_t currentTime = tr.refdef.time;

    if (r_speeds->integer == 5 && to->registered == qfalse) {
        ri.Printf(R_PRINT_ALL, "no fog - calc zFar: %0.1f\n",
                  tr.viewParms.zFar);
        return;
    }

    if (to->finishTime == 0 || to->finishTime < currentTime) {
        *current = *to;
        current->dirty = qfalse;
    } else if ((from->mode == GL_EXP && to->mode == GL_LINEAR) ||
               (from->mode == GL_LINEAR && to->mode == GL_EXP)) {
        *current = *to;
        to->finishTime = 0;
        current->dirty = qtrue;
    } else {
        int32_t transitionDuration =
            to->finishTime - to->startTime;
        const int32_t transitionElapsed =
            currentTime - to->startTime;
        float transitionFraction;

        if (transitionDuration <= 0)
            transitionDuration = 1;

        transitionFraction =
            (float)transitionElapsed / (float)transitionDuration;
        if (transitionFraction > 1.0f)
            transitionFraction = 1.0f;

        current->mode = to->mode;
        current->registered = qtrue;

        current->start = from->start +
            (to->start - from->start) * transitionFraction;
        current->end = from->end +
            (to->end - from->end) * transitionFraction;
        current->density = from->density +
            (to->density - from->density) * transitionFraction;
        current->color[0] = from->color[0] +
            (to->color[0] - from->color[0]) * transitionFraction;
        current->color[1] = from->color[1] +
            (to->color[1] - from->color[1]) * transitionFraction;
        current->color[2] = from->color[2] +
            (to->color[2] - from->color[2]) * transitionFraction;

        current->clearScreen =
            (to->clearScreen != qfalse || from->clearScreen != qfalse);
        current->dirty = qtrue;
    }

    if (current->mode == GL_LINEAR &&
        current->end < tr.viewParms.zFar) {
        tr.viewParms.zFar = current->end;
    }

    if (r_speeds->integer == 5) {
        const char *format = current->mode == GL_LINEAR
            ? "farclip fog - den: %0.1f  calc zFar: %0.1f  fog zfar: %0.1f\n"
            : "density fog - den: %0.6f  calc zFar: %0.1f  fog zFar: %0.1f\n";

        ri.Printf(R_PRINT_ALL, format, current->density,
                  tr.viewParms.zFar, current->end);
    }
}

/* Source: CoDUOMP.exe 0x004e4460..0x004e44d1.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e4460_004e44d1.mcode.
 * Name: same-module Mac symbol SetFarClip. The no-world-model view uses 2048;
 * an explicit nonzero r_zfar wins otherwise, and the normal scene starts from
 * 524288 before the current linear fog may pull the clip plane inward. */
void SetFarClip(void)
{
    if ((tr.refdef.rdflags & RDF_NOWORLDMODEL) != 0) {
        tr.viewParms.zFar = 2048.0f;
        return;
    }

    if (r_zfar->value != 0.0f) {
        tr.viewParms.zFar = r_zfar->value;
        R_SetFrameFog();
        if (r_speeds->integer == 5) {
            ri.Printf(R_PRINT_ALL, "farclip at: %f\n",
                      tr.viewParms.zFar);
        }
        return;
    }

    tr.viewParms.zFar = 524288.0f;
    R_SetFrameFog();
}
