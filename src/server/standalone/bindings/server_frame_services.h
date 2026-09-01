#ifndef CODUO_SERVER_FRAME_SERVICES_H
#define CODUO_SERVER_FRAME_SERVICES_H

#include <stdint.h>

int32_t Sys_Milliseconds(void);

/* The dedicated SV_Frame has no client debug-geometry presentation edge. */
#define SERVER_FRAME_PRE_GAME_VM() ((void)0)
#define SERVER_FRAME_MILLISECONDS() Sys_Milliseconds()

#endif
