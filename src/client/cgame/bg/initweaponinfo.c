#include "../client_recovered.h"
#include "../globals.h"

#include <stddef.h>
#include <stdint.h>

// Source: uo_cgame_mp_x86.dll 0x30010df0..0x30010f65
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30010df0_30010f65.mcode
//
// InitWeaponInfo — the weapon-info registration entry point. It:
//   1. Allocates (or fetches) the engine-owned bg_weaponInfos pointer array via
//      CG_GET_WEAPON_INFO_MEMORY (trap 0xcb), storing the base at bg_weaponInfos; a NULL return
//      is a fatal "Could not allocate weaponInfo_t array" error. The trap also
//      uses 0x200 as the original 32-bit pointer-array allocation size, and
//      writes the previous-owner flag through loadedFlag below.
//   2. Resets the two per-slot dedup tables — the ammo-type table
//      (bg_ammoTypeNames[]/bg_ammoTypeMax[], count bg_numAmmoTypes) and the
//      clip table (bg_ammoClipNames[]/bg_ammoClipSizes[], count bg_numAmmoClips):
//      it zeroes all 128 name entries, zeroes value entry [0], and reserves
//      index 0 as "none" with each count seeded to 1.
//   3. If loadedFlag == 0 (fresh load): copies the CS_WEAPONS config string (the
//      space-separated weapon-name list, config string 7) into a local buffer,
//      tokenizes it on spaces into an argv array, and hands it to
//      BG_ParseWeaponInfoFiles(argv, argc) which populates the weaponInfo_t records.
//      If loadedFlag != 0 (array already populated): it instead recounts the
//      non-NULL entries in bg_weaponInfos[1..127] into bg_numWeapons.
//   4. Runs the derived-table passes: BG_SetupWeaponADSRates,
//      BG_SetupAmmoIndexes, BG_SetupSharedAmmoIndexes, BG_SetupClipIndexes,
//      BG_FillInWeaponItems, BG_SetupAltWeaponIndexes.
//
// Naming: the .mcode header name "fire_artillery_barrage" is a pure size-only
// corpus guess and is REJECTED (this function does no artillery/fire logic; it
// registers weapon info). Named by behavior — the weaponInfo_t array allocation,
// the "Could not allocate weaponInfo_t array" fatal string, and the weapon-list
// parse — matching the cgame_mp.dll "InitWeaponInfo" name-bank entry.
//
// A /GS stack cookie (__security_cookie snapshotted at 0x30010e00, checked via
// __security_check_cookie 0x30061639 at the RET) and the __chkstk 0x220c-byte
// frame probe (0x30010dfb) are compiler artifacts and are not modelled here.
//
// The trap dispatcher and Com_Error are the caller-observed ABI records in
// client_recovered.h; the six worker passes are declared there too.

// Local-frame layout proven from the machine code:
//   [ESP+0x2208] = /GS cookie snapshot (frame top)         -> not modelled
//   [ESP+0x208..+0x2207] = char weaponListBuf[0x2000]
//   [ESP+0x08.. ]  = char *argv[128]            (token pointer array)
//   [ESP+0x04]     = int32_t loadedFlag         (trap out-param dword)
enum {
    BG_WEAPON_LIST_BUFSIZE = 8192   /* 0x2208 cookie - 0x208 buffer start */
};

void InitWeaponInfo(void)
{
    char weaponListBuf[BG_WEAPON_LIST_BUFSIZE];
    char *argv[MAX_WEAPONS];
    int32_t loadedFlag;

    // 0x30010e0d..0x30010e2f: allocate/fetch the weaponInfo_t pointer array. Trap
    // arg order is (command=0xcb, byteCount=0x200, &loadedFlag) on i386; the
    // trap returns the array base and writes the previous-owner flag into
    // loadedFlag. Use the native pointer-array size so the same 128 entries fit
    // after pointers widen, while preserving the exact original 0x200 on i386.
    weaponInfo_t **weaponArray =
        (weaponInfo_t **)(intptr_t)cgame_syscall(CG_GET_WEAPON_INFO_MEMORY, sizeof(weaponInfo_t *) * MAX_WEAPONS, (intptr_t)&loadedFlag);
    bg_weaponInfos = weaponArray;

    // 0x30010e2f/0x30010e31: NULL array => fatal error (ERR_DROP). The "\x15"
    // leading byte is the CG_ERROR marker consumed by the Com_Error family.
    if (weaponArray == NULL) {
        Com_Error(ERR_DROP, "\x15"
                            "Could not allocate weaponInfo_t array\n");
        /* 0x30010e3d reloads the global if the fatal callback returns. */
        weaponArray = bg_weaponInfos;
    }

    // 0x30010e46..0x30010ea1: reset the two dedup tables.
    //   Ammo-type table:  bg_ammoTypeNames (0x300a8038, 128 entries cleared),
    //                     bg_ammoTypeMax[0]=0 (0x300a7a28), name[0]="none",
    //                     bg_numAmmoTypes=1 (0x300a7e30).
    //   Clip table:       bg_ammoClipNames (0x300a7828, 128 entries cleared),
    //                     bg_ammoClipSizes[0]=0 (0x300a8238), name[0]="none",
    //                     bg_numAmmoClips=1 (0x300a7e28).
    for (int i = 0; i < MAX_AMMO_TYPES; ++i) {
        bg_ammoTypeNames[i] = NULL;
    }
    for (int i = 0; i < MAX_AMMO_TYPES; ++i) {
        bg_ammoClipNames[i] = NULL;
    }
    bg_ammoTypeMax[0] = 0;
    bg_ammoClipSizes[0] = 0;

    // 0x30010e60..0x30010e73: clear the local argv[] pointer array (127 dwords),
    // then snapshot loadedFlag and seed the two per-slot name/count pairs.
    for (int i = 0; i < MAX_WEAPON_FILES; ++i) {
        argv[i] = NULL;
    }
    int32_t alreadyLoaded = loadedFlag;
    // 0x30010e7b/0x30010e8f: the two dedup tables' slot-0 config-string names are
    // seeded to "none" (the .rdata string at 0x30072894), and the slot counters
    // set to 1. Keep their exact source order after the flag load/test.
    bg_ammoTypeNames[0] = "none";
    bg_numAmmoTypes = 1;
    bg_ammoClipNames[0] = "none";
    bg_numAmmoClips = 1;

    // 0x30010e75/0x30010ea3: branch on the trap's "already populated" flag.
    if (alreadyLoaded == 0) {
        // 0x30010ea5..0x30010f17: fresh load. Copy the CS_WEAPONS config string
        // (config string 7 = the space-separated weapon-name list) into the local
        // buffer, tokenize it on spaces, and register the weapons.
        //
        // 0x30010ea5..0x30010eaa: cg_gameState.stringData + stringOffsets[CS_WEAPONS]
        // is the config-string text. The SUB/MOV byte loop is a strcpy into
        // weaponListBuf.
        const char *weaponList = &cg_gameState.stringData[cg_gameState.stringOffsets[CS_WEAPONS]];
        {
            const char *src = weaponList;
            char *dst = weaponListBuf;
            char ch;
            do {
                ch = *src++;
                *dst++ = ch;
            } while (ch != '\0');
        }

        // 0x30010ed1..0x30010f07: tokenize weaponListBuf in place on ' '. argv[0]
        // is the whole-string start; each maximal non-space run after a run of
        // spaces becomes the next token (spaces overwritten with NUL). argc counts
        // the tokens (starts at 1 for argv[0]).
        int argc = 1;
        argv[0] = weaponListBuf;
        char *p = weaponListBuf;
        if (*p != '\0') {
            for (;;) {
                if (*p == ' ') {
                    *p = '\0';
                    ++p;
                    if (*p == '\0') {
                        break;
                    }
                    if (*p != ' ') {
                        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
                        if (argc >= MAX_WEAPON_FILES) {
                            Com_Error(ERR_DROP, "\x15"
                                                "Server sent too many weapons");
                            return;
                        }
                        argv[argc] = p;
                        ++argc;
                    }
                    // else: consecutive space, fall through to the tail test.
                } else {
                    ++p;
                }
                if (*p == '\0') {
                    break;
                }
            }
        }

        // 0x30010f09..0x30010f17: register the parsed weapon list.
        BG_ParseWeaponInfoFiles((const char **)argv, argc);
    } else {
        // 0x30010f19..0x30010f34: array already populated — recount the non-NULL
        // weaponInfo_t pointers in bg_weaponInfos[1..127] into bg_numWeapons. The
        // scan walks &weaponArray[1] forward, capping the index at 126 (<= 0x7e).
        int32_t count = 0;
        bg_numWeapons = count;
        weaponInfo_t **entry = &weaponArray[1];
        while (*entry != NULL) {
            ++count;
            ++entry;
            if (count > 126) {
                break;
            }
        }
        bg_numWeapons = count;
    }

    // 0x30010f36..0x30010f4f: run the derived-table registration passes.
    BG_SetupWeaponADSRates();
    BG_SetupAmmoIndexes();
    BG_SetupSharedAmmoIndexes();
    BG_SetupClipIndexes();
    BG_FillInWeaponItems();
    BG_SetupAltWeaponIndexes();
}
