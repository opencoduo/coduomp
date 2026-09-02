#include "../module/ui_functions.h"
#include "client/common/client_legacy_crt.h"

enum {
    UI_LANGUAGE_BUFFER_SIZE = 8,
    UI_LANGUAGE_COPY_SIZE = 7
};

// Source: uo_ui_mp_x86.dll 0x4000bef0..0x4000c002
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000bef0_4000c002.mcode
// Exact same-module PPC symbol: UI_VerifyLanguage.
void UI_VerifyLanguage(void)
{
    char uiLanguage[UI_LANGUAGE_BUFFER_SIZE];
    char clientLanguage[UI_LANGUAGE_BUFFER_SIZE];
    int32_t verifiedLanguage;

    /* Each copy is strncpy(dst, src, 7) + dst[7] = 0: 7 characters
     * survive. */
    Q_strncpyz(clientLanguage, UI_Cvar_VariableString("cl_language"), UI_LANGUAGE_COPY_SIZE + 1);

    Q_strncpyz(uiLanguage, UI_Cvar_VariableString("ui_language"), UI_LANGUAGE_COPY_SIZE + 1);

    verifiedLanguage = trap_VerifyLanguageSelection(coduo_crt_atoi(uiLanguage));
    if (verifiedLanguage != coduo_crt_atoi(uiLanguage)) {
        Q_strncpyz(uiLanguage, va("%i", verifiedLanguage), UI_LANGUAGE_COPY_SIZE + 1);
        trap_Cvar_Set("ui_language", uiLanguage);
    }

    /* 0x4000bfba..0x4000bfd3 converts the UI copy first, then the client
     * copy; keep the two import calls sequenced instead of relying on C's
     * unspecified operand evaluation order. */
    int32_t uiLanguageValue = coduo_crt_atoi(uiLanguage);
    int32_t clientLanguageValue = coduo_crt_atoi(clientLanguage);
    trap_Cvar_Set("ui_languagechanged", uiLanguageValue != clientLanguageValue ? "1" : "0");
}
