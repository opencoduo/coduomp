/*
 * Source reconstruction for shared script-object helpers.
 *
 * Binary SHA-256:
 * 4d2391c10e234559ace294fb69d96741cad522653c361f13f626f4dc1891acb7
 */

#include <stdint.h>

#include "recovered_game.h"
#include "scr_vm.h"


/* VERIFIED_DECOMPILER(0x666e7, 766e7_FUN_000766e7.c, VERIFY-SCRIPT-OBJECT-2026-06-17): DATAFLOW_VERIFIED; object id range check and g_entities lookup. */
gentity_t *script_object_to_gentity(uint32_t scriptObject)
{
    gentity_t *result;

    if (scriptObject < MAX_GENTITIES) {
        result = &g_entities[scriptObject];
    } else {
        Scr_ObjectError("not an entity");
        result = 0;
    }

    return result;
}

/* VERIFIED_DECOMPILER(0x66739, 76739_FUN_00076739.c, VERIFY-SCRIPT-OBJECT-2026-06-17): DATAFLOW_VERIFIED; player client check, targetname fallback, and error text fields. */
gentity_t *script_object_to_player(uint32_t scriptObject)
{
    gentity_t *ent = script_object_to_gentity(scriptObject);

    if (ent->client == 0) {
        const char *targetname = "<undefined>";

        if (ent->targetname != 0) {
            targetname = SL_ConvertToString(ent->targetname);
        }

        Scr_Error(va("only valid on players; called on entity %i at %.0f %.0f %.0f classname %s targetname %s\n", (int)scriptObject,
                     (double)ent->currentOrigin[0], (double)ent->currentOrigin[1], (double)ent->currentOrigin[2],
                     SL_ConvertToString(ent->scriptClassname), targetname));
    }

    return ent;
}
