#include "precompiler.h"

#include "compat/crt/format_compat.h"
#include "precompiler_float.h"
#include "precompiler_services.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(WINDOWS_BEHAVIOR)
void *GetClearedMemory(size_t size);
void FreeMemory(void *memory);
void Log_Write(const char *format, ...);
#elif defined(LINUX_BEHAVIOR)
void *Com_ZoneDebugAlloc(size_t size);
void Com_DebugFree(void *pointer);
void Com_LogPrintf(const char *format, ...);
#else
#error "precompiler_define.c requires WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

enum {
    PC_DEFINE_HASH_INDEX_BIAS = 119,
    PC_DEFINE_HASH_SHIFT_A = 10,
    PC_DEFINE_HASH_BUCKET_MASK = PC_DEFINE_HASH_BUCKET_COUNT - 1,
    PC_DEFINE_PARAMETER_NOT_FOUND = -1,
    PC_CTIME_DATE_MONTH_DAY_OFFSET = 4,
    PC_CTIME_DATE_MONTH_DAY_LENGTH = 7,
    PC_CTIME_DATE_YEAR_OFFSET = 20,
    PC_CTIME_DATE_YEAR_LENGTH = 4,
    PC_CTIME_TIME_OFFSET = 11,
    PC_CTIME_TIME_LENGTH = 8
};

/* NOT_FROM_ORIGINAL_SOURCE: common ownership cleanup for copied macro-token
 * lists on malformed expansion paths. */
static void coduo_pc_free_token_list(token_t **tokens)
{
    while (*tokens != NULL) {
        token_t *const token = *tokens;
        *tokens = token->next;
        PC_FreeToken(token);
    }
}

/* Source: CoDUOMP.exe 0x00442cd0..0x00443020.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00442cd0_00443021.mcode.
 * Name: exact same-module Mac symbol PC_ReadDefineParms. */
qboolean PC_ReadDefineParms(source_t *source, const define_t *define,
                            token_t **actualParms,
                            int32_t maxParms)
{
    token_t token;
    if (PC_ReadSourceToken(source, &token) == qfalse) {
        SourceError(source, "define %s missing parms", define->name);
        return qfalse;
    }

    if (maxParms < define->numParms) {
        SourceError(source, "define with more than %d parameters", maxParms);
        return qfalse;
    }

    for (int32_t parmIndex = 0; parmIndex < define->numParms; ++parmIndex)
        actualParms[parmIndex] = NULL;

    if (strcmp(token.string, "(") != 0) {
        PC_UnreadSourceToken(source, &token);
        SourceError(source, "define %s missing parms", define->name);
        return qfalse;
    }

    qboolean done = qfalse;
    int32_t parmIndex = 0;
    int32_t parentheses = 0;

    while (done == qfalse) {
        if (parmIndex >= maxParms) {
            SourceError(source, "define %s with too many parms",
                        define->name);
            goto failure;
        }
        if (parmIndex >= define->numParms) {
            SourceWarning(source, "define %s has too many parms",
                          define->name);
            goto failure;
        }

        actualParms[parmIndex] = NULL;
        qboolean firstToken = qtrue;
        token_t *lastToken = NULL;

        while (done == qfalse) {
            if (PC_ReadSourceToken(source, &token) == qfalse) {
                SourceError(source, "define %s incomplete", define->name);
                goto failure;
            }

            if (strcmp(token.string, ",") == 0 && parentheses < 1) {
                if (firstToken != qfalse)
                    SourceWarning(source, "too many comma's");
                break;
            }

            firstToken = qfalse;
            if (strcmp(token.string, "(") == 0) {
                ++parentheses;
                continue;
            }
            if (strcmp(token.string, ")") == 0 && --parentheses < 1) {
                if (actualParms[define->numParms - 1] == NULL)
                    SourceWarning(source, "too few define parms");
                done = qtrue;
                break;
            }

            if (parmIndex < define->numParms) {
                token_t *copy = PC_CopyToken(&token);
                copy->next = NULL;
                if (lastToken == NULL)
                    actualParms[parmIndex] = copy;
                else
                    lastToken->next = copy;
                lastToken = copy;
            }
        }

        ++parmIndex;
    }

    return qtrue;

failure:
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    for (int32_t cleanupIndex = 0; cleanupIndex < define->numParms;
         ++cleanupIndex) {
        coduo_pc_free_token_list(&actualParms[cleanupIndex]);
    }
    return qfalse;
}

/* Source: CoDUOMP.exe 0x00443030..0x004430d7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00443030_004430d8.mcode.
 * Name: exact same-module Mac symbol PC_StringizeTokens. */
qboolean PC_StringizeTokens(token_t *tokens,
                            token_t *token)
{
    token->type = PC_TOKEN_TYPE_STRING;
    token->whitespaceStart = NULL;
    token->whitespaceEnd = NULL;
    token->string[0] = '\0';
    strcat(token->string, "\"");

    /* NOT_FROM_ORIGINAL_SOURCE: every token, both quotes, and the final NUL
     * must fit before publishing the stringized expansion. */
    for (token_t *scan = tokens; scan != NULL;
         scan = scan->next) {
        const size_t destinationLength = strlen(token->string);
        const size_t sourceLength = strlen(scan->string);
        if (sourceLength > MAX_TOKEN_CHARS - 1u - destinationLength) {
            return qfalse;
        }
        strcat(token->string, scan->string);
    }

    if (strlen(token->string) >= MAX_TOKEN_CHARS - 1u) {
        return qfalse;
    }
    strcat(token->string, "\"");
    return qtrue;
}

/* Source: CoDUOMP.exe 0x004430e0..0x00443174.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004430e0_00443175.mcode.
 * Name: exact same-module Mac symbol PC_MergeTokens. */
qboolean PC_MergeTokens(token_t *token,
                        const token_t *next)
{
    if (token->type == PC_TOKEN_TYPE_NAME &&
        (next->type == PC_TOKEN_TYPE_NAME ||
         next->type == PC_TOKEN_TYPE_NUMBER)) {
        const size_t tokenLength = strlen(token->string);
        const size_t nextLength = strlen(next->string);
        /* NOT_FROM_ORIGINAL_SOURCE: both complete tokens and the final NUL
         * must fit before changing the destination token. */
        if (tokenLength >= MAX_TOKEN_CHARS ||
            nextLength >= MAX_TOKEN_CHARS ||
            nextLength > MAX_TOKEN_CHARS - 1u - tokenLength) {
            return qfalse;
        }
        strcat(token->string, next->string);
        return qtrue;
    }

    if (next->type == PC_TOKEN_TYPE_STRING &&
        token->type == PC_TOKEN_TYPE_STRING) {
        const size_t tokenLength = strlen(token->string);
        const size_t nextLength = strlen(next->string);
        /* NOT_FROM_ORIGINAL_SOURCE: both quoted tokens must form one complete
         * terminated token before either input is changed. */
        if (tokenLength == 0 || nextLength == 0 ||
            tokenLength >= MAX_TOKEN_CHARS ||
            nextLength >= MAX_TOKEN_CHARS ||
            nextLength - 1u > MAX_TOKEN_CHARS - tokenLength) {
            return qfalse;
        }
        token->string[tokenLength - 1u] = '\0';
        strcat(token->string, next->string + 1);
        return qtrue;
    }

    return qfalse;
}

/* Source: CoDUOMP.exe 0x00443180..0x004431cf.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00443180_004431d0.mcode.
 * Role name: the botlib PC_PrintDefineHashTable diagnostic. */
void PC_PrintDefineHashTable(define_t **defineHash)
{
    for (int32_t bucket = 0; bucket < PC_DEFINE_HASH_BUCKET_COUNT; ++bucket) {
#if defined(WINDOWS_BEHAVIOR)
        Log_Write("%4d:", bucket);
#else
        Com_LogPrintf("%4d:", bucket);
#endif
        for (define_t *define = defineHash[bucket]; define != NULL;
             define = define->hashNext) {
#if defined(WINDOWS_BEHAVIOR)
            Log_Write(" %s", define->name);
#else
            Com_LogPrintf(" %s", define->name);
#endif
        }
#if defined(WINDOWS_BEHAVIOR)
        Log_Write("\n");
#else
        Com_LogPrintf("\n");
#endif
    }
}

/* Source: CoDUOMP.exe 0x004431d0..0x00443207.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004431d0_00443208.mcode.
 * Name: exact same-module Mac symbol PC_NameHash. */
uint32_t PC_NameHash(const char *name)
{
    uint32_t hash = 0;
    for (int32_t index = 0; name[index] != '\0'; ++index) {
        const int32_t character = (int8_t)name[index];
        hash += (uint32_t)((index + PC_DEFINE_HASH_INDEX_BIAS) * character);
    }

    const int32_t signedHash = (int32_t)hash;
    int32_t foldedHash = signedHash >> PC_DEFINE_HASH_SHIFT_A;
    foldedHash ^= signedHash;
    foldedHash >>= PC_DEFINE_HASH_SHIFT_A;
    foldedHash ^= signedHash;
    return (uint32_t)foldedHash & PC_DEFINE_HASH_BUCKET_MASK;
}

/* Source: CoDUOMP.exe 0x00443210..0x00443220.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00443210_00443221.mcode.
 * Name: exact same-module Mac symbol PC_AddDefineToHash. */
void PC_AddDefineToHash(define_t *define, define_t **defineHash)
{
    const uint32_t bucket = PC_NameHash(define->name);
    define->hashNext = defineHash[bucket];
    defineHash[bucket] = define;
}

/* Source: CoDUOMP.exe 0x00443230..0x00443299.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00443230_0044329a.mcode.
 * Name: exact same-module Mac symbol PC_FindHashedDefine. */
define_t *PC_FindHashedDefine(define_t **defineHash,
                                 const char *name)
{
    const uint32_t bucket = PC_NameHash(name);
    for (define_t *define = defineHash[bucket]; define != NULL;
         define = define->hashNext) {
        if (strcmp(define->name, name) == 0)
            return define;
    }
    return NULL;
}

/* Source: CoDUOMP.exe 0x004432a0..0x004432f9.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_004432a0_004432fa.mcode.
 * Role name: the botlib PC_FindDefine list lookup. */
define_t *PC_FindDefine(define_t *defines, const char *name)
{
    for (define_t *define = defines; define != NULL;
         define = define->next) {
        if (strcmp(define->name, name) == 0)
            return define;
    }
    return NULL;
}

/* Source: CoDUOMP.exe 0x00443300..0x0044335a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00443300_0044335b.mcode.
 * Name: exact same-module Mac symbol PC_FindDefineParm. */
int32_t PC_FindDefineParm(const define_t *define, const char *name)
{
    int32_t parmIndex = 0;
    for (token_t *parm = define->parms; parm != NULL;
         parm = parm->next, ++parmIndex) {
        if (strcmp(parm->string, name) == 0)
            return parmIndex;
    }
    return PC_DEFINE_PARAMETER_NOT_FOUND;
}

/* Source: CoDUOMP.exe 0x00443360..0x004433f6.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00443360_004433f7.mcode.
 * Name: exact same-module Mac symbol PC_FreeDefine. */
void PC_FreeDefine(define_t *define)
{
    for (token_t *parm = define->parms; parm != NULL;) {
        token_t *next = parm->next;
        PC_FreeToken(parm);
        parm = next;
    }
    for (token_t *token = define->tokens; token != NULL;) {
        token_t *next = token->next;
        PC_FreeToken(token);
        token = next;
    }
#if defined(WINDOWS_BEHAVIOR)
    FreeMemory(define);
#else
    Com_DebugFree(define);
#endif
}

/* Source: CoDUOMP.exe 0x00443400..0x00443545.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00443400_00443546.mcode.
 * Role name: install the four standard botlib builtin definitions. */
void PC_AddBuiltinDefines(source_t *source)
{
    /* The original constructs these five records in its stack frame. Host
     * pointers widen naturally; the Win32 record stride remains eight bytes. */
    const struct pc_builtin_define_s {
        const char *defineName;
        enum pc_builtin_define_e builtinKind;
    } builtins[] = {
        {"__LINE__", PC_BUILTIN_LINE},
        {"__FILE__", PC_BUILTIN_FILE},
        {"__DATE__", PC_BUILTIN_DATE},
        {"__TIME__", PC_BUILTIN_TIME},
        {NULL, PC_BUILTIN_NONE}
    };

    for (int32_t index = 0; builtins[index].defineName != NULL; ++index) {
        const size_t nameLength = strlen(builtins[index].defineName);
        define_t *define;
#if defined(WINDOWS_BEHAVIOR)
        define = GetClearedMemory(sizeof(*define) + nameLength + 1);
#else
        define = Com_ZoneDebugAlloc(sizeof(*define) + nameLength + 1);
        memset(define, 0, sizeof(*define));
#endif

        define->name = define->nameStorage;
        strcpy(define->name, builtins[index].defineName);
        define->flags |= PC_DEFINE_FLAG_BUILTIN;
        define->builtin = builtins[index].builtinKind;
        PC_AddDefineToHash(define, source->defineHash);
    }
}

/* Source: CoDUOMP.exe 0x00443550..0x0044373e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00443550_0044373f.mcode.
 * Name: exact same-module Mac symbol PC_ExpandBuiltinDefine. */
qboolean PC_ExpandBuiltinDefine(source_t *source,
                                const token_t *token,
                                const define_t *define,
                                token_t **firstToken,
                                token_t **lastToken)
{
    token_t *expanded = PC_CopyToken(token);

    switch (define->builtin) {
    case PC_BUILTIN_LINE:
#if defined(WINDOWS_BEHAVIOR)
        coduo_crt_snprintf(expanded->string, sizeof(expanded->string),
                             "%d", token->line);
#else
        sprintf(expanded->string, "%d", token->line);
#endif
        expanded->intValue = token->line;
#if defined(WINDOWS_BEHAVIOR)
        expanded->floatValue = (double)token->line;
#elif EMULATE_X87
        coduo_pc_store_token_float80(
            expanded->floatValue, x87f_load_i32(token->line));
#else
        {
            const long double lineValue = (long double)token->line;
            memcpy(expanded->floatValue, &lineValue,
                   PC_X87_EXTENDED_TBYTE_SIZE);
        }
#endif
        expanded->type = PC_TOKEN_TYPE_NUMBER;
        expanded->subtype = PC_TOKEN_SUBTYPE_DECIMAL_INTEGER;
        *firstToken = expanded;
        *lastToken = expanded;
        break;

    case PC_BUILTIN_FILE:
        strcpy(expanded->string, source->scriptStack->filename);
        expanded->type = PC_TOKEN_TYPE_NAME;
        expanded->subtype = (int32_t)strlen(expanded->string);
        *firstToken = expanded;
        *lastToken = expanded;
        break;

    case PC_BUILTIN_DATE: {
        time_t rawTime = time(NULL);
        const char *timeString = ctime(&rawTime);

        strcpy(expanded->string, "\"");
        strncat(expanded->string,
                timeString + PC_CTIME_DATE_MONTH_DAY_OFFSET,
                PC_CTIME_DATE_MONTH_DAY_LENGTH);
        strncat(expanded->string + PC_CTIME_DATE_MONTH_DAY_LENGTH,
                timeString + PC_CTIME_DATE_YEAR_OFFSET,
                PC_CTIME_DATE_YEAR_LENGTH);
        strcat(expanded->string, "\"");
        /* NOT_FROM_ORIGINAL_SOURCE: the C runtime retains ownership of the
         * static ctime result; there is intentionally no release here. */
        expanded->type = PC_TOKEN_TYPE_NAME;
        expanded->subtype = (int32_t)strlen(expanded->string);
        *firstToken = expanded;
        *lastToken = expanded;
        break;
    }

    case PC_BUILTIN_TIME: {
        time_t rawTime = time(NULL);
        const char *timeString = ctime(&rawTime);

        strcpy(expanded->string, "\"");
        strncat(expanded->string, timeString + PC_CTIME_TIME_OFFSET,
                PC_CTIME_TIME_LENGTH);
        strcat(expanded->string, "\"");
        /* NOT_FROM_ORIGINAL_SOURCE: the C runtime retains ownership of the
         * static ctime result; there is intentionally no release here. */
        expanded->type = PC_TOKEN_TYPE_NAME;
        expanded->subtype = (int32_t)strlen(expanded->string);
        *firstToken = expanded;
        *lastToken = expanded;
        break;
    }

    default:
        *firstToken = NULL;
        *lastToken = NULL;
        break;
    }

    return qtrue;
}

/* Source: CoDUOMP.exe 0x00443750..0x00443a9a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00443750_00443a9b.mcode.
 * Name: exact same-module Mac symbol PC_ExpandDefine. */
qboolean PC_ExpandDefine(source_t *source,
                         const token_t *token,
                         const define_t *define,
                         token_t **firstToken,
                         token_t **lastToken)
{
    if (define->builtin != 0) {
        return PC_ExpandBuiltinDefine(source, token, define, firstToken,
                                      lastToken);
    }

    token_t *actualParms[PC_DEFINE_MAX_PARMS];
    if (define->numParms != 0 &&
        PC_ReadDefineParms(source, define, actualParms,
                           PC_DEFINE_MAX_PARMS) == qfalse) {
        return qfalse;
    }

    token_t *first = NULL;
    token_t *last = NULL;

    for (const token_t *scan = define->tokens; scan != NULL;
         scan = scan->next) {
        int32_t parmIndex = PC_DEFINE_PARAMETER_NOT_FOUND;
        if (scan->type == PC_TOKEN_TYPE_NAME)
            parmIndex = PC_FindDefineParm(define, scan->string);

        token_t *copy;
        if (parmIndex < 0) {
            if (strcmp(scan->string, "#") == 0) {
                const token_t *next = scan->next;
                if (next != NULL)
                    parmIndex = PC_FindDefineParm(define, next->string);
                if (parmIndex < 0) {
                    SourceWarning(
                        source,
                        "stringizing operator without define parameter");
                    continue;
                }

                scan = next;
                token_t stringized;
                if (PC_StringizeTokens(actualParms[parmIndex],
                                       &stringized) == qfalse) {
                    SourceError(source, "can't stringize tokens");
                    goto failure;
                }
                copy = PC_CopyToken(&stringized);
            } else {
                copy = PC_CopyToken(scan);
            }

            copy->next = NULL;
            if (last == NULL)
                first = copy;
            else
                last->next = copy;
            last = copy;
            continue;
        }

        for (token_t *parmToken = actualParms[parmIndex];
             parmToken != NULL; parmToken = parmToken->next) {
            copy = PC_CopyToken(parmToken);
            copy->next = NULL;
            if (last == NULL)
                first = copy;
            else
                last->next = copy;
            last = copy;
        }
    }

    token_t *mergeToken = first;
    while (mergeToken != NULL) {
        token_t *hashHash = mergeToken->next;
        if (hashHash == NULL ||
            hashHash->string[0] != '#' ||
            hashHash->string[1] != '#') {
            mergeToken = hashHash;
            continue;
        }

        token_t *right = hashHash->next;
        if (right == NULL) {
            mergeToken = hashHash;
            continue;
        }

        if (PC_MergeTokens(mergeToken, right) == qfalse) {
            SourceError(source, "can't merge %s with %s",
                        mergeToken->string, right->string);
            goto failure;
        }

        PC_FreeToken(hashHash);
        mergeToken->next = right->next;
        if (right == last)
            last = mergeToken;
        PC_FreeToken(right);

        /* 0x00443999 returns directly to the test at 0x00443900, retaining
         * the merged left token so a following ## is processed as part of
         * the same paste chain. */
    }

    *firstToken = first;
    *lastToken = last;

    for (int32_t parmIndex = 0; parmIndex < define->numParms; ++parmIndex) {
        coduo_pc_free_token_list(&actualParms[parmIndex]);
    }

    return qtrue;

failure:
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    coduo_pc_free_token_list(&first);
    for (int32_t parmIndex = 0; parmIndex < define->numParms; ++parmIndex)
        coduo_pc_free_token_list(&actualParms[parmIndex]);
    return qfalse;
}

/* Source: CoDUOMP.exe 0x00443aa0..0x00443aef.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00443aa0_00443af0.mcode.
 * Name: exact same-module Mac symbol PC_ExpandDefineIntoSource. */
qboolean PC_ExpandDefineIntoSource(source_t *source,
                                   const token_t *token,
                                   const define_t *define)
{
    token_t *firstToken;
    token_t *lastToken;
    if (PC_ExpandDefine(source, token, define, &firstToken, &lastToken) ==
        qfalse) {
        return qfalse;
    }
    if (firstToken == NULL || lastToken == NULL)
        return qfalse;

    lastToken->next = source->tokens;
    source->tokens = firstToken;
    return qtrue;
}

/* Source: CoDUOMP.exe 0x00443af0..0x00443b4e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00443af0_00443b4f.mcode.
 * Name: exact same-module Mac symbol PC_ConvertPath. The Windows body writes
 * backslashes; maintained POSIX builds select their native slash after the
 * same duplicate-separator collapse. */
void PC_ConvertPath(char *path)
{
    char *cursor = path;
    while (*cursor != '\0') {
        if ((*cursor == '\\' || *cursor == '/') &&
            (cursor[1] == '\\' || cursor[1] == '/')) {
            memmove(cursor, cursor + 1, strlen(cursor));
        } else {
            ++cursor;
        }
    }

    for (cursor = path; *cursor != '\0'; ++cursor) {
        if (*cursor == '\\' || *cursor == '/') {
#if defined(_WIN32)
            *cursor = '\\';
#else
            *cursor = '/';
#endif
        }
    }
}

/* Source: CoDUOMP.exe 0x00443b50..0x00443dad.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00443b50_00443dae.mcode.
 * Name: exact same-module Mac symbol PC_Directive_include. */
qboolean PC_Directive_include(source_t *source)
{
    if (source->skip > 0)
        return qtrue;

    token_t token;
    if (PC_ReadSourceToken(source, &token) == qfalse ||
        token.linesCrossed > 0) {
        SourceError(source, "#include without file name");
        return qfalse;
    }

    script_t *script = NULL;
    char includeName[PC_SOURCE_INCLUDE_PATH_CAPACITY];

    if (token.type == PC_TOKEN_TYPE_STRING) {
        StripDoubleQuotes(token.string);
        PC_ConvertPath(token.string);
        script = LoadScriptFile(token.string);
        if (script == NULL) {
            const size_t baseLength = strlen(source->includePath);
            const size_t nameLength = strlen(token.string);
            /* NOT_FROM_ORIGINAL_SOURCE: the complete base path, include name,
             * and NUL must fit; never substitute a truncated include path. */
            if (baseLength >= sizeof(includeName) ||
                nameLength > sizeof(includeName) - 1u - baseLength) {
                SourceError(source, "include path is too long");
                return qfalse;
            }
            memcpy(includeName, source->includePath, baseLength);
            memcpy(includeName + baseLength, token.string, nameLength + 1u);
            script = LoadScriptFile(includeName);
        }
    } else {
        if (token.type != PC_TOKEN_TYPE_PUNCTUATION ||
            token.string[0] != '<') {
            SourceError(source, "#include without file name");
            return qfalse;
        }

        size_t includeLength = strlen(source->includePath);
        if (includeLength >= sizeof(includeName)) {
            SourceError(source, "include path is too long");
            return qfalse;
        }
        memcpy(includeName, source->includePath, includeLength + 1u);
        while (PC_ReadSourceToken(source, &token) != qfalse) {
            if (token.linesCrossed > 0) {
                PC_UnreadSourceToken(source, &token);
                break;
            }
            if (token.type == PC_TOKEN_TYPE_PUNCTUATION &&
                token.string[0] == '>')
                break;

            const size_t tokenLength = strlen(token.string);
            /* NOT_FROM_ORIGINAL_SOURCE: every include token and the final NUL
             * must remain inside the complete pathname capacity. */
            if (tokenLength > sizeof(includeName) - 1u - includeLength) {
                SourceError(source, "include path is too long");
                return qfalse;
            }
            memcpy(includeName + includeLength, token.string, tokenLength + 1u);
            includeLength += tokenLength;
        }

        if (token.string[0] != '>')
            SourceWarning(source, "#include missing trailing >");
        if (includeName[0] == '\0') {
            SourceError(source,
                        "#include without file name between < >");
            return qfalse;
        }

        PC_ConvertPath(includeName);
        script = LoadScriptFile(includeName);
    }

    if (script == NULL) {
        SourceError(source, "file %s not found", includeName);
        return qfalse;
    }

    PC_PushScript(source, script);
    return qtrue;
}
