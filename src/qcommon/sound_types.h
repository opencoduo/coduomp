#ifndef QCOMMON_SOUND_TYPES_H
#define QCOMMON_SOUND_TYPES_H

#include "q_shared_types.h"

#include <stddef.h>
#include <stdint.h>

/* NOT_FROM_ORIGINAL_SOURCE: names of the statically linked native adapters
 * exposed through the retail sound-provider cvar and options-menu control. */
#if defined(__APPLE__) || defined(__linux__)
#define CODUOMP_OPENAL_3D_PROVIDER_NAME "OpenAL Fast 2D Positional Audio"
#define CODUOMP_MINIAUDIO_3D_PROVIDER_NAME "Miniaudio Fast 2D Positional Audio"
#else
#define CODUOMP_OPENAL_3D_PROVIDER_NAME "Miles Fast 2D Positional Audio"
#define CODUOMP_MINIAUDIO_3D_PROVIDER_NAME "Miles Fast 2D Positional Audio"
#endif

#if defined(__APPLE__) || defined(__linux__)
/* NOT_FROM_ORIGINAL_SOURCE: one Miles Fast 2D voice has one transport and
 * playback cursor. The native OpenAL adapter needs two independent sources to
 * reproduce the provider's left/right gain law, so it cannot also preserve
 * that lifetime invariant. Miniaudio supplies the same gain law through one
 * transport and is the default native 3D provider. */
#define CODUOMP_3D_PROVIDER_NAME CODUOMP_MINIAUDIO_3D_PROVIDER_NAME
#else
#define CODUOMP_3D_PROVIDER_NAME CODUOMP_OPENAL_3D_PROVIDER_NAME
#endif

/* The three engine-owned alias banks and their compact hash tables are shared
 * by the Windows client and Linux dedicated engine. */
typedef enum sndAliasBank_e {
    SND_ALIAS_BANK_COMMON = 0,
    SND_ALIAS_BANK_CGAME = 1,
    SND_ALIAS_BANK_GAME = 2,
    SND_ALIAS_BANK_COUNT = 3
} sndAliasBank_t;

enum {
    SND_ALIAS_HASH_BUCKET_COUNT = 256,
    SND_ALIAS_PARSE_SHORT_STRING_CAPACITY = 64,
    SND_ALIAS_SOURCE_NAME_CAPACITY = 64,
    SND_ALIAS_PARSE_SUBTITLE_CAPACITY = 4096,
    SND_ALIAS_SUBTITLE_REFERENCE_CAPACITY = 1024
};

typedef enum sndAliasType_e {
    SND_ALIAS_TYPE_UNKNOWN = 0,
    SND_ALIAS_TYPE_LOADED = 1,
    SND_ALIAS_TYPE_STREAMED = 2
} sndAliasType_t;

typedef enum sndAliasChannel_e {
    SND_ALIAS_CHANNEL_AUTO = 0,
    SND_ALIAS_CHANNEL_MENU = 1,
    SND_ALIAS_CHANNEL_WEAPON = 2,
    SND_ALIAS_CHANNEL_VOICE = 3,
    SND_ALIAS_CHANNEL_ITEM = 4,
    SND_ALIAS_CHANNEL_BODY = 5,
    SND_ALIAS_CHANNEL_LOCAL = 6,
    SND_ALIAS_CHANNEL_MUSIC = 7,
    SND_ALIAS_CHANNEL_ANNOUNCER = 8,
    SND_ALIAS_CHANNEL_SHELLSHOCK = 9,
    SND_ALIAS_CHANNEL_COUNT = 10
} sndAliasChannel_t;

/* Public Miles metadata stored behind a loaded client alias. The Linux
 * dedicated engine keeps the same pointer lane in snd_alias_t but stores NULL
 * because it has no sound backend. */
typedef struct snd_alias_sound_file_s {
    int32_t formatTag;
    void *data;
    uint32_t dataLength;
    uint32_t sampleRate;
    int32_t bitsPerSample;
    int32_t channelCount;
    uint32_t sampleCount;
    uint32_t blockSize;
    void *initialData;
} snd_alias_sound_file_t;

/* Engine-owned sound-alias record shared by the Windows client, Windows game
 * syscall boundary, Linux dedicated engine, and cgame. CoDUOMP.exe and
 * coduo_lnxded both index compact tables at a 0x4c-byte i386 stride and agree
 * on all fields. The dedicated engine initializes soundFileInfo to NULL and
 * does not consume streamedFileExists; those are backend-use differences, not
 * layout differences. Mac symbols retain the snd_alias_t identity and the LOD
 * spelling for the selector range at +0x40/+0x44. */
typedef struct snd_alias_s {
    const char *aliasName;
    const char *soundFile;
    const char *subtitle;
    snd_alias_sound_file_t *soundFileInfo;
    int32_t pickSequence;
    float volumeMin;
    float volumeMax;
    float pitchMin;
    float pitchMax;
    float distanceMin;
    float distanceMax;
    sndAliasChannel_t channel;
    sndAliasType_t type;
    uint8_t loop;
    uint8_t isMaster;
    uint8_t isSlave;
    uint8_t streamedFileExists;
    float slavePercentage;
    float selectionWeight;
    float lodMin;
    float lodMax;
    struct snd_alias_s *hashNext;
} snd_alias_t;

typedef enum sndAliasField_e {
    SND_ALIAS_FIELD_UNKNOWN = 0,
    SND_ALIAS_FIELD_NAME = 1,
    SND_ALIAS_FIELD_SEQUENCE = 2,
    SND_ALIAS_FIELD_FILE = 3,
    SND_ALIAS_FIELD_SUBTITLE = 4,
    SND_ALIAS_FIELD_VOLUME_MIN = 5,
    SND_ALIAS_FIELD_VOLUME_MAX = 6,
    SND_ALIAS_FIELD_PITCH_MIN = 7,
    SND_ALIAS_FIELD_PITCH_MAX = 8,
    SND_ALIAS_FIELD_DISTANCE_MIN = 9,
    SND_ALIAS_FIELD_DISTANCE_MAX = 10,
    SND_ALIAS_FIELD_CHANNEL = 11,
    SND_ALIAS_FIELD_TYPE = 12,
    SND_ALIAS_FIELD_LOOP = 13,
    SND_ALIAS_FIELD_PROBABILITY = 14,
    SND_ALIAS_FIELD_LOAD_SPEC = 15,
    SND_ALIAS_FIELD_MASTER_SLAVE = 16,
    SND_ALIAS_FIELD_LOD_MIN = 17,
    SND_ALIAS_FIELD_LOD_MAX = 18,
    SND_ALIAS_FIELD_COUNT = 19
} sndAliasField_t;

/* Temporary CSV row expanded while a sound-alias file is parsed. Windows and
 * Linux copy the same 0x1108-byte i386 record and agree on every field. The two
 * linkage fields are filled by later build/sort stages, so the default
 * initializer deliberately leaves permanentSoundFile and duplicateFileNode
 * alone. Natural alignment contributes one unused byte after the three flags;
 * whole-record copies preserve it. Linux recovery names formerly described
 * aliasName/soundFile as name/file and the LOD range as a generic selector;
 * the Windows names retained here also agree with the final snd_alias_t fields
 * and Mac symbol terminology. */
typedef struct snd_alias_parse_node_s {
    char sourceFile[SND_ALIAS_PARSE_SHORT_STRING_CAPACITY];
    char aliasName[SND_ALIAS_PARSE_SHORT_STRING_CAPACITY];
    char subtitle[SND_ALIAS_PARSE_SUBTITLE_CAPACITY];
    int32_t sequence;
    char soundFile[SND_ALIAS_PARSE_SHORT_STRING_CAPACITY];
    const char *permanentSoundFile;
    float volumeMin;
    float volumeMax;
    float pitchMin;
    float pitchMax;
    float distanceMin;
    float distanceMax;
    sndAliasChannel_t channel;
    sndAliasType_t type;
    uint8_t loop;
    uint8_t isMaster;
    uint8_t isSlave;
    float slavePercentage;
    float selectionWeight;
    qboolean matchesLoadSpecification;
    float lodMin;
    float lodMax;
    struct snd_alias_parse_node_s *duplicateFileNode;
    struct snd_alias_parse_node_s *next;
} snd_alias_parse_node_t;

#if UINTPTR_MAX == UINT32_MAX
#define SOUND_TYPE_LAYOUT_ASSERT(name, expression) typedef char name[(expression) ? 1 : -1]
SOUND_TYPE_LAYOUT_ASSERT(q_sound_file_data_offset, offsetof(snd_alias_sound_file_t, data) == 0x04);
SOUND_TYPE_LAYOUT_ASSERT(q_sound_file_initial_data_offset, offsetof(snd_alias_sound_file_t, initialData) == 0x20);
SOUND_TYPE_LAYOUT_ASSERT(q_sound_file_extent, sizeof(snd_alias_sound_file_t) == 0x24);
SOUND_TYPE_LAYOUT_ASSERT(q_sound_alias_subtitle_offset, offsetof(snd_alias_t, subtitle) == 0x08);
SOUND_TYPE_LAYOUT_ASSERT(q_sound_alias_sound_file_info_offset, offsetof(snd_alias_t, soundFileInfo) == 0x0c);
SOUND_TYPE_LAYOUT_ASSERT(q_sound_alias_pick_sequence_offset, offsetof(snd_alias_t, pickSequence) == 0x10);
SOUND_TYPE_LAYOUT_ASSERT(q_sound_alias_distance_min_offset, offsetof(snd_alias_t, distanceMin) == 0x24);
SOUND_TYPE_LAYOUT_ASSERT(q_sound_alias_channel_offset, offsetof(snd_alias_t, channel) == 0x2c);
SOUND_TYPE_LAYOUT_ASSERT(q_sound_alias_type_offset, offsetof(snd_alias_t, type) == 0x30);
SOUND_TYPE_LAYOUT_ASSERT(q_sound_alias_streamed_file_offset, offsetof(snd_alias_t, streamedFileExists) == 0x37);
SOUND_TYPE_LAYOUT_ASSERT(q_sound_alias_selection_weight_offset, offsetof(snd_alias_t, selectionWeight) == 0x3c);
SOUND_TYPE_LAYOUT_ASSERT(q_sound_alias_lod_min_offset, offsetof(snd_alias_t, lodMin) == 0x40);
SOUND_TYPE_LAYOUT_ASSERT(q_sound_alias_lod_max_offset, offsetof(snd_alias_t, lodMax) == 0x44);
SOUND_TYPE_LAYOUT_ASSERT(q_sound_alias_hash_next_offset, offsetof(snd_alias_t, hashNext) == 0x48);
SOUND_TYPE_LAYOUT_ASSERT(q_sound_alias_extent, sizeof(snd_alias_t) == 0x4c);
SOUND_TYPE_LAYOUT_ASSERT(q_sound_alias_parse_alias_name_offset, offsetof(snd_alias_parse_node_t, aliasName) == 0x0040);
SOUND_TYPE_LAYOUT_ASSERT(q_sound_alias_parse_subtitle_offset, offsetof(snd_alias_parse_node_t, subtitle) == 0x0080);
SOUND_TYPE_LAYOUT_ASSERT(q_sound_alias_parse_sequence_offset, offsetof(snd_alias_parse_node_t, sequence) == 0x1080);
SOUND_TYPE_LAYOUT_ASSERT(q_sound_alias_parse_sound_file_offset, offsetof(snd_alias_parse_node_t, soundFile) == 0x1084);
SOUND_TYPE_LAYOUT_ASSERT(q_sound_alias_parse_permanent_file_offset, offsetof(snd_alias_parse_node_t, permanentSoundFile) == 0x10c4);
SOUND_TYPE_LAYOUT_ASSERT(q_sound_alias_parse_loop_offset, offsetof(snd_alias_parse_node_t, loop) == 0x10e8);
SOUND_TYPE_LAYOUT_ASSERT(q_sound_alias_parse_slave_percentage_offset, offsetof(snd_alias_parse_node_t, slavePercentage) == 0x10ec);
SOUND_TYPE_LAYOUT_ASSERT(q_sound_alias_parse_load_spec_offset, offsetof(snd_alias_parse_node_t, matchesLoadSpecification) == 0x10f4);
SOUND_TYPE_LAYOUT_ASSERT(q_sound_alias_parse_lod_min_offset, offsetof(snd_alias_parse_node_t, lodMin) == 0x10f8);
SOUND_TYPE_LAYOUT_ASSERT(q_sound_alias_parse_duplicate_offset, offsetof(snd_alias_parse_node_t, duplicateFileNode) == 0x1100);
SOUND_TYPE_LAYOUT_ASSERT(q_sound_alias_parse_next_offset, offsetof(snd_alias_parse_node_t, next) == 0x1104);
SOUND_TYPE_LAYOUT_ASSERT(q_sound_alias_parse_extent, sizeof(snd_alias_parse_node_t) == 0x1108);
#undef SOUND_TYPE_LAYOUT_ASSERT
#endif

/* Exact engine-to-cgame sound diagnostic boundary.  All three CoDUOMP.exe
 * MSS overlay producers advance the caller array by 0x14 and write these five
 * fields; CG_DrawSoundOverlay consumes the matching offsets.  The borrowed
 * soundFile pointer widens naturally in native host builds. */
typedef struct mss_sound_overlay_s {
    const char *soundFile;
    float logicalVolume;
    float relativeVolume;
    int32_t basePlaybackRate;
    float pitchScale;
} mss_sound_overlay_t;

#if UINTPTR_MAX == UINT32_MAX
typedef char q_mss_overlay_sound_file_offset[offsetof(mss_sound_overlay_t, soundFile) == 0x00 ? 1 : -1];
typedef char q_mss_overlay_logical_volume_offset[offsetof(mss_sound_overlay_t, logicalVolume) == 0x04 ? 1 : -1];
typedef char q_mss_overlay_relative_volume_offset[offsetof(mss_sound_overlay_t, relativeVolume) == 0x08 ? 1 : -1];
typedef char q_mss_overlay_playback_rate_offset[offsetof(mss_sound_overlay_t, basePlaybackRate) == 0x0c ? 1 : -1];
typedef char q_mss_overlay_pitch_scale_offset[offsetof(mss_sound_overlay_t, pitchScale) == 0x10 ? 1 : -1];
typedef char q_mss_overlay_size[sizeof(mss_sound_overlay_t) == 0x14 ? 1 : -1];
#endif

#endif
