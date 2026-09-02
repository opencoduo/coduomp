#include <string.h>

#include "../module/ui_functions.h"

enum {
    UI_STATUS_ADDRESS_COPY_SIZE = 63,
    UI_STATUS_PLAYER_SECTION_LIMIT = UI_SERVER_STATUS_MAX_LINES - 3
};

// Source: uo_ui_mp_x86.dll 0x4000e050..0x4000e36e
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000e050_4000e36e.mcode
// Exact same-module PPC symbol: UI_GetServerStatusInfo.
qboolean UI_GetServerStatusInfo(const char *address, uiServerStatusInfo_t *statusInfo)
{
    static const char empty[] = "";
    char *cursor;

    if (statusInfo == NULL) {
        trap_LAN_ServerStatus(address, NULL, 0);
        return qfalse;
    }

    memset(statusInfo, 0, sizeof(*statusInfo));
    if (!trap_LAN_ServerStatus(address, statusInfo->text, sizeof(statusInfo->text))) {
        return qfalse;
    }

    strncpy(statusInfo->address, address, UI_STATUS_ADDRESS_COPY_SIZE);
    statusInfo->address[UI_STATUS_ADDRESS_COPY_SIZE] = '\0';
    statusInfo->lines[0].column[0] = "address";
    statusInfo->lines[0].column[1] = empty;
    statusInfo->lines[0].column[2] = empty;
    statusInfo->lines[0].column[3] = statusInfo->address;
    statusInfo->numLines = 1;

    cursor = statusInfo->text;
    while (cursor != NULL && cursor[0] != '\0') {
        char *delimiter = strchr(cursor, '\\');
        uiServerStatusLine_t *line;

        if (delimiter == NULL) {
            break;
        }
        delimiter[0] = '\0';
        cursor = delimiter + 1;
        if (cursor[0] == '\\') {
            break;
        }

        line = &statusInfo->lines[statusInfo->numLines];
        line->column[0] = cursor;
        line->column[1] = empty;
        line->column[2] = empty;
        delimiter = strchr(cursor, '\\');
        if (delimiter == NULL) {
            break;
        }
        delimiter[0] = '\0';
        cursor = delimiter + 1;
        line->column[3] = cursor;
        ++statusInfo->numLines;
        if (statusInfo->numLines >= UI_SERVER_STATUS_MAX_LINES) {
            break;
        }
    }

    if (statusInfo->numLines < UI_STATUS_PLAYER_SECTION_LIMIT) {
        uiServerStatusLine_t *line;
        int32_t playerIndex = 0;

        line = &statusInfo->lines[statusInfo->numLines++];
        line->column[0] = empty;
        line->column[1] = empty;
        line->column[2] = empty;
        line->column[3] = empty;

        line = &statusInfo->lines[statusInfo->numLines++];
        line->column[0] = "@EXE_SV_INFO_NUM";
        line->column[1] = "@EXE_SV_INFO_SCORE";
        line->column[2] = "@EXE_SV_INFO_PING";
        line->column[3] = "@EXE_SV_INFO_NAME";

        while (cursor != NULL && cursor[0] != '\0') {
            char *score;
            char *ping;
            char *name;
            char *delimiter;

            if (cursor[0] == '\\') {
                cursor[0] = '\0';
                ++cursor;
            }
            score = cursor;
            delimiter = strchr(score, ' ');
            if (delimiter == NULL) {
                break;
            }
            delimiter[0] = '\0';
            ping = delimiter + 1;
            delimiter = strchr(ping, ' ');
            if (delimiter == NULL) {
                break;
            }
            delimiter[0] = '\0';
            name = delimiter + 1;

            /* NOT_FROM_ORIGINAL_SOURCE: retain at most MAX_CLIENTS rows. Each
             * generated number owns a fixed slot so the table scales with the
             * configured client limit. */
            if (playerIndex >= MAX_CLIENTS) {
                break;
            }

            line = &statusInfo->lines[statusInfo->numLines];
            Com_sprintf(statusInfo->numberText[playerIndex], sizeof(statusInfo->numberText[playerIndex]), "%d", playerIndex);
            line->column[0] = statusInfo->numberText[playerIndex];
            line->column[1] = score;
            line->column[2] = ping;
            line->column[3] = name;
            ++statusInfo->numLines;
            if (statusInfo->numLines >= UI_SERVER_STATUS_MAX_LINES) {
                break;
            }

            delimiter = strchr(name, '\\');
            if (delimiter == NULL) {
                break;
            }
            delimiter[0] = '\0';
            cursor = delimiter + 1;
            ++playerIndex;
        }
    }

    UI_SortServerStatusInfo(statusInfo);
    return qtrue;
}
