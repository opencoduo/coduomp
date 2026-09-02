#ifndef CODUOMP_MACOS_APP_BUNDLE_H
#define CODUOMP_MACOS_APP_BUNDLE_H

#include <stddef.h>

/* NOT_FROM_ORIGINAL_SOURCE: macOS application-bundle launch boundary. */
int coduomp_macos_prepare_launch(int argc, char *const argv[]);
const char *coduomp_macos_default_cd_path(void);
const char *coduomp_macos_bundle_resources_path(void);
int coduomp_macos_framework_module_path(const char *moduleName, char *path, size_t pathSize);

#endif
