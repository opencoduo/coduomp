#ifndef SOUND_SYSTEM_H
#define SOUND_SYSTEM_H

#include "../q_shared.h"
#include "qcommon/sound_types.h"
#include "sound/alias/sound_alias.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) && defined(_MSC_VER) && defined(_M_IX86)
#define AUDIO_CALLBACK __stdcall
#elif defined(_WIN32) && defined(__i386__)
#define AUDIO_CALLBACK __attribute__((stdcall))
#else
#define AUDIO_CALLBACK
#endif

typedef void *audio_driver_t;
typedef void *audio_window_handle_t;
typedef int32_t audio_provider_t;
typedef int32_t audio_provider_enumerator_t;
typedef struct audio_sample_handle_s *audio_sample_handle_t;
typedef struct audio_3d_sample_handle_s *audio_3d_sample_handle_t;
typedef struct audio_stream_handle_s *audio_stream_handle_t;
typedef struct eax_manager_s eax_manager_t;
typedef int32_t (AUDIO_CALLBACK *audio_file_open_callback_t)(
    const char *filename, int32_t *fileHandle);
typedef void (AUDIO_CALLBACK *audio_file_close_callback_t)(
    int32_t fileHandle);
typedef int32_t (AUDIO_CALLBACK *audio_file_seek_callback_t)(
    int32_t fileHandle, int32_t offset, int32_t origin);
typedef int32_t (AUDIO_CALLBACK *audio_file_read_callback_t)(
    int32_t fileHandle, void *buffer, int32_t byteCount);

typedef enum audioSampleType_e {
    AUDIO_SAMPLE_TYPE_MONO_8 = 0,
    AUDIO_SAMPLE_TYPE_MONO_16 = 1,
    AUDIO_SAMPLE_TYPE_STEREO_8 = 2,
    AUDIO_SAMPLE_TYPE_STEREO_16 = 3,
    AUDIO_SAMPLE_TYPE_MONO_IMA_ADPCM = 5,
    AUDIO_SAMPLE_TYPE_STEREO_IMA_ADPCM = 7
} audio_sample_type_t;

enum {
    AUDIO_SAMPLE_STATUS_DONE = 2,
    MSS_3D_CHANNEL_FIRST = 0,
    MSS_3D_CHANNEL_CAPACITY = 32,
    MSS_STREAM_CHANNEL_FIRST = 32,
    MSS_STREAM_CHANNEL_CAPACITY = 13,
    MSS_ALIAS_STREAM_SLOT_FIRST = 5,
    MSS_ALIAS_STREAM_CHANNEL_FIRST =
        MSS_STREAM_CHANNEL_FIRST + MSS_ALIAS_STREAM_SLOT_FIRST,
    MSS_2D_CHANNEL_FIRST = 45,
    MSS_2D_CHANNEL_CAPACITY = 32,
    MSS_TOTAL_CHANNEL_COUNT = 77
};

/* Complete runtime channel descriptor. CoDUOMP.exe and the Mac PPC build both
 * store and consume effectId and aliasChannel as complete 32-bit lanes at
 * +0x00 and +0x04; there is no padding within the first eight bytes. The save
 * path deliberately serializes only their two- and one-byte address prefixes.
 * The three playback scalars are logical volume, the unpitched sound-file
 * rate, and the alias-selected pitch multiplier. */
typedef struct mss_channel_info_s {
    int32_t effectId;
    sndAliasChannel_t aliasChannel;
    int32_t lastUpdateTime;
    int32_t endTime;
    float logicalVolume;
    int32_t basePlaybackRate;
    float aliasPitchScale;
    snd_alias_t *alias;
    snd_alias_t *secondaryAlias;
    float aliasBlend;
    vec3_t effectOffset;
    qboolean paused;
} mss_channel_info_t;

/* Per-stream spatial state. The thirteen original records use a 0x10 stride;
 * stream start writes the 3D selector and position, while spatialization and
 * the update loop consume both. */
typedef struct mss_stream_channel_s {
    qboolean is3D;
    vec3_t position;
} mss_stream_channel_t;

/* Master and per-alias-channel volume ramps. MSS_UpdateVolume proves the
 * current/target/per-millisecond-rate order and complete 0x0c extent. */
typedef struct mss_channel_volume_s {
    float current;
    float target;
    float ratePerMsec;
} mss_channel_volume_t;

/* Per-background target and signed per-millisecond rate, indexed at an 0x08
 * stride by the start, stop, save/restore, and frame-update paths. */
typedef struct mss_background_fade_s {
    float targetVolume;
    float ratePerMsec;
} mss_background_fade_t;

typedef enum mssSoundOverlayType_e {
    MSS_SOUND_OVERLAY_3D = 1,
    MSS_SOUND_OVERLAY_STREAM = 2,
    MSS_SOUND_OVERLAY_2D = 3
} mssSoundOverlayType_t;

enum { MSS_RAW_BUFFER_COUNT = 32 };

#ifdef __cplusplus
#define MSS_ALIGNAS_EIGHT alignas(8)
#else
#define MSS_ALIGNAS_EIGHT _Alignas(8)
#endif

typedef struct mss_raw_sample_state_s {
    audio_sample_handle_t sample;
    int32_t sampleRate;
    int32_t sampleWidthBytes;
    int32_t channelCount;
    uint8_t *ringBuffer;
    uint8_t segmentReady[MSS_RAW_BUFFER_COUNT];
    int32_t segmentSize;
    int32_t writeSegmentOffset;
    int32_t readSegmentIndex;
    int32_t writeSegmentIndex;
    /* Implicit +0x44..+0x47 alignment padding has no semantic consumer in
     * CoDUOMP.exe; it is covered only by whole-state zeroing. */
    MSS_ALIGNAS_EIGHT double sampleTimeBaseMsec;
    double sampleTimePerByteMsec;
} mss_raw_sample_state_t;

/* The Miles processing calls take a private working descriptor whose public
 * prefix is the 0x24-byte sound metadata copied into each loaded alias.
 * CoDUOMP.exe MSS_LoadSoundFile reserves 0x78 bytes for this descriptor, and
 * mss32.dll AIL_process_digital_audio advances descriptors by 0x78 and writes
 * private state through +0x74. The private tail is needed only on 32-bit
 * targets; native replacement backends consume the widened public prefix. */
enum {
    AUDIO_SOUND_INFO_I386_PUBLIC_EXTENT = 0x24,
    AUDIO_SOUND_INFO_I386_EXTENT = 0x78
};

typedef struct audio_sound_info_s {
    snd_alias_sound_file_t publicInfo;
#if UINTPTR_MAX == UINT32_MAX
    uint8_t privateState[
        AUDIO_SOUND_INFO_I386_EXTENT -
        AUDIO_SOUND_INFO_I386_PUBLIC_EXTENT];
#endif
} audio_sound_info_t;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(offsetof(audio_sound_info_t, privateState) ==
                   AUDIO_SOUND_INFO_I386_PUBLIC_EXTENT,
               "i386 Miles sound-info private-state offset changed");
_Static_assert(sizeof(audio_sound_info_t) ==
                   AUDIO_SOUND_INFO_I386_EXTENT,
               "i386 Miles sound-info extent changed");
#endif

#undef MSS_ALIGNAS_EIGHT

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(mss_channel_info_t) == 0x04,
               "i386 MSS channel-info alignment changed");
_Static_assert(offsetof(mss_channel_info_t, effectId) == 0x00,
               "i386 MSS channel effect-id offset changed");
_Static_assert(sizeof(((mss_channel_info_t *)0)->effectId) == 0x04,
               "i386 MSS channel effect-id extent changed");
_Static_assert(offsetof(mss_channel_info_t, aliasChannel) == 0x04,
               "i386 MSS channel alias-channel offset changed");
_Static_assert(sizeof(((mss_channel_info_t *)0)->aliasChannel) == 0x04,
               "i386 MSS channel alias-channel extent changed");
_Static_assert(offsetof(mss_channel_info_t, lastUpdateTime) == 0x08,
               "i386 MSS channel last-update offset changed");
_Static_assert(sizeof(((mss_channel_info_t *)0)->lastUpdateTime) == 0x04,
               "i386 MSS channel last-update extent changed");
_Static_assert(offsetof(mss_channel_info_t, endTime) == 0x0c,
               "i386 MSS channel end-time offset changed");
_Static_assert(sizeof(((mss_channel_info_t *)0)->endTime) == 0x04,
               "i386 MSS channel end-time extent changed");
_Static_assert(offsetof(mss_channel_info_t, logicalVolume) == 0x10,
               "i386 MSS channel volume offset changed");
_Static_assert(sizeof(((mss_channel_info_t *)0)->logicalVolume) == 0x04,
               "i386 MSS channel volume extent changed");
_Static_assert(offsetof(mss_channel_info_t, basePlaybackRate) == 0x14,
               "i386 MSS channel playback-rate offset changed");
_Static_assert(sizeof(((mss_channel_info_t *)0)->basePlaybackRate) == 0x04,
               "i386 MSS channel playback-rate extent changed");
_Static_assert(offsetof(mss_channel_info_t, aliasPitchScale) == 0x18,
               "i386 MSS channel pitch offset changed");
_Static_assert(sizeof(((mss_channel_info_t *)0)->aliasPitchScale) == 0x04,
               "i386 MSS channel pitch extent changed");
_Static_assert(offsetof(mss_channel_info_t, alias) == 0x1c,
               "i386 MSS channel alias offset changed");
_Static_assert(sizeof(((mss_channel_info_t *)0)->alias) == 0x04,
               "i386 MSS channel alias extent changed");
_Static_assert(offsetof(mss_channel_info_t, secondaryAlias) == 0x20,
               "i386 MSS channel secondary-alias offset changed");
_Static_assert(sizeof(((mss_channel_info_t *)0)->secondaryAlias) == 0x04,
               "i386 MSS channel secondary-alias extent changed");
_Static_assert(offsetof(mss_channel_info_t, aliasBlend) == 0x24,
               "i386 MSS channel alias-blend offset changed");
_Static_assert(sizeof(((mss_channel_info_t *)0)->aliasBlend) == 0x04,
               "i386 MSS channel alias-blend extent changed");
_Static_assert(offsetof(mss_channel_info_t, effectOffset) == 0x28,
               "i386 MSS channel effect-offset field changed");
_Static_assert(sizeof(((mss_channel_info_t *)0)->effectOffset) == 0x0c,
               "i386 MSS channel effect-offset extent changed");
_Static_assert(offsetof(mss_channel_info_t, paused) == 0x34,
               "i386 MSS channel paused-state offset changed");
_Static_assert(sizeof(((mss_channel_info_t *)0)->paused) == 0x04,
               "i386 MSS channel paused-state extent changed");
_Static_assert(sizeof(mss_channel_info_t) == 0x38,
               "original i386 MSS channel-info size changed");
_Static_assert(_Alignof(mss_stream_channel_t) == 0x04,
               "i386 MSS stream-channel alignment changed");
_Static_assert(offsetof(mss_stream_channel_t, is3D) == 0x00,
               "i386 MSS stream 3D-state offset changed");
_Static_assert(sizeof(((mss_stream_channel_t *)0)->is3D) == 0x04,
               "i386 MSS stream 3D-state extent changed");
_Static_assert(offsetof(mss_stream_channel_t, position) == 0x4,
               "i386 MSS stream position offset changed");
_Static_assert(sizeof(((mss_stream_channel_t *)0)->position) == 0x0c,
               "i386 MSS stream position extent changed");
_Static_assert(sizeof(mss_stream_channel_t) == 0x10,
               "original i386 MSS stream-channel size changed");

_Static_assert(_Alignof(mss_channel_volume_t) == 0x04,
               "i386 MSS channel-volume alignment changed");
_Static_assert(offsetof(mss_channel_volume_t, current) == 0x00,
               "i386 MSS channel-volume current offset changed");
_Static_assert(sizeof(((mss_channel_volume_t *)0)->current) == 0x04,
               "i386 MSS channel-volume current extent changed");
_Static_assert(offsetof(mss_channel_volume_t, target) == 0x04,
               "i386 MSS channel-volume target offset changed");
_Static_assert(sizeof(((mss_channel_volume_t *)0)->target) == 0x04,
               "i386 MSS channel-volume target extent changed");
_Static_assert(offsetof(mss_channel_volume_t, ratePerMsec) == 0x08,
               "i386 MSS channel-volume rate offset changed");
_Static_assert(sizeof(((mss_channel_volume_t *)0)->ratePerMsec) == 0x04,
               "i386 MSS channel-volume rate extent changed");
_Static_assert(sizeof(mss_channel_volume_t) == 0x0c,
               "i386 MSS channel-volume size changed");

_Static_assert(_Alignof(mss_background_fade_t) == 0x04,
               "i386 MSS background-fade alignment changed");
_Static_assert(offsetof(mss_background_fade_t, targetVolume) == 0x00,
               "i386 MSS background target-volume offset changed");
_Static_assert(sizeof(((mss_background_fade_t *)0)->targetVolume) == 0x04,
               "i386 MSS background target-volume extent changed");
_Static_assert(offsetof(mss_background_fade_t, ratePerMsec) == 0x04,
               "i386 MSS background fade-rate offset changed");
_Static_assert(sizeof(((mss_background_fade_t *)0)->ratePerMsec) == 0x04,
               "i386 MSS background fade-rate extent changed");
_Static_assert(sizeof(mss_background_fade_t) == 0x08,
               "i386 MSS background-fade size changed");

_Static_assert(_Alignof(mss_raw_sample_state_t) == 0x08,
               "i386 raw-sample state alignment changed");
_Static_assert(offsetof(mss_raw_sample_state_t, sample) == 0x00,
               "i386 raw-sample handle offset changed");
_Static_assert(sizeof(((mss_raw_sample_state_t *)0)->sample) == 0x04,
               "i386 raw-sample handle extent changed");
_Static_assert(offsetof(mss_raw_sample_state_t, sampleRate) == 0x04,
               "i386 raw-sample rate offset changed");
_Static_assert(sizeof(((mss_raw_sample_state_t *)0)->sampleRate) == 0x04,
               "i386 raw-sample rate extent changed");
_Static_assert(offsetof(mss_raw_sample_state_t, sampleWidthBytes) == 0x08,
               "i386 raw-sample width offset changed");
_Static_assert(sizeof(((mss_raw_sample_state_t *)0)->sampleWidthBytes) == 0x04,
               "i386 raw-sample width extent changed");
_Static_assert(offsetof(mss_raw_sample_state_t, channelCount) == 0x0c,
               "i386 raw-sample channel-count offset changed");
_Static_assert(sizeof(((mss_raw_sample_state_t *)0)->channelCount) == 0x04,
               "i386 raw-sample channel-count extent changed");
_Static_assert(offsetof(mss_raw_sample_state_t, ringBuffer) == 0x10,
               "i386 raw-sample buffer-pointer offset changed");
_Static_assert(sizeof(((mss_raw_sample_state_t *)0)->ringBuffer) == 0x04,
               "i386 raw-sample buffer-pointer extent changed");
_Static_assert(offsetof(mss_raw_sample_state_t, segmentReady) == 0x14,
               "i386 raw-sample ready-ring offset changed");
_Static_assert(sizeof(((mss_raw_sample_state_t *)0)->segmentReady) == 0x20,
               "i386 raw-sample ready-ring extent changed");
_Static_assert(offsetof(mss_raw_sample_state_t, segmentSize) == 0x34,
               "i386 raw-sample buffer-size offset changed");
_Static_assert(sizeof(((mss_raw_sample_state_t *)0)->segmentSize) == 0x04,
               "i386 raw-sample buffer-size extent changed");
_Static_assert(offsetof(mss_raw_sample_state_t, writeSegmentOffset) == 0x38,
               "i386 raw-sample write-offset field changed");
_Static_assert(sizeof(((mss_raw_sample_state_t *)0)->writeSegmentOffset) == 0x04,
               "i386 raw-sample write-offset extent changed");
_Static_assert(offsetof(mss_raw_sample_state_t, readSegmentIndex) == 0x3c,
               "i386 raw-sample load-index offset changed");
_Static_assert(sizeof(((mss_raw_sample_state_t *)0)->readSegmentIndex) == 0x04,
               "i386 raw-sample load-index extent changed");
_Static_assert(offsetof(mss_raw_sample_state_t, writeSegmentIndex) == 0x40,
               "i386 raw-sample write-index offset changed");
_Static_assert(sizeof(((mss_raw_sample_state_t *)0)->writeSegmentIndex) == 0x04,
               "i386 raw-sample write-index extent changed");
_Static_assert(offsetof(mss_raw_sample_state_t, sampleTimeBaseMsec) == 0x48,
               "i386 raw-sample time-base offset changed");
_Static_assert(sizeof(((mss_raw_sample_state_t *)0)->sampleTimeBaseMsec) == 0x08,
               "i386 raw-sample time-base extent changed");
_Static_assert(offsetof(mss_raw_sample_state_t, sampleTimePerByteMsec) == 0x50,
               "i386 raw-sample time-scale offset changed");
_Static_assert(sizeof(((mss_raw_sample_state_t *)0)->sampleTimePerByteMsec) == 0x08,
               "i386 raw-sample time-scale extent changed");
_Static_assert(sizeof(mss_raw_sample_state_t) == 0x58,
               "original i386 raw-sample state size changed");
#endif

extern cvar_t *mss_stereo;
extern cvar_t *mss_bits;
extern cvar_t *mss_khz;
extern cvar_t *mss_q3fs;
extern cvar_t *mss_errorOnMissing;
extern cvar_t *mss_3d_provider;
extern cvar_t *mss_volume;
extern cvar_t *mss_roomtype;
extern cvar_t *mss_wetlevel;
extern audio_driver_t mss_digitalDriver;
extern int32_t mss_sampleRate;
extern int32_t mss_sampleBits;
extern int32_t mss_channelCount;
extern float mss_playbackRateScale;
extern float mss_effectVolume;
extern mss_channel_volume_t mss_masterVolume;
extern mss_channel_volume_t
    mss_channelVolumes[SND_ALIAS_CHANNEL_COUNT];
extern int32_t mss_roomType;
extern float mss_reverbLevel;
extern float mss_reverbTarget;
extern float mss_reverbRate;
extern qboolean mss_underwaterEffectActive;
extern eax_manager_t *mss_eaxManager;
extern void *mss_eaxLibrary;
extern qboolean mss_eaxEnvironmentLoaded;
extern int32_t mss_eaxRoomId;
extern int32_t mss_2dChannelCount;
extern int32_t mss_streamChannelCount;
extern audio_provider_t mss_3dProvider;
extern int32_t mss_max3DChannels;
extern qboolean mss_eaxAvailable;
extern char mss_eaxMapName[MAX_QPATH];
extern vec3_t mss_listenerOrigin;
extern axis_t mss_listenerAxis;
extern int32_t mss_listenerTime;
extern audio_sample_handle_t
    mss_2dSampleHandles[MSS_2D_CHANNEL_CAPACITY];
extern audio_3d_sample_handle_t
    mss_3dSampleHandles[MSS_3D_CHANNEL_CAPACITY];
extern audio_stream_handle_t
    mss_streamHandles[MSS_STREAM_CHANNEL_CAPACITY];
extern int32_t mss_ambientBackgroundIndex;
extern mss_background_fade_t
    mss_backgroundFades[MSS_ALIAS_STREAM_SLOT_FIRST];
extern mss_raw_sample_state_t mss_rawSampleState;
extern qboolean mss_paused;
extern int32_t mss_pauseStartTime;
extern int32_t mss_cpuPercent;
extern uint8_t *mss_restoreBuffer;
extern int32_t mss_restoreSize;
extern int32_t mss_soundTime;
extern int32_t mss_lastSoundTime;
extern qboolean mss_anyMasters;
extern mss_channel_info_t mss_channelInfo[MSS_TOTAL_CHANNEL_COUNT];
extern mss_stream_channel_t
    mss_streamChannels[MSS_STREAM_CHANNEL_CAPACITY];



int32_t AUDIO_CALLBACK MSS_FileOpenCallback(const char *filename,
                                             int32_t *fileHandle);
void AUDIO_CALLBACK MSS_FileCloseCallback(int32_t fileHandle);
int32_t AUDIO_CALLBACK MSS_FileSeekCallback(int32_t fileHandle,
                                             int32_t offset, int32_t origin);
int32_t AUDIO_CALLBACK MSS_FileReadCallback(int32_t fileHandle, void *buffer,
                                             int32_t byteCount);
void StripExtension(const char *input, char *output);
void *MSS_Alloc(size_t size);
void MSS_Free(void *memory);
void MSS_InitFailed(void);
void MSS_Init(void);
void MSS_Shutdown(void);
void MSS_ErrorCleanup(void);
void MSS_SetWindowHandle(audio_window_handle_t windowHandle);
int32_t MSS_Write(uint8_t *buffer, int32_t offset,
                  int32_t bufferSize, const void *source,
                  size_t byteCount);
int32_t MSS_Read(const uint8_t *buffer, int32_t offset,
                 int32_t bufferSize, void *destination,
                 size_t byteCount);
int32_t MSS_SaveChanInfo(uint8_t *buffer, int32_t offset,
                         int32_t bufferSize,
                         const mss_channel_info_t *channelInfo);
int32_t MSS_RestoreChanInfo(const uint8_t *buffer, int32_t offset,
                            int32_t bufferSize,
                            mss_channel_info_t *channelInfo);
int32_t MSS_Save3DChannel(uint8_t *buffer, int32_t offset,
                          int32_t bufferSize, int32_t channelIndex);
int32_t MSS_Restore3DChannel(const uint8_t *buffer, int32_t offset,
                             int32_t bufferSize,
                             int32_t primaryAliasIndex);
int32_t MSS_Save2DChannel(uint8_t *buffer, int32_t offset,
                          int32_t bufferSize, int32_t channelIndex);
int32_t MSS_Restore2DChannel(const uint8_t *buffer, int32_t offset,
                             int32_t bufferSize,
                             int32_t primaryAliasIndex);
int32_t MSS_SaveStreamChannel(uint8_t *buffer, int32_t offset,
                              int32_t bufferSize,
                              int32_t channelIndex);
int32_t MSS_RestoreStreamChannel(const uint8_t *buffer,
                                 int32_t offset, int32_t bufferSize,
                                 int32_t primaryAliasIndex,
                                 int32_t requestedChannelIndex);
int32_t MSS_Save(uint8_t *saveData, int32_t saveCapacity);
void MSS_QueueRestore(const uint8_t *saveData, int32_t saveSize);
int32_t MSS_GetSoundOverlay2D(mss_sound_overlay_t *overlay,
                              int32_t maxCount);
int32_t MSS_GetSoundOverlay3D(mss_sound_overlay_t *overlay,
                              int32_t maxCount);
int32_t MSS_GetSoundOverlayStream(mss_sound_overlay_t *overlay,
                                  int32_t maxCount);
int32_t MSS_GetSoundOverlay(mssSoundOverlayType_t overlayType,
                            mss_sound_overlay_t *overlay,
                            int32_t maxCount, int32_t *cpuPercent);
qboolean MSS_Init2D(void);
qboolean MSS_Init3DProvider(const char *preferredProviderName,
                            qboolean listProviders);
void InitEAXManager(void);
void ReleaseEAXManager(void);
void EALFileInit(const char *mapName);
qboolean LoadEALFile(const char *path);
void UnloadEALFile(void);
void UpdateEAXListener(const vec3_t origin);
void MSS_SetListener(int32_t time, const vec3_t origin, const axis_t axis);
void MSS_GetListener(axis_t axis, vec3_t origin, int32_t *time);
void MSS_InitChannels(void);
long double MSS_Attenuate(float distance, float minimumDistance,
                          float maximumDistance);
void MSS_GetCurrent3DPosition(int32_t effectId, const vec3_t localOffset,
                              vec3_t worldPosition);
void MSS_Set3DPosition(audio_3d_sample_handle_t sample,
                       const vec3_t worldPosition);
void UpdateEAXBuffer(audio_3d_sample_handle_t sample,
                     const vec3_t position);
qboolean MSS_IsAliasChannel3D(sndAliasChannel_t aliasChannel);
int32_t MSS_CompareReplacableChannels(int32_t firstChannelIndex,
                                      int32_t secondChannelIndex,
                                      int32_t preferredEffectId);
qboolean MSS_IsChannelReplacable(int32_t channelIndex,
                                 sndAliasChannel_t maximumAliasChannel);
int32_t MSS_FindReplacableChannel(int32_t firstChannelIndex,
                                  int32_t channelCount,
                                  int32_t preferredEffectId,
                                  sndAliasChannel_t maximumAliasChannel);
void MSS_Stop2DChannel(int32_t channelIndex);
void MSS_Pause2DChannel(int32_t channelIndex);
void MSS_Unpause2DChannel(int32_t channelIndex, int32_t timeShift);
qboolean MSS_Is2DChannelFree(int32_t channelIndex);
int32_t MSS_FindFree2DChannel(int32_t preferredEffectId,
                              sndAliasChannel_t maximumAliasChannel);
void MSS_Stop3DChannel(int32_t channelIndex);
void MSS_Pause3DChannel(int32_t channelIndex);
void MSS_Unpause3DChannel(int32_t channelIndex, int32_t timeShift);
qboolean MSS_Is3DChannelFree(int32_t channelIndex);
int32_t MSS_FindFree3DChannel(int32_t preferredEffectId,
                              sndAliasChannel_t maximumAliasChannel);
void MSS_StopStreamChannel(int32_t channelIndex);
void MSS_PauseStreamChannel(int32_t channelIndex);
void MSS_UnpauseStreamChannel(int32_t channelIndex, int32_t timeShift);
qboolean MSS_IsStreamChannelFree(int32_t channelIndex);
int32_t MSS_FindFreeStreamChannel(int32_t preferredEffectId,
                                  sndAliasChannel_t maximumAliasChannel);
void MSS_StopEntityChannel(int32_t effectId,
                           sndAliasChannel_t aliasChannel);
void MSS_PauseSounds(void);
void MSS_UnpauseSounds(void);
void MSS_UpdateLoopingSounds(void);
qboolean MSS_UpdateBackgroundVolume(int32_t backgroundIndex,
                                    int32_t elapsedMsec);
void MSS_UpdateVolume(mss_channel_volume_t *volume,
                      int32_t elapsedMsec);
void MSS_UpdateMasterVolumes(int32_t elapsedMsec);
qboolean MSS_AnyMasters(void);
void MSS_Update3DChannel(int32_t channelIndex);
void MSS_Update2DChannel(int32_t channelIndex);
void MSS_UpdateStreamChannel(int32_t channelIndex,
                             int32_t elapsedMsec);
void MSS_UpdateAllChannels(int32_t elapsedMsec);
int32_t MSS_RoomTypeFromString(const char *roomType);
void MSS_SetEnvironmentEffects(const char *roomType, float reverbLevel,
                               int32_t fadeMsec);
void MSS_UpdateRoomEffects(int32_t elapsedMsec);
void MSS_UpdateTimeScale(void);
void MSS_UpdatePause(void);
void MSS_Restore(const uint8_t *saveData, int32_t saveSize);
audio_sample_type_t MSS_SampleType(int32_t waveFormatTag,
                                 int32_t sampleBits,
                                 int32_t channelCount);
snd_alias_sound_file_t *MSS_LoadSoundFile(const char *filename);
void MSS_UnloadSoundFile(snd_alias_sound_file_t *soundFile);
int32_t MSS_GetSoundFileSize(const snd_alias_sound_file_t *soundFile);
qboolean MSS_StartAlias2DSample(int32_t *outChannelIndex,
                                snd_alias_t *alias,
                                snd_alias_t *secondaryAlias,
                                float aliasBlend, int32_t effectId,
                                float volume, float pitch,
                                int32_t timeShift, float startFraction);
qboolean MSS_StartAlias3DSample(int32_t *outChannelIndex,
                                snd_alias_t *alias,
                                const vec3_t position,
                                snd_alias_t *secondaryAlias,
                                float aliasBlend, int32_t effectId,
                                float volume, float pitch,
                                int32_t timeShift, float startFraction);
qboolean MSS_StartAliasSample(int32_t *outChannelIndex,
                              snd_alias_t *alias,
                              snd_alias_t *secondaryAlias,
                              float aliasBlend, int32_t effectId,
                              const vec3_t position, float volume,
                              float pitch, int32_t timeShift);
int32_t MSS_StartAliasStreamOnChannel(
    snd_alias_t *alias, snd_alias_t *secondaryAlias, float aliasBlend,
    int32_t effectId, const vec3_t position, float volume, float pitch,
    int32_t timeShift, float startFraction, int32_t channelIndex);
int32_t MSS_StartAliasStream(
    int32_t *outChannelIndex, snd_alias_t *alias,
    snd_alias_t *secondaryAlias, float aliasBlend, int32_t effectId,
    const vec3_t position, float volume, float pitch, int32_t timeShift,
    float startFraction);
qboolean MSS_ContinueLoopingSound(
    snd_alias_t *alias, snd_alias_t *secondaryAlias, float aliasBlend,
    float pitchScale, int32_t effectId, const vec3_t position,
    int32_t *outChannelIndex);
void MSS_ChoosePitchAndVolume(snd_alias_t *alias,
                              snd_alias_t *secondaryAlias,
                              float aliasBlend, float *outVolume,
                              float *outPitch);
int32_t MSS_PlaySoundAlias_Internal(
    snd_alias_t *alias, snd_alias_t *secondaryAlias, float aliasBlend,
    float pitchScale, int32_t effectId, const vec3_t position,
    int32_t *outChannelIndex, int32_t timeShift);
int32_t MSS_PlaySoundAlias(snd_alias_t *alias, int32_t effectId,
                           const vec3_t position, int32_t timeShift);
qboolean MSS_ValidateSoundAliasBlend(const snd_alias_t *alias,
                                     const snd_alias_t *secondaryAlias,
                                     qboolean reportError);
int32_t MSS_PlayBlendedSoundAliases(
    snd_alias_t *alias, snd_alias_t *secondaryAlias, float aliasBlend,
    int32_t effectId, const vec3_t position, int32_t timeShift);
int32_t MSS_PlayLocalSoundAlias(const char *name, sndAliasBank_t bank);
void MSS_StartBackground(int32_t backgroundIndex, snd_alias_t *alias,
                         int32_t fadeTimeMsec);
void MSS_StopBackground(int32_t backgroundIndex, int32_t fadeTimeMsec);
void MSS_PlayMusicAlias(snd_alias_t *alias);
void MSS_StopMusic(int32_t fadeTimeMsec);
void MSS_PlayAmbientAlias(snd_alias_t *alias, int32_t fadeTimeMsec);
void MSS_BeginRawSamples(int32_t sampleRate, int32_t sampleWidthBytes,
                         int32_t channelCount);
void MSS_UpdateRawSamples(void);
void MSS_Update(void);
void MSS_StopSounds(uint32_t flags);
void MSS_FadeSelectSounds(
    const float targetVolumes[SND_ALIAS_CHANNEL_COUNT],
    int32_t durationMsec);
void MSS_SpatializeStream(int32_t streamIndex, float *volume, float *pan);
void MSS_SetChannelInfo(int32_t channelIndex, int32_t effectId,
                        snd_alias_t *alias, snd_alias_t *secondaryAlias,
                        float aliasBlend, const vec3_t position, float volume,
                        float pitch, int32_t playbackRate,
                        int32_t durationMsec, int32_t timeShift);

#ifdef __cplusplus
}
#endif

#endif
