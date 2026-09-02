#ifndef QCOMMON_COM_PARSE_H
#define QCOMMON_COM_PARSE_H

#include "q_shared_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The original UI/shared parsers read this state directly in their small
 * String_Parse, Float_Parse, and Int_Parse wrappers. Each linked binary owns
 * its own instance through src/qcommon/com_parse.c. */
extern com_parse_session_t *com_parseSession;
extern com_parse_session_t com_parseSessions[MAX_PARSE_SESSIONS];
extern int32_t com_numParseSessions;
extern char *com_lastTokenStart;
extern char *com_tokenStart;

char *Com_ParseExt(char **data, qboolean allowLineBreaks);
char *Com_Parse(char **data);
char *Com_ParseOnLine(char **data);

void Com_BeginParseSession(const char *name);
void Com_EndParseSession(void);
void Com_ResetParseSessions(void);
void Com_SetSpaceDelimited(qboolean enabled);
void Com_SetCSV(qboolean enabled);
void Com_SetParseNegativeNumbers(qboolean enabled);
int32_t Com_GetCurrentParseLine(void);
void Com_ScriptError(const char *format, ...);
void Com_ScriptWarning(const char *format, ...);
void Com_UngetToken(void);
void Com_ParseSetMark(char **data, com_parse_mark_t *mark);
void Com_ParseReturnToMark(char **data, const com_parse_mark_t *mark);
char *SkipWhitespace(char *data, qboolean *hasNewLines);
int32_t Com_Compress(char *data);
char *Com_GetLastTokenPos(void);
void Com_MatchToken(char **data, const char *match, qboolean warning);
qboolean Com_SkipBracedSection(char **data, int32_t depth);
void Com_SkipRestOfLine(char **data);
char *Com_ParseRestOfLine(char **data);

#if defined(WINDOWS_BEHAVIOR)
long double Com_ParseFloat(char **data);
#elif defined(LINUX_BEHAVIOR)
float Com_ParseFloat(char **data);
#else
#error "com_parse.h requires WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

int32_t Com_ParseInt(char **data);
void Com_Parse1DMatrix(char **data, int32_t x, float *matrix);
void Com_Parse2DMatrix(char **data, int32_t y, int32_t x, float *matrix);
void Com_Parse3DMatrix(char **data, int32_t z, int32_t y, int32_t x, float *matrix);

#ifdef __cplusplus
}
#endif

#endif
