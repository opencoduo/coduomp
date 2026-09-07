#ifndef CODUOMP_FILESYSTEM_CONFIG_H
#define CODUOMP_FILESYSTEM_CONFIG_H

#include "qcommon/config_store.h"

qboolean coduomp_fs_config_destination(const char *root, const char *game, const char *name, char path[CODUOMP_CONFIG_PATH], char error[256]);
int coduomp_fs_config_read(const char *name, char **data, size_t *size, char source[CODUOMP_CONFIG_PATH], char error[256]);
void coduomp_fs_config_read_origin(const char *root, const char *path, const char *entry);
void coduomp_fs_config_read_error(void);
void coduomp_fs_config_changed(void);

#endif
