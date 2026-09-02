#include "server_namespace_provider.h"

/* NOT_FROM_ORIGINAL_SOURCE: behaviorally inert provider for stock behavior.
 * It never creates a namespace and returns the original filesystem root
 * unchanged. */
static void coduomp_stock_server_namespace_reset(void)
{
}

static qboolean coduomp_stock_server_namespace_activate(
    const netadr_t *address, const char *serverName,
    qboolean eligibleRemoteServer)
{
    (void)address;
    (void)serverName;
    (void)eligibleRemoteServer;
    return qfalse;
}

static qboolean coduomp_stock_server_namespace_deactivate(void)
{
    return qfalse;
}

static qboolean coduomp_stock_server_namespace_is_active(void)
{
    return qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: stock behavior has no cache to seed. */
static qboolean coduomp_stock_server_namespace_cache_referenced_paks(void)
{
    return qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: stock behavior exposes only the retail mod list. */
static int32_t coduomp_stock_server_namespace_append_cached_mods(
    char *listBuffer, int32_t bufferSize)
{
    (void)listBuffer;
    (void)bufferSize;
    return 0;
}

static const char *coduomp_stock_server_namespace_root(
    const char *ordinaryHomeRoot)
{
    return ordinaryHomeRoot;
}

static qboolean coduomp_stock_server_namespace_allows(
    const searchpath_t *searchpath)
{
    (void)searchpath;
    return qtrue;
}

static void coduomp_stock_server_namespace_promote_config(void)
{
}

static void coduomp_stock_server_namespace_clear_configs(void)
{
}

const coduomp_server_namespace_provider_t
    coduomp_server_namespace_provider = {
        coduomp_stock_server_namespace_reset,
        coduomp_stock_server_namespace_activate,
        coduomp_stock_server_namespace_deactivate,
        coduomp_stock_server_namespace_is_active,
        coduomp_stock_server_namespace_cache_referenced_paks,
        coduomp_stock_server_namespace_append_cached_mods,
        coduomp_stock_server_namespace_root,
        coduomp_stock_server_namespace_root,
        coduomp_stock_server_namespace_allows,
        coduomp_stock_server_namespace_promote_config,
        coduomp_stock_server_namespace_clear_configs,
    };
