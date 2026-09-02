#include "miles_boundary.h"

#include "../client/cgame.h"
#include "../math/vector_math.h"
#include "qcommon/hunk.h"
#include "qcommon/com_sprintf.h"
#include "qcommon/q_string.h"
#include "../platform/crt_boundary.h"
#include "../platform/dynamic_library_boundary.h"
#include "../system_fatal.h"
#include "../ui/ui_module_loader.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

enum {
    MILES_FILE_OPEN_FAILED = -1,
    MILES_FILE_OPEN_FAILURE_RESULT = 0
};

typedef enum milesFileSeekOrigin_e {
    MILES_FILE_SEEK_END = 0,
    MILES_FILE_SEEK_SET = 1,
    MILES_FILE_SEEK_CURRENT = 2
} milesFileSeekOrigin_t;

typedef enum engineFileSeekOrigin_e {
    ENGINE_FILE_SEEK_SET = 0,
    ENGINE_FILE_SEEK_CURRENT = 1,
    ENGINE_FILE_SEEK_END = 2
} engineFileSeekOrigin_t;

enum {
    MILES_ALLOCATION_SIZE_ALIGNMENT = 4,
    MILES_HUNK_ALIGNMENT = 32,
    MILES_MIXER_CHANNEL_PREFERENCE = 1,
    MSS_EFFECT_ID_LIMIT = 1024,
    MSS_MUSIC_BACKGROUND_INDEX = 0,
    MSS_AMBIENT_BACKGROUND_FIRST_INDEX = 1,
    MSS_AMBIENT_BACKGROUND_SECOND_INDEX = 2,
    MSS_AMBIENT_BACKGROUND_INITIAL_INDEX = MSS_AMBIENT_BACKGROUND_SECOND_INDEX,
    MSS_BACKGROUND_FADE_IMMEDIATE_MSEC = 0,
    MILES_CHANNEL_COUNT_MONO = 1,
    MILES_CHANNEL_COUNT_STEREO = 2,
    MILES_SAMPLE_BITS_IMA_ADPCM = 4,
    MILES_SAMPLE_BITS_8 = 8,
    MILES_SAMPLE_BITS_16 = 16,
    MILES_WAVE_FORMAT_PCM = 1,
    MILES_WAVE_FORMAT_IMA_ADPCM = 17,
    MSS_RAW_MINIMUM_BUFFER_SIZE = 8192,
    MSS_ROOM_TYPE_COUNT = 26,
    MSS_SOUND_CPU_WARNING_THRESHOLD_PERCENT = 2,
    MSS_SOUND_CPU_WARNING_ENTRY = 2,
    MSS_SOUND_CPU_WARNING_DURATION_MSEC = 3000,
    MSS_ENVIRONMENT_FADE_IMMEDIATE_MSEC = 0,
    MILES_WAV_INFO_INVALID = 0,
    MILES_PROCESS_AUDIO_SINGLE_BUFFER = 1,
    MSS_RIFF_FILE_HEADER_BYTES = 12,
    MSS_RIFF_CHUNK_HEADER_BYTES = 8,
    MSS_RIFF_FOURCC_BYTES = 4,
    MSS_RIFF_SIZE_OFFSET = 4,
    MSS_RIFF_FORM_OFFSET = 8,
    MSS_RIFF_CHUNK_SIZE_OFFSET = 4,
    MSS_WAVE_FORMAT_BASE_BYTES = 16,
    MSS_WAVE_FORMAT_TAG_OFFSET = 0,
    MSS_WAVE_CHANNEL_COUNT_OFFSET = 2,
    MSS_WAVE_SAMPLE_RATE_OFFSET = 4,
    MSS_WAVE_AVERAGE_BYTES_PER_SECOND_OFFSET = 8,
    MSS_WAVE_BLOCK_SIZE_OFFSET = 12,
    MSS_WAVE_BITS_PER_SAMPLE_OFFSET = 14,
    MSS_WAVE_IMA_HEADER_BYTES_PER_CHANNEL = 4,
    MSS_WAVE_IMA_STEP_INDEX_OFFSET = 2,
    MSS_WAVE_IMA_MAXIMUM_STEP_INDEX = 88,
    MSS_WAVE_IMA_STEREO_GROUP_BYTES = 8,
    MSS_EAL_LIST_TYPE_BYTES = 4,
    MSS_EAL_VERSION_BYTES = 4,
    MSS_EAL_VECTOR_BYTES = 12,
    MSS_EAL_NAME_BYTES = 32,
    MSS_EAL_PATH_BYTES = 260,
    MSS_EAL_GEOMETRY_MINIMUM_BYTES = 28,
    MSS_LIGHT_VIS_COMPILE_AND_QUIT_MODE = 2,
    MILES_STARTUP_FAILED = 0,
    MSS_EAX_DRY_ROOM_LEVEL = -10000,
    MSS_EAX_ROOM_ID_INVALID = 65535,
    MSS_EAX_RESULT_OK = 0,
    MSS_EAX_ENVIRONMENT_NONE = 0,
    MSS_EAX_LISTENER_ID = 0,
    MSS_EAX_SOURCE_ID = 0,
    EAX_SOURCE_PROPERTIES_SIZE = 112,
    EAX_MATERIAL_PROPERTIES_SIZE = 16,
    MSS_EAL_SOURCE_PATH = 0,
    MSS_EAL_SOURCE_MEMORY = 2,
    MSS_EAL_FILE_READ_FAILED = -1
};

typedef uint32_t(MILES_CALLBACK *eax_manager_release_method_t)(eax_manager_t *manager);
typedef int32_t(MILES_CALLBACK *eax_manager_query_interface_method_t)(eax_manager_t *manager, const void *interfaceId, void **outInterface);
typedef uint32_t(MILES_CALLBACK *eax_manager_add_ref_method_t)(eax_manager_t *manager);
typedef int32_t(MILES_CALLBACK *eax_manager_load_environment_method_t)(eax_manager_t *manager, const void *source, int32_t sourceType);
typedef int32_t(MILES_CALLBACK *eax_manager_clear_environment_method_t)(eax_manager_t *manager, int32_t environmentId);
typedef int32_t(MILES_CALLBACK *eax_manager_get_loaded_data_size_method_t)(eax_manager_t *manager, uint32_t *outSizeBytes,
                                                                           uint32_t unusedArgument);
typedef int32_t(MILES_CALLBACK *eax_manager_get_listener_position_method_t)(eax_manager_t *manager, vec3_t outPosition);
typedef int32_t(MILES_CALLBACK *eax_manager_find_named_id_method_t)(eax_manager_t *manager, const char *name, int32_t *outId);
typedef int32_t(MILES_CALLBACK *eax_manager_get_source_properties_method_t)(eax_manager_t *manager, int32_t sourceId,
                                                                            uint8_t outProperties[EAX_SOURCE_PROPERTIES_SIZE]);
typedef int32_t(MILES_CALLBACK *eax_manager_get_source_position_count_method_t)(eax_manager_t *manager, int32_t sourceId,
                                                                                int32_t *outPositionCount);
typedef int32_t(MILES_CALLBACK *eax_manager_get_source_position_method_t)(eax_manager_t *manager, int32_t sourceId, int32_t positionIndex,
                                                                          vec3_t outPosition);
typedef int32_t(MILES_CALLBACK *eax_manager_get_material_properties_method_t)(eax_manager_t *manager, int32_t materialId,
                                                                              uint8_t outProperties[EAX_MATERIAL_PROPERTIES_SIZE]);

/* Creative EAX 3 listener-property carrier consumed by Miles' "EAX all
 * parameters" preference. The Windows engine passes 112 bytes and explicitly
 * overwrites airAbsorptionHF at +92 before crossing the Miles boundary. */
typedef struct eax_listener_properties_s {
    uint32_t environment;
    float environmentSize;
    float environmentDiffusion;
    int32_t room;
    int32_t roomHF;
    int32_t roomLF;
    float decayTime;
    float decayHFRatio;
    float decayLFRatio;
    int32_t reflections;
    float reflectionsDelay;
    vec3_t reflectionsPan;
    int32_t reverb;
    float reverbDelay;
    vec3_t reverbPan;
    float echoTime;
    float echoDepth;
    float modulationTime;
    float modulationDepth;
    float airAbsorptionHF;
    float hfReference;
    float lfReference;
    float roomRolloffFactor;
    uint32_t flags;
} eax_listener_properties_t;

/* EAXMan's source-position query receives a final 28-byte work record. The
 * bundled DLL copies the queried position into +0x00 but CoDUOMP never reads
 * it. Neither binary accesses +0x0c..+0x17. CoDUOMP initializes scale at
 * +0x18, but the bundled EaxMan.dll never reads it. */
typedef struct eax_source_query_workspace_s {
    vec3_t queriedPosition; /* manager output; unused by CoDUOMP.exe */
    uint8_t unused0c[12]; /* unused by CoDUOMP.exe and bundled EaxMan.dll */
    float scale; /* written but otherwise unused by CoDUOMP.exe */
} eax_source_query_workspace_t;

typedef int32_t(MILES_CALLBACK *eax_manager_get_environment_method_t)(eax_manager_t *manager, int32_t environmentId,
                                                                      eax_listener_properties_t *properties);
typedef int32_t(MILES_CALLBACK *eax_manager_query_listener_method_t)(eax_manager_t *manager, int32_t listenerId, const vec3_t position,
                                                                     int32_t *environmentId, qboolean updateState);
typedef int32_t(MILES_CALLBACK *eax_manager_query_source_method_t)(eax_manager_t *manager, int32_t sourceId, const vec3_t position,
                                                                   int32_t *obstruction, float *obstructionLFRatio, int32_t *occlusion,
                                                                   float *occlusionLFRatio, float *occlusionRoomRatio,
                                                                   eax_source_query_workspace_t *workspace, qboolean updateState);

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(eax_listener_properties_t) == 0x04, "i386 EAX listener-properties alignment changed");
_Static_assert(offsetof(eax_listener_properties_t, environment) == 0x00, "i386 EAX environment offset changed");
_Static_assert(sizeof(((eax_listener_properties_t *)0)->environment) == 0x04, "i386 EAX environment extent changed");
_Static_assert(offsetof(eax_listener_properties_t, environmentSize) == 0x04, "i386 EAX environment-size offset changed");
_Static_assert(sizeof(((eax_listener_properties_t *)0)->environmentSize) == 0x04, "i386 EAX environment-size extent changed");
_Static_assert(offsetof(eax_listener_properties_t, environmentDiffusion) == 0x08, "i386 EAX environment-diffusion offset changed");
_Static_assert(sizeof(((eax_listener_properties_t *)0)->environmentDiffusion) == 0x04, "i386 EAX environment-diffusion extent changed");
_Static_assert(offsetof(eax_listener_properties_t, room) == 0x0c, "i386 EAX room offset changed");
_Static_assert(sizeof(((eax_listener_properties_t *)0)->room) == 0x04, "i386 EAX room extent changed");
_Static_assert(offsetof(eax_listener_properties_t, roomHF) == 0x10, "i386 EAX room-HF offset changed");
_Static_assert(sizeof(((eax_listener_properties_t *)0)->roomHF) == 0x04, "i386 EAX room-HF extent changed");
_Static_assert(offsetof(eax_listener_properties_t, roomLF) == 0x14, "i386 EAX room-LF offset changed");
_Static_assert(sizeof(((eax_listener_properties_t *)0)->roomLF) == 0x04, "i386 EAX room-LF extent changed");
_Static_assert(offsetof(eax_listener_properties_t, decayTime) == 0x18, "i386 EAX decay-time offset changed");
_Static_assert(sizeof(((eax_listener_properties_t *)0)->decayTime) == 0x04, "i386 EAX decay-time extent changed");
_Static_assert(offsetof(eax_listener_properties_t, decayHFRatio) == 0x1c, "i386 EAX decay-HF ratio offset changed");
_Static_assert(sizeof(((eax_listener_properties_t *)0)->decayHFRatio) == 0x04, "i386 EAX decay-HF ratio extent changed");
_Static_assert(offsetof(eax_listener_properties_t, decayLFRatio) == 0x20, "i386 EAX decay-LF ratio offset changed");
_Static_assert(sizeof(((eax_listener_properties_t *)0)->decayLFRatio) == 0x04, "i386 EAX decay-LF ratio extent changed");
_Static_assert(offsetof(eax_listener_properties_t, reflections) == 0x24, "i386 EAX reflections offset changed");
_Static_assert(sizeof(((eax_listener_properties_t *)0)->reflections) == 0x04, "i386 EAX reflections extent changed");
_Static_assert(offsetof(eax_listener_properties_t, reflectionsDelay) == 0x28, "i386 EAX reflections-delay offset changed");
_Static_assert(sizeof(((eax_listener_properties_t *)0)->reflectionsDelay) == 0x04, "i386 EAX reflections-delay extent changed");
_Static_assert(offsetof(eax_listener_properties_t, reflectionsPan) == 0x2c, "i386 EAX reflections-pan offset changed");
_Static_assert(sizeof(((eax_listener_properties_t *)0)->reflectionsPan) == 0x0c, "i386 EAX reflections-pan extent changed");
_Static_assert(offsetof(eax_listener_properties_t, reverb) == 0x38, "i386 EAX reverb offset changed");
_Static_assert(sizeof(((eax_listener_properties_t *)0)->reverb) == 0x04, "i386 EAX reverb extent changed");
_Static_assert(offsetof(eax_listener_properties_t, reverbDelay) == 0x3c, "i386 EAX reverb-delay offset changed");
_Static_assert(sizeof(((eax_listener_properties_t *)0)->reverbDelay) == 0x04, "i386 EAX reverb-delay extent changed");
_Static_assert(offsetof(eax_listener_properties_t, reverbPan) == 0x40, "i386 EAX reverb-pan offset changed");
_Static_assert(sizeof(((eax_listener_properties_t *)0)->reverbPan) == 0x0c, "i386 EAX reverb-pan extent changed");
_Static_assert(offsetof(eax_listener_properties_t, echoTime) == 0x4c, "i386 EAX echo-time offset changed");
_Static_assert(sizeof(((eax_listener_properties_t *)0)->echoTime) == 0x04, "i386 EAX echo-time extent changed");
_Static_assert(offsetof(eax_listener_properties_t, echoDepth) == 0x50, "i386 EAX echo-depth offset changed");
_Static_assert(sizeof(((eax_listener_properties_t *)0)->echoDepth) == 0x04, "i386 EAX echo-depth extent changed");
_Static_assert(offsetof(eax_listener_properties_t, modulationTime) == 0x54, "i386 EAX modulation-time offset changed");
_Static_assert(sizeof(((eax_listener_properties_t *)0)->modulationTime) == 0x04, "i386 EAX modulation-time extent changed");
_Static_assert(offsetof(eax_listener_properties_t, modulationDepth) == 0x58, "i386 EAX modulation-depth offset changed");
_Static_assert(sizeof(((eax_listener_properties_t *)0)->modulationDepth) == 0x04, "i386 EAX modulation-depth extent changed");
_Static_assert(offsetof(eax_listener_properties_t, airAbsorptionHF) == 0x5c, "i386 EAX air-absorption offset changed");
_Static_assert(sizeof(((eax_listener_properties_t *)0)->airAbsorptionHF) == 0x04, "i386 EAX air-absorption extent changed");
_Static_assert(offsetof(eax_listener_properties_t, hfReference) == 0x60, "i386 EAX HF-reference offset changed");
_Static_assert(sizeof(((eax_listener_properties_t *)0)->hfReference) == 0x04, "i386 EAX HF-reference extent changed");
_Static_assert(offsetof(eax_listener_properties_t, lfReference) == 0x64, "i386 EAX LF-reference offset changed");
_Static_assert(sizeof(((eax_listener_properties_t *)0)->lfReference) == 0x04, "i386 EAX LF-reference extent changed");
_Static_assert(offsetof(eax_listener_properties_t, roomRolloffFactor) == 0x68, "i386 EAX room-rolloff offset changed");
_Static_assert(sizeof(((eax_listener_properties_t *)0)->roomRolloffFactor) == 0x04, "i386 EAX room-rolloff extent changed");
_Static_assert(offsetof(eax_listener_properties_t, flags) == 0x6c, "i386 EAX flags offset changed");
_Static_assert(sizeof(((eax_listener_properties_t *)0)->flags) == 0x04, "i386 EAX flags extent changed");
_Static_assert(sizeof(eax_listener_properties_t) == 0x70, "original i386 EAX listener-properties size changed");

_Static_assert(_Alignof(eax_source_query_workspace_t) == 0x04, "i386 EAX source-workspace alignment changed");
_Static_assert(offsetof(eax_source_query_workspace_t, queriedPosition) == 0x00, "i386 EAX source-workspace position offset changed");
_Static_assert(sizeof(((eax_source_query_workspace_t *)0)->queriedPosition) == 0x0c, "i386 EAX source-workspace position extent changed");
_Static_assert(offsetof(eax_source_query_workspace_t, unused0c) == 0x0c, "i386 EAX source-workspace unused lane offset changed");
_Static_assert(sizeof(((eax_source_query_workspace_t *)0)->unused0c) == 0x0c, "i386 EAX source-workspace unused lane extent changed");
_Static_assert(offsetof(eax_source_query_workspace_t, scale) == 0x18, "i386 EAX source-workspace scale offset changed");
_Static_assert(sizeof(((eax_source_query_workspace_t *)0)->scale) == 0x04, "i386 EAX source-workspace scale extent changed");
_Static_assert(sizeof(eax_source_query_workspace_t) == 0x1c, "original i386 EAX source-workspace size changed");
#endif

/* Complete CoDUOMP-visible EAXMan COM vtable prefix. EaxMan.dll's table at RVA
 * 0x0000e188 proves all eighteen slots; its method bodies prove the semantic
 * names below even for methods that the engine never calls. */
typedef struct eax_manager_vtable_s {
    eax_manager_query_interface_method_t queryInterface;
    eax_manager_add_ref_method_t addRef;
    eax_manager_release_method_t release;
    /* EaxMan.dll RVA 0x000016a0 sums loaded EAL dynamic allocations. The
     * bundled DLL ignores the final argument; its public parameter identity
     * remains unknown. Unused by CoDUOMP.exe. */
    eax_manager_get_loaded_data_size_method_t getLoadedDataSize;
    eax_manager_load_environment_method_t loadEnvironment;
    eax_manager_clear_environment_method_t clearEnvironment;
    /* EaxMan.dll RVA 0x00002530. */
    eax_manager_get_listener_position_method_t getListenerPosition; /* unused by CoDUOMP.exe */
    /* EaxMan.dll RVA 0x000025f0. */
    eax_manager_find_named_id_method_t findSourceId; /* unused by CoDUOMP.exe */
    /* EaxMan.dll RVA 0x00002610. */
    eax_manager_get_source_properties_method_t getSourceProperties; /* unused by CoDUOMP.exe */
    /* EaxMan.dll RVA 0x00002680. */
    eax_manager_get_source_position_count_method_t getSourcePositionCount; /* unused by CoDUOMP.exe */
    /* EaxMan.dll RVA 0x000026d0. */
    eax_manager_get_source_position_method_t getSourcePosition; /* unused by CoDUOMP.exe */
    /* EaxMan.dll RVA 0x00002560. */
    eax_manager_find_named_id_method_t findEnvironmentId; /* unused by CoDUOMP.exe */
    eax_manager_get_environment_method_t getEnvironment;
    /* EaxMan.dll RVA 0x00002750. */
    eax_manager_find_named_id_method_t findMaterialId; /* unused by CoDUOMP.exe */
    /* EaxMan.dll RVA 0x00002770. */
    eax_manager_get_material_properties_method_t getMaterialProperties; /* unused by CoDUOMP.exe */
    /* EaxMan.dll RVA 0x000027f0. */
    eax_manager_find_named_id_method_t findGeometryId; /* unused by CoDUOMP.exe */
    eax_manager_query_listener_method_t queryListener;
    eax_manager_query_source_method_t querySource;
} eax_manager_vtable_t;

/* Engine-visible COM interface prefix only. EaxMan.dll allocates the concrete
 * 0x484-byte implementation; CoDUOMP only dereferences this leading vtable. */
struct eax_manager_s {
    const eax_manager_vtable_t *vtable;
};

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(eax_manager_vtable_t) == 0x04, "i386 EAX manager-vtable alignment changed");
_Static_assert(offsetof(eax_manager_vtable_t, queryInterface) == 0x00, "i386 EAX QueryInterface slot changed");
_Static_assert(sizeof(((eax_manager_vtable_t *)0)->queryInterface) == 0x04, "i386 EAX QueryInterface slot extent changed");
_Static_assert(offsetof(eax_manager_vtable_t, addRef) == 0x04, "i386 EAX AddRef slot changed");
_Static_assert(sizeof(((eax_manager_vtable_t *)0)->addRef) == 0x04, "i386 EAX AddRef slot extent changed");
_Static_assert(offsetof(eax_manager_vtable_t, release) == 0x08, "i386 EAX Release slot changed");
_Static_assert(sizeof(((eax_manager_vtable_t *)0)->release) == 0x04, "i386 EAX Release slot extent changed");
_Static_assert(offsetof(eax_manager_vtable_t, getLoadedDataSize) == 0x0c, "i386 EAX loaded-data-size slot changed");
_Static_assert(sizeof(((eax_manager_vtable_t *)0)->getLoadedDataSize) == 0x04, "i386 EAX loaded-data-size slot extent changed");
_Static_assert(offsetof(eax_manager_vtable_t, loadEnvironment) == 0x10, "i386 EAX load-environment slot changed");
_Static_assert(sizeof(((eax_manager_vtable_t *)0)->loadEnvironment) == 0x04, "i386 EAX load-environment slot extent changed");
_Static_assert(offsetof(eax_manager_vtable_t, clearEnvironment) == 0x14, "i386 EAX clear-environment slot changed");
_Static_assert(sizeof(((eax_manager_vtable_t *)0)->clearEnvironment) == 0x04, "i386 EAX clear-environment slot extent changed");
_Static_assert(offsetof(eax_manager_vtable_t, getListenerPosition) == 0x18, "i386 EAX listener-position slot changed");
_Static_assert(sizeof(((eax_manager_vtable_t *)0)->getListenerPosition) == 0x04, "i386 EAX listener-position slot extent changed");
_Static_assert(offsetof(eax_manager_vtable_t, findSourceId) == 0x1c, "i386 EAX source-lookup slot changed");
_Static_assert(sizeof(((eax_manager_vtable_t *)0)->findSourceId) == 0x04, "i386 EAX source-lookup slot extent changed");
_Static_assert(offsetof(eax_manager_vtable_t, getSourceProperties) == 0x20, "i386 EAX source-properties slot changed");
_Static_assert(sizeof(((eax_manager_vtable_t *)0)->getSourceProperties) == 0x04, "i386 EAX source-properties slot extent changed");
_Static_assert(offsetof(eax_manager_vtable_t, getSourcePositionCount) == 0x24, "i386 EAX source-position-count slot changed");
_Static_assert(sizeof(((eax_manager_vtable_t *)0)->getSourcePositionCount) == 0x04, "i386 EAX source-position-count slot extent changed");
_Static_assert(offsetof(eax_manager_vtable_t, getSourcePosition) == 0x28, "i386 EAX source-position slot changed");
_Static_assert(sizeof(((eax_manager_vtable_t *)0)->getSourcePosition) == 0x04, "i386 EAX source-position slot extent changed");
_Static_assert(offsetof(eax_manager_vtable_t, findEnvironmentId) == 0x2c, "i386 EAX environment-lookup slot changed");
_Static_assert(sizeof(((eax_manager_vtable_t *)0)->findEnvironmentId) == 0x04, "i386 EAX environment-lookup slot extent changed");
_Static_assert(offsetof(eax_manager_vtable_t, getEnvironment) == 0x30, "i386 EAX get-environment slot changed");
_Static_assert(sizeof(((eax_manager_vtable_t *)0)->getEnvironment) == 0x04, "i386 EAX get-environment slot extent changed");
_Static_assert(offsetof(eax_manager_vtable_t, findMaterialId) == 0x34, "i386 EAX material-lookup slot changed");
_Static_assert(sizeof(((eax_manager_vtable_t *)0)->findMaterialId) == 0x04, "i386 EAX material-lookup slot extent changed");
_Static_assert(offsetof(eax_manager_vtable_t, getMaterialProperties) == 0x38, "i386 EAX material-properties slot changed");
_Static_assert(sizeof(((eax_manager_vtable_t *)0)->getMaterialProperties) == 0x04, "i386 EAX material-properties slot extent changed");
_Static_assert(offsetof(eax_manager_vtable_t, findGeometryId) == 0x3c, "i386 EAX geometry-lookup slot changed");
_Static_assert(sizeof(((eax_manager_vtable_t *)0)->findGeometryId) == 0x04, "i386 EAX geometry-lookup slot extent changed");
_Static_assert(offsetof(eax_manager_vtable_t, queryListener) == 0x40, "i386 EAX query-listener slot changed");
_Static_assert(sizeof(((eax_manager_vtable_t *)0)->queryListener) == 0x04, "i386 EAX query-listener slot extent changed");
_Static_assert(offsetof(eax_manager_vtable_t, querySource) == 0x44, "i386 EAX query-source slot changed");
_Static_assert(sizeof(((eax_manager_vtable_t *)0)->querySource) == 0x04, "i386 EAX query-source slot extent changed");
_Static_assert(sizeof(eax_manager_vtable_t) == 0x48, "original i386 EAX manager-vtable prefix size changed");

_Static_assert(_Alignof(eax_manager_t) == 0x04, "i386 EAX manager-interface alignment changed");
_Static_assert(offsetof(eax_manager_t, vtable) == 0x00, "i386 EAX manager vtable-pointer offset changed");
_Static_assert(sizeof(((eax_manager_t *)0)->vtable) == 0x04, "i386 EAX manager vtable-pointer extent changed");
#endif

typedef int32_t(MILES_CALLBACK *eax_manager_create_t)(eax_manager_t **outManager);

/* Source: CoDUOMP.exe pointer table 0x005ca2e8 and strings
 * 0x0059b86c..0x0059b977. The table is stored in reverse address order but
 * indexes these room-type names in the source order below.
 * PE_RELOCATION_VALUES_VERIFIED: all 26 string targets match the PE. */
static const char *const mss_roomTypeNames[MSS_ROOM_TYPE_COUNT] = {
    "generic",     "paddedcell", "room",    "bathroom",  "livingroom",      "stoneroom", "auditorium",
    "concerthall", "cave",       "arena",   "hangar",    "carpetedhallway", "hallway",   "stonecorridor",
    "alley",       "forest",     "city",    "mountains", "quarry",          "plain",     "parkinglot",
    "sewerpipe",   "underwater", "drugged", "dizzy",     "psychotic"};

/* Source literal 0.8f at CoDUOMP.exe 0x005b9c20 (0x3f4ccccd). */
#define MSS_ALIAS_VOLUME_SCALE 0.800000011920929f
#define MSS_DRY_REVERB_LEVEL 0.0f
#define MSS_SILENT_VOLUME 0.0f
#define MSS_FULL_VOLUME 1.0f
/* Exact 1/32768 float at CoDUOMP.exe 0x005b9b68 (0x38000000). */
#define MSS_RANDOM_UNIT 0.000030517578125f
/* Packed negative blend values encode pitch percent in their whole part. */
#define MSS_ENCODED_PITCH_PERCENT_SCALE 0.009999999776482582f
/* Exact 1.25f at CoDUOMP.exe 0x005b9d78 (0x3fa00000). */
#define MSS_OVERLAY_VOLUME_SCALE 1.25f

typedef enum milesDigitalFormat_e {
    MILES_DIGITAL_FORMAT_8_BIT = 1,
    MILES_DIGITAL_FORMAT_16_BIT = 2
} milesDigitalFormat_t;

/* Raw private savegame payloads written and read by the paired MSS channel and
 * environment routines. The original passes each complete record to
 * MSS_Write/MSS_Read, and the restart paths consume every field below; none
 * of these four records has padding. */
typedef struct mss_3d_channel_save_s {
    float startFraction;
    float aliasPitchScale;
    float logicalVolume;
    vec3_t position;
} mss_3d_channel_save_t;

typedef struct mss_2d_channel_save_s {
    float startFraction;
    float aliasPitchScale;
    float logicalVolume;
    float pan;
} mss_2d_channel_save_t;

typedef struct mss_stream_channel_save_s {
    float startFraction;
    int32_t basePlaybackRate;
    float logicalVolume;
    float relativeVolume;
    float pan;
    vec3_t position;
} mss_stream_channel_save_t;

typedef struct mss_environment_save_s {
    int32_t roomType;
    float reverbLevel;
    float reverbTarget;
    float reverbRatePerMsec;
} mss_environment_save_t;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(mss_3d_channel_save_t) == 0x04, "i386 MSS 3D-channel save alignment changed");
_Static_assert(offsetof(mss_3d_channel_save_t, startFraction) == 0x00, "i386 MSS 3D-channel start-fraction offset changed");
_Static_assert(sizeof(((mss_3d_channel_save_t *)0)->startFraction) == 0x04, "i386 MSS 3D-channel start-fraction extent changed");
_Static_assert(offsetof(mss_3d_channel_save_t, aliasPitchScale) == 0x04, "i386 MSS 3D-channel pitch offset changed");
_Static_assert(sizeof(((mss_3d_channel_save_t *)0)->aliasPitchScale) == 0x04, "i386 MSS 3D-channel pitch extent changed");
_Static_assert(offsetof(mss_3d_channel_save_t, logicalVolume) == 0x08, "i386 MSS 3D-channel volume offset changed");
_Static_assert(sizeof(((mss_3d_channel_save_t *)0)->logicalVolume) == 0x04, "i386 MSS 3D-channel volume extent changed");
_Static_assert(offsetof(mss_3d_channel_save_t, position) == 0x0c, "i386 MSS 3D-channel position offset changed");
_Static_assert(sizeof(((mss_3d_channel_save_t *)0)->position) == 0x0c, "i386 MSS 3D-channel position extent changed");
_Static_assert(sizeof(mss_3d_channel_save_t) == 0x18, "i386 MSS 3D-channel save size changed");

_Static_assert(_Alignof(mss_2d_channel_save_t) == 0x04, "i386 MSS 2D-channel save alignment changed");
_Static_assert(offsetof(mss_2d_channel_save_t, startFraction) == 0x00, "i386 MSS 2D-channel start-fraction offset changed");
_Static_assert(sizeof(((mss_2d_channel_save_t *)0)->startFraction) == 0x04, "i386 MSS 2D-channel start-fraction extent changed");
_Static_assert(offsetof(mss_2d_channel_save_t, aliasPitchScale) == 0x04, "i386 MSS 2D-channel pitch offset changed");
_Static_assert(sizeof(((mss_2d_channel_save_t *)0)->aliasPitchScale) == 0x04, "i386 MSS 2D-channel pitch extent changed");
_Static_assert(offsetof(mss_2d_channel_save_t, logicalVolume) == 0x08, "i386 MSS 2D-channel volume offset changed");
_Static_assert(sizeof(((mss_2d_channel_save_t *)0)->logicalVolume) == 0x04, "i386 MSS 2D-channel volume extent changed");
_Static_assert(offsetof(mss_2d_channel_save_t, pan) == 0x0c, "i386 MSS 2D-channel pan offset changed");
_Static_assert(sizeof(((mss_2d_channel_save_t *)0)->pan) == 0x04, "i386 MSS 2D-channel pan extent changed");
_Static_assert(sizeof(mss_2d_channel_save_t) == 0x10, "i386 MSS 2D-channel save size changed");

_Static_assert(_Alignof(mss_stream_channel_save_t) == 0x04, "i386 MSS stream-channel save alignment changed");
_Static_assert(offsetof(mss_stream_channel_save_t, startFraction) == 0x00, "i386 MSS stream start-fraction offset changed");
_Static_assert(sizeof(((mss_stream_channel_save_t *)0)->startFraction) == 0x04, "i386 MSS stream start-fraction extent changed");
_Static_assert(offsetof(mss_stream_channel_save_t, basePlaybackRate) == 0x04, "i386 MSS stream playback-rate offset changed");
_Static_assert(sizeof(((mss_stream_channel_save_t *)0)->basePlaybackRate) == 0x04, "i386 MSS stream playback-rate extent changed");
_Static_assert(offsetof(mss_stream_channel_save_t, logicalVolume) == 0x08, "i386 MSS stream volume offset changed");
_Static_assert(sizeof(((mss_stream_channel_save_t *)0)->logicalVolume) == 0x04, "i386 MSS stream volume extent changed");
_Static_assert(offsetof(mss_stream_channel_save_t, relativeVolume) == 0x0c, "i386 MSS stream sample-volume offset changed");
_Static_assert(sizeof(((mss_stream_channel_save_t *)0)->relativeVolume) == 0x04, "i386 MSS stream sample-volume extent changed");
_Static_assert(offsetof(mss_stream_channel_save_t, pan) == 0x10, "i386 MSS stream pan offset changed");
_Static_assert(sizeof(((mss_stream_channel_save_t *)0)->pan) == 0x04, "i386 MSS stream pan extent changed");
_Static_assert(offsetof(mss_stream_channel_save_t, position) == 0x14, "i386 MSS stream position offset changed");
_Static_assert(sizeof(((mss_stream_channel_save_t *)0)->position) == 0x0c, "i386 MSS stream position extent changed");
_Static_assert(sizeof(mss_stream_channel_save_t) == 0x20, "i386 MSS stream-channel save size changed");

_Static_assert(_Alignof(mss_environment_save_t) == 0x04, "i386 MSS environment save alignment changed");
_Static_assert(offsetof(mss_environment_save_t, roomType) == 0x00, "i386 MSS environment room-type offset changed");
_Static_assert(sizeof(((mss_environment_save_t *)0)->roomType) == 0x04, "i386 MSS environment room-type extent changed");
_Static_assert(offsetof(mss_environment_save_t, reverbLevel) == 0x04, "i386 MSS environment reverb-level offset changed");
_Static_assert(sizeof(((mss_environment_save_t *)0)->reverbLevel) == 0x04, "i386 MSS environment reverb-level extent changed");
_Static_assert(offsetof(mss_environment_save_t, reverbTarget) == 0x08, "i386 MSS environment reverb-target offset changed");
_Static_assert(sizeof(((mss_environment_save_t *)0)->reverbTarget) == 0x04, "i386 MSS environment reverb-target extent changed");
_Static_assert(offsetof(mss_environment_save_t, reverbRatePerMsec) == 0x0c, "i386 MSS environment reverb-rate offset changed");
_Static_assert(sizeof(((mss_environment_save_t *)0)->reverbRatePerMsec) == 0x04, "i386 MSS environment reverb-rate extent changed");
_Static_assert(sizeof(mss_environment_save_t) == 0x10, "i386 MSS environment save size changed");
#endif

cvar_t *mss_stereo;                       /* original 0x0491cd50 */
cvar_t *mss_bits;                         /* original 0x0491cd58 */
cvar_t *mss_khz;                          /* original 0x0491cd5c */
cvar_t *mss_q3fs;                         /* original 0x0491cd80 */
cvar_t *mss_errorOnMissing;               /* original 0x0491cd54 */
cvar_t *mss_3d_provider;                  /* original 0x0491cd68 */
cvar_t *mss_volume;                       /* original 0x0491cd64 */
cvar_t *mss_roomtype;                     /* original 0x0491cd78 */
cvar_t *mss_wetlevel;                     /* original 0x0491cd7c */
miles_digital_driver_t mss_digitalDriver; /* original 0x009cbeb8 */
int32_t mss_sampleRate;                   /* original 0x009cbec0 */
int32_t mss_sampleBits;                   /* original 0x009cbec4 */
int32_t mss_channelCount;                 /* original 0x009cbec8 */
float mss_playbackRateScale;              /* original 0x009cbecc */
float mss_effectVolume;                   /* original 0x009cbee4 */
mss_channel_volume_t mss_masterVolume;    /* original 0x009cbee8 */
mss_channel_volume_t mss_channelVolumes[SND_ALIAS_CHANNEL_COUNT]; /* original 0x009cbef4 */
int32_t mss_roomType;                     /* original 0x009cbf98 */
float mss_reverbLevel;                    /* original 0x009cbf9c */
float mss_reverbTarget;                   /* original 0x009cbfa0 */
float mss_reverbRate;                     /* original 0x009cbfa4 */
qboolean mss_underwaterEffectActive;      /* original 0x0389fdb0 */
eax_manager_t *mss_eaxManager;            /* original 0x0491cd60 */
void *mss_eaxLibrary;                     /* original 0x0491cd70 */
qboolean mss_eaxEnvironmentLoaded;        /* original 0x0491cd74 */
int32_t mss_eaxRoomId;                    /* original 0x0491cd6c */
static qboolean mss_eaxApplyObstruction = qtrue; /* original 0x005ca2dc */
static qboolean mss_eaxApplyOcclusion = qtrue;   /* original 0x005ca2e0 */
static qboolean mss_eaxApplyListener = qtrue;    /* original 0x005ca2e4 */
int32_t mss_2dChannelCount;               /* original 0x009cd2c4 */
int32_t mss_streamChannelCount;           /* original 0x009cd2cc */
miles_3d_provider_t mss_3dProvider;       /* original 0x009cbebc */
int32_t mss_max3DChannels;                /* original 0x009cd2c8 */
qboolean mss_eaxAvailable;                /* original 0x0491cd84 */
char mss_eaxMapName[MAX_QPATH];           /* original 0x0389fd70 */
/* NOT_FROM_ORIGINAL_SOURCE: distinguishes a safely truncated rejected map
 * name from a valid 63-byte name so later map changes are compared correctly. */
static qboolean mss_eaxMapNameWasTruncated;
vec3_t mss_listenerOrigin;                /* original 0x009cbfa8 */
axis_t mss_listenerAxis;                  /* original 0x009cbfb4 */
int32_t mss_listenerTime;                 /* original 0x009cbfd8 */
miles_sample_handle_t mss_2dSampleHandles[MSS_2D_CHANNEL_CAPACITY];      /* 0x009cd0c0 */
miles_3d_sample_handle_t mss_3dSampleHandles[MSS_3D_CHANNEL_CAPACITY];      /* 0x009cd140 */
miles_stream_handle_t mss_streamHandles[MSS_STREAM_CHANNEL_CAPACITY];    /* 0x009cd1c0 */
int32_t mss_ambientBackgroundIndex;       /* original 0x009cbf94 */
mss_background_fade_t mss_backgroundFades[MSS_ALIAS_STREAM_SLOT_FIRST]; /* 0x009cbf6c */
mss_raw_sample_state_t mss_rawSampleState; /* original 0x009cd2d0 */
qboolean mss_paused;                      /* original 0x009cbed0 */
int32_t mss_pauseStartTime;               /* original 0x009cbed4 */
int32_t mss_cpuPercent;                   /* original 0x009cbed8 */
uint8_t *mss_restoreBuffer;               /* original 0x009cbedc */
int32_t mss_restoreSize;                  /* original 0x009cbee0 */
int32_t mss_soundTime;                    /* original 0x009cbfdc */
int32_t mss_lastSoundTime;                /* original 0x009cbfe0 */
qboolean mss_anyMasters;                  /* original 0x009cbfe4 */
mss_channel_info_t mss_channelInfo[MSS_TOTAL_CHANNEL_COUNT]; /* original 0x009cbfe8 */
mss_stream_channel_t mss_streamChannels[MSS_STREAM_CHANNEL_CAPACITY]; /* original 0x009cd1f4 */

/* Source: CoDUOMP.exe 0x004508c0..0x004508d8.
 * Name: exact same-module Mac symbol StripExtension. This translation-unit
 * duplicate has the same instructions as Com_StripExtension at 0x0044f330;
 * it is retained because both bodies independently exist in the executable. */
void StripExtension(const char *input, char *output)
{
    while (*input != '\0' && *input != '.')
        *output++ = *input++;
    *output = '\0';
}

/* NOT_FROM_ORIGINAL_SOURCE: validate the loaded audio data and playback state before crossing the Miles boundary. */
static qboolean coduomp_mss_validate_riff_extent(const void *fileData, int32_t fileLength, const char *expectedForm,
                                                 const char *payloadChunkId, const void *payload, uint32_t payloadLength,
                                                 qboolean requireCompleteStream)
{
    if (fileData == NULL || expectedForm == NULL || fileLength < MSS_RIFF_FILE_HEADER_BYTES) {
        return qfalse;
    }

    const uint8_t *const bytes = fileData;
    if (memcmp(bytes, "RIFF", MSS_RIFF_FOURCC_BYTES) != 0 ||
        memcmp(bytes + MSS_RIFF_FORM_OFFSET, expectedForm, MSS_RIFF_FOURCC_BYTES) != 0) {
        return qfalse;
    }

    const uint32_t riffSize = (uint32_t)bytes[MSS_RIFF_SIZE_OFFSET] | ((uint32_t)bytes[MSS_RIFF_SIZE_OFFSET + 1] << 8) |
                              ((uint32_t)bytes[MSS_RIFF_SIZE_OFFSET + 2] << 16) | ((uint32_t)bytes[MSS_RIFF_SIZE_OFFSET + 3] << 24);
    const uint32_t loadedLength = (uint32_t)fileLength;
    if (riffSize < MSS_RIFF_FOURCC_BYTES || riffSize > loadedLength - MSS_RIFF_CHUNK_HEADER_BYTES) {
        return qfalse;
    }

    const uint32_t riffEnd = riffSize + MSS_RIFF_CHUNK_HEADER_BYTES;
    if (requireCompleteStream == qfalse && payloadChunkId == NULL)
        return qtrue;

    uint32_t offset = MSS_RIFF_FILE_HEADER_BYTES;
    qboolean payloadIsBounded = payloadChunkId == NULL ? qtrue : qfalse;

    while (offset < riffEnd) {
        if (riffEnd - offset < MSS_RIFF_CHUNK_HEADER_BYTES)
            return qfalse;

        const uint32_t chunkSize = (uint32_t)bytes[offset + MSS_RIFF_CHUNK_SIZE_OFFSET] |
                                   ((uint32_t)bytes[offset + MSS_RIFF_CHUNK_SIZE_OFFSET + 1] << 8) |
                                   ((uint32_t)bytes[offset + MSS_RIFF_CHUNK_SIZE_OFFSET + 2] << 16) |
                                   ((uint32_t)bytes[offset + MSS_RIFF_CHUNK_SIZE_OFFSET + 3] << 24);
        const uint32_t dataOffset = offset + MSS_RIFF_CHUNK_HEADER_BYTES;
        if (chunkSize > riffEnd - dataOffset)
            return qfalse;

        if (payloadChunkId != NULL && memcmp(bytes + offset, payloadChunkId, MSS_RIFF_FOURCC_BYTES) == 0 &&
            payload == (const void *)(bytes + dataOffset) && payloadLength <= chunkSize) {
            payloadIsBounded = qtrue;
            if (requireCompleteStream == qfalse)
                return qtrue;
        }

        const uint32_t paddedSize = chunkSize + (chunkSize & 1u);
        if (paddedSize > riffEnd - dataOffset)
            return qfalse;
        offset = dataOffset + paddedSize;
    }

    return payloadIsBounded;
}

/* NOT_FROM_ORIGINAL_SOURCE: report a deterministic compatibility repair. */
static void coduomp_mss_warn_wav_repair(const char *path, const char *field, uint32_t oldValue, uint32_t newValue)
{
    Com_Printf("^3WARNING: repaired WAV '%s' %s (%u -> %u)\n", path, field, oldValue, newValue);
}

#if !defined(_WIN32)
/* NOT_FROM_ORIGINAL_SOURCE: a bounded top-level chunk found in a loaded WAV. */
typedef struct coduompMssWavChunk_s {
    uint8_t *data;
    uint32_t declaredSize;
    uint32_t availableSize;
} coduompMssWavChunk_t;

/* NOT_FROM_ORIGINAL_SOURCE: read a little-endian WAV word without relying on
 * host alignment. */
static uint16_t coduomp_mss_read_wav_u16(const uint8_t *bytes)
{
    return (uint16_t)bytes[0] | (uint16_t)((uint16_t)bytes[1] << 8);
}

/* NOT_FROM_ORIGINAL_SOURCE: read a little-endian RIFF dword without relying
 * on host alignment. */
static uint32_t coduomp_mss_read_wav_u32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

/* NOT_FROM_ORIGINAL_SOURCE: report why a loaded WAV cannot be represented by
 * the native OpenAL/miniaudio descriptor without guessing. */
static qboolean coduomp_mss_reject_wav(const char *path, const char *reason)
{
    Com_Printf("^3WARNING: WAV '%s' cannot be decoded without guessing: %s\n", path, reason);
    return qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: mirror the bundled AIL_WAV_info first-match,
 * case-insensitive top-level chunk search without reading outside the loaded
 * file. A target chunk may declare more payload than remains; the caller then
 * has both the declared and physically available extents. */
static qboolean coduomp_mss_find_loaded_wav_chunk(uint8_t *bytes, uint32_t loadedLength, uint32_t scanEnd, const char *chunkId,
                                                  coduompMssWavChunk_t *outChunk)
{
    uint32_t offset = MSS_RIFF_FILE_HEADER_BYTES;
    memset(outChunk, 0, sizeof(*outChunk));
    if (scanEnd > loadedLength)
        scanEnd = loadedLength;

    while (offset < scanEnd) {
        if (offset > loadedLength || loadedLength - offset < MSS_RIFF_CHUNK_HEADER_BYTES) {
            return qfalse;
        }

        const uint32_t chunkSize = coduomp_mss_read_wav_u32(bytes + offset + MSS_RIFF_CHUNK_SIZE_OFFSET);
        const uint32_t dataOffset = offset + MSS_RIFF_CHUNK_HEADER_BYTES;
        if (Q_stricmpn((const char *)bytes + offset, chunkId, MSS_RIFF_FOURCC_BYTES) == 0) {
            outChunk->data = bytes + dataOffset;
            outChunk->declaredSize = chunkSize;
            outChunk->availableSize = loadedLength - dataOffset;
            if (outChunk->availableSize > chunkSize)
                outChunk->availableSize = chunkSize;
            return qtrue;
        }

        const uint64_t nextOffset = (uint64_t)dataOffset + chunkSize + (chunkSize & 1u);
        if (nextOffset >= scanEnd || nextOffset > UINT32_MAX)
            return qfalse;
        offset = (uint32_t)nextOffset;
    }
    return qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: construct the native sound descriptor from a
 * length-bounded WAV before the source-visible OpenAL/miniaudio adapters see
 * audio data. It accepts harmless RIFF quirks and performs only repairs whose
 * value is uniquely determined by the loaded bytes and the declared codec. */
static qboolean coduomp_mss_parse_loaded_wav(void *fileData, int32_t fileLength, const char *path, miles_sound_info_t *soundInfo)
{
    if (fileData == NULL || path == NULL || soundInfo == NULL || fileLength < MSS_RIFF_FILE_HEADER_BYTES) {
        return qfalse;
    }

    uint8_t *const bytes = fileData;
    if (Q_stricmpn((const char *)bytes + MSS_RIFF_FORM_OFFSET, "WAVE", MSS_RIFF_FOURCC_BYTES) != 0) {
        return coduomp_mss_reject_wav(path, "missing WAVE form");
    }

    const uint32_t loadedLength = (uint32_t)fileLength;
    const uint32_t declaredScanEnd = coduomp_mss_read_wav_u32(bytes + MSS_RIFF_SIZE_OFFSET);
    uint32_t scanEnd = declaredScanEnd;
    if (scanEnd > loadedLength) {
        scanEnd = loadedLength;
        coduomp_mss_warn_wav_repair(path, "RIFF scan extent", declaredScanEnd, scanEnd);
    }

    coduompMssWavChunk_t formatChunk;
    coduompMssWavChunk_t dataChunk;
    qboolean foundFormat = coduomp_mss_find_loaded_wav_chunk(bytes, loadedLength, scanEnd, "fmt ", &formatChunk);
    qboolean foundData = coduomp_mss_find_loaded_wav_chunk(bytes, loadedLength, scanEnd, "data", &dataChunk);

    if ((foundFormat == qfalse || foundData == qfalse) && scanEnd < loadedLength) {
        if (foundFormat == qfalse) {
            foundFormat = coduomp_mss_find_loaded_wav_chunk(bytes, loadedLength, loadedLength, "fmt ", &formatChunk);
        }
        if (foundData == qfalse) {
            foundData = coduomp_mss_find_loaded_wav_chunk(bytes, loadedLength, loadedLength, "data", &dataChunk);
        }
        if (foundFormat != qfalse && foundData != qfalse) {
            coduomp_mss_warn_wav_repair(path, "short RIFF scan extent", scanEnd, loadedLength);
            scanEnd = loadedLength;
        }
    }

    if (foundFormat == qfalse || foundData == qfalse)
        return coduomp_mss_reject_wav(path, "missing fmt or data chunk");
    if (formatChunk.declaredSize < MSS_WAVE_FORMAT_BASE_BYTES || formatChunk.availableSize < MSS_WAVE_FORMAT_BASE_BYTES) {
        return coduomp_mss_reject_wav(path, "truncated base format record");
    }

    const uint16_t formatTag = coduomp_mss_read_wav_u16(formatChunk.data + MSS_WAVE_FORMAT_TAG_OFFSET);
    const uint16_t channelCount = coduomp_mss_read_wav_u16(formatChunk.data + MSS_WAVE_CHANNEL_COUNT_OFFSET);
    uint32_t sampleRate = coduomp_mss_read_wav_u32(formatChunk.data + MSS_WAVE_SAMPLE_RATE_OFFSET);
    const uint32_t averageBytesPerSecond = coduomp_mss_read_wav_u32(formatChunk.data + MSS_WAVE_AVERAGE_BYTES_PER_SECOND_OFFSET);
    uint16_t blockSize = coduomp_mss_read_wav_u16(formatChunk.data + MSS_WAVE_BLOCK_SIZE_OFFSET);
    const uint16_t bitsPerSample = coduomp_mss_read_wav_u16(formatChunk.data + MSS_WAVE_BITS_PER_SAMPLE_OFFSET);

    if (formatTag != MILES_WAVE_FORMAT_PCM && formatTag != MILES_WAVE_FORMAT_IMA_ADPCM) {
        return coduomp_mss_reject_wav(path, "unsupported format tag");
    }
    if (channelCount != MILES_CHANNEL_COUNT_MONO && channelCount != MILES_CHANNEL_COUNT_STEREO) {
        return coduomp_mss_reject_wav(path, "unsupported channel count");
    }

    uint32_t dataLength = dataChunk.declaredSize;
    if (dataLength > dataChunk.availableSize) {
        dataLength = dataChunk.availableSize;
        coduomp_mss_warn_wav_repair(path, "data length", dataChunk.declaredSize, dataLength);
    }

    uint32_t sampleCount;
    if (formatTag == MILES_WAVE_FORMAT_PCM) {
        if (bitsPerSample != MILES_SAMPLE_BITS_8 && bitsPerSample != MILES_SAMPLE_BITS_16) {
            return coduomp_mss_reject_wav(path, "unsupported PCM bit depth");
        }

        const uint16_t derivedBlockSize = (uint16_t)(channelCount * (bitsPerSample / 8u));
        if (blockSize != derivedBlockSize) {
            coduomp_mss_warn_wav_repair(path, "PCM block size", blockSize, derivedBlockSize);
            blockSize = derivedBlockSize;
        }
        if (sampleRate == 0) {
            if (averageBytesPerSecond == 0 || averageBytesPerSecond % blockSize != 0) {
                return coduomp_mss_reject_wav(path, "PCM sample rate is not derivable");
            }
            const uint32_t derivedSampleRate = averageBytesPerSecond / blockSize;
            if (derivedSampleRate == 0 || derivedSampleRate > INT32_MAX) {
                return coduomp_mss_reject_wav(path, "PCM sample rate is out of range");
            }
            coduomp_mss_warn_wav_repair(path, "PCM sample rate", sampleRate, derivedSampleRate);
            sampleRate = derivedSampleRate;
        }

        const uint32_t trailingBytes = dataLength % blockSize;
        if (trailingBytes != 0) {
            const uint32_t sourceDataLength = dataLength;
            dataLength -= trailingBytes;
            coduomp_mss_warn_wav_repair(path, "trailing incomplete PCM frame", sourceDataLength, dataLength);
        }
        sampleCount = dataLength * 8u / bitsPerSample;
    } else {
        if (bitsPerSample != MILES_SAMPLE_BITS_IMA_ADPCM)
            return coduomp_mss_reject_wav(path, "invalid IMA bit depth");

        const uint32_t imaHeaderSize = (uint32_t)channelCount * MSS_WAVE_IMA_HEADER_BYTES_PER_CHANNEL;
        if (blockSize < imaHeaderSize)
            return coduomp_mss_reject_wav(path, "invalid IMA block size");
        if (channelCount == MILES_CHANNEL_COUNT_STEREO && ((uint32_t)blockSize - imaHeaderSize) % MSS_WAVE_IMA_STEREO_GROUP_BYTES != 0) {
            return coduomp_mss_reject_wav(path, "incomplete channel group in each IMA block");
        }
        const uint32_t samplesPerFullBlock = 1u + ((uint32_t)blockSize - imaHeaderSize) * 2u / channelCount;
        if (sampleRate == 0) {
            const uint64_t sampleRateNumerator = (uint64_t)averageBytesPerSecond * samplesPerFullBlock;
            if (averageBytesPerSecond == 0 || sampleRateNumerator % blockSize != 0) {
                return coduomp_mss_reject_wav(path, "IMA sample rate is not derivable");
            }
            const uint64_t derivedSampleRateWide = sampleRateNumerator / blockSize;
            if (derivedSampleRateWide == 0 || derivedSampleRateWide > INT32_MAX) {
                return coduomp_mss_reject_wav(path, "IMA sample rate is out of range");
            }
            const uint32_t derivedSampleRate = (uint32_t)derivedSampleRateWide;
            coduomp_mss_warn_wav_repair(path, "IMA sample rate", sampleRate, derivedSampleRate);
            sampleRate = derivedSampleRate;
        }

        const uint32_t incompleteBlockSize = dataLength % blockSize;
        if (incompleteBlockSize != 0 && incompleteBlockSize < imaHeaderSize) {
            const uint32_t sourceDataLength = dataLength;
            dataLength -= incompleteBlockSize;
            coduomp_mss_warn_wav_repair(path, "trailing incomplete IMA header", sourceDataLength, dataLength);
        } else if (channelCount == MILES_CHANNEL_COUNT_STEREO && incompleteBlockSize > imaHeaderSize) {
            const uint32_t stereoPayloadSize = incompleteBlockSize - imaHeaderSize;
            const uint32_t incompleteGroupSize = stereoPayloadSize % MSS_WAVE_IMA_STEREO_GROUP_BYTES;
            if (incompleteGroupSize != 0) {
                const uint32_t sourceDataLength = dataLength;
                dataLength -= incompleteGroupSize;
                coduomp_mss_warn_wav_repair(path, "trailing incomplete stereo IMA group", sourceDataLength, dataLength);
            }
        }

        uint32_t blockOffset = 0;
        while (blockOffset < dataLength) {
            uint32_t currentBlockSize = dataLength - blockOffset;
            if (currentBlockSize > blockSize)
                currentBlockSize = blockSize;
            if (currentBlockSize < imaHeaderSize)
                return coduomp_mss_reject_wav(path, "truncated IMA header");
            for (uint32_t channel = 0; channel < channelCount; ++channel) {
                const uint32_t stepIndexOffset =
                    blockOffset + channel * MSS_WAVE_IMA_HEADER_BYTES_PER_CHANNEL + MSS_WAVE_IMA_STEP_INDEX_OFFSET;
                if (dataChunk.data[stepIndexOffset] > MSS_WAVE_IMA_MAXIMUM_STEP_INDEX) {
                    return coduomp_mss_reject_wav(path, "IMA step index is out of range");
                }
            }
            blockOffset += currentBlockSize;
        }

        const uint32_t fullBlockCount = dataLength / blockSize;
        const uint32_t partialBlockSize = dataLength % blockSize;
        uint64_t maximumSampleCount = (uint64_t)fullBlockCount * samplesPerFullBlock;
        if (partialBlockSize >= imaHeaderSize) {
            maximumSampleCount += 1u + (partialBlockSize - imaHeaderSize) * 2u / channelCount;
        }
        if (maximumSampleCount == 0 || maximumSampleCount > UINT32_MAX)
            return coduomp_mss_reject_wav(path, "IMA sample count is out of range");

        sampleCount = (uint32_t)maximumSampleCount;
        coduompMssWavChunk_t factChunk;
        if (coduomp_mss_find_loaded_wav_chunk(bytes, loadedLength, scanEnd, "fact", &factChunk) != qfalse) {
            if (factChunk.declaredSize < sizeof(uint32_t) || factChunk.availableSize < sizeof(uint32_t)) {
                Com_Printf("^3WARNING: WAV '%s' has a truncated optional fact chunk; ignoring it\n", path);
            } else {
                const uint32_t declaredSampleCount = coduomp_mss_read_wav_u32(factChunk.data);
                if (declaredSampleCount == 0) {
                    return coduomp_mss_reject_wav(path, "IMA fact sample count is zero");
                }
                if (declaredSampleCount <= maximumSampleCount) {
                    sampleCount = declaredSampleCount;
                } else {
                    coduomp_mss_warn_wav_repair(path, "IMA sample count", declaredSampleCount, sampleCount);
                }
            }
        }
    }

    if (sampleRate == 0 || sampleRate > INT32_MAX)
        return coduomp_mss_reject_wav(path, "sample rate is out of range");
    if (dataLength == 0 || sampleCount == 0)
        return coduomp_mss_reject_wav(path, "empty audio payload");

    memset(soundInfo, 0, sizeof(*soundInfo));
    snd_alias_sound_file_t *const publicInfo = &soundInfo->publicInfo;
    publicInfo->formatTag = formatTag;
    publicInfo->data = dataChunk.data;
    publicInfo->dataLength = dataLength;
    publicInfo->sampleRate = sampleRate;
    publicInfo->bitsPerSample = bitsPerSample;
    publicInfo->channelCount = channelCount;
    publicInfo->sampleCount = sampleCount;
    publicInfo->blockSize = blockSize;
    publicInfo->initialData = dataChunk.data;
    return qtrue;
}
#endif

#if defined(_WIN32)
/* NOT_FROM_ORIGINAL_SOURCE: Miles parses the original WAV buffer on Windows,
 * matching stock. Bound only the payload range returned to the engine before
 * its allocation and copy; do not reinterpret or repair Miles metadata. */
static qboolean coduomp_mss_bound_miles_wav_payload(const void *fileData, int32_t fileLength, const char *path,
                                                    snd_alias_sound_file_t *publicInfo)
{
    if (fileData == NULL || path == NULL || publicInfo == NULL || fileLength < 0 || publicInfo->data == NULL) {
        return qfalse;
    }

    const uintptr_t fileAddress = (uintptr_t)fileData;
    const uintptr_t dataAddress = (uintptr_t)publicInfo->data;
    const uint32_t loadedLength = (uint32_t)fileLength;
    if (dataAddress < fileAddress || dataAddress - fileAddress > loadedLength) {
        return qfalse;
    }

    const uint32_t dataOffset = (uint32_t)(dataAddress - fileAddress);
    const uint32_t availableDataLength = loadedLength - dataOffset;
    if (publicInfo->dataLength > availableDataLength) {
        const uint32_t declaredDataLength = publicInfo->dataLength;
        publicInfo->dataLength = availableDataLength;
        coduomp_mss_warn_wav_repair(path, "data length", declaredDataLength, publicInfo->dataLength);
    }
    return qtrue;
}
#endif

/* NOT_FROM_ORIGINAL_SOURCE: validate the loaded audio data and playback state before crossing the Miles boundary. */
static uint32_t coduomp_mss_read_eal_u32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

/* NOT_FROM_ORIGINAL_SOURCE: validate the loaded audio data and playback state before crossing the Miles boundary. */
static qboolean coduomp_mss_validate_eal_list(const uint8_t *listData, uint32_t listSize)
{
    if (listData == NULL || listSize < MSS_EAL_LIST_TYPE_BYTES)
        return qfalse;

    const char *nameChunkId;
    const char *firstDataChunkId;
    const char *secondDataChunkId = NULL;
    uint32_t firstRecordSize;
    uint32_t secondRecordSize = 0;
    qboolean geometryList = qfalse;

    if (memcmp(listData, "envp", MSS_RIFF_FOURCC_BYTES) == 0) {
        nameChunkId = "nams";
        firstDataChunkId = "lisp";
        firstRecordSize = sizeof(eax_listener_properties_t);
    } else if (memcmp(listData, "srcp", MSS_RIFF_FOURCC_BYTES) == 0) {
        nameChunkId = "nams";
        firstDataChunkId = "fils";
        firstRecordSize = MSS_EAL_PATH_BYTES;
        secondDataChunkId = "srca";
        secondRecordSize = EAX_SOURCE_PROPERTIES_SIZE;
    } else if (memcmp(listData, "matp", MSS_RIFF_FOURCC_BYTES) == 0) {
        nameChunkId = "nams";
        firstDataChunkId = "mata";
        firstRecordSize = EAX_MATERIAL_PROPERTIES_SIZE;
    } else if (memcmp(listData, "gemp", MSS_RIFF_FOURCC_BYTES) == 0) {
        nameChunkId = "nams";
        firstDataChunkId = "fils";
        firstRecordSize = MSS_EAL_PATH_BYTES;
        secondDataChunkId = "gema";
        geometryList = qtrue;
    } else {
        return qtrue;
    }

    const uint8_t *countData = NULL;
    qboolean foundNames = qfalse;
    qboolean foundFirstData = qfalse;
    qboolean foundSecondData = secondDataChunkId == NULL;
    uint32_t nameSize = 0;
    uint32_t firstDataSize = 0;
    uint32_t secondDataSize = 0;

    uint32_t offset = MSS_EAL_LIST_TYPE_BYTES;
    while (offset < listSize) {
        if (listSize - offset < MSS_RIFF_CHUNK_HEADER_BYTES)
            return qfalse;

        const uint32_t chunkSize = coduomp_mss_read_eal_u32(listData + offset + MSS_RIFF_CHUNK_SIZE_OFFSET);
        const uint32_t dataOffset = offset + MSS_RIFF_CHUNK_HEADER_BYTES;
        /* EAXMan advances by the raw size and does not consume RIFF's pad
         * byte, so an odd-sized chunk desynchronizes its next header read. */
        if ((chunkSize & 1u) != 0 || chunkSize > listSize - dataOffset)
            return qfalse;

        if (memcmp(listData + offset, "num ", MSS_RIFF_FOURCC_BYTES) == 0) {
            if (countData != NULL || chunkSize != sizeof(uint32_t))
                return qfalse;
            countData = listData + dataOffset;
        } else if (memcmp(listData + offset, nameChunkId, MSS_RIFF_FOURCC_BYTES) == 0) {
            if (foundNames)
                return qfalse;
            foundNames = qtrue;
            nameSize = chunkSize;
        } else if (memcmp(listData + offset, firstDataChunkId, MSS_RIFF_FOURCC_BYTES) == 0) {
            if (foundFirstData)
                return qfalse;
            foundFirstData = qtrue;
            firstDataSize = chunkSize;
        } else if (secondDataChunkId != NULL && memcmp(listData + offset, secondDataChunkId, MSS_RIFF_FOURCC_BYTES) == 0) {
            if (foundSecondData)
                return qfalse;
            foundSecondData = qtrue;
            secondDataSize = chunkSize;
        }

        const uint32_t paddedSize = chunkSize + (chunkSize & 1u);
        if (paddedSize > listSize - dataOffset)
            return qfalse;
        offset = dataOffset + paddedSize;
    }

    if (countData == NULL || !foundNames || !foundFirstData || !foundSecondData) {
        return qfalse;
    }

    const uint32_t recordCount = coduomp_mss_read_eal_u32(countData);
    if (recordCount > INT32_MAX || nameSize % MSS_EAL_NAME_BYTES != 0 || nameSize / MSS_EAL_NAME_BYTES != recordCount ||
        firstDataSize % firstRecordSize != 0 || firstDataSize / firstRecordSize != recordCount) {
        return qfalse;
    }

    if (geometryList) {
        return recordCount <= secondDataSize / MSS_EAL_GEOMETRY_MINIMUM_BYTES ? qtrue : qfalse;
    }
    if (secondDataChunkId != NULL && (secondDataSize % secondRecordSize != 0 || secondDataSize / secondRecordSize != recordCount)) {
        return qfalse;
    }
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: validate the loaded audio data and playback state before crossing the Miles boundary. */
static qboolean coduomp_mss_validate_eal_extent(const void *fileData, int32_t fileLength)
{
    if (coduomp_mss_validate_riff_extent(fileData, fileLength, "eal ", NULL, NULL, 0, qtrue) == qfalse) {
        return qfalse;
    }

    const uint8_t *const bytes = fileData;
    const uint32_t riffEnd = coduomp_mss_read_eal_u32(bytes + MSS_RIFF_SIZE_OFFSET) + MSS_RIFF_CHUNK_HEADER_BYTES;
    qboolean foundMajorVersion = qfalse;
    qboolean foundMinorVersion = qfalse;
    qboolean foundListenerAxis = qfalse;
    qboolean foundDefaultGeometry = qfalse;
    qboolean foundDefaultEnvironment = qfalse;
    qboolean foundDefaultSource = qfalse;
    qboolean foundDefaultMaterial = qfalse;
    qboolean foundEnvironmentPool = qfalse;
    qboolean foundSourcePool = qfalse;
    qboolean foundMaterialPool = qfalse;
    qboolean foundGeometryPool = qfalse;

    for (uint32_t offset = MSS_RIFF_FILE_HEADER_BYTES; offset < riffEnd;) {
        const uint32_t chunkSize = coduomp_mss_read_eal_u32(bytes + offset + MSS_RIFF_CHUNK_SIZE_OFFSET);
        const uint32_t dataOffset = offset + MSS_RIFF_CHUNK_HEADER_BYTES;
        qboolean *foundFixedChunk = NULL;
        uint32_t expectedSize = 0;

        /* The bundled top-level search also ignores RIFF pad bytes. */
        if ((chunkSize & 1u) != 0)
            return qfalse;

        if (memcmp(bytes + offset, "majv", MSS_RIFF_FOURCC_BYTES) == 0) {
            foundFixedChunk = &foundMajorVersion;
            expectedSize = MSS_EAL_VERSION_BYTES;
        } else if (memcmp(bytes + offset, "minv", MSS_RIFF_FOURCC_BYTES) == 0) {
            foundFixedChunk = &foundMinorVersion;
            expectedSize = MSS_EAL_VERSION_BYTES;
        } else if (memcmp(bytes + offset, "lisa", MSS_RIFF_FOURCC_BYTES) == 0) {
            foundFixedChunk = &foundListenerAxis;
            expectedSize = MSS_EAL_VECTOR_BYTES;
        } else if (memcmp(bytes + offset, "gdfm", MSS_RIFF_FOURCC_BYTES) == 0) {
            foundFixedChunk = &foundDefaultGeometry;
            expectedSize = MSS_EAL_VECTOR_BYTES;
        } else if (memcmp(bytes + offset, "denv", MSS_RIFF_FOURCC_BYTES) == 0) {
            foundFixedChunk = &foundDefaultEnvironment;
            expectedSize = sizeof(eax_listener_properties_t);
        } else if (memcmp(bytes + offset, "dsrc", MSS_RIFF_FOURCC_BYTES) == 0) {
            foundFixedChunk = &foundDefaultSource;
            expectedSize = EAX_SOURCE_PROPERTIES_SIZE;
        } else if (memcmp(bytes + offset, "dmat", MSS_RIFF_FOURCC_BYTES) == 0) {
            foundFixedChunk = &foundDefaultMaterial;
            expectedSize = EAX_MATERIAL_PROPERTIES_SIZE;
        }

        if (foundFixedChunk != NULL) {
            if (*foundFixedChunk || chunkSize != expectedSize)
                return qfalse;
            *foundFixedChunk = qtrue;
        } else if (memcmp(bytes + offset, "LIST", MSS_RIFF_FOURCC_BYTES) == 0) {
            if (chunkSize < MSS_EAL_LIST_TYPE_BYTES)
                return qfalse;
            const uint8_t *const listData = bytes + dataOffset;
            qboolean *foundList = NULL;
            if (memcmp(listData, "envp", MSS_RIFF_FOURCC_BYTES) == 0)
                foundList = &foundEnvironmentPool;
            else if (memcmp(listData, "srcp", MSS_RIFF_FOURCC_BYTES) == 0)
                foundList = &foundSourcePool;
            else if (memcmp(listData, "matp", MSS_RIFF_FOURCC_BYTES) == 0)
                foundList = &foundMaterialPool;
            else if (memcmp(listData, "gemp", MSS_RIFF_FOURCC_BYTES) == 0)
                foundList = &foundGeometryPool;

            if (foundList != NULL) {
                if (*foundList || coduomp_mss_validate_eal_list(listData, chunkSize) == qfalse) {
                    return qfalse;
                }
                *foundList = qtrue;
            }
        }

        offset = dataOffset + chunkSize + (chunkSize & 1u);
    }

    return foundMajorVersion && foundMinorVersion && foundListenerAxis && foundDefaultGeometry && foundDefaultEnvironment &&
                   foundDefaultSource && foundDefaultMaterial && foundEnvironmentPool && foundSourcePool && foundMaterialPool &&
                   foundGeometryPool
               ? qtrue
               : qfalse;
}

/* Source: CoDUOMP.exe 0x004508e0..0x004508ef.
 * Name: exact same-module Mac symbol MSS_Alloc. Miles rounds requests to four
 * bytes before taking 32-byte-aligned permanent hunk storage. */
void *MSS_Alloc(size_t size)
{
    const size_t alignedSize = (size + MILES_ALLOCATION_SIZE_ALIGNMENT - 1u) & ~(size_t)(MILES_ALLOCATION_SIZE_ALIGNMENT - 1u);
    return Hunk_AllocAlignInternal(alignedSize, MILES_HUNK_ALIGNMENT);
}

/* Source: CoDUOMP.exe 0x004508f0. Name: exact same-module Mac symbol MSS_Free.
 * Miles allocations come from the permanent hunk, so individual frees are
 * intentionally no-ops. */
void MSS_Free(void *memory)
{
    (void)memory;
}

/* Source: CoDUOMP.exe 0x00455190..0x004551b7.
 * Name and source structure: exact same-module Mac symbol MSS_InitFailed.
 * The renderer's r_vc_compile mode 2 builds visibility data and quits; sound
 * initialization is dispensable in that tool-like run, so the original
 * deliberately suppresses the ordinary failure message there. */
void MSS_InitFailed(void)
{
    cvar_t *const lightVisCompile = Cvar_Get("r_vc_compile", "0", CVAR_NONE);
    if (lightVisCompile->integer != MSS_LIGHT_VIS_COMPILE_AND_QUIT_MODE)
        Com_Printf("Miles sound system initialization failed\n");
}

/* NOT_FROM_ORIGINAL_SOURCE: portable source spelling of the static-linkage
 * no-argument thunk at 0x005399c0, registered by 0x005399d0. */
static void coduomp_mss_ail_shutdown_at_exit(void)
{
    AIL_shutdown();
}

/* NOT_FROM_ORIGINAL_SOURCE: typed factoring of the original memset over
 * CoDUOMP.exe 0x009cbeb8..0x009cd328. Explicit objects replace that raw
 * contiguous i386 range so native pointers can widen safely. */
static void coduomp_mss_clear_runtime_state(void)
{
    mss_digitalDriver = NULL;
    mss_3dProvider = 0;
    mss_sampleRate = 0;
    mss_sampleBits = 0;
    mss_channelCount = 0;
    mss_playbackRateScale = 0.0f;
    mss_paused = qfalse;
    mss_pauseStartTime = 0;
    mss_cpuPercent = 0;
    mss_restoreBuffer = NULL;
    mss_restoreSize = 0;
    mss_effectVolume = 0.0f;
    memset(&mss_masterVolume, 0, sizeof(mss_masterVolume));
    memset(mss_channelVolumes, 0, sizeof(mss_channelVolumes));
    memset(mss_backgroundFades, 0, sizeof(mss_backgroundFades));
    mss_ambientBackgroundIndex = 0;
    mss_roomType = 0;
    mss_reverbLevel = 0.0f;
    mss_reverbTarget = 0.0f;
    mss_reverbRate = 0.0f;
    memset(mss_listenerOrigin, 0, sizeof(mss_listenerOrigin));
    memset(mss_listenerAxis, 0, sizeof(mss_listenerAxis));
    mss_listenerTime = 0;
    mss_soundTime = 0;
    mss_lastSoundTime = 0;
    mss_anyMasters = qfalse;
    memset(mss_channelInfo, 0, sizeof(mss_channelInfo));
    memset(mss_2dSampleHandles, 0, sizeof(mss_2dSampleHandles));
    memset(mss_3dSampleHandles, 0, sizeof(mss_3dSampleHandles));
    memset(mss_streamHandles, 0, sizeof(mss_streamHandles));
    memset(mss_streamChannels, 0, sizeof(mss_streamChannels));
    mss_2dChannelCount = 0;
    mss_max3DChannels = 0;
    mss_streamChannelCount = 0;
    memset(&mss_rawSampleState, 0, sizeof(mss_rawSampleState));
}

/* Source: CoDUOMP.exe 0x004551c0..0x00455594.
 * Name and source-level loop structure: exact same-module Mac symbol
 * MSS_Init. Windows additionally selects the "miles" redistribution folder
 * and registers the statically linked AIL_shutdown atexit thunk. */
void MSS_Init(void)
{
    Com_Printf("\n------- Miles sound system initialization -------\n");

    mss_q3fs = Cvar_Get("mss_q3fs", "1", CVAR_LATCH);
    if (mss_q3fs->integer != 0) {
        AIL_set_file_callbacks(MSS_FileOpenCallback, MSS_FileCloseCallback, MSS_FileSeekCallback, MSS_FileReadCallback);
    }
    AIL_set_redist_directory("miles");
    (void)atexit(coduomp_mss_ail_shutdown_at_exit);
    if (AIL_startup() == MILES_STARTUP_FAILED) {
        MSS_InitFailed();
        return;
    }

    mss_errorOnMissing = Cvar_Get("mss_errorOnMissing", "1", CVAR_ARCHIVE);
    mss_khz = Cvar_Get("mss_khz", "44", CVAR_ARCHIVE | CVAR_LATCH);
    mss_bits = Cvar_Get("mss_bits", "16", CVAR_ARCHIVE | CVAR_LATCH);
    mss_stereo = Cvar_Get("mss_stereo", "1", CVAR_ARCHIVE | CVAR_LATCH);
    /* NOT_FROM_ORIGINAL_SOURCE: native builds archive their platform adapter
     * name; the macro remains the retail provider name on Win32. */
    mss_3d_provider = Cvar_Get("mss_3d_provider", CODUOMP_3D_PROVIDER_NAME, CVAR_ARCHIVE | CVAR_LATCH);
    mss_volume = Cvar_Get("mss_volume", "0.8", CVAR_ARCHIVE);
    mss_roomtype = Cvar_Get("mss_roomtype", "0", CVAR_CHEAT);
    mss_wetlevel = Cvar_Get("mss_wetlevel", "0", CVAR_CHEAT);

    if (!MSS_Init2D() || !MSS_Init3DProvider(mss_3d_provider->string, qtrue)) {
        AIL_shutdown();
        coduomp_mss_clear_runtime_state();
        MSS_InitFailed();
        return;
    }

    MSS_InitChannels();
    mss_masterVolume.current = MSS_FULL_VOLUME;
    mss_masterVolume.target = MSS_FULL_VOLUME;
    mss_masterVolume.ratePerMsec = MSS_SILENT_VOLUME;
    for (int32_t channel = 0; channel < SND_ALIAS_CHANNEL_COUNT; ++channel) {
        mss_channelVolumes[channel].current = MSS_FULL_VOLUME;
        mss_channelVolumes[channel].target = MSS_FULL_VOLUME;
        mss_channelVolumes[channel].ratePerMsec = MSS_SILENT_VOLUME;
    }

    mss_soundTime = (int32_t)Sys_Milliseconds();
    mss_lastSoundTime = mss_soundTime;
    mss_anyMasters = qfalse;
    mss_effectVolume = Com_ClampFloat(MSS_SILENT_VOLUME, MSS_FULL_VOLUME, mss_volume->value);
    Com_Printf("------- Miles successfully initialized -------\n");
}

/* Source: CoDUOMP.exe 0x004555a0..0x004556bf.
 * Name and source-level call structure: exact same-module Mac symbol
 * MSS_Shutdown. The Win32 compiler inlines the two Com_UnloadSoundAliases
 * calls around their shared MSS alias-release helper. Retail does not clear
 * the restore buffer before its driver-null return; the annotated fix below
 * changes only that freed-pointer publication. */
void MSS_Shutdown(void)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (mss_restoreBuffer != NULL) {
        Z_FreeInternal(mss_restoreBuffer);
        mss_restoreBuffer = NULL;
    }

    if (mss_digitalDriver == NULL)
        return;

    MSS_StopSounds(MSS_STOP_ALL_SOUNDS);
    Com_UnloadSoundAliases(SND_ALIAS_BANK_CGAME);
    Com_UnloadSoundAliases(SND_ALIAS_BANK_COMMON);

    AIL_close_3D_provider(mss_3dProvider);
    mss_3dProvider = 0;
    if (mss_eaxAvailable) {
        ReleaseEAXManager();
        mss_eaxAvailable = qfalse;
    }

    AIL_shutdown();
    coduomp_mss_clear_runtime_state();
}

/* Source: CoDUOMP.exe 0x004556c0..0x004556dd, recovered from the executable
 * gap following MSS_Shutdown. Name and source structure: exact same-module
 * Mac symbol MSS_ErrorCleanup. This path deliberately leaves restoreSize
 * untouched while making repeated cleanup safe. */
void MSS_ErrorCleanup(void)
{
    if (mss_restoreBuffer != NULL) {
        Z_FreeInternal(mss_restoreBuffer);
        mss_restoreBuffer = NULL;
    }
}

/* Source: CoDUOMP.exe 0x004556e0..0x004556f2. Windows-only role name: this
 * WM_CREATE path passes the native game-window handle to Miles after the
 * digital driver exists. The Mac build has no corresponding function. */
void MSS_SetWindowHandle(miles_window_handle_t windowHandle)
{
    if (mss_digitalDriver != NULL)
        AIL_set_DirectSound_HWND(mss_digitalDriver, windowHandle);
}

/* Source: CoDUOMP.exe 0x00455700..0x0045573e.
 * Name and source structure: exact same-module Mac symbol MSS_Write. */
int32_t MSS_Write(uint8_t *buffer, int32_t offset, int32_t bufferSize, const void *source, size_t byteCount)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (offset < 0 || bufferSize < 0 || byteCount > (size_t)bufferSize || (size_t)offset > (size_t)bufferSize - byteCount) {
        Com_Error(ERR_DROP,
                  "\x15"
                  "buffer overflow while saving sound state: "
                  "more than %i bytes used\n",
                  bufferSize);
        return offset;
    }
    const int32_t nextOffset = offset + (int32_t)byteCount;
    memcpy(buffer + offset, source, byteCount);
    return nextOffset;
}

/* Source: CoDUOMP.exe 0x00455740..0x0045577e.
 * Name and source structure: exact same-module Mac symbol MSS_Read. */
int32_t MSS_Read(const uint8_t *buffer, int32_t offset, int32_t bufferSize, void *destination, size_t byteCount)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (offset < 0 || bufferSize < 0 || byteCount > (size_t)bufferSize || (size_t)offset > (size_t)bufferSize - byteCount) {
        Com_Error(ERR_DROP, "\x15"
                            "buffer overflow while restoring sound state: "
                            "probably due to an old or corrupt savegame\n");
        return offset;
    }
    const int32_t nextOffset = offset + (int32_t)byteCount;
    memcpy(destination, buffer + offset, byteCount);
    return nextOffset;
}

enum {
    MSS_SAVED_EFFECT_ID_BYTE_COUNT = sizeof(int16_t),
    MSS_SAVED_ALIAS_CHANNEL_BYTE_COUNT = sizeof(uint8_t),
    MSS_SAVED_CHANNEL_INFO_BYTE_COUNT = MSS_SAVED_EFFECT_ID_BYTE_COUNT + MSS_SAVED_ALIAS_CHANNEL_BYTE_COUNT + sizeof(int32_t) +
                                        sizeof(float) + sizeof(float) + sizeof(vec3_t),
    MSS_SAVE_FIXED_STATE_BYTE_COUNT =
        sizeof(int32_t) + sizeof(mss_channelVolumes) + sizeof(mss_environment_save_t) + 3 * sizeof(mss_backgroundFades[0]),
    MSS_SAVE_MAX_BYTE_COUNT =
        MSS_SAVE_FIXED_STATE_BYTE_COUNT +
        MSS_3D_CHANNEL_CAPACITY * (2 * sizeof(uint16_t) + MSS_SAVED_CHANNEL_INFO_BYTE_COUNT + sizeof(mss_3d_channel_save_t)) +
        MSS_2D_CHANNEL_CAPACITY * (2 * sizeof(uint16_t) + MSS_SAVED_CHANNEL_INFO_BYTE_COUNT + sizeof(mss_2d_channel_save_t)) +
        MSS_STREAM_CHANNEL_CAPACITY * (2 * sizeof(uint16_t) + MSS_SAVED_CHANNEL_INFO_BYTE_COUNT + sizeof(mss_stream_channel_save_t)) +
        3 * sizeof(uint16_t)
};

/* Source: CoDUOMP.exe 0x00455780..0x00455875.
 * Name and field sequence: exact same-module Mac symbol MSS_SaveChanInfo.
 * Only persistent channel identity, timing, gain, blend, and spatial offset
 * are serialized; live Miles handles and alias pointers are reconstructed by
 * the channel-type-specific restore routines. Both retail architectures copy
 * the two- and one-byte address prefixes of the full-width runtime identity
 * fields directly, so the original save representation is endian-dependent. */
int32_t MSS_SaveChanInfo(uint8_t *buffer, int32_t offset, int32_t bufferSize, const mss_channel_info_t *channelInfo)
{
    offset = MSS_Write(buffer, offset, bufferSize, &channelInfo->effectId, MSS_SAVED_EFFECT_ID_BYTE_COUNT);
    offset = MSS_Write(buffer, offset, bufferSize, &channelInfo->aliasChannel, MSS_SAVED_ALIAS_CHANNEL_BYTE_COUNT);
    offset = MSS_Write(buffer, offset, bufferSize, &channelInfo->endTime, sizeof(channelInfo->endTime));
    offset = MSS_Write(buffer, offset, bufferSize, &channelInfo->logicalVolume, sizeof(channelInfo->logicalVolume));
    offset = MSS_Write(buffer, offset, bufferSize, &channelInfo->aliasBlend, sizeof(channelInfo->aliasBlend));
    return MSS_Write(buffer, offset, bufferSize, channelInfo->effectOffset, sizeof(channelInfo->effectOffset));
}

/* Source: CoDUOMP.exe 0x00455880..0x0045597d.
 * Name and field sequence: exact same-module Mac symbol
 * MSS_RestoreChanInfo. The original clears the complete runtime record before
 * restoring the portable subset serialized by MSS_SaveChanInfo. */
int32_t MSS_RestoreChanInfo(const uint8_t *buffer, int32_t offset, int32_t bufferSize, mss_channel_info_t *channelInfo)
{
    memset(channelInfo, 0, sizeof(*channelInfo));
    offset = MSS_Read(buffer, offset, bufferSize, &channelInfo->effectId, MSS_SAVED_EFFECT_ID_BYTE_COUNT);
    offset = MSS_Read(buffer, offset, bufferSize, &channelInfo->aliasChannel, MSS_SAVED_ALIAS_CHANNEL_BYTE_COUNT);
    offset = MSS_Read(buffer, offset, bufferSize, &channelInfo->endTime, sizeof(channelInfo->endTime));
    offset = MSS_Read(buffer, offset, bufferSize, &channelInfo->logicalVolume, sizeof(channelInfo->logicalVolume));
    offset = MSS_Read(buffer, offset, bufferSize, &channelInfo->aliasBlend, sizeof(channelInfo->aliasBlend));
    return MSS_Read(buffer, offset, bufferSize, channelInfo->effectOffset, sizeof(channelInfo->effectOffset));
}

/* Source: CoDUOMP.exe 0x00455980..0x00455b50.
 * Name and source-level call structure: exact same-module Mac symbol
 * MSS_Save3DChannel. Alias-table indices use the original 16-bit save
 * representation; zero means that the channel cannot be restored. */
int32_t MSS_Save3DChannel(uint8_t *buffer, int32_t offset, int32_t bufferSize, int32_t channelIndex)
{
    if (MSS_Is3DChannelFree(channelIndex))
        return offset;

    mss_channel_info_t *const channel = &mss_channelInfo[channelIndex];
    miles_3d_sample_handle_t const sample = mss_3dSampleHandles[channelIndex];
    const uint16_t primaryAliasIndex = (uint16_t)Com_SoundAliasIndex(channel->alias, SND_ALIAS_BANK_CGAME);
    if (primaryAliasIndex == 0)
        return offset;
    const uint16_t secondaryAliasIndex = (uint16_t)Com_SoundAliasIndex(channel->secondaryAlias, SND_ALIAS_BANK_CGAME);
    if (secondaryAliasIndex == 0)
        return offset;

    const uint32_t sampleOffset = AIL_3D_sample_offset(sample);
    const uint32_t sampleLength = AIL_3D_sample_length(sample);
    const int32_t playbackRate = AIL_3D_sample_playback_rate(sample);
    if (sampleLength == 0 || playbackRate == 0)
        return offset;

    mss_3d_channel_save_t savedState;
    savedState.startFraction = (float)((long double)sampleOffset / (long double)sampleLength);
    savedState.aliasPitchScale = channel->aliasPitchScale;
    const float sampleVolume = AIL_3D_sample_volume(sample);
    if (mss_effectVolume == MSS_FULL_VOLUME) {
        savedState.logicalVolume = channel->logicalVolume;
    } else {
        savedState.logicalVolume = sampleVolume / mss_effectVolume;
    }
    AIL_3D_position(sample, &savedState.position[0], &savedState.position[1], &savedState.position[2]);

    offset = MSS_Write(buffer, offset, bufferSize, &primaryAliasIndex, sizeof(primaryAliasIndex));
    offset = MSS_Write(buffer, offset, bufferSize, &secondaryAliasIndex, sizeof(secondaryAliasIndex));
    offset = MSS_SaveChanInfo(buffer, offset, bufferSize, channel);
    return MSS_Write(buffer, offset, bufferSize, &savedState, sizeof(savedState));
}

/* Source: CoDUOMP.exe 0x00455b50..0x00455d46.
 * Name and source-level call structure: exact same-module Mac symbol
 * MSS_Restore3DChannel. The caller has already consumed the primary alias
 * index; this routine begins by reading the secondary index. Invalid or
 * obsolete alias pairs consume their complete record but do not restart a
 * channel. */
int32_t MSS_Restore3DChannel(const uint8_t *buffer, int32_t offset, int32_t bufferSize, int32_t primaryAliasIndex)
{
    uint16_t secondaryAliasIndex = 0;
    mss_channel_info_t savedChannel;
    mss_3d_channel_save_t savedState;

    offset = MSS_Read(buffer, offset, bufferSize, &secondaryAliasIndex, sizeof(secondaryAliasIndex));
    offset = MSS_RestoreChanInfo(buffer, offset, bufferSize, &savedChannel);
    offset = MSS_Read(buffer, offset, bufferSize, &savedState, sizeof(savedState));

    snd_alias_t *const primaryAlias = Com_GetSoundAlias(primaryAliasIndex, SND_ALIAS_BANK_CGAME);
    snd_alias_t *const secondaryAlias = Com_GetSoundAlias(secondaryAliasIndex, SND_ALIAS_BANK_CGAME);
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if ((uint32_t)savedChannel.effectId >= (uint32_t)MSS_EFFECT_ID_LIMIT || primaryAlias == NULL || secondaryAlias == NULL ||
        primaryAlias->type != SND_ALIAS_TYPE_LOADED || secondaryAlias->type != SND_ALIAS_TYPE_LOADED ||
        primaryAlias->soundFileInfo == NULL || secondaryAlias->soundFileInfo != primaryAlias->soundFileInfo ||
        !MSS_ValidateSoundAliasBlend(primaryAlias, secondaryAlias, qfalse)) {
        return offset;
    }

    int32_t channelIndex = -1;
    (void)MSS_StartAlias3DSample(&channelIndex, primaryAlias, savedState.position, secondaryAlias, savedChannel.aliasBlend,
                                 savedChannel.effectId, savedState.logicalVolume, savedState.aliasPitchScale, 0, savedState.startFraction);
    if (channelIndex >= 0) {
        memcpy(mss_channelInfo[channelIndex].effectOffset, savedChannel.effectOffset, sizeof(savedChannel.effectOffset));
    }
    return offset;
}

/* Source: CoDUOMP.exe 0x00455d50..0x00455f17.
 * Name and source-level call structure: exact same-module Mac symbol
 * MSS_Save2DChannel. The saved volume is normalized against the global
 * effect volume so restore can reapply the actual Miles gain independently
 * of later channel-volume changes. */
int32_t MSS_Save2DChannel(uint8_t *buffer, int32_t offset, int32_t bufferSize, int32_t channelIndex)
{
    if (MSS_Is2DChannelFree(channelIndex))
        return offset;

    mss_channel_info_t *const channel = &mss_channelInfo[channelIndex];
    miles_sample_handle_t const sample = mss_2dSampleHandles[channelIndex - MSS_2D_CHANNEL_FIRST];
    const uint16_t primaryAliasIndex = (uint16_t)Com_SoundAliasIndex(channel->alias, SND_ALIAS_BANK_CGAME);
    if (primaryAliasIndex == 0)
        return offset;
    const uint16_t secondaryAliasIndex = (uint16_t)Com_SoundAliasIndex(channel->secondaryAlias, SND_ALIAS_BANK_CGAME);
    if (secondaryAliasIndex == 0)
        return offset;

    int32_t totalMsec;
    int32_t currentMsec;
    AIL_sample_ms_position(sample, &totalMsec, &currentMsec);
    const int32_t playbackRate = AIL_sample_playback_rate(sample);
    if (totalMsec == 0 || playbackRate == 0)
        return offset;

    mss_2d_channel_save_t savedState;
    savedState.startFraction = (float)((long double)(uint32_t)currentMsec / (long double)(uint32_t)totalMsec);
    savedState.aliasPitchScale = channel->aliasPitchScale;
    AIL_sample_volume_pan(sample, &savedState.logicalVolume, &savedState.pan);
    if (mss_effectVolume == MSS_FULL_VOLUME) {
        savedState.logicalVolume = channel->logicalVolume;
    } else {
        savedState.logicalVolume /= mss_effectVolume;
    }

    offset = MSS_Write(buffer, offset, bufferSize, &primaryAliasIndex, sizeof(primaryAliasIndex));
    offset = MSS_Write(buffer, offset, bufferSize, &secondaryAliasIndex, sizeof(secondaryAliasIndex));
    offset = MSS_SaveChanInfo(buffer, offset, bufferSize, channel);
    return MSS_Write(buffer, offset, bufferSize, &savedState, sizeof(savedState));
}

/* Source: CoDUOMP.exe 0x00455f20..0x0045610a.
 * Name and source-level call structure: exact same-module Mac symbol
 * MSS_Restore2DChannel. The primary alias index was consumed by the caller.
 * The original consumes but does not use the saved secondary index, then
 * resolves the primary index for both blend endpoints; that asymmetry with
 * the 3D and stream restorers is preserved here. */
int32_t MSS_Restore2DChannel(const uint8_t *buffer, int32_t offset, int32_t bufferSize, int32_t primaryAliasIndex)
{
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    uint16_t discardedSecondaryAliasIndex = 0;
    mss_channel_info_t savedChannel;
    mss_2d_channel_save_t savedState;

    offset = MSS_Read(buffer, offset, bufferSize, &discardedSecondaryAliasIndex, sizeof(discardedSecondaryAliasIndex));
    offset = MSS_RestoreChanInfo(buffer, offset, bufferSize, &savedChannel);
    offset = MSS_Read(buffer, offset, bufferSize, &savedState, sizeof(savedState));

    snd_alias_t *const primaryAlias = Com_GetSoundAlias(primaryAliasIndex, SND_ALIAS_BANK_CGAME);
    snd_alias_t *const secondaryAlias = Com_GetSoundAlias(primaryAliasIndex, SND_ALIAS_BANK_CGAME);
    if (primaryAlias == NULL || secondaryAlias == NULL || primaryAlias->type != SND_ALIAS_TYPE_LOADED ||
        secondaryAlias->type != SND_ALIAS_TYPE_LOADED || primaryAlias->soundFileInfo == NULL ||
        secondaryAlias->soundFileInfo != primaryAlias->soundFileInfo ||
        !MSS_ValidateSoundAliasBlend(primaryAlias, secondaryAlias, qfalse)) {
        return offset;
    }

    int32_t channelIndex = -1;
    (void)MSS_StartAlias2DSample(&channelIndex, primaryAlias, secondaryAlias, savedChannel.aliasBlend, savedChannel.effectId,
                                 savedState.logicalVolume, savedState.aliasPitchScale, 0, savedState.startFraction);
    if (channelIndex >= 0) {
        miles_sample_handle_t const sample = mss_2dSampleHandles[channelIndex - MSS_2D_CHANNEL_FIRST];
        AIL_set_sample_volume_pan(sample, mss_effectVolume * savedState.logicalVolume, savedState.pan);
        memcpy(mss_channelInfo[channelIndex].effectOffset, savedChannel.effectOffset, sizeof(savedChannel.effectOffset));
    }
    return offset;
}

/* Source: CoDUOMP.exe 0x00456110..0x0045632c.
 * Name and source-level call structure: exact same-module Mac symbol
 * MSS_SaveStreamChannel. The two ambient background slots are intentionally
 * omitted. Other reserved background slots write a zero alias marker when
 * empty, while empty replaceable alias-stream slots contribute no record. */
int32_t MSS_SaveStreamChannel(uint8_t *buffer, int32_t offset, int32_t bufferSize, int32_t channelIndex)
{
    if (channelIndex < MSS_ALIAS_STREAM_CHANNEL_FIRST) {
        if (channelIndex == MSS_STREAM_CHANNEL_FIRST + MSS_AMBIENT_BACKGROUND_FIRST_INDEX ||
            channelIndex == MSS_STREAM_CHANNEL_FIRST + MSS_AMBIENT_BACKGROUND_SECOND_INDEX) {
            return offset;
        }
        if (MSS_IsStreamChannelFree(channelIndex)) {
            const uint16_t emptyAliasIndex = 0;
            return MSS_Write(buffer, offset, bufferSize, &emptyAliasIndex, sizeof(emptyAliasIndex));
        }
    } else if (MSS_IsStreamChannelFree(channelIndex)) {
        return offset;
    }

    const int32_t streamIndex = channelIndex - MSS_STREAM_CHANNEL_FIRST;
    miles_stream_handle_t const stream = mss_streamHandles[streamIndex];
    mss_channel_info_t *const channel = &mss_channelInfo[channelIndex];
    const uint16_t primaryAliasIndex = (uint16_t)Com_SoundAliasIndex(channel->alias, SND_ALIAS_BANK_CGAME);
    if (primaryAliasIndex == 0)
        return offset;
    const uint16_t secondaryAliasIndex = (uint16_t)Com_SoundAliasIndex(channel->secondaryAlias, SND_ALIAS_BANK_CGAME);
    if (secondaryAliasIndex == 0)
        return offset;

    int32_t totalMsec;
    int32_t currentMsec;
    AIL_stream_ms_position(stream, &totalMsec, &currentMsec);
    if (totalMsec == 0)
        return offset;

    mss_stream_channel_save_t savedState;
    savedState.startFraction = (float)((long double)(uint32_t)currentMsec / (long double)(uint32_t)totalMsec);
    const int32_t playbackRate = AIL_stream_playback_rate(stream);
    savedState.basePlaybackRate = FastRound((float)((long double)playbackRate / (long double)mss_playbackRateScale));
    savedState.logicalVolume = channel->logicalVolume;
    AIL_stream_volume_pan(stream, &savedState.relativeVolume, &savedState.pan);
    if (mss_effectVolume == MSS_FULL_VOLUME) {
        savedState.relativeVolume = channel->logicalVolume;
    } else {
        savedState.relativeVolume /= mss_effectVolume;
    }
    memcpy(savedState.position, mss_streamChannels[streamIndex].position, sizeof(savedState.position));

    offset = MSS_Write(buffer, offset, bufferSize, &primaryAliasIndex, sizeof(primaryAliasIndex));
    offset = MSS_Write(buffer, offset, bufferSize, &secondaryAliasIndex, sizeof(secondaryAliasIndex));
    offset = MSS_SaveChanInfo(buffer, offset, bufferSize, channel);
    return MSS_Write(buffer, offset, bufferSize, &savedState, sizeof(savedState));
}

/* Source: CoDUOMP.exe 0x00456330..0x00456586.
 * Name and source-level call structure: exact same-module Mac symbol
 * MSS_RestoreStreamChannel. As in MSS_Restore2DChannel, the secondary alias
 * index is consumed but the primary alias is resolved for both endpoints. */
int32_t MSS_RestoreStreamChannel(const uint8_t *buffer, int32_t offset, int32_t bufferSize, int32_t primaryAliasIndex,
                                 int32_t requestedChannelIndex)
{
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    uint16_t discardedSecondaryAliasIndex = 0;
    mss_channel_info_t savedChannel;
    mss_stream_channel_save_t savedState;

    offset = MSS_Read(buffer, offset, bufferSize, &discardedSecondaryAliasIndex, sizeof(discardedSecondaryAliasIndex));
    offset = MSS_RestoreChanInfo(buffer, offset, bufferSize, &savedChannel);
    offset = MSS_Read(buffer, offset, bufferSize, &savedState, sizeof(savedState));

    snd_alias_t *const primaryAlias = Com_GetSoundAlias(primaryAliasIndex, SND_ALIAS_BANK_CGAME);
    snd_alias_t *const secondaryAlias = Com_GetSoundAlias(primaryAliasIndex, SND_ALIAS_BANK_CGAME);
    if (primaryAlias == NULL || secondaryAlias == NULL || primaryAlias->type != SND_ALIAS_TYPE_STREAMED ||
        secondaryAlias->type != SND_ALIAS_TYPE_STREAMED ||
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        (MSS_IsAliasChannel3D(primaryAlias->channel) && (uint32_t)savedChannel.effectId >= (uint32_t)MSS_EFFECT_ID_LIMIT) ||
        !MSS_ValidateSoundAliasBlend(primaryAlias, secondaryAlias, qfalse)) {
        return offset;
    }

    int32_t channelIndex = requestedChannelIndex;
    int32_t durationMsec;
    if (requestedChannelIndex < 0) {
        durationMsec = MSS_StartAliasStream(&channelIndex, primaryAlias, secondaryAlias, savedChannel.aliasBlend, savedChannel.effectId,
                                            savedState.position, savedState.logicalVolume, MSS_FULL_VOLUME, 0, savedState.startFraction);
    } else {
        durationMsec =
            MSS_StartAliasStreamOnChannel(primaryAlias, secondaryAlias, savedChannel.aliasBlend, savedChannel.effectId, savedState.position,
                                          savedState.logicalVolume, MSS_FULL_VOLUME, 0, savedState.startFraction, requestedChannelIndex);
    }

    if (channelIndex >= 0) {
        const int32_t streamIndex = channelIndex - MSS_STREAM_CHANNEL_FIRST;
        miles_stream_handle_t const stream = mss_streamHandles[streamIndex];
        const int32_t elapsedMsec = FastRound((float)((long double)durationMsec * (long double)savedState.startFraction));
        mss_channelInfo[channelIndex].endTime -= elapsedMsec;
        AIL_set_stream_volume_pan(stream, mss_effectVolume * savedState.relativeVolume, savedState.pan);
        const float scaledPlaybackRate =
            (float)((long double)savedState.basePlaybackRate * (long double)mss_channelInfo[channelIndex].aliasPitchScale *
                    (long double)mss_playbackRateScale);
        AIL_set_stream_playback_rate(stream, FastRound(scaledPlaybackRate));
        memcpy(mss_channelInfo[channelIndex].effectOffset, savedChannel.effectOffset, sizeof(savedChannel.effectOffset));
    }
    return offset;
}

/* Source: CoDUOMP.exe 0x00456590..0x004567c3.
 * Name and source-level loop structure: exact same-module Mac symbol
 * MSS_Save. The outer format contains the cgame alias checksum, channel
 * volume/fade state, then zero-terminated 3D, 2D, and stream channel lists. */
int32_t MSS_Save(uint8_t *saveData, int32_t saveCapacity)
{
    int32_t offset = 0;
    const int32_t aliasChecksum = Com_SoundAliasChecksum(SND_ALIAS_BANK_CGAME);
    offset = MSS_Write(saveData, offset, saveCapacity, &aliasChecksum, sizeof(aliasChecksum));
    offset = MSS_Write(saveData, offset, saveCapacity, mss_channelVolumes, sizeof(mss_channelVolumes));

    const mss_environment_save_t environment = {mss_roomType, mss_reverbLevel, mss_reverbTarget, mss_reverbRate};
    offset = MSS_Write(saveData, offset, saveCapacity, &environment, sizeof(environment));
    offset = MSS_Write(saveData, offset, saveCapacity, &mss_backgroundFades[0], sizeof(mss_backgroundFades[0]));
    offset = MSS_Write(saveData, offset, saveCapacity, &mss_backgroundFades[3], sizeof(mss_backgroundFades[3]));
    offset = MSS_Write(saveData, offset, saveCapacity, &mss_backgroundFades[4], sizeof(mss_backgroundFades[4]));

    if (mss_digitalDriver != NULL) {
        for (int32_t channelIndex = 0; channelIndex < mss_max3DChannels; ++channelIndex) {
            offset = MSS_Save3DChannel(saveData, offset, saveCapacity, channelIndex);
        }
    }
    const uint16_t listTerminator = 0;
    offset = MSS_Write(saveData, offset, saveCapacity, &listTerminator, sizeof(listTerminator));

    if (mss_digitalDriver != NULL) {
        const int32_t end2DChannel = MSS_2D_CHANNEL_FIRST + mss_2dChannelCount;
        for (int32_t channelIndex = MSS_2D_CHANNEL_FIRST; channelIndex < end2DChannel; ++channelIndex) {
            offset = MSS_Save2DChannel(saveData, offset, saveCapacity, channelIndex);
        }
    }
    offset = MSS_Write(saveData, offset, saveCapacity, &listTerminator, sizeof(listTerminator));

    if (mss_digitalDriver != NULL) {
        const int32_t endStreamChannel = MSS_STREAM_CHANNEL_FIRST + mss_streamChannelCount;
        for (int32_t channelIndex = MSS_STREAM_CHANNEL_FIRST; channelIndex < endStreamChannel; ++channelIndex) {
            offset = MSS_SaveStreamChannel(saveData, offset, saveCapacity, channelIndex);
        }
    }
    return MSS_Write(saveData, offset, saveCapacity, &listTerminator, sizeof(listTerminator));
}

/* Source: CoDUOMP.exe 0x004567d0..0x00456a7e.
 * Name and source-level loop structure: exact same-module Mac symbol
 * MSS_Restore. A checksum mismatch is nonfatal and leaves the current sound
 * state untouched; malformed/trailing data is a drop error after the whole
 * expected format has been consumed. */
void MSS_Restore(const uint8_t *saveData, int32_t saveSize)
{
    if (mss_digitalDriver == NULL)
        return;

    int32_t savedAliasChecksum;
    int32_t offset = MSS_Read(saveData, 0, saveSize, &savedAliasChecksum, sizeof(savedAliasChecksum));
    if (savedAliasChecksum != Com_SoundAliasChecksum(SND_ALIAS_BANK_CGAME)) {
        Com_Printf("^3Couldn't restore sound state: sound aliases have changed "
                   "since saving.\n");
        return;
    }

    offset = MSS_Read(saveData, offset, saveSize, mss_channelVolumes, sizeof(mss_channelVolumes));
    mss_environment_save_t environment;
    offset = MSS_Read(saveData, offset, saveSize, &environment, sizeof(environment));
    mss_roomType = environment.roomType;
    mss_reverbLevel = environment.reverbLevel;
    mss_reverbTarget = environment.reverbTarget;
    mss_reverbRate = environment.reverbRatePerMsec;
    AIL_set_digital_master_room_type(mss_digitalDriver, mss_roomType);
    AIL_set_3D_room_type(mss_3dProvider, mss_roomType);

    offset = MSS_Read(saveData, offset, saveSize, &mss_backgroundFades[0], sizeof(mss_backgroundFades[0]));
    offset = MSS_Read(saveData, offset, saveSize, &mss_backgroundFades[3], sizeof(mss_backgroundFades[3]));
    offset = MSS_Read(saveData, offset, saveSize, &mss_backgroundFades[4], sizeof(mss_backgroundFades[4]));

    for (;;) {
        uint16_t primaryAliasIndex = 0;
        offset = MSS_Read(saveData, offset, saveSize, &primaryAliasIndex, sizeof(primaryAliasIndex));
        if (primaryAliasIndex == 0)
            break;
        offset = MSS_Restore3DChannel(saveData, offset, saveSize, primaryAliasIndex);
    }

    for (;;) {
        uint16_t primaryAliasIndex = 0;
        offset = MSS_Read(saveData, offset, saveSize, &primaryAliasIndex, sizeof(primaryAliasIndex));
        if (primaryAliasIndex == 0)
            break;
        offset = MSS_Restore2DChannel(saveData, offset, saveSize, primaryAliasIndex);
    }

    for (int32_t backgroundIndex = 0; backgroundIndex < MSS_ALIAS_STREAM_SLOT_FIRST; ++backgroundIndex) {
        if (backgroundIndex == MSS_AMBIENT_BACKGROUND_FIRST_INDEX || backgroundIndex == MSS_AMBIENT_BACKGROUND_SECOND_INDEX) {
            continue;
        }
        uint16_t primaryAliasIndex = 0;
        offset = MSS_Read(saveData, offset, saveSize, &primaryAliasIndex, sizeof(primaryAliasIndex));
        if (primaryAliasIndex != 0) {
            offset = MSS_RestoreStreamChannel(saveData, offset, saveSize, primaryAliasIndex, MSS_STREAM_CHANNEL_FIRST + backgroundIndex);
        }
    }

    for (;;) {
        uint16_t primaryAliasIndex = 0;
        offset = MSS_Read(saveData, offset, saveSize, &primaryAliasIndex, sizeof(primaryAliasIndex));
        if (primaryAliasIndex == 0)
            break;
        offset = MSS_RestoreStreamChannel(saveData, offset, saveSize, primaryAliasIndex, -1);
    }

    if (offset != saveSize) {
        Com_Error(ERR_DROP, "\x15"
                            "sound state buffer size too big; probably due to an "
                            "old or corrupt savegame\n");
    }
}

/* Source: CoDUOMP.exe 0x00456a80..0x00456ad7, recovered from the executable
 * gap after MSS_Restore. Windows-only role name: MSS_QueueRestore. This entry
 * owns a private pending copy because the actual restore is deferred until
 * MSS_Update reaches an unpaused frame. Music and ambient streams survive
 * while the other active sounds are stopped immediately. */
void MSS_QueueRestore(const uint8_t *saveData, int32_t saveSize)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (saveData == NULL || saveSize <= 0 || saveSize > MSS_SAVE_MAX_BYTE_COUNT) {
        Com_Error(ERR_DROP,
                  "\x15"
                  "invalid queued sound state size: %i bytes\n",
                  saveSize);
        return;
    }

    MSS_StopSounds(MSS_STOP_PRESERVE_MUSIC | MSS_STOP_PRESERVE_AMBIENT);
    if (mss_restoreBuffer != NULL)
        Z_FreeInternal(mss_restoreBuffer);

    mss_restoreSize = saveSize;
    const size_t byteCount = (uint32_t)saveSize;
    uint8_t *const pendingRestore = Z_MallocInternal(byteCount);
    if (pendingRestore == NULL)
        Sys_OutOfMemory();
    memset(pendingRestore, 0, byteCount);
    mss_restoreBuffer = pendingRestore;
    Com_Memcpy(pendingRestore, saveData, byteCount);
}

/* Source: CoDUOMP.exe 0x00456ae0..0x00456bde.
 * Name and source-level loop structure: exact same-module Mac symbol
 * MSS_GetSoundOverlay2D. Free slots clear only soundFile; callers own the
 * remaining fields of unused records. */
int32_t MSS_GetSoundOverlay2D(mss_sound_overlay_t *overlay, int32_t maxCount)
{
    if (maxCount > mss_2dChannelCount)
        maxCount = mss_2dChannelCount;

    for (int32_t index = 0; index < maxCount; ++index) {
        const int32_t channelIndex = MSS_2D_CHANNEL_FIRST + index;
        mss_sound_overlay_t *const entry = &overlay[index];
        if (MSS_Is2DChannelFree(channelIndex)) {
            entry->soundFile = NULL;
            continue;
        }

        const mss_channel_info_t *const channel = &mss_channelInfo[channelIndex];
        miles_sample_handle_t const sample = mss_2dSampleHandles[index];
        entry->soundFile = channel->alias->soundFile;
        int32_t playbackRate = AIL_sample_playback_rate(sample);
        if (playbackRate == 0)
            playbackRate = channel->basePlaybackRate;
        entry->logicalVolume = channel->logicalVolume * MSS_OVERLAY_VOLUME_SCALE;
        AIL_sample_volume_pan(sample, &entry->relativeVolume, NULL);
        entry->relativeVolume *= MSS_OVERLAY_VOLUME_SCALE;
        if (mss_effectVolume != MSS_FULL_VOLUME)
            entry->relativeVolume /= mss_effectVolume;
        entry->basePlaybackRate = channel->basePlaybackRate;
        entry->pitchScale = (float)((long double)playbackRate / (long double)channel->basePlaybackRate);
    }
    return maxCount;
}

/* Source: CoDUOMP.exe 0x00456be0..0x00456d07.
 * Name and source-level loop structure: exact same-module Mac symbol
 * MSS_GetSoundOverlay3D. The original queries the current Miles position
 * even though the optimized Win32 body does not retain it in the overlay. */
int32_t MSS_GetSoundOverlay3D(mss_sound_overlay_t *overlay, int32_t maxCount)
{
    if (maxCount > mss_max3DChannels)
        maxCount = mss_max3DChannels;

    for (int32_t channelIndex = 0; channelIndex < maxCount; ++channelIndex) {
        mss_sound_overlay_t *const entry = &overlay[channelIndex];
        if (MSS_Is3DChannelFree(channelIndex)) {
            entry->soundFile = NULL;
            continue;
        }

        const mss_channel_info_t *const channel = &mss_channelInfo[channelIndex];
        miles_3d_sample_handle_t const sample = mss_3dSampleHandles[channelIndex];
        entry->soundFile = channel->alias->soundFile;
        (void)AIL_3D_sample_length(sample);
        int32_t playbackRate = AIL_3D_sample_playback_rate(sample);
        if (playbackRate == 0)
            playbackRate = channel->basePlaybackRate;
        vec3_t unusedPosition;
        AIL_3D_position(sample, &unusedPosition[0], &unusedPosition[1], &unusedPosition[2]);
        entry->logicalVolume = channel->logicalVolume * MSS_OVERLAY_VOLUME_SCALE;
        entry->relativeVolume = AIL_3D_sample_volume(sample) * MSS_OVERLAY_VOLUME_SCALE;
        if (mss_effectVolume != MSS_FULL_VOLUME)
            entry->relativeVolume /= mss_effectVolume;
        entry->basePlaybackRate = channel->basePlaybackRate;
        /* Unlike the 2D and stream bodies, this path stores both integer
         * conversions as binary32 before reloading them for the divide. */
        const float playbackRateFloat = (float)playbackRate;
        const float basePlaybackRateFloat = (float)channel->basePlaybackRate;
        entry->pitchScale = (float)((long double)playbackRateFloat / (long double)basePlaybackRateFloat);
    }
    return maxCount;
}

/* Source: CoDUOMP.exe 0x00456d10..0x00456e0f.
 * Name and source-level loop structure: exact same-module Mac symbol
 * MSS_GetSoundOverlayStream. MSS_IsStreamChannelFree also performs the
 * original close-and-clear of streams whose Miles status is done. */
int32_t MSS_GetSoundOverlayStream(mss_sound_overlay_t *overlay, int32_t maxCount)
{
    if (maxCount > mss_streamChannelCount)
        maxCount = mss_streamChannelCount;

    for (int32_t index = 0; index < maxCount; ++index) {
        const int32_t channelIndex = MSS_STREAM_CHANNEL_FIRST + index;
        mss_sound_overlay_t *const entry = &overlay[index];
        if (MSS_IsStreamChannelFree(channelIndex)) {
            entry->soundFile = NULL;
            continue;
        }

        const mss_channel_info_t *const channel = &mss_channelInfo[channelIndex];
        miles_stream_handle_t const stream = mss_streamHandles[index];
        entry->soundFile = channel->alias->soundFile;
        const int32_t playbackRate = AIL_stream_playback_rate(stream);
        entry->logicalVolume = channel->logicalVolume * MSS_OVERLAY_VOLUME_SCALE;
        AIL_stream_volume_pan(stream, &entry->relativeVolume, NULL);
        entry->relativeVolume *= MSS_OVERLAY_VOLUME_SCALE;
        if (mss_effectVolume != MSS_FULL_VOLUME)
            entry->relativeVolume /= mss_effectVolume;
        entry->basePlaybackRate = channel->basePlaybackRate;
        entry->pitchScale = (float)((long double)playbackRate / (long double)channel->basePlaybackRate);
    }
    return maxCount;
}

/* Source: CoDUOMP.exe 0x00456e10..0x00456e56.
 * Name and dispatch values: exact same-module Mac symbol
 * MSS_GetSoundOverlay. */
int32_t MSS_GetSoundOverlay(mssSoundOverlayType_t overlayType, mss_sound_overlay_t *overlay, int32_t maxCount, int32_t *cpuPercent)
{
    if (mss_digitalDriver == NULL)
        return 0;
    if (cpuPercent != NULL)
        *cpuPercent = mss_cpuPercent;

    switch (overlayType) {
    case MSS_SOUND_OVERLAY_3D:
        return MSS_GetSoundOverlay3D(overlay, maxCount);
    case MSS_SOUND_OVERLAY_STREAM:
        return MSS_GetSoundOverlayStream(overlay, maxCount);
    case MSS_SOUND_OVERLAY_2D:
        return MSS_GetSoundOverlay2D(overlay, maxCount);
    default:
        return 0;
    }
}

/* Source: CoDUOMP.exe 0x00456e60..0x00456f41.
 * Name: exact same-module Mac symbol InitEAXManager. The Mac implementation
 * is a stub because EAXMan is Windows-only; this shared body reaches the same
 * disabled state whenever the selected sound provider does not expose EAX.
 * The Windows body dynamically loads EAXMan.dll and resolves its one factory
 * rather than making EAXMan a required link dependency. */
void InitEAXManager(void)
{
    mss_eaxEnvironmentLoaded = qfalse;

    if (mss_eaxAvailable) {
        mss_eaxLibrary = coduomp_library_open("EAXMan.dll");
        if (mss_eaxLibrary != NULL) {
            eax_manager_create_t createManager = NULL;
            coduomp_library_symbol(mss_eaxLibrary, "EaxManagerCreate", &createManager, sizeof(createManager));
            if (createManager != NULL && createManager(&mss_eaxManager) == MSS_EAX_RESULT_OK) {
                const cvar_t *const mapNameCvar = Cvar_FindVar("mapname");
                const char *const mapName = mapNameCvar != NULL ? mapNameCvar->string : "";
                const size_t mapNameLength = strlen(mapName);
                if (mss_eaxMapNameWasTruncated != (mapNameLength >= sizeof(mss_eaxMapName)) ||
                    (mapNameLength >= sizeof(mss_eaxMapName) ? strncmp(mss_eaxMapName, mapName, sizeof(mss_eaxMapName) - 1) != 0
                                                             : strcmp(mss_eaxMapName, mapName) != 0)) {
                    EALFileInit(mapName);
                    /* NOT_FROM_ORIGINAL_SOURCE: validate the loaded audio data and playback state before crossing the Miles boundary. */
                    Q_strncpyz(mss_eaxMapName, mapName, sizeof(mss_eaxMapName));
                    mss_eaxMapNameWasTruncated = mapNameLength >= sizeof(mss_eaxMapName);
                }
                return;
            }
        }
    }

    if (mss_eaxLibrary != NULL) {
        coduomp_library_close(mss_eaxLibrary);
        mss_eaxLibrary = NULL;
    }
    mss_eaxManager = NULL;
    mss_eaxAvailable = qfalse;
}

/* Source: CoDUOMP.exe 0x00456f50..0x00456fd1.
 * Name: exact same-module Mac symbol ReleaseEAXManager. The two EAXMan calls
 * use the external object's stdcall vtable; the Mac symbol is an empty stub. */
void ReleaseEAXManager(void)
{
    mss_eaxAvailable = qfalse;
    mss_eaxMapName[0] = '\0';
    mss_eaxMapNameWasTruncated = qfalse;
    mss_eaxRoomId = MSS_EAX_ROOM_ID_INVALID;

    if (mss_eaxEnvironmentLoaded) {
        (void)mss_eaxManager->vtable->clearEnvironment(mss_eaxManager, MSS_EAX_ENVIRONMENT_NONE);
        mss_eaxEnvironmentLoaded = qfalse;
    }

    int32_t dryRoomLevel = MSS_EAX_DRY_ROOM_LEVEL;
    (void)AIL_set_3D_provider_preference(mss_3dProvider, "EAX2 room", &dryRoomLevel);

    if (mss_eaxManager != NULL) {
        (void)mss_eaxManager->vtable->release(mss_eaxManager);
        mss_eaxManager = NULL;
    }
    if (mss_eaxLibrary != NULL) {
        coduomp_library_close(mss_eaxLibrary);
        mss_eaxLibrary = NULL;
    }
}

/* Source: CoDUOMP.exe 0x00456fe0..0x00457092.
 * Name and source-level call structure: exact same-module Mac symbol
 * EALFileInit. MSVC inlines StripExtension and UnloadEALFile here. The two
 * adjacent 64-byte stack objects and the unbounded sprintf call are proven by
 * the Windows stack offsets. */
void EALFileInit(const char *mapName)
{
    if (mss_eaxEnvironmentLoaded)
        UnloadEALFile();

    char strippedMapName[MAX_QPATH];
    char ealPath[MAX_QPATH];
    /* NOT_FROM_ORIGINAL_SOURCE: validate the loaded audio data and playback state before crossing the Miles boundary. */
    if (strlen(mapName) >= sizeof(strippedMapName)) {
        Com_Printf("WARNING: ignoring overlong EAX map name\n");
        mss_eaxEnvironmentLoaded = qfalse;
        return;
    }
    StripExtension(mapName, strippedMapName);
    if (strlen(strippedMapName) > sizeof(ealPath) - sizeof("eagle/") - sizeof(".eal") + 1) {
        Com_Printf("WARNING: ignoring overlong EAX environment path\n");
        mss_eaxEnvironmentLoaded = qfalse;
        return;
    }
    Com_sprintf(ealPath, sizeof(ealPath), "eagle/%s.eal", strippedMapName);

    mss_eaxEnvironmentLoaded = LoadEALFile(ealPath);
    if (!mss_eaxEnvironmentLoaded) {
        int32_t dryRoomLevel = MSS_EAX_DRY_ROOM_LEVEL;
        (void)AIL_set_3D_provider_preference(mss_3dProvider, "EAX2 room", &dryRoomLevel);
    }
}

/* Source: CoDUOMP.exe 0x004570a0..0x00457144.
 * Name: exact same-module Mac symbol LoadEALFile (a stub there because EAX is
 * Windows-only). The Windows EAXMan method accepts either an engine-loaded
 * memory image (source type 2) or a filesystem path (source type 0). The
 * original uses different success tests for those two API modes: exactly zero
 * for memory, any nonnegative HRESULT for a path. */
qboolean LoadEALFile(const char *path)
{
    void *fileData = NULL;

    if (mss_eaxManager == NULL || !mss_eaxAvailable)
        return qfalse;
    if (strstr(path, "nomap") != NULL)
        return qfalse;

    mss_eaxRoomId = MSS_EAX_ROOM_ID_INVALID;
    const int32_t fileLength = FS_ReadFile(path, &fileData);
    if (fileData != NULL && fileLength != MSS_EAL_FILE_READ_FAILED) {
        /* NOT_FROM_ORIGINAL_SOURCE: validate the loaded audio data and playback state before crossing the Miles boundary. */
        if (coduomp_mss_validate_eal_extent(fileData, fileLength) == qfalse) {
            Com_Printf("WARNING: rejecting malformed EAX environment '%s'\n", path);
            FS_FreeFile(fileData);
            return qfalse;
        }
        const int32_t result = mss_eaxManager->vtable->loadEnvironment(mss_eaxManager, fileData, MSS_EAL_SOURCE_MEMORY);
        FS_FreeFile(fileData);
        return result == MSS_EAX_RESULT_OK ? qtrue : qfalse;
    }

    const int32_t result = mss_eaxManager->vtable->loadEnvironment(mss_eaxManager, path, MSS_EAL_SOURCE_PATH);
    return result < MSS_EAX_RESULT_OK ? qfalse : qtrue;
}

/* Source: CoDUOMP.exe 0x00457150..0x00457171, recovered from the executable
 * gap between LoadEALFile and UpdateEAXListener. Name and behavior: exact
 * same-module Mac symbol UnloadEALFile. */
void UnloadEALFile(void)
{
    if (!mss_eaxEnvironmentLoaded)
        return;

    (void)mss_eaxManager->vtable->clearEnvironment(mss_eaxManager, MSS_EAX_ENVIRONMENT_NONE);
    mss_eaxEnvironmentLoaded = qfalse;
}

/* Source: CoDUOMP.exe 0x00457180..0x0045722d.
 * Name: exact same-module Mac symbol UpdateEAXListener (a platform stub in
 * that build). EAXMan uses x,z,y coordinates. A newly selected environment is
 * pushed to Miles only when it differs from mss_eaxRoomId; the room id is not
 * cached when either EAXMan query or the listener-application gate fails. */
void UpdateEAXListener(const vec3_t origin)
{
    if (!mss_eaxEnvironmentLoaded || mss_underwaterEffectActive)
        return;

    const vec3_t eaxPosition = {origin[0], origin[2], origin[1]};
    int32_t environmentId;
    if (mss_eaxManager->vtable->queryListener(mss_eaxManager, MSS_EAX_LISTENER_ID, eaxPosition, &environmentId, qtrue) <
        MSS_EAX_RESULT_OK) {
        return;
    }
    if (environmentId == mss_eaxRoomId || !mss_eaxApplyListener)
        return;

    eax_listener_properties_t properties;
    if (mss_eaxManager->vtable->getEnvironment(mss_eaxManager, environmentId, &properties) < MSS_EAX_RESULT_OK) {
        return;
    }

    properties.airAbsorptionHF = 0.0f;
    (void)AIL_set_3D_provider_preference(mss_3dProvider, "EAX all parameters", &properties);
    mss_eaxRoomId = environmentId;
}

/* Source: CoDUOMP.exe 0x00457230..0x00457300.
 * Name: exact same-module Mac symbol UpdateEAXBuffer (a platform stub there).
 * The Windows caller carries the Miles sample in EDI and the world position in
 * EAX; both are real source inputs. The previous one-argument prototype lost
 * that sample dependency. EAXMan again consumes x,z,y coordinates and returns
 * the five Miles EAX2 sample preferences used below. */
void UpdateEAXBuffer(miles_3d_sample_handle_t sample, const vec3_t position)
{
    if (!mss_eaxEnvironmentLoaded || mss_underwaterEffectActive)
        return;

    const vec3_t eaxPosition = {position[0], position[2], position[1]};
    int32_t obstruction;
    float obstructionLFRatio;
    int32_t occlusion;
    float occlusionLFRatio;
    float occlusionRoomRatio;
    eax_source_query_workspace_t workspace;
    workspace.scale = 1.0f;

    if (mss_eaxManager->vtable->querySource(mss_eaxManager, MSS_EAX_SOURCE_ID, eaxPosition, &obstruction, &obstructionLFRatio, &occlusion,
                                            &occlusionLFRatio, &occlusionRoomRatio, &workspace, qfalse) != MSS_EAX_RESULT_OK) {
        return;
    }

    if (mss_eaxApplyObstruction) {
        (void)AIL_set_3D_sample_preference(sample, "EAX2 sample obstruction", &obstruction);
        (void)AIL_set_3D_sample_preference(sample, "EAX2 sample obstruction LF ratio", &obstructionLFRatio);
    }
    if (mss_eaxApplyOcclusion) {
        (void)AIL_set_3D_sample_preference(sample, "EAX2 sample occlusion", &occlusion);
        (void)AIL_set_3D_sample_preference(sample, "EAX2 sample occlusion LF ratio", &occlusionLFRatio);
        (void)AIL_set_3D_sample_preference(sample, "EAX2 sample occlusion room ratio", &occlusionRoomRatio);
    }
}

/* Source: CoDUOMP.exe 0x004509f0..0x00450b20.
 * Name: exact same-module Mac symbol MSS_Init2D. The Miles digital-format
 * argument is its 1/2 format enum, while mss_sampleBits retains the human
 * 8/16-bit value used by the rest of the sound state. */
qboolean MSS_Init2D(void)
{
    int32_t sampleRate;
    switch (mss_khz->integer) {
    case 11:
        sampleRate = 11025;
        break;
    case 44:
        sampleRate = 44100;
        break;
    case 22:
        sampleRate = 22050;
        break;
    default:
        Com_Printf("invalid value %i for mss_khz, using 22 khz instead\n", mss_khz->integer);
        sampleRate = 22050;
        break;
    }

    milesDigitalFormat_t sampleFormat;
    switch (mss_bits->integer) {
    case 8:
        sampleFormat = MILES_DIGITAL_FORMAT_8_BIT;
        break;
    case 16:
        sampleFormat = MILES_DIGITAL_FORMAT_16_BIT;
        break;
    default:
        Com_Printf("invalid value %i for mss_bits (should be 8 or 16), using 16 instead\n", mss_bits->integer);
        sampleFormat = MILES_DIGITAL_FORMAT_16_BIT;
        break;
    }

    const int32_t channelCount = mss_stereo->integer != 0 ? 2 : 1;
    const char *const channelLayout = channelCount == 2 ? "stereo" : "mono";
    const int32_t sampleBits = (int32_t)sampleFormat * 8;
    Com_Printf("Attempting %i kHz %i bit %s sound\n", sampleRate / 1000, sampleBits, channelLayout);

    (void)AIL_set_preference(MILES_MIXER_CHANNEL_PREFERENCE, MSS_TOTAL_CHANNEL_COUNT);
    mss_digitalDriver = AIL_open_digital_driver(sampleRate, sampleFormat, channelCount, 0);
    if (mss_digitalDriver == NULL) {
        Com_Printf("couldn't initialize 2D provider: %s\n", AIL_last_error());
        return qfalse;
    }

    mss_2dChannelCount = MSS_2D_CHANNEL_CAPACITY;
    mss_streamChannelCount = MSS_STREAM_CHANNEL_CAPACITY;
    mss_sampleRate = sampleRate;
    mss_sampleBits = sampleBits;
    mss_channelCount = channelCount;
    mss_playbackRateScale = 1.0f;

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    Com_Printf("2D provider initialized at %i %i %i\n", mss_sampleRate, mss_sampleBits, mss_channelCount);
    return qtrue;
}

/* Source: CoDUOMP.exe 0x00450b30..0x00450d69.
 * Name: exact same-module Mac symbol MSS_Init3DProvider. Miles HPROVIDER and
 * HPROENUM are 32-bit API handles rather than host pointers. The distance
 * factor literal is the exact float encoded by 0x3cd013a9; it represents
 * 0.0254 metres per inch. */
qboolean MSS_Init3DProvider(const char *preferredProviderName, qboolean listProviders)
{
    /* NOT_FROM_ORIGINAL_SOURCE: on native builds this is the enumerated
     * platform adapter. Existing configs naming the retail Miles provider
     * reach this fallback and are migrated by the original Cvar_Set2 path. */
    static const char fallbackProviderName[] = CODUOMP_3D_PROVIDER_NAME;
    miles_3d_provider_enumerator_t enumerator = 0;
    miles_3d_provider_t requestedProvider = 0;
    miles_3d_provider_t fallbackProvider = 0;
    miles_3d_provider_t enumeratedProvider;
    const char *enumeratedName;

    if (listProviders)
        Com_Printf("available 3D providers:\n");

    while (AIL_enumerate_3D_providers(&enumerator, &enumeratedProvider, &enumeratedName)) {
        if (listProviders)
            Com_Printf("  %s\n", enumeratedName);
        if (coduo_crt_stricmp(enumeratedName, preferredProviderName) == 0)
            requestedProvider = enumeratedProvider;
        if (coduo_crt_stricmp(enumeratedName, fallbackProviderName) == 0)
            fallbackProvider = enumeratedProvider;
    }

    mss_3dProvider = 0;
    if (requestedProvider != 0) {
        if (AIL_open_3D_provider(requestedProvider) == 0) {
            Com_Printf("using 3D provider '%s'\n", preferredProviderName);
            mss_3dProvider = requestedProvider;
        } else {
            Com_Printf("couldn't open 3D provider '%s': %s\n", preferredProviderName, AIL_last_error());
        }
    }

    if (mss_3dProvider == 0) {
        if (fallbackProvider == 0 || fallbackProvider == requestedProvider)
            return qfalse;

        if (preferredProviderName[0] != '\0' && coduo_crt_stricmp(preferredProviderName, fallbackProviderName) != 0) {
#if defined(__APPLE__) || defined(__linux__)
            Com_Printf("trying to use '%s' instead of '%s'\n", fallbackProviderName, preferredProviderName);
#else
            Com_Printf("trying to use 'Miles Fast 2D Positional Audio' instead of '%s'\n", preferredProviderName);
#endif
        }

        if (AIL_open_3D_provider(fallbackProvider) != 0) {
            Com_Printf("couldn't open 3D provider '%s': %s\n", fallbackProviderName, AIL_last_error());
            return qfalse;
        }

        Com_Printf("using 3D provider '%s'\n", fallbackProviderName);
        /* 0x00450d13 publishes the working handle before Cvar_Set2. */
        mss_3dProvider = fallbackProvider;
        (void)Cvar_Set2("mss_3d_provider", fallbackProviderName, qtrue);
    }

    (void)AIL_3D_provider_attribute(mss_3dProvider, "Maximum supported samples", &mss_max3DChannels);
    if (mss_max3DChannels > MSS_3D_CHANNEL_CAPACITY)
        mss_max3DChannels = MSS_3D_CHANNEL_CAPACITY;
    Com_Printf("%i max 3D channels\n", mss_max3DChannels);

    AIL_set_3D_distance_factor(mss_3dProvider, 0.02539999969303608f);

    int32_t eaxRoomLowFrequency = 0;
    (void)AIL_3D_provider_attribute(mss_3dProvider, "EAX3 room LF", &eaxRoomLowFrequency);
    if (eaxRoomLowFrequency != -1)
        mss_eaxAvailable = qtrue;
    if (mss_eaxAvailable)
        InitEAXManager();
    return qtrue;
}

/* Source: CoDUOMP.exe 0x00450d70..0x00450ea8.
 * Name: exact same-module Mac symbol MSS_SetListener. The sound command record
 * supplies time, origin, and axis in that order. When EAX is active, map
 * settings are reloaded on a map-name change and the EAX listener is updated
 * before the new origin becomes the cached origin. */
void MSS_SetListener(int32_t time, const vec3_t origin, const axis_t axis)
{
    if (mss_digitalDriver == NULL)
        return;

    if (mss_eaxAvailable) {
        const cvar_t *const mapNameCvar = Cvar_FindVar("mapname");
        const char *const mapName = mapNameCvar != NULL ? mapNameCvar->string : "";
        const size_t mapNameLength = strlen(mapName);
        if (mss_eaxMapNameWasTruncated != (mapNameLength >= sizeof(mss_eaxMapName)) ||
            (mapNameLength >= sizeof(mss_eaxMapName) ? strncmp(mss_eaxMapName, mapName, sizeof(mss_eaxMapName) - 1) != 0
                                                     : strcmp(mss_eaxMapName, mapName) != 0)) {
            EALFileInit(mapName);
            /* NOT_FROM_ORIGINAL_SOURCE: validate the loaded audio data and playback state before crossing the Miles boundary. */
            Q_strncpyz(mss_eaxMapName, mapName, sizeof(mss_eaxMapName));
            mss_eaxMapNameWasTruncated = mapNameLength >= sizeof(mss_eaxMapName);
        }

        if (origin[0] != mss_listenerOrigin[0] || origin[1] != mss_listenerOrigin[1] || origin[2] != mss_listenerOrigin[2]) {
            UpdateEAXListener(origin);
        }
    }

    memcpy(mss_listenerAxis, axis, sizeof(mss_listenerAxis));
    memcpy(mss_listenerOrigin, origin, sizeof(mss_listenerOrigin));
    mss_listenerTime = time;
}

/* Source: CoDUOMP.exe 0x00450eb0..0x00450f37.
 * Name: exact same-module Mac symbol MSS_GetListener. Each output is optional,
 * and no cached state is exposed until the 2D digital driver exists. */
void MSS_GetListener(axis_t axis, vec3_t origin, int32_t *time)
{
    if (mss_digitalDriver == NULL)
        return;
    if (axis != NULL)
        memcpy(axis, mss_listenerAxis, sizeof(mss_listenerAxis));
    if (origin != NULL)
        memcpy(origin, mss_listenerOrigin, sizeof(mss_listenerOrigin));
    if (time != NULL)
        *time = mss_listenerTime;
}

/* Source: CoDUOMP.exe 0x00450f40..0x00450fd1.
 * Name: exact same-module Mac symbol MSS_InitChannels. Miles sample handles
 * are native opaque pointers; only the original i386 arrays occupy four bytes
 * per slot. Provider initialization has already capped the 3D count at the
 * proven 32-slot capacity. */
void MSS_InitChannels(void)
{
    for (int32_t channel = 0; channel < mss_2dChannelCount; ++channel) {
        mss_2dSampleHandles[channel] = AIL_allocate_sample_handle(mss_digitalDriver);
        if (mss_2dSampleHandles[channel] == NULL) {
            Com_Error(ERR_DROP, "\x15MILES 2D sound sample allocation failed on channel %i\n", channel + 1);
        }
    }

    for (int32_t channel = 0; channel < mss_max3DChannels; ++channel) {
        mss_3dSampleHandles[channel] = AIL_allocate_3D_sample_handle(mss_3dProvider);
        if (mss_3dSampleHandles[channel] == NULL) {
            Com_Error(ERR_DROP, "\x15MILES 3D sound sample allocation failed on channel %i\n", channel + 1);
        }
    }

    mss_ambientBackgroundIndex = MSS_AMBIENT_BACKGROUND_INITIAL_INDEX;
}

/* Source: CoDUOMP.exe 0x00454c80..0x00454ca8 and same-module Mac function
 * MSS_UpdatePause at code offset 0x2c1e0. MSVC also inlined the same body at
 * 0x00452ca0..0x00452cd3 and 0x00454d37..0x00454d5a. */
void MSS_UpdatePause(void)
{
    const qboolean shouldPause = cl_paused->integer != 0;
    if (shouldPause == mss_paused)
        return;
    if (shouldPause)
        MSS_PauseSounds();
    else
        MSS_UnpauseSounds();
}

/* Source: CoDUOMP.exe 0x00450fe0..0x00451024.
 * Name: exact same-module Mac symbol MSS_Attenuate. This is the original
 * linear distance rolloff, including its comparison behavior for NaNs and a
 * degenerate minimum/maximum interval. The result leaves the original body
 * live in ST0; the widened return records that no binary32 store occurs. */
long double MSS_Attenuate(float distance, float minimumDistance, float maximumDistance)
{
    const long double distancePastMinimum = (long double)distance - (long double)minimumDistance;
    if (distancePastMinimum <= (long double)0.0f)
        return (long double)1.0f;

    const long double attenuation = distancePastMinimum / ((long double)maximumDistance - (long double)minimumDistance);
    if (attenuation >= (long double)1.0f)
        return (long double)0.0f;
    return (long double)1.0f - attenuation;
}

/* Source: CoDUOMP.exe 0x00451030..0x004510cb.
 * Name: exact same-module Mac symbol MSS_GetCurrent3DPosition. Cgame VM
 * command 14 supplies the effect frame. X and Y are rounded after the first
 * two axis terms; Z rounds the origin-plus-first-axis term instead. All three
 * add the last axis term before their final binary32 stores. */
void MSS_GetCurrent3DPosition(int32_t effectId, const vec3_t localOffset, vec3_t worldPosition)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if ((uint32_t)effectId >= (uint32_t)MSS_EFFECT_ID_LIMIT) {
        worldPosition[0] = 0.0f;
        worldPosition[1] = 0.0f;
        worldPosition[2] = 0.0f;
        return;
    }

    vec3_t effectOrigin;
    axis_t effectAxis;
    (void)VM_Call(coduo_cgameVm, CGVM_GET_EFFECT_ORIGIN_AXIS, effectId, (intptr_t)effectOrigin, (intptr_t)effectAxis, 0, 0, 0, 0, 0, 0, 0,
                  0, 0);

    const long double originAxisX = (long double)effectAxis[0][0] * (long double)localOffset[0] + (long double)effectOrigin[0];
    const long double originAxisY = (long double)effectAxis[0][1] * (long double)localOffset[0] + (long double)effectOrigin[1];
    const float firstTwoAxesX = (float)((long double)effectAxis[1][0] * (long double)localOffset[1] + originAxisX);
    const float firstTwoAxesY = (float)((long double)effectAxis[1][1] * (long double)localOffset[1] + originAxisY);

    const float originAxisZ = (float)((long double)effectAxis[0][2] * (long double)localOffset[0] + (long double)effectOrigin[2]);
    const long double firstTwoAxesZ = (long double)effectAxis[1][2] * (long double)localOffset[1] + (long double)originAxisZ;

    const float resultX = (float)((long double)effectAxis[2][0] * (long double)localOffset[2] + (long double)firstTwoAxesX);
    const float resultY = (float)((long double)effectAxis[2][1] * (long double)localOffset[2] + (long double)firstTwoAxesY);
    const float resultZ = (float)((long double)effectAxis[2][2] * (long double)localOffset[2] + firstTwoAxesZ);

    worldPosition[2] = resultZ;
    worldPosition[0] = resultX;
    worldPosition[1] = resultY;
}

/* Source: CoDUOMP.exe 0x004510d0..0x0045117c.
 * Name: exact same-module Mac symbol MSS_Set3DPosition. Miles uses a different
 * axis convention: negative listener-right, listener-up, listener-forward.
 * Each dot product is rounded to float before crossing the Miles boundary. */
void MSS_Set3DPosition(miles_3d_sample_handle_t sample, const vec3_t worldPosition)
{
    const long double deltaX = (long double)worldPosition[0] - (long double)mss_listenerOrigin[0];
    const long double deltaY = (long double)worldPosition[1] - (long double)mss_listenerOrigin[1];
    const long double deltaZ = (long double)worldPosition[2] - (long double)mss_listenerOrigin[2];

    float listenerCoordinates[3];
    for (int32_t axis = 0; axis < 3; ++axis) {
        listenerCoordinates[axis] =
            (float)((long double)mss_listenerAxis[axis][2] * deltaZ + (long double)mss_listenerAxis[axis][1] * deltaY +
                    (long double)mss_listenerAxis[axis][0] * deltaX);
    }

    AIL_set_3D_position(sample, -listenerCoordinates[1], listenerCoordinates[2], listenerCoordinates[0]);
    UpdateEAXBuffer(sample, worldPosition);
}

/* Source: CoDUOMP.exe 0x00451180..0x00451197.
 * Name: exact same-module Mac symbol MSS_IsAliasChannel3D. Menu and the
 * local/music/announcer/shellshock group are nonspatial; auto plus the normal
 * weapon/voice/item/body channels use 3D positioning. */
qboolean MSS_IsAliasChannel3D(sndAliasChannel_t aliasChannel)
{
    if (aliasChannel == SND_ALIAS_CHANNEL_MENU)
        return qfalse;
    if (aliasChannel <= SND_ALIAS_CHANNEL_BODY)
        return qtrue;
    if (aliasChannel <= SND_ALIAS_CHANNEL_SHELLSHOCK)
        return qfalse;
    return qtrue;
}

/* Source: CoDUOMP.exe 0x00451330..0x00451376.
 * Name and parameter order: exact same-module Mac symbol
 * MSS_CompareReplacableChannels. Channels attached to the preferred effect
 * sort first; remaining channels sort by alias-channel priority and then by
 * end time. Explicit unsigned subtraction preserves the original wrapping
 * 32-bit comparator result without signed-overflow undefined behavior. */
int32_t MSS_CompareReplacableChannels(int32_t firstChannelIndex, int32_t secondChannelIndex, int32_t preferredEffectId)
{
    const mss_channel_info_t *const first = &mss_channelInfo[firstChannelIndex];
    const mss_channel_info_t *const second = &mss_channelInfo[secondChannelIndex];

    if (first->effectId != second->effectId) {
        if (first->effectId == preferredEffectId)
            return -1;
        if (second->effectId == preferredEffectId)
            return 1;
    }

    const int32_t channelDifference = (int32_t)((uint32_t)first->aliasChannel - (uint32_t)second->aliasChannel);
    if (channelDifference != 0)
        return channelDifference;
    return (int32_t)((uint32_t)first->endTime - (uint32_t)second->endTime);
}

/* Source: CoDUOMP.exe 0x00451380..0x00451396.
 * Name and parameter order: exact same-module Mac symbol
 * MSS_IsChannelReplacable. Alias channels with a numerically greater priority
 * value are protected from a request at a lower maximum channel value. */
qboolean MSS_IsChannelReplacable(int32_t channelIndex, sndAliasChannel_t maximumAliasChannel)
{
    return mss_channelInfo[channelIndex].aliasChannel <= maximumAliasChannel;
}

/* Source: CoDUOMP.exe 0x004514b0..0x0045152b.
 * Name: exact same-module Mac symbol MSS_FindReplacableChannel. The Windows
 * build passes the first index and count in ECX/EDX and the remaining values
 * on the stack. It rejects records above the caller's maximum alias-channel
 * priority, then prefers the requested effect, the lower alias-channel value,
 * and finally the earlier end time. The original spelling is retained. */
int32_t MSS_FindReplacableChannel(int32_t firstChannelIndex, int32_t channelCount, int32_t preferredEffectId,
                                  sndAliasChannel_t maximumAliasChannel)
{
    const int32_t endChannelIndex = firstChannelIndex + channelCount;
    int32_t bestChannelIndex = -1;

    for (int32_t channelIndex = firstChannelIndex; channelIndex < endChannelIndex; ++channelIndex) {
        if (!MSS_IsChannelReplacable(channelIndex, maximumAliasChannel))
            continue;
        if (bestChannelIndex < 0 || MSS_CompareReplacableChannels(channelIndex, bestChannelIndex, preferredEffectId) < 0) {
            bestChannelIndex = channelIndex;
        }
    }
    return bestChannelIndex;
}

/* Source: CoDUOMP.exe 0x00451530..0x0045154f.
 * Name: exact same-module Mac symbol MSS_Stop2DChannel. */
void MSS_Stop2DChannel(int32_t channelIndex)
{
    AIL_end_sample(mss_2dSampleHandles[channelIndex - MSS_2D_CHANNEL_FIRST]);
    mss_channelInfo[channelIndex].paused = qfalse;
}

/* Source: CoDUOMP.exe 0x00451550..0x0045156f.
 * Name: exact same-module Mac symbol MSS_Pause2DChannel. */
void MSS_Pause2DChannel(int32_t channelIndex)
{
    AIL_stop_sample(mss_2dSampleHandles[channelIndex - MSS_2D_CHANNEL_FIRST]);
    mss_channelInfo[channelIndex].paused = qtrue;
}

/* Source: CoDUOMP.exe 0x00451570..0x00451597.
 * Name: exact same-module Mac symbol MSS_Unpause2DChannel. The elapsed pause
 * duration is added to the channel's scheduled end time before it becomes
 * eligible for replacement again. */
void MSS_Unpause2DChannel(int32_t channelIndex, int32_t timeShift)
{
    AIL_resume_sample(mss_2dSampleHandles[channelIndex - MSS_2D_CHANNEL_FIRST]);
    mss_channelInfo[channelIndex].endTime += timeShift;
    mss_channelInfo[channelIndex].paused = qfalse;
}

/* Source: CoDUOMP.exe 0x004515a0..0x004515c8.
 * Name: exact same-module Mac symbol MSS_Is2DChannelFree. A paused sample is
 * deliberately retained even when Miles reports a completed sample. */
qboolean MSS_Is2DChannelFree(int32_t channelIndex)
{
    if (mss_channelInfo[channelIndex].paused)
        return qfalse;
    return AIL_sample_status(mss_2dSampleHandles[channelIndex - MSS_2D_CHANNEL_FIRST]) == MILES_SAMPLE_STATUS_DONE;
}

/* Source: CoDUOMP.exe 0x004515d0..0x00451664.
 * Name: exact same-module Mac symbol MSS_FindFree2DChannel. It first reuses a
 * completed, unpaused sample. When all active slots are occupied it applies
 * the shared replacement policy and explicitly ends the selected sample. */
int32_t MSS_FindFree2DChannel(int32_t preferredEffectId, sndAliasChannel_t maximumAliasChannel)
{
    for (int32_t sampleIndex = 0; sampleIndex < mss_2dChannelCount; ++sampleIndex) {
        const int32_t channelIndex = MSS_2D_CHANNEL_FIRST + sampleIndex;
        if (MSS_Is2DChannelFree(channelIndex))
            return channelIndex;
    }

    const int32_t channelIndex =
        MSS_FindReplacableChannel(MSS_2D_CHANNEL_FIRST, mss_2dChannelCount, preferredEffectId, maximumAliasChannel);
    if (channelIndex > 0)
        MSS_Stop2DChannel(channelIndex);
    return channelIndex;
}

/* Source: CoDUOMP.exe 0x00451670..0x0045168f.
 * Name: exact same-module Mac symbol MSS_Stop3DChannel. */
void MSS_Stop3DChannel(int32_t channelIndex)
{
    AIL_end_3D_sample(mss_3dSampleHandles[channelIndex]);
    mss_channelInfo[channelIndex].paused = qfalse;
}

/* Source: CoDUOMP.exe 0x00451690..0x004516af.
 * Name: exact same-module Mac symbol MSS_Pause3DChannel. */
void MSS_Pause3DChannel(int32_t channelIndex)
{
    AIL_stop_3D_sample(mss_3dSampleHandles[channelIndex]);
    mss_channelInfo[channelIndex].paused = qtrue;
}

/* Source: CoDUOMP.exe 0x004516b0..0x004516d7.
 * Name: exact same-module Mac symbol MSS_Unpause3DChannel. */
void MSS_Unpause3DChannel(int32_t channelIndex, int32_t timeShift)
{
    AIL_resume_3D_sample(mss_3dSampleHandles[channelIndex]);
    mss_channelInfo[channelIndex].endTime += timeShift;
    mss_channelInfo[channelIndex].paused = qfalse;
}

/* Source: CoDUOMP.exe 0x004516e0..0x00451708.
 * Name: exact same-module Mac symbol MSS_Is3DChannelFree. */
qboolean MSS_Is3DChannelFree(int32_t channelIndex)
{
    if (mss_channelInfo[channelIndex].paused)
        return qfalse;
    return AIL_3D_sample_status(mss_3dSampleHandles[channelIndex]) == MILES_SAMPLE_STATUS_DONE;
}

/* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
int32_t MSS_FindFree3DChannel(int32_t preferredEffectId, sndAliasChannel_t maximumAliasChannel)
{
    for (int32_t channelIndex = MSS_3D_CHANNEL_FIRST; channelIndex < mss_max3DChannels; ++channelIndex) {
        if (MSS_Is3DChannelFree(channelIndex))
            return channelIndex;
    }

    const int32_t channelIndex = MSS_FindReplacableChannel(MSS_3D_CHANNEL_FIRST, mss_max3DChannels, preferredEffectId, maximumAliasChannel);
    if (channelIndex > MSS_3D_CHANNEL_FIRST)
        MSS_Stop3DChannel(channelIndex);
    return channelIndex;
}

/* Source: CoDUOMP.exe 0x004517a0..0x004517c4.
 * Name: exact same-module Mac symbol MSS_StopStreamChannel. Stream channels
 * use logical IDs 32..44; the native handle array uses local indices 0..12. */
void MSS_StopStreamChannel(int32_t channelIndex)
{
    miles_stream_handle_t *const stream = &mss_streamHandles[channelIndex - MSS_STREAM_CHANNEL_FIRST];
    AIL_close_stream(*stream);
    *stream = NULL;
    mss_channelInfo[channelIndex].paused = qfalse;
}

/* Source: CoDUOMP.exe 0x004517d0..0x004517f1.
 * Name: exact same-module Mac symbol MSS_PauseStreamChannel. */
void MSS_PauseStreamChannel(int32_t channelIndex)
{
    AIL_pause_stream(mss_streamHandles[channelIndex - MSS_STREAM_CHANNEL_FIRST], qtrue);
    mss_channelInfo[channelIndex].paused = qtrue;
}

/* Source: CoDUOMP.exe 0x00451800..0x00451829.
 * Name: exact same-module Mac symbol MSS_UnpauseStreamChannel. */
void MSS_UnpauseStreamChannel(int32_t channelIndex, int32_t timeShift)
{
    AIL_pause_stream(mss_streamHandles[channelIndex - MSS_STREAM_CHANNEL_FIRST], qfalse);
    mss_channelInfo[channelIndex].endTime += timeShift;
    mss_channelInfo[channelIndex].paused = qfalse;
}

/* Source: CoDUOMP.exe 0x00451830..0x00451877.
 * Name: exact same-module Mac symbol MSS_IsStreamChannelFree. Completed
 * streams are closed here and their slots cleared before being reported free. */
qboolean MSS_IsStreamChannelFree(int32_t channelIndex)
{
    miles_stream_handle_t *const stream = &mss_streamHandles[channelIndex - MSS_STREAM_CHANNEL_FIRST];
    if (*stream == NULL)
        return qtrue;
    if (mss_channelInfo[channelIndex].paused)
        return qfalse;
    if (AIL_stream_status(*stream) != MILES_SAMPLE_STATUS_DONE)
        return qfalse;

    AIL_close_stream(*stream);
    *stream = NULL;
    return qtrue;
}

/* Source: CoDUOMP.exe 0x00451880..0x00451934.
 * Name: exact same-module Mac symbol MSS_FindFreeStreamChannel. Slots 0..4
 * are reserved for dedicated streams; alias playback searches slots 5..12,
 * represented by logical channel IDs 37..44 in mss_channelInfo. */
int32_t MSS_FindFreeStreamChannel(int32_t preferredEffectId, sndAliasChannel_t maximumAliasChannel)
{
    for (int32_t streamIndex = MSS_ALIAS_STREAM_SLOT_FIRST; streamIndex < mss_streamChannelCount; ++streamIndex) {
        const int32_t channelIndex = MSS_STREAM_CHANNEL_FIRST + streamIndex;
        if (MSS_IsStreamChannelFree(channelIndex))
            return channelIndex;
    }

    const int32_t channelIndex = MSS_FindReplacableChannel(
        MSS_ALIAS_STREAM_CHANNEL_FIRST, mss_streamChannelCount - MSS_ALIAS_STREAM_SLOT_FIRST, preferredEffectId, maximumAliasChannel);
    if (channelIndex > 0)
        MSS_StopStreamChannel(channelIndex);
    return channelIndex;
}

/* Source: CoDUOMP.exe 0x00451940..0x00451aa7.
 * Name and parameter order: exact same-module Mac symbol
 * MSS_StopEntityChannel. Every channel family uses the same effect/alias
 * identity pair. The free tests preserve completed samples and reclaim
 * completed streams; active or paused matches are stopped explicitly. */
void MSS_StopEntityChannel(int32_t effectId, sndAliasChannel_t aliasChannel)
{
    for (int32_t channelIndex = MSS_3D_CHANNEL_FIRST; channelIndex < mss_max3DChannels; ++channelIndex) {
        const mss_channel_info_t *const channel = &mss_channelInfo[channelIndex];
        if (channel->effectId == effectId && channel->aliasChannel == aliasChannel && !MSS_Is3DChannelFree(channelIndex)) {
            MSS_Stop3DChannel(channelIndex);
        }
    }

    const int32_t streamChannelEnd = MSS_STREAM_CHANNEL_FIRST + mss_streamChannelCount;
    for (int32_t channelIndex = MSS_STREAM_CHANNEL_FIRST; channelIndex < streamChannelEnd; ++channelIndex) {
        const mss_channel_info_t *const channel = &mss_channelInfo[channelIndex];
        if (channel->effectId == effectId && channel->aliasChannel == aliasChannel && !MSS_IsStreamChannelFree(channelIndex)) {
            MSS_StopStreamChannel(channelIndex);
        }
    }

    const int32_t channel2DEnd = MSS_2D_CHANNEL_FIRST + mss_2dChannelCount;
    for (int32_t channelIndex = MSS_2D_CHANNEL_FIRST; channelIndex < channel2DEnd; ++channelIndex) {
        const mss_channel_info_t *const channel = &mss_channelInfo[channelIndex];
        if (channel->effectId == effectId && channel->aliasChannel == aliasChannel && !MSS_Is2DChannelFree(channelIndex)) {
            MSS_Stop2DChannel(channelIndex);
        }
    }
}

/* Source: CoDUOMP.exe 0x00451ab0..0x00451adf.
 * Name and parameter roles: exact same-module Mac symbol MSS_SampleType and
 * the Windows AIL_set_sample_type caller. The return values are the Miles
 * mono/stereo, 8/16-bit, and IMA ADPCM format constants. */
milesSampleType_t MSS_SampleType(int32_t waveFormatTag, int32_t sampleBits, int32_t channelCount)
{
    if (channelCount == MILES_CHANNEL_COUNT_MONO) {
        if (waveFormatTag == MILES_WAVE_FORMAT_IMA_ADPCM)
            return MILES_SAMPLE_TYPE_MONO_IMA_ADPCM;
        return sampleBits > MILES_SAMPLE_BITS_8 ? MILES_SAMPLE_TYPE_MONO_16 : MILES_SAMPLE_TYPE_MONO_8;
    }

    if (waveFormatTag == MILES_WAVE_FORMAT_IMA_ADPCM)
        return MILES_SAMPLE_TYPE_STEREO_IMA_ADPCM;
    return sampleBits > MILES_SAMPLE_BITS_8 ? MILES_SAMPLE_TYPE_STEREO_16 : MILES_SAMPLE_TYPE_STEREO_8;
}

/* Source: CoDUOMP.exe 0x00454f60..0x00455167.
 * Name and source-level structure: exact same-module Mac symbol
 * MSS_LoadSoundFile. Compatible WAV payloads are retained byte-for-byte;
 * files exceeding the active output format are converted through Miles after
 * rate, bit depth, and channel count are reduced exactly as in the Windows
 * instruction flow. */
snd_alias_sound_file_t *MSS_LoadSoundFile(const char *filename)
{
    if (mss_digitalDriver == NULL)
        return NULL;

    const char *const path = va("sound/%s", filename);
    void *fileData;
    const int32_t fileLength = FS_ReadFile(path, &fileData);
    if (fileLength < 0) {
        Com_Printf("^1Sound file '%s' not found\n", path);
        return NULL;
    }

    miles_sound_info_t sourceInfo;
#if defined(_WIN32)
    if (AIL_WAV_info(fileData, &sourceInfo) == MILES_WAV_INFO_INVALID)
        goto invalid_sound_file;
    /* NOT_FROM_ORIGINAL_SOURCE: validate the loaded audio data and playback state before crossing the Miles boundary. */
    if (coduomp_mss_bound_miles_wav_payload(fileData, fileLength, path, &sourceInfo.publicInfo) == qfalse) {
        goto invalid_sound_file;
    }
#else
    /* NOT_FROM_ORIGINAL_SOURCE: validate the loaded audio data and playback state before crossing the Miles boundary. */
    if (coduomp_mss_parse_loaded_wav(fileData, fileLength, path, &sourceInfo) == qfalse) {
        goto invalid_sound_file;
    }
#endif

    snd_alias_sound_file_t *soundFile;
    snd_alias_sound_file_t *const publicInfo = &sourceInfo.publicInfo;
#if SIZE_MAX == UINT32_MAX
    if (publicInfo->dataLength > SIZE_MAX - sizeof(*soundFile) - (MILES_ALLOCATION_SIZE_ALIGNMENT - 1u)) {
        goto invalid_sound_file;
    }
#endif

    if (publicInfo->sampleRate <= (uint32_t)mss_sampleRate &&
        (publicInfo->bitsPerSample <= mss_sampleBits || publicInfo->formatTag == MILES_WAVE_FORMAT_IMA_ADPCM) &&
        publicInfo->channelCount <= mss_channelCount) {
        soundFile = MSS_Alloc(sizeof(*soundFile) + publicInfo->dataLength);
        *soundFile = *publicInfo;
        uint8_t *const payload = (uint8_t *)(soundFile + 1);
        soundFile->data = payload;
        soundFile->initialData = payload;
        Com_Memcpy(payload, publicInfo->data, publicInfo->dataLength);
    } else {
        uint32_t sampleRate = publicInfo->sampleRate;
        uint32_t sampleCount = publicInfo->sampleCount;
        while (sampleRate > (uint32_t)mss_sampleRate) {
            sampleRate >>= 1;
            sampleCount >>= 1;
        }

        int32_t sampleBits = publicInfo->bitsPerSample;
        if (sampleBits > mss_sampleBits)
            sampleBits = mss_sampleBits;
        int32_t channelCount = publicInfo->channelCount;
        if (channelCount > mss_channelCount)
            channelCount = mss_channelCount;

        const milesSampleType_t sampleType = MSS_SampleType(publicInfo->formatTag, sampleBits, channelCount);
        const uint32_t processedSize =
            AIL_size_processed_digital_audio(sampleRate, sampleType, MILES_PROCESS_AUDIO_SINGLE_BUFFER, &sourceInfo);
        /* NOT_FROM_ORIGINAL_SOURCE: validate the loaded audio data and playback state before crossing the Miles boundary. */
#if SIZE_MAX == UINT32_MAX
        if (processedSize > SIZE_MAX - sizeof(*soundFile) - (MILES_ALLOCATION_SIZE_ALIGNMENT - 1u)) {
            goto invalid_sound_file;
        }
#endif
        soundFile = MSS_Alloc(sizeof(*soundFile) + processedSize);
        uint8_t *const payload = (uint8_t *)(soundFile + 1);
        soundFile->formatTag = publicInfo->formatTag;
        soundFile->data = payload;
        soundFile->dataLength = processedSize;
        soundFile->sampleRate = sampleRate;
        soundFile->bitsPerSample = sampleBits;
        soundFile->channelCount = channelCount;
        soundFile->sampleCount = sampleCount;
        soundFile->blockSize = publicInfo->blockSize;
        soundFile->initialData = payload;
        (void)AIL_process_digital_audio(payload, processedSize, sampleRate, sampleType, MILES_PROCESS_AUDIO_SINGLE_BUFFER, &sourceInfo);
    }

    FS_FreeFile(fileData);
    return soundFile;

invalid_sound_file:
    Com_Printf("Sound file '%s' is in an invalid or corrupted format\n", path);
    FS_FreeFile(fileData);
    return NULL;
}

/* Source: CoDUOMP.exe 0x00455170. Name and source interface: exact
 * same-module Mac symbol MSS_UnloadSoundFile. MSS_Free is intentionally a
 * no-op for hunk allocations, so MSVC reduces this body to RET. */
void MSS_UnloadSoundFile(snd_alias_sound_file_t *soundFile)
{
#if (defined(__APPLE__) || defined(__linux__)) && !defined(CODUOMP_DISABLE_AUDIO)
    /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): OpenAL copies loaded
     * aliases into cache-owned buffers. The original unload caller has
     * already stopped every channel, so detach and evict those copies before
     * a map hunk clear makes the payload address reusable. */
    coduomp_openal_forget_loaded_sound(soundFile);
#endif
    MSS_Free(soundFile);
}

/* Source: CoDUOMP.exe 0x00455180..0x00455186.
 * Name: exact same-module Mac symbol MSS_GetSoundFileSize. Native builds use
 * their widened metadata extent; the i386 static assertion retains 0x24. */
int32_t MSS_GetSoundFileSize(const snd_alias_sound_file_t *soundFile)
{
    return (int32_t)(sizeof(*soundFile) + soundFile->dataLength);
}

/* Source: CoDUOMP.exe 0x00451ae0..0x00451ce2.
 * Name and source parameter order: exact same-module Mac symbol
 * MSS_StartAlias2DSample and its Windows callers. Loaded sound metadata is
 * passed directly to the Miles sample API. Playback-rate and fractional-start
 * conversions use FastRound, matching the executable's float store, exact
 * 2^-30 bias, and x87 FISTP. */
qboolean MSS_StartAlias2DSample(int32_t *outChannelIndex, snd_alias_t *alias, snd_alias_t *secondaryAlias, float aliasBlend,
                                int32_t effectId, float volume, float pitch, int32_t timeShift, float startFraction)
{
    const int32_t channelIndex = MSS_FindFree2DChannel(effectId, alias->channel);
    if (outChannelIndex != NULL)
        *outChannelIndex = channelIndex;
    if (channelIndex < 0)
        return qfalse;

    miles_sample_handle_t const sample = mss_2dSampleHandles[channelIndex - MSS_2D_CHANNEL_FIRST];
    const snd_alias_sound_file_t *const soundFile = alias->soundFileInfo;

    AIL_init_sample(sample);
    AIL_set_sample_type(sample, MSS_SampleType(soundFile->formatTag, soundFile->bitsPerSample, soundFile->channelCount), 0);
    AIL_set_sample_address(sample, soundFile->data, soundFile->dataLength);
    AIL_set_sample_adpcm_block_size(sample, soundFile->blockSize);
#if (defined(__APPLE__) || defined(__linux__)) && !defined(CODUOMP_DISABLE_AUDIO)
    /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): the native OpenAL
     * adapter shares one immutable decoded buffer per loaded sound. Supply
     * the base rate before the per-play pitch is applied below. */
    coduomp_openal_bind_loaded_sample(sample, soundFile);
#endif

    const float scaledPlaybackRate = (float)((long double)soundFile->sampleRate * (long double)mss_playbackRateScale * (long double)pitch);
    const int32_t playbackRate = FastRound(scaledPlaybackRate);
    AIL_set_sample_playback_rate(sample, playbackRate);

    const float sampleVolume =
        (float)((long double)mss_effectVolume * (long double)mss_channelVolumes[alias->channel].current * (long double)volume);
    AIL_set_sample_volume_pan(sample, sampleVolume, 0.5f);
    AIL_set_sample_loop_count(sample, alias->loop == 0);
    AIL_set_sample_reverb_levels(sample, 1.0f, mss_reverbLevel);

    int32_t durationMsec;
    AIL_sample_ms_position(sample, &durationMsec, NULL);

    int32_t samplePositionMsec;
    if (startFraction != 0.0f) {
        const float scaledPosition = (float)((long double)durationMsec * (long double)startFraction);
        samplePositionMsec = FastRound(scaledPosition);
    } else {
        samplePositionMsec = timeShift;
    }

    if (samplePositionMsec != 0)
        AIL_set_sample_ms_position(sample, samplePositionMsec);

    if (!mss_paused || alias->channel == SND_ALIAS_CHANNEL_MENU) {
        if (samplePositionMsec != 0)
            AIL_resume_sample(sample);
        else
            AIL_start_sample(sample);
    }

    if (alias->loop != 0)
        durationMsec = 0;
    MSS_SetChannelInfo(channelIndex, effectId, alias, secondaryAlias, aliasBlend, NULL, volume, pitch, (int32_t)soundFile->sampleRate,
                       durationMsec, samplePositionMsec);
    return qtrue;
}

/* Source: CoDUOMP.exe 0x00451cf0..0x0045203f.
 * Name and source parameter order: exact same-module Mac symbol
 * MSS_StartAlias3DSample and its Windows callers. Distance attenuation is
 * performed by the game before Miles receives the volume; Miles gets equal
 * minimum/maximum distances so it does not apply a second rolloff. The stored
 * channel rate remains the sound file's base rate, while the handle receives
 * the pitch- and global-scale-adjusted rate. */
qboolean MSS_StartAlias3DSample(int32_t *outChannelIndex, snd_alias_t *alias, const vec3_t position, snd_alias_t *secondaryAlias,
                                float aliasBlend, int32_t effectId, float volume, float pitch, int32_t timeShift, float startFraction)
{
    const int32_t channelIndex = MSS_FindFree3DChannel(effectId, alias->channel);
    if (outChannelIndex != NULL)
        *outChannelIndex = channelIndex;
    if (channelIndex < 0)
        return qfalse;

    miles_3d_sample_handle_t const sample = mss_3dSampleHandles[channelIndex];
    const snd_alias_sound_file_t *const soundFile = alias->soundFileInfo;
    const float oneMinusBlend = 1.0f - aliasBlend;
    const float minimumDistance = (float)((long double)oneMinusBlend * (long double)alias->distanceMin +
                                          (long double)aliasBlend * (long double)secondaryAlias->distanceMin);
    const float maximumDistance = (float)((long double)oneMinusBlend * (long double)alias->distanceMax +
                                          (long double)aliasBlend * (long double)secondaryAlias->distanceMax);

    AIL_set_3D_sample_info(sample, soundFile);

    vec3_t listenerDelta;
    for (int32_t component = 0; component < 3; ++component) {
        listenerDelta[component] = mss_listenerOrigin[component] - position[component];
    }
    const float distance =
        (float)sqrtl(((long double)listenerDelta[2] * listenerDelta[2] + (long double)listenerDelta[1] * listenerDelta[1]) +
                     (long double)listenerDelta[0] * listenerDelta[0]);

    const float distancePastMinimum = (float)((long double)distance - (long double)minimumDistance);
    float attenuation;
    if (distancePastMinimum <= 0.0f) {
        attenuation = 1.0f;
    } else {
        const float fraction = (float)((long double)distancePastMinimum / ((long double)maximumDistance - (long double)minimumDistance));
        if (fraction >= 1.0f)
            attenuation = 0.0f;
        else
            attenuation = (float)((long double)1.0f - (long double)fraction);
    }
    const float attenuatedVolume =
        (float)((long double)attenuation * (long double)mss_channelVolumes[alias->channel].current * (long double)volume);
    const float sampleVolume = (float)((long double)mss_effectVolume * (long double)attenuatedVolume);
    AIL_set_3D_sample_volume(sample, sampleVolume);
    AIL_set_3D_sample_distances(sample, maximumDistance, maximumDistance);

    const int32_t basePlaybackRate = AIL_3D_sample_playback_rate(sample);
    const float basePlaybackRateFloat = (float)basePlaybackRate;
    const float scaledPlaybackRate = (float)((long double)basePlaybackRateFloat * (long double)mss_playbackRateScale * (long double)pitch);
    const int32_t playbackRate = FastRound(scaledPlaybackRate);
    AIL_set_3D_sample_playback_rate(sample, playbackRate);
    MSS_Set3DPosition(sample, position);
    AIL_set_3D_sample_loop_count(sample, alias->loop == 0);
    AIL_set_3D_sample_effects_level(sample, mss_eaxEnvironmentLoaded ? 1.0f : mss_reverbLevel);

    const uint32_t scaledSampleCount = soundFile->sampleCount * 1000u;
    const float scaledSampleCountFloat = (float)scaledSampleCount;
    const float playbackRateFloat = (float)playbackRate;
    const float scaledDuration =
        (float)((long double)scaledSampleCountFloat * (long double)mss_playbackRateScale / (long double)playbackRateFloat);
    int32_t durationMsec = FastRound(scaledDuration);

    int32_t samplePositionMsec;
    if (startFraction != 0.0f) {
        const float durationMsecFloat = (float)durationMsec;
        const float scaledPosition = (float)((long double)durationMsecFloat * (long double)startFraction);
        samplePositionMsec = FastRound(scaledPosition);
    } else {
        samplePositionMsec = timeShift;
    }

    if (samplePositionMsec != 0) {
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        const float dataLengthFloat = (float)soundFile->dataLength;
        const float scaledByteOffset = (float)((long double)dataLengthFloat * (long double)startFraction);
        AIL_set_3D_sample_offset(sample, FastRound(scaledByteOffset));
        if (!mss_paused)
            AIL_resume_3D_sample(sample);
    } else if (!mss_paused) {
        AIL_start_3D_sample(sample);
    }

    if (alias->loop != 0)
        durationMsec = 0;
    MSS_SetChannelInfo(channelIndex, effectId, alias, secondaryAlias, aliasBlend, position, volume, pitch, (int32_t)soundFile->sampleRate,
                       durationMsec, samplePositionMsec);
    return qtrue;
}

/* Source: CoDUOMP.exe 0x00452040..0x004520c9.
 * Name and source parameter order: exact same-module Mac symbol
 * MSS_StartAliasSample and the Windows calls into the typed 2D/3D starters.
 * The loaded-file failure diagnostic deliberately names the sound file before
 * its alias. Menu and non-spatial channels use a 2D sample; the other alias
 * channels use the supplied world position and a 3D sample. */
qboolean MSS_StartAliasSample(int32_t *outChannelIndex, snd_alias_t *alias, snd_alias_t *secondaryAlias, float aliasBlend, int32_t effectId,
                              const vec3_t position, float volume, float pitch, int32_t timeShift)
{
    if (alias->soundFileInfo == NULL) {
        Com_DPrintf("Tried to play sound '%s' from alias '%s', but it was not "
                    "successfully loaded.\n",
                    alias->soundFile, alias->aliasName);
        if (outChannelIndex != NULL)
            *outChannelIndex = -1;
        return qfalse;
    }

    if (MSS_IsAliasChannel3D(alias->channel)) {
        return MSS_StartAlias3DSample(outChannelIndex, alias, position, secondaryAlias, aliasBlend, effectId, volume, pitch, timeShift,
                                      0.0f);
    }

    return MSS_StartAlias2DSample(outChannelIndex, alias, secondaryAlias, aliasBlend, effectId, volume, pitch, timeShift, 0.0f);
}

/* Source: CoDUOMP.exe 0x004520d0..0x0045236a.
 * Name and source parameter order: exact same-module Mac symbol
 * MSS_StartAliasStreamOnChannel and its Windows callers. Logical stream
 * channels 32..44 map onto the thirteen Miles stream handles. With mss_q3fs
 * enabled, Miles uses the engine file callbacks and receives the qpath;
 * otherwise it receives the filesystem-resolved native path. The returned
 * value is the total non-looping duration, or zero on failure/looping. */
int32_t MSS_StartAliasStreamOnChannel(snd_alias_t *alias, snd_alias_t *secondaryAlias, float aliasBlend, int32_t effectId,
                                      const vec3_t position, float volume, float pitch, int32_t timeShift, float startFraction,
                                      int32_t channelIndex)
{
    if (!alias->streamedFileExists) {
        Com_DPrintf("Tried to play streamed sound '%s' from alias '%s', but it was "
                    "not found at load time.\n",
                    alias->soundFile, alias->aliasName);
        return 0;
    }

    const int32_t streamIndex = channelIndex - MSS_STREAM_CHANNEL_FIRST;
    miles_stream_handle_t *const streamSlot = &mss_streamHandles[streamIndex];
    if (*streamSlot != NULL) {
        AIL_close_stream(*streamSlot);
        *streamSlot = NULL;
    }

    const char *const qpath = va("sound/%s", alias->soundFile);
    const char *const streamPath = mss_q3fs->integer ? qpath : FS_ShortOSFilePath(qpath);
    miles_stream_handle_t const stream = AIL_open_stream(mss_digitalDriver, streamPath, 0);
    if (stream == NULL) {
        Com_Printf("Couldn't play stream '%s' from alias '%s' - %s\n", qpath, alias->aliasName, AIL_last_error());
        return 0;
    }
    *streamSlot = stream;

    const int32_t basePlaybackRate = AIL_stream_playback_rate(stream);
    const float scaledPlaybackRate = (float)((long double)basePlaybackRate * (long double)mss_playbackRateScale * (long double)pitch);
    AIL_set_stream_playback_rate(stream, FastRound(scaledPlaybackRate));

    const float initialVolume =
        (float)((long double)mss_effectVolume * (long double)mss_channelVolumes[alias->channel].current * (long double)volume);
    AIL_set_stream_volume_pan(stream, initialVolume, 0.5f);
    AIL_set_stream_loop_count(stream, alias->loop == 0);
    AIL_set_stream_reverb_levels(stream, 1.0f, mss_reverbLevel);

    int32_t durationMsec;
    AIL_stream_ms_position(stream, &durationMsec, NULL);

    int32_t streamPositionMsec;
    if (startFraction != 0.0f) {
        const float scaledPosition = (float)((long double)durationMsec * (long double)startFraction);
        streamPositionMsec = FastRound(scaledPosition);
    } else {
        streamPositionMsec = timeShift;
    }

    if (streamPositionMsec != 0) {
        AIL_set_stream_ms_position(stream, streamPositionMsec);
        if (!mss_paused || alias->channel == SND_ALIAS_CHANNEL_MENU)
            AIL_pause_stream(stream, qfalse);
    } else if (!mss_paused || alias->channel == SND_ALIAS_CHANNEL_MENU) {
        AIL_start_stream(stream);
    }

    if (alias->loop != 0)
        durationMsec = 0;

    mss_stream_channel_t *const streamChannel = &mss_streamChannels[streamIndex];
    streamChannel->is3D = MSS_IsAliasChannel3D(alias->channel);
    for (int32_t component = 0; component < 3; ++component)
        streamChannel->position[component] = position[component];

    MSS_SetChannelInfo(channelIndex, effectId, alias, secondaryAlias, aliasBlend, position, volume, pitch, basePlaybackRate, durationMsec,
                       streamPositionMsec);

    if (streamChannel->is3D) {
        float pan;
        MSS_SpatializeStream(streamIndex, &volume, &pan);
        const float spatializedVolume = (float)((long double)mss_effectVolume * (long double)volume);
        AIL_set_stream_volume_pan(stream, spatializedVolume, pan);
    }

    return durationMsec;
}

/* Source: CoDUOMP.exe 0x00452370..0x004523b9.
 * Name and parameter order: exact same-module Mac symbol
 * MSS_StartAliasStream. Alias streams use the replaceable stream-slot range;
 * the optional result receives the logical channel even when allocation
 * fails, matching the loaded-sample starters. */
int32_t MSS_StartAliasStream(int32_t *outChannelIndex, snd_alias_t *alias, snd_alias_t *secondaryAlias, float aliasBlend, int32_t effectId,
                             const vec3_t position, float volume, float pitch, int32_t timeShift, float startFraction)
{
    const int32_t channelIndex = MSS_FindFreeStreamChannel(effectId, alias->channel);
    if (outChannelIndex != NULL)
        *outChannelIndex = channelIndex;
    if (channelIndex < 0)
        return 0;

    return MSS_StartAliasStreamOnChannel(alias, secondaryAlias, aliasBlend, effectId, position, volume, pitch, timeShift, startFraction,
                                         channelIndex);
}

/* Source: CoDUOMP.exe 0x004523c0..0x004527ba.
 * Name and parameter order: exact same-module Mac symbol
 * MSS_ContinueLoopingSound and its Windows caller. The Mac call graph also
 * proves the three family-specific free-channel helpers that MSVC inlined in
 * this body. Alias names are compared by pointer exactly as in the executable;
 * the alias table interns these strings. */
qboolean MSS_ContinueLoopingSound(snd_alias_t *alias, snd_alias_t *secondaryAlias, float aliasBlend, float pitchScale, int32_t effectId,
                                  const vec3_t position, int32_t *outChannelIndex)
{
    if (pitchScale == 0.0f)
        pitchScale = 1.0f;

    for (int32_t channelIndex = MSS_3D_CHANNEL_FIRST; channelIndex < mss_max3DChannels; ++channelIndex) {
        mss_channel_info_t *const channel = &mss_channelInfo[channelIndex];
        if (channel->effectId != effectId || MSS_Is3DChannelFree(channelIndex) || channel->alias->loop == 0 ||
            channel->alias->aliasName != alias->aliasName || channel->secondaryAlias->aliasName != secondaryAlias->aliasName) {
            continue;
        }

        const float oneMinusBlend = (float)((long double)1.0f - (long double)aliasBlend);
        channel->logicalVolume = (float)(((long double)oneMinusBlend * (long double)alias->volumeMin +
                                          (long double)aliasBlend * (long double)secondaryAlias->volumeMin) *
                                         (long double)MSS_ALIAS_VOLUME_SCALE);
        if ((alias->pitchMin != 1.0f || secondaryAlias->pitchMin != 1.0f) && alias->pitchMin == alias->pitchMax &&
            secondaryAlias->pitchMin == secondaryAlias->pitchMax) {
            channel->aliasPitchScale = (float)((long double)oneMinusBlend * (long double)alias->pitchMin +
                                               (long double)aliasBlend * (long double)secondaryAlias->pitchMin);
        }

        const float scaledPlaybackRate = (float)((long double)channel->basePlaybackRate * (long double)channel->aliasPitchScale *
                                                 (long double)mss_playbackRateScale * (long double)pitchScale);
        AIL_set_3D_sample_playback_rate(mss_3dSampleHandles[channelIndex], FastRound(scaledPlaybackRate));
        MSS_Set3DPosition(mss_3dSampleHandles[channelIndex], position);
        channel->lastUpdateTime = mss_lastSoundTime;
        channel->aliasBlend = aliasBlend;
        if (outChannelIndex != NULL)
            *outChannelIndex = channelIndex;
        return qtrue;
    }

    const int32_t channel2DEnd = MSS_2D_CHANNEL_FIRST + mss_2dChannelCount;
    for (int32_t channelIndex = MSS_2D_CHANNEL_FIRST; channelIndex < channel2DEnd; ++channelIndex) {
        mss_channel_info_t *const channel = &mss_channelInfo[channelIndex];
        if (channel->effectId != effectId || MSS_Is2DChannelFree(channelIndex) || channel->alias->loop == 0 ||
            channel->alias->aliasName != alias->aliasName || channel->secondaryAlias->aliasName != secondaryAlias->aliasName) {
            continue;
        }

        const float oneMinusBlend = (float)((long double)1.0f - (long double)aliasBlend);
        channel->logicalVolume = (float)(((long double)oneMinusBlend * (long double)alias->volumeMin +
                                          (long double)aliasBlend * (long double)secondaryAlias->volumeMin) *
                                         (long double)MSS_ALIAS_VOLUME_SCALE);

        const float scaledPlaybackRate = (float)((long double)channel->basePlaybackRate * (long double)channel->aliasPitchScale *
                                                 (long double)mss_playbackRateScale * (long double)pitchScale);
        AIL_set_sample_playback_rate(mss_2dSampleHandles[channelIndex - MSS_2D_CHANNEL_FIRST], FastRound(scaledPlaybackRate));
        channel->lastUpdateTime = mss_lastSoundTime;
        channel->aliasBlend = aliasBlend;
        if (outChannelIndex != NULL)
            *outChannelIndex = channelIndex;
        return qtrue;
    }

    const int32_t streamChannelEnd = MSS_STREAM_CHANNEL_FIRST + mss_streamChannelCount;
    for (int32_t channelIndex = MSS_STREAM_CHANNEL_FIRST; channelIndex < streamChannelEnd; ++channelIndex) {
        mss_channel_info_t *const channel = &mss_channelInfo[channelIndex];
        if (channel->effectId != effectId || MSS_IsStreamChannelFree(channelIndex) || channel->alias->loop == 0 ||
            channel->alias->aliasName != alias->aliasName || channel->secondaryAlias->aliasName != secondaryAlias->aliasName) {
            continue;
        }

        const float oneMinusBlend = (float)((long double)1.0f - (long double)aliasBlend);
        channel->logicalVolume = (float)(((long double)oneMinusBlend * (long double)alias->volumeMin +
                                          (long double)aliasBlend * (long double)secondaryAlias->volumeMin) *
                                         (long double)MSS_ALIAS_VOLUME_SCALE);
        const float scaledPlaybackRate = (float)((long double)channel->basePlaybackRate * (long double)channel->aliasPitchScale *
                                                 (long double)mss_playbackRateScale * (long double)pitchScale);
        const int32_t streamIndex = channelIndex - MSS_STREAM_CHANNEL_FIRST;
        AIL_set_stream_playback_rate(mss_streamHandles[streamIndex], FastRound(scaledPlaybackRate));
        for (int32_t component = 0; component < 3; ++component) {
            mss_streamChannels[streamIndex].position[component] = position[component];
        }
        channel->lastUpdateTime = mss_lastSoundTime;
        channel->aliasBlend = aliasBlend;
        if (outChannelIndex != NULL)
            *outChannelIndex = channelIndex;
        return qtrue;
    }

    return qfalse;
}

/* Source: CoDUOMP.exe 0x004527c0..0x00452867.
 * Name and parameter order: exact same-module Mac symbol
 * MSS_ChoosePitchAndVolume and the Windows caller. Blend interpolation is
 * rounded to float before each independent MSVC-rand selection. Volume uses
 * the original 0.8 output scale; pitch does not. */
void MSS_ChoosePitchAndVolume(snd_alias_t *alias, snd_alias_t *secondaryAlias, float aliasBlend, float *outVolume, float *outPitch)
{
    const long double oneMinusBlend = (long double)1.0f - (long double)aliasBlend;
    const float volumeMin =
        (float)(oneMinusBlend * (long double)alias->volumeMin + (long double)aliasBlend * (long double)secondaryAlias->volumeMin);
    const float volumeMax =
        (float)(oneMinusBlend * (long double)alias->volumeMax + (long double)aliasBlend * (long double)secondaryAlias->volumeMax);
    const float pitchMin =
        (float)(oneMinusBlend * (long double)alias->pitchMin + (long double)aliasBlend * (long double)secondaryAlias->pitchMin);
    const float pitchMax =
        (float)(oneMinusBlend * (long double)alias->pitchMax + (long double)aliasBlend * (long double)secondaryAlias->pitchMax);

    const long double volumeRandom = (long double)coduo_crt_rand() * (long double)MSS_RANDOM_UNIT;
    *outVolume = (float)(((long double)volumeMin + volumeRandom * ((long double)volumeMax - (long double)volumeMin)) *
                         (long double)MSS_ALIAS_VOLUME_SCALE);

    const long double pitchRandom = (long double)coduo_crt_rand() * (long double)MSS_RANDOM_UNIT;
    *outPitch = (float)((long double)pitchMin + pitchRandom * ((long double)pitchMax - (long double)pitchMin));
}

/* Source: CoDUOMP.exe 0x00452870..0x004529ec.
 * Name and source parameter order: exact same-module Mac symbol
 * MSS_PlaySoundAlias_Internal. The Mac body preserves the source calls to
 * MSS_IsAliasChannel3D and VectorDistanceSquared that MSVC inlined. A loop
 * that was successfully continued returns zero; only a newly started loaded
 * sample or stream supplies the function result. */
int32_t MSS_PlaySoundAlias_Internal(snd_alias_t *alias, snd_alias_t *secondaryAlias, float aliasBlend, float pitchScale, int32_t effectId,
                                    const vec3_t position, int32_t *outChannelIndex, int32_t timeShift)
{
    if (mss_digitalDriver == NULL)
        return 0;

    if (outChannelIndex != NULL)
        *outChannelIndex = -1;

    if (MSS_IsAliasChannel3D(alias->channel)) {
        const long double oneMinusBlend = (long double)1.0f - (long double)aliasBlend;
        const float maximumDistance =
            (float)(oneMinusBlend * (long double)alias->distanceMax + (long double)aliasBlend * (long double)secondaryAlias->distanceMax);
        if (VectorDistanceSquared(position, mss_listenerOrigin) > (long double)maximumDistance * (long double)maximumDistance) {
            return 0;
        }
    }

    if (MSS_ContinueLoopingSound(alias, secondaryAlias, aliasBlend, pitchScale, effectId, position, outChannelIndex)) {
        return 0;
    }

    if (alias->channel != SND_ALIAS_CHANNEL_AUTO)
        MSS_StopEntityChannel(effectId, alias->channel);

    float volume;
    float pitch;
    MSS_ChoosePitchAndVolume(alias, secondaryAlias, aliasBlend, &volume, &pitch);
    if (pitchScale != 0.0f)
        pitch = (float)((long double)pitch * (long double)pitchScale);

    if (alias->type == SND_ALIAS_TYPE_LOADED) {
        return MSS_StartAliasSample(outChannelIndex, alias, secondaryAlias, aliasBlend, effectId, position, volume, pitch, timeShift);
    }
    if (alias->type == SND_ALIAS_TYPE_STREAMED) {
        return MSS_StartAliasStream(outChannelIndex, alias, secondaryAlias, aliasBlend, effectId, position, volume, pitch, timeShift, 0.0f);
    }
    return 0;
}

/* Source: CoDUOMP.exe 0x004529f0..0x00452a13.
 * Name and signature: exact same-module Mac symbol MSS_PlaySoundAlias. The
 * unblended public entry supplies the same alias as both blend endpoints and
 * does not request the allocated channel index. */
int32_t MSS_PlaySoundAlias(snd_alias_t *alias, int32_t effectId, const vec3_t position, int32_t timeShift)
{
    if (alias == NULL)
        return 0;
    return MSS_PlaySoundAlias_Internal(alias, alias, 0.0f, 0.0f, effectId, position, NULL, timeShift);
}

/* Source: CoDUOMP.exe 0x00452a20..0x00452b99.
 * Name and signature: exact same-module Mac symbol
 * MSS_ValidateSoundAliasBlend. Alias string and sound-file identity are
 * pointer comparisons in both original builds. Looping blends additionally
 * require fixed pitch and volume on both endpoints because repeated random
 * selection would not remain synchronized. */
qboolean MSS_ValidateSoundAliasBlend(const snd_alias_t *alias, const snd_alias_t *secondaryAlias, qboolean reportError)
{
    if (alias == secondaryAlias)
        return qtrue;

    if (alias->soundFile != secondaryAlias->soundFile) {
        if (reportError) {
            Com_Error(ERR_DROP,
                      "\x15"
                      "tried to blend between sound aliases '%s' and '%s', "
                      "but they don't use the same sound file\n",
                      alias->aliasName, secondaryAlias->aliasName);
        }
        return qfalse;
    }
    if (alias->loop != secondaryAlias->loop) {
        if (reportError) {
            Com_Error(ERR_DROP,
                      "\x15"
                      "tried to blend between sound aliases '%s' and '%s', "
                      "but they do not have the same looping status\n",
                      alias->aliasName, secondaryAlias->aliasName);
        }
        return qfalse;
    }
    if (alias->type != secondaryAlias->type) {
        if (reportError) {
            Com_Error(ERR_DROP,
                      "\x15"
                      "tried to blend between sound aliases '%s' and '%s', "
                      "but they are not both loaded or both streamed\n",
                      alias->aliasName, secondaryAlias->aliasName);
        }
        return qfalse;
    }
    if (alias->channel != secondaryAlias->channel) {
        if (reportError) {
            Com_Error(ERR_DROP,
                      "\x15"
                      "tried to blend between sound aliases '%s' and '%s', "
                      "but they don't use the same channel\n",
                      alias->aliasName, secondaryAlias->aliasName);
        }
        return qfalse;
    }
    if (alias->isMaster != secondaryAlias->isMaster) {
        if (reportError) {
            Com_Error(ERR_DROP,
                      "\x15"
                      "tried to blend between sound aliases '%s' and '%s', "
                      "but only one of them is a 'master' alias\n",
                      alias->aliasName, secondaryAlias->aliasName);
        }
        return qfalse;
    }
    if (alias->isSlave != secondaryAlias->isSlave) {
        if (reportError) {
            Com_Error(ERR_DROP,
                      "\x15"
                      "tried to blend between sound aliases '%s' and '%s', "
                      "but only one of them is a 'slave' alias\n",
                      alias->aliasName, secondaryAlias->aliasName);
        }
        return qfalse;
    }

    if (alias->loop != 0 && (alias->pitchMin != alias->pitchMax || secondaryAlias->pitchMin != secondaryAlias->pitchMax)) {
        if (reportError) {
            Com_Error(ERR_DROP,
                      "\x15"
                      "tried to blend between sound aliases '%s' and '%s', "
                      "but they are looping and at least one of them has a random "
                      "pitch\n",
                      alias->aliasName, secondaryAlias->aliasName);
        }
        return qfalse;
    }
    if (alias->loop != 0 && (alias->volumeMin != alias->volumeMax || secondaryAlias->volumeMin != secondaryAlias->volumeMax)) {
        if (reportError) {
            Com_Error(ERR_DROP,
                      "\x15"
                      "tried to blend between sound aliases '%s' and '%s', "
                      "but they are looping and at least one of them has a random "
                      "volume\n",
                      alias->aliasName, secondaryAlias->aliasName);
        }
        return qfalse;
    }
    return qtrue;
}

/* Source: CoDUOMP.exe 0x00452ba0..0x00452c52.
 * Name and signature: exact same-module Mac symbol
 * MSS_PlayBlendedSoundAliases. A negative blend is a packed value: its
 * absolute fractional part remains the alias blend, while its absolute whole
 * part is a pitch percentage. Both original builds call floor separately for
 * the two decoded values. */
int32_t MSS_PlayBlendedSoundAliases(snd_alias_t *alias, snd_alias_t *secondaryAlias, float aliasBlend, int32_t effectId,
                                    const vec3_t position, int32_t timeShift)
{
    if (alias == NULL || secondaryAlias == NULL)
        return 0;

    (void)MSS_ValidateSoundAliasBlend(alias, secondaryAlias, qtrue);

    float pitchScale;
    if (aliasBlend < 0.0f) {
        const float absoluteBlend = fabsf(aliasBlend);
        pitchScale = (float)(floor((double)absoluteBlend) * (double)MSS_ENCODED_PITCH_PERCENT_SCALE);
        aliasBlend = (float)((double)absoluteBlend - floor((double)absoluteBlend));
    } else {
        pitchScale = 1.0f;
    }

    return MSS_PlaySoundAlias_Internal(alias, secondaryAlias, aliasBlend, pitchScale, effectId, position, NULL, timeShift);
}

/* Source: CoDUOMP.exe 0x00452c60..0x00452c99.
 * Name and signature: exact same-module Mac symbol MSS_PlayLocalSoundAlias.
 * Alias selection is position-independent at vec3_origin; playback itself is
 * attached to the current listener origin and uses listener time as its
 * unique effect identifier. */
int32_t MSS_PlayLocalSoundAlias(const char *name, sndAliasBank_t bank)
{
    if (name == NULL)
        return 0;

    snd_alias_t *const alias = Com_PickSoundAlias(name, bank, vec3_origin);
    if (alias == NULL)
        return 0;

    return MSS_PlaySoundAlias_Internal(alias, alias, 0.0f, 0.0f, mss_listenerTime, mss_listenerOrigin, NULL, 0);
}

/* Source: CoDUOMP.exe 0x00452ca0..0x00452e2a.
 * Name and signature: exact same-module Mac symbol MSS_StartBackground.
 * Background index zero is music; indices one and two are the alternating
 * ambient slots. Their logical indices map directly to stream channels
 * 32..34. The fade record retains the selected target volume while the
 * channel starts from silence when a positive fade time is requested. */
void MSS_StartBackground(int32_t backgroundIndex, snd_alias_t *alias, int32_t fadeTimeMsec)
{
    MSS_UpdatePause();

    if (MSS_IsAliasChannel3D(alias->channel)) {
        Com_Error(ERR_DROP,
                  "\x15"
                  "alias %s sound %s played as an ambient / music track "
                  "uses a 3D channel type; should probably be channel 'local'\n",
                  alias->aliasName, alias->soundFile);
    }
    if (alias->type != SND_ALIAS_TYPE_STREAMED) {
        Com_Error(ERR_DROP,
                  "\x15"
                  "alias %s sound %s played as an ambient / music track "
                  "is not streamed; type must be 'streamed'\n",
                  alias->aliasName, alias->soundFile);
    }

    const long double volumeRandom = (long double)coduo_crt_rand() * (long double)MSS_RANDOM_UNIT;
    const float volume =
        (float)(((long double)alias->volumeMin + volumeRandom * ((long double)alias->volumeMax - (long double)alias->volumeMin)) *
                (long double)MSS_ALIAS_VOLUME_SCALE);
    const long double pitchRandom = (long double)coduo_crt_rand() * (long double)MSS_RANDOM_UNIT;
    const float pitch = (float)((long double)alias->pitchMin + pitchRandom * ((long double)alias->pitchMax - (long double)alias->pitchMin));

    const int32_t channelIndex = MSS_STREAM_CHANNEL_FIRST + backgroundIndex;
    if (!MSS_IsStreamChannelFree(channelIndex))
        MSS_StopStreamChannel(channelIndex);

    (void)MSS_StartAliasStreamOnChannel(alias, alias, 0.0f, MSS_EFFECT_ID_LIMIT, mss_listenerOrigin, volume, pitch, 0, 0.0f, channelIndex);

    mss_background_fade_t *const fade = &mss_backgroundFades[backgroundIndex];
    mss_channel_info_t *const channel = &mss_channelInfo[channelIndex];
    fade->targetVolume = channel->logicalVolume;
    if (fadeTimeMsec > 0) {
        fade->ratePerMsec = (float)((long double)channel->logicalVolume / (long double)fadeTimeMsec);
        channel->logicalVolume = 0.0f;
    } else {
        fade->ratePerMsec = 0.0f;
    }
}

/* Source: CoDUOMP.exe 0x00452e30..0x00452eda.
 * Name and signature: exact same-module Mac symbol MSS_StopBackground.
 * An immediate stop releases the stream. A timed stop instead changes the
 * per-background fade target to silence and records a negative rate; the
 * ordinary sound update owns the eventual stream release. */
void MSS_StopBackground(int32_t backgroundIndex, int32_t fadeTimeMsec)
{
    const int32_t channelIndex = MSS_STREAM_CHANNEL_FIRST + backgroundIndex;
    if (MSS_IsStreamChannelFree(channelIndex))
        return;

    if (fadeTimeMsec == 0) {
        MSS_StopStreamChannel(channelIndex);
        return;
    }

    mss_background_fade_t *const fade = &mss_backgroundFades[backgroundIndex];
    if (fade->targetVolume > 0.0f) {
        fade->ratePerMsec = (float)(-(long double)fade->targetVolume / (long double)fadeTimeMsec);
        fade->targetVolume = 0.0f;
    }
}

/* Source: CoDUOMP.exe 0x00452ee0..0x00452f0b.
 * Name and signature: exact same-module Mac symbol MSS_PlayMusicAlias. Music
 * owns background slot zero and starts only while its stream channel is free. */
void MSS_PlayMusicAlias(snd_alias_t *alias)
{
    if (mss_digitalDriver == NULL || alias == NULL)
        return;
    if (MSS_IsStreamChannelFree(MSS_STREAM_CHANNEL_FIRST + MSS_MUSIC_BACKGROUND_INDEX)) {
        MSS_StartBackground(MSS_MUSIC_BACKGROUND_INDEX, alias, MSS_BACKGROUND_FADE_IMMEDIATE_MSEC);
    }
}

/* Source: CoDUOMP.exe 0x00452f10..0x00452f1d.
 * Name and signature: exact same-module Mac symbol MSS_StopMusic. */
void MSS_StopMusic(int32_t fadeTimeMsec)
{
    MSS_StopBackground(MSS_MUSIC_BACKGROUND_INDEX, fadeTimeMsec);
}

/* Source: CoDUOMP.exe 0x00452f20..0x0045302f.
 * Name and signature: exact same-module Mac symbol MSS_PlayAmbientAlias.
 * Compatible aliases can reuse the active stream: only the channel's alias
 * pointers and target volume change. A different sound, loop mode, pitch
 * range, or alias channel crossfades through the other ambient background
 * slot instead. Alias names and sound-file names are interned pointers and
 * retain the executable's pointer-identity comparisons. */
void MSS_PlayAmbientAlias(snd_alias_t *alias, int32_t fadeTimeMsec)
{
    if (mss_digitalDriver == NULL || alias == NULL)
        return;

    const int32_t currentIndex = mss_ambientBackgroundIndex;
    const int32_t channelIndex = MSS_STREAM_CHANNEL_FIRST + currentIndex;
    if (!MSS_IsStreamChannelFree(channelIndex)) {
        mss_channel_info_t *const channel = &mss_channelInfo[channelIndex];
        snd_alias_t *const currentAlias = channel->alias;

        if (currentAlias->aliasName == alias->aliasName)
            return;

        if (currentAlias->soundFile == alias->soundFile && currentAlias->loop == alias->loop && currentAlias->pitchMin == alias->pitchMin &&
            currentAlias->pitchMax == alias->pitchMax && currentAlias->channel == alias->channel) {
            channel->alias = alias;
            channel->secondaryAlias = alias;

            const long double volumeRandom = (long double)coduo_crt_rand() * (long double)MSS_RANDOM_UNIT;
            const float targetVolume =
                (float)(((long double)alias->volumeMin + volumeRandom * ((long double)alias->volumeMax - (long double)alias->volumeMin)) *
                        (long double)MSS_ALIAS_VOLUME_SCALE);
            mss_background_fade_t *const fade = &mss_backgroundFades[currentIndex];
            if (fadeTimeMsec != 0) {
                fade->ratePerMsec = (float)(((long double)targetVolume - (long double)fade->targetVolume) / (long double)fadeTimeMsec);
            } else {
                fade->ratePerMsec = 0.0f;
            }
            fade->targetVolume = targetVolume;
            return;
        }
    }

    MSS_StopBackground(currentIndex, fadeTimeMsec);
    mss_ambientBackgroundIndex = MSS_AMBIENT_BACKGROUND_FIRST_INDEX + MSS_AMBIENT_BACKGROUND_SECOND_INDEX - currentIndex;
    MSS_StartBackground(mss_ambientBackgroundIndex, alias, fadeTimeMsec);
}

/* Source: CoDUOMP.exe 0x00453030..0x00453196.
 * Name and signature: exact same-module Mac symbol MSS_BeginRawSamples.
 * Raw PCM uses one Miles 2D sample and a 32-segment producer/consumer ring.
 * The explicit eight-byte alignment on the timing field reproduces MSVC's
 * i386 double alignment without a source-level padding member and remains
 * natural on 64-bit targets. */
void MSS_BeginRawSamples(int32_t sampleRate, int32_t sampleWidthBytes, int32_t channelCount)
{
    mss_raw_sample_state_t *const raw = &mss_rawSampleState;
    raw->sample = AIL_allocate_sample_handle(mss_digitalDriver);
    if (raw->sample == NULL) {
        Com_Error(ERR_DROP, "\x15"
                            "MILES 2D sound sample allocation failed on raw channel\n");
    }

    raw->sampleRate = sampleRate;
    raw->sampleWidthBytes = sampleWidthBytes;
    raw->channelCount = channelCount;
    AIL_init_sample(raw->sample);

    milesSampleType_t sampleType;
    if (channelCount == MILES_CHANNEL_COUNT_MONO) {
        sampleType = sampleWidthBytes == 1 ? MILES_SAMPLE_TYPE_MONO_8 : MILES_SAMPLE_TYPE_MONO_16;
    } else {
        sampleType = sampleWidthBytes == 1 ? MILES_SAMPLE_TYPE_STEREO_8 : MILES_SAMPLE_TYPE_STEREO_16;
    }
    AIL_set_sample_type(raw->sample, sampleType, 0);
    AIL_set_sample_playback_rate(raw->sample, sampleRate);
    AIL_set_sample_volume_pan(raw->sample, mss_effectVolume, 0.5f);

    raw->segmentSize = MSS_RAW_MINIMUM_BUFFER_SIZE;
    const int32_t milesMinimum = AIL_minimum_sample_buffer_size(mss_digitalDriver, sampleRate, sampleType);
    if (milesMinimum > raw->segmentSize)
        raw->segmentSize = milesMinimum;

    raw->ringBuffer = Z_MallocInternal((size_t)raw->segmentSize * MSS_RAW_BUFFER_COUNT);
    memset(raw->segmentReady, 0, sizeof(raw->segmentReady));
    raw->writeSegmentOffset = 0;
    raw->readSegmentIndex = 0;
    raw->writeSegmentIndex = 0;
    raw->sampleTimeBaseMsec = 0.0;
    raw->sampleTimePerByteMsec = 1000.0 / (double)(sampleRate * sampleWidthBytes * channelCount);
}

/* Source: CoDUOMP.exe 0x004531a0..0x004531d3.
 * Name and signature: exact same-module Mac symbol MSS_EndRawSamples. */
void MSS_EndRawSamples(void)
{
    mss_raw_sample_state_t *const raw = &mss_rawSampleState;
    if (raw->sample == NULL)
        return;

    AIL_end_sample(raw->sample);
    AIL_release_sample_handle(raw->sample);
    raw->sample = NULL;
    Z_FreeInternal(raw->ringBuffer);
}

/* Source: CoDUOMP.exe 0x004531e0..0x00453216.
 * Name and signature: exact same-module Mac symbol MSS_RawSamplesTime.
 * Miles' byte position is unsigned; the Windows x87 conversion explicitly
 * adds 2^32 when its high bit is set before applying the millisecond scale. */
int32_t MSS_RawSamplesTime(void)
{
    const mss_raw_sample_state_t *const raw = &mss_rawSampleState;
    if (raw->sample == NULL)
        return 0;

    const uint32_t samplePosition = AIL_sample_position(raw->sample);
    return coduo_fp_to_i32_extended((long double)raw->sampleTimeBaseMsec +
                                    (long double)samplePosition * (long double)raw->sampleTimePerByteMsec);
}

/* Source: CoDUOMP.exe 0x00453220..0x004532c2.
 * Name and signature: exact same-module Mac symbol MSS_UpdateRawSamples.
 * A ready producer segment is submitted only when Miles exposes one of its
 * internal sample buffers. The accumulated time base advances by exactly one
 * engine ring segment per successful submission. */
void MSS_UpdateRawSamples(void)
{
    mss_raw_sample_state_t *const raw = &mss_rawSampleState;
    if (raw->sample == NULL)
        return;

    AIL_set_sample_volume_pan(raw->sample, mss_effectVolume, 0.5f);
    if (!raw->segmentReady[raw->readSegmentIndex])
        return;

    const int32_t milesBufferIndex = AIL_sample_buffer_ready(raw->sample);
    if (milesBufferIndex == -1)
        return;

    raw->sampleTimeBaseMsec += (double)raw->segmentSize * raw->sampleTimePerByteMsec;
    AIL_load_sample_buffer(raw->sample, milesBufferIndex, raw->ringBuffer + raw->readSegmentIndex * raw->segmentSize, raw->segmentSize);
    raw->segmentReady[raw->readSegmentIndex] = 0;
    raw->readSegmentIndex = (raw->readSegmentIndex + 1) % MSS_RAW_BUFFER_COUNT;
}

/* Source: CoDUOMP.exe 0x004532d0..0x004533c8.
 * Name and signature: exact same-module Mac symbol MSS_RawSamples. The input
 * is a genuinely untyped PCM byte stream because sampleWidthBytes selects its
 * element width. Producer writes block until the 32-segment ring exposes a
 * free segment, driving the ordinary Miles update exactly as the executable
 * does while waiting. */
void MSS_RawSamples(int32_t sampleFrameCount, int32_t sampleRate, int32_t sampleWidthBytes, int32_t channelCount, const void *sampleData)
{
    if (mss_digitalDriver == NULL)
        return;

    mss_raw_sample_state_t *const raw = &mss_rawSampleState;
    if (raw->sample == NULL) {
        MSS_BeginRawSamples(sampleRate, sampleWidthBytes, channelCount);
        if (raw->sample == NULL)
            return;
    }

    int32_t bytesRemaining = (int32_t)((uint32_t)sampleFrameCount * (uint32_t)sampleWidthBytes * (uint32_t)channelCount);
    const uint8_t *source = sampleData;
    while (bytesRemaining != 0) {
        while (raw->segmentReady[raw->writeSegmentIndex])
            MSS_Update();

        int32_t copyBytes = raw->segmentSize - raw->writeSegmentOffset;
        if (copyBytes > bytesRemaining)
            copyBytes = bytesRemaining;
        Com_Memcpy(raw->ringBuffer + raw->writeSegmentIndex * raw->segmentSize + raw->writeSegmentOffset, source, (size_t)copyBytes);
        raw->writeSegmentOffset += copyBytes;
        source += copyBytes;
        bytesRemaining -= copyBytes;

        if (raw->writeSegmentOffset == raw->segmentSize) {
            raw->writeSegmentOffset = 0;
            raw->segmentReady[raw->writeSegmentIndex] = 1;
            raw->writeSegmentIndex = (raw->writeSegmentIndex + 1) % MSS_RAW_BUFFER_COUNT;
        }
    }
}

/* Source: CoDUOMP.exe 0x004533d0..0x0045341d.
 * Name and signature: exact same-module Mac symbol MSS_FadeAllSounds. A
 * zero-duration fade retains the target/current delta as the update rate;
 * when the target is exactly silent it also stops all active sounds. */
void MSS_FadeAllSounds(float targetVolume, int32_t durationMsec)
{
    mss_masterVolume.target = targetVolume;
    mss_masterVolume.ratePerMsec = targetVolume - mss_masterVolume.current;
    if (durationMsec != 0) {
        mss_masterVolume.ratePerMsec = (float)((long double)mss_masterVolume.ratePerMsec / (long double)durationMsec);
    } else if (targetVolume == 0.0f) {
        MSS_StopSounds(MSS_STOP_ALL_SOUNDS);
    }
}

/* Source: CoDUOMP.exe 0x00453420..0x004535da.
 * Name and signature: exact same-module Mac symbol MSS_FadeSelectSounds.
 * MSVC unrolled all ten alias-channel updates; the Mac body retains the
 * source loop over the same SND_ALIAS_CHANNEL_COUNT records. */
void MSS_FadeSelectSounds(const float targetVolumes[SND_ALIAS_CHANNEL_COUNT], int32_t durationMsec)
{
    for (int32_t channel = 0; channel < SND_ALIAS_CHANNEL_COUNT; ++channel) {
        mss_channel_volume_t *const volume = &mss_channelVolumes[channel];
        volume->target = targetVolumes[channel];
        volume->ratePerMsec = volume->target - volume->current;
        if (durationMsec != 0) {
            volume->ratePerMsec = (float)((long double)volume->ratePerMsec / (long double)durationMsec);
        }
    }
}

/* Source: CoDUOMP.exe 0x004535e0..0x00453758.
 * Name and source-level helper structure: exact same-module Mac symbol
 * MSS_PauseSounds. Menu-channel 2D samples and streams deliberately continue
 * while the game is paused; 3D channels have no corresponding menu exception. */
void MSS_PauseSounds(void)
{
    if (mss_digitalDriver == NULL || mss_paused)
        return;

    const int32_t first2DChannel = MSS_2D_CHANNEL_FIRST;
    const int32_t end2DChannel = first2DChannel + mss_2dChannelCount;
    for (int32_t channelIndex = first2DChannel; channelIndex < end2DChannel; ++channelIndex) {
        if (!MSS_Is2DChannelFree(channelIndex) && mss_channelInfo[channelIndex].alias->channel != SND_ALIAS_CHANNEL_MENU) {
            MSS_Pause2DChannel(channelIndex);
        }
    }

    for (int32_t channelIndex = 0; channelIndex < mss_max3DChannels; ++channelIndex) {
        if (!MSS_Is3DChannelFree(channelIndex))
            MSS_Pause3DChannel(channelIndex);
    }

    const int32_t firstStreamChannel = MSS_STREAM_CHANNEL_FIRST;
    const int32_t endStreamChannel = firstStreamChannel + mss_streamChannelCount;
    for (int32_t channelIndex = firstStreamChannel; channelIndex < endStreamChannel; ++channelIndex) {
        if (!MSS_IsStreamChannelFree(channelIndex) && mss_channelInfo[channelIndex].alias->channel != SND_ALIAS_CHANNEL_MENU) {
            MSS_PauseStreamChannel(channelIndex);
        }
    }

    mss_paused = qtrue;
    mss_pauseStartTime = mss_soundTime;
}

/* Source: CoDUOMP.exe 0x00453760..0x004538e2.
 * Name and source-level helper structure: exact same-module Mac symbol
 * MSS_UnpauseSounds. Only records carrying the per-channel pause latch resume;
 * their end times advance by the same wrapping 32-bit elapsed interval. */
void MSS_UnpauseSounds(void)
{
    if (mss_digitalDriver == NULL || !mss_paused)
        return;

    const int32_t timeShift = (int32_t)((uint32_t)mss_soundTime - (uint32_t)mss_pauseStartTime);

    const int32_t first2DChannel = MSS_2D_CHANNEL_FIRST;
    const int32_t end2DChannel = first2DChannel + mss_2dChannelCount;
    for (int32_t channelIndex = first2DChannel; channelIndex < end2DChannel; ++channelIndex) {
        if (!MSS_Is2DChannelFree(channelIndex) && mss_channelInfo[channelIndex].paused) {
            MSS_Unpause2DChannel(channelIndex, timeShift);
        }
    }

    for (int32_t channelIndex = 0; channelIndex < mss_max3DChannels; ++channelIndex) {
        if (!MSS_Is3DChannelFree(channelIndex) && mss_channelInfo[channelIndex].paused) {
            MSS_Unpause3DChannel(channelIndex, timeShift);
        }
    }

    const int32_t firstStreamChannel = MSS_STREAM_CHANNEL_FIRST;
    const int32_t endStreamChannel = firstStreamChannel + mss_streamChannelCount;
    for (int32_t channelIndex = firstStreamChannel; channelIndex < endStreamChannel; ++channelIndex) {
        if (!MSS_IsStreamChannelFree(channelIndex) && mss_channelInfo[channelIndex].paused) {
            MSS_UnpauseStreamChannel(channelIndex, timeShift);
        }
    }

    mss_paused = qfalse;
    mss_pauseStartTime = 0;
}

/* Source: CoDUOMP.exe 0x004538f0..0x00453a8a.
 * Name and source-level helper structure: exact same-module Mac symbol
 * MSS_UpdateLoopingSounds. A looping channel must be refreshed once per
 * sound frame; otherwise this pass stops it before advancing the frame tag. */
void MSS_UpdateLoopingSounds(void)
{
    if (mss_digitalDriver == NULL || mss_paused)
        return;

    for (int32_t channelIndex = 0; channelIndex < mss_max3DChannels; ++channelIndex) {
        const mss_channel_info_t *const channel = &mss_channelInfo[channelIndex];
        if (!MSS_Is3DChannelFree(channelIndex) && channel->alias->loop != 0 && channel->lastUpdateTime != mss_lastSoundTime) {
            MSS_Stop3DChannel(channelIndex);
        }
    }

    const int32_t end2DChannel = MSS_2D_CHANNEL_FIRST + mss_2dChannelCount;
    for (int32_t channelIndex = MSS_2D_CHANNEL_FIRST; channelIndex < end2DChannel; ++channelIndex) {
        const mss_channel_info_t *const channel = &mss_channelInfo[channelIndex];
        if (!MSS_Is2DChannelFree(channelIndex) && channel->alias->loop != 0 && channel->lastUpdateTime != mss_lastSoundTime) {
            MSS_Stop2DChannel(channelIndex);
        }
    }

    const int32_t endStreamChannel = MSS_STREAM_CHANNEL_FIRST + mss_streamChannelCount;
    for (int32_t channelIndex = MSS_STREAM_CHANNEL_FIRST; channelIndex < endStreamChannel; ++channelIndex) {
        const mss_channel_info_t *const channel = &mss_channelInfo[channelIndex];
        if (!MSS_IsStreamChannelFree(channelIndex) && channel->alias->loop != 0 && channel->lastUpdateTime != mss_lastSoundTime) {
            MSS_StopStreamChannel(channelIndex);
        }
    }

    mss_lastSoundTime = mss_soundTime;
}

/* Source: CoDUOMP.exe 0x00453a90..0x00453b28.
 * Name and parameter order: exact same-module Mac symbol
 * MSS_UpdateBackgroundVolume. Background streams use logical channels 32..34.
 * A downward fade that reaches an exactly silent target closes the stream. */
qboolean MSS_UpdateBackgroundVolume(int32_t backgroundIndex, int32_t elapsedMsec)
{
    mss_background_fade_t *const fade = &mss_backgroundFades[backgroundIndex];
    const int32_t channelIndex = MSS_STREAM_CHANNEL_FIRST + backgroundIndex;
    mss_channel_info_t *const channel = &mss_channelInfo[channelIndex];
    long double updatedVolume = (long double)elapsedMsec * fade->ratePerMsec + channel->logicalVolume;

    if (fade->ratePerMsec > 0.0f) {
        if (updatedVolume > fade->targetVolume)
            updatedVolume = fade->targetVolume;
    } else if (updatedVolume < fade->targetVolume) {
        updatedVolume = fade->targetVolume;
        if (fade->targetVolume == 0.0f) {
            MSS_StopStreamChannel(channelIndex);
            return qfalse;
        }
    }

    channel->lastUpdateTime = mss_lastSoundTime;
    channel->logicalVolume = (float)updatedVolume;
    return qtrue;
}

/* Source: CoDUOMP.exe 0x00453b30..0x00453b73, recovered from executable
 * bytes omitted by Ghidra's initial function boundaries. Name and parameter
 * order: exact same-module Mac symbol MSS_UpdateVolume. Crossing the target
 * snaps to it and clears the rate; an in-progress value retains its rate. */
void MSS_UpdateVolume(mss_channel_volume_t *volume, int32_t elapsedMsec)
{
    long double updated = (long double)elapsedMsec * volume->ratePerMsec + volume->current;
    volume->current = (float)updated; /* FST [ecx], x87 value retained */
    /* The x87 status test puts unordered rates on the nonnegative path. */
    if (!(volume->ratePerMsec < 0.0f)) {
        if (updated > volume->target) {
            volume->current = volume->target;
            volume->ratePerMsec = 0.0f;
        }
    } else if (updated < volume->target) {
        volume->current = volume->target;
        volume->ratePerMsec = 0.0f;
    }
}

/* Source: CoDUOMP.exe 0x00453b80..0x0045408a.
 * Name and source-level factoring: exact same-module Mac symbol
 * MSS_UpdateMasterVolumes. MSVC inlined all eleven MSS_UpdateVolume calls;
 * the Mac body proves the original loop over the ten alias-channel records.
 * The public mss_volume cvar is clamped to [0,1] before it scales the fading
 * master volume, with unordered values deliberately left unchanged. */
void MSS_UpdateMasterVolumes(int32_t elapsedMsec)
{
    for (int32_t channel = 0; channel < SND_ALIAS_CHANNEL_COUNT; ++channel) {
        mss_channel_volume_t *const volume = &mss_channelVolumes[channel];
        /* The inlined Windows copies use FSTP and reload current before the
         * target compare, unlike standalone MSS_UpdateVolume's retained ST0. */
        float updated = (float)((long double)elapsedMsec * volume->ratePerMsec + volume->current);
        volume->current = updated;
        if (!(volume->ratePerMsec < 0.0f)) {
            if (updated > volume->target) {
                volume->current = volume->target;
                volume->ratePerMsec = 0.0f;
            }
        } else if (updated < volume->target) {
            volume->current = volume->target;
            volume->ratePerMsec = 0.0f;
        }
    }

    if (mss_masterVolume.ratePerMsec == 0.0f) {
        if (!mss_volume->modified)
            return;
    } else {
        float updated = (float)((long double)elapsedMsec * mss_masterVolume.ratePerMsec + mss_masterVolume.current);
        mss_masterVolume.current = updated;
        if (!(mss_masterVolume.ratePerMsec < 0.0f)) {
            if (updated > mss_masterVolume.target) {
                mss_masterVolume.current = mss_masterVolume.target;
                mss_masterVolume.ratePerMsec = 0.0f;
            }
        } else if (updated < mss_masterVolume.target) {
            mss_masterVolume.current = mss_masterVolume.target;
            mss_masterVolume.ratePerMsec = 0.0f;
        }
        if (mss_masterVolume.current == 0.0f && mss_masterVolume.ratePerMsec == 0.0f) {
            MSS_StopSounds(MSS_STOP_ALL_SOUNDS);
        }
    }

    mss_volume->modified = qfalse;
    float configuredVolume = mss_volume->value;
    if (configuredVolume < 0.0f)
        configuredVolume = 0.0f;
    else if (configuredVolume > 1.0f)
        configuredVolume = 1.0f;
    mss_effectVolume = mss_masterVolume.current * configuredVolume;
}

/* Source: CoDUOMP.exe 0x00454090..0x004541ca.
 * Name and source-level helper structure: exact same-module Mac symbol
 * MSS_AnyMasters. Free checks may reclaim completed streams as a side effect;
 * any still-active alias carrying the isMaster flag makes the result true. */
qboolean MSS_AnyMasters(void)
{
    for (int32_t channelIndex = 0; channelIndex < mss_max3DChannels; ++channelIndex) {
        if (!MSS_Is3DChannelFree(channelIndex) && mss_channelInfo[channelIndex].alias->isMaster)
            return qtrue;
    }

    const int32_t endStreamChannel = MSS_STREAM_CHANNEL_FIRST + mss_streamChannelCount;
    for (int32_t channelIndex = MSS_STREAM_CHANNEL_FIRST; channelIndex < endStreamChannel; ++channelIndex) {
        if (!MSS_IsStreamChannelFree(channelIndex) && mss_channelInfo[channelIndex].alias->isMaster)
            return qtrue;
    }

    const int32_t end2DChannel = MSS_2D_CHANNEL_FIRST + mss_2dChannelCount;
    for (int32_t channelIndex = MSS_2D_CHANNEL_FIRST; channelIndex < end2DChannel; ++channelIndex) {
        if (!MSS_Is2DChannelFree(channelIndex) && mss_channelInfo[channelIndex].alias->isMaster)
            return qtrue;
    }
    return qfalse;
}

/* Source: CoDUOMP.exe 0x004541d0..0x0045438e.
 * Name and helper structure: exact same-module Mac symbol
 * MSS_Update3DChannel. The ordinary alias-blended distance attenuation is
 * followed by master/slave ducking, alias-channel volume, and global volume. */
void MSS_Update3DChannel(int32_t channelIndex)
{
    mss_channel_info_t *const channel = &mss_channelInfo[channelIndex];
    snd_alias_t *const alias = channel->alias;
    snd_alias_t *const secondaryAlias = channel->secondaryAlias;
    const float aliasBlend = channel->aliasBlend;
    const float oneMinusBlend = 1.0f - aliasBlend;
    miles_3d_sample_handle_t const sample = mss_3dSampleHandles[channelIndex];
    vec3_t position;

    MSS_GetCurrent3DPosition(channel->effectId, channel->effectOffset, position);
    MSS_Set3DPosition(sample, position);

    const float minimumDistance = (float)((long double)oneMinusBlend * (long double)alias->distanceMin +
                                          (long double)aliasBlend * (long double)secondaryAlias->distanceMin);
    const float maximumDistance = (float)((long double)oneMinusBlend * (long double)alias->distanceMax +
                                          (long double)aliasBlend * (long double)secondaryAlias->distanceMax);
    vec3_t listenerDelta;
    for (int32_t component = 0; component < 3; ++component) {
        listenerDelta[component] = mss_listenerOrigin[component] - position[component];
    }
    const float distance =
        (float)sqrtl(((long double)listenerDelta[2] * listenerDelta[2] + (long double)listenerDelta[1] * listenerDelta[1]) +
                     (long double)listenerDelta[0] * listenerDelta[0]);

    const float distancePastMinimum = (float)((long double)distance - (long double)minimumDistance);
    float attenuation;
    if (distancePastMinimum <= 0.0f) {
        attenuation = 1.0f;
    } else {
        const float fraction = (float)((long double)distancePastMinimum / ((long double)maximumDistance - (long double)minimumDistance));
        if (fraction >= 1.0f)
            attenuation = 0.0f;
        else
            attenuation = (float)((long double)1.0f - (long double)fraction);
    }
    float volume = (float)((long double)channel->logicalVolume * (long double)attenuation);

    if (mss_anyMasters && alias->isSlave) {
        const float slaveVolume = (float)((long double)oneMinusBlend * (long double)alias->slavePercentage +
                                          (long double)aliasBlend * (long double)secondaryAlias->slavePercentage);
        if (volume > slaveVolume)
            volume = slaveVolume;
    }

    volume = (float)((long double)volume * (long double)mss_channelVolumes[alias->channel].current);
    const float sampleVolume = (float)((long double)mss_effectVolume * (long double)volume);
    AIL_set_3D_sample_volume(sample, sampleVolume);
}

/* Source: CoDUOMP.exe 0x00454390..0x00454418.
 * Name and helper structure: exact same-module Mac symbol
 * MSS_Update2DChannel. It applies the same master/slave ducking and volume
 * hierarchy as the 3D path, with a fixed centered pan. */
void MSS_Update2DChannel(int32_t channelIndex)
{
    mss_channel_info_t *const channel = &mss_channelInfo[channelIndex];
    snd_alias_t *const alias = channel->alias;
    long double volume = (long double)channel->logicalVolume;

    if (mss_anyMasters && alias->isSlave) {
        const float aliasBlend = channel->aliasBlend;
        const float slaveVolume = (float)(((long double)1.0f - (long double)aliasBlend) * (long double)alias->slavePercentage +
                                          (long double)aliasBlend * (long double)channel->secondaryAlias->slavePercentage);
        if (volume > (long double)slaveVolume)
            volume = (long double)slaveVolume;
    }

    volume *= (long double)mss_channelVolumes[alias->channel].current;
    AIL_set_sample_volume_pan(mss_2dSampleHandles[channelIndex - MSS_2D_CHANNEL_FIRST], (float)((long double)mss_effectVolume * volume),
                              0.5f);
}

/* Source: CoDUOMP.exe 0x00454420..0x0045452e.
 * Name and parameter order: exact same-module Mac symbol
 * MSS_UpdateStreamChannel. Dedicated stream slots 0..4 first advance their
 * background fade; positional streams then refresh their effect-relative
 * position and pan before the shared master/slave volume hierarchy applies. */
void MSS_UpdateStreamChannel(int32_t channelIndex, int32_t elapsedMsec)
{
    const int32_t streamIndex = channelIndex - MSS_STREAM_CHANNEL_FIRST;
    if (streamIndex < MSS_ALIAS_STREAM_SLOT_FIRST && !MSS_UpdateBackgroundVolume(streamIndex, elapsedMsec)) {
        return;
    }

    mss_channel_info_t *const channel = &mss_channelInfo[channelIndex];
    mss_stream_channel_t *const streamChannel = &mss_streamChannels[streamIndex];
    snd_alias_t *const alias = channel->alias;
    float volume = channel->logicalVolume;
    float pan = 0.5f;

    if (streamChannel->is3D) {
        MSS_GetCurrent3DPosition(channel->effectId, channel->effectOffset, streamChannel->position);
        MSS_SpatializeStream(streamIndex, &volume, &pan);
    }

    long double volumeWide = (long double)volume;
    if (mss_anyMasters && alias->isSlave) {
        const float aliasBlend = channel->aliasBlend;
        const float slaveVolume = (float)(((long double)1.0f - (long double)aliasBlend) * (long double)alias->slavePercentage +
                                          (long double)aliasBlend * (long double)channel->secondaryAlias->slavePercentage);
        if (volumeWide > (long double)slaveVolume)
            volumeWide = (long double)slaveVolume;
    }

    volumeWide *= (long double)mss_channelVolumes[alias->channel].current;
    AIL_set_stream_volume_pan(mss_streamHandles[streamIndex], (float)((long double)mss_effectVolume * volumeWide), pan);
}

/* Source: CoDUOMP.exe 0x00454530..0x0045464f.
 * Name and source-level helper structure: exact same-module Mac symbol
 * MSS_UpdateAllChannels. The master-alias latch is sampled before any channel
 * volumes are updated so every active family uses one consistent ducking
 * decision for the frame. */
void MSS_UpdateAllChannels(int32_t elapsedMsec)
{
    mss_anyMasters = MSS_AnyMasters();

    for (int32_t channelIndex = 0; channelIndex < mss_max3DChannels; ++channelIndex) {
        if (!MSS_Is3DChannelFree(channelIndex))
            MSS_Update3DChannel(channelIndex);
    }

    const int32_t endStreamChannel = MSS_STREAM_CHANNEL_FIRST + mss_streamChannelCount;
    for (int32_t channelIndex = MSS_STREAM_CHANNEL_FIRST; channelIndex < endStreamChannel; ++channelIndex) {
        if (!MSS_IsStreamChannelFree(channelIndex))
            MSS_UpdateStreamChannel(channelIndex, elapsedMsec);
    }

    const int32_t end2DChannel = MSS_2D_CHANNEL_FIRST + mss_2dChannelCount;
    for (int32_t channelIndex = MSS_2D_CHANNEL_FIRST; channelIndex < end2DChannel; ++channelIndex) {
        if (!MSS_Is2DChannelFree(channelIndex))
            MSS_Update2DChannel(channelIndex);
    }
}

/* Source: CoDUOMP.exe 0x00454650..0x004546c2.
 * Name and signature: exact same-module Mac symbol MSS_RoomTypeFromString.
 * Numeric input and case-insensitive symbolic input share the 0..25 Miles
 * room-type range; invalid input prints the complete accepted-name list. */
int32_t MSS_RoomTypeFromString(const char *roomType)
{
    if (roomType[0] >= '0' && roomType[0] <= '9') {
        const int32_t roomTypeIndex = coduo_crt_atoi(roomType);
        if (roomTypeIndex >= 0 && roomTypeIndex < MSS_ROOM_TYPE_COUNT)
            return roomTypeIndex;
    } else {
        for (int32_t roomTypeIndex = 0; roomTypeIndex < MSS_ROOM_TYPE_COUNT; ++roomTypeIndex) {
            if (coduo_crt_stricmp(roomType, mss_roomTypeNames[roomTypeIndex]) == 0) {
                return roomTypeIndex;
            }
        }
    }

    Com_Printf("roomtype must be an integer from 0 to %i, or one of the following strings:\n", MSS_ROOM_TYPE_COUNT - 1);
    for (int32_t roomTypeIndex = 0; roomTypeIndex < MSS_ROOM_TYPE_COUNT; ++roomTypeIndex) {
        Com_Printf("  %s\n", mss_roomTypeNames[roomTypeIndex]);
    }
    return 0;
}

/* Source: CoDUOMP.exe 0x004546d0..0x00454813.
 * Name and parameter order: exact same-module Mac symbol
 * MSS_SetEnvironmentEffects. With EAX active this boundary handles only the
 * explicit underwater override; ordinary environment selection remains with
 * the EAX manager. Without EAX, Miles receives every requested room type. */
void MSS_SetEnvironmentEffects(const char *roomType, float reverbLevel, int32_t fadeMsec)
{
    qboolean applyRoomType = qfalse;

    if (mss_eaxAvailable) {
        if (strcmp(roomType, "underwater") == 0) {
            mss_underwaterEffectActive = qtrue;
            applyRoomType = qtrue;
        } else if (mss_underwaterEffectActive) {
            mss_underwaterEffectActive = qfalse;
            for (int32_t channelIndex = 0; channelIndex < mss_max3DChannels; ++channelIndex) {
                if (!MSS_Is3DChannelFree(channelIndex)) {
                    AIL_set_3D_sample_effects_level(mss_3dSampleHandles[channelIndex], 1.0f);
                }
            }
        }
    } else if (mss_digitalDriver != NULL) {
        applyRoomType = qtrue;
    }

    if (!applyRoomType)
        return;

    const int32_t roomTypeIndex = MSS_RoomTypeFromString(roomType);
    AIL_set_digital_master_room_type(mss_digitalDriver, roomTypeIndex);
    AIL_set_3D_room_type(mss_3dProvider, roomTypeIndex);
    mss_roomType = roomTypeIndex;
    mss_reverbTarget = reverbLevel;
    if (fadeMsec < 1)
        fadeMsec = 1;
    mss_reverbRate = (float)(((long double)mss_reverbTarget - (long double)mss_reverbLevel) / (long double)fadeMsec);
}

/* Source: CoDUOMP.exe 0x00454820..0x00454a5e.
 * Name and source-level helper structure: exact same-module Mac symbol
 * MSS_UpdateRoomEffects. Modified room cvars restart the environment fade;
 * an active fade then propagates its wet level to every live channel family. */
void MSS_UpdateRoomEffects(int32_t elapsedMsec)
{
    if (mss_roomtype->modified || mss_wetlevel->modified) {
        mss_roomtype->modified = qfalse;
        mss_wetlevel->modified = qfalse;
        float wetLevel = mss_wetlevel->value;
        if (wetLevel < 0.0f)
            wetLevel = 0.0f;
        else if (wetLevel > 1.0f)
            wetLevel = 1.0f;
        MSS_SetEnvironmentEffects(mss_roomtype->string, wetLevel, 0);
    }

    if (mss_reverbRate == 0.0f)
        return;

    mss_reverbLevel = (float)((long double)elapsedMsec * (long double)mss_reverbRate + (long double)mss_reverbLevel);
    if ((!(mss_reverbRate < 0.0f) && mss_reverbLevel >= mss_reverbTarget) ||
        (mss_reverbRate < 0.0f && mss_reverbLevel <= mss_reverbTarget)) {
        mss_reverbLevel = mss_reverbTarget;
        mss_reverbRate = 0.0f;
    }

    for (int32_t channelIndex = 0; channelIndex < mss_max3DChannels; ++channelIndex) {
        if (!MSS_Is3DChannelFree(channelIndex)) {
            AIL_set_3D_sample_effects_level(mss_3dSampleHandles[channelIndex], mss_reverbLevel);
        }
    }

    const int32_t end2DChannel = MSS_2D_CHANNEL_FIRST + mss_2dChannelCount;
    for (int32_t channelIndex = MSS_2D_CHANNEL_FIRST; channelIndex < end2DChannel; ++channelIndex) {
        if (!MSS_Is2DChannelFree(channelIndex)) {
            AIL_set_sample_reverb_levels(mss_2dSampleHandles[channelIndex - MSS_2D_CHANNEL_FIRST], 1.0f, mss_reverbLevel);
        }
    }

    const int32_t endStreamChannel = MSS_STREAM_CHANNEL_FIRST + mss_streamChannelCount;
    for (int32_t channelIndex = MSS_STREAM_CHANNEL_FIRST; channelIndex < endStreamChannel; ++channelIndex) {
        if (!MSS_IsStreamChannelFree(channelIndex)) {
            AIL_set_stream_reverb_levels(mss_streamHandles[channelIndex - MSS_STREAM_CHANNEL_FIRST], 1.0f, mss_reverbLevel);
        }
    }
}

/* Source: CoDUOMP.exe 0x00454a60..0x00454c76.
 * Name and source-level helper structure: exact same-module Mac symbol
 * MSS_UpdateTimeScale. Non-positive timescale is treated as normal speed;
 * every live channel's current Miles playback rate is rescaled by new/old. */
void MSS_UpdateTimeScale(void)
{
    float newScale = com_timescale->value;
    if (newScale <= 0.0f)
        newScale = 1.0f;
    if (newScale == mss_playbackRateScale)
        return;

    const float rateScale = newScale / mss_playbackRateScale;
    mss_playbackRateScale = newScale;

    for (int32_t channelIndex = 0; channelIndex < mss_max3DChannels; ++channelIndex) {
        if (!MSS_Is3DChannelFree(channelIndex)) {
            miles_3d_sample_handle_t const sample = mss_3dSampleHandles[channelIndex];
            const int32_t playbackRate = AIL_3D_sample_playback_rate(sample);
            AIL_set_3D_sample_playback_rate(sample, FastRound((float)((long double)playbackRate * (long double)rateScale)));
        }
    }

    const int32_t endStreamChannel = MSS_STREAM_CHANNEL_FIRST + mss_streamChannelCount;
    for (int32_t channelIndex = MSS_STREAM_CHANNEL_FIRST; channelIndex < endStreamChannel; ++channelIndex) {
        if (!MSS_IsStreamChannelFree(channelIndex)) {
            miles_stream_handle_t const stream = mss_streamHandles[channelIndex - MSS_STREAM_CHANNEL_FIRST];
            const int32_t playbackRate = AIL_stream_playback_rate(stream);
            AIL_set_stream_playback_rate(stream, FastRound((float)((long double)playbackRate * (long double)rateScale)));
        }
    }

    const int32_t end2DChannel = MSS_2D_CHANNEL_FIRST + mss_2dChannelCount;
    for (int32_t channelIndex = MSS_2D_CHANNEL_FIRST; channelIndex < end2DChannel; ++channelIndex) {
        if (!MSS_Is2DChannelFree(channelIndex)) {
            miles_sample_handle_t const sample = mss_2dSampleHandles[channelIndex - MSS_2D_CHANNEL_FIRST];
            const int32_t playbackRate = AIL_sample_playback_rate(sample);
            AIL_set_sample_playback_rate(sample, FastRound((float)((long double)playbackRate * (long double)rateScale)));
        }
    }
}

/* Source: CoDUOMP.exe 0x00454cb0..0x00454dc7.
 * Name and source-level call structure: exact same-module Mac symbol
 * MSS_Update. The Win32 compiler inlines Sys_Milliseconds and a copy of
 * MSS_UpdatePause, while retaining the separate function above, and keeps
 * MSS_Restore's save-data argument live in EBX; the Mac body proves the
 * ordinary source interfaces used here. */
void MSS_Update(void)
{
    if (mss_digitalDriver == NULL)
        return;

    mss_cpuPercent = AIL_digital_CPU_percent(mss_digitalDriver);
    if (com_statmon->integer != 0 && mss_cpuPercent > MSS_SOUND_CPU_WARNING_THRESHOLD_PERCENT) {
        StatMon_Warning(MSS_SOUND_CPU_WARNING_ENTRY, MSS_SOUND_CPU_WARNING_DURATION_MSEC, "gfx/2d/warning@soundcpu.jpg");
    }

    const uint32_t now = Sys_Milliseconds();
    const int32_t elapsedMsec = (int32_t)(now - (uint32_t)mss_soundTime);
    mss_soundTime = (int32_t)((uint32_t)mss_soundTime + (uint32_t)elapsedMsec);

    MSS_UpdatePause();
    MSS_UpdateMasterVolumes(elapsedMsec);
    if (!mss_paused) {
        if (mss_restoreBuffer != NULL) {
            MSS_Restore(mss_restoreBuffer, mss_restoreSize);
            Z_FreeInternal(mss_restoreBuffer);
            mss_restoreBuffer = NULL;
            mss_restoreSize = 0;
        }
        MSS_UpdateTimeScale();
        MSS_UpdateRoomEffects(elapsedMsec);
        MSS_UpdateAllChannels(elapsedMsec);
    }
    MSS_UpdateRawSamples();
}

/* Source: CoDUOMP.exe 0x00454dd0..0x00454f57.
 * Name and source-level helper structure: exact same-module Mac symbol
 * MSS_StopSounds. The flag names describe their directly proven preserve
 * behavior; zero stops every channel and restores the generic dry room. */
void MSS_StopSounds(uint32_t flags)
{
    if (mss_digitalDriver == NULL)
        return;

    if ((flags & MSS_STOP_PRESERVE_2D_AND_3D) == 0) {
        const int32_t end2DChannel = MSS_2D_CHANNEL_FIRST + mss_2dChannelCount;
        for (int32_t channelIndex = MSS_2D_CHANNEL_FIRST; channelIndex < end2DChannel; ++channelIndex) {
            if (!MSS_Is2DChannelFree(channelIndex))
                MSS_Stop2DChannel(channelIndex);
        }

        for (int32_t channelIndex = 0; channelIndex < mss_max3DChannels; ++channelIndex) {
            if (!MSS_Is3DChannelFree(channelIndex))
                MSS_Stop3DChannel(channelIndex);
        }
    }

    const int32_t endStreamChannel = MSS_STREAM_CHANNEL_FIRST + mss_streamChannelCount;
    const int32_t musicChannel = MSS_STREAM_CHANNEL_FIRST + MSS_MUSIC_BACKGROUND_INDEX;
    const int32_t firstAmbientChannel = MSS_STREAM_CHANNEL_FIRST + MSS_AMBIENT_BACKGROUND_FIRST_INDEX;
    const int32_t secondAmbientChannel = MSS_STREAM_CHANNEL_FIRST + MSS_AMBIENT_BACKGROUND_SECOND_INDEX;
    for (int32_t channelIndex = MSS_STREAM_CHANNEL_FIRST; channelIndex < endStreamChannel; ++channelIndex) {
        if (MSS_IsStreamChannelFree(channelIndex))
            continue;
        if ((flags & MSS_STOP_PRESERVE_MUSIC) != 0 && channelIndex == musicChannel) {
            continue;
        }
        if ((flags & MSS_STOP_PRESERVE_AMBIENT) != 0 && (channelIndex == firstAmbientChannel || channelIndex == secondAmbientChannel)) {
            continue;
        }
        MSS_StopStreamChannel(channelIndex);
    }

    if ((flags & MSS_STOP_PRESERVE_ROOM_EFFECTS) == 0)
        MSS_SetEnvironmentEffects("generic", MSS_DRY_REVERB_LEVEL, MSS_ENVIRONMENT_FADE_IMMEDIATE_MSEC);
}

/* Source: CoDUOMP.exe 0x004513a0..0x004514aa.
 * Name and parameter order: exact same-module Mac symbol MSS_SpatializeStream.
 * Stream positions occupy a separate 13-entry state array, while alias and
 * blend metadata use logical channel records 32..44. The normalized listener-
 * right projection maps to Miles pan as `(1 - projection) * 0.5`; distance
 * attenuation uses the alias-blended minimum and maximum ranges. */
void MSS_SpatializeStream(int32_t streamIndex, float *volume, float *pan)
{
    const mss_stream_channel_t *const stream = &mss_streamChannels[streamIndex];
    const mss_channel_info_t *const channel = &mss_channelInfo[MSS_STREAM_CHANNEL_FIRST + streamIndex];
    vec3_t direction;

    for (int32_t component = 0; component < 3; ++component) {
        direction[component] = stream->position[component] - mss_listenerOrigin[component];
    }
    const float distance = VectorNormalize(direction);
    const float rightProjection = (float)((long double)mss_listenerAxis[1][2] * (long double)direction[2] +
                                          (long double)mss_listenerAxis[1][1] * (long double)direction[1] +
                                          (long double)mss_listenerAxis[1][0] * (long double)direction[0]);

    const long double oneMinusBlendWide = (long double)1.0f - (long double)channel->aliasBlend;
    const float oneMinusBlend = (float)oneMinusBlendWide;
    const float minimumDistance = (float)(oneMinusBlendWide * (long double)channel->alias->distanceMin +
                                          (long double)channel->aliasBlend * (long double)channel->secondaryAlias->distanceMin);
    const long double distancePastMinimum = (long double)distance - (long double)minimumDistance;

    long double attenuation;
    if (distancePastMinimum <= (long double)0.0f) {
        attenuation = (long double)1.0f;
    } else {
        const long double maximumDistance = (long double)oneMinusBlend * (long double)channel->alias->distanceMax +
                                            (long double)channel->aliasBlend * (long double)channel->secondaryAlias->distanceMax;
        const long double fraction = distancePastMinimum / (maximumDistance - (long double)minimumDistance);
        if (fraction >= (long double)1.0f)
            attenuation = (long double)0.0f;
        else
            attenuation = (long double)1.0f - fraction;
    }

    *volume = (float)((long double)*volume * attenuation);
    *pan = (float)(((long double)1.0f - (long double)rightProjection) * (long double)0.5f);
}

/* Source: CoDUOMP.exe 0x004511a0..0x00451322.
 * Name and source parameter order: same-module Mac symbol MSS_SetChannelInfo
 * and its three named PPC callers. A spatial sound attached to an effect keeps
 * its position in that effect's local frame; the current frame is supplied by
 * cgame VM command 14. The X delta is rounded to float while Y and Z remain
 * live; each dot product then accumulates Z, Y, and rounded X before its
 * binary32 store.
 * Channel end time uses explicit unsigned arithmetic for the executable's
 * 32-bit wrapping `soundTime - timeShift + durationMsec` calculation. */
void MSS_SetChannelInfo(int32_t channelIndex, int32_t effectId, snd_alias_t *alias, snd_alias_t *secondaryAlias, float aliasBlend,
                        const vec3_t position, float volume, float pitch, int32_t playbackRate, int32_t durationMsec, int32_t timeShift)
{
    mss_channel_info_t *const channel = &mss_channelInfo[channelIndex];

    if (effectId >= 0 && effectId < MSS_EFFECT_ID_LIMIT && MSS_IsAliasChannel3D(alias->channel)) {
        vec3_t effectOrigin;
        axis_t effectAxis;

        (void)VM_Call(coduo_cgameVm, CGVM_GET_EFFECT_ORIGIN_AXIS, effectId, (intptr_t)effectOrigin, (intptr_t)effectAxis, 0, 0, 0, 0, 0, 0,
                      0, 0, 0);
        const float deltaX = (float)((long double)position[0] - (long double)effectOrigin[0]);
        const long double deltaY = (long double)position[1] - (long double)effectOrigin[1];
        const long double deltaZ = (long double)position[2] - (long double)effectOrigin[2];
        for (int32_t axis = 0; axis < 3; ++axis) {
            channel->effectOffset[axis] = (float)((long double)effectAxis[axis][2] * deltaZ + (long double)effectAxis[axis][1] * deltaY +
                                                  (long double)effectAxis[axis][0] * (long double)deltaX);
        }
    } else {
        channel->effectOffset[2] = 0.0f;
        channel->effectOffset[1] = 0.0f;
        channel->effectOffset[0] = 0.0f;
    }

    channel->effectId = effectId;
    channel->aliasChannel = alias->channel;
    channel->logicalVolume = volume;
    channel->aliasPitchScale = pitch;
    channel->basePlaybackRate = playbackRate;
    channel->alias = alias;
    channel->secondaryAlias = secondaryAlias;
    channel->aliasBlend = aliasBlend;
    channel->lastUpdateTime = mss_lastSoundTime;
    channel->endTime = (int32_t)((uint32_t)mss_soundTime - (uint32_t)timeShift + (uint32_t)durationMsec);
    channel->paused = mss_paused && alias->channel != SND_ALIAS_CHANNEL_MENU;
}

/* Source: CoDUOMP.exe 0x00450900..0x00450922.
 * Name and callback role: exact same-module Mac symbol MSS_FileOpenCallback;
 * the function-pointer position is proven by AIL_set_file_callbacks at
 * 0x004551ea. Miles receives zero rather than the engine's -1 open failure. */
int32_t MILES_CALLBACK MSS_FileOpenCallback(const char *filename, int32_t *fileHandle)
{
    const int32_t fileSize = FS_FOpenFileRead_Internal(filename, fileHandle, qtrue, qtrue);
    return fileSize == MILES_FILE_OPEN_FAILED ? MILES_FILE_OPEN_FAILURE_RESULT : fileSize;
}

/* Source: CoDUOMP.exe 0x00450930..0x0045093d.
 * Name and callback role: exact same-module Mac symbol MSS_FileCloseCallback. */
void MILES_CALLBACK MSS_FileCloseCallback(int32_t fileHandle)
{
    FS_FCloseFile(fileHandle);
}

/* Source: CoDUOMP.exe 0x00450940..0x004509b9.
 * Name and callback role: exact same-module Mac symbol MSS_FileSeekCallback.
 * Miles' origin values are reordered into the engine/CRT SEEK_SET/CUR/END
 * values before seeking; the original then inlines FS_FTell. */
int32_t MILES_CALLBACK MSS_FileSeekCallback(int32_t fileHandle, int32_t offset, int32_t origin)
{
    engineFileSeekOrigin_t engineOrigin;
    switch ((milesFileSeekOrigin_t)origin) {
    case MILES_FILE_SEEK_END:
        engineOrigin = ENGINE_FILE_SEEK_END;
        break;
    case MILES_FILE_SEEK_SET:
        engineOrigin = ENGINE_FILE_SEEK_SET;
        break;
    case MILES_FILE_SEEK_CURRENT:
        engineOrigin = ENGINE_FILE_SEEK_CURRENT;
        break;
    default:
        return 0;
    }

    (void)FS_Seek(fileHandle, offset, engineOrigin);
    return FS_FTell(fileHandle);
}

/* Source: CoDUOMP.exe 0x004509c0..0x004509d9.
 * Name and callback role: exact same-module Mac symbol MSS_FileReadCallback. */
int32_t MILES_CALLBACK MSS_FileReadCallback(int32_t fileHandle, void *buffer, int32_t byteCount)
{
    return FS_Read(buffer, byteCount, fileHandle);
}

/* Source: CoDUOMP.exe 0x004509e0. This unreferenced compiler leaf consists
 * only of RET. It has no direct code/data xrefs and no named Mac counterpart,
 * so its source identity remains honestly unresolved. */
void FUN_004509e0(void)
{
}
