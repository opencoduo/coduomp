// Miscellaneous complete leaves recovered from uo_cgame_mp_x86.dll.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

enum {
    WEAPON_FILE_NAME_COMPARE_LIMIT = 99999
};

// Source RVA: 0x30010dd0
int32_t compare_weaponfile_names(const char *const *left, const char *const *right)
{
    const char *leftString = *left;
    const char *rightString = *right;

    /* Both dword pointers are loaded before either null branch is taken. */
    if (leftString == NULL || rightString == NULL) {
        return -1;
    }
    /* 0x30010dd0 receives left in EAX and right in ECX, then forwards left in
     * Q_stricmpn's EDX/left register and right in its ECX/right register. */
    return Q_stricmpn(leftString, rightString, WEAPON_FILE_NAME_COMPARE_LIMIT);
}
