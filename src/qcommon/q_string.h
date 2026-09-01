#ifndef QCOMMON_Q_STRING_H
#define QCOMMON_Q_STRING_H

#include "q_shared_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

qboolean Q_isprint(int32_t character);
qboolean Q_islower(int32_t character);
qboolean Q_isupper(int32_t character);
qboolean Q_isalpha(int32_t character);
qboolean Q_isnumeric(int32_t character);
qboolean Q_isalphanumeric(int32_t character);
qboolean Q_isforfilename(int32_t character);

char *Q_strrchr(const char *string, int32_t character);
void Q_strncpyz(char *destination, const char *source, int32_t size);
int32_t Q_stricmpn(const char *left, const char *right, int32_t count);
int32_t Q_strncmp(const char *left, const char *right, int32_t count);
int32_t Q_stricmp(const char *left, const char *right);
char *Q_strlwr(char *string);
char *Q_strupr(char *string);
void Q_strcat(char *destination, int32_t size, const char *source);
int32_t Q_GetDecimalDelimiter(language_t language);
void Q_LocalizedFloatToString(float value, char *buffer,
                              uint32_t bufferSize, int32_t precision,
                              language_t language);

const char *Com_StringContains(const char *haystack, const char *needle,
                               qboolean caseSensitive);
qboolean Com_Filter(const char *filter, const char *name,
                    qboolean caseSensitive);
qboolean Com_FilterPath(const char *filter, const char *name,
                        qboolean caseSensitive);

int32_t Q_DrawStrlen(const char *string);
char *Q_CleanStr(char *string);
uint8_t Q_CleanCharacter(uint8_t character);
int32_t Q_strncasecmp(const char *left, const char *right, int32_t count);
int32_t Q_strcasecmp(const char *left, const char *right);

char *va(const char *format, ...);
float *tv(float x, float y, float z);

/* Original zero-initialized subsystem storage.  The module lifecycle adapters
 * expose it only to reproduce a fresh DLL load in retained native modules. */
extern char q_vaStringBuffer[MAX_VA_STRING];
extern char q_vaTempBuffer[MAX_VA_STRING];
extern int32_t q_vaStringOffset;
extern vec3_t q_tempVectors[8];
extern int32_t q_tempVectorIndex;

#ifdef __cplusplus
}
#endif

#endif
