#include "precompiler.h"

#include "precompiler_services.h"
#include "q_string.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#if defined(WINDOWS_BEHAVIOR)
void *GetMemory(size_t size);
void FreeMemory(void *memory);
#elif defined(LINUX_BEHAVIOR)
void *Com_ZoneDebugAlloc(size_t size);
void Com_DebugFree(void *pointer);
#else
#error "precompiler_core.c requires WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

/* Original Windows live copied-token count at 0x04927e84. */
int32_t pc_tokenNodeCount;

/* Source: CoDUOMP.exe 0x00442870..0x004428d2.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00442870_004428d3.mcode.
 * Name: exact same-module Mac symbol SourceError. */
void SourceError(source_t *source, const char *format, ...)
{
    char message[PC_DIAGNOSTIC_CAPACITY];
    va_list args;

    va_start(args, format);
    /* NOT_FROM_ORIGINAL_SOURCE: bound the formatted source diagnostic to its
     * fixed destination. */
    (void)vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    Com_Printf("^1Error: file %s, line %d: %s\n", source->scriptStack->filename, source->scriptStack->line, message);
}

/* Source: CoDUOMP.exe 0x004428e0..0x00442942.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004428e0_00442943.mcode.
 * Name: exact same-module Mac symbol SourceWarning. */
void SourceWarning(source_t *source, const char *format, ...)
{
    char message[PC_DIAGNOSTIC_CAPACITY];
    va_list args;

    va_start(args, format);
    /* NOT_FROM_ORIGINAL_SOURCE: bound the formatted source diagnostic to its
     * fixed destination. */
    (void)vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    Com_Printf("file %s, line %d: %s\n", source->scriptStack->filename, source->scriptStack->line, message);
}

/* Source: CoDUOMP.exe 0x00442950..0x004429b7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00442950_004429b8.mcode.
 * Name: exact same-module Mac symbol PC_PushIndent. */
void PC_PushIndent(source_t *source, int32_t type, qboolean skip)
{
    indent_t *indent;
#if defined(WINDOWS_BEHAVIOR)
    indent = GetMemory(sizeof(*indent));
#else
    indent = Com_ZoneDebugAlloc(sizeof(*indent));
#endif
    indent->type = type;
    indent->skip = skip != qfalse ? qtrue : qfalse;
    indent->script = source->scriptStack;
    source->skip += indent->skip;
    indent->next = source->indentStack;
    source->indentStack = indent;
}

/* Source: CoDUOMP.exe 0x004429c0..0x00442a28.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004429c0_00442a29.mcode.
 * Name: exact same-module Mac symbol PC_PopIndent. */
void PC_PopIndent(source_t *source, int32_t *type, qboolean *skip)
{
    *type = PC_INDENT_TYPE_NONE;
    *skip = qfalse;

    indent_t *indent = source->indentStack;
    if (indent == NULL || indent->script != source->scriptStack)
        return;

    *type = indent->type;
    *skip = indent->skip;
    source->indentStack = indent->next;
    source->skip -= indent->skip;
#if defined(WINDOWS_BEHAVIOR)
    FreeMemory(indent);
#else
    Com_DebugFree(indent);
#endif
}

/* Source: CoDUOMP.exe 0x00442a30..0x00442a85.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00442a30_00442a86.mcode.
 * Name: exact same-module Mac symbol PC_PushScript. */
void PC_PushScript(source_t *source, script_t *script)
{
    for (script_t *scan = source->scriptStack; scan != NULL; scan = scan->next) {
        if (Q_stricmp(script->filename, scan->filename) == 0) {
            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            SourceError(source, "%s recursively included", script->filename);
            FreeScript(script);
            return;
        }
    }

    script->next = source->scriptStack;
    source->scriptStack = script;
}

/* Source: CoDUOMP.exe 0x00442a90. Name: exact same-module Mac symbol
 * PC_InitTokenHeap. This build has no separate token-pool initialization. */
void PC_InitTokenHeap(void)
{
}

/* Source: CoDUOMP.exe 0x00442aa0..0x00442b0f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00442aa0_00442b10.mcode.
 * Name: exact same-module Mac symbol PC_CopyToken. */
token_t *PC_CopyToken(const token_t *token)
{
    token_t *copy;
#if defined(WINDOWS_BEHAVIOR)
    copy = GetMemory(sizeof(*copy));
#else
    copy = Com_ZoneDebugAlloc(sizeof(*copy));
#endif
    if (copy == NULL) {
        Com_Error(ERR_FATAL, "EXE_ERR_OUT_OF_MEMORY");
        return NULL;
    }

    memcpy(copy, token, sizeof(*copy));
    copy->next = NULL;
    ++pc_tokenNodeCount;
    return copy;
}

/* Source: CoDUOMP.exe 0x00442b10..0x00442b2d.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00442b10_00442b2e.mcode.
 * Name: exact same-module Mac symbol PC_FreeToken. */
void PC_FreeToken(token_t *token)
{
#if defined(WINDOWS_BEHAVIOR)
    FreeMemory(token);
#else
    Com_DebugFree(token);
#endif
    --pc_tokenNodeCount;
}

/* Source: CoDUOMP.exe 0x00442b30..0x00442c91.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00442b30_00442c92.mcode.
 * Name: exact same-module Mac symbol PC_ReadSourceToken. */
qboolean PC_ReadSourceToken(source_t *source, token_t *token)
{
    if (source->tokens != NULL) {
        token_t *cachedToken = source->tokens;
        memcpy(token, cachedToken, sizeof(*token));
        source->tokens = cachedToken->next;
        PC_FreeToken(cachedToken);
        return qtrue;
    }

    while (PS_ReadToken(source->scriptStack, token) == qfalse) {
        if (source->scriptStack->scriptCursor >= source->scriptStack->endCursor) {
            while (source->indentStack != NULL && source->indentStack->script == source->scriptStack) {
                int32_t indentType;
                qboolean indentSkip;
                SourceWarning(source, "missing #endif");
                PC_PopIndent(source, &indentType, &indentSkip);
            }
        }

        if (source->scriptStack->next == NULL)
            return qfalse;

        script_t *script = source->scriptStack;
        source->scriptStack = script->next;
        FreeScript(script);
    }

    return qtrue;
}

/* Source: CoDUOMP.exe 0x00442ca0..0x00442cc0.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00442ca0_00442cc1.mcode.
 * Name: exact same-module Mac symbol PC_UnreadSourceToken. */
qboolean PC_UnreadSourceToken(source_t *source, const token_t *token)
{
    token_t *copy = PC_CopyToken(token);
    copy->next = source->tokens;
    source->tokens = copy;
    return qtrue;
}

/* Source: CoDUOMP.exe 0x00446360..0x0044652f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00446360_00446530.mcode.
 * Name: exact same-module Mac symbol PC_ReadToken. */
qboolean PC_ReadToken(source_t *source, token_t *token)
{
    for (;;) {
        if (PC_ReadSourceToken(source, token) == qfalse)
            return qfalse;

        if (token->type == PC_TOKEN_TYPE_PUNCTUATION && token->string[0] == '#') {
            if (PC_ReadDirective(source) == qfalse)
                return qfalse;
            continue;
        }

        if (token->type == PC_TOKEN_TYPE_PUNCTUATION && token->string[0] == '$') {
            if (PC_ReadDollarDirective(source) == qfalse)
                return qfalse;
            continue;
        }

        if (token->type == PC_TOKEN_TYPE_STRING) {
            token_t next;
            if (PC_ReadToken(source, &next) != qfalse) {
                if (next.type == PC_TOKEN_TYPE_STRING) {
                    const size_t firstLength = strlen(token->string);
                    token->string[firstLength - 1] = '\0';
                    const size_t combinedLength = strlen(token->string) + strlen(next.string + 1) + 1;
                    if (combinedLength >= MAX_TOKEN_CHARS) {
                        SourceError(source, "string longer than MAX_TOKEN %d\n", MAX_TOKEN_CHARS);
                        return qfalse;
                    }
                    strcat(token->string, next.string + 1);
                } else {
                    (void)PC_UnreadSourceToken(source, &next);
                }
            }
        }

        if (source->skip != 0)
            continue;

        if (token->type == PC_TOKEN_TYPE_NAME) {
            define_t *define = PC_FindHashedDefine(source->defineHash, token->string);
            if (define != NULL) {
                if (PC_ExpandDefineIntoSource(source, token, define) == qfalse)
                    return qfalse;
                continue;
            }
        }

        memcpy(&source->token, token, sizeof(source->token));
        return qtrue;
    }
}

/* Source: CoDUOMP.exe 0x00446530..0x00446605.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00446530_00446606.mcode.
 * Name: exact same-module Mac symbol PC_ExpectTokenString. */
qboolean PC_ExpectTokenString(source_t *source, const char *string)
{
    token_t token;
    if (PC_ReadToken(source, &token) == qfalse) {
        SourceError(source, "couldn't find expected %s", string);
        return qfalse;
    }

    if (strcmp(token.string, string) == 0)
        return qtrue;

    SourceError(source, "expected %s, found %s", string, token.string);
    return qfalse;
}

/* Source: CoDUOMP.exe 0x00446610..0x004468ef.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00446610_004468f0.mcode.
 * Name: exact same-module Mac symbol PC_ExpectTokenType. */
qboolean PC_ExpectTokenType(source_t *source, int32_t type, int32_t subtype, token_t *token)
{
    char expected[MAX_TOKEN_CHARS];

    if (PC_ReadToken(source, token) == qfalse) {
        SourceError(source, "couldn't read expected token");
        return qfalse;
    }

    if (token->type != type) {
        expected[0] = '\0';
        if (type == PC_TOKEN_TYPE_STRING)
            strcpy(expected, "string");
        else if (type == PC_TOKEN_TYPE_LITERAL)
            strcpy(expected, "literal");
        else if (type == PC_TOKEN_TYPE_NUMBER)
            strcpy(expected, "number");
        else if (type == PC_TOKEN_TYPE_NAME)
            strcpy(expected, "name");
        else if (type == PC_TOKEN_TYPE_PUNCTUATION)
            strcpy(expected, "punctuation");

        SourceError(source, "expected a %s, found %s", expected, token->string);
        return qfalse;
    }

    if (token->type == PC_TOKEN_TYPE_NUMBER && (token->subtype & subtype) != subtype) {
        /* Every numeric subtype accepted by this parser includes one radix
         * bit. The PE begins by copying that radix name; it does not clear
         * this buffer on the impossible no-radix path. */
        if ((subtype & PC_TOKEN_SUBTYPE_DECIMAL) != 0)
            strcpy(expected, "decimal");
        if ((subtype & PC_TOKEN_SUBTYPE_HEX) != 0)
            strcpy(expected, "hex");
        if ((subtype & PC_TOKEN_SUBTYPE_OCTAL) != 0)
            strcpy(expected, "octal");
        if ((subtype & PC_TOKEN_SUBTYPE_BINARY) != 0)
            strcpy(expected, "binary");
        if ((subtype & PC_TOKEN_SUBTYPE_LONG) != 0)
            strcat(expected, " long");
        if ((subtype & PC_TOKEN_SUBTYPE_UNSIGNED) != 0)
            strcat(expected, " unsigned");
        if ((subtype & PC_TOKEN_SUBTYPE_FLOAT) != 0)
            strcat(expected, " float");
        if ((subtype & PC_TOKEN_SUBTYPE_INTEGER) != 0)
            strcat(expected, " integer");

        SourceError(source, "expected %s, found %s", expected, token->string);
        return qfalse;
    }

    if (token->type == PC_TOKEN_TYPE_PUNCTUATION && token->subtype != subtype) {
        SourceError(source, "found %s", token->string);
        return qfalse;
    }

    return qtrue;
}

/* Source: CoDUOMP.exe 0x004468f0..0x00446914.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_004468f0_00446915.mcode.
 * Name: exact same-module Mac symbol PC_ExpectAnyToken. */
qboolean PC_ExpectAnyToken(source_t *source, token_t *token)
{
    if (PC_ReadToken(source, token) == qfalse) {
        SourceError(source, "couldn't read expected token");
        return qfalse;
    }
    return qtrue;
}

/* Source: CoDUOMP.exe 0x00446920..0x004469cd.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00446920_004469ce.mcode.
 * Name: exact same-module Mac symbol PC_CheckTokenString. */
qboolean PC_CheckTokenString(source_t *source, const char *string)
{
    token_t token;
    if (PC_ReadToken(source, &token) == qfalse)
        return qfalse;

    if (strcmp(token.string, string) == 0)
        return qtrue;

    (void)PC_UnreadSourceToken(source, &token);
    return qfalse;
}

/* Source: CoDUOMP.exe 0x004469d0..0x00446a78.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_004469d0_00446a79.mcode.
 * Name: role-proven PC_CheckTokenType. */
qboolean PC_CheckTokenType(source_t *source, int32_t type, int32_t subtype, token_t *token)
{
    token_t readToken;
    if (PC_ReadToken(source, &readToken) == qfalse)
        return qfalse;

    if (readToken.type == type && (readToken.subtype & subtype) == subtype) {
        memcpy(token, &readToken, sizeof(*token));
        return qtrue;
    }

    (void)PC_UnreadSourceToken(source, &readToken);
    return qfalse;
}

/* Source: CoDUOMP.exe 0x00446a80..0x00446b2b.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00446a80_00446b2c.mcode.
 * Name: role-proven PC_SkipUntilString. */
qboolean PC_SkipUntilString(source_t *source, const char *string)
{
    token_t token;
    do {
        if (PC_ReadToken(source, &token) == qfalse)
            return qfalse;
    } while (strcmp(token.string, string) != 0);
    return qtrue;
}

/* Source: CoDUOMP.exe 0x00446b30..0x00446b51.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00446b30_00446b52.mcode.
 * Name: exact same-module Mac symbol PC_UnreadToken. */
void PC_UnreadToken(source_t *source)
{
    (void)PC_UnreadSourceToken(source, &source->token);
}

/* Source: CoDUOMP.exe 0x00446b60..0x00446b7b.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00446b60_00446b7c.mcode.
 * Name: role-proven PC_UnreadTokenValue; unlike PC_UnreadToken, this form
 * pushes the caller-provided token rather than source->token. */
void PC_UnreadTokenValue(source_t *source, const token_t *token)
{
    (void)PC_UnreadSourceToken(source, token);
}

/* Source: CoDUOMP.exe 0x00446b80..0x00446be3.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00446b80_00446be4.mcode.
 * Name: role-proven PC_SetIncludePath. */
void PC_SetIncludePath(source_t *source, const char *path)
{
    const size_t length = strlen(path);
    const qboolean needsSeparator = length != 0 && path[length - 1] != '\\' && path[length - 1] != '/';

    /* NOT_FROM_ORIGINAL_SOURCE: preserve an empty base path; otherwise require
     * the complete path, optional separator, and final NUL to fit. */
    if (length >= sizeof(source->includePath) || (needsSeparator != qfalse && length >= sizeof(source->includePath) - 1u)) {
        source->includePath[0] = '\0';
        SourceError(source, "include path is too long");
        return;
    }

    memcpy(source->includePath, path, length + 1u);
    if (needsSeparator != qfalse) {
        source->includePath[length] = '/';
        source->includePath[length + 1u] = '\0';
    }
}

/* Source: CoDUOMP.exe 0x00446bf0..0x00446bf6.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00446bf0_00446bf7.mcode.
 * Name: role-proven PC_SetPunctuations. */
void PC_SetPunctuations(source_t *source, punctuation_t *punctuations)
{
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    source->punctuations = punctuations;
}
