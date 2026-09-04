// Source: uo_cgame_mp_x86.dll 0x300384c0..0x3003878b
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300384c0_3003878b.mcode

#include "../client_recovered.h"
#include "../globals.h"

/*
 * CG_ParseFog — parse CS_FOGVARS as six floats followed by one integer and
 * forward the result to the renderer fog trap. The name is present in the
 * same-module PPC bank and the config-string/trap behavior matches it.
 */
void CG_ParseFog(void)
{
    char *cursor = &cg_gameState.stringData[
        cg_gameState.stringOffsets[CS_FOGVARS]];
    float values[6];
    char *token;

    values[0] = (float)atof(Com_Parse(&cursor));

    token = Com_Parse(&cursor);
    if (token == NULL || token[0] == '\0') {
        /* Stock pushes FOUR trailing zeros here (0x3003873d..0x30038770,
         * add $0x20 cleanup = id + 7 args), same shape as the full-parse
         * call below; the engine dispatcher always reads all seven. */
        cgame_syscall(CG_R_SET_FOG, 8, 3,
                      coduo_fp_to_i32_extended((long double)values[0]), 0, 0, 0,
                      0);
        return;
    }

    values[1] = (float)atof(token);
    values[2] = (float)atof(Com_Parse(&cursor));
    values[3] = (float)atof(Com_Parse(&cursor));
    values[4] = (float)atof(Com_Parse(&cursor));
    values[5] = (float)atof(Com_Parse(&cursor));

    token = Com_Parse(&cursor);
    int32_t fogType = coduo_crt_atoi(token);

    trap_R_SetFog(4, coduo_fp_to_i32_extended((long double)values[0]),
              coduo_fp_to_i32_extended((long double)values[1]),
              CG_FloatBits(values[3]), CG_FloatBits(values[4]),
              CG_FloatBits(values[5]), CG_FloatBits(values[2]));
    cgame_syscall(CG_R_SET_FOG, 8, 4, fogType, 0, 0, 0, 0);
}
