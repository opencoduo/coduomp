// Source: uo_cgame_mp_x86.dll 0x30041e20..0x3004210b
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30041e20_3004210b.mcode
//
// CG_DrawSkyBoxPortal — temporarily replace the current refdef with the skybox
// portal origin/fog state from config string 10, submit that scene, then restore the
// caller's complete 80-byte refdef prefix.
//
// The .mcode name `script_method_player_cloneplayer` is rejected: the body parses
// the skybox config string, updates refdef FOV/origin/flags, configures fog, and
// calls trap_R_RenderScene.  The diagnostic strings identify the exact same-module
// PPC symbol CG_DrawSkyBoxPortal.

#include "../client_recovered.h"
#include "compat/coduo_native_x87.h"
#include "../globals.h"

#include <math.h>

enum {
    CS_SKYBOX_PORTAL = 10,
    CG_SKYBOX_FOG_MODE = 2
};

void CG_DrawSkyBoxPortal(void)
{
    const char *config = CG_ConfigString(CS_SKYBOX_PORTAL);
    char *parse;
    refdef_t saved;
    char *token;
    int32_t fogState;

    if (config[0] == '\0') {
        return;
    }

    saved = cg_refdef;

    if (cg_skybox_vmCvar.integer) {
        parse = (char *)config;

        token = Com_ParseOnLine(&parse);
        if (token == NULL || token[0] == '\0') {
            Com_ErrorMessage("CG_DrawSkyBoxPortal: error parsing skybox configstring\n");
        }
        cg_refdef.vieworg[0] = (float)atof(token);

        token = Com_ParseOnLine(&parse);
        if (token == NULL || token[0] == '\0') {
            Com_ErrorMessage("CG_DrawSkyBoxPortal: error parsing skybox configstring\n");
        }
        cg_refdef.vieworg[1] = (float)atof(token);

        token = Com_ParseOnLine(&parse);
        if (token == NULL || token[0] == '\0') {
            Com_ErrorMessage("CG_DrawSkyBoxPortal: error parsing skybox configstring\n");
        }
        cg_refdef.vieworg[2] = (float)atof(token);

        /* The fourth required integer is parsed for format compatibility but is not
         * consumed afterward by this build (CALL Q_atoi, result overwritten). */
        token = Com_ParseOnLine(&parse);
        if (token == NULL || token[0] == '\0') {
            Com_ErrorMessage("CG_DrawSkyBoxPortal: error parsing skybox configstring\n");
        }
        (void)coduo_crt_atoi(token);

        token = Com_ParseOnLine(&parse);
        if (token == NULL || token[0] == '\0') {
            Com_ErrorMessage(
                "CG_DrawSkyBoxPortal: error parsing skybox configstring.  No fog state\n");
        }
        fogState = coduo_crt_atoi(token);

        if (fogState != 0) {
            float fogParam[3];
            int32_t fogInt0;
            int32_t fogInt1;

            token = Com_ParseOnLine(&parse);
            if (token == NULL || token[0] == '\0') {
                Com_ErrorMessage(
                    "CG_DrawSkyBoxPortal: error parsing skybox configstring.  No fog[0]\n");
            }
            fogParam[0] = (float)atof(token);

            token = Com_ParseOnLine(&parse);
            if (token == NULL || token[0] == '\0') {
                Com_ErrorMessage(
                    "CG_DrawSkyBoxPortal: error parsing skybox configstring.  No fog[1]\n");
            }
            fogParam[1] = (float)atof(token);

            token = Com_ParseOnLine(&parse);
            if (token == NULL || token[0] == '\0') {
                Com_ErrorMessage(
                    "CG_DrawSkyBoxPortal: error parsing skybox configstring.  No fog[2]\n");
            }
            fogParam[2] = (float)atof(token);

            token = Com_ParseOnLine(&parse);
            fogInt0 = (token != NULL && token[0] != '\0') ? coduo_crt_atoi(token) : 0;
            token = Com_ParseOnLine(&parse);
            fogInt1 = (token != NULL && token[0] != '\0') ? coduo_crt_atoi(token) : 0;

            trap_R_SetFog(CG_SKYBOX_FOG_MODE, fogInt0, fogInt1,
                      CG_FloatBits(fogParam[0]), CG_FloatBits(fogParam[1]),
                      CG_FloatBits(fogParam[2]), CG_FloatBits(1.1f));
            cg_skyboxFogConfigured = qtrue;
        } else if (!cg_skyboxFogConfigured) {
            trap_R_SetFog(CG_SKYBOX_FOG_MODE, 0, 0, 0, 0, 0, 0);
            cg_skyboxFogConfigured = qtrue;
        }

        cg_refdef.fov_x = CG_CalcFov();
        long double tangent = coduo_x87_tanl(
            (long double)cg_refdef.fov_x *
            (long double)DEG_TO_HALF_RAD);
        cg_refdef.fov_y = (float)(coduo_x87_atan2l(
            (long double)coduo_int32_from_bits((uint32_t)cg_refdef.height),
            (long double)coduo_int32_from_bits((uint32_t)cg_refdef.width) /
                tangent) *
            (long double)HALF_RAD_TO_DEG);
        cg_refdef.rdflags |= RDF_SKYBOX_PORTAL | RDF_SKYBOX_PORTAL_ACTIVE;
    } else {
        cg_refdef.rdflags &= ~(uint32_t)RDF_SKYBOX_PORTAL_ACTIVE;
        cg_refdef.rdflags |= RDF_SKYBOX_PORTAL;
    }
    cg_refdef.time = coduo_int32_from_bits(cg_time);

    cgame_syscall(CG_R_RENDER_SCENE, (intptr_t)&cg_refdef);

    cg_refdef = saved;
}
