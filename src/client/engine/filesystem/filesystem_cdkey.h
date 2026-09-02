#ifndef CODUOMP_FILESYSTEM_CDKEY_H
#define CODUOMP_FILESYSTEM_CDKEY_H

void Com_ResetCDKeyToDefault(void);
#if defined(_WIN32)
void Com_ReadCDKey(void);
void Com_WriteCDKey(void);
#else
void Com_ReadCDKey(const char *gameDirectory);
void Com_AppendCDKey(const char *gameDirectory);
void Com_WriteCDKey(const char *gameDirectory, const char *key, const char *checksum);
#endif

#endif
