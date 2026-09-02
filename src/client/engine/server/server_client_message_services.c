#include "server_client_message_services.h"

#include "../localization/string_ed_api.h"
#include "../networking/net_address.h"
#include "server.h"
#include "server/engine/server_authorize.h"

/* NOT_FROM_ORIGINAL_SOURCE: retain the Windows listen-server localization
 * presentation around the common client-drop lifecycle. */
void server_compat_emit_drop_messages(client_t *client, const char *name, const char *dropReason)
{
    if (dropReason == NULL || Q_stricmpn(dropReason, "EXE_DISCONNECTED", 99999) != 0) {
        SV_SendServerCommand(NULL, qfalse, "e \"\x15%s^7 \x14%s\"", name, dropReason);
    }

    const int32_t clientNum = (int32_t)(client - svs.clients);
    Com_Printf("%i:%s %s\n", clientNum, name, dropReason);

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    const char *localizedReason = dropReason;
    if (cl_languagetranslate != NULL && cl_languagetranslate->integer != 0 && dropReason[0] != '\0' && dropReason[1] != '\0') {
        localizedReason = SEH_StringEd_GetString(dropReason);
    }

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if (localizedReason == NULL) {
        SV_SendServerCommand(client, qtrue, "w \"%s^7 %s\"", name, dropReason);
    } else {
        SV_SendServerCommand(client, qtrue, "w \"%s\"", dropReason);
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: bind the shared resend path to the Windows
 * authorization policy. */
void server_compat_authorize_resent_gamestate(client_t *client)
{
    if (net_lanauthorize->integer != 0 || Sys_IsLANAddress(client->netchan.remoteAddress) == qfalse) {
        SV_AuthorizeRequest(client->netchan.remoteAddress, client->challenge);
    }
}
