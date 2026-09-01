#ifndef FILESYSTEM_PATH_SECURITY_H
#define FILESYSTEM_PATH_SECURITY_H

#include "qcommon/q_shared_types.h"

/* NOT_FROM_ORIGINAL_SOURCE: shared client/server host-path confinement used
 * at filesystem and network boundaries. */
qboolean coduo_compat_path_is_safe_relative(const char *path);

/* NOT_FROM_ORIGINAL_SOURCE: ordinary network package transfers must retain
 * the package filename type expected by the filesystem restart path. */
qboolean coduo_compat_path_is_pk3(const char *path);

#endif
