#include <stdlib.h>

#include "../module/ui_functions.h"
#include "client/common/client_legacy_crt.h"

typedef enum {
    UI_STATUS_VALUE_TEXT = 0,
    UI_STATUS_VALUE_BOOLEAN = 1,
    UI_STATUS_VALUE_FRIENDLY_FIRE = 2
} uiStatusValueType_t;

typedef struct {
    const char *key;
    const char *label;
    uiStatusValueType_t valueType;
} uiStatusField_t;

/* Source: uo_ui_mp_x86.dll data 0x40040308..0x40040427.
 * PE_RELOCATION_VALUES_VERIFIED: all 48 key/label pointers target the listed
 * strings in record order; every valueType word matches the PE bytes. */
static const uiStatusField_t ui_statusFields[] = {{"sv_hostname", "@EXE_SV_INFO_SERVERNAME", UI_STATUS_VALUE_TEXT},
                                                  {"address", "@EXE_SV_INFO_ADDRESS", UI_STATUS_VALUE_TEXT},
                                                  {"pswrd", "@EXE_SV_INFO_PASSWORD", UI_STATUS_VALUE_BOOLEAN},
                                                  {"gamename", "@EXE_SV_INFO_GAMENAME", UI_STATUS_VALUE_TEXT},
                                                  {"g_gametype", "@EXE_SV_INFO_GAMETYPE", UI_STATUS_VALUE_TEXT},
                                                  {"sv_pure", "@EXE_SV_INFO_PURE", UI_STATUS_VALUE_BOOLEAN},
                                                  {"mapname", "@EXE_SV_INFO_MAP", UI_STATUS_VALUE_TEXT},
                                                  {"shortversion", "@EXE_SV_INFO_VERSION", UI_STATUS_VALUE_TEXT},
                                                  {"protocol", "@EXE_SV_INFO_PROTOCOL", UI_STATUS_VALUE_TEXT},
                                                  {"sv_maxping", "@EXE_SV_INFO_MAXPING", UI_STATUS_VALUE_TEXT},
                                                  {"sv_minping", "@EXE_SV_INFO_MINPING", UI_STATUS_VALUE_TEXT},
                                                  {"sv_maxrate", "@EXE_SV_INFO_MAXRATE", UI_STATUS_VALUE_TEXT},
                                                  {"sv_floodprotect", "@EXE_SV_INFO_FLOODPROTECT", UI_STATUS_VALUE_BOOLEAN},
                                                  {"sv_allowanonymous", "@EXE_SV_INFO_ALLOWANON", UI_STATUS_VALUE_TEXT},
                                                  {"sv_maxclients", "@EXE_SV_INFO_MAXCLIENTS", UI_STATUS_VALUE_TEXT},
                                                  {"sv_privateclients", "@EXE_SV_INFO_PRIVATECLIENTS", UI_STATUS_VALUE_TEXT},
                                                  {"scr_friendlyFire", "@EXE_SV_INFO_FRIENDLY_FIRE", UI_STATUS_VALUE_FRIENDLY_FIRE},
                                                  {"fs_game", "@EXE_SV_INFO_MOD", UI_STATUS_VALUE_TEXT},
                                                  {"mod", "@MENU_MODS", UI_STATUS_VALUE_BOOLEAN},
                                                  {"scr_killcam", "@EXE_SV_INFO_KILLCAM", UI_STATUS_VALUE_BOOLEAN},
                                                  {"sv_punkbuster", "@PATCH_1_3_PUNKBUSTER", UI_STATUS_VALUE_BOOLEAN},
                                                  {"scr_allow_jeeps", "@GMI_EXE_SV_INFO_ALLOW_JEEPS", UI_STATUS_VALUE_BOOLEAN},
                                                  {"scr_allow_tanks", "@GMI_EXE_SV_INFO_ALLOW_TANKS", UI_STATUS_VALUE_BOOLEAN},
                                                  {"g_timeoutsallowed", "@PATCH_1_5_SV_INFO_TIMEOUTSALLOWED", UI_STATUS_VALUE_TEXT}};

// NOT_FROM_ORIGINAL_SOURCE: expression-level factoring of the switch at
// 0x4000dfcb..0x4000dffd.
static const char *UI_StatusFriendlyFireName(int32_t value)
{
    // NOT_FROM_ORIGINAL_SOURCE: expression-level factoring of the switch at
    // 0x4000dfcb..0x4000dffd.
    switch (value) {
    case 0:
        return "@MENU_SHARED";
    case 1:
        return "@MENU_ON";
    case 2:
        return "@MENU_REFLECT";
    default:
        return "@MENU_OFF";
    }
}

// Source: uo_ui_mp_x86.dll 0x4000ded0..0x4000e041
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000ded0_4000e041.mcode
// Exact same-module PPC symbol: UI_SortServerStatusInfo.
void UI_SortServerStatusInfo(uiServerStatusInfo_t *statusInfo)
{
    enum {
        UI_STATUS_KEY_COMPARE_LIMIT = 99999
    };
    int32_t outputIndex = 0;
    size_t fieldIndex;

    for (fieldIndex = 0; fieldIndex < sizeof(ui_statusFields) / sizeof(ui_statusFields[0]); ++fieldIndex) {
        const uiStatusField_t *field = &ui_statusFields[fieldIndex];
        int32_t lineIndex;

        for (lineIndex = 0; lineIndex < statusInfo->numLines; ++lineIndex) {
            uiServerStatusLine_t *line = &statusInfo->lines[lineIndex];
            uiServerStatusLine_t *output = &statusInfo->lines[outputIndex];
            const char *saved;

            if (line->column[1] == NULL || line->column[1][0] != '\0' || line->column[0] == NULL ||
                Q_stricmpn(field->key, line->column[0], UI_STATUS_KEY_COMPARE_LIMIT) != 0) {
                continue;
            }

            saved = output->column[0];
            output->column[0] = line->column[0];
            line->column[0] = saved;
            saved = output->column[3];
            output->column[3] = line->column[3];
            line->column[3] = saved;

            if (field->label[0] != '\0') {
                output->column[0] = field->label;
            }
            if (field->valueType == UI_STATUS_VALUE_BOOLEAN) {
                output->column[3] = coduo_crt_atoi(output->column[3]) != 0 ? "@EXE_YES" : "@EXE_NO";
            } else if (field->valueType == UI_STATUS_VALUE_FRIENDLY_FIRE) {
                output->column[3] = UI_StatusFriendlyFireName(coduo_crt_atoi(output->column[3]));
            }
            ++outputIndex;
        }
    }
}
