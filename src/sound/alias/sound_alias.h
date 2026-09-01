#ifndef SHARED_SOUND_ALIAS_H
#define SHARED_SOUND_ALIAS_H

#include "qcommon/q_shared_types.h"
#include "qcommon/q_vector_types.h"
#include "qcommon/sound_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void Com_LoadSoundAliases(const char *aliasFile,
                          sndAliasBank_t bank);

snd_alias_t *Com_GetSoundAlias(int32_t soundHandle,
                               sndAliasBank_t bank);
snd_alias_t *Com_PickSoundAlias(const char *name,
                                sndAliasBank_t bank,
                                const vec3_t origin);
int32_t Com_SoundAliasIndex(const snd_alias_t *alias,
                            sndAliasBank_t bank);
const char *Com_SoundAliasString(const char *name, sndAliasBank_t bank);
int32_t Com_SoundAliasChecksum(sndAliasBank_t bank);
qboolean Com_SoundAliasSubtitleReferenceExists(
    const char *subtitle);
const char *Com_SoundAliasSubtitleReferenceForText(
    const char *englishText);

void Com_WriteSoundAliasSubtitleEntry(
    const char *reference, const char *englishText,
    int32_t fileHandle);
void Com_UpdateSoundAliasSubtitleFile(
    const char *subtitle, const char *englishText);
void Com_LocalizeSoundAliasCsvFile(
    const char *csvPath, const char *unusedPath);
void COM_WriteFinalStringEdFile(
    const char *sourcePath, const char *destinationPath);
void Com_UnloadSoundAliases(sndAliasBank_t bank);
uint32_t Com_HashAliasName(const char *name);
qboolean Com_IsValidAliasName(const char *name);
qboolean Com_ValidateSoundAliasLOD(const snd_alias_t *alias,
                                   float selector);
snd_alias_t *Com_FindSoundAlias(const char *name,
                                sndAliasBank_t bank,
                                float selector);
sndAliasChannel_t Com_SoundAliasChannelForName(const char *name);
sndAliasType_t Com_SoundAliasTypeForName(const char *name);
qboolean Com_SoundAliasLoop(const char *name);
qboolean Com_SoundAliasLoadSpec(const char *sourceFile,
                                const char *text);
void Com_SoundAliasMasterSlave(const char *text,
                               snd_alias_parse_node_t *node);
void Com_LoadSoundAliasField(
    const char *sourceFile, const char *text,
    sndAliasField_t field,
    uint8_t seenFields[SND_ALIAS_FIELD_COUNT],
    snd_alias_parse_node_t *node);
void Com_FinishBuildingSoundAlias(snd_alias_parse_node_t *node);
snd_alias_parse_node_t *
Com_AddBuildSoundAlias(const snd_alias_parse_node_t *node);
void Com_AddSoundAlias(const snd_alias_parse_node_t *node,
                       snd_alias_t *alias,
                       const char *aliasName,
                       const char *soundFile,
                       const char *subtitle,
                       sndAliasBank_t bank);
int32_t Com_LoadSoundAliasFile(const char *csvPath,
                               const char *sourceFile,
                               int32_t aliasCount,
                               qboolean defaultLoadSpecification);
snd_alias_parse_node_t *
Com_SortTempSoundAliases_r(snd_alias_parse_node_t *head,
                           int32_t *aliasCount);
void Com_MakeSoundAliasesPermanent(int32_t aliasCount,
                                   sndAliasBank_t bank);
int32_t Com_AddFileToList(const char *fileName,
                          char **fileList,
                          int32_t fileCount);
void Com_FreeFileList(char **fileList);
char **Com_ParseLoadSpecFile(const char *loadSpecPath,
                             int32_t *fileCount);
void Com_LoadSoundAliasSounds(sndAliasBank_t bank);
void Com_UnloadSoundAliasSounds(sndAliasBank_t bank);
void Com_LoadSoundAliasDefaults(snd_alias_parse_node_t *node,
                                qboolean defaultLoadSpecification);
void Com_LoadedSoundList(sndAliasBank_t bank);
void Com_StreamedSoundList(sndAliasBank_t bank);
void Com_SoundList_f(void);

extern uint8_t com_soundAliasBankActive[SND_ALIAS_BANK_COUNT];
extern snd_alias_t
    *com_soundAliasHash[SND_ALIAS_BANK_COUNT][SND_ALIAS_HASH_BUCKET_COUNT];
extern snd_alias_t *com_soundAliases[SND_ALIAS_BANK_COUNT];
extern int32_t com_soundAliasCount[SND_ALIAS_BANK_COUNT];
extern uint32_t com_soundAliasChecksum[SND_ALIAS_BANK_COUNT];
extern char com_soundAliasSubtitleReference[
    SND_ALIAS_SUBTITLE_REFERENCE_CAPACITY];
extern char *com_soundAliasCurrentFile;
extern char
    com_soundAliasLocalizedSource[SND_ALIAS_SOURCE_NAME_CAPACITY];
extern snd_alias_parse_node_t *com_soundAliasBuildList;
extern const char *const
    soundAliasFieldNames[SND_ALIAS_FIELD_COUNT];

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(snd_alias_sound_file_t) == 0x04,
               "i386 loaded sound-info alignment changed");
_Static_assert(offsetof(snd_alias_sound_file_t, formatTag) == 0x00,
               "i386 loaded sound format-tag offset changed");
_Static_assert(sizeof(((snd_alias_sound_file_t *)0)->formatTag) == 0x04,
               "i386 loaded sound format-tag extent changed");
_Static_assert(offsetof(snd_alias_sound_file_t, data) == 0x04,
               "i386 loaded sound data-pointer offset changed");
_Static_assert(sizeof(((snd_alias_sound_file_t *)0)->data) == 0x04,
               "i386 loaded sound data-pointer extent changed");
_Static_assert(offsetof(snd_alias_sound_file_t, dataLength) == 0x08,
               "i386 loaded sound data-length offset changed");
_Static_assert(sizeof(((snd_alias_sound_file_t *)0)->dataLength) == 0x04,
               "i386 loaded sound data-length extent changed");
_Static_assert(offsetof(snd_alias_sound_file_t, sampleRate) == 0x0c,
               "i386 loaded sound sample-rate offset changed");
_Static_assert(sizeof(((snd_alias_sound_file_t *)0)->sampleRate) == 0x04,
               "i386 loaded sound sample-rate extent changed");
_Static_assert(offsetof(snd_alias_sound_file_t, bitsPerSample) == 0x10,
               "i386 loaded sound sample-bits offset changed");
_Static_assert(sizeof(((snd_alias_sound_file_t *)0)->bitsPerSample) == 0x04,
               "i386 loaded sound sample-bits extent changed");
_Static_assert(offsetof(snd_alias_sound_file_t, channelCount) == 0x14,
               "i386 loaded sound channel-count offset changed");
_Static_assert(sizeof(((snd_alias_sound_file_t *)0)->channelCount) == 0x04,
               "i386 loaded sound channel-count extent changed");
_Static_assert(offsetof(snd_alias_sound_file_t, sampleCount) == 0x18,
               "i386 loaded sound sample-count offset changed");
_Static_assert(sizeof(((snd_alias_sound_file_t *)0)->sampleCount) == 0x04,
               "i386 loaded sound sample-count extent changed");
_Static_assert(offsetof(snd_alias_sound_file_t, blockSize) == 0x1c,
               "i386 loaded sound block-size offset changed");
_Static_assert(sizeof(((snd_alias_sound_file_t *)0)->blockSize) == 0x04,
               "i386 loaded sound block-size extent changed");
_Static_assert(offsetof(snd_alias_sound_file_t, initialData) == 0x20,
               "i386 loaded sound initial-data offset changed");
_Static_assert(sizeof(((snd_alias_sound_file_t *)0)->initialData) == 0x04,
               "i386 loaded sound initial-data extent changed");
_Static_assert(sizeof(snd_alias_sound_file_t) == 0x24,
               "original i386 loaded sound-info size changed");

_Static_assert(_Alignof(snd_alias_t) == 0x04,
               "i386 snd_alias_t alignment changed");
_Static_assert(offsetof(snd_alias_t, aliasName) == 0x00,
               "i386 snd_alias_t name offset changed");
_Static_assert(sizeof(((snd_alias_t *)0)->aliasName) == 0x04,
               "i386 snd_alias_t name extent changed");
_Static_assert(offsetof(snd_alias_t, soundFile) == 0x04,
               "i386 snd_alias_t sound-file offset changed");
_Static_assert(sizeof(((snd_alias_t *)0)->soundFile) == 0x04,
               "i386 snd_alias_t sound-file extent changed");
_Static_assert(offsetof(snd_alias_t, subtitle) == 0x08,
               "i386 snd_alias_t subtitle offset changed");
_Static_assert(sizeof(((snd_alias_t *)0)->subtitle) == 0x04,
               "i386 snd_alias_t subtitle extent changed");
_Static_assert(offsetof(snd_alias_t, soundFileInfo) == 0x0c,
               "i386 snd_alias_t sound-file-info offset changed");
_Static_assert(sizeof(((snd_alias_t *)0)->soundFileInfo) == 0x04,
               "i386 snd_alias_t sound-file-info extent changed");
_Static_assert(offsetof(snd_alias_t, pickSequence) == 0x10,
               "i386 snd_alias_t pick-sequence offset changed");
_Static_assert(sizeof(((snd_alias_t *)0)->pickSequence) == 0x04,
               "i386 snd_alias_t pick-sequence extent changed");
_Static_assert(offsetof(snd_alias_t, volumeMin) == 0x14,
               "i386 snd_alias_t minimum-volume offset changed");
_Static_assert(sizeof(((snd_alias_t *)0)->volumeMin) == 0x04,
               "i386 snd_alias_t minimum-volume extent changed");
_Static_assert(offsetof(snd_alias_t, volumeMax) == 0x18,
               "i386 snd_alias_t maximum-volume offset changed");
_Static_assert(sizeof(((snd_alias_t *)0)->volumeMax) == 0x04,
               "i386 snd_alias_t maximum-volume extent changed");
_Static_assert(offsetof(snd_alias_t, pitchMin) == 0x1c,
               "i386 snd_alias_t minimum-pitch offset changed");
_Static_assert(sizeof(((snd_alias_t *)0)->pitchMin) == 0x04,
               "i386 snd_alias_t minimum-pitch extent changed");
_Static_assert(offsetof(snd_alias_t, pitchMax) == 0x20,
               "i386 snd_alias_t maximum-pitch offset changed");
_Static_assert(sizeof(((snd_alias_t *)0)->pitchMax) == 0x04,
               "i386 snd_alias_t maximum-pitch extent changed");
_Static_assert(offsetof(snd_alias_t, distanceMin) == 0x24,
               "i386 snd_alias_t minimum-distance offset changed");
_Static_assert(sizeof(((snd_alias_t *)0)->distanceMin) == 0x04,
               "i386 snd_alias_t minimum-distance extent changed");
_Static_assert(offsetof(snd_alias_t, distanceMax) == 0x28,
               "i386 snd_alias_t maximum-distance offset changed");
_Static_assert(sizeof(((snd_alias_t *)0)->distanceMax) == 0x04,
               "i386 snd_alias_t maximum-distance extent changed");
_Static_assert(offsetof(snd_alias_t, channel) == 0x2c,
               "i386 snd_alias_t channel offset changed");
_Static_assert(sizeof(((snd_alias_t *)0)->channel) == 0x04,
               "i386 snd_alias_t channel extent changed");
_Static_assert(offsetof(snd_alias_t, type) == 0x30,
               "i386 snd_alias_t type offset changed");
_Static_assert(sizeof(((snd_alias_t *)0)->type) == 0x04,
               "i386 snd_alias_t type extent changed");
_Static_assert(offsetof(snd_alias_t, loop) == 0x34,
               "i386 snd_alias_t loop offset changed");
_Static_assert(sizeof(((snd_alias_t *)0)->loop) == 0x01,
               "i386 snd_alias_t loop extent changed");
_Static_assert(offsetof(snd_alias_t, isMaster) == 0x35,
               "i386 snd_alias_t master flag offset changed");
_Static_assert(sizeof(((snd_alias_t *)0)->isMaster) == 0x01,
               "i386 snd_alias_t master flag extent changed");
_Static_assert(offsetof(snd_alias_t, isSlave) == 0x36,
               "i386 snd_alias_t slave flag offset changed");
_Static_assert(sizeof(((snd_alias_t *)0)->isSlave) == 0x01,
               "i386 snd_alias_t slave flag extent changed");
_Static_assert(offsetof(snd_alias_t, streamedFileExists) == 0x37,
               "i386 snd_alias_t streamed-file-presence offset changed");
_Static_assert(sizeof(((snd_alias_t *)0)->streamedFileExists) == 0x01,
               "i386 snd_alias_t streamed-file-presence extent changed");
_Static_assert(offsetof(snd_alias_t, slavePercentage) == 0x38,
               "i386 snd_alias_t slave-percentage offset changed");
_Static_assert(sizeof(((snd_alias_t *)0)->slavePercentage) == 0x04,
               "i386 snd_alias_t slave-percentage extent changed");
_Static_assert(offsetof(snd_alias_t, selectionWeight) == 0x3c,
               "i386 snd_alias_t selection-weight offset changed");
_Static_assert(sizeof(((snd_alias_t *)0)->selectionWeight) == 0x04,
               "i386 snd_alias_t selection-weight extent changed");
_Static_assert(offsetof(snd_alias_t, lodMin) == 0x40,
               "i386 snd_alias_t minimum-LOD offset changed");
_Static_assert(sizeof(((snd_alias_t *)0)->lodMin) == 0x04,
               "i386 snd_alias_t minimum-LOD extent changed");
_Static_assert(offsetof(snd_alias_t, lodMax) == 0x44,
               "i386 snd_alias_t maximum-LOD offset changed");
_Static_assert(sizeof(((snd_alias_t *)0)->lodMax) == 0x04,
               "i386 snd_alias_t maximum-LOD extent changed");
_Static_assert(offsetof(snd_alias_t, hashNext) == 0x48,
               "i386 snd_alias_t hash-link offset changed");
_Static_assert(sizeof(((snd_alias_t *)0)->hashNext) == 0x04,
               "i386 snd_alias_t hash-link extent changed");
_Static_assert(sizeof(snd_alias_t) == 0x4c,
               "original i386 snd_alias_t size changed");

_Static_assert(_Alignof(snd_alias_parse_node_t) == 0x04,
               "i386 sound-alias parse-node alignment changed");
_Static_assert(offsetof(snd_alias_parse_node_t, sourceFile) == 0x0000,
               "i386 sound-alias parse source-file offset changed");
_Static_assert(sizeof(((snd_alias_parse_node_t *)0)->sourceFile) == 0x0040,
               "i386 sound-alias parse source-file extent changed");
_Static_assert(offsetof(snd_alias_parse_node_t, aliasName) == 0x0040,
               "i386 sound-alias parse name offset changed");
_Static_assert(sizeof(((snd_alias_parse_node_t *)0)->aliasName) == 0x0040,
               "i386 sound-alias parse name extent changed");
_Static_assert(offsetof(snd_alias_parse_node_t, subtitle) == 0x0080,
               "i386 sound-alias parse subtitle offset changed");
_Static_assert(sizeof(((snd_alias_parse_node_t *)0)->subtitle) == 0x1000,
               "i386 sound-alias parse subtitle extent changed");
_Static_assert(offsetof(snd_alias_parse_node_t, sequence) == 0x1080,
               "i386 sound-alias parse sequence offset changed");
_Static_assert(sizeof(((snd_alias_parse_node_t *)0)->sequence) == 0x04,
               "i386 sound-alias parse sequence extent changed");
_Static_assert(offsetof(snd_alias_parse_node_t, soundFile) == 0x1084,
               "i386 sound-alias parse file offset changed");
_Static_assert(sizeof(((snd_alias_parse_node_t *)0)->soundFile) == 0x40,
               "i386 sound-alias parse file extent changed");
_Static_assert(offsetof(snd_alias_parse_node_t, permanentSoundFile) == 0x10c4,
               "i386 sound-alias compact-file offset changed");
_Static_assert(sizeof(((snd_alias_parse_node_t *)0)->permanentSoundFile) == 0x04,
               "i386 sound-alias compact-file extent changed");
_Static_assert(offsetof(snd_alias_parse_node_t, volumeMin) == 0x10c8,
               "i386 sound-alias parse volume offset changed");
_Static_assert(sizeof(((snd_alias_parse_node_t *)0)->volumeMin) == 0x04,
               "i386 sound-alias minimum-volume extent changed");
_Static_assert(offsetof(snd_alias_parse_node_t, volumeMax) == 0x10cc,
               "i386 sound-alias maximum-volume offset changed");
_Static_assert(sizeof(((snd_alias_parse_node_t *)0)->volumeMax) == 0x04,
               "i386 sound-alias maximum-volume extent changed");
_Static_assert(offsetof(snd_alias_parse_node_t, pitchMin) == 0x10d0,
               "i386 sound-alias minimum-pitch offset changed");
_Static_assert(sizeof(((snd_alias_parse_node_t *)0)->pitchMin) == 0x04,
               "i386 sound-alias minimum-pitch extent changed");
_Static_assert(offsetof(snd_alias_parse_node_t, pitchMax) == 0x10d4,
               "i386 sound-alias maximum-pitch offset changed");
_Static_assert(sizeof(((snd_alias_parse_node_t *)0)->pitchMax) == 0x04,
               "i386 sound-alias maximum-pitch extent changed");
_Static_assert(offsetof(snd_alias_parse_node_t, distanceMin) == 0x10d8,
               "i386 sound-alias minimum-distance offset changed");
_Static_assert(sizeof(((snd_alias_parse_node_t *)0)->distanceMin) == 0x04,
               "i386 sound-alias minimum-distance extent changed");
_Static_assert(offsetof(snd_alias_parse_node_t, distanceMax) == 0x10dc,
               "i386 sound-alias maximum-distance offset changed");
_Static_assert(sizeof(((snd_alias_parse_node_t *)0)->distanceMax) == 0x04,
               "i386 sound-alias maximum-distance extent changed");
_Static_assert(offsetof(snd_alias_parse_node_t, channel) == 0x10e0,
               "i386 sound-alias channel offset changed");
_Static_assert(sizeof(((snd_alias_parse_node_t *)0)->channel) == 0x04,
               "i386 sound-alias channel extent changed");
_Static_assert(offsetof(snd_alias_parse_node_t, type) == 0x10e4,
               "i386 sound-alias type offset changed");
_Static_assert(sizeof(((snd_alias_parse_node_t *)0)->type) == 0x04,
               "i386 sound-alias type extent changed");
_Static_assert(offsetof(snd_alias_parse_node_t, loop) == 0x10e8,
               "i386 sound-alias loop offset changed");
_Static_assert(sizeof(((snd_alias_parse_node_t *)0)->loop) == 0x01,
               "i386 sound-alias loop extent changed");
_Static_assert(offsetof(snd_alias_parse_node_t, isMaster) == 0x10e9,
               "i386 sound-alias master flag offset changed");
_Static_assert(sizeof(((snd_alias_parse_node_t *)0)->isMaster) == 0x01,
               "i386 sound-alias master flag extent changed");
_Static_assert(offsetof(snd_alias_parse_node_t, isSlave) == 0x10ea,
               "i386 sound-alias slave flag offset changed");
_Static_assert(sizeof(((snd_alias_parse_node_t *)0)->isSlave) == 0x01,
               "i386 sound-alias slave flag extent changed");
_Static_assert(offsetof(snd_alias_parse_node_t, slavePercentage) == 0x10ec,
               "i386 sound-alias slave-percentage offset changed");
_Static_assert(sizeof(((snd_alias_parse_node_t *)0)->slavePercentage) == 0x04,
               "i386 sound-alias slave-percentage extent changed");
_Static_assert(offsetof(snd_alias_parse_node_t, selectionWeight) == 0x10f0,
               "i386 sound-alias selection-weight offset changed");
_Static_assert(sizeof(((snd_alias_parse_node_t *)0)->selectionWeight) == 0x04,
               "i386 sound-alias selection-weight extent changed");
_Static_assert(offsetof(snd_alias_parse_node_t, matchesLoadSpecification) == 0x10f4,
               "i386 sound-alias load-specification offset changed");
_Static_assert(sizeof(((snd_alias_parse_node_t *)0)->matchesLoadSpecification) == 0x04,
               "i386 sound-alias load-specification extent changed");
_Static_assert(offsetof(snd_alias_parse_node_t, lodMin) == 0x10f8,
               "i386 sound-alias minimum-LOD offset changed");
_Static_assert(sizeof(((snd_alias_parse_node_t *)0)->lodMin) == 0x04,
               "i386 sound-alias minimum-LOD extent changed");
_Static_assert(offsetof(snd_alias_parse_node_t, lodMax) == 0x10fc,
               "i386 sound-alias maximum-LOD offset changed");
_Static_assert(sizeof(((snd_alias_parse_node_t *)0)->lodMax) == 0x04,
               "i386 sound-alias maximum-LOD extent changed");
_Static_assert(offsetof(snd_alias_parse_node_t, duplicateFileNode) == 0x1100,
               "i386 sound-alias duplicate-file link offset changed");
_Static_assert(sizeof(((snd_alias_parse_node_t *)0)->duplicateFileNode) == 0x04,
               "i386 sound-alias duplicate-file link extent changed");
_Static_assert(offsetof(snd_alias_parse_node_t, next) == 0x1104,
               "i386 sound-alias parse link offset changed");
_Static_assert(sizeof(((snd_alias_parse_node_t *)0)->next) == 0x04,
               "i386 sound-alias parse link extent changed");
_Static_assert(sizeof(snd_alias_parse_node_t) == 0x1108,
               "original i386 sound-alias parse-node size changed");
#endif

#ifdef __cplusplus
}
#endif

#endif
