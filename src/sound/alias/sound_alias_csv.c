#include <stdint.h>
#include <string.h>

#include "sound_alias_private.h"

enum {
    SOUND_ALIAS_MAX_CSV_COLUMNS = 256
};

/* CoDUOMP.exe 0x00437220..0x004375c1 and coduo_lnxded
 * 0x0806d851..0x0806dae7; canonical name confirmed by the supporting Mac
 * engine symbol. */
int32_t
Com_LoadSoundAliasFile(const char *csvPath, const char *sourceName,
                       int32_t count, qboolean defaultLoadspec)
{
    void *fileBuffer;
    char *parseCursor;
    char *token;
    int32_t columnMap[SOUND_ALIAS_MAX_CSV_COLUMNS];
    uint8_t seenColumns[SND_ALIAS_FIELD_COUNT];
    snd_alias_parse_node_t parseNode;
    int32_t result;
    int columnCount;
    int column;
    qboolean hasNameColumn;
    qboolean hasFileColumn;

    if (FS_ReadFile(csvPath, &fileBuffer) < 0) {
        return count;
    }

    Com_BeginParseSession(csvPath);
    Com_SetCSV(qtrue);
    parseCursor = fileBuffer;
    columnCount = 0;

    while (1) {
        token = Com_Parse(&parseCursor);
        if (parseCursor == NULL) {
            Com_EndParseSession();
            result = count;
            return result;
        }

        if (token[0] == '\0' || token[0] == '#') {
            Com_SkipRestOfLine(&parseCursor);
            continue;
        }

        if (columnCount == 0) {
            hasNameColumn = qfalse;
            hasFileColumn = qfalse;

            do {
                columnMap[columnCount] =
                    SND_ALIAS_FIELD_UNKNOWN;

                for (column = SND_ALIAS_FIELD_NAME;
                     column < SND_ALIAS_FIELD_COUNT;
                     column++) {
                    if (Q_stricmp(soundAliasFieldNames[column],
                                  token) == 0) {
                        columnMap[columnCount] = column;
                        if (column == SND_ALIAS_FIELD_NAME) {
                            hasNameColumn = qtrue;
                        } else if (column ==
                                   SND_ALIAS_FIELD_FILE) {
                            hasFileColumn = qtrue;
                        }
                        break;
                    }
                }

                columnCount++;
                if (columnCount == SOUND_ALIAS_MAX_CSV_COLUMNS ||
                    parseCursor == NULL || parseCursor[0] == '\n') {
                    break;
                }

                token = Com_ParseOnLine(&parseCursor);
            } while (1);

            if (!hasNameColumn || !hasFileColumn) {
                Com_Error(ERR_DROP,
                          "\x15" "Sound alias file %s: missing 'name' "
                                 "and/or 'file' columns\n",
                          com_soundAliasCurrentFile);
            }

            Com_SkipRestOfLine(&parseCursor);
            continue;
        }

        memset(seenColumns, 0, sizeof(seenColumns));
        Com_LoadSoundAliasDefaults(&parseNode, defaultLoadspec);

        column = 0;
        while (1) {
            if (token[0] != '\0') {
                Com_LoadSoundAliasField(sourceName, token,
                                          columnMap[column], seenColumns,
                                          &parseNode);
            }

            column++;
            if (column == columnCount) {
                break;
            }

            token = Com_ParseOnLine(&parseCursor);
        }

        if (seenColumns[SND_ALIAS_FIELD_NAME] == 0 ||
            seenColumns[SND_ALIAS_FIELD_FILE] == 0) {
            Com_Error(ERR_DROP,
                      "\x15" "Sound alias file %s: alias entry missing name "
                             "and/or file\n",
                      com_soundAliasCurrentFile);
        }

        if (parseNode.matchesLoadSpecification != 0) {
            Com_FinishBuildingSoundAlias(&parseNode);
            Com_AddBuildSoundAlias(&parseNode);
            count++;
        }

        Com_SkipRestOfLine(&parseCursor);
    }
}

/* CoDUOMP.exe 0x004375d0..0x00437777 and coduo_lnxded
 * 0x0806dae7..0x0806dcf2; canonical name confirmed by the supporting Mac
 * engine symbol. */
snd_alias_parse_node_t *
Com_SortTempSoundAliases_r(snd_alias_parse_node_t *head,
                            int32_t *count)
{
    int32_t leftCount;
    int32_t rightCount;
    int32_t index;
    snd_alias_parse_node_t *rightHead;
    snd_alias_parse_node_t *leftSorted;
    snd_alias_parse_node_t *rightSorted;
    snd_alias_parse_node_t *mergedHead;
    snd_alias_parse_node_t *tailNode;
    snd_alias_parse_node_t *selectedNode;
    int compare;
    int sourceCompare;

    if (*count == 1) {
        head->next = 0;
        return head;
    }

    leftCount = *count / 2;
    rightCount = *count - leftCount;
    rightHead = head;
    for (index = 0; index < leftCount; index++) {
        rightHead = rightHead->next;
    }

    leftSorted = Com_SortTempSoundAliases_r(head, &leftCount);
    rightSorted = Com_SortTempSoundAliases_r(rightHead, &rightCount);
    *count = 0;
    mergedHead = 0;
    tailNode = 0;

    while (1) {
        while (leftCount != 0 && rightCount != 0) {
            compare = Q_stricmp(leftSorted->aliasName,
                                rightSorted->aliasName);
            if (compare == 0) {
                compare = leftSorted->sequence - rightSorted->sequence;
                if (compare == 0) {
                    sourceCompare =
                        Q_stricmp(leftSorted->sourceFile,
                                  rightSorted->sourceFile);
                    if (sourceCompare == 0) {
                        Com_Error(ERR_DROP,
                                  "\x15" "sound alias file %s: duplicate "
                                         "alias '%s'\n",
                                  leftSorted->sourceFile,
                                  leftSorted->aliasName);
                        compare = 0;
                    } else if (sourceCompare < 0) {
                        leftSorted = leftSorted->next;
                        leftCount--;
                        continue;
                    } else {
                        rightSorted = rightSorted->next;
                        rightCount--;
                        continue;
                    }
                }
            }

            if (compare < 0) {
                selectedNode = leftSorted;
                leftSorted = leftSorted->next;
                leftCount--;
            } else {
                selectedNode = rightSorted;
                rightSorted = rightSorted->next;
                rightCount--;
            }

            if (mergedHead == NULL) {
                mergedHead = selectedNode;
            } else {
                tailNode->next = selectedNode;
            }
            tailNode = selectedNode;

            (*count)++;
        }

        if (leftCount == 0) {
            if (mergedHead == NULL) {
                mergedHead = rightSorted;
            } else {
                tailNode->next = rightSorted;
            }
            *count += rightCount;
        } else {
            if (mergedHead == NULL) {
                mergedHead = leftSorted;
            } else {
                tailNode->next = leftSorted;
            }
            *count += leftCount;
        }

        return mergedHead;
    }
}
