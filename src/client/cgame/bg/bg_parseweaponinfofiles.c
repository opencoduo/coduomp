// Source: uo_cgame_mp_x86.dll 0x3000fee0..0x300101c7
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3000fee0_300101c7.mcode
//
// BG_ParseWeaponInfoFiles — initialize the weapon-info sentinel, then load and parse
// each named weapon definition from the caller's argv array.
//
// The .mcode header's PM_SetMovementDir assignment is rejected: it was a size
// match. The function allocates weaponInfo_t records, opens "weapons/mp/<name>",
// validates the WEAPONFILE header, parses 293 weapon fields, and rolls back a
// failed record. The Mac cgame symbol BG_ParseWeaponInfoFiles shares the two error
// reporting callees and adds the corresponding initialization call, resolving the
// source name. Original ABI is ordinary cdecl (argv, argc).

#include "../client_recovered.h"
#include "../globals.h"

#include <stdio.h>
#include <string.h>

enum {
    WEAPON_TEXT_SIZE = 8192
};

void BG_ParseWeaponInfoFiles(const char **argv, int argc)
{
    static const char weaponFileHeader[] = "WEAPONFILE";
    const int32_t headerLength = (int32_t)(sizeof(weaponFileHeader) - 1);
    char filename[MAX_QPATH];
    char text[WEAPON_TEXT_SIZE];
    int32_t fileHandle;
    int32_t i;

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    if (argc < 0 || argc > MAX_WEAPON_FILES) {
        Com_Error(ERR_DROP, "\x15" "Server sent too many weapons");
        return;
    }

    /* 0x3000ff0e..0x3000ff28: create the shared heap-backed empty string. The
     * pointer-valued syscall result is an explicit 32-bit ABI boundary. */
    cg_emptyString = (const char *)(intptr_t)
        cgame_syscall(CG_HUNK_ALLOC_LOW_ALIGN_EXPLICIT, 1, 1);
    ((char *)cg_emptyString)[0] = '\0';

    /* Index zero is the "none" sentinel weaponInfo_t. */
    weaponInfo_t *none = CG_AllocWeaponInfo(BG_WEAPON_FIELD_COUNT, 0,
                                          bg_weaponFieldDefs);
    CG_CopyString("none", (char **)&none->pickupName);
    bg_numWeapons = 0;

    for (i = 0; i < argc; ++i) {
        int32_t debugWeaponMessages;
        int32_t fileLength;
        int32_t textLength;
        weaponInfo_t *weapon;

        bg_numWeapons = coduo_int32_from_bits(
            (uint32_t)bg_numWeapons + 1u);
        weapon = CG_AllocWeaponInfo(BG_WEAPON_FIELD_COUNT, bg_numWeapons,
                                    bg_weaponFieldDefs);

        const size_t folderLength = strlen(bg_weaponDefsPath);
        const size_t tokenLength = strlen(argv[i]);
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
        if (folderLength >= sizeof(filename) ||
            tokenLength > sizeof(filename) - folderLength - 2) {
            Com_Error(ERR_DROP,
                      "\x15" "Server sent an overlong weapon name");
            return;
        }
        Com_sprintf(filename, sizeof(filename), "%s/%s", bg_weaponDefsPath,
                    argv[i]);

        /* 0x3000ffe5 loads the cvar integer once, then tests and compares the
         * retained EAX value. */
        debugWeaponMessages = bg_debugWeaponMessages_vmCvar.integer;
        if (debugWeaponMessages != 0 && debugWeaponMessages != 3) {
            Com_DPrintf("Parsing weapon file \"%s\"...\n", filename);
        }

        fileLength = (int32_t)cgame_syscall(CG_FS_FOPEN_FILE, filename,
                                   (intptr_t)&fileHandle, FS_READ);
        if (fileLength <= 0) {
            Com_Error(ERR_DROP,
                "\x15" "Could not load weapon file '%s'", filename);
        }

        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
        if (fileLength < headerLength) {
            (void)cgame_syscall(CG_FS_FCLOSE_FILE, fileHandle);
            Com_Error(ERR_DROP,
                "\x15\"%s\" is too short to be a weapon file", filename);
            return;
        }

        /* Read and validate the fixed header separately; the second read resumes
         * from the current file position and obtains only the definition text. */
        (void)cgame_syscall(CG_FS_READ, text, headerLength, fileHandle);
        text[headerLength] = '\0';
        if (strncmp(text, weaponFileHeader, (size_t)headerLength) != 0) {
            (void)cgame_syscall(CG_FS_FCLOSE_FILE, fileHandle);
            Com_Error(ERR_DROP,
                "\x15\"%s\" does not appear to be a weapon file", filename);
        }

        textLength = coduo_int32_from_bits((uint32_t)fileLength -
                                      (uint32_t)headerLength);
        if (textLength >= WEAPON_TEXT_SIZE) {
            (void)cgame_syscall(CG_FS_FCLOSE_FILE, fileHandle);
            Com_Error(ERR_DROP,
                "\x15\"%s\" Is too long of a weapon file to parse", filename);
        }

        memset(text, 0, sizeof(text));
        (void)cgame_syscall(CG_FS_READ, text, textLength, fileHandle);
        text[textLength] = '\0';
        (void)cgame_syscall(CG_FS_FCLOSE_FILE, fileHandle);

        /* Quotation marks and semicolons are forbidden anywhere in the payload. */
        if (strchr(text, '"') != NULL || strchr(text, ';') != NULL) {
            Com_Error(ERR_DROP,
                "\x15\"%s\" is not a valid weapon file", filename);
        }

        CG_CopyString(argv[i], (char **)&weapon->pickupName);

        if (!ParseConfigStringToStruct(weapon, bg_weaponFieldDefs,
                                      BG_WEAPON_FIELD_COUNT, text,
                                      WEAPON_FIELD_CUSTOM_TYPE_LIMIT,
                                      BG_ParseWeaponInfoSpecificFieldType,
                                      CG_WeaponInfoSetString)) {
            bg_weaponInfos[bg_numWeapons] = NULL;
            bg_numWeapons = coduo_int32_from_bits(
                (uint32_t)bg_numWeapons - 1u);
        }
    }
}
