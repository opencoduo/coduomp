#ifndef QCOMMON_SERVER_PUNKBUSTER_TYPES_H
#define QCOMMON_SERVER_PUNKBUSTER_TYPES_H

#include <stddef.h>
#include <stdint.h>

enum {
    SERVER_PB_QUERY_COPY_LIMIT = 255,
    SERVER_PB_TITLE_SIZE = 32,
    SERVER_PB_BASE_PATH_SIZE = 260,
    SERVER_PUNKBUSTER_MAGIC = 0x357afe19
};

typedef enum serverPbQuery_e {
    PB_SERVER_QUERY_MAX_CLIENTS = 'e',
    PB_SERVER_QUERY_CLIENT_INFO = 'f',
    PB_SERVER_QUERY_CVAR_STRING = 'g',
    PB_SERVER_QUERY_CLIENT_STATUS = 'r'
} serverPbQuery_t;

typedef enum serverPbSaValue_e {
    PB_SERVER_SA_FRAME = 0,
    PB_SERVER_SA_COMMAND_DRAIN = -1
} serverPbSaValue_t;

typedef enum serverPbSbOpcode_e {
    PB_SERVER_SB_CLIENT_PACKET = 13,
    PB_SERVER_SB_START = 16,
    PB_SERVER_SB_NOTIFY_ENABLED = 117,
    PB_SERVER_SB_NOTIFY_DISABLED = 118
} serverPbSbOpcode_t;

typedef struct serverPbState_s serverPbState_t;

typedef int32_t (*serverPbCommandCallback_t)(const char *command,
                                             intptr_t value);
typedef const char *(*serverPbQueryCallback_t)(serverPbQuery_t query,
                                               char *buffer);
typedef int32_t (*serverPbOutputCallback_t)(const char *text);
typedef int32_t (*serverPbChecksumCallback_t)(const char *data,
                                              int32_t length,
                                              int32_t clientNum);
typedef intptr_t (*serverPbModuleSbCallback_t)(
    serverPbState_t *state, int32_t opcode, intptr_t arg0, intptr_t arg1,
    intptr_t arg2, intptr_t arg3);
typedef int32_t (*serverPbModuleSaCallback_t)(serverPbState_t *state,
                                              serverPbSaValue_t value);
typedef int32_t (*serverPbPacketCallback_t)(const char *address,
                                            uint16_t port,
                                            const void *data,
                                            int32_t length);
typedef void (*serverPbEventCallback_t)(serverPbState_t *state,
                                        const char *text,
                                        intptr_t value);
typedef const char *(*serverPbStringQueryCallback_t)(
    serverPbState_t *state, const char *text, intptr_t arg1, intptr_t arg2);
typedef void (*serverPbPrintCallback_t)(serverPbState_t *state,
                                        const char *text,
                                        int32_t severity);

/* CoDUOMP.exe and coduo_lnxded agree on this complete 0x168-byte i386 server
 * object, including every handle and callback slot.  The words at +0x004 and
 * +0x164 are only cleared as raw dwords, so they do not widen with host
 * pointers.  Callback payload arguments use intptr_t solely to keep native
 * wider-host module calls lossless; their original i386 width remains four. */
struct serverPbState_s {
    uint32_t magic;
    uint32_t opaque04;
    void *serverModuleHandle;
    void *moduleHandle0c;
    void *moduleHandle10;
    char title[SERVER_PB_TITLE_SIZE];
    char basePath[SERVER_PB_BASE_PATH_SIZE];
    int32_t loadPending;
    serverPbCommandCallback_t commandCallback;
    serverPbQueryCallback_t queryCallback;
    serverPbOutputCallback_t outputCallback;
    serverPbChecksumCallback_t checksumCallback;
    serverPbModuleSbCallback_t serverSbCallback;
    serverPbModuleSaCallback_t serverSaCallback;
    serverPbPacketCallback_t packetCallback;
    serverPbEventCallback_t eventCallback;
    serverPbStringQueryCallback_t stringQueryCallback;
    serverPbPrintCallback_t printCallback;
    uint32_t opaque164;
};

/* PB_ServerQuery's fixed three-string client result. */
typedef struct serverPbClientInfo_s {
    char name[33];
    char guid[33];
    char address[33];
    uint8_t padding63[5];
} serverPbClientInfo_t;

#if defined(__cplusplus)
#define SERVER_PB_STATIC_ASSERT static_assert
#define SERVER_PB_ALIGNOF alignof
#else
#define SERVER_PB_STATIC_ASSERT _Static_assert
#define SERVER_PB_ALIGNOF _Alignof
#endif

SERVER_PB_STATIC_ASSERT(sizeof(serverPbClientInfo_t) == 0x68,
                        "PunkBuster client-info size changed");
SERVER_PB_STATIC_ASSERT(offsetof(serverPbClientInfo_t, guid) == 0x21,
                        "PunkBuster client GUID moved");
SERVER_PB_STATIC_ASSERT(offsetof(serverPbClientInfo_t, address) == 0x42,
                        "PunkBuster client address moved");

#if UINTPTR_MAX == UINT32_MAX
SERVER_PB_STATIC_ASSERT(SERVER_PB_ALIGNOF(serverPbState_t) == 4,
                        "i386 PunkBuster server-state alignment changed");
SERVER_PB_STATIC_ASSERT(sizeof(serverPbState_t) == 0x168,
                        "i386 PunkBuster server-state size changed");
SERVER_PB_STATIC_ASSERT(offsetof(serverPbState_t, opaque04) == 0x004,
                        "i386 PunkBuster opaque word moved");
SERVER_PB_STATIC_ASSERT(offsetof(serverPbState_t, serverModuleHandle) == 0x008,
                        "i386 PunkBuster server module moved");
SERVER_PB_STATIC_ASSERT(offsetof(serverPbState_t, title) == 0x014,
                        "i386 PunkBuster title moved");
SERVER_PB_STATIC_ASSERT(offsetof(serverPbState_t, basePath) == 0x034,
                        "i386 PunkBuster base path moved");
SERVER_PB_STATIC_ASSERT(offsetof(serverPbState_t, loadPending) == 0x138,
                        "i386 PunkBuster load flag moved");
SERVER_PB_STATIC_ASSERT(offsetof(serverPbState_t, commandCallback) == 0x13c,
                        "i386 PunkBuster command callback moved");
SERVER_PB_STATIC_ASSERT(offsetof(serverPbState_t, queryCallback) == 0x140,
                        "i386 PunkBuster query callback moved");
SERVER_PB_STATIC_ASSERT(offsetof(serverPbState_t, outputCallback) == 0x144,
                        "i386 PunkBuster output callback moved");
SERVER_PB_STATIC_ASSERT(offsetof(serverPbState_t, checksumCallback) == 0x148,
                        "i386 PunkBuster checksum callback moved");
SERVER_PB_STATIC_ASSERT(offsetof(serverPbState_t, serverSbCallback) == 0x14c,
                        "i386 PunkBuster sb callback moved");
SERVER_PB_STATIC_ASSERT(offsetof(serverPbState_t, serverSaCallback) == 0x150,
                        "i386 PunkBuster sa callback moved");
SERVER_PB_STATIC_ASSERT(offsetof(serverPbState_t, packetCallback) == 0x154,
                        "i386 PunkBuster packet callback moved");
SERVER_PB_STATIC_ASSERT(offsetof(serverPbState_t, eventCallback) == 0x158,
                        "i386 PunkBuster event callback moved");
SERVER_PB_STATIC_ASSERT(offsetof(serverPbState_t, stringQueryCallback) == 0x15c,
                        "i386 PunkBuster string-query callback moved");
SERVER_PB_STATIC_ASSERT(offsetof(serverPbState_t, printCallback) == 0x160,
                        "i386 PunkBuster print callback moved");
SERVER_PB_STATIC_ASSERT(offsetof(serverPbState_t, opaque164) == 0x164,
                        "i386 PunkBuster trailing word moved");
#endif

#undef SERVER_PB_STATIC_ASSERT
#undef SERVER_PB_ALIGNOF

#endif
