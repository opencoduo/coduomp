#ifndef CODUOMP_SERVER_CLIENT_MESSAGE_SERVICES_H
#define CODUOMP_SERVER_CLIENT_MESSAGE_SERVICES_H

#include "qcommon/server_runtime_types.h"

void server_compat_emit_drop_messages(client_t *client, const char *name,
                                      const char *dropReason);
void server_compat_authorize_resent_gamestate(client_t *client);

#endif
