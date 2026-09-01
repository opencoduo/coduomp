#ifndef QCOMMON_LIMITS_H
#define QCOMMON_LIMITS_H

/* Qcommon and engine capacities shared by the maintained targets. */
enum {
    MAX_CLIENTS = 64,
    MAX_RELIABLE_COMMANDS = 64,
    MAX_PACKET_USERCMDS = 32,
    COM_ERROR_MESSAGE_CAPACITY = 4096,
    BIG_INFO_STRING = 8192,
    MAX_INFO_STRING = 1024,
    MAX_NAME_LENGTH = 32,
    PACKET_BACKUP = 32,
    MAX_MAP_AREA_BYTES = 32,
    MAX_MSGLEN = 32768,
    MAX_DOWNLOAD_WINDOW = 8,
    MAX_CHALLENGES = 1024,
    MAX_MASTER_SERVERS = 5,
    MAX_ENT_CLUSTERS = 16,
    /* NOT_FROM_ORIGINAL_SOURCE: retail host-path scratch objects commonly
     * hold 256 bytes.  Use one larger capacity for maintained OS filesystem
     * paths.  This does not replace MAX_QPATH or fixed file/network/ABI fields. */
    MAX_OSPATH = 1024
};

/*
 * Player clones occupy one contiguous entity-number range shared by the game
 * module that allocates them and the cgame module that owns their animation
 * records. PLAYER_CLONE_ENTITYNUM_BASE is the first entity number in that
 * range, not a byte offset; it normally follows MAX_CLIENTS, so changing the
 * player limit relocates the clone range automatically. PLAYER_CLONE_COUNT is
 * the capacity knob. Retail therefore reserves eight entities numbered 64..71.
 * Coordinated client/server builds must use the same limits.
 */
#ifndef PLAYER_CLONE_ENTITYNUM_BASE
#define PLAYER_CLONE_ENTITYNUM_BASE MAX_CLIENTS
#endif

#ifndef PLAYER_CLONE_COUNT
#define PLAYER_CLONE_COUNT 8
#endif

#endif
