#include <stdint.h>
#include <string.h>
#include "sound_alias_private.h"

enum {
    SOUND_ALIAS_CHECKSUM_MULTIPLIER = 31337
};

/* CoDUOMP.exe 0x00437780..0x00437a62 and coduo_lnxded
 * 0x0806dcf2..0x0806e081; canonical name confirmed by the supporting Mac
 * engine symbol. */
void Com_MakeSoundAliasesPermanent(int32_t count, sndAliasBank_t bank)
{
    snd_alias_parse_node_t *node;
    snd_alias_parse_node_t *scan;
    snd_alias_parse_node_t *duplicateFileNode;
    snd_alias_t *table;
    char *stringBase;
    char *stringCursor;
    char *currentName;
    char *file;
    char *subtitle;
    size_t length;
    int32_t recordIndex;
    int stringBytes;
    int sharedBytes;
    uint32_t checksum;
    int byteIndex;

    com_soundAliasBuildList = Com_SortTempSoundAliases_r(com_soundAliasBuildList, &count);

    stringBytes = 0;
    sharedBytes = 0;
    currentName = NULL;
    for (node = com_soundAliasBuildList; node != NULL; node = node->next) {
        length = strlen(node->aliasName) + 1;
        if (currentName == NULL || Q_stricmp(currentName, node->aliasName) != 0) {
            stringBytes += (int)length;
            currentName = node->aliasName;
        } else {
            sharedBytes += (int)length;
        }

        if (node->subtitle[0] != '\0') {
            stringBytes += (int)strlen(node->subtitle) + 1;
        }

        length = strlen(node->soundFile) + 1;
        for (scan = com_soundAliasBuildList; scan != node; scan = scan->next) {
            if (Q_stricmp(scan->soundFile, node->soundFile) == 0) {
                sharedBytes += (int)length;
                node->duplicateFileNode = scan;
                goto next_node;
            }
        }

        node->duplicateFileNode = NULL;
        stringBytes += (int)length;

    next_node:;
    }

    /* Linux 0x0806de38/0x0806de47 keeps the FILD byte counts exact through
     * division by 1024; Windows uses the equivalent exact power-of-two scale.
     * Widening the int first preserves that result on non-x87 hosts too. */
    Com_DPrintf("Sound alias strings use %.1f KB; %.1f KB saved by string sharing\n", (double)stringBytes * 0.0009765625,
                (double)sharedBytes * 0.0009765625);

    table = Z_MallocInternal((size_t)count * sizeof(*table) + (size_t)stringBytes);
    stringBase = (char *)&table[count];
    stringCursor = stringBase;
    currentName = NULL;
    recordIndex = 0;

    for (node = com_soundAliasBuildList; node != NULL; node = node->next) {
        if (currentName == NULL || Q_stricmp(currentName, node->aliasName) != 0) {
            currentName = stringCursor;
            strcpy(stringCursor, node->aliasName);
            stringCursor += strlen(stringCursor) + 1;
        }

        if (node->subtitle[0] == '\0') {
            subtitle = NULL;
        } else {
            subtitle = stringCursor;
            strcpy(stringCursor, node->subtitle);
            stringCursor += strlen(stringCursor) + 1;
        }

        if (node->duplicateFileNode == NULL) {
            file = stringCursor;
            strcpy(stringCursor, node->soundFile);
            stringCursor += strlen(stringCursor) + 1;
        } else {
            duplicateFileNode = node->duplicateFileNode;
            file = (char *)duplicateFileNode->permanentSoundFile;
        }
        node->permanentSoundFile = file;

        Com_AddSoundAlias(node, &table[recordIndex], currentName, file, subtitle, bank);
        recordIndex++;
    }

    com_soundAliases[bank] = table;
    com_soundAliasCount[bank] = count;
    /*
     * The checksum recurrence (imul reg,reg,0x7a69 then add) relies on 32-bit
     * two's-complement wraparound. Accumulate in uint32_t so the overflow is
     * defined; the low 32 bits are bit-identical to the signed imul/add the
     * original computes, and the store back into the int32_t table preserves
     * the bit pattern.
     */
    checksum = (uint32_t)count * (uint32_t)stringBytes;
    com_soundAliasChecksum[bank] = (int32_t)checksum;
    for (byteIndex = 0; byteIndex < stringBytes; byteIndex++) {
        checksum = (uint32_t)com_soundAliasChecksum[bank] * SOUND_ALIAS_CHECKSUM_MULTIPLIER;
        checksum += (uint32_t)(signed char)stringBase[byteIndex];
        com_soundAliasChecksum[bank] = (int32_t)checksum;
    }
}
