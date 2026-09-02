#include "precompiler.h"

#include "compat/coduo_int32_bits.h"
#include "compat/crt/format_compat.h"
#include "precompiler_float.h"
#include "precompiler_services.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if defined(WINDOWS_BEHAVIOR)
void *GetMemory(size_t size);
void *GetClearedMemory(size_t size);
void FreeMemory(void *memory);
#elif defined(LINUX_BEHAVIOR)
void *Com_ZoneDebugAlloc(size_t size);
void *Com_ZoneDebugAllocClear(size_t size);
void Com_DebugFree(void *pointer);
#else
#error "precompiler_directives.c requires WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

/* Original global-define list head at CoDUOMP.exe 0x04927e80; the Linux
 * dedicated parser owns the same process-global list. */
define_t *pc_globalDefines;

/* NOT_FROM_ORIGINAL_SOURCE: isolate the proved token payload ABI.  Windows
 * stores binary64; Linux stores an x87 TBYTE in the same logical field. */
static void coduo_pc_store_synthetic_float_from_i32(token_t *token,
                                                     int32_t value)
{
#if defined(WINDOWS_BEHAVIOR)
    token->floatValue = (double)value;
#elif EMULATE_X87
    memset(token->floatValue, 0, sizeof(token->floatValue));
    coduo_pc_store_token_float80(token->floatValue, x87f_load_i32(value));
#else
    const long double extendedValue = (long double)value;
    const size_t copySize = sizeof(extendedValue) < PC_X87_EXTENDED_TBYTE_SIZE
                                ? sizeof(extendedValue)
                                : PC_X87_EXTENDED_TBYTE_SIZE;
    memset(token->floatValue, 0, sizeof(token->floatValue));
    memcpy(token->floatValue, &extendedValue, copySize);
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: binary64-input companion for $evalfloat. */
static void coduo_pc_store_synthetic_float_from_f64(token_t *token,
                                                     double value)
{
#if defined(WINDOWS_BEHAVIOR)
    token->floatValue = value;
#elif EMULATE_X87
    memset(token->floatValue, 0, sizeof(token->floatValue));
    coduo_pc_store_token_float80(token->floatValue, x87f_load_f64(value));
#else
    const long double extendedValue = (long double)value;
    const size_t copySize = sizeof(extendedValue) < PC_X87_EXTENDED_TBYTE_SIZE
                                ? sizeof(extendedValue)
                                : PC_X87_EXTENDED_TBYTE_SIZE;
    memset(token->floatValue, 0, sizeof(token->floatValue));
    memcpy(token->floatValue, &extendedValue, copySize);
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: both i386 compilers lower the $evalfloat integer
 * payload through a signed-qword x87 conversion and retain its low dword. */
static int32_t coduo_pc_f64_to_low_i32(double value)
{
    uint32_t lowBits = 0;
    int32_t result;

    if (value >= -0x1p63 && value < 0x1p63) {
        lowBits = (uint32_t)(uint64_t)(int64_t)value;
    }
    memcpy(&result, &lowBits, sizeof(result));
    return result;
}

/* Source: CoDUOMP.exe 0x00443db0..0x00443e14.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00443db0_00443e15.mcode.
 * Name: exact same-module Mac symbol PC_ReadLine. */
qboolean PC_ReadLine(source_t *source, token_t *token)
{
    int32_t continuationLines = 0;

    while (PC_ReadSourceToken(source, token) != qfalse) {
        if (token->linesCrossed > continuationLines) {
            PC_UnreadSourceToken(source, token);
            return qfalse;
        }

        continuationLines = 1;
        if (strcmp(token->string, "\\") != 0)
            return qtrue;
    }

    return qfalse;
}

/* Source: CoDUOMP.exe 0x00443e20..0x00443e33.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00443e20_00443e34.mcode.
 * Name: exact same-module Mac symbol PC_WhiteSpaceBeforeToken. */
qboolean PC_WhiteSpaceBeforeToken(const token_t *token)
{
    return token->whitespaceEnd - token->whitespaceStart > 0 ? qtrue
                                                              : qfalse;
}

/* Source: CoDUOMP.exe 0x00443e40..0x00443e54.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00443e40_00443e55.mcode.
 * Name: exact same-module Mac symbol PC_ClearTokenWhiteSpace. */
void PC_ClearTokenWhiteSpace(token_t *token)
{
    token->whitespaceStart = NULL;
    token->whitespaceEnd = NULL;
    token->linesCrossed = 0;
}

/* Source: CoDUOMP.exe 0x00443e60..0x00443fbe.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00443e60_00443fbf.mcode.
 * Name: exact same-module Mac symbol PC_Directive_undef. */
qboolean PC_Directive_undef(source_t *source)
{
    if (source->skip > 0)
        return qtrue;

    token_t token;
    if (PC_ReadLine(source, &token) == qfalse) {
        SourceError(source, "undef without name");
        return qfalse;
    }

    if (token.type != PC_TOKEN_TYPE_NAME) {
        PC_UnreadSourceToken(source, &token);
        SourceError(source, "expected name, found %s", token.string);
        return qfalse;
    }

    const uint32_t bucket = PC_NameHash(token.string);
    define_t *previous = NULL;
    define_t *define = source->defineHash[bucket];
    while (define != NULL) {
        if (strcmp(define->name, token.string) == 0) {
            if ((define->flags & PC_DEFINE_FLAG_BUILTIN) != 0) {
                SourceWarning(source, "can't undef %s", token.string);
            } else {
                if (previous == NULL)
                    source->defineHash[bucket] = define->hashNext;
                else
                    previous->hashNext = define->hashNext;
                PC_FreeDefine(define);
            }
            break;
        }

        previous = define;
        define = define->hashNext;
    }

    return qtrue;
}

/* Source: CoDUOMP.exe 0x00443fc0..0x004444d9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00443fc0_004444da.mcode.
 * Name: exact same-module Mac symbol PC_Directive_define. */
qboolean PC_Directive_define(source_t *source)
{
    if (source->skip > 0)
        return qtrue;

    token_t token;
    if (PC_ReadLine(source, &token) == qfalse) {
        SourceError(source, "#define without name");
        return qfalse;
    }

    if (token.type != PC_TOKEN_TYPE_NAME) {
        PC_UnreadSourceToken(source, &token);
        SourceError(source, "expected name after #define, found %s",
                    token.string);
        return qfalse;
    }

    define_t *existing =
        PC_FindHashedDefine(source->defineHash, token.string);
    if (existing != NULL) {
        if ((existing->flags & PC_DEFINE_FLAG_BUILTIN) != 0) {
            SourceError(source, "can't redefine %s", token.string);
            return qfalse;
        }

        SourceWarning(source, "redefinition of %s", token.string);
        PC_UnreadSourceToken(source, &token);
        if (PC_Directive_undef(source) == qfalse)
            return qfalse;
    }

    const size_t nameLength = strlen(token.string);
    define_t *define;
#if defined(WINDOWS_BEHAVIOR)
    define = GetMemory(sizeof(*define) + nameLength + 1);
#else
    define = Com_ZoneDebugAlloc(sizeof(*define) + nameLength + 1);
#endif
    memset(define, 0, sizeof(*define));
    define->name = define->nameStorage;
    strcpy(define->name, token.string);
    PC_AddDefineToHash(define, source->defineHash);

    if (PC_ReadLine(source, &token) == qfalse)
        return qtrue;

    const qboolean hasFormalParms =
        PC_WhiteSpaceBeforeToken(&token) == qfalse &&
        strcmp(token.string, "(") == 0 ? qtrue : qfalse;
    if (hasFormalParms != qfalse) {
        token_t *lastParm = NULL;
        if (PC_CheckTokenString(source, ")") == qfalse) {
            for (;;) {
                if (PC_ReadLine(source, &token) == qfalse) {
                    SourceError(source, "expected define parameter");
                    return qfalse;
                }
                if (token.type != PC_TOKEN_TYPE_NAME) {
                    SourceError(source, "invalid define parameter");
                    return qfalse;
                }
                if (PC_FindDefineParm(define, token.string) >= 0) {
                    SourceError(source, "two the same define parameters");
                    return qfalse;
                }

                token_t *parm = PC_CopyToken(&token);
                PC_ClearTokenWhiteSpace(parm);
                parm->next = NULL;
                if (lastParm == NULL)
                    define->parms = parm;
                else
                    lastParm->next = parm;
                ++define->numParms;

                if (PC_ReadLine(source, &token) == qfalse) {
                    SourceError(source,
                                "define parameters not terminated");
                    return qfalse;
                }
                if (strcmp(token.string, ")") == 0)
                    break;

                lastParm = parm;
                if (strcmp(token.string, ",") != 0) {
                    SourceError(source, "define not terminated");
                    return qfalse;
                }
            }
        }

        if (PC_ReadLine(source, &token) == qfalse)
            return qtrue;
    }

    token_t *lastToken = NULL;
    do {
        token_t *copy = PC_CopyToken(&token);
        if (copy->type == PC_TOKEN_TYPE_NAME &&
            strcmp(copy->string, define->name) == 0) {
            SourceError(source, "recursive define (removed recursion)");
        } else {
            PC_ClearTokenWhiteSpace(copy);
            copy->next = NULL;
            if (lastToken == NULL)
                define->tokens = copy;
            else
                lastToken->next = copy;
            lastToken = copy;
        }
    } while (PC_ReadLine(source, &token) != qfalse);

    if (lastToken == NULL ||
        (strcmp(define->tokens->string, "##") != 0 &&
         strcmp(lastToken->string, "##") != 0)) {
        return qtrue;
    }

    SourceError(source, "define with misplaced ##");
    return qfalse;
}

/* Source: CoDUOMP.exe 0x004444e0..0x00444682.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004444e0_00444683.mcode.
 * Name: exact same-module Mac symbol PC_DefineFromString. */
define_t *PC_DefineFromString(const char *string)
{
    PC_InitTokenHeap();

    script_t *script =
        LoadScriptMemory(string, strlen(string), "*extern");

    source_t source;
    memset(&source, 0, sizeof(source));
    strncpy(source.filename, "*extern", sizeof(source.filename));
    source.scriptStack = script;
#if defined(WINDOWS_BEHAVIOR)
    source.defineHash = GetClearedMemory(
        PC_DEFINE_HASH_BUCKET_COUNT * sizeof(*source.defineHash));
#else
    source.defineHash = Com_ZoneDebugAllocClear(
        PC_DEFINE_HASH_BUCKET_COUNT * sizeof(*source.defineHash));
#endif

    const qboolean parsed = PC_Directive_define(&source);

    while (source.tokens != NULL) {
        token_t *token = source.tokens;
        source.tokens = token->next;
        PC_FreeToken(token);
    }

    define_t *define = NULL;
    for (int32_t bucket = 0; bucket < PC_DEFINE_HASH_BUCKET_COUNT;
         ++bucket) {
        if (source.defineHash[bucket] != NULL) {
            define = source.defineHash[bucket];
            break;
        }
    }

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (parsed == qfalse) {
        for (int32_t bucket = 0; bucket < PC_DEFINE_HASH_BUCKET_COUNT;
             ++bucket) {
            while (source.defineHash[bucket] != NULL) {
                define_t *const failedDefine = source.defineHash[bucket];
                source.defineHash[bucket] = failedDefine->hashNext;
                PC_FreeDefine(failedDefine);
            }
        }
        define = NULL;
    }

#if defined(WINDOWS_BEHAVIOR)
    FreeMemory(source.defineHash);
#else
    Com_DebugFree(source.defineHash);
#endif
    FreeScript(script);

    if (parsed == qfalse)
        return NULL;

    return define;
}

/* Source: CoDUOMP.exe 0x00444690..0x004446c0.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00444690_004446c1.mcode.
 * Role name: the botlib PC_AddDefineToSource API. */
qboolean PC_AddDefineToSource(source_t *source, const char *string)
{
    define_t *define = PC_DefineFromString(string);
    if (define != NULL)
        PC_AddDefineToHash(define, source->defineHash);
    return define != NULL ? qtrue : qfalse;
}

/* Source: CoDUOMP.exe 0x004446d0..0x004446ed.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004446d0_004446ee.mcode.
 * Name: exact same-module Mac symbol PC_AddGlobalDefine. */
qboolean PC_AddGlobalDefine(const char *string)
{
    define_t *define = PC_DefineFromString(string);
    if (define != NULL) {
        define->next = pc_globalDefines;
        pc_globalDefines = define;
    }
    return define != NULL ? qtrue : qfalse;
}

/* Source: CoDUOMP.exe 0x004446f0..0x00444713.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_004446f0_00444714.mcode.
 * Role name: the botlib PC_RemoveGlobalDefine API. */
qboolean PC_RemoveGlobalDefine(const char *name)
{
    define_t **link = &pc_globalDefines;
    while (*link != NULL && strcmp((*link)->name, name) != 0)
        link = &(*link)->next;

    if (*link == NULL)
        return qfalse;

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    define_t *const define = *link;
    *link = define->next;
    PC_FreeDefine(define);
    return qtrue;
}

/* Source: CoDUOMP.exe 0x00444720..0x00444748.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00444720_00444749.mcode.
 * Role name: the botlib PC_RemoveAllGlobalDefines API. */
void PC_RemoveAllGlobalDefines(void)
{
    while (pc_globalDefines != NULL) {
        define_t *define = pc_globalDefines;
        pc_globalDefines = define->next;
        PC_FreeDefine(define);
    }
}

/* Source: CoDUOMP.exe 0x00444750..0x00444833.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00444750_00444834.mcode.
 * Name: exact same-module Mac symbol PC_CopyDefine. The source parameter is
 * present in the original API but is not read by the PE implementation. */
define_t *PC_CopyDefine(source_t *source,
                           const define_t *define)
{
    (void)source;

    const size_t nameLength = strlen(define->name);
    define_t *copy;
#if defined(WINDOWS_BEHAVIOR)
    copy = GetMemory(sizeof(*copy) + nameLength + 1);
#else
    copy = Com_ZoneDebugAlloc(sizeof(*copy) + nameLength + 1);
#endif

    copy->name = copy->nameStorage;
    strcpy(copy->name, define->name);
    copy->flags = define->flags;
    copy->builtin = define->builtin;
    copy->numParms = define->numParms;
    copy->next = NULL;
    copy->hashNext = NULL;
    copy->tokens = NULL;

    token_t *lastToken = NULL;
    for (const token_t *token = define->tokens;
         token != NULL; token = token->next) {
        token_t *tokenCopy = PC_CopyToken(token);
        tokenCopy->next = NULL;
        if (lastToken == NULL)
            copy->tokens = tokenCopy;
        else
            lastToken->next = tokenCopy;
        lastToken = tokenCopy;
    }

    copy->parms = NULL;
    token_t *lastParm = NULL;
    for (const token_t *parm = define->parms;
         parm != NULL; parm = parm->next) {
        token_t *parmCopy = PC_CopyToken(parm);
        parmCopy->next = NULL;
        if (lastParm == NULL)
            copy->parms = parmCopy;
        else
            lastParm->next = parmCopy;
        lastParm = parmCopy;
    }

    return copy;
}

/* Source: CoDUOMP.exe 0x00444840..0x004448be.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00444840_004448bf.mcode.
 * Name: exact same-module Mac symbol PC_AddGlobalDefinesToSource. */
void PC_AddGlobalDefinesToSource(source_t *source)
{
    for (define_t *define = pc_globalDefines; define != NULL;
         define = define->next) {
        define_t *copy = PC_CopyDefine(source, define);
        PC_AddDefineToHash(copy, source->defineHash);
    }
}

/* Source: CoDUOMP.exe 0x004448c0..0x0044499d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004448c0_0044499e.mcode.
 * Name: exact same-module Mac symbol PC_Directive_if_def. */
qboolean PC_Directive_if_def(source_t *source, int32_t type)
{
    token_t token;
    if (PC_ReadLine(source, &token) == qfalse) {
        SourceError(source, "#ifdef without name");
        return qfalse;
    }

    if (token.type != PC_TOKEN_TYPE_NAME) {
        PC_UnreadSourceToken(source, &token);
        SourceError(source, "expected name after #ifdef, found %s",
                    token.string);
        return qfalse;
    }

    const qboolean undefined =
        PC_FindHashedDefine(source->defineHash, token.string) == NULL
            ? qtrue
            : qfalse;
    const qboolean isIfdef =
        type == PC_INDENT_TYPE_IFDEF ? qtrue : qfalse;
    PC_PushIndent(source, type,
                  undefined == isIfdef ? qtrue : qfalse);
    return qtrue;
}

/* Source: CoDUOMP.exe 0x004449a0..0x004449b0.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_004449a0_004449b1.mcode.
 * Name: exact same-module Mac symbol PC_Directive_ifdef. */
qboolean PC_Directive_ifdef(source_t *source)
{
    return PC_Directive_if_def(source, PC_INDENT_TYPE_IFDEF);
}

/* Source: CoDUOMP.exe 0x004449c0..0x004449d0.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_004449c0_004449d1.mcode.
 * Name: exact same-module Mac symbol PC_Directive_ifndef. */
qboolean PC_Directive_ifndef(source_t *source)
{
    return PC_Directive_if_def(source, PC_INDENT_TYPE_IFNDEF);
}

/* Source: CoDUOMP.exe 0x004449e0..0x00444a4d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004449e0_00444a4e.mcode.
 * Name: exact same-module Mac symbol PC_Directive_else. */
qboolean PC_Directive_else(source_t *source)
{
    int32_t type;
    qboolean skip;
    PC_PopIndent(source, &type, &skip);

    if (type == PC_INDENT_TYPE_NONE) {
        SourceError(source, "misplaced #else");
        return qfalse;
    }
    if (type == PC_INDENT_TYPE_ELSE) {
        SourceError(source, "#else after #else");
        return qfalse;
    }

    PC_PushIndent(source, PC_INDENT_TYPE_ELSE,
                  skip == qfalse ? qtrue : qfalse);
    return qtrue;
}

/* Source: CoDUOMP.exe 0x00444a50..0x00444a8d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00444a50_00444a8e.mcode.
 * Name: exact same-module Mac symbol PC_Directive_endif. */
qboolean PC_Directive_endif(source_t *source)
{
    int32_t type;
    qboolean skip;
    PC_PopIndent(source, &type, &skip);
    if (type == PC_INDENT_TYPE_NONE) {
        SourceError(source, "misplaced #endif");
        return qfalse;
    }
    return qtrue;
}

/* Source: CoDUOMP.exe 0x00445a60..0x00445ace.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00445a60_00445acf.mcode.
 * Name: exact same-module Mac symbol PC_Directive_elif. */
qboolean PC_Directive_elif(source_t *source)
{
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    int32_t type;
    qboolean skip;
    PC_PopIndent(source, &type, &skip);

    if (type == PC_INDENT_TYPE_NONE || type == PC_INDENT_TYPE_ELSE) {
        SourceError(source, "misplaced #elif");
        return qfalse;
    }

    int32_t value;
    if (PC_Evaluate(source, &value, NULL, qtrue) == qfalse)
        return qfalse;

    PC_PushIndent(source, PC_INDENT_TYPE_ELIF,
                  value == 0 ? qtrue : qfalse);
    return qtrue;
}

/* Source: CoDUOMP.exe 0x00445ad0..0x00445b0c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00445ad0_00445b0d.mcode.
 * Name: exact same-module Mac symbol PC_Directive_if. */
qboolean PC_Directive_if(source_t *source)
{
    int32_t value;
    if (PC_Evaluate(source, &value, NULL, qtrue) == qfalse)
        return qfalse;

    PC_PushIndent(source, PC_INDENT_TYPE_IF,
                  value == 0 ? qtrue : qfalse);
    return qtrue;
}

/* Source: CoDUOMP.exe 0x00445b10..0x00445b24.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00445b10_00445b25.mcode.
 * Name: exact same-module Mac symbol PC_Directive_line. */
qboolean PC_Directive_line(source_t *source)
{
    SourceError(source, "#line directive not supported");
    return qfalse;
}

/* Source: CoDUOMP.exe 0x00445b30..0x00445b80.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00445b30_00445b81.mcode.
 * Name: exact same-module Mac symbol PC_Directive_error. */
qboolean PC_Directive_error(source_t *source)
{
    token_t token;
    token.string[0] = '\0';
    (void)PC_ReadSourceToken(source, &token);
    SourceError(source, "#error directive: %s", token.string);
    return qfalse;
}

/* Source: CoDUOMP.exe 0x00445b90..0x00445be3.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00445b90_00445be4.mcode.
 * Name: exact same-module Mac symbol PC_Directive_pragma. */
qboolean PC_Directive_pragma(source_t *source)
{
    token_t token;
    SourceWarning(source, "#pragma directive not supported");
    while (PC_ReadLine(source, &token) != qfalse) {
    }
    return qtrue;
}

/* Source: CoDUOMP.exe 0x00445bf0..0x00445c83.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00445bf0_00445c84.mcode.
 * Name: exact same-module Mac symbol UnreadSignToken. */
void UnreadSignToken(source_t *source)
{
    token_t token;
    token.string[0] = '-';
    token.string[1] = '\0';
    token.type = PC_TOKEN_TYPE_PUNCTUATION;
    token.subtype = PC_OPERATOR_SUBTRACT;
    token.whitespaceStart = source->scriptStack->scriptCursor;
    token.whitespaceEnd = source->scriptStack->scriptCursor;
    token.line = source->scriptStack->line;
    token.linesCrossed = 0;
    (void)PC_UnreadSourceToken(source, &token);
}

/* Source: CoDUOMP.exe 0x00445c90..0x00445d7f.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00445c90_00445d80.mcode.
 * Name: exact same-module Mac symbol PC_Directive_eval. */
qboolean PC_Directive_eval(source_t *source)
{
    int32_t value;
    if (PC_Evaluate(source, &value, NULL, qtrue) == qfalse)
        return qfalse;

    token_t token;
    const int32_t magnitude = value == INT32_MIN
                                  ? INT32_MIN
                                  : (value < 0 ? -value : value);
#if defined(WINDOWS_BEHAVIOR)
    coduo_crt_snprintf(token.string, sizeof(token.string), "%d",
                         magnitude);
#else
    sprintf(token.string, "%d", magnitude);
#endif
    token.type = PC_TOKEN_TYPE_NUMBER;
    token.subtype = PC_TOKEN_SUBTYPE_DECIMAL_INTEGER_LONG;
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    token.intValue = value;
    coduo_pc_store_synthetic_float_from_i32(&token, value);
    token.whitespaceStart = source->scriptStack->scriptCursor;
    token.whitespaceEnd = source->scriptStack->scriptCursor;
    token.line = source->scriptStack->line;
    token.linesCrossed = 0;
    (void)PC_UnreadSourceToken(source, &token);

    if (value < 0)
        UnreadSignToken(source);
    return qtrue;
}

/* Source: CoDUOMP.exe 0x00445d80..0x00445e7a.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00445d80_00445e7b.mcode.
 * Name: exact same-module Mac symbol PC_Directive_evalfloat. */
qboolean PC_Directive_evalfloat(source_t *source)
{
    double value;
    if (PC_Evaluate(source, NULL, &value, qfalse) == qfalse)
        return qfalse;

    token_t token;
#if defined(WINDOWS_BEHAVIOR)
    coduo_crt_snprintf(token.string, sizeof(token.string), "%1.2f",
                         fabs(value));
#else
    sprintf(token.string, "%1.2f", fabs(value));
#endif
    token.type = PC_TOKEN_TYPE_NUMBER;
    token.subtype = PC_TOKEN_SUBTYPE_DECIMAL_FLOAT_LONG;
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    token.intValue = coduo_pc_f64_to_low_i32(value);
    coduo_pc_store_synthetic_float_from_f64(&token, value);
    token.whitespaceStart = source->scriptStack->scriptCursor;
    token.whitespaceEnd = source->scriptStack->scriptCursor;
    token.line = source->scriptStack->line;
    token.linesCrossed = 0;
    (void)PC_UnreadSourceToken(source, &token);

    if (value < 0.0)
        UnreadSignToken(source);
    return qtrue;
}

typedef qboolean (*pc_directive_handler_t)(source_t *source);

typedef struct pc_directive_entry_s {
    const char *directiveName;
    pc_directive_handler_t handler;
} pc_directive_entry_t;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(pc_directive_entry_t) == 0x04,
               "original parser directive-entry alignment changed");
_Static_assert(offsetof(pc_directive_entry_t, directiveName) == 0x00,
               "original parser directive-entry name moved");
_Static_assert(sizeof(((pc_directive_entry_t *)0)->directiveName) == 0x04,
               "original parser directive-entry name extent changed");
_Static_assert(offsetof(pc_directive_entry_t, handler) == 0x04,
               "original parser directive-entry handler moved");
_Static_assert(sizeof(((pc_directive_entry_t *)0)->handler) == 0x04,
               "original parser directive-entry handler extent changed");
_Static_assert(sizeof(pc_directive_entry_t) == 0x08,
               "original parser directive-entry size changed");
#endif

/* Source: CoDUOMP.exe initialized table 0x005c5298..0x005c5310.
 * PE_RELOCATION_VALUES_VERIFIED: all fourteen name/handler pairs and the
 * null terminator match the original pointer values and function entries. */
static const pc_directive_entry_t pc_directives[] = {
    {"if", PC_Directive_if},
    {"ifdef", PC_Directive_ifdef},
    {"ifndef", PC_Directive_ifndef},
    {"elif", PC_Directive_elif},
    {"else", PC_Directive_else},
    {"endif", PC_Directive_endif},
    {"include", PC_Directive_include},
    {"define", PC_Directive_define},
    {"undef", PC_Directive_undef},
    {"line", PC_Directive_line},
    {"error", PC_Directive_error},
    {"pragma", PC_Directive_pragma},
    {"eval", PC_Directive_eval},
    {"evalfloat", PC_Directive_evalfloat},
    {NULL, NULL}
};

/* Source: CoDUOMP.exe 0x00445e80..0x00445fc0.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00445e80_00445fc1.mcode.
 * Name: exact same-module Mac symbol PC_ReadDirective. */
qboolean PC_ReadDirective(source_t *source)
{
    token_t token;
    if (PC_ReadSourceToken(source, &token) == qfalse) {
        SourceError(source, "found # without name");
        return qfalse;
    }

    if (token.linesCrossed > 0) {
        (void)PC_UnreadSourceToken(source, &token);
        SourceError(source, "found # at end of line");
        return qfalse;
    }

    if (token.type == PC_TOKEN_TYPE_NAME) {
        for (const pc_directive_entry_t *directive = pc_directives;
             directive->directiveName != NULL; ++directive) {
            if (strcmp(directive->directiveName, token.string) == 0)
                return directive->handler(source);
        }
    }

    SourceError(source, "unknown precompiler directive %s", token.string);
    return qfalse;
}

/* Source: CoDUOMP.exe 0x00445fd0..0x004460d2.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00445fd0_004460d3.mcode.
 * Name: exact same-module Mac symbol PC_DollarDirective_evalint. */
qboolean PC_DollarDirective_evalint(source_t *source)
{
    int32_t value;
    if (PC_DollarEvaluate(source, &value, NULL, qtrue) == qfalse)
        return qfalse;

    token_t token;
    const int32_t magnitude = value == INT32_MIN
                                  ? INT32_MIN
                                  : (value < 0 ? -value : value);
#if defined(WINDOWS_BEHAVIOR)
    coduo_crt_snprintf(token.string, sizeof(token.string), "%d",
                         magnitude);
#else
    sprintf(token.string, "%d", magnitude);
#endif
    token.type = PC_TOKEN_TYPE_NUMBER;
    token.subtype = PC_TOKEN_SUBTYPE_DECIMAL_INTEGER_LONG;
    token.intValue = value;
    coduo_pc_store_synthetic_float_from_i32(&token, value);
    token.whitespaceStart = source->scriptStack->scriptCursor;
    token.whitespaceEnd = source->scriptStack->scriptCursor;
    token.line = source->scriptStack->line;
    token.linesCrossed = 0;
    (void)PC_UnreadSourceToken(source, &token);

    if (value < 0)
        UnreadSignToken(source);
    return qtrue;
}

/* Source: CoDUOMP.exe 0x004460e0..0x004461f4.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_004460e0_004461f5.mcode.
 * Name: exact same-module Mac symbol PC_DollarDirective_evalfloat. */
qboolean PC_DollarDirective_evalfloat(source_t *source)
{
    double value;
    if (PC_DollarEvaluate(source, NULL, &value, qfalse) == qfalse)
        return qfalse;

    token_t token;
#if defined(WINDOWS_BEHAVIOR)
    coduo_crt_snprintf(token.string, sizeof(token.string), "%1.2f",
                         fabs(value));
#else
    sprintf(token.string, "%1.2f", fabs(value));
#endif
    token.type = PC_TOKEN_TYPE_NUMBER;
    token.subtype = PC_TOKEN_SUBTYPE_DECIMAL_FLOAT_LONG;
    token.intValue = coduo_pc_f64_to_low_i32(value);
    coduo_pc_store_synthetic_float_from_f64(&token, value);
    token.whitespaceStart = source->scriptStack->scriptCursor;
    token.whitespaceEnd = source->scriptStack->scriptCursor;
    token.line = source->scriptStack->line;
    token.linesCrossed = 0;
    (void)PC_UnreadSourceToken(source, &token);

    if (value < 0.0)
        UnreadSignToken(source);
    return qtrue;
}

/* Source: CoDUOMP.exe initialized table 0x005c5338..0x005c5350.
 * PE_RELOCATION_VALUES_VERIFIED: both name/handler pairs and the null
 * terminator match the original pointer values and function entries. */
static const pc_directive_entry_t pc_dollarDirectives[] = {
    {"evalint", PC_DollarDirective_evalint},
    {"evalfloat", PC_DollarDirective_evalfloat},
    {NULL, NULL}
};

/* Source: CoDUOMP.exe 0x00446200..0x0044635c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00446200_0044635d.mcode.
 * Name: exact same-module Mac symbol PC_ReadDollarDirective. */
qboolean PC_ReadDollarDirective(source_t *source)
{
    token_t token;
    if (PC_ReadSourceToken(source, &token) == qfalse) {
        SourceError(source, "found $ without name");
        return qfalse;
    }

    if (token.linesCrossed > 0) {
        (void)PC_UnreadSourceToken(source, &token);
        SourceError(source, "found $ at end of line");
        return qfalse;
    }

    if (token.type == PC_TOKEN_TYPE_NAME) {
        for (const pc_directive_entry_t *directive = pc_dollarDirectives;
             directive->directiveName != NULL; ++directive) {
            if (strcmp(directive->directiveName, token.string) == 0)
                return directive->handler(source);
        }
    }

    (void)PC_UnreadSourceToken(source, &token);
    SourceError(source, "unknown precompiler directive %s", token.string);
    return qfalse;
}
