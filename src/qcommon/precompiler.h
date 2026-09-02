#ifndef QCOMMON_PRECOMPILER_H
#define QCOMMON_PRECOMPILER_H

#include <stddef.h>
#include <stdint.h>

#include "precompiler_types.h"
#include "q_shared_types.h"

extern define_t *pc_globalDefines;
extern char pc_baseFolder[MAX_QPATH];

#ifdef __cplusplus
extern "C" {
#endif

/* Engine-owned diagnostics used by the shared parser implementation. */
void Com_Error(errorParm_t code, const char *format, ...);
void Com_Printf(const char *format, ...);

void SourceError(source_t *source, const char *format, ...);
void SourceWarning(source_t *source, const char *format, ...);
void PC_PushIndent(source_t *source, int32_t type, qboolean skip);
void PC_PopIndent(source_t *source, int32_t *type, qboolean *skip);
void PC_PushScript(source_t *source, script_t *script);
void PC_InitTokenHeap(void);
token_t *PC_CopyToken(const token_t *token);
void PC_FreeToken(token_t *token);
qboolean PC_ReadSourceToken(source_t *source,
                            token_t *token);
qboolean PC_UnreadSourceToken(source_t *source,
                              const token_t *token);
qboolean PC_ReadDefineParms(source_t *source, const define_t *define,
                            token_t **actualParms,
                            int32_t maxParms);
qboolean PC_StringizeTokens(token_t *tokens,
                            token_t *token);
qboolean PC_MergeTokens(token_t *token,
                        const token_t *next);
void PC_PrintDefineHashTable(define_t **defineHash);
uint32_t PC_NameHash(const char *name);
void PC_AddDefineToHash(define_t *define, define_t **defineHash);
define_t *PC_FindHashedDefine(define_t **defineHash,
                                 const char *name);
define_t *PC_FindDefine(define_t *defines, const char *name);
int32_t PC_FindDefineParm(const define_t *define, const char *name);
void PC_FreeDefine(define_t *define);
void PC_AddBuiltinDefines(source_t *source);
qboolean PC_ExpandBuiltinDefine(source_t *source,
                                const token_t *token,
                                const define_t *define,
                                token_t **firstToken,
                                token_t **lastToken);
qboolean PC_ExpandDefine(source_t *source,
                         const token_t *token,
                         const define_t *define,
                         token_t **firstToken,
                         token_t **lastToken);
qboolean PC_ExpandDefineIntoSource(source_t *source,
                                   const token_t *token,
                                   const define_t *define);
void PC_ConvertPath(char *path);
qboolean PC_Directive_include(source_t *source);
qboolean PC_ReadLine(source_t *source, token_t *token);
qboolean PC_WhiteSpaceBeforeToken(const token_t *token);
void PC_ClearTokenWhiteSpace(token_t *token);
qboolean PC_Directive_undef(source_t *source);
qboolean PC_Directive_define(source_t *source);
define_t *PC_DefineFromString(const char *string);
qboolean PC_AddDefineToSource(source_t *source, const char *string);
qboolean PC_AddGlobalDefine(const char *string);
qboolean PC_RemoveGlobalDefine(const char *name);
void PC_RemoveAllGlobalDefines(void);
define_t *PC_CopyDefine(source_t *source,
                           const define_t *define);
void PC_AddGlobalDefinesToSource(source_t *source);
qboolean PC_Directive_if_def(source_t *source, int32_t type);
qboolean PC_Directive_ifdef(source_t *source);
qboolean PC_Directive_ifndef(source_t *source);
qboolean PC_Directive_else(source_t *source);
qboolean PC_Directive_endif(source_t *source);
qboolean PC_Directive_elif(source_t *source);
qboolean PC_Directive_if(source_t *source);
qboolean PC_Directive_line(source_t *source);
qboolean PC_Directive_error(source_t *source);
qboolean PC_Directive_pragma(source_t *source);
void UnreadSignToken(source_t *source);
qboolean PC_Directive_eval(source_t *source);
qboolean PC_Directive_evalfloat(source_t *source);
qboolean PC_ReadDirective(source_t *source);
qboolean PC_DollarDirective_evalint(source_t *source);
qboolean PC_DollarDirective_evalfloat(source_t *source);
qboolean PC_ReadDollarDirective(source_t *source);
qboolean PC_ReadToken(source_t *source, token_t *token);
qboolean PC_ExpectTokenString(source_t *source, const char *string);
qboolean PC_ExpectTokenType(source_t *source, int32_t type,
                            int32_t subtype,
                            token_t *token);
qboolean PC_ExpectAnyToken(source_t *source,
                           token_t *token);
qboolean PC_CheckTokenString(source_t *source, const char *string);
qboolean PC_CheckTokenType(source_t *source, int32_t type,
                           int32_t subtype,
                           token_t *token);
qboolean PC_SkipUntilString(source_t *source, const char *string);
void PC_UnreadToken(source_t *source);
void PC_UnreadTokenValue(source_t *source,
                         const token_t *token);
void PC_SetIncludePath(source_t *source, const char *path);
void PC_SetPunctuations(source_t *source,
                        punctuation_t *punctuations);
source_t *LoadSourceFile(const char *filename);
source_t *LoadSourceMemory(const char *buffer, size_t length,
                              const char *name);
void FreeSource(source_t *source);
int32_t PC_LoadSourceHandle(const char *filename);
qboolean PC_FreeSourceHandle(int32_t handle);
qboolean PC_ReadTokenHandle(int32_t handle, pc_token_t *token);
qboolean PC_SourceFileAndLine(int32_t handle, char *filename,
                              int32_t *line);
void PC_SetBaseFolder(const char *path);
void PC_CheckOpenSourceHandles(void);
void SetScriptPunctuations(script_t *script,
                           punctuation_t *punctuations);
const char *PS_PunctuationStringForSubtype(script_t *script,
                                           int32_t subtype);
void ScriptError(script_t *script, const char *format, ...);
void ScriptWarning(script_t *script, const char *format, ...);
void PS_CreatePunctuationTable(script_t *script,
                               punctuation_t *punctuations);
qboolean PS_ReadWhiteSpace(script_t *script);
qboolean PS_ReadEscapeCharacter(script_t *script, char *out);
qboolean PS_ReadString(script_t *script, token_t *token,
                       int32_t quote);
qboolean PS_ReadName(script_t *script, token_t *token);
#if defined(WINDOWS_BEHAVIOR)
void NumberValue(const char *string, int32_t subtype,
                 int32_t *intValue, double *floatValue);
#elif defined(LINUX_BEHAVIOR)
void NumberValue(const char *string, int32_t subtype,
                 int32_t *intValue,
                 uint8_t floatValue[PC_TOKEN_FLOAT_VALUE_SIZE]);
#else
#error "precompiler.h requires WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif
qboolean PS_ReadNumber(script_t *script, token_t *token);
qboolean PS_ReadLiteral(script_t *script, token_t *token);
qboolean PS_ReadPunctuation(script_t *script,
                            token_t *token);
qboolean PS_ReadPrimitive(script_t *script,
                          token_t *token);
qboolean PS_ExpectTokenString(script_t *script, const char *string);
qboolean PS_ExpectTokenType(script_t *script, int32_t type,
                            int32_t subtype,
                            token_t *token);
qboolean PS_ReadTokenOrError(script_t *script,
                             token_t *token);
qboolean PS_CheckTokenString(script_t *script, const char *string);
qboolean PS_CheckTokenType(script_t *script, int32_t type,
                           int32_t subtype,
                           token_t *token);
qboolean PS_SkipUntilString(script_t *script, const char *string);
void PS_UnreadLastToken(script_t *script);
void PS_UnreadToken(script_t *script,
                    const token_t *token);
#if defined(WINDOWS_BEHAVIOR)
char PS_ReadWhitespaceChar(script_t *script);
#else
int32_t PS_ReadWhitespaceChar(script_t *script);
#endif
void StripDoubleQuotes(char *string);
void StripSingleQuotes(char *string);
#if defined(WINDOWS_BEHAVIOR)
double PS_ReadFloat(script_t *script);
#else
long double PS_ReadFloat(script_t *script);
#endif
int32_t PS_ReadInteger(script_t *script);
void SetScriptFlags(script_t *script, int32_t flags);
int32_t GetScriptFlags(const script_t *script);
void ResetScript(script_t *script);
qboolean EndOfScript(script_t *script);
int32_t PS_LinesCrossed(const script_t *script);
qboolean PS_FindStringInScript(script_t *script, const char *string);
int32_t PC_OperatorPriority(int32_t operatorSubtype);
qboolean PC_EvaluateTokens(source_t *source,
                           token_t *tokens,
                           int32_t *intValue, double *floatValue,
                           qboolean integerEval);
qboolean PC_Evaluate(source_t *source, int32_t *intValue,
                     double *floatValue, qboolean integerEval);
qboolean PC_DollarEvaluate(source_t *source, int32_t *intValue,
                           double *floatValue, qboolean integerEval);

qboolean PS_ReadToken(script_t *script, token_t *token);
void FreeScript(script_t *script);
script_t *LoadScriptFile(const char *filename);
script_t *LoadScriptMemory(const char *buffer, size_t length,
                              const char *name);
void PS_SetBaseFolder(const char *path);

#ifdef __cplusplus
}
#endif

#endif
