#include "../module/ui_functions.h"
#include "../module/ui_globals.h"

#include <stddef.h>
#include <string.h>

static const char UI_UNLOCALIZED_PREFIX[] = "^1UNLOCALIZED(^7";
static const char UI_UNLOCALIZED_SUFFIX[] = "^1)^7";

enum {
    UI_TRANSLATION_BUFFER_SIZE =
        MAX_STRING_CHARS + sizeof(UI_UNLOCALIZED_PREFIX) +
        sizeof(UI_UNLOCALIZED_SUFFIX) - 2,
    UI_COMPAT_LANGUAGE_SPANISH = 4
};


// Source: uo_ui_mp_x86.dll 0x400577d8..0x40057bd7.
// NOT_FROM_ORIGINAL_SOURCE: this buffer includes the complete parser-domain
// reference, both warning decorations, and one shared terminator.
static char ui_translationBuffer[UI_TRANSLATION_BUFFER_SIZE];

// Source: uo_ui_mp_x86.dll 0x40011740..0x40011841
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40011740_40011841.mcode
// Exact same-module PPC symbol: UI_SafeTranslateString.
const char *UI_SafeTranslateString(const char *reference)
{
    const char *translation = trap_SE_TranslateReference(reference);


    if (translation != NULL) {
        return translation;
    }

    if (cl_languageWarnings.integer != 0) {
        if (cl_languageWarningsAsErrors.integer != 0) {
            Com_Error(ERR_LOCALIZATION,
                      "Could not translate ui string \"%s\"", reference);
        } else {
            Com_Printf("^3WARNING: Could not translate ui string \"%s\"\n",
                       reference);
        }
        /* NOT_FROM_ORIGINAL_SOURCE: preserve parser-domain references while
         * bounding nonstandard callers to the shared fallback buffer. */
        Com_sprintf(ui_translationBuffer, sizeof(ui_translationBuffer),
                    "%s%s%s", UI_UNLOCALIZED_PREFIX, reference,
                    UI_UNLOCALIZED_SUFFIX);
    } else {
        /* NOT_FROM_ORIGINAL_SOURCE: the warning-disabled path shares the same
         * fallback-buffer capacity. */
        Q_strncpyz(ui_translationBuffer, reference,
                   (int32_t)sizeof(ui_translationBuffer));
    }
    return ui_translationBuffer;
}
