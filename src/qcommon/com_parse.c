#include "com_parse.h"
#include "com_parse_error_binding.h"
#include "q_string.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    COM_PARSE_INITIAL_LINE = 1,
    COM_PARSE_ERROR_MESSAGE_BYTES = 32000,
    COM_PARSE_TOKEN_LAST_INDEX = MAX_TOKEN_CHARS - 1
};

void Com_Error(errorParm_t errorCode, const char *format, ...);
void Com_Printf(const char *format, ...);

/*
 * Complete common Com text-parser subsystem. The four Windows i386 bodies
 * have matching function boundaries and operation graphs throughout the
 * cluster; absolute data/call operands are image-local. The Windows game DLL
 * alone binds parser diagnostics to G_Error/G_Printf rather than
 * Com_Error/Com_Printf. The Linux engine and game module retain the same state
 * transitions, token grammar, punctuation order, unchecked malformed-input
 * behavior, and public API. The Mac client traceback symbols independently
 * retain Com_Parse, Com_ParseExt, Com_ParseOnLine, and Com_ParseCSV in cgame,
 * UI, game, and the engine.
 *
 * Cluster starts:
 *   CoDUOMP.exe                 0x0044e550
 *   uo_cgame_mp_x86.dll        0x3004d250
 *   uo_ui_mp_x86.dll           0x40005260
 *   uo_game_mp_x86.dll         0x20056a80
 *   coduo_lnxded               0x0808521c
 *   game.mp.uo.i386.so         RVA 0x00091a44
 */

/* Original operator/session/state storage:
 *   CoDUOMP.exe: operators 0x005c5cd8, sessions 0x005c5d18,
 *       current-session pointer 0x005ca2d8, depth 0x009b8020,
 *       rest-of-line buffer 0x009b8028, token cursors 0x009b8428/0x009b842c.
 *   uo_cgame_mp_x86.dll: operators 0x300866bc, sessions 0x300866f8,
 *       current-session pointer 0x3008acb8, depth 0x300dca48,
 *       rest-of-line buffer 0x300dca50, token cursors 0x300dce50/0x300dce54.
 *   uo_ui_mp_x86.dll: operators 0x4003b7dc, sessions 0x4003b818,
 *       current-session pointer 0x4003fdd8, depth 0x40041838,
 *       rest-of-line buffer 0x40041840, token cursors 0x40041c40/0x40041c44.
 * The operator tables contain the same fourteen strings in the order below,
 * followed by a terminating NULL pointer. */
static const char *const com_parseOperators[] = {
    "+=", "-=", "*=", "/=", "&=", "|=", "++", "--",
    "&&", "||", "<=", ">=", "==", "!=", NULL
};

/* Each binary initializes only session zero with line 1, space delimiting
 * enabled, negative-number parsing enabled, and saved-line 1. Sessions 1..15
 * are initially zero; Com_BeginParseSession establishes only line, ungetToken,
 * spaceDelimited, csv, and name for a nested session. */
#define COM_PARSE_STATE_INITIALIZER \
    { \
        .line = COM_PARSE_INITIAL_LINE, \
        .spaceDelimited = qtrue, \
        .parseNegativeNumbers = qtrue, \
        .savedLine = COM_PARSE_INITIAL_LINE \
    }
com_parse_session_t com_parseSessions[MAX_PARSE_SESSIONS] = {
    COM_PARSE_STATE_INITIALIZER
};
#undef COM_PARSE_STATE_INITIALIZER
/* PE_RELOCATION_VALUES_VERIFIED: original 0x005ca2d8 contains 0x005c5d18,
 * the address of com_parseSessions[0]. */
com_parse_session_t *com_parseSession = &com_parseSessions[0];
int32_t com_numParseSessions;
char *com_lastTokenStart;
char *com_tokenStart;
/* Original 1024-byte rest-of-line accumulator at 0x009b8028. */
static char com_parseRestOfLineBuffer[MAX_TOKEN_CHARS];

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the token buffer's
 * repeated 1023-byte append guard. */
static void qcommon_compat_append_parse_token_character(int32_t *length,
                                                         char character)
{
    if (*length < COM_PARSE_TOKEN_LAST_INDEX)
        com_parseSession->token[(*length)++] = character;
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level spelling of the parser's repeated
 * ASCII range tests. */
static qboolean qcommon_compat_is_parse_digit(char character)
{
    const int32_t signedCharacter = (int8_t)character;
    return signedCharacter >= '0' && signedCharacter <= '9';
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level spelling of the parser's repeated
 * ASCII range tests. */
static qboolean qcommon_compat_is_parse_alpha(char character)
{
    const int32_t signedCharacter = (int8_t)character;
    return (signedCharacter >= 'a' && signedCharacter <= 'z') ||
           (signedCharacter >= 'A' && signedCharacter <= 'Z');
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level spelling of the parser's repeated
 * identifier continuation test. */
static qboolean qcommon_compat_is_parse_name_character(char character)
{
    return qcommon_compat_is_parse_alpha(character) ||
           qcommon_compat_is_parse_digit(character) ||
           character == '_' || character == '/' || character == '\\' ||
           character == ':' || character == '.';
}

/* Source: CoDUOMP.exe 0x0044e550..0x0044e5c5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0044e550_0044e5c6.mcode.
 * Name and signature: exact same-module Mac symbol Com_BeginParseSession. */
void Com_BeginParseSession(const char *name)
{
    if (com_numParseSessions == MAX_PARSE_SESSIONS - 1)
        COM_PARSE_ERROR(0,
                        "\x15" "Com_BeginParseSession: session overflow");

    com_parseSession = &com_parseSessions[++com_numParseSessions];
    com_parseSession->line = COM_PARSE_INITIAL_LINE;
    com_parseSession->ungetToken = qfalse;
    com_parseSession->spaceDelimited = qtrue;
    com_parseSession->csv = qfalse;
    Q_strncpyz(com_parseSession->name, name,
               (int32_t)sizeof(com_parseSession->name));
}

/* Source: CoDUOMP.exe 0x0044e5d0..0x0044e602.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0044e5d0_0044e603.mcode.
 * Name and signature: exact same-module Mac symbol Com_EndParseSession. */
void Com_EndParseSession(void)
{
    if (com_numParseSessions == 0)
        COM_PARSE_ERROR(0,
                        "\x15" "Com_EndParseSession: session underflow");

    com_parseSession = &com_parseSessions[--com_numParseSessions];
}

/* Source: CoDUOMP.exe 0x0044e610..0x0044e624, recovered from an executable
 * gap. Name and signature: exact same-module Mac symbol
 * Com_ResetParseSessions. */
void Com_ResetParseSessions(void)
{
    com_numParseSessions = 0;
    com_parseSession = &com_parseSessions[0];
}

/* Source: CoDUOMP.exe 0x0044e630..0x0044e63b, recovered from an executable
 * gap. Name: exact same-module Mac symbol Com_SetSpaceDelimited. */
void Com_SetSpaceDelimited(qboolean enabled)
{
    com_parseSession->spaceDelimited = enabled;
}

/* Source: CoDUOMP.exe 0x0044e640..0x0044e64b, recovered from an executable
 * gap. Name: exact same-module Mac symbol Com_SetCSV. */
void Com_SetCSV(qboolean enabled)
{
    com_parseSession->csv = enabled;
}

/* Source: CoDUOMP.exe 0x0044e650..0x0044e65b, recovered from an executable
 * gap. Name: exact same-module Mac symbol Com_SetParseNegativeNumbers. */
void Com_SetParseNegativeNumbers(qboolean enabled)
{
    com_parseSession->parseNegativeNumbers = enabled;
}

/* Source: CoDUOMP.exe 0x0044e660..0x0044e66b, recovered from an executable
 * gap. Name: exact same-family Mac symbol Com_GetCurrentParseLine. */
int32_t Com_GetCurrentParseLine(void)
{
    return com_parseSession->line;
}

/* Source: CoDUOMP.exe 0x0044e670..0x0044e6d5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0044e670_0044e6d6.mcode.
 * Name and signature: exact same-module Mac symbol Com_ScriptError. */
void Com_ScriptError(const char *format, ...)
{
    char message[COM_PARSE_ERROR_MESSAGE_BYTES];
    va_list arguments;

    va_start(arguments, format);
    /* NOT_FROM_ORIGINAL_SOURCE: bound the formatted parser diagnostic to its
     * fixed destination. */
    (void)vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);
    COM_PARSE_ERROR(ERR_DROP, "\x15" "File %s, line %i: %s",
                    com_parseSession->name, com_parseSession->line, message);
}

/* Source: CoDUOMP.exe 0x0044e6e0..0x0044e743.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0044e6e0_0044e744.mcode.
 * Name inferred as the non-fatal sibling of same-module Com_ScriptError; its
 * format string and Com_Printf call prove the warning behavior. */
void Com_ScriptWarning(const char *format, ...)
{
    char message[COM_PARSE_ERROR_MESSAGE_BYTES];
    va_list arguments;

    va_start(arguments, format);
    /* NOT_FROM_ORIGINAL_SOURCE: bound the formatted parser diagnostic to its
     * fixed destination. */
    (void)vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);
    COM_PARSE_PRINT("File %s, line %i: %s", com_parseSession->name,
                    com_parseSession->line, message);
}

/* Source: CoDUOMP.exe 0x0044e750..0x0044e79e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0044e750_0044e79f.mcode.
 * Name and signature: exact same-module Mac symbol Com_UngetToken. */
void Com_UngetToken(void)
{
    if (com_parseSession->ungetToken != qfalse)
        Com_ScriptError("UngetToken called twice");

    com_tokenStart = com_lastTokenStart;
    com_parseSession->ungetToken = qtrue;
}

/* Source: CoDUOMP.exe 0x0044e7a0..0x0044e7d2, recovered from an executable
 * gap. Name: exact same-module Mac symbol Com_ParseSetMark. */
void Com_ParseSetMark(char **data, com_parse_mark_t *mark)
{
    mark->line = com_parseSession->line;
    mark->parse = *data;
    mark->ungetToken = com_parseSession->ungetToken;
    mark->savedLine = com_parseSession->savedLine;
    mark->savedParse = com_parseSession->savedParse;
}

/* Source: CoDUOMP.exe 0x0044e7e0..0x0044e80e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0044e7e0_0044e80f.mcode.
 * Name: exact same-module Mac symbol Com_ParseReturnToMark. */
void Com_ParseReturnToMark(char **data, const com_parse_mark_t *mark)
{
    com_parseSession->line = mark->line;
    *data = mark->parse;
    com_parseSession->ungetToken = mark->ungetToken;
    com_parseSession->savedLine = mark->savedLine;
    com_parseSession->savedParse = mark->savedParse;
}

/* Source: CoDUOMP.exe 0x0044e810..0x0044e842.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0044e810_0044e843.mcode.
 * Name and signature: exact same-module Mac symbol SkipWhitespace. */
char *SkipWhitespace(char *data, qboolean *hasNewLines)
{
    while ((int8_t)*data <= ' ') {
        if (*data == '\0')
            return NULL;
        if (*data == '\n') {
            ++com_parseSession->line;
            *hasNewLines = qtrue;
        }
        ++data;
    }
    return data;
}

/* Source: CoDUOMP.exe 0x0044e850..0x0044e8da.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0044e850_0044e8db.mcode.
 * Name and signature: exact same-module Mac symbol Com_Compress. Newlines in
 * block comments are retained, while line comments leave their terminating
 * newline for the ordinary copy path. */
int32_t Com_Compress(char *data)
{
    char *output = data;
    char *scan = data;
    int32_t length = 0;

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (data == NULL) {
        return 0;
    }

    while (*scan != '\0') {
        if (*scan == '\r' || *scan == '\n') {
            *output++ = *scan++;
            ++length;
        } else if (scan[0] == '/' && scan[1] == '/') {
            while (*scan != '\0' && *scan != '\n')
                ++scan;
        } else if (scan[0] == '/' && scan[1] == '*') {
            while (*scan != '\0' &&
                   (scan[0] != '*' || scan[1] != '/')) {
                if (*scan == '\n') {
                    *output++ = '\n';
                    ++length;
                }
                ++scan;
            }
            if (*scan != '\0')
                scan += 2;
        } else {
            *output++ = *scan++;
            ++length;
        }
    }

    *output = '\0';
    return length;
}

/* Source: CoDUOMP.exe 0x0044e8e0..0x0044e8e5, recovered from an executable
 * gap. Name and signature: exact same-module Mac symbol
 * Com_GetLastTokenPos. */
char *Com_GetLastTokenPos(void)
{
    return com_tokenStart;
}

/* Source: CoDUOMP.exe 0x0044e8f0..0x0044e9ae.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0044e8f0_0044e9af.mcode.
 * Name: exact same-module Mac symbol Com_ParseCSV. A doubled quote inside a
 * quoted field emits one quote; an ordinary closing quote resumes delimiter
 * scanning. */
static char *Com_ParseCSV(char **data, qboolean allowLineBreaks)
{
    char *text = *data;
    int32_t tokenLength = 0;

    com_parseSession->token[0] = '\0';

    if (allowLineBreaks == qfalse) {
        if (*text == '\r' || *text == '\n')
            return com_parseSession->token;
    } else {
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        while (*text == '\r' || *text == '\n')
            ++text;
    }

    com_lastTokenStart = com_tokenStart;
    com_tokenStart = text;

    while (*text != '\0' && *text != ',' && *text != '\n') {
        if (*text == '\r') {
            ++text;
        } else if (*text == '"') {
            ++text;
            /* NOT_FROM_ORIGINAL_SOURCE: an unterminated quoted field ends at
             * the source NUL and follows the ordinary EOF result below. */
            for (;;) {
                if (*text == '\0')
                    break;
                if (*text == '"') {
                    if (text[1] != '"') {
                        ++text;
                        break;
                    }
                    qcommon_compat_append_parse_token_character(&tokenLength, '"');
                    text += 2;
                } else {
                    qcommon_compat_append_parse_token_character(&tokenLength,
                                                         *text++);
                }
            }
        } else {
            qcommon_compat_append_parse_token_character(&tokenLength, *text++);
        }
    }

    if (*text == '\0') {
        *data = NULL;
    } else {
        if (*text != '\n')
            ++text;
        *data = text;
    }

    com_parseSession->token[tokenLength] = '\0';
    return com_parseSession->token;
}

/* Source: CoDUOMP.exe 0x0044e9b0..0x0044ed1f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0044e9b0_0044ed20.mcode.
 * Name and signature: exact same-module Mac symbol Com_ParseExt. */
char *Com_ParseExt(char **data, qboolean allowLineBreaks)
{
    qboolean hasNewLines = qfalse;
    int32_t tokenLength = 0;
    char *text;
    char character;

    if (data == NULL)
        COM_PARSE_ERROR(0, "\x15" "Com_ParseExt: NULL data_p");

    text = *data;
    com_parseSession->token[0] = '\0';

    if (text == NULL) {
        *data = NULL;
        return com_parseSession->token;
    }

    com_parseSession->savedLine = com_parseSession->line;
    com_parseSession->savedParse = text;

    if (com_parseSession->csv != qfalse)
        return Com_ParseCSV(data, allowLineBreaks);

    while ((text = SkipWhitespace(text, &hasNewLines)) != NULL) {
        if (hasNewLines != qfalse && allowLineBreaks == qfalse) {
            *data = text;
            return com_parseSession->token;
        }

        character = *text;
        if (character == '/' && text[1] == '/') {
            while (*text != '\0' && *text != '\n')
                ++text;
            continue;
        }

        if (character == '/' && text[1] == '*') {
            while (*text != '\0' &&
                   (*text != '*' || text[1] != '/')) {
                if (*text == '\n')
                    ++com_parseSession->line;
                ++text;
            }
            if (*text != '\0')
                text += 2;
            continue;
        }

        com_lastTokenStart = com_tokenStart;
        com_tokenStart = text;

        if (character == '"') {
            ++text;
            for (;;) {
                character = *text;
                /* NOT_FROM_ORIGINAL_SOURCE: an unterminated quoted token ends
                 * at the source NUL, preserving its partial contents and the
                 * ordinary EOF cursor. */
                if (character == '\0')
                    break;
                ++text;
                if (character == '\\' && *text == '"') {
                    character = *text++;
                } else {
                    if (character == '"')
                        break;
                }
                /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
                if (character == '\n')
                    ++com_parseSession->line;
                qcommon_compat_append_parse_token_character(&tokenLength, character);
            }
            goto finish_token;
        }

        if (com_parseSession->spaceDelimited != qfalse) {
            do {
                qcommon_compat_append_parse_token_character(&tokenLength, character);
                character = *++text;
            } while ((int8_t)character > ' ');
            goto finish_token;
        }

        if (qcommon_compat_is_parse_digit(character) ||
            (com_parseSession->parseNegativeNumbers != qfalse &&
             character == '-' && qcommon_compat_is_parse_digit(text[1])) ||
            (character == '.' && qcommon_compat_is_parse_digit(text[1]))) {
            do {
                qcommon_compat_append_parse_token_character(&tokenLength, character);
                character = *++text;
            } while (qcommon_compat_is_parse_digit(character) || character == '.');

            if (character == 'e' || character == 'E') {
                qcommon_compat_append_parse_token_character(&tokenLength, character);
                character = *++text;
                if (character == '-' || character == '+') {
                    qcommon_compat_append_parse_token_character(&tokenLength,
                                                         character);
                    character = *++text;
                }
                /* NOT_FROM_ORIGINAL_SOURCE: a malformed exponent prefix may
                 * end at NUL; keep the partial token and ordinary EOF cursor. */
                if (character != '\0') {
                    do {
                        qcommon_compat_append_parse_token_character(&tokenLength,
                                                             character);
                        character = *++text;
                    } while (qcommon_compat_is_parse_digit(character));
                }
            }
            goto finish_token;
        }

        if (qcommon_compat_is_parse_alpha(character) || character == '_' ||
            character == '/' || character == '\\') {
            do {
                qcommon_compat_append_parse_token_character(&tokenLength, character);
                character = *++text;
            } while (qcommon_compat_is_parse_name_character(character));
            goto finish_token;
        }

        for (const char *const *operator = com_parseOperators;
             *operator != NULL; ++operator) {
            const size_t operatorLength = strlen(*operator);
            if (strncmp(text, *operator, operatorLength) == 0) {
                memcpy(com_parseSession->token, *operator, operatorLength);
                tokenLength = (int32_t)operatorLength;
                text += operatorLength;
                goto finish_token;
            }
        }

        com_parseSession->token[0] = *text++;
        tokenLength = 1;

finish_token:
        if (tokenLength == MAX_TOKEN_CHARS)
            tokenLength = 0;
        com_parseSession->token[tokenLength] = '\0';
        *data = text;
        return com_parseSession->token;
    }

    *data = NULL;
    return com_parseSession->token;
}

/* Source: CoDUOMP.exe 0x0044ed20..0x0044ed58.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0044ed20_0044ed59.mcode.
 * Name and signature: exact same-module Mac symbol Com_Parse. */
char *Com_Parse(char **data)
{
    if (com_parseSession->ungetToken != qfalse) {
        com_parseSession->ungetToken = qfalse;
        *data = com_parseSession->savedParse;
        com_parseSession->line = com_parseSession->savedLine;
    }
    return Com_ParseExt(data, qtrue);
}

/* Source: CoDUOMP.exe 0x0044ed60..0x0044eda2.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0044ed60_0044eda3.mcode.
 * Name and signature: exact same-module Mac symbol Com_ParseOnLine. In the
 * non-space-delimited mode, an unget returns the existing token without
 * rewinding the input cursor. */
char *Com_ParseOnLine(char **data)
{
    if (com_parseSession->ungetToken != qfalse) {
        com_parseSession->ungetToken = qfalse;
        if (com_parseSession->spaceDelimited == qfalse)
            return com_parseSession->token;
        *data = com_parseSession->savedParse;
        com_parseSession->line = com_parseSession->savedLine;
    }
    return Com_ParseExt(data, qfalse);
}

/* Source: CoDUOMP.exe 0x0044edb0..0x0044ee4d, recovered from an executable
 * gap. Name and signature: same-family Com_MatchToken, with behavior proved
 * directly by the Windows instructions and format string at 0x0059b100. */
void Com_MatchToken(char **data, const char *match, qboolean warning)
{
    char *token = Com_Parse(data);

    if (strcmp(token, match) != 0) {
        if (warning != qfalse)
            Com_ScriptWarning("MatchToken: %s != %s", token, match);
        else
            Com_ScriptError("MatchToken: %s != %s", token, match);
    }
}

/* Source: CoDUOMP.exe 0x0044ee50..0x0044eec7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0044ee50_0044eec8.mcode.
 * Name and signature: exact same-module Mac symbol
 * Com_SkipBracedSection. */
qboolean Com_SkipBracedSection(char **data, int32_t depth)
{
    qboolean skippedOuterBrace = qfalse;
    int32_t braceDepth = 0;

    do {
        char *token = Com_Parse(data);
        if (token[1] == '\0') {
            if (token[0] == '{') {
                if (braceDepth == depth)
                    skippedOuterBrace = qtrue;
                else
                    ++braceDepth;
            } else if (token[0] == '}') {
                --braceDepth;
            }
        }
    } while (braceDepth != 0 && *data != NULL);

    return skippedOuterBrace;
}

/* Source: CoDUOMP.exe 0x0044eed0..0x0044eefd.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0044eed0_0044eefe.mcode.
 * Name and signature: exact same-module Mac symbol Com_SkipRestOfLine. */
void Com_SkipRestOfLine(char **data)
{
    char *text = *data;

    if (text == NULL)
        return;

    for (;;) {
        const char character = *text;
        if (character == '\0')
            break;
        ++text;
        if (character == '\n') {
            ++com_parseSession->line;
            break;
        }
    }
    *data = text;
}

/* Source: CoDUOMP.exe 0x0044ef00..0x0044f00e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0044ef00_0044f00f.mcode.
 * Name and signature: same-family Com_ParseRestOfLine. The two Q_strcat calls
 * reproduce the binary's inlined overflow checks and bounded appends. */
char *Com_ParseRestOfLine(char **data)
{
    com_parseRestOfLineBuffer[0] = '\0';

    for (;;) {
        char *token = Com_ParseOnLine(data);
        if (token[0] == '\0')
            break;

        if (com_parseRestOfLineBuffer[0] != '\0')
            Q_strcat(com_parseRestOfLineBuffer,
                     sizeof(com_parseRestOfLineBuffer), " ");
        Q_strcat(com_parseRestOfLineBuffer,
                 sizeof(com_parseRestOfLineBuffer), token);
    }

    return com_parseRestOfLineBuffer;
}

/*
 * Source: CoDUOMP.exe 0x0044f010..0x0044f05c.
 * Source: uo_cgame_mp_x86.dll 0x3004dd10..0x3004dd5c.
 * Source: uo_ui_mp_x86.dll 0x40005d20..0x40005d6c.
 * Source: uo_game_mp_x86.dll 0x20057530..0x2005757c.
 * Source: coduo_lnxded 0x08086071..0x080860ac.
 * Source: game.mp.uo.i386.so RVA 0x00092a44..0x00092a93.
 *
 * Windows returns atof's binary64 value directly in ST0 and loads a binary32
 * zero only for the empty-token path. Linux explicitly spills either result
 * to binary32 and reloads it before returning. The wider Windows host carrier
 * preserves that proved difference without changing the i386 ST0 ABI.
 */
#if defined(WINDOWS_BEHAVIOR)
long double Com_ParseFloat(char **data)
{
    char *token = Com_Parse(data);
    return token[0] == '\0' ? 0.0L : (long double)atof(token);
}
#else
float Com_ParseFloat(char **data)
{
    char *token = Com_Parse(data);
    return token[0] == '\0' ? 0.0f : (float)atof(token);
}
#endif

/* Source: CoDUOMP.exe 0x0044f060..0x0044f0aa, recovered from an executable
 * gap. Name and signature: same-family Com_ParseInt. */
int32_t Com_ParseInt(char **data)
{
    char *token = Com_Parse(data);
    return token[0] == '\0' ? 0 : atoi(token);
}

/* Source: CoDUOMP.exe 0x0044f0b0..0x0044f177, recovered from an executable
 * gap. Name and signature: same-family Com_Parse1DMatrix. */
void Com_Parse1DMatrix(char **data, int32_t x, float *matrix)
{
    Com_MatchToken(data, "(", qfalse);
    for (int32_t column = 0; column < x; ++column)
        matrix[column] = (float)atof(Com_Parse(data));
    Com_MatchToken(data, ")", qfalse);
}

/* Source: CoDUOMP.exe 0x0044f180..0x0044f21b, recovered from an executable
 * gap. Name and signature: same-family Com_Parse2DMatrix. */
void Com_Parse2DMatrix(char **data, int32_t y, int32_t x, float *matrix)
{
    Com_MatchToken(data, "(", qfalse);
    for (int32_t row = 0; row < y; ++row)
        Com_Parse1DMatrix(data, x, &matrix[row * x]);
    Com_MatchToken(data, ")", qfalse);
}

/* Source: CoDUOMP.exe 0x0044f220..0x0044f2c9, recovered from an executable
 * gap. Name and signature: same-family Com_Parse3DMatrix. */
void Com_Parse3DMatrix(char **data, int32_t z, int32_t y, int32_t x,
                       float *matrix)
{
    Com_MatchToken(data, "(", qfalse);
    for (int32_t plane = 0; plane < z; ++plane)
        Com_Parse2DMatrix(data, y, x, &matrix[plane * y * x]);
    Com_MatchToken(data, ")", qfalse);
}

#if UINTPTR_MAX == UINT32_MAX
#define COM_PARSE_LAYOUT_ASSERT(name, expression) \
    typedef char name[(expression) ? 1 : -1]
COM_PARSE_LAYOUT_ASSERT(com_parse_session_alignment,
                        __alignof__(com_parse_session_t) == 4);
COM_PARSE_LAYOUT_ASSERT(com_parse_session_token_offset,
                        offsetof(com_parse_session_t, token) == 0x000);
COM_PARSE_LAYOUT_ASSERT(com_parse_session_token_extent,
                        sizeof(((com_parse_session_t *)0)->token) == 0x400);
COM_PARSE_LAYOUT_ASSERT(com_parse_session_line_offset,
                        offsetof(com_parse_session_t, line) == 0x400);
COM_PARSE_LAYOUT_ASSERT(com_parse_session_unget_offset,
                        offsetof(com_parse_session_t, ungetToken) == 0x404);
COM_PARSE_LAYOUT_ASSERT(com_parse_session_delimiter_offset,
                        offsetof(com_parse_session_t, spaceDelimited) == 0x408);
COM_PARSE_LAYOUT_ASSERT(com_parse_session_csv_offset,
                        offsetof(com_parse_session_t, csv) == 0x40c);
COM_PARSE_LAYOUT_ASSERT(com_parse_session_negative_offset,
                        offsetof(com_parse_session_t, parseNegativeNumbers) == 0x410);
COM_PARSE_LAYOUT_ASSERT(com_parse_session_saved_line_offset,
                        offsetof(com_parse_session_t, savedLine) == 0x414);
COM_PARSE_LAYOUT_ASSERT(com_parse_session_saved_parse_offset,
                        offsetof(com_parse_session_t, savedParse) == 0x418);
COM_PARSE_LAYOUT_ASSERT(com_parse_session_name_offset,
                        offsetof(com_parse_session_t, name) == 0x41c);
COM_PARSE_LAYOUT_ASSERT(com_parse_session_name_extent,
                        sizeof(((com_parse_session_t *)0)->name) == 0x40);
COM_PARSE_LAYOUT_ASSERT(com_parse_session_size,
                        sizeof(com_parse_session_t) == 0x45c);
COM_PARSE_LAYOUT_ASSERT(com_parse_mark_alignment,
                        __alignof__(com_parse_mark_t) == 4);
COM_PARSE_LAYOUT_ASSERT(com_parse_mark_line_offset,
                        offsetof(com_parse_mark_t, line) == 0x00);
COM_PARSE_LAYOUT_ASSERT(com_parse_mark_parse_offset,
                        offsetof(com_parse_mark_t, parse) == 0x04);
COM_PARSE_LAYOUT_ASSERT(com_parse_mark_unget_offset,
                        offsetof(com_parse_mark_t, ungetToken) == 0x08);
COM_PARSE_LAYOUT_ASSERT(com_parse_mark_saved_line_offset,
                        offsetof(com_parse_mark_t, savedLine) == 0x0c);
COM_PARSE_LAYOUT_ASSERT(com_parse_mark_saved_parse_offset,
                        offsetof(com_parse_mark_t, savedParse) == 0x10);
COM_PARSE_LAYOUT_ASSERT(com_parse_mark_size,
                        sizeof(com_parse_mark_t) == 0x14);
#undef COM_PARSE_LAYOUT_ASSERT
#endif
