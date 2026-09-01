#include "ui_parse.h"

#include "client/common/client_legacy_crt.h"
#include "qcommon/com_parse.h"
#include "qcommon/precompiler_types.h"
#include "qcommon/q_string.h"
#include "ui_memory.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    PC_COLOR_COMPONENT_COUNT = 4,
    PC_SCRIPT_COMPARE_LIMIT = 99999,
    PC_SOURCE_FILENAME_SIZE = 128
};

/*
 * Complete shared UI value/precompiler parsing layer.  The Windows cgame and
 * UI bodies have the same instruction graphs throughout this cluster; their
 * absolute calls, globals, and parser syscall numbers are module-local:
 *
 *   cgame 0x30050200..0x300509df
 *   UI    0x40011d20..0x400124ff
 *
 * Init_Display immediately follows at 0x300509e0/0x40012500.  The Mac
 * cgame/UI traceback symbols independently retain every public name, matching
 * function size, and ordering.  Both Windows diagnostic bodies also use the
 * same 128-byte filename frame at 0x30050010/0x30050090 and
 * 0x40011b30/0x40011bb0.
 */

/* The retail buffers occupy different observable data neighborhoods in the
 * two DLLs, so each module retains its storage at its module-local location. */
extern char pc_sourceWarningMessage[4096];
extern char pc_sourceErrorMessage[4096];

void Com_Printf(const char *format, ...);
void Com_Error(errorParm_t level, const char *format, ...);

void Init_Display(displayContextDef_t *displayContext)
{
    DC = displayContext;
}

qboolean Float_Parse(char **handle, float *destination)
{
    char *token;

    if (com_parseSession->ungetToken != 0) {
        int32_t wasSpaceDelimited = com_parseSession->spaceDelimited;

        com_parseSession->ungetToken = 0;
        if (wasSpaceDelimited == 0) {
            token = com_parseSession->token;
            goto have_token;
        }
        *handle = com_parseSession->savedParse;
        com_parseSession->line = com_parseSession->savedLine;
    }
    token = Com_ParseExt(handle, qfalse);

have_token:
    if (token == NULL || token[0] == '\0')
        return qfalse;

    *destination = (float)atof(token);
    return qtrue;
}

qboolean Int_Parse(char **handle, int32_t *destination)
{
    char *token;

    if (com_parseSession->ungetToken != 0) {
        int32_t wasSpaceDelimited = com_parseSession->spaceDelimited;

        com_parseSession->ungetToken = 0;
        if (wasSpaceDelimited == 0) {
            token = com_parseSession->token;
            goto have_token;
        }
        *handle = com_parseSession->savedParse;
        com_parseSession->line = com_parseSession->savedLine;
    }
    token = Com_ParseExt(handle, qfalse);

have_token:
    if (token == NULL || token[0] == '\0')
        return qfalse;

    *destination = coduo_crt_atoi(token);
    return qtrue;
}

qboolean Color_Parse(char **handle, vec4_t destination)
{
    int32_t component;

    for (component = 0; component < PC_COLOR_COMPONENT_COUNT; ++component) {
        if (!Float_Parse(handle, &destination[component]))
            return qfalse;
    }
    return qtrue;
}

qboolean Rect_Parse(char **handle, rectDef_t *destination)
{
    return Float_Parse(handle, &destination->x) &&
           Float_Parse(handle, &destination->y) &&
           Float_Parse(handle, &destination->w) &&
           Float_Parse(handle, &destination->h);
}

qboolean String_Parse(char **handle, const char **destination)
{
    char *token;

    if (com_parseSession->ungetToken != 0) {
        int32_t wasSpaceDelimited = com_parseSession->spaceDelimited;

        com_parseSession->ungetToken = 0;
        if (wasSpaceDelimited == 0) {
            token = com_parseSession->token;
            goto have_token;
        }
        *handle = com_parseSession->savedParse;
        com_parseSession->line = com_parseSession->savedLine;
    }
    token = Com_ParseExt(handle, qfalse);

have_token:
    if (token == NULL || token[0] == '\0')
        return qfalse;

    if (token[0] == '@') {
        const char *translated = DC->translateString(token + 1);

        if (translated != NULL) {
            *destination = String_Alloc(translated);
            return qtrue;
        }
        if (DC->getCVarValue("cl_languagewarnings") != 0.0L) {
            if (DC->getCVarValue("cl_languagewarningsaserrors") != 0.0L) {
                Com_Error(ERR_LOCALIZATION,
                          "Could not translate menu string reference %s",
                          token);
            } else {
                Com_Printf("^3WARNING: Could not translate menu string "
                           "reference %s\n", token);
            }
        }
    }

    *destination = String_Alloc(token);
    return qtrue;
}

qboolean PC_Float_Parse(int32_t sourceHandle, float *destination)
{
    pc_token_t token;
    qboolean negative = qfalse;

    if (!trap_PC_ReadToken(sourceHandle, &token))
        return qfalse;
    if (token.string[0] == '-') {
        if (!trap_PC_ReadToken(sourceHandle, &token))
            return qfalse;
        negative = qtrue;
    }
    if (token.type != PC_TOKEN_TYPE_NUMBER) {
        PC_SourceError(sourceHandle, "expected float but found %s\n",
                       token.string);
        return qfalse;
    }

    if (negative) {
        *destination = (float)(-(long double)token.floatValue);
    } else {
        memcpy(destination, &token.floatValue, sizeof(*destination));
    }
    return qtrue;
}

qboolean PC_Int_Parse(int32_t sourceHandle, int32_t *destination)
{
    pc_token_t token;
    qboolean negative = qfalse;
    uint32_t valueBits;

    if (!trap_PC_ReadToken(sourceHandle, &token))
        return qfalse;
    if (token.string[0] == '-') {
        if (!trap_PC_ReadToken(sourceHandle, &token))
            return qfalse;
        negative = qtrue;
    }
    if (token.type != PC_TOKEN_TYPE_NUMBER) {
        PC_SourceError(sourceHandle, "expected integer but found %s\n",
                       token.string);
        return qfalse;
    }

    valueBits = (uint32_t)token.intValue;
    if (negative)
        valueBits = 0u - valueBits;
    memcpy(destination, &valueBits, sizeof(valueBits));
    return qtrue;
}

qboolean PC_Color_Parse(int32_t sourceHandle, vec4_t destination)
{
    int32_t component;

    for (component = 0; component < PC_COLOR_COMPONENT_COUNT; ++component) {
        float value;

        if (!PC_Float_Parse(sourceHandle, &value))
            return qfalse;
        /* Retail performs an x87 load/store rather than a raw dword copy. */
        destination[component] = (float)(long double)value;
    }
    return qtrue;
}

qboolean PC_Rect_Parse(int32_t sourceHandle, rectDef_t *destination)
{
    return PC_Float_Parse(sourceHandle, &destination->x) &&
           PC_Float_Parse(sourceHandle, &destination->y) &&
           PC_Float_Parse(sourceHandle, &destination->w) &&
           PC_Float_Parse(sourceHandle, &destination->h);
}

qboolean PC_String_Parse(int32_t sourceHandle, const char **destination)
{
    pc_token_t token;
    const char *result;

    if (!trap_PC_ReadToken(sourceHandle, &token))
        return qfalse;

    result = token.string;
    if (token.string[0] == '@') {
        const char *translated = DC->translateString(token.string + 1);

        if (translated != NULL) {
            result = translated;
        } else if (DC->getCVarValue("cl_languagewarnings") != 0.0L) {
            if (DC->getCVarValue("cl_languagewarningsaserrors") != 0.0L) {
                Com_Error(ERR_LOCALIZATION,
                          "Could not translate menu string reference %s",
                          token.string);
            } else {
                Com_Printf("^3WARNING: Could not translate menu string "
                           "reference %s\n", token.string);
            }
        }
    }

    *destination = String_Alloc(result);
    return qtrue;
}

qboolean PC_Char_Parse(int32_t sourceHandle, char *destination)
{
    pc_token_t token;

    if (!trap_PC_ReadToken(sourceHandle, &token))
        return qfalse;
    *destination = token.string[0];
    return qtrue;
}

qboolean PC_Script_Parse(int32_t sourceHandle, const char **destination)
{
    char script[MAX_STRING_CHARS];
    pc_token_t token;

    /* Retail clears the complete 1024-byte stack accumulator. */
    memset(script, 0, sizeof(script));
    if (!trap_PC_ReadToken(sourceHandle, &token) ||
        Q_stricmpn(token.string, "{", PC_SCRIPT_COMPARE_LIMIT) != 0) {
        return qfalse;
    }

    for (;;) {
        if (!trap_PC_ReadToken(sourceHandle, &token))
            return qfalse;
        if (Q_stricmpn(token.string, "}", PC_SCRIPT_COMPARE_LIMIT) == 0) {
            *destination = String_Alloc(script);
            return qtrue;
        }

        if (token.string[1] != '\0') {
            Q_strcat(script, sizeof(script), va("\"%s\"", token.string));
        } else {
            Q_strcat(script, sizeof(script), token.string);
        }
        Q_strcat(script, sizeof(script), " ");
    }
}

void PC_SourceWarning(int32_t sourceHandle, const char *format, ...)
{
    va_list arguments;
    char filename[PC_SOURCE_FILENAME_SIZE];
    int32_t line = 0;

    va_start(arguments, format);
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    (void)vsnprintf(pc_sourceWarningMessage,
                    sizeof(pc_sourceWarningMessage), format, arguments);
    va_end(arguments);

    filename[0] = '\0';
    trap_PC_SourceFileAndLine(sourceHandle, filename, &line);
    Com_Printf("^3WARNING: %s, line %d: %s\n",
               filename, line, pc_sourceWarningMessage);
}

void PC_SourceError(int32_t sourceHandle, const char *format, ...)
{
    va_list arguments;
    char filename[PC_SOURCE_FILENAME_SIZE];
    int32_t line = 0;

    va_start(arguments, format);
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    (void)vsnprintf(pc_sourceErrorMessage, sizeof(pc_sourceErrorMessage),
                    format, arguments);
    va_end(arguments);

    filename[0] = '\0';
    trap_PC_SourceFileAndLine(sourceHandle, filename, &line);
    Com_Printf("^1Menu load error: %s, line %d: %s\n",
               filename, line, pc_sourceErrorMessage);
}
