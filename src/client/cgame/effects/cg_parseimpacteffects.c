// Source: uo_cgame_mp_x86.dll 0x3001dd30..0x3001df86
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001dd30_3001df86.mcode
//
// CG_ParseImpactEffects — parse one CSV impact-effect definition file.  Each
// non-comment row is:
//
//     effect-type, surface-type, effect-definition
//
// The first column selects one of the caller's effect-type rows, the second is
// resolved by the engine's surface-name service, and the third is copied into the
// corresponding 64-byte effectDef_t.  Empty third columns mark a deliberately
// optional surface definition.
//
// The .mcode name `script_method_scriptbuiltin_istouching` is rejected: it came
// from a size collision and the body contains only CSV parsing, effect/surface
// diagnostics, and effectDef_t table writes.  The same-module PPC name
// CG_ParseImpactEffects is corroborated by the caller CG_RegisterImpactEffects.
//
#include "../client_recovered.h"
#include "../globals.h"

#include <string.h>

enum {
    CG_IMPACT_EFFECT_NAME_CAPACITY = 64,
    CG_SURFACE_TYPE_VEHICLE = 23
};

char *CG_ParseImpactEffects(const char *path, char *text,
                            int32_t effectTypeCount,
                            const char *const *effectTypeNames,
                            effectDef_t *defTables)
{
    for (;;) {
        char *effectTypeToken = Com_ParseExt(&text, qtrue);
        int32_t effectType;
        int32_t surfaceType;
        char *surfaceToken;
        char *definitionToken;
        effectDef_t *definition;

        if (text == NULL) {
            return NULL;
        }

        /* Empty lines and # comment lines are discarded through the physical end
         * of the line, matching 0x3001defb..0x3001df19. */
        if (effectTypeToken[0] == '\0' || effectTypeToken[0] == '#') {
            goto next_line;
        }

        for (effectType = 0; effectType < effectTypeCount; effectType++) {
            if (coduo_crt_stricmp(effectTypeNames[effectType], effectTypeToken) == 0) {
                break;
            }
        }
        if (effectType == effectTypeCount) {
            return (char *)va("unknown effect type '%s' in first column of file '%s'",
                              effectTypeToken, path);
        }

        surfaceToken = Com_ParseExt(&text, qfalse);
        if (surfaceToken[0] == '\0') {
            return (char *)va("missing surface type in second column of file '%s'",
                              path);
        }

        surfaceType = (int32_t)cgame_syscall(CG_SURFACE_TYPE_FROM_NAME,
                                    (intptr_t)surfaceToken);
        if (surfaceType < 0) {
            /* The engine surface lookup excludes the special vehicle slot; the
             * original parser supplies its terminal table index explicitly. */
            if (memcmp(surfaceToken, "vehicle", sizeof("vehicle")) != 0) {
                return (char *)va("unknown surface type '%s' in second column of file '%s'",
                                  surfaceToken, path);
            }
            surfaceType = CG_SURFACE_TYPE_VEHICLE;
        }

        definitionToken = Com_ParseExt(&text, qfalse);
        definition = &defTables[effectType * CG_IMPACT_SURFACE_TYPES + surfaceType];

        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
        if (strlen(definitionToken) >= CG_IMPACT_EFFECT_NAME_CAPACITY) {
            return (char *)va(
                "effect filename '%s' in third column of file '%s' is longer than %i characters",
                definitionToken, path, CG_IMPACT_EFFECT_NAME_CAPACITY - 1);
        }
        strcpy(definition->name, definitionToken);

        if (definitionToken[0] == '\0') {
            definition->name[1] = 1;
        }

next_line:
        if (text == NULL) {
            continue;
        }
        while (*text != '\0' && *text != '\n') {
            text++;
        }
        if (*text == '\n') {
            text++;
            com_parseSession->line = coduo_int32_from_bits(
                (uint32_t)com_parseSession->line + 1u);
        }
    }
}
