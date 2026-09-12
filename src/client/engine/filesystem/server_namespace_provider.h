#ifndef CODUOMP_SERVER_NAMESPACE_PROVIDER_H
#define CODUOMP_SERVER_NAMESPACE_PROVIDER_H

#include "server_namespace.h"

typedef struct coduomp_server_namespace_provider_s {
    void (*resetForStartup)(void);
    qboolean (*activate)(const char *serverName,
                         qboolean eligibleRemoteServer);
    qboolean (*activateCached)(const char *serverName);
    qboolean (*deactivate)(void);
    qboolean (*isActive)(void);
    qboolean (*cacheReferencedPaks)(void);
    int32_t (*appendCachedMods)(char *listBuffer, int32_t bufferSize);
    int32_t (*resolveCachedDemo)(
        const char *modName, const char *demoFileName,
        const char *serverName, char resolvedServer[MAX_QPATH],
        char resolvedMod[FS_PACK_NAME_SIZE]);
    int32_t (*listCachedDemos)(const char *modName);
    const char *(*stateRoot)(const char *ordinaryHomeRoot);
    const char *(*contentRoot)(const char *ordinaryHomeRoot);
    qboolean (*allowsSearchpath)(const searchpath_t *searchpath);
    void (*promoteCurrentConfig)(void);
    void (*clearConfigs)(void);
} coduomp_server_namespace_provider_t;

/* Exactly one build-selected provider defines this object. */
extern const coduomp_server_namespace_provider_t
    coduomp_server_namespace_provider;

#endif
