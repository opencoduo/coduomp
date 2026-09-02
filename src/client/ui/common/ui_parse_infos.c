#include "../module/ui_functions.h"

#include <string.h>

enum {
    UI_INFO_BUFFER_SIZE = 1024,
    UI_INFO_MAX_NUMBER_RESERVE = 64
};

// Source: uo_ui_mp_x86.dll 0x40007e70..0x400080d3
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40007e70_400080d3.mcode
// Role name: UI_ParseInfos; exact brace tokens, diagnostics, info-string setter,
// allocation reserve, and output-array behavior identify the shared UI parser.
int32_t UI_ParseInfos(char *buffer, int32_t maxInfos, char **infos)
{
    int32_t count = 0;

    for (;;) {
        char info[UI_INFO_BUFFER_SIZE];
        char key[UI_INFO_BUFFER_SIZE];
        char *token = Com_Parse(&buffer);
        char *allocation;
        size_t allocationSize;

        if (*token == '\0') {
            break;
        }
        if (strcmp(token, "{") != 0) {
            Com_Printf("Missing { in info file\n");
            break;
        }
        if (count == maxInfos) {
            Com_Printf("Max infos exceeded\n");
            break;
        }

        info[0] = '\0';
        for (;;) {
            const char *value;

            token = Com_Parse(&buffer);
            if (*token == '\0') {
                Com_Printf("Unexpected end of info file\n");
                break;
            }
            if (strcmp(token, "}") == 0) {
                break;
            }
            strncpy(key, token, UI_INFO_BUFFER_SIZE - 1);
            key[UI_INFO_BUFFER_SIZE - 1] = '\0';

            value = Com_ParseOnLine(&buffer);
            if (*value == '\0') {
                value = "<NULL>";
            }
            Info_SetValueForKey(info, key, value);
        }

        allocationSize = strlen(info) + strlen(va("%d", UI_INFO_MAX_NUMBER_RESERVE)) + 6;
        allocation = UI_Alloc(allocationSize);
        infos[count] = allocation;
        if (allocation == NULL) {
            continue;
        }
        strcpy(allocation, info);
        count++;
    }

    return count;
}
