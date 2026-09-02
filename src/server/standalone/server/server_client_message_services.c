#include "server_client_message_services.h"

#include "../core_runtime/core_runtime_private.h"
#include "../networking/netchan_private.h"
#include "qcommon/q_string.h"
#include "server/engine/server_authorize.h"
#include "server/engine/server_commands.h"
#include "sv_globals_private.h"

void Com_Printf(const char *format, ...);
void SV_SendServerCommand(client_t *client, qboolean reliable,
                          const char *format, ...);

/* NOT_FROM_ORIGINAL_SOURCE: retain the dedicated engine's untranslated drop
 * presentation around the common client-drop lifecycle. */
void server_compat_emit_drop_messages(client_t *client, const char *name,
                                      const char *dropReason)
{
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (Q_stricmp(dropReason, "EXE_DISCONNECTED") != 0) {
        SV_SendServerCommand(NULL, qfalse, "e \"\x15%s^7 \x14%s\"",
                             name, dropReason);
    }

    Com_Printf("%i:%s %s\n", (int32_t)(client - svs.clients), name,
               dropReason);
    /* NOT_FROM_ORIGINAL_SOURCE: keep the drop reason as data through the
     * single server-command formatting pass. */
    SV_SendServerCommand(client, qtrue, "w \"%s\"", dropReason);
}

/* NOT_FROM_ORIGINAL_SOURCE: preserve the existing dedicated build option at
 * the target boundary rather than forking the common parser. */
void server_compat_authorize_resent_gamestate(client_t *client)
{
#if !CODUO_DISABLE_SERVER_AUTH
    if (net_lanauthorize->integer != 0 ||
        Sys_IsLANAddress(client->netchan.remoteAddress) == qfalse) {
        SV_AuthorizeRequest(client->netchan.remoteAddress, client->challenge);
    }
#else
    (void)client;
#endif
}
