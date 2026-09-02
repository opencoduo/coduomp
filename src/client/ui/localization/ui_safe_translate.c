#include "../module/ui_functions.h"
#include "../module/ui_globals.h"

#include <stddef.h>
#include <string.h>

static const char UI_UNLOCALIZED_PREFIX[] = "^1UNLOCALIZED(^7";
static const char UI_UNLOCALIZED_SUFFIX[] = "^1)^7";

enum {
    UI_TRANSLATION_BUFFER_SIZE = MAX_STRING_CHARS + sizeof(UI_UNLOCALIZED_PREFIX) + sizeof(UI_UNLOCALIZED_SUFFIX) - 2,
    UI_COMPAT_LANGUAGE_SPANISH = 4
};

typedef struct ui_compat_translation_s {
    const char *reference;
    const char *english;
    const char *spanish;
} ui_compat_translation_t;

/* NOT_FROM_ORIGINAL_SOURCE_STORAGE_FILE: fallback text for labels introduced
 * by the recovered client's menu extensions. A mounted localization entry is
 * queried first and therefore overrides these built-in strings. */
static const ui_compat_translation_t uiCompatTranslations[] = {
    {"CODUOMP_GRAPHICS_CURRENT_DISPLAY", "Maximum Display (Automatic)", "Resolución máxima (automática)"},
    {"CODUOMP_GRAPHICS_WINDOWED", "Windowed", "En ventana"},
    {"CODUOMP_GRAPHICS_FULLSCREEN", "Fullscreen", "Pantalla completa"},
    {"CODUOMP_GRAPHICS_BORDERLESS", "Borderless", "Sin bordes"},
    {"CODUOMP_GRAPHICS_DISPLAY_MODE", "Display Mode", "Modo de pantalla"},
    {"CODUOMP_GRAPHICS_FILL_SCREEN", "Fill Screen (Wider FOV)", "Llenar pantalla (FOV m\341s amplio)"},
    {"CODUOMP_GRAPHICS_LETTERBOX", "Classic 4:3 (Letterboxed)", "4:3 cl\341sico (con barras)"},
    {"CODUOMP_GRAPHICS_GAMEPLAY_VIEW", "Gameplay View", "Vista de juego"},
    {"CODUOMP_ADVANCED", "Advanced", "Avanzado"},
    {"CODUOMP_SERVER_CACHE", "Server Cache", "Cach\351 del servidor"}};

/* NOT_FROM_ORIGINAL_SOURCE: resolves code-authored UI references after the
 * ordinary mounted localization lookup misses. */
static const char *ui_compat_translate_string(const char *reference)
{
    const int32_t language = (int32_t)trap_Cvar_VariableValue("cl_language");

    for (size_t index = 0; index < sizeof(uiCompatTranslations) / sizeof(uiCompatTranslations[0]); ++index) {
        const ui_compat_translation_t *const translation = &uiCompatTranslations[index];

        if (strcmp(reference, translation->reference) == 0) {
            return language == UI_COMPAT_LANGUAGE_SPANISH ? translation->spanish : translation->english;
        }
    }
    return NULL;
}

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

    if (translation == NULL)
        translation = ui_compat_translate_string(reference);

    if (translation != NULL) {
        return translation;
    }

    if (cl_languageWarnings.integer != 0) {
        if (cl_languageWarningsAsErrors.integer != 0) {
            Com_Error(ERR_LOCALIZATION, "Could not translate ui string \"%s\"", reference);
        } else {
            Com_Printf("^3WARNING: Could not translate ui string \"%s\"\n", reference);
        }
        /* NOT_FROM_ORIGINAL_SOURCE: preserve parser-domain references while
         * bounding nonstandard callers to the shared fallback buffer. */
        Com_sprintf(ui_translationBuffer, sizeof(ui_translationBuffer), "%s%s%s", UI_UNLOCALIZED_PREFIX, reference, UI_UNLOCALIZED_SUFFIX);
    } else {
        /* NOT_FROM_ORIGINAL_SOURCE: the warning-disabled path shares the same
         * fallback-buffer capacity. */
        Q_strncpyz(ui_translationBuffer, reference, (int32_t)sizeof(ui_translationBuffer));
    }
    return ui_translationBuffer;
}
