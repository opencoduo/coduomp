#ifndef QCOMMON_Q_PATH_H
#define QCOMMON_Q_PATH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

char *Com_SkipPath(char *path);
void Com_StripExtension(const char *input, char *output);
void Com_StripFilename(const char *input, char *output);
void Com_DefaultExtension(char *path, int32_t maximumSize,
                          const char *extension);

#ifdef __cplusplus
}
#endif

#endif
