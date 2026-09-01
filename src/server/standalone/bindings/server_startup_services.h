#ifndef CODUO_SERVER_STARTUP_SERVICES_H
#define CODUO_SERVER_STARTUP_SERVICES_H

#include "qcommon/q_shared_types.h"

#include <stdint.h>

void server_compat_begin_map_load(void);
void server_compat_prepare_second_game_shutdown(void);
void server_compat_finish_client_shutdown(void);
int32_t server_compat_generate_checksum_feed(void);
void server_compat_initialize_new_server_pools(void);
void server_compat_initialize_restart_pools(void);
int32_t server_compat_tolower_gametype_byte(char value);
void server_compat_notify_punkbuster_state(qboolean enabled);

#endif
