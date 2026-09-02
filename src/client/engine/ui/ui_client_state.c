#include "ui_client_state.h"

#include "../client/cgame.h"
#include "../client/server_browser.h"
#include "filesystem/filesystem.h"
#include "client/engine/filesystem/filesystem_cdkey.h"
#include "../platform/punkbuster_boundary.h"
#include "ui_module_loader.h"

#include <string.h>

/* Source: CoDUOMP.exe 0x0041b430..0x0041b43d.
 * Name and boolean role: exact same-module Mac symbol CLUI_SetPbClStatus.
 * The retired PunkBuster implementation is isolated behind the maintained
 * optional-backend boundary. */
void CLUI_SetPbClStatus(qboolean enabled)
{
    PB_SetClientEnabled(enabled);
}

/* Source: CoDUOMP.exe 0x0041b440..0x0041b452.
 * Windows copies 40 dwords/0xa0 bytes. The exact same-module Mac symbol proves
 * the CL_GetGlconfig name, but that client copies its distinct 0x1490-byte
 * configuration carrier and does not corroborate the Windows field layout. */
void CL_GetGlconfig(glconfig_t *config)
{
    memcpy(config, &cls.rendererConfig, sizeof(*config));
}

/* Source: CoDUOMP.exe 0x0041b460..0x0041b48a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041b460_0041b48b.mcode.
 * Name and signature: exact same-module Mac symbol GetClipboardDataUI. The
 * Windows compiler inlines Q_strncpyz and the Z_FreeInternal tail call. */
void GetClipboardDataUI(char *buffer, int32_t bufferSize)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (buffer == NULL || bufferSize <= 0) {
        return;
    }

    char *const clipboard = Sys_GetClipboardData();
    if (clipboard == NULL) {
        buffer[0] = '\0';
        return;
    }

    Q_strncpyz(buffer, clipboard, bufferSize);
    Z_FreeInternal(clipboard);
}

/* Source: CoDUOMP.exe 0x0041b520..0x0041b5c9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041b520_0041b5ca.mcode.
 * Name and argument positions: exact same-module Mac symbol CLUI_GetCDKey.
 * The second syscall argument is deliberately unused. If the UI requests a
 * unique key while an fs_game module is active, that module's key/checksum
 * pair is returned; otherwise the primary CoD:UO pair is returned. */
void CLUI_GetCDKey(char *key, int32_t keySize, char *checksum)
{
    const cvar_t *const fsGame = Cvar_Get("fs_game", "", CVAR_SYSTEMINFO | CVAR_INIT);
    const qboolean useUniqueModKey = coduo_uiVm != NULL && UI_usesUniqueCDKey() != 0 && fsGame != NULL && fsGame->string[0] != '\0';
    const int32_t keyOffset = useUniqueModKey ? CL_UNIQUE_MOD_CDKEY_OFFSET : CL_PRIMARY_CDKEY_OFFSET;
    const int32_t checksumOffset = useUniqueModKey ? CL_UNIQUE_MOD_CDKEY_CHECKSUM_OFFSET : CL_PRIMARY_CDKEY_CHECKSUM_OFFSET;

    (void)keySize;
    memcpy(key, &cl_cdkey[keyOffset], CL_CDKEY_PART_SIZE);
    key[CL_CDKEY_PART_SIZE] = '\0';
    memcpy(checksum, &cl_cdkeyChecksums[checksumOffset], CL_CDKEY_CHECKSUM_SIZE);
    checksum[CL_CDKEY_CHECKSUM_SIZE] = '\0';
}

/* Source: CoDUOMP.exe 0x0041b5d0..0x0041b60f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041b5d0_0041b610.mcode.
 * Exact same-module Mac symbol: CLUI_SetCDKey. The Windows body always updates
 * the primary CoD:UO pair and writes the United Offensive registry record. The
 * same-version Mac body uses the same unique-key predicate as CLUI_GetCDKey.
 * The modern native path retains that selection but maps the primary key to
 * the user-data root, keeping it separate from retail content directories. */
void CLUI_SetCDKey(const char *key, const char *checksum)
{
#if defined(_WIN32)
    memcpy(&cl_cdkey[CL_PRIMARY_CDKEY_OFFSET], key, CL_CDKEY_PART_SIZE);
    cl_cdkey[CL_UNIQUE_MOD_CDKEY_OFFSET] = '\0';
    memcpy(&cl_cdkeyChecksums[CL_PRIMARY_CDKEY_CHECKSUM_OFFSET], checksum, CL_CDKEY_CHECKSUM_SIZE);
    cl_cdkeyChecksums[CL_UNIQUE_MOD_CDKEY_CHECKSUM_OFFSET] = '\0';
    Com_WriteCDKey();
#else
    const cvar_t *const fsGame = Cvar_Get("fs_game", "", CVAR_SYSTEMINFO | CVAR_INIT);
    const qboolean useUniqueModKey = coduo_uiVm != NULL && UI_usesUniqueCDKey() != 0 && fsGame != NULL && fsGame->string[0] != '\0';
    const int32_t keyOffset = useUniqueModKey ? CL_UNIQUE_MOD_CDKEY_OFFSET : CL_PRIMARY_CDKEY_OFFSET;
    const int32_t checksumOffset = useUniqueModKey ? CL_UNIQUE_MOD_CDKEY_CHECKSUM_OFFSET : CL_PRIMARY_CDKEY_CHECKSUM_OFFSET;
    /* NOT_FROM_ORIGINAL_SOURCE: modern user-data/content separation. */
    const char *const gameDirectory = useUniqueModKey ? fsGame->string : "";

    memcpy(&cl_cdkey[keyOffset], key, CL_CDKEY_PART_SIZE);
    cl_cdkey[keyOffset + CL_CDKEY_PART_SIZE] = '\0';
    memcpy(&cl_cdkeyChecksums[checksumOffset], checksum, CL_CDKEY_CHECKSUM_SIZE);
    cl_cdkeyChecksums[checksumOffset + CL_CDKEY_CHECKSUM_SIZE] = '\0';
    Com_WriteCDKey(gameDirectory, &cl_cdkey[keyOffset], &cl_cdkeyChecksums[checksumOffset]);
#endif
}

/* Source: CoDUOMP.exe 0x0041b610..0x0041b64d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041b610_0041b64e.mcode.
 * Name and argument roles: exact same-module Mac symbol GetConfigString. */
qboolean GetConfigString(int32_t index, char *buffer, int32_t bufferSize)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (buffer == NULL || bufferSize <= 0 || index < 0 || index >= MAX_CONFIGSTRINGS) {
        return qfalse;
    }

    const int32_t offset = cl.gameState.stringOffsets[index];
    if (offset == 0) {
        buffer[0] = '\0';
        return qfalse;
    }

    Q_strncpyz(buffer, &cl.gameState.stringData[offset], bufferSize);
    return qtrue;
}

/* Source: CoDUOMP.exe 0x0041b650..0x0041b6a7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041b650_0041b6a8.mcode.
 * Name and argument roles: exact same-module Mac symbol GetClientname. */
qboolean GetClientname(int32_t clientNum, char *buffer, int32_t bufferSize)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (buffer == NULL || bufferSize <= 0) {
        return qfalse;
    }

    buffer[0] = '\0';
    if (cl.snap.valid == qfalse)
        return qfalse;

    for (int32_t index = 0; index < cl.snap.numClients; ++index) {
        const clientState_t *const client = &cl.parseClients[(cl.snap.firstClientSequence + index) & (CODUO_PARSE_RING_COUNT - 1)];
        if (client->clientNum == clientNum) {
            int32_t copySize = bufferSize;
            if ((size_t)copySize > sizeof(client->name)) {
                copySize = (int32_t)sizeof(client->name);
            }
            Q_strncpyz(buffer, client->name, copySize);
            return qtrue;
        }
    }

    return qfalse;
}

/* Source: CoDUOMP.exe 0x0041a460..0x0041a4d2.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041a460_0041a4d3.mcode.
 * Name, output layout, and field roles: exact same-module Mac symbol
 * GetClientState plus the shared UI syscall ABI. Each string copy deliberately
 * reserves and writes its final NUL byte, matching the three 1023-byte copies
 * in the original. */
void GetClientState(uiClientState_t *state)
{
    state->connectPacketCount = clc.connectPacketCount;
    state->connState = cls.state;

    strncpy(state->servername, cls.serverName, sizeof(state->servername) - 1);
    state->servername[sizeof(state->servername) - 1] = '\0';

    strncpy(state->updateInfoString, cls.updateInfoString, sizeof(state->updateInfoString) - 1);
    state->updateInfoString[sizeof(state->updateInfoString) - 1] = '\0';

    strncpy(state->messageString, clc.serverMessage, sizeof(state->messageString) - 1);
    state->messageString[sizeof(state->messageString) - 1] = '\0';

    state->clientNum = cl.snap.ps.psClientNum;
}
