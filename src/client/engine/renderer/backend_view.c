#include "backend.h"

#include "gl_api.h"
#include "gl_state.h"

/* Source: CoDUOMP.exe 0x004be580..0x004be5c3.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004be580_004be5c4.mcode.
 * Name: same-module Mac symbol RB_Hyperspace. */
void RB_Hyperspace(void)
{
    /* 0x3b808081, the original binary's single-precision 1/255. */
    const float byteToUnit = 0.0039215688593685627f;
    const float color = (float)(backEnd.refdef.time & 255) * byteToUnit;

    qglClearColor(color, color, color, 1.0f);
    qglClear(GL_COLOR_BUFFER_BIT);
    backEnd.isHyperspace = qtrue;
}

/* Source: CoDUOMP.exe 0x004be5d0..0x004be632.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004be5d0_004be633.mcode.
 * Name: same-module Mac symbol SetViewportAndScissor. */
void SetViewportAndScissor(void)
{
    qglMatrixMode(GL_PROJECTION);
    qglLoadMatrixf(backEnd.viewParms.projectionMatrix);
    qglMatrixMode(GL_MODELVIEW);
    qglViewport(backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
                backEnd.viewParms.viewportWidth,
                backEnd.viewParms.viewportHeight);
    qglScissor(backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
               backEnd.viewParms.viewportWidth,
               backEnd.viewParms.viewportHeight);
}

/* Source: CoDUOMP.exe 0x004be640..0x004be8ea.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004be640_004be8eb.mcode.
 * Name: same-module Mac symbol RB_BeginDrawingView. The cvar names are proved
 * by their registration strings; the four refdef flag names are provisional
 * role names for their exact Windows bit tests. */
void RB_BeginDrawingView(void)
{
    renderer_fog_t *portalFog = &rendererFogs[R_FOG_PORTAL_VIEW];
    renderer_fog_t *worldFog = &rendererFogs[R_FOG_WORLD_VIEW];
    const uint32_t refdefFlags = (uint32_t)backEnd.refdef.rdflags;
    uint32_t clearBits = 0;

    if (r_finish->integer == 1 && !glState.finishCalled) {
        qglFinish();
        glState.finishCalled = qtrue;
    }
    if (r_finish->integer == 0)
        glState.finishCalled = qtrue;

    backEnd.projection2D = qfalse;
    SetViewportAndScissor();
    GL_State(GLS_DEPTHMASK_TRUE);
    glState.currentLightingEntity = NULL;

    /* 0x004be6c0 CMP rendererFogCount,0; JE set-stencil; else TEST cl,0x8 (portal);
     * so the stencil clear runs when r_measureOverdraw != 0 AND
     * (rendererFogCount == 0 OR RDF_SKYBOX_PORTAL). A prior pass had
     * (rendererFogCount != 0 && portal). */
    if (cg_shadows->integer == 2 ||
        (r_measureOverdraw->integer != 0 &&
         (rendererFogCount == 0 ||
          (refdefFlags & RDF_SKYBOX_PORTAL) != 0))) {
        clearBits = GL_STENCIL_BUFFER_BIT;
    }

    if (r_uifullscreen->integer != 0) {
        clearBits = GL_DEPTH_BUFFER_BIT;
    } else {
        clearBits |= GL_DEPTH_BUFFER_BIT;

        /* 0x004be6ae loads rendererFogCount (0x04884dd4), not the
         * independently derived portal-active marker at 0x04884da8. */
        if (rendererFogCount != 0) {
            if ((refdefFlags & RDF_SKYBOX_PORTAL) != 0) {
                if (r_fastsky->integer == 0 &&
                    (refdefFlags & RDF_NOWORLDMODEL) == 0) {
                    if (portalFog->registered && portalFog->clearScreen) {
                        qglClearColor(portalFog->color[0],
                                      portalFog->color[1],
                                      portalFog->color[2],
                                      portalFog->color[3]);
                        clearBits |= GL_COLOR_BUFFER_BIT;
                    }
                } else {
                    clearBits |= GL_COLOR_BUFFER_BIT;
                    if (portalFog->registered) {
                        qglClearColor(portalFog->color[0],
                                      portalFog->color[1],
                                      portalFog->color[2],
                                      portalFog->color[3]);
                    } else if (rendererCurrentFogIndex > 0 &&
                               worldFog->registered) {
                        qglClearColor(worldFog->color[0], worldFog->color[1],
                                      worldFog->color[2], worldFog->color[3]);
                    } else {
                        const float clearColor = tr.identityLight * 0.5f;
                        qglClearColor(clearColor, clearColor, clearColor,
                                      1.0f);
                    }
                }
            } else if (rendererCurrentFogIndex > 0 &&
                       worldFog->registered) {
                if ((refdefFlags & RDF_DRAW_SKYBOX) != 0) {
                    /* 0x004be7ef CMP worldFog->mode,GL_LINEAR(0x2601); the COLOR-bit OR
                     * at 0x4be806 runs on the fall-through, i.e. mode == GL_LINEAR. A
                     * prior pass used != GL_LINEAR. */
                    if (worldFog->mode == GL_LINEAR)
                        clearBits |= GL_COLOR_BUFFER_BIT;
                } else if (cg_skybox->integer == 0) {
                    clearBits |= GL_COLOR_BUFFER_BIT;
                }

                if ((clearBits & GL_COLOR_BUFFER_BIT) != 0) {
                    qglClearColor(worldFog->color[0], worldFog->color[1],
                                  worldFog->color[2], worldFog->color[3]);
                }
            }
        } else if ((refdefFlags & RDF_NOWORLDMODEL) != 0) {
            clearBits &= ~GL_COLOR_BUFFER_BIT;
        } else if (r_fastsky->integer != 0) {
            clearBits |= GL_COLOR_BUFFER_BIT;
            if (worldFog->registered) {
                qglClearColor(worldFog->color[0], worldFog->color[1],
                              worldFog->color[2], worldFog->color[3]);
            } else {
                const float clearColor = tr.identityLight * 0.5f;
                qglClearColor(clearColor, clearColor, clearColor, 1.0f);
            }
        } else if (worldFog->registered && worldFog->clearScreen) {
            qglClearColor(worldFog->color[0], worldFog->color[1],
                          worldFog->color[2], worldFog->color[3]);
            clearBits |= GL_COLOR_BUFFER_BIT;
        }
    }

    if (clearBits != 0)
        qglClear(clearBits);

    if ((refdefFlags & RDF_HYPERSPACE) != 0) {
        RB_Hyperspace();
        return;
    }

    backEnd.isHyperspace = qfalse;
    backEnd.skyRenderedThisView = qfalse;
}
