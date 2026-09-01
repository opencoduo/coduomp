#ifndef CODUO_SERVER_GAME_LIFECYCLE_SERVICES_H
#define CODUO_SERVER_GAME_LIFECYCLE_SERVICES_H

void Sys_LoadingKeepAlive(void);

/* coduo_lnxded 0x08090bfe and 0x08090c3a call this empty platform hook around
 * GAME_INIT. */
#define SERVER_GAME_LOADING_KEEPALIVE() Sys_LoadingKeepAlive()

#endif
