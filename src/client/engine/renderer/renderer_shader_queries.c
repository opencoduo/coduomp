#include "backend.h"
#include "gl_api.h"
#include "renderer_api.h"

/* Source: CoDUOMP.exe 0x00503d50..0x00503d8b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00503d50_00503d8c.mcode.
 * Name and shader-handle argument: exact same-module Mac symbol
 * RE_GetShaderName. Invalid and default-shader handles deliberately expose
 * the empty string rather than the renderer's internal default-shader name. */
const char *RE_GetShaderName(int32_t shaderHandle)
{
    if (shaderHandle < 0 || shaderHandle >= tr.numShaders) {
        ri.Printf(R_PRINT_WARNING, "R_GetShaderByHandle: out of range hShader '%d'\n", shaderHandle);
        return "";
    }

    shader_t *const shader = tr.shaders[shaderHandle];
    return shader == tr.defaultShader ? "" : shader->name;
}

/* Source: CoDUOMP.exe 0x00503d90..0x00503dba.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00503d90_00503dbb.mcode.
 * Name and shader-handle argument: exact same-module Mac symbol
 * R_GetShaderByHandle. */
shader_t *R_GetShaderByHandle(int32_t shaderHandle)
{
    if (shaderHandle < 0 || shaderHandle >= tr.numShaders) {
        ri.Printf(R_PRINT_WARNING, "R_GetShaderByHandle: out of range hShader '%d'\n", shaderHandle);
        return tr.defaultShader;
    }

    return tr.shaders[shaderHandle];
}

/* Source: CoDUOMP.exe 0x00503dc0..0x00503f20.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00503dc0_00503f21.mcode.
 * Name and no-argument signature: exact same-module Mac symbol
 * R_ShaderList_f. A second command argument selects the sorted shader table;
 * the ordinary command order is registration order. */
void R_ShaderList_f(void)
{
    int32_t listedShaderCount = 0;

    ri.Printf(R_PRINT_ALL, "-----------------------\n");

    for (int32_t shaderIndex = 0; shaderIndex < tr.numShaders; ++shaderIndex) {
        shader_t *const shader = ri.Cmd_Argc() > 1 ? tr.sortedShaders[shaderIndex] : tr.shaders[shaderIndex];

        ri.Printf(R_PRINT_ALL, "%i ", shader->numUnfoggedPasses);
        ri.Printf(R_PRINT_ALL, shader->lightmapIndex >= 0 ? "L " : "  ");

        if (shader->stages[0] == NULL) {
            ri.Printf(R_PRINT_ALL, "      ");
        } else {
            switch (shader->stages[0]->bundle[1].textureEnvMode) {
            case GL_ADD:
                ri.Printf(R_PRINT_ALL, "MT(a) ");
                break;
            case GL_MODULATE:
                ri.Printf(R_PRINT_ALL, "MT(m) ");
                break;
            case GL_DECAL:
                ri.Printf(R_PRINT_ALL, "MT(d) ");
                break;
            default:
                ri.Printf(R_PRINT_ALL, "      ");
                break;
            }
        }

        ri.Printf(R_PRINT_ALL, (shader->flags & SHADER_FLAG_EXPLICITLY_DEFINED) != 0 ? "E " : "  ");

        if (shader->optimalStageIteratorFunc == tr.stageIteratorFunc) {
            ri.Printf(R_PRINT_ALL, "gen ");
        } else if (shader->optimalStageIteratorFunc == RB_StageIteratorSky) {
            ri.Printf(R_PRINT_ALL, "sky ");
        } else {
            ri.Printf(R_PRINT_ALL, "    ");
        }

        ri.Printf(R_PRINT_ALL, (shader->flags & SHADER_FLAG_DEFAULTED) != 0 ? ": %s (DEFAULTED)\n" : ": %s\n", shader->name);
        ++listedShaderCount;
    }

    ri.Printf(R_PRINT_ALL, "%i total shaders\n", listedShaderCount);
    ri.Printf(R_PRINT_ALL, "------------------\n");
}
