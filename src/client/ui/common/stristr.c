#include "../module/ui_functions.h"
#include "client/common/client_legacy_crt.h"

// Source: uo_ui_mp_x86.dll 0x4000e370..0x4000e3dd
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000e370_4000e3dd.mcode
// Exact same-module PPC symbol: stristr.
const char *stristr(const char *string, const char *substring)
{
    const char *candidate;

    if (*string == '\0') {
        return NULL;
    }
    for (candidate = string; *candidate != '\0'; ++candidate) {
        int32_t index;

        for (index = 0; substring[index] != '\0' && candidate[index] != '\0'; ++index) {
            int32_t candidateCharacter = (int32_t)(signed char)candidate[index];
            int32_t substringCharacter = (int32_t)(signed char)substring[index];

            if (coduo_crt_toupper(candidateCharacter) != coduo_crt_toupper(substringCharacter)) {
                break;
            }
        }
        if (substring[index] == '\0') {
            return candidate;
        }
    }
    return NULL;
}
