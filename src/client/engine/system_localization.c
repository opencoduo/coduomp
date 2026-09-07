#include "system_localization.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

enum {
    SYS_LOCALIZATION_TEXT_CAPACITY = 4096,
    SYS_LOCALIZATION_LANGUAGE_SPANISH = 4
};

/* Original storage is 0x009cdda8..0x009ceda7. The two original pointer
 * globals select the complete buffer and the entries following its first
 * language-name line. */
static char sysLocalizationText[SYS_LOCALIZATION_TEXT_CAPACITY];
static char *sysLocalizationBuffer;  /* original 0x009cdda0 */
static char *sysLocalizationEntries; /* original 0x009cdda4 */

/* NOT_FROM_ORIGINAL_SOURCE: startup tables can omit the improper-quit strings
 * or be absent altogether. These Windows-1252 fallbacks apply when the file
 * language or the Windows UI language is Spanish and no translated text is
 * available. File-provided translations take priority. */
static int32_t coduompLocalizationLanguageIndex;
static const char coduompSpanishImproperQuitBody[] =
    "Parece que Call of Duty no se cerr\363 correctamente la \372ltima vez que se ejecut\363.\n"
    "\277Quieres ejecutar el juego en modo seguro?\n\n"
    "Esta opci\363n es recomendable para la mayor\355a de los usuarios.\n"
    "Cambiar\341 la configuraci\363n del sistema, pero no tus controles.";
static const char coduompSpanishImproperQuitTitle[] =
    "\277Ejecutar en modo seguro?";

/* Source: CoDUOMP.exe 0x0046a720..0x0046a7e8.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046a720_0046a7e9.mcode.
 * Role name: loads the Windows early-startup localization.txt resource and
 * returns the language index named on its first line. The original statically
 * linked MSVC FILE calls are represented by the native C library boundary. */
int32_t Sys_InitLocalization(void)
{
    int32_t languageIndex = 0;
    FILE *file;

    sysLocalizationBuffer = NULL;
    sysLocalizationEntries = NULL;
    coduompLocalizationLanguageIndex = 0;

    file = fopen("localization.txt", "r");
    if (file == NULL)
        return 0;

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if (fseek(file, 0, SEEK_END) != 0) {
        fputs("CoDUOMP: localization.txt is unreadable; ignoring it\n", stderr);
        (void)fclose(file);
        return 0;
    }
    const long fileLength = ftell(file);
    if (fileLength < 0 || fileLength >= SYS_LOCALIZATION_TEXT_CAPACITY) {
        fputs("CoDUOMP: localization.txt is too large or unreadable; ignoring it\n", stderr);
        (void)fclose(file);
        return 0;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fputs("CoDUOMP: localization.txt is unreadable; ignoring it\n", stderr);
        (void)fclose(file);
        return 0;
    }

    sysLocalizationBuffer = sysLocalizationText;
    const uint32_t bytesRead = (uint32_t)fread(
        sysLocalizationBuffer, 1, (size_t)fileLength, file);
    (void)fclose(file);
    sysLocalizationBuffer[bytesRead] = '\0';

    if (bytesRead == 0) {
        sysLocalizationBuffer = NULL;
        return 0;
    }

    for (uint32_t offset = 0;
         sysLocalizationBuffer[offset] != '\0';
         ++offset) {
        if (sysLocalizationBuffer[offset] == '\n') {
            sysLocalizationBuffer[offset] = '\0';
            sysLocalizationEntries =
                &sysLocalizationBuffer[offset + 1];
            (void)SEH_GetLanguageIndexForName(
                sysLocalizationBuffer, &languageIndex);
            coduompLocalizationLanguageIndex = languageIndex;
            break;
        }
    }

    return languageIndex;
}

/* Source: CoDUOMP.exe 0x0046a7f0..0x0046a7fc, recovered from an executable
 * gap after repairing the missing Ghidra function entry.
 * Role name: releases the logical early-localization view; its backing array
 * is static and is not freed. */
void Sys_ShutdownLocalization(void)
{
    sysLocalizationBuffer = NULL;
    sysLocalizationEntries = NULL;
    coduompLocalizationLanguageIndex = 0;
}

/* Source: CoDUOMP.exe 0x0046a800..0x0046a80e, recovered from an executable
 * gap after repairing the missing Ghidra function entry.
 * Role name: copies a selected early-localization string into va storage. */
static const char *Sys_CopyLocalizationString(const char *text)
{
    return va("%s", text);
}

/* Source: CoDUOMP.exe 0x0046a810..0x0046a953.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046a810_0046a954.mcode.
 * Role name: resolves a reference in the key/value token stream loaded from
 * localization.txt. Missing references deliberately return a va copy of the
 * reference itself. */
const char *Sys_LocalizeString(const char *reference)
{
    const char *result = reference;
    char *parse = sysLocalizationEntries;

    Com_BeginParseSession("localization");
    for (;;) {
        const char *key = Com_ParseExt(&parse, qtrue);
        if (*key == '\0')
            break;

        const qboolean matched =
            strcmp(key, reference) == 0 ? qtrue : qfalse;
        const char *value = Com_ParseExt(&parse, qtrue);
        if (*value == '\0')
            break;

        if (matched != qfalse) {
            result = value;
            break;
        }
    }
    Com_EndParseSession();

    /* NOT_FROM_ORIGINAL_SOURCE: this prompt precedes cvar and game-language
     * initialization. A missing entry or a value that repeats its key can
     * use the Spanish Windows UI language even if the file did not select
     * Spanish. PRIMARYLANGID includes every Spanish region, such as Argentina. */
    if (strcmp(result, reference) == 0) {
        qboolean spanish = coduompLocalizationLanguageIndex == SYS_LOCALIZATION_LANGUAGE_SPANISH;
#if defined(_WIN32)
        if (PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_SPANISH)
            spanish = qtrue;
#endif
        if (spanish != qfalse) {
            if (strcmp(reference, "WIN_IMPROPER_QUIT_BODY") == 0) {
                result = coduompSpanishImproperQuitBody;
            } else if (strcmp(reference, "WIN_IMPROPER_QUIT_TITLE") == 0) {
                result = coduompSpanishImproperQuitTitle;
            }
        }
    }
    return Sys_CopyLocalizationString(result);
}
