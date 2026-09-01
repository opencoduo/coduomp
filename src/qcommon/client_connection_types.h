#ifndef QCOMMON_CLIENT_CONNECTION_TYPES_H
#define QCOMMON_CLIENT_CONNECTION_TYPES_H

/* Client connection state passed unchanged through uiClientState_t. */
typedef enum connstate_e {
    CA_DISCONNECTED = 0,
    CA_CONNECTING = 1,
    CA_CHALLENGING = 2,
    CA_CONNECTED = 3,
    CA_LOADING = 4,
    CA_PRIMED = 5,
    CA_ACTIVE = 6,
    CA_CINEMATIC = 7,
    CA_LOGO = 8
} connstate_t;

typedef char q_connstate_size[sizeof(connstate_t) == 4 ? 1 : -1];

#endif
