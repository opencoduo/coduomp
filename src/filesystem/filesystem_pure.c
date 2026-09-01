#include "filesystem.h"
#include "filesystem_services.h"

#include "qcommon/com_sprintf.h"
#include "qcommon/q_command.h"
#include "qcommon/q_string.h"

#include <stdlib.h>
#include <string.h>

enum {
    FS_PURE_CHECKSUM_TEXT_SIZE = 8192,
    FS_PURE_REFERENCE_CLASS_COUNT = 3,
    FS_PURE_MATCH_WORD_BITS = 32,
    FS_PURE_MATCH_WORD_COUNT =
        (FS_MAX_SERVER_PAKS + FS_PURE_MATCH_WORD_BITS - 1) /
        FS_PURE_MATCH_WORD_BITS,
    FS_RESTRICTED_DEMO_PAK0_CHECKSUM =
        (int32_t)0xb1f595f5u,
};

/* Original Win32 return buffer at 0x0099f3a0. */
static char fs_referencedPakPureChecksums[FS_PURE_CHECKSUM_TEXT_SIZE];
static char fs_loadedPakChecksums[FS_PURE_CHECKSUM_TEXT_SIZE];
static char fs_loadedPakNames[FS_PURE_CHECKSUM_TEXT_SIZE];
static char fs_loadedPakPureChecksums[FS_PURE_CHECKSUM_TEXT_SIZE];
static char fs_referencedPakChecksums[FS_PURE_CHECKSUM_TEXT_SIZE];
static char fs_referencedPakNames[FS_PURE_CHECKSUM_TEXT_SIZE];

/* Source: CoDUOMP.exe 0x0042c8d0..0x0042c902.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0042c8d0_0042c903.mcode.
 * Name, argument, and checksum-list role: exact same-module Mac symbol
 * FS_PakIsPure. With no server-referenced checksums every pack is accepted. */
qboolean FS_PakIsPure(const pack_t *pack)
{
    if (fs_numServerPaks == 0)
        return qtrue;

    for (int32_t pakIndex = 0;
         pakIndex < fs_numServerPaks;
         ++pakIndex) {
        if (pack->checksum == fs_serverPaks[pakIndex])
            return qtrue;
    }

    return qfalse;
}

/* Source: CoDUOMP.exe 0x0043fa80..0x0043fb77.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043fa80_0043fb78.mcode.
 * Name and signature: exact same-module Mac symbol FS_FileIsInPAK. A leading
 * host separator is ignored, but relative path components are rejected.
 * Localized packs bypass the pure-server filter exactly as in the original. */
int32_t FS_FileIsInPAK(const char *path, int32_t *checksumOut)
{
    if (path == NULL) {
        Com_Error(0, "\x15"
                     "FS_FOpenFileRead: NULL 'filename' parameter passed");
    }

    if (path[0] == '/' || path[0] == '\\')
        ++path;

    if (strstr(path, "..") != NULL || strstr(path, "::") != NULL)
        return -1;

    for (searchpath_t *search = fs_searchpaths;
         search != NULL;
         search = search->next) {
        if (filesystem_compat_server_scope_allows_searchpath(search) ==
            qfalse) {
            continue;
        }
        pack_t *const pack = search->pack;
        if (pack == NULL)
            continue;

        const uint32_t hash =
            FS_HashFileName(path, pack->hashSize);
        fileInPack_t *packFile = pack->hashTable[hash];
        if (packFile == NULL)
            continue;

        if (search->localized == qfalse &&
            FS_PakIsPure(pack) == qfalse) {
            continue;
        }

        for (; packFile != NULL; packFile = packFile->next) {
            if (FS_FilenameCompare(path, packFile->name) != 0)
                continue;

            if (checksumOut != NULL)
                *checksumOut = pack->pureChecksum;
            return 1;
        }
    }

    return -1;
}

/* Source: CoDUOMP.exe 0x004403a0..0x004405b1.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004403a0_004405b2.mcode.
 * Name and signature: exact same-module Mac symbol FS_idPak. The stock-pak
 * spellings and 24 hexadecimal suffix variants are all explicit in the
 * original comparison chain. */
qboolean FS_idPak(const char *path, const char *mainGame,
                  const char *baseGame)
{
    if (FS_FilenameCompare(path, va("%s/mp_bin", mainGame)) == 0)
        return qtrue;

    for (int32_t pakIndex = 0; pakIndex < 24; ++pakIndex) {
        if (FS_FilenameCompare(
                path, va("%s/pak%x", mainGame, pakIndex)) == 0 ||
            FS_FilenameCompare(
                path, va("%s/pakuo%x", baseGame, pakIndex)) == 0 ||
            FS_FilenameCompare(
                path, va("%s/pakuo0%x", baseGame, pakIndex)) == 0 ||
            FS_FilenameCompare(
                path, va("%s/mp_pak%x", mainGame, pakIndex)) == 0 ||
            FS_FilenameCompare(
                path, va("%s/sp_pak%x", mainGame, pakIndex)) == 0) {
            return qtrue;
        }
    }

    const char *const localized = strstr(path, "localized_");
    if (localized == NULL)
        return qfalse;

    char localPath[MAX_QPATH];
    /* NOT_FROM_ORIGINAL_SOURCE: every caller must provide a complete path and
     * NUL that fit the fixed local qpath before classification. */
    if (strlen(path) >= sizeof(localPath))
        return qfalse;

    strcpy(localPath, path);
    localPath[(size_t)(localized - path) + strlen("localized_")] = '\0';
    if (FS_FilenameCompare(
            localPath, va("%s/localized_", mainGame)) != 0 &&
        FS_FilenameCompare(
            localPath, va("%s/localized_", baseGame)) != 0) {
        return qfalse;
    }

    strcpy(localPath, localized + strlen("localized_"));
    Q_strlwr(localPath);
    for (int32_t pakIndex = 0; pakIndex < 24; ++pakIndex) {
        if (strstr(localPath, va("_pak%x", pakIndex)) != NULL ||
            strstr(localPath, va("_pakuo%x", pakIndex)) != NULL ||
            strstr(localPath, va("_pakuo0%x", pakIndex)) != NULL) {
            return qtrue;
        }
    }

    return qfalse;
}

/* Source: CoDUOMP.exe 0x004405c0..0x0044062b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004405c0_0044062c.mcode.
 * Name and signature: exact same-module Mac symbol FS_serverPak. */
qboolean FS_serverPak(const char *pakName)
{
    char lowercaseName[MAX_QPATH];

    /* NOT_FROM_ORIGINAL_SOURCE: require a complete terminated pack name that
     * fits the fixed classification qpath. */
    if (strlen(pakName) >= sizeof(lowercaseName))
        return qfalse;

    strcpy(lowercaseName, pakName);
    Q_strlwr(lowercaseName);
    return strstr(lowercaseName, "_svr_") != NULL ? qtrue : qfalse;
}

/* Source: CoDUOMP.exe 0x00440630..0x004408f3.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00440630_004408f4.mcode.
 * Name and signature: exact same-module Mac symbol FS_ComparePaks. The
 * alternate-name form is the '@remote@local' download protocol; the readable
 * form reports one missing pak per line. A redirected bad-checksum file is
 * removed from the home root before it is reported again. */
qboolean FS_ComparePaks(char *neededPaks, int32_t neededPaksSize,
                        qboolean includeAlternateNames)
{
    if (fs_numServerReferencedPaks == 0)
        return qfalse;

    neededPaks[0] = '\0';
    for (int32_t pakIndex = 0;
         pakIndex < fs_numServerReferencedPaks;
        ++pakIndex) {
        const char *const pakName = fs_serverReferencedPakNames[pakIndex];

        /* NOT_FROM_ORIGINAL_SOURCE: a published pack name must satisfy the
         * relative virtual-path and local-name policy before classification. */
        if (filesystem_compat_accept_server_pak_name(pakName) == qfalse) {
            continue;
        }

        if (FS_idPak(pakName, "main", fs_basegame->string) != qfalse ||
            FS_serverPak(pakName) != qfalse) {
            continue;
        }

        qboolean found = qfalse;
        for (searchpath_t *search = fs_searchpaths;
             search != NULL;
             search = search->next) {
            if (filesystem_compat_server_scope_allows_searchpath(search) !=
                    qfalse &&
                search->pack != NULL &&
                search->pack->checksum == fs_serverReferencedPaks[pakIndex]) {
                found = qtrue;
                break;
            }
        }

        if (found != qfalse || pakName == NULL || pakName[0] == '\0')
            continue;

        if (includeAlternateNames != qfalse) {
            Q_strcat(neededPaks, neededPaksSize, "@");
            Q_strcat(neededPaks, neededPaksSize, pakName);
            Q_strcat(neededPaks, neededPaksSize, ".pk3");
            Q_strcat(neededPaks, neededPaksSize, "@");

            if (filesystem_compat_download_file_exists(
                    va("%s.pk3", pakName)) == qfalse) {
                Q_strcat(neededPaks, neededPaksSize, pakName);
                Q_strcat(neededPaks, neededPaksSize, ".pk3");
            } else {
                char alternateName[MAX_OSPATH];
                Com_sprintf(alternateName, sizeof(alternateName),
                            "%s.%08x.pk3", pakName,
                            (uint32_t)fs_serverReferencedPaks[pakIndex]);
                Q_strcat(neededPaks, neededPaksSize, alternateName);
            }
            continue;
        }

        Q_strcat(neededPaks, neededPaksSize, pakName);
        Q_strcat(neededPaks, neededPaksSize, ".pk3");
        if (filesystem_compat_download_file_exists(
                va("%s.pk3", pakName)) != qfalse) {
            Q_strcat(neededPaks, neededPaksSize,
                     " (local file exists with wrong checksum)");

            if (filesystem_compat_www_bad_checksum(
                    va("%s.pk3", pakName)) != qfalse) {
                filesystem_compat_remove_download(
                    va("%s.pk3", pakName));
            }
        }
        Q_strcat(neededPaks, neededPaksSize, "\n");
    }

    if (neededPaks[0] == '\0')
        return qfalse;

    Com_Printf("Need paks: %s\n", neededPaks);
    return qtrue;
}

/* Source: CoDUOMP.exe 0x00440a20..0x00440ab2.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00440a20_00440ab3.mcode.
 * Name: exact same-module Mac symbol FS_LoadedPakChecksums. */
const char *FS_LoadedPakChecksums(void)
{
    fs_loadedPakChecksums[0] = '\0';
    for (searchpath_t *search = fs_searchpaths;
         search != NULL;
         search = search->next) {
        if (filesystem_compat_server_scope_allows_searchpath(search) !=
                qfalse &&
            search->pack != NULL && search->localized == qfalse) {
            Q_strcat(fs_loadedPakChecksums,
                     sizeof(fs_loadedPakChecksums),
                     va("%i ", search->pack->checksum));
        }
    }
    return fs_loadedPakChecksums;
}

/* Source: CoDUOMP.exe 0x00440ac0..0x00440bb5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00440ac0_00440bb6.mcode.
 * Name: exact same-module Mac symbol FS_LoadedPakNames. */
const char *FS_LoadedPakNames(void)
{
    fs_loadedPakNames[0] = '\0';
    for (searchpath_t *search = fs_searchpaths;
         search != NULL;
         search = search->next) {
        if (filesystem_compat_server_scope_allows_searchpath(search) ==
                qfalse ||
            search->pack == NULL || search->localized != qfalse) {
            continue;
        }

        if (fs_loadedPakNames[0] != '\0') {
            Q_strcat(fs_loadedPakNames,
                     sizeof(fs_loadedPakNames), " ");
        }
        Q_strcat(fs_loadedPakNames, sizeof(fs_loadedPakNames),
                 search->pack->pakBasename);
    }
    return fs_loadedPakNames;
}

/* Source: CoDUOMP.exe 0x00440bc0..0x00440c52.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00440bc0_00440c53.mcode.
 * Name: exact same-module Mac symbol FS_LoadedPakPureChecksums. */
const char *FS_LoadedPakPureChecksums(void)
{
    fs_loadedPakPureChecksums[0] = '\0';
    for (searchpath_t *search = fs_searchpaths;
         search != NULL;
         search = search->next) {
        if (filesystem_compat_server_scope_allows_searchpath(search) !=
                qfalse &&
            search->pack != NULL && search->localized == qfalse) {
            Q_strcat(fs_loadedPakPureChecksums,
                     sizeof(fs_loadedPakPureChecksums),
                     va("%i ", search->pack->pureChecksum));
        }
    }
    return fs_loadedPakPureChecksums;
}

/* The referenced-pak text APIs include a pack when any of its general/UI/
 * cgame/game reference bytes is set, or when its game directory is neither
 * the retail "main" directory nor the configured base game. */
static qboolean coduomp_pack_should_report_referenced(
    const pack_t *pack)
{
    /* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the identical
     * 32-bit flag gates in the two following original functions. */
    if (pack->generalReference != 0 ||
        pack->uiModuleReference != 0 ||
        pack->cgameModuleReference != 0 ||
        pack->gameModuleReference != 0) {
        return qtrue;
    }
    if (Q_stricmpn(pack->pakGamename, "main", 4) == 0)
        return qfalse;
    if (Q_stricmpn(pack->pakGamename, fs_basegame->string,
                   (int32_t)strlen(fs_basegame->string)) == 0) {
        return qfalse;
    }
    return qtrue;
}

/* Source: CoDUOMP.exe 0x00440c60..0x00440d55.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00440c60_00440d56.mcode.
 * Name: exact same-module Mac symbol FS_ReferencedPakChecksums. */
const char *FS_ReferencedPakChecksums(void)
{
    fs_referencedPakChecksums[0] = '\0';
    for (searchpath_t *search = fs_searchpaths;
         search != NULL;
         search = search->next) {
        if (filesystem_compat_server_scope_allows_searchpath(search) !=
                qfalse &&
            search->pack != NULL &&
            coduomp_pack_should_report_referenced(search->pack) != qfalse) {
            Q_strcat(fs_referencedPakChecksums,
                     sizeof(fs_referencedPakChecksums),
                     va("%i ", search->pack->checksum));
        }
    }
    return fs_referencedPakChecksums;
}

/* Source: CoDUOMP.exe 0x00440d60..0x00440f4d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00440d60_00440f4e.mcode.
 * Name: exact same-module Mac symbol FS_ReferencedPakNames. */
const char *FS_ReferencedPakNames(void)
{
    fs_referencedPakNames[0] = '\0';
    for (searchpath_t *search = fs_searchpaths;
         search != NULL;
         search = search->next) {
        pack_t *const pack = search->pack;
        if (filesystem_compat_server_scope_allows_searchpath(search) ==
                qfalse ||
            pack == NULL ||
            coduomp_pack_should_report_referenced(pack) == qfalse) {
            continue;
        }

        if (fs_referencedPakNames[0] != '\0') {
            Q_strcat(fs_referencedPakNames,
                     sizeof(fs_referencedPakNames), " ");
        }
        Q_strcat(fs_referencedPakNames,
                 sizeof(fs_referencedPakNames), pack->pakGamename);
        Q_strcat(fs_referencedPakNames,
                 sizeof(fs_referencedPakNames), "/");
        Q_strcat(fs_referencedPakNames,
                 sizeof(fs_referencedPakNames), pack->pakBasename);
    }
    return fs_referencedPakNames;
}

/* Source: CoDUOMP.exe 0x00440980..0x00440a19.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00440980_00440a1a.mcode.
 * Name and accepted pak checksum: same-family Linux engine symbol
 * FS_CheckRestrictedDemoPaks. Localized paths outside the active language
 * are ignored by the same filter used by normal filesystem lookup. */
void FS_CheckRestrictedDemoPaks(void)
{
    if (fs_restrict->integer == 0)
        return;

    (void)Cvar_Set2("fs_restrict", "0", qtrue);
    Com_Printf("\nRunning in restricted demo mode.\n\n");

    for (searchpath_t *search = fs_searchpaths;
         search != NULL;
         search = search->next) {
        if (FS_UseSearchPath(search) == qfalse) {
            continue;
        }

        pack_t *const pack = search->pack;
        if (pack != NULL &&
            pack->checksum !=
                FS_RESTRICTED_DEMO_PAK0_CHECKSUM) {
            Com_Error(
                ERR_FATAL,
                "Corrupted pak0.pk3: %u",
                (uint32_t)pack->checksum);
        }
    }
}

/* Source: CoDUOMP.exe 0x00430ba0..0x00430be3.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00430ba0_00430be4.mcode.
 * Name and boolean argument: exact same-module Mac symbol
 * FS_ClearPakReferences. Cgame- and UI-module references are always cleared;
 * general and game-module references survive the client video restart but
 * are cleared for a complete filesystem/server restart. */
void FS_ClearPakReferences(qboolean preserveGeneralAndGameReferences)
{
    for (searchpath_t *search = fs_searchpaths;
         search != NULL;
         search = search->next) {
        pack_t *const pack = search->pack;
        if (pack == NULL)
            continue;

        pack->cgameModuleReference = 0;
        pack->uiModuleReference = 0;
        if (preserveGeneralAndGameReferences == qfalse) {
            pack->generalReference = 0;
            pack->gameModuleReference = 0;
        }
    }
}

/* Source: CoDUOMP.exe 0x00440f50..0x0044118e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00440f50_0044118f.mcode.
 * Name and return type: exact same-module Mac symbol
 * FS_ReferencedPakPureChecksums. The three passes consume the consecutive
 * cgame, UI, and general-reference bytes at pack offsets 0x312, 0x311, and
 * 0x310. Only the general pass includes every matching pack in the final XOR;
 * the first two passes stop after their first match. */
const char *FS_ReferencedPakPureChecksums(void)
{
    int32_t combinedChecksum = fs_checksumFeed;
    int32_t checksumCount = 0;

    fs_referencedPakPureChecksums[0] = '\0';

    for (int32_t referenceClass = FS_PURE_REFERENCE_CLASS_COUNT - 1;
         referenceClass >= 0;
         --referenceClass) {
        if (referenceClass == 0)
            strcat(fs_referencedPakPureChecksums, "@ ");

        for (searchpath_t *search = fs_searchpaths;
             search != NULL;
             search = search->next) {
            pack_t *const pack = search->pack;
            if (filesystem_compat_server_scope_allows_searchpath(search) ==
                    qfalse ||
                pack == NULL || search->localized != qfalse) {
                continue;
            }

            qboolean noted;
            if (referenceClass == 2)
                noted = pack->cgameModuleReference != 0;
            else if (referenceClass == 1)
                noted = pack->uiModuleReference != 0;
            else
                noted = pack->generalReference != 0;

            if (noted == qfalse)
                continue;

            Q_strcat(fs_referencedPakPureChecksums,
                     sizeof(fs_referencedPakPureChecksums),
                     va("%i ", pack->pureChecksum));

            if (referenceClass != 0)
                break;

            combinedChecksum ^= pack->pureChecksum;
            ++checksumCount;
        }

        if (fs_fakeChkSum != 0) {
            Q_strcat(fs_referencedPakPureChecksums,
                     sizeof(fs_referencedPakPureChecksums),
                     va("%i ", fs_fakeChkSum));
        }
    }

    Q_strcat(fs_referencedPakPureChecksums,
             sizeof(fs_referencedPakPureChecksums),
             va("%i ", combinedChecksum ^ checksumCount));
    return fs_referencedPakPureChecksums;
}

/* Source: CoDUOMP.exe 0x00441190..0x00441449.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00441190_0044144a.mcode.
 * Name and argument order: exact same-module Mac symbol
 * FS_PureServerSetLoadedPaks. The existing set comparison is
 * deliberately order-independent. Reinstalling a changed nonempty set stops
 * stream/room sound state while preserving active 2D and 3D channels. */
void FS_PureServerSetLoadedPaks(const char *checksumText,
                                    const char *nameText)
{
    int32_t checksums[FS_MAX_SERVER_PAKS];
    char *names[FS_MAX_SERVER_PAKS];

    Cmd_TokenizeString(checksumText);
    int32_t checksumCount = Cmd_Argc();
    if (checksumCount > FS_MAX_SERVER_PAKS)
        checksumCount = FS_MAX_SERVER_PAKS;

    for (int32_t pakIndex = 0;
         pakIndex < checksumCount;
         ++pakIndex) {
        checksums[pakIndex] = atoi(Cmd_Argv(pakIndex));
    }

    Cmd_TokenizeString(nameText);
    int32_t nameCount = Cmd_Argc();
    if (nameCount > FS_MAX_SERVER_PAKS)
        nameCount = FS_MAX_SERVER_PAKS;

    for (int32_t pakIndex = 0;
         pakIndex < nameCount;
         ++pakIndex) {
        names[pakIndex] = CopyStringInternal(Cmd_Argv(pakIndex));
    }

    if (checksumCount != nameCount)
        Com_Error(ERR_DROP, "pak sum/name mismatch");

    qboolean unchanged =
        checksumCount == fs_numServerPaks;
    if (unchanged != qfalse) {
        uint32_t matchedInstalled[FS_PURE_MATCH_WORD_COUNT] = {0};

        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        for (int32_t candidateIndex = 0;
             candidateIndex < checksumCount;
             ++candidateIndex) {
            qboolean matched = qfalse;
            for (int32_t installedIndex = 0;
                 installedIndex < fs_numServerPaks;
                 ++installedIndex) {
                const uint32_t matchMask =
                    UINT32_C(1) <<
                    ((uint32_t)installedIndex % FS_PURE_MATCH_WORD_BITS);
                uint32_t *const matchWord =
                    &matchedInstalled[(uint32_t)installedIndex /
                                      FS_PURE_MATCH_WORD_BITS];

                if ((*matchWord & matchMask) != 0)
                    continue;
                if (checksums[candidateIndex] ==
                        fs_serverPaks[installedIndex] &&
                    names[candidateIndex] != NULL &&
                    fs_serverPakNames[installedIndex] != NULL &&
                    Q_stricmp(
                        names[candidateIndex],
                        fs_serverPakNames[installedIndex]) == 0) {
                    *matchWord |= matchMask;
                    matched = qtrue;
                    break;
                }
            }
            if (matched == qfalse) {
                unchanged = qfalse;
                break;
            }
        }
    }

    if (unchanged != qfalse) {
        for (int32_t pakIndex = 0;
             pakIndex < nameCount;
             ++pakIndex) {
            Z_FreeInternal(names[pakIndex]);
        }
        return;
    }

    filesystem_compat_pure_set_changed();
    FS_ShutdownServerPakNames();
    fs_numServerPaks = checksumCount;

    if (checksumCount == 0)
        return;

    Com_DPrintf("Connected to a pure server.\n");
    memcpy(fs_serverPaks, checksums,
           (size_t)checksumCount * sizeof(checksums[0]));
    memcpy(fs_serverPakNames, names,
           (size_t)nameCount * sizeof(names[0]));
    fs_fakeChkSum = 0;
}

/* Source: CoDUOMP.exe 0x00441450..0x004415b1.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00441450_004415b2.mcode.
 * Name and argument order: exact same-module Mac symbol
 * FS_PureServerSetReferencedPaks. Existing owned names are released before the
 * replacement checksum list is parsed, matching the original update order. */
void FS_PureServerSetReferencedPaks(const char *checksumText,
                                const char *nameText)
{
    Cmd_TokenizeString(checksumText);
    int32_t checksumCount = Cmd_Argc();
    if (checksumCount > FS_MAX_SERVER_PAKS)
        checksumCount = FS_MAX_SERVER_PAKS;

    FS_ShutdownServerReferencedPaks();
    for (int32_t pakIndex = 0;
         pakIndex < checksumCount;
         ++pakIndex) {
        fs_serverReferencedPaks[pakIndex] = atoi(Cmd_Argv(pakIndex));
    }

    if (nameText == NULL || nameText[0] == '\0') {
        if (checksumCount != 0)
            Com_Error(ERR_DROP, "pak sum/name mismatch");
        fs_numServerReferencedPaks = checksumCount;
        return;
    }

    Cmd_TokenizeString(nameText);
    int32_t nameCount = Cmd_Argc();
    if (nameCount > FS_MAX_SERVER_PAKS)
        nameCount = FS_MAX_SERVER_PAKS;

    if (checksumCount != nameCount)
        Com_Error(ERR_DROP, "pak sum/name mismatch");

    for (int32_t pakIndex = 0;
         pakIndex < nameCount;
         ++pakIndex) {
        const char *const pakName = Cmd_Argv(pakIndex);

        /* NOT_FROM_ORIGINAL_SOURCE: validate every published pack token before
         * publishing owned name pointers to fixed-qpath classifiers. */
        if (strlen(pakName) >= MAX_QPATH) {
            Com_Error(
                ERR_DROP,
                "\x15" "FS_PureServerSetReferencedPaks: "
                "pak name %i exceeds MAX_QPATH",
                pakIndex);
        }
    }

    for (int32_t pakIndex = 0;
         pakIndex < nameCount;
         ++pakIndex) {
        fs_serverReferencedPakNames[pakIndex] =
            CopyStringInternal(Cmd_Argv(pakIndex));
    }

    fs_numServerReferencedPaks = checksumCount;
}
