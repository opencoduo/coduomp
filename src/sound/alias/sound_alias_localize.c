#include "sound_alias_private.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Sources: CoDUOMP.exe 0x004389f0..0x00438a88 and coduo_lnxded
 * 0x0806eb5f..0x0806ec2a.
 * Name: same-module Mac symbol Com_WriteSoundAliasSubtitleEntry. */
void Com_WriteSoundAliasSubtitleEntry(const char *reference, const char *englishText, int32_t fileHandle)
{
    static const char referencePrefix[] = "REFERENCE           ";
    static const char englishPrefix[] = "\r\nLANG_ENGLISH        \"";
    static const char entrySuffix[] = "\"\r\n\r\n";

    (void)FS_Write(referencePrefix, (int32_t)strlen(referencePrefix), fileHandle);
    (void)FS_Write(reference, (int32_t)strlen(reference), fileHandle);
    (void)FS_Write(englishPrefix, (int32_t)strlen(englishPrefix), fileHandle);
    (void)FS_Write(englishText, (int32_t)strlen(englishText), fileHandle);
    (void)FS_Write(entrySuffix, (int32_t)strlen(entrySuffix), fileHandle);
}

/* Sources: CoDUOMP.exe 0x00438a90..0x00438e2b and coduo_lnxded
 * 0x0806ec2b..0x0806ef7d.
 * Name: same-module Mac symbol Com_UpdateSoundAliasSubtitleFile. The existing
 * StringEd file is rewritten through temp.st so an existing reference can be
 * replaced without disturbing unrelated entries or the final ENDMARKER. */
void Com_UpdateSoundAliasSubtitleFile(const char *subtitle, const char *englishText)
{
    static const char subtitleFile[] = "soundaliases/subtitle.st";
    static const char temporaryFile[] = "soundaliases/temp.st";
    static const char referenceToken[] = "REFERENCE";
    static const char endMarkerToken[] = "ENDMARKER";
    static const char endMarkerSuffix[] = "\r\nENDMARKER\r\n\r\n\r\n";
    enum {
        END_MARKER_LINE_ENDING_LENGTH = 2,
        END_MARKER_WRITE_TRIM = sizeof(endMarkerToken) - 1 + END_MARKER_LINE_ENDING_LENGTH
    };
    const char *const reference = subtitle + 9;
    qboolean found = qfalse;

    const int32_t temporaryHandle = FS_FOpenFileWrite(temporaryFile);
    if (temporaryHandle == 0) {
        Com_Printf("WARNING: Could not open output file %s for writing\n", temporaryFile);
        return;
    }

    void *subtitleBuffer;
    if (FS_ReadFile(subtitleFile, &subtitleBuffer) < 0) {
        Com_Printf("WARNING: Could not read local copy of StringEd file %s\n", subtitleFile);
        FS_FCloseFile(temporaryHandle);
        return;
    }

    Com_BeginParseSession(subtitleFile);
    char *parseCursor = subtitleBuffer;
    char *writeStart = subtitleBuffer;
    qboolean malformedEndMarker = qfalse;

    for (;;) {
        char *token = Com_Parse(&parseCursor);
        if (parseCursor == NULL)
            break;

        if (strncmp(token, endMarkerToken, sizeof(endMarkerToken)) == 0) {
            if (writeStart < parseCursor) {
                const ptrdiff_t markerSpan = parseCursor - writeStart;

                /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
                if (markerSpan < END_MARKER_WRITE_TRIM) {
                    Com_Printf("WARNING: malformed ENDMARKER in %s\n", subtitleFile);
                    malformedEndMarker = qtrue;
                    break;
                }
                const int32_t writeLength = (int32_t)markerSpan - END_MARKER_WRITE_TRIM;
                (void)FS_Write(writeStart, writeLength, temporaryHandle);
            }
            break;
        }

        if (strncmp(token, referenceToken, sizeof(referenceToken)) == 0) {
            token = Com_ParseOnLine(&parseCursor);
            if (strcmp(token, reference) == 0) {
                if (writeStart < parseCursor) {
                    (void)FS_Write(writeStart, (int32_t)(parseCursor - writeStart), temporaryHandle);
                }

                Com_WriteSoundAliasSubtitleEntry(reference, englishText, temporaryHandle);
                found = qtrue;

                do {
                    writeStart = parseCursor;
                    token = Com_Parse(&parseCursor);
                    if (parseCursor == NULL) {
                        writeStart = NULL;
                        break;
                    }
                } while (strncmp(token, referenceToken, sizeof(referenceToken)) != 0 &&
                         strncmp(token, endMarkerToken, sizeof(endMarkerToken)) != 0);

                if (parseCursor != NULL)
                    Com_UngetToken();
            }
        }

        Com_SkipRestOfLine(&parseCursor);
    }

    if (malformedEndMarker) {
        char temporaryOSPath[MAX_OSPATH];

        Com_EndParseSession();
        FS_FreeFile(subtitleBuffer);
        FS_FCloseFile(temporaryHandle);
        FS_BuildOSPath(fs_basepath->string, fs_currentGameDir, temporaryFile, temporaryOSPath);
        FS_Remove(temporaryOSPath);
        return;
    }

    if (!found) {
        Com_WriteSoundAliasSubtitleEntry(reference, englishText, temporaryHandle);
    }

    Com_EndParseSession();
    FS_FreeFile(subtitleBuffer);
    (void)FS_Write(endMarkerSuffix, (int32_t)strlen(endMarkerSuffix), temporaryHandle);
    FS_FCloseFile(temporaryHandle);

    char temporaryOSPath[MAX_OSPATH];
    char subtitleOSPath[MAX_OSPATH];
    FS_BuildOSPath(fs_basepath->string, fs_currentGameDir, temporaryFile, temporaryOSPath);
    FS_BuildOSPath(fs_basepath->string, fs_currentGameDir, subtitleFile, subtitleOSPath);
    FS_Copyfiles(temporaryOSPath, subtitleOSPath);
    FS_Remove(temporaryOSPath);
}

/* Sources: CoDUOMP.exe 0x00438e30..0x004395c5 and coduo_lnxded
 * 0x0806ef7e..0x0806fa61.
 * Name: same-module Mac symbol Com_LocalizeSoundAliasCsvFile. The second path
 * argument is present in the source ABI but is never read by this body. */
void Com_LocalizeSoundAliasCsvFile(const char *csvPath, const char *unusedPath)
{
    static const char temporaryCsv[] = "soundaliases/temp.csv";
    static const char subtitlePrefix[] = "SUBTITLE_";
    enum {
        LOCALIZE_MAX_COLUMNS = 256,
        LOCALIZE_FIELD_CAPACITY = 1024,
        LOCALIZE_COMPARE_LIMIT = 99999
    };
    int32_t columnMap[LOCALIZE_MAX_COLUMNS];
    uint8_t seenFields[SND_ALIAS_FIELD_COUNT];
    snd_alias_parse_node_t parseNode;
    char cellValues[SND_ALIAS_FIELD_COUNT][LOCALIZE_FIELD_CAPACITY];
    char subtitleReference[LOCALIZE_FIELD_CAPACITY];
    char writableOSPath[MAX_OSPATH];
    char temporaryOSPath[MAX_OSPATH];
    char csvOSPath[MAX_OSPATH];
    int32_t columnCount = 0;
    int32_t localizedCount = 0;

    (void)unusedPath;
    FS_BuildOSPath(fs_basepath->string, fs_currentGameDir, csvPath, writableOSPath);
    Com_Printf("Processing sound alias file %s..\n", writableOSPath);

    FILE *writableProbe = fopen(writableOSPath, "r+");
    if (writableProbe == NULL) {
        Com_Printf("WARNING: Can not write to sound alias file %s\n", writableOSPath);
        return;
    }
    fclose(writableProbe);

    char *fileBuffer;
    if (FS_ReadFile(csvPath, (void **)&fileBuffer) < 0) {
        Com_Printf("WARNING: Could not read sound alias file %s\n", csvPath);
        return;
    }

    const int32_t outputHandle = FS_FOpenFileWrite(temporaryCsv);
    if (outputHandle == 0) {
        Com_Printf("WARNING: Could not open output file %s for writing\n", temporaryCsv);
        return;
    }

    Com_BeginParseSession(csvPath);
    Com_SetCSV(qtrue);
    char *parseCursor = fileBuffer;

    while (parseCursor != NULL) {
        while (*parseCursor == '\r')
            ++parseCursor;

        if (*parseCursor == '\n') {
            ++parseCursor;
            (void)FS_Write("\r\n", 2, outputHandle);
        }

        char *const rowStart = parseCursor;
        char *token = Com_Parse(&parseCursor);
        if (parseCursor == NULL)
            break;

        if (token[0] == '\0' || token[0] == '#') {
            Com_SkipRestOfLine(&parseCursor);
            if (rowStart[0] == '\n')
                (void)FS_Write("\r", 1, outputHandle);
            (void)FS_Write(rowStart, (int32_t)(parseCursor - rowStart), outputHandle);
            continue;
        }

        if (columnCount == 0) {
            qboolean hasNameColumn = qfalse;
            qboolean hasFileColumn = qfalse;

            do {
                columnMap[columnCount] = SND_ALIAS_FIELD_UNKNOWN;

                for (int32_t field = SND_ALIAS_FIELD_NAME; field < SND_ALIAS_FIELD_COUNT; ++field) {
                    if (Q_stricmpn(soundAliasFieldNames[field], token, LOCALIZE_COMPARE_LIMIT) == 0) {
                        columnMap[columnCount] = field;
                        if (field == SND_ALIAS_FIELD_NAME)
                            hasNameColumn = qtrue;
                        else if (field == SND_ALIAS_FIELD_FILE)
                            hasFileColumn = qtrue;
                        break;
                    }
                }

                ++columnCount;
                if (columnCount == LOCALIZE_MAX_COLUMNS || parseCursor == NULL || parseCursor[0] == '\n') {
                    break;
                }
                token = Com_ParseOnLine(&parseCursor);
            } while (qtrue);

            if (!hasNameColumn || !hasFileColumn) {
                Com_Error(1,
                          "\x15Sound alias file %s: missing 'name' "
                          "and/or 'file' columns\n",
                          com_soundAliasCurrentFile);
            }

            Com_SkipRestOfLine(&parseCursor);
            if (rowStart[0] == '\n')
                (void)FS_Write("\r", 1, outputHandle);
            (void)FS_Write(rowStart, (int32_t)(parseCursor - rowStart), outputHandle);
            continue;
        }

        memset(seenFields, 0, sizeof(seenFields));
        Com_LoadSoundAliasDefaults(&parseNode, qfalse);

        for (int32_t column = 0; column < columnCount; ++column) {
            const sndAliasField_t field = (sndAliasField_t)columnMap[column];
            strcpy(cellValues[field], token);
            if (token[0] != '\0') {
                Com_LoadSoundAliasField("", token, field, seenFields, &parseNode);
            }
            if (column != columnCount - 1)
                token = Com_ParseOnLine(&parseCursor);
        }

        if (!seenFields[SND_ALIAS_FIELD_NAME] || !seenFields[SND_ALIAS_FIELD_FILE]) {
            Com_Error(1,
                      "\x15Sound alias file %s: alias entry missing "
                      "name and/or file\n",
                      com_soundAliasCurrentFile);
        }

        qboolean shouldLocalize = qfalse;
        if (seenFields[SND_ALIAS_FIELD_SUBTITLE]) {
            const char *const subtitle = cellValues[SND_ALIAS_FIELD_SUBTITLE];
            const int32_t subtitleLength = (int32_t)strlen(subtitle);
            int32_t characterIndex = 0;

            while (characterIndex < subtitleLength) {
                const char character = subtitle[characterIndex];
                if (!((character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9') || character == '_')) {
                    break;
                }
                ++characterIndex;
            }

            if (characterIndex < subtitleLength || strncmp(subtitle, subtitlePrefix, sizeof(subtitlePrefix) - 1) != 0 ||
                !Com_SoundAliasSubtitleReferenceExists(subtitle)) {
                shouldLocalize = qtrue;
            }
        }

        if (!shouldLocalize) {
            Com_SkipRestOfLine(&parseCursor);
            (void)FS_Write(rowStart, (int32_t)(parseCursor - rowStart), outputHandle);
            continue;
        }

        for (int32_t column = 0; column < columnCount; ++column) {
            const sndAliasField_t field = (sndAliasField_t)columnMap[column];

            if (field == SND_ALIAS_FIELD_UNKNOWN || !seenFields[field]) {
                if (column != columnCount - 1)
                    (void)FS_Write(",", 1, outputHandle);
                continue;
            }

            if (field == SND_ALIAS_FIELD_SUBTITLE) {
                const char *outputText = Com_SoundAliasSubtitleReferenceForText(cellValues[SND_ALIAS_FIELD_SUBTITLE]);

                if (outputText == NULL) {
                    if (seenFields[SND_ALIAS_FIELD_SEQUENCE]) {
                        Com_sprintf(subtitleReference, sizeof(subtitleReference), "%s%s_%s", subtitlePrefix,
                                    cellValues[SND_ALIAS_FIELD_NAME], cellValues[SND_ALIAS_FIELD_SEQUENCE]);
                    } else {
                        Com_sprintf(subtitleReference, sizeof(subtitleReference), "%s%s", subtitlePrefix, cellValues[SND_ALIAS_FIELD_NAME]);
                    }
                    outputText = Q_strupr(subtitleReference);
                    Com_UpdateSoundAliasSubtitleFile(subtitleReference, cellValues[SND_ALIAS_FIELD_SUBTITLE]);
                    ++localizedCount;
                } else {
                    Com_sprintf(subtitleReference, sizeof(subtitleReference), "%s%s", subtitlePrefix, outputText);
                    outputText = Q_strupr(subtitleReference);
                }

                (void)FS_Write(outputText, (int32_t)strlen(outputText), outputHandle);
                continue;
            }

            const char *const cellText = cellValues[field];
            const qboolean needsQuotes = strchr(cellText, ',') != NULL || strchr(cellText, ' ') != NULL || strchr(cellText, '\n') != NULL ||
                                         strchr(cellText, '\r') != NULL;
            const char *outputText;

            if (column == columnCount - 1) {
                outputText = needsQuotes ? va("\"%s\"", cellText) : va("%s", cellText);
            } else {
                outputText = needsQuotes ? va("\"%s\",", cellText) : va("%s,", cellText);
            }

            (void)FS_Write(outputText, (int32_t)strlen(outputText), outputHandle);
        }

        (void)FS_Write("\r\n", 2, outputHandle);
        Com_SkipRestOfLine(&parseCursor);
    }

    Com_EndParseSession();
    FS_FCloseFile(outputHandle);
    FS_BuildOSPath(fs_basepath->string, fs_currentGameDir, temporaryCsv, temporaryOSPath);
    FS_BuildOSPath(fs_basepath->string, fs_currentGameDir, csvPath, csvOSPath);
    if (localizedCount != 0)
        FS_Copyfiles(temporaryOSPath, csvOSPath);
    FS_Remove(temporaryOSPath);
    Com_Printf("Localized %i sound alias subtitles\n", localizedCount);
}

/* Sources: CoDUOMP.exe 0x004395d0..0x0043968c and coduo_lnxded
 * 0x0806fa62..0x0806fb9a.
 * Name: same-module Mac symbol COM_WriteFinalStringEdFile. MSVC passes the
 * source path in EAX as an internal calling optimization; maintained source
 * exposes the ordinary two-path source signature. */
void COM_WriteFinalStringEdFile(const char *sourcePath, const char *destinationPath)
{
    FILE *source = fopen(sourcePath, "rb");
    if (source == NULL)
        return;

    (void)fseek(source, 0, SEEK_END);
    const long byteCount = ftell(source);
    (void)fseek(source, 0, SEEK_SET);

    void *const buffer = malloc((size_t)byteCount);
    if (fread(buffer, 1, (size_t)byteCount, source) != (size_t)byteCount) {
        Com_Error(0, "\x15Short read in COM_WriteFinalStringEdFile()\n");
    }
    (void)fclose(source);

    FILE *destination = fopen(destinationPath, "wb");
    if (destination == NULL) {
        free(buffer);
        return;
    }

    if (fwrite(buffer, 1, (size_t)byteCount, destination) != (size_t)byteCount) {
        Com_Error(0, "\x15Short write in COM_WriteFinalStringEdFile()\n");
    }
    (void)fclose(destination);
    free(buffer);
}
