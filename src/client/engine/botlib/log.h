#ifndef CODUOMP_BOTLIB_LOG_H
#define CODUOMP_BOTLIB_LOG_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

void Log_Open(const char *filename);
void Log_Create(const char *filename);
void Log_Close(void);
void Log_Shutdown(void);
void Log_Write(const char *format, ...);
FILE *Log_FilePointer(void);
void Log_Flush(void);

#ifdef __cplusplus
}
#endif

#endif
