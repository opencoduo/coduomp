#include "cinematic.h"

#include "cgame.h"
#include "console.h"
#include "qcommon/q_string.h"
#include "../renderer/gl_state.h"
#include "../renderer/renderer_api.h"

#include <string.h>

/* Source: CoDUOMP.exe 0x008cb4f8..0x008cd1f8 (.bss). Sixteen RoQ playback
 * records with an original i386 stride of 0x1d0 bytes. */
cinematic_t cinematics[MAX_VIDEO_HANDLES];

/* Source: CoDUOMP.exe 0x0058fb98..0x0058fba7 (.rdata). */
const vec4_t colorBlack = {0.0f, 0.0f, 0.0f, 1.0f};

/* Source: CoDUOMP.exe 0x007e26e0..0x007e28e0 (.bss). RLL byte-delta table:
 * indices 0..127 contain +n^2 and 128..255 contain -n^2. */
static int16_t rllSquareTable[256];

/* Source: CoDUOMP.exe .bss. These five 256-entry conversion tables retain the
 * conventional RoQ component names. ROQ_GenYUVTables is their only producer;
 * the codebook decoder and the two pixel packers consume them. */
static int32_t roqUgTable[256]; /* 0x00842cf8 */
static int32_t roqVgTable[256]; /* 0x008430f8 */
static int32_t roqVrTable[256]; /* 0x008cd1f8 */
static int32_t roqYyTable[256]; /* 0x008cd5f8 */
static int32_t roqUbTable[256]; /* 0x008cd9f8 */

enum {
    ROQ_CODEBOOK_ENTRY_COUNT = 256,
    ROQ_CODEBOOK_RECORD_BYTES = 6,
    ROQ_CODEBOOK_LUMA_COUNT = 4,
    ROQ_CODEBOOK_INDEX_COUNT = 4,
    ROQ_CODEBOOK_VQ2_COUNT_SHIFT = 8,
    ROQ_CODEBOOK_COUNT_MASK = 255,
    ROQ_VQ_SCALE = 2,
    ROQ_VQ2_NORMAL_WIDTH = 2,
    ROQ_VQ2_NORMAL_HEIGHT = 2,
    ROQ_VQ2_HALF_WIDTH = 1,
    ROQ_VQ2_SMOOTHED_HEIGHT = 4,
    ROQ_VQ2_SMOOTHED_PIXEL_COUNT = 8,
    ROQ_PIXEL_BYTES_LUMA = 1,
    ROQ_PIXEL_BYTES_RGB565 = 2,
    ROQ_PIXEL_BYTES_RGBA32 = 4
};

/* NOT_FROM_ORIGINAL_SOURCE: typed views of three original .bss storage banks.
 * CoDUOMP.exe and the PowerPC Mac client prove the byte-addressed decoder view
 * and the normal RGBA32 blitter view, but not retail C union declarations. */
typedef union roq_vq2_storage_u {
    uint8_t bytes[ROQ_CODEBOOK_ENTRY_COUNT * ROQ_VQ2_SMOOTHED_HEIGHT * ROQ_VQ2_NORMAL_WIDTH * sizeof(uint32_t)];
    uint32_t normalRgba32[ROQ_CODEBOOK_ENTRY_COUNT][ROQ_VQ2_NORMAL_HEIGHT][ROQ_VQ2_NORMAL_WIDTH];
} roq_vq2_storage_t;

typedef union roq_vq4_storage_u {
    uint8_t bytes[ROQ_CODEBOOK_ENTRY_COUNT * (ROQ_VQ2_SMOOTHED_HEIGHT * ROQ_VQ_SCALE) * (ROQ_VQ2_NORMAL_WIDTH * ROQ_VQ_SCALE) *
                  sizeof(uint32_t)];
    uint32_t normalRgba32[ROQ_CODEBOOK_ENTRY_COUNT][ROQ_VQ2_NORMAL_HEIGHT * ROQ_VQ_SCALE][ROQ_VQ2_NORMAL_WIDTH * ROQ_VQ_SCALE];
} roq_vq4_storage_t;

typedef union roq_vq8_storage_u {
    uint8_t bytes[ROQ_CODEBOOK_ENTRY_COUNT * (ROQ_VQ2_SMOOTHED_HEIGHT * ROQ_VQ_SCALE * ROQ_VQ_SCALE) *
                  (ROQ_VQ2_NORMAL_WIDTH * ROQ_VQ_SCALE * ROQ_VQ_SCALE) * sizeof(uint32_t)];
    uint32_t normalRgba32[ROQ_CODEBOOK_ENTRY_COUNT][ROQ_VQ2_NORMAL_HEIGHT * ROQ_VQ_SCALE * ROQ_VQ_SCALE]
                         [ROQ_VQ2_NORMAL_WIDTH * ROQ_VQ_SCALE * ROQ_VQ_SCALE];
} roq_vq8_storage_t;

static roq_vq2_storage_t roqVq2Codebook; /* 0x008c34f8 */
static roq_vq4_storage_t roqVq4Codebook; /* 0x00822cf8 */
static roq_vq8_storage_t roqVq8Codebook; /* 0x008434f8 */

/* Source: CoDUOMP.exe 0x007e28e0..0x007e2ce0 (.bss). Signed byte offsets from
 * a destination block to the motion-compensation source block. */
static int32_t roqMotionOffsets[256];

/* Source: CoDUOMP.exe 0x005c3d50 (.data). Index of the cinematic whose decoder
 * state is active. This is process-local state, not a serialized handle. */
static int32_t currentCinematicHandle = -1;

/* Source: CoDUOMP.exe 0x005c3d54 (.data). Handle used by the client-level
 * full-screen cinematic path; distinct from the decoder's current handle. */
static int32_t clientCinematicHandle = -1;

/* Source: CoDUOMP.exe 0x00822cf0 (.bss). CIN_PlayCinematic establishes this
 * alongside currentCinematicHandle; CIN_RunCinematic uses a mismatch to detect
 * a decoder-context switch and restart the requested stream. */
static int32_t lastCinematicHandle;

enum {
    ROQ_FRAME_BUFFER_BYTES = 2097152,
    ROQ_FILE_BUFFER_BYTES = 65536,
    ROQ_CHUNK_HEADER_BYTES = 8,
    ROQ_MAX_CHUNK_PAYLOAD_BYTES = ROQ_FILE_BUFFER_BYTES - ROQ_CHUNK_HEADER_BYTES,
    ROQ_QUAD_POINTER_CAPACITY = 32768,
    ROQ_QUAD_TERMINATOR_COUNT = 64,
    ROQ_ROOT_QUAD_SIZE = 16,
    ROQ_MIN_QUAD_SIZE = 4,
    ROQ_MAX_STORED_QUAD_SIZE = 8,
    ROQ_MOTION_DIMENSION = 16,
    ROQ_LEGACY_MAX_DIMENSION = 256,
    ROQ_DEFAULT_DIMENSION = 512,
    ROQ_FILE_HEADER_BYTES = 16,
    ROQ_AUDIO_SAMPLE_RATE = 22050,
    ROQ_AUDIO_SAMPLE_WIDTH_BYTES = 2,
    ROQ_AUDIO_MONO_CHANNELS = 1,
    ROQ_AUDIO_STEREO_CHANNELS = 2,
    ROQ_AUDIO_MONO_MAX_ENCODED_BYTES = ROQ_FILE_BUFFER_BYTES / (2 * sizeof(int16_t)),
    ROQ_AUDIO_STEREO_MAX_ENCODED_BYTES = ROQ_FILE_BUFFER_BYTES / sizeof(int16_t)
};

/* Source: CoDUOMP.exe 0x005d26e0..0x007d26e0 (.bss). Two-megabyte decoded
 * cinematic frame store. Each quad list addresses both frame-sized regions. */
static uint8_t roqFrameBuffer[ROQ_FRAME_BUFFER_BYTES];

/* Source: CoDUOMP.exe 0x007d26e0..0x007e26e0 (.bss). File/chunk staging
 * buffer. RoQReset loads the 16-byte file header here before RoQ_init parses
 * it, and RoQInterrupt reuses it for chunk payloads. */
static uint8_t roqFileBuffer[ROQ_FILE_BUFFER_BYTES];

/* Source: CoDUOMP.exe 0x007e2ce0 and 0x00802ce0 (.bss). Parallel terminated
 * destination-pointer lists for the two decoded frame regions. */
static uint32_t *roqQuadDestinations[2][ROQ_QUAD_POINTER_CAPACITY];

/* NOT_FROM_ORIGINAL_SOURCE: named grouping of the four original contiguous
 * cache dwords at CoDUOMP.exe 0x00822ce0..0x00822cef. setupQuad avoids
 * rebuilding the recursive destination lists while all four inputs match. */
typedef struct roq_quad_setup_cache_s {
    int32_t xOffset;
    int32_t yOffset;
    int32_t height;
    int32_t width;
} roq_quad_setup_cache_t;

static roq_quad_setup_cache_t roqQuadSetupCache;

enum {
    ROQ_COPY_ALIGNMENT_BYTES = 8,
    ROQ_BLOCK_2 = 2,
    ROQ_BLOCK_4 = 4,
    ROQ_BLOCK_8 = 8,
    ROQ_VQ_SKIP = 0x0000,
    ROQ_VQ_MOTION = 0x4000,
    ROQ_VQ_VECTOR = 0x8000,
    ROQ_VQ_SUBDIVIDE = 0xc000
};

typedef enum roq_chunk_id_e {
    ROQ_QUAD_INFO = 0x1001,
    ROQ_CODEBOOK = 0x1002,
    ROQ_QUAD_VQ = 0x1011,
    ROQ_QUAD_JPEG = 0x1012,
    ROQ_QUAD_HANG = 0x1013,
    ROQ_SOUND_MONO = 0x1020,
    ROQ_SOUND_STEREO = 0x1021,
    ROQ_PACKET = 0x1030,
    ROQ_FILE = 0x1084
} roq_chunk_id_t;

/* NOT_FROM_ORIGINAL_SOURCE: typed view of four-byte RGBA pixels used only to
 * express the original byte-lane scaling loops in CIN_DrawCinematic. */
typedef struct roq_rgba_pixel_s {
    uint8_t rgba[ROQ_PIXEL_BYTES_RGBA32];
} roq_rgba_pixel_t;

#define CIN_LETTERBOX_HEIGHT_FRACTION 0.21875f /* 0x3e600000 = 7 / 32 */

enum {
    CIN_VIRTUAL_SCREEN_WIDTH = 640,
    CIN_VIRTUAL_SCREEN_HEIGHT = 480,
    CIN_LETTERBOX_SCREEN_Y = 105,
    CIN_LETTERBOX_SCREEN_HEIGHT = 270,
    CIN_COMMAND_NAME_ARGUMENT = 1,
    CIN_COMMAND_OPTION_ARGUMENT = 2,
    CIN_OPTION_HOLD = '1',
    CIN_OPTION_LOOP = '2',
    CIN_OPTION_LETTERBOX = '3',
    CIN_AUDIO_FADE_MSEC = 1
};

/* Source: CoDUOMP.exe 0x00405d40..0x00405d6c.
 * Evidence: repaired function record
 * coduomp/mcode/CoDUOMP/FUN_00405d40_00405d6d.mcode.
 * Name: exact same-module Mac symbol CIN_HandleForVideo. */
int32_t CIN_HandleForVideo(void)
{
    for (int32_t handle = 0; handle < MAX_VIDEO_HANDLES; ++handle) {
        if (cinematics[handle].fileName[0] == '\0')
            return handle;
    }

    Com_Error(1, "\x15"
                 "CIN_HandleForVideo: none free");
    return 0;
}

/* Source: CoDUOMP.exe 0x00405d10..0x00405d3d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00405d10_00405d3e.mcode.
 * Name: exact same-module Mac symbol CIN_CloseAllVideos. */
void CIN_CloseAllVideos(void)
{
    for (int32_t handle = 0; handle < MAX_VIDEO_HANDLES; ++handle) {
        if (cinematics[handle].fileName[0] != '\0')
            (void)CIN_StopCinematic(handle);
    }
}

/* Source: CoDUOMP.exe 0x00405d70..0x00405ddb.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00405d70_00405ddc.mcode.
 * Name: exact same-module Mac symbol RllSetupTable. */
void RllSetupTable(void)
{
    for (int32_t magnitude = 0; magnitude < 128; ++magnitude) {
        const int16_t square = (int16_t)(magnitude * magnitude);
        rllSquareTable[magnitude] = square;
        rllSquareTable[magnitude + 128] = (int16_t)-square;
    }
}

/* Source: CoDUOMP.exe 0x004083c0..0x004087c7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004083c0_004087c8.mcode.
 * Name and public argument order: exact same-module Mac symbol
 * CIN_PlayCinematic. The original clears one contiguous i386 BSS interval
 * containing the separately typed RoQ work areas below; each maintained
 * object is cleared separately so native pointer width cannot alter ownership. */
int32_t CIN_PlayCinematic(const char *name, int32_t x, int32_t y, int32_t width, int32_t height, int32_t flags)
{
    char fileName[CINEMATIC_NAME_SIZE];
    const qboolean systemCinematic = (flags & CIN_SYSTEM) != 0 ? qtrue : qfalse;

    if (strstr(name, "/") == NULL && strstr(name, "\\") == NULL) {
        Com_sprintf(fileName, sizeof(fileName), "video/%s", name);
    } else {
        Com_sprintf(fileName, sizeof(fileName), "%s", name);
    }

    if (systemCinematic == qfalse) {
        for (int32_t handle = 0; handle < MAX_VIDEO_HANDLES; ++handle) {
            if (Q_stricmp(fileName, cinematics[handle].fileName) == 0)
                return handle;
        }
    } else if (cls.state != CA_DISCONNECTED && cls.state != CA_CINEMATIC && cls.state != CA_LOGO) {
        Com_Printf("Can't play a cinematic while connected to a server; "
                   "use 'disconnect' first\n");
        return -1;
    }

    Com_DPrintf("SCR_PlayCinematic( %s )\n", name);

    memset(roqFrameBuffer, 0, sizeof(roqFrameBuffer));
    memset(roqFileBuffer, 0, sizeof(roqFileBuffer));
    memset(rllSquareTable, 0, sizeof(rllSquareTable));
    memset(roqMotionOffsets, 0, sizeof(roqMotionOffsets));
    memset(roqQuadDestinations, 0, sizeof(roqQuadDestinations));
    memset(&roqQuadSetupCache, 0, sizeof(roqQuadSetupCache));
    lastCinematicHandle = 0;

    const int32_t handle = CIN_HandleForVideo();
    currentCinematicHandle = handle;
    lastCinematicHandle = handle;

    cinematic_t *const cinematic = &cinematics[handle];
    strcpy(cinematic->fileName, fileName);
    cinematic->fileSize = 0;

    fs_fileAccessed = 1;
    cinematic->fileSize = FS_FOpenFileRead(cinematic->fileName, &cinematic->fileHandle, qtrue);
    if (cinematic->fileSize <= 0) {
        Com_DPrintf("play(%s), ROQSize<=0\n", name);
        cinematic->fileName[0] = '\0';
        return -1;
    }

    CIN_SetExtents(handle, x, y, width, height);
    CIN_SetLooping(handle, (flags & CIN_LOOP) != 0 ? qtrue : qfalse);

    cinematic->width = ROQ_DEFAULT_DIMENSION;
    cinematic->height = ROQ_DEFAULT_DIMENSION;
    cinematic->holdAtEnd = (flags & CIN_HOLD) != 0 ? qtrue : qfalse;
    cinematic->alterGameState = systemCinematic;
    cinematic->silent = (flags & CIN_SILENT) != 0 ? qtrue : qfalse;
    cinematic->shader = (flags & CIN_SHADER) != 0 ? qtrue : qfalse;
    cinematic->letterBox = (flags & CIN_LETTERBOX) != 0 ? qtrue : qfalse;
    cinematic->rawAudioActive = qfalse;
    cinematic->playOnWalls = CIN_WALL_VIDEO_ENABLED;

    if (cinematic->alterGameState != qfalse) {
        if (coduo_uiVm != NULL) {
            (void)VM_Call(coduo_uiVm, UIVM_SET_ACTIVE_MENU, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        }
    } else {
        cinematic->playOnWalls = r_inGameVideo->integer;
    }

    cinematic->configuredFrameBlitters[0] = blitVQQuad32fs;
    cinematic->configuredFrameBlitters[1] = blitVQQuad32fs;
    cinematic->samplesPerPixel = ROQ_PIXEL_BYTES_RGBA32;
    ROQ_GenYUVTables();
    RllSetupTable();

    FS_Read(roqFileBuffer, ROQ_FILE_HEADER_BYTES, cinematic->fileHandle);
    const uint16_t fileId = (uint16_t)(roqFileBuffer[0] | ((uint16_t)roqFileBuffer[1] << 8));
    if (fileId != ROQ_FILE) {
        Com_DPrintf("trFMV::play(), invalid RoQ ID\n");
        RoQShutdown();
        return -1;
    }

    RoQ_init();
    cinematic->status = FMV_PLAY;
    Com_DPrintf("trFMV::play(), playing %s\n", name);
    if (cinematic->alterGameState != qfalse)
        cls.state = CA_CINEMATIC;
    Con_Close();
    return handle;
}

/* Source: CoDUOMP.exe 0x004087d0..0x00408816.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004087d0_00408817.mcode.
 * Name and argument order: exact same-module Mac symbol CIN_SetExtents. */
void CIN_SetExtents(int32_t handle, int32_t x, int32_t y, int32_t width, int32_t height)
{
    if (handle < 0 || handle >= MAX_VIDEO_HANDLES)
        return;

    cinematic_t *const cinematic = &cinematics[handle];
    if (cinematic->status == FMV_EOF)
        return;

    cinematic->screenX = x;
    cinematic->screenY = y;
    cinematic->screenWidth = width;
    cinematic->screenHeight = height;
    cinematic->dirty = qtrue;
}

/* Source: CoDUOMP.exe 0x00408820..0x00408842. Ghidra omitted this function
 * from its first-pass function table; the bytes were promoted from the
 * executable-gap record after the adjacent Mac symbol identified the role.
 * Name and signature: exact same-module Mac symbol CIN_SetLooping. */
void CIN_SetLooping(int32_t handle, qboolean looping)
{
    if (handle < 0 || handle >= MAX_VIDEO_HANDLES)
        return;

    cinematic_t *const cinematic = &cinematics[handle];
    if (cinematic->status == FMV_EOF)
        return;

    cinematic->looping = looping;
}

/* Source: CoDUOMP.exe 0x00408850..0x00408be7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00408850_00408be8.mcode.
 * Name and public signature: exact same-module Mac symbol
 * CIN_DrawCinematic. On hardware restricted to 256-pixel cinematic textures,
 * dirty 512-wide frames are reduced into a temporary 256x256 RGBA image;
 * the two specialized branches preserve the DLL's horizontal or 2x2 byte
 * averaging, while the general branch preserves its nearest-sample path. */
void CIN_DrawCinematic(int32_t handle)
{
    if (handle < 0 || handle >= MAX_VIDEO_HANDLES)
        return;

    cinematic_t *const cinematic = &cinematics[handle];
    if (cinematic->status == FMV_EOF || cinematic->decodedFrame == NULL)
        return;

    float x = (float)cinematic->screenX;
    float y = (float)cinematic->screenY;
    float width = (float)cinematic->screenWidth;
    float height = (float)cinematic->screenHeight;
    SCR_AdjustFrom640(&x, &y, &width, &height);

    if (cinematic->letterBox != qfalse) {
        const float barHeight = (float)((long double)cls.rendererConfig.vidHeight * (long double)CIN_LETTERBOX_HEIGHT_FRACTION);
        rendererExports.SetColor(colorBlack);
        rendererExports.StretchPic(0.0f, 0.0f, width, barHeight, 0.0f, 0.0f, 0.0f, 0.0f, cls.whiteShader);
        rendererExports.StretchPic(0.0f, (float)cls.rendererConfig.vidHeight - barHeight - 1.0f, width, barHeight + 1.0f, 0.0f, 0.0f, 0.0f,
                                   0.0f, cls.whiteShader);
    }

    if (cinematic->dirty != qfalse && (cinematic->width != cinematic->displayWidth || cinematic->height != cinematic->displayHeight)) {
        const int32_t horizontalScale = cinematic->width / ROQ_LEGACY_MAX_DIMENSION;
        const int32_t verticalScale = cinematic->height / ROQ_LEGACY_MAX_DIMENSION;
        const size_t scaledFrameBytes = (size_t)ROQ_LEGACY_MAX_DIMENSION * ROQ_LEGACY_MAX_DIMENSION * sizeof(roq_rgba_pixel_t);
        roq_rgba_pixel_t *const scaledFrame = Hunk_AllocateTempMemoryInternal(scaledFrameBytes);
        const roq_rgba_pixel_t *const decodedFrame = (const roq_rgba_pixel_t *)cinematic->decodedFrame;

        if (horizontalScale == 2 && verticalScale == 2) {
            for (int32_t outputY = 0; outputY < ROQ_LEGACY_MAX_DIMENSION; ++outputY) {
                const int32_t sourceY = outputY * 2;
                for (int32_t outputX = 0; outputX < ROQ_LEGACY_MAX_DIMENSION; ++outputX) {
                    const int32_t sourceX = outputX * 2;
                    roq_rgba_pixel_t *const output = &scaledFrame[outputY * ROQ_LEGACY_MAX_DIMENSION + outputX];
                    const roq_rgba_pixel_t *const topLeft = &decodedFrame[sourceY * ROQ_DEFAULT_DIMENSION + sourceX];
                    const roq_rgba_pixel_t *const topRight = topLeft + 1;
                    const roq_rgba_pixel_t *const bottomLeft = topLeft + ROQ_DEFAULT_DIMENSION;
                    const roq_rgba_pixel_t *const bottomRight = bottomLeft + 1;
                    for (int32_t component = 0; component < ROQ_PIXEL_BYTES_RGBA32; ++component) {
                        output->rgba[component] = (uint8_t)(((int32_t)topLeft->rgba[component] + topRight->rgba[component] +
                                                             bottomLeft->rgba[component] + bottomRight->rgba[component]) /
                                                            4);
                    }
                }
            }
        } else if (horizontalScale == 2 && verticalScale == 1) {
            for (int32_t outputY = 0; outputY < ROQ_LEGACY_MAX_DIMENSION; ++outputY) {
                for (int32_t outputX = 0; outputX < ROQ_LEGACY_MAX_DIMENSION; ++outputX) {
                    const int32_t sourceX = outputX * 2;
                    roq_rgba_pixel_t *const output = &scaledFrame[outputY * ROQ_LEGACY_MAX_DIMENSION + outputX];
                    const roq_rgba_pixel_t *const left = &decodedFrame[outputY * ROQ_DEFAULT_DIMENSION + sourceX];
                    const roq_rgba_pixel_t *const right = left + 1;
                    for (int32_t component = 0; component < ROQ_PIXEL_BYTES_RGBA32; ++component) {
                        output->rgba[component] = (uint8_t)(((int32_t)left->rgba[component] + right->rgba[component]) / 2);
                    }
                }
            }
        } else {
            const int32_t sourceRowPixels = cinematic->width == ROQ_DEFAULT_DIMENSION ? ROQ_DEFAULT_DIMENSION : ROQ_LEGACY_MAX_DIMENSION;
            for (int32_t outputY = 0; outputY < ROQ_LEGACY_MAX_DIMENSION; ++outputY) {
                const int32_t sourceY = outputY * verticalScale;
                for (int32_t outputX = 0; outputX < ROQ_LEGACY_MAX_DIMENSION; ++outputX) {
                    const int32_t sourceX = outputX * horizontalScale;
                    scaledFrame[outputY * ROQ_LEGACY_MAX_DIMENSION + outputX] = decodedFrame[sourceY * sourceRowPixels + sourceX];
                }
            }
        }

        rendererExports.StretchRaw((int32_t)x, (int32_t)y, (int32_t)width, (int32_t)height, ROQ_LEGACY_MAX_DIMENSION,
                                   ROQ_LEGACY_MAX_DIMENSION, (const uint8_t *)scaledFrame, handle, qtrue);
        Hunk_FreeTempMemory(scaledFrame);
    } else {
        rendererExports.StretchRaw((int32_t)x, (int32_t)y, (int32_t)width, (int32_t)height, cinematic->displayWidth,
                                   cinematic->displayHeight, cinematic->decodedFrame, handle, cinematic->dirty);
    }

    cinematic->dirty = qfalse;
}

/* Source: CoDUOMP.exe 0x00408d60..0x00408d75.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00408d60_00408d76.mcode.
 * Name and signature: exact same-module Mac symbol SCR_DrawCinematic. */
void SCR_DrawCinematic(void)
{
    if (clientCinematicHandle >= 0 && clientCinematicHandle < MAX_VIDEO_HANDLES) {
        CIN_DrawCinematic(clientCinematicHandle);
    }
}

/* Source: CoDUOMP.exe 0x00408d80..0x00408d95. Ghidra omitted this function
 * from its first-pass function table; its boundary was recovered from the
 * executable-gap record and the adjacent Mac symbol.
 * Name and signature: exact same-module Mac symbol SCR_RunCinematic. */
void SCR_RunCinematic(void)
{
    if (clientCinematicHandle >= 0 && clientCinematicHandle < MAX_VIDEO_HANDLES) {
        (void)CIN_RunCinematic(clientCinematicHandle);
    }
}

/* Source: CoDUOMP.exe 0x00408da0..0x00408dc8. Ghidra omitted this function
 * from its first-pass function table; its boundary was recovered from the
 * executable-gap record and the adjacent Mac symbol.
 * Name and signature: exact same-module Mac symbol SCR_StopCinematic. */
void SCR_StopCinematic(void)
{
    if (clientCinematicHandle >= 0 && clientCinematicHandle < MAX_VIDEO_HANDLES) {
        (void)CIN_StopCinematic(clientCinematicHandle);
        MSS_StopSounds(MSS_STOP_ALL_SOUNDS);
        clientCinematicHandle = -1;
    }
}

/* Source: CoDUOMP.exe 0x00408dd0..0x00408e84. Ghidra omitted this function
 * from its first-pass function table; its boundary was recovered from the
 * executable-gap record and the adjacent Mac symbol.
 * Name and signature: exact same-module Mac symbol CIN_UploadCinematic. */
void CIN_UploadCinematic(int32_t handle)
{
    if (handle < 0 || handle >= MAX_VIDEO_HANDLES)
        return;

    cinematic_t *const cinematic = &cinematics[handle];
    if (cinematic->decodedFrame == NULL)
        return;

    if (cinematic->playOnWalls <= 0 && cinematic->dirty != qfalse) {
        if (cinematic->playOnWalls == CIN_WALL_VIDEO_DISABLED) {
            cinematic->playOnWalls = CIN_WALL_VIDEO_FINAL_UPLOAD;
        } else if (cinematic->playOnWalls == CIN_WALL_VIDEO_FINAL_UPLOAD) {
            cinematic->playOnWalls = CIN_WALL_VIDEO_STOPPED;
        } else {
            cinematic->dirty = qfalse;
        }
    }

    rendererExports.UploadCinematic(ROQ_LEGACY_MAX_DIMENSION, ROQ_LEGACY_MAX_DIMENSION, ROQ_LEGACY_MAX_DIMENSION, ROQ_LEGACY_MAX_DIMENSION,
                                    cinematic->decodedFrame, handle, cinematic->dirty);

    if (r_inGameVideo->integer == 0 && cinematic->playOnWalls == CIN_WALL_VIDEO_ENABLED) {
        cinematic->playOnWalls = CIN_WALL_VIDEO_DISABLED;
    }
}

/* Source: CoDUOMP.exe 0x00408bf0..0x00408d59. Ghidra omitted this function
 * from its first-pass function table; its boundary was recovered from the
 * executable-gap record. Name and signature: exact same-module Mac symbol
 * CL_PlayCinematic_f. The direct sound-fade global stores at 0x00408cfe are
 * the compiler-inlined form of MSS_FadeAllSounds(1.0f, 1). */
void CL_PlayCinematic_f(void)
{
    Com_DPrintf("CL_PlayCinematic_f\n");

    if (cls.state == CA_CINEMATIC) {
        SCR_StopCinematic();
    } else if (cls.state == CA_LOGO) {
        cls.state = CA_DISCONNECTED;
    }

    const int32_t argumentCount = Cmd_Argc();
    const char *const name = argumentCount > CIN_COMMAND_NAME_ARGUMENT ? Cmd_Argv(CIN_COMMAND_NAME_ARGUMENT) : "";
    const char *const option = argumentCount > CIN_COMMAND_OPTION_ARGUMENT ? Cmd_Argv(CIN_COMMAND_OPTION_ARGUMENT) : "";

    int32_t flags = CIN_SYSTEM;
    if ((option != NULL && option[0] == CIN_OPTION_HOLD) ||
        (name != NULL && (Q_stricmp(name, "end.roq") == 0 || Q_stricmp(name, "demoend.roq") == 0))) {
        flags = CIN_SYSTEM | CIN_HOLD;
    }

    if (option != NULL) {
        if (option[0] == CIN_OPTION_LOOP)
            flags |= CIN_LOOP;
        if (option[0] == CIN_OPTION_LETTERBOX)
            flags |= CIN_LETTERBOX;
    }

    if ((flags & CIN_LETTERBOX) != 0) {
        clientCinematicHandle =
            CIN_PlayCinematic(name, 0, CIN_LETTERBOX_SCREEN_Y, CIN_VIRTUAL_SCREEN_WIDTH, CIN_LETTERBOX_SCREEN_HEIGHT, flags);
    } else {
        clientCinematicHandle = CIN_PlayCinematic(name, 0, 0, CIN_VIRTUAL_SCREEN_WIDTH, CIN_VIRTUAL_SCREEN_HEIGHT, flags);
    }

    if (clientCinematicHandle < 0)
        return;

    MSS_StopSounds(MSS_STOP_ALL_SOUNDS);
    MSS_FadeAllSounds(1.0f, CIN_AUDIO_FADE_MSEC);

    do {
        SCR_RunCinematic();
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (currentCinematicHandle < 0)
            break;
    } while (cinematics[currentCinematicHandle].decodedFrame == NULL && cinematics[currentCinematicHandle].status == FMV_PLAY);
}

/* Source: CoDUOMP.exe 0x00405de0..0x00405e1b.
 * Evidence: repaired function record
 * coduomp/mcode/CoDUOMP/FUN_00405de0_00405e1c.mcode.
 * Role name: the Windows-only mono-input/mono-output counterpart of the named
 * Mac RLL decoder variants. */
int32_t RllDecodeMonoToMono(const uint8_t *input, int16_t *output, uint32_t sampleCount, qboolean signedOutput, uint16_t initialSample)
{
    uint16_t predictor = initialSample;
    if (signedOutput)
        predictor = (uint16_t)(predictor - UINT16_C(0x8000));

    for (uint32_t sample = 0; sample < sampleCount; ++sample) {
        predictor = (uint16_t)(predictor + (uint16_t)rllSquareTable[input[sample]]);
        output[sample] = (int16_t)predictor;
    }
    return (int32_t)sampleCount;
}

/* Source: CoDUOMP.exe 0x00405e20..0x00405e5c.
 * Evidence: repaired function record
 * coduomp/mcode/CoDUOMP/FUN_00405e20_00405e5d.mcode.
 * Name: exact same-module Mac symbol RllDecodeMonoToStereo. */
int32_t RllDecodeMonoToStereo(const uint8_t *input, int16_t *output, uint32_t sampleCount, qboolean signedOutput, uint16_t initialSample)
{
    uint16_t predictor = initialSample;
    if (signedOutput)
        predictor = (uint16_t)(predictor - UINT16_C(0x8000));

    for (uint32_t sample = 0; sample < sampleCount; ++sample) {
        predictor = (uint16_t)(predictor + (uint16_t)rllSquareTable[input[sample]]);
        output[sample * 2] = (int16_t)predictor;
        output[sample * 2 + 1] = (int16_t)predictor;
    }
    return (int32_t)sampleCount;
}

/* Source: CoDUOMP.exe 0x00405e60..0x00405ee1.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00405e60_00405ee2.mcode.
 * Name and argument order: same-module Mac symbol
 * RllDecodeStereoToStereo and PowerPC r3..r7 entry state. */
int32_t RllDecodeStereoToStereo(const uint8_t *input, int16_t *output, uint32_t encodedByteCount, qboolean signedOutput,
                                uint16_t initialSamples)
{
    uint16_t leftPredictor = initialSamples & UINT16_C(0xff00);
    uint16_t rightPredictor = (uint16_t)(initialSamples << 8);
    if (signedOutput) {
        leftPredictor = (uint16_t)(leftPredictor - UINT16_C(0x8000));
        rightPredictor = (uint16_t)(rightPredictor - UINT16_C(0x8000));
    }

    for (uint32_t byteIndex = 0; byteIndex < encodedByteCount; byteIndex += 2) {
        leftPredictor = (uint16_t)(leftPredictor + (uint16_t)rllSquareTable[input[byteIndex]]);
        rightPredictor = (uint16_t)(rightPredictor + (uint16_t)rllSquareTable[input[byteIndex + 1]]);
        output[byteIndex] = (int16_t)leftPredictor;
        output[byteIndex + 1] = (int16_t)rightPredictor;
    }
    return (int32_t)(encodedByteCount >> 1);
}

/* Source: CoDUOMP.exe 0x00405ef0..0x00405f89.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00405ef0_00405f8a.mcode.
 * Role name: the Windows-only stereo-input/mono-output counterpart of the
 * named Mac RLL decoder variants. */
int32_t RllDecodeStereoToMono(const uint8_t *input, int16_t *output, uint32_t sampleFrameCount, qboolean signedOutput,
                              uint16_t initialSamples)
{
    uint16_t leftPredictor = initialSamples & UINT16_C(0xff00);
    uint16_t rightPredictor = (uint16_t)(initialSamples << 8);
    if (signedOutput) {
        leftPredictor = (uint16_t)(leftPredictor - UINT16_C(0x8000));
        rightPredictor = (uint16_t)(rightPredictor - UINT16_C(0x8000));
    }

    for (uint32_t frame = 0; frame < sampleFrameCount; ++frame) {
        leftPredictor = (uint16_t)(leftPredictor + (uint16_t)rllSquareTable[input[frame * 2]]);
        rightPredictor = (uint16_t)(rightPredictor + (uint16_t)rllSquareTable[input[frame * 2 + 1]]);
        const int32_t mixed = (int32_t)(int16_t)leftPredictor + (int32_t)(int16_t)rightPredictor;
        output[frame] = (int16_t)(mixed / 2);
    }
    return (int32_t)sampleFrameCount;
}

/* Source: CoDUOMP.exe 0x00405f90..0x00406062.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00405f90_00406063.mcode.
 * Name and argument roles: exact same-module Mac symbol move8_32 and its
 * PowerPC entry registers. Both images copy an 8-by-8 block between surfaces
 * having the same positive row stride. The original rounds that stride down
 * to an eight-byte boundary before copying. */
void move8_32(const uint32_t *source, uint32_t *destination, int32_t rowStrideBytes)
{
    const int32_t alignedStrideBytes = rowStrideBytes - rowStrideBytes % ROQ_COPY_ALIGNMENT_BYTES;
    const size_t rowStride = (size_t)alignedStrideBytes / sizeof(*destination);

    for (size_t row = 0; row < ROQ_BLOCK_8; ++row) {
        for (size_t column = 0; column < ROQ_BLOCK_8; ++column) {
            destination[row * rowStride + column] = source[row * rowStride + column];
        }
    }
}

/* Source: CoDUOMP.exe 0x00406070..0x004060af.
 * Evidence: repaired function record
 * coduomp/mcode/CoDUOMP/FUN_00406070_004060b0.mcode.
 * Name and argument roles: exact same-module Mac symbol move4_32 and its
 * PowerPC entry registers. */
void move4_32(const uint32_t *source, uint32_t *destination, int32_t rowStrideBytes)
{
    const int32_t alignedStrideBytes = rowStrideBytes - rowStrideBytes % ROQ_COPY_ALIGNMENT_BYTES;
    const size_t rowStride = (size_t)alignedStrideBytes / sizeof(*destination);

    for (size_t row = 0; row < ROQ_BLOCK_4; ++row) {
        for (size_t column = 0; column < ROQ_BLOCK_4; ++column) {
            destination[row * rowStride + column] = source[row * rowStride + column];
        }
    }
}

/* Source: CoDUOMP.exe 0x004060b0..0x004061a4.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004060b0_004061a5.mcode.
 * Name and argument roles: exact same-module Mac symbol blit8_32 and its
 * PowerPC entry registers. The input block is tightly packed; only the output
 * uses the aligned surface row stride. */
void blit8_32(const uint32_t *source, uint32_t *destination, int32_t rowStrideBytes)
{
    const int32_t alignedStrideBytes = rowStrideBytes - rowStrideBytes % ROQ_COPY_ALIGNMENT_BYTES;
    const size_t destinationRowStride = (size_t)alignedStrideBytes / sizeof(*destination);

    for (size_t row = 0; row < ROQ_BLOCK_8; ++row) {
        for (size_t column = 0; column < ROQ_BLOCK_8; ++column) {
            destination[row * destinationRowStride + column] = source[row * ROQ_BLOCK_8 + column];
        }
    }
}

/* Source: CoDUOMP.exe 0x004061b0..0x004061f6.
 * Evidence: repaired function record
 * coduomp/mcode/CoDUOMP/FUN_004061b0_004061f7.mcode.
 * Name and argument roles: exact same-module Mac symbol blit4_32 and its
 * PowerPC entry registers. */
void blit4_32(const uint32_t *source, uint32_t *destination, int32_t rowStrideBytes)
{
    const int32_t alignedStrideBytes = rowStrideBytes - rowStrideBytes % ROQ_COPY_ALIGNMENT_BYTES;
    const size_t destinationRowStride = (size_t)alignedStrideBytes / sizeof(*destination);

    for (size_t row = 0; row < ROQ_BLOCK_4; ++row) {
        for (size_t column = 0; column < ROQ_BLOCK_4; ++column) {
            destination[row * destinationRowStride + column] = source[row * ROQ_BLOCK_4 + column];
        }
    }
}

/* Source: CoDUOMP.exe 0x00406200..0x0040620d.
 * Evidence: repaired function record
 * coduomp/mcode/CoDUOMP/FUN_00406200_0040620e.mcode.
 * Name and argument roles: exact same-module Mac symbol blit2_32 and its
 * PowerPC entry registers. */
void blit2_32(const uint32_t *source, uint32_t *destination, int32_t rowStrideBytes)
{
    const int32_t alignedStrideBytes = rowStrideBytes - rowStrideBytes % ROQ_COPY_ALIGNMENT_BYTES;
    const size_t destinationRowStride = (size_t)alignedStrideBytes / sizeof(*destination);

    for (size_t row = 0; row < ROQ_BLOCK_2; ++row) {
        for (size_t column = 0; column < ROQ_BLOCK_2; ++column) {
            destination[row * destinationRowStride + column] = source[row * ROQ_BLOCK_2 + column];
        }
    }
}

/* Source: CoDUOMP.exe 0x00406490..0x0040652e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00406490_0040652f.mcode.
 * Name: exact same-module Mac symbol ROQ_GenYUVTables. The four decimal
 * coefficients reproduce the exact float bit patterns stored by the Windows
 * image; their roles are the U/V contributions to blue, red, and green. */
void ROQ_GenYUVTables(void)
{
    const float ubCoefficient = 57.203998565673828125f;    /* 0x4264d0e5 */
    const float vrCoefficient = 45.3639984130859375f;      /* 0x423574bc */
    const float ugCoefficient = -11.5124797821044921875f;  /* 0xc138331e */
    const float vgCoefficient = -23.3524799346923828125f;  /* 0xc1bad1e1 */

    for (int32_t component = 0; component < 256; ++component) {
        const int32_t centered = component * 2 - 255;
        roqUbTable[component] = (int32_t)((float)centered * ubCoefficient + 32.0f);
        roqVrTable[component] = (int32_t)((float)centered * vrCoefficient + 32.0f);
        roqUgTable[component] = (int32_t)((float)centered * ugCoefficient);
        roqVgTable[component] = (int32_t)((float)centered * vgCoefficient + 32.0f);
        roqYyTable[component] = (component << 6) | (component >> 2);
    }
}

/* Source: CoDUOMP.exe 0x00406530..0x004065a2.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00406530_004065a3.mcode.
 * Name and y/u/v argument order: exact same-module Mac symbol yuv_to_rgb and
 * its PowerPC entry registers. Returns a packed RGB565 pixel. */
uint16_t yuv_to_rgb(int32_t y, int32_t u, int32_t v)
{
    int32_t red = (roqYyTable[y] + roqVrTable[v]) >> 9;
    int32_t green = (roqYyTable[y] + roqUgTable[u] + roqVgTable[v]) >> 8;
    int32_t blue = (roqYyTable[y] + roqUbTable[u]) >> 9;

    if (red < 0)
        red = 0;
    if (green < 0)
        green = 0;
    if (blue < 0)
        blue = 0;
    if (red > 31)
        red = 31;
    if (green > 63)
        green = 63;
    if (blue > 31)
        blue = 31;

    return (uint16_t)((red << 11) | (green << 5) | blue);
}

/* Source: CoDUOMP.exe 0x004065b0..0x0040662f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004065b0_00406630.mcode.
 * Role name: Windows 32-bit-pixel counterpart of yuv_to_rgb. The returned
 * 0xffBBGGRR word yields RGBA byte order in little-endian destination memory. */
uint32_t yuv_to_rgba32(int32_t y, int32_t u, int32_t v)
{
    int32_t red = (roqYyTable[y] + roqVrTable[v]) >> 6;
    int32_t green = (roqYyTable[y] + roqUgTable[u] + roqVgTable[v]) >> 6;
    int32_t blue = (roqYyTable[y] + roqUbTable[u]) >> 6;

    if (red < 0)
        red = 0;
    if (green < 0)
        green = 0;
    if (blue < 0)
        blue = 0;
    if (red > 255)
        red = 255;
    if (green > 255)
        green = 255;
    if (blue > 255)
        blue = 255;

    return UINT32_C(0xff000000) | ((uint32_t)blue << 16) | ((uint32_t)green << 8) | (uint32_t)red;
}

/* Source: CoDUOMP.exe 0x00406210..0x00406481.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00406210_00406482.mcode.
 * Name and argument order: exact same-module Mac symbol blitVQQuad32fs and
 * its PowerPC entry registers. Each little-endian command word supplies eight
 * two-bit commands from its most-significant end. A top-level subdivide command
 * consumes four child entries from the terminated quad-destination list. */
void blitVQQuad32fs(uint32_t *const *quadDestinations, const uint8_t *encodedData)
{
    const int32_t rowStrideBytes = cinematics[currentCinematicHandle].samplesPerLine;
    const size_t rowStridePixels = (size_t)rowStrideBytes / sizeof(**quadDestinations);
    uint16_t commandWord = 0;
    int32_t commandsRemaining = 0;
    size_t quadIndex = 0;

    while (quadDestinations[quadIndex] != NULL) {
        if (commandsRemaining == 0) {
            commandWord = (uint16_t)(encodedData[0] | ((uint16_t)encodedData[1] << 8));
            encodedData += 2;
            commandsRemaining = 7;
        } else {
            --commandsRemaining;
        }

        const uint16_t command = commandWord & ROQ_VQ_SUBDIVIDE;
        commandWord = (uint16_t)(commandWord << 2);
        uint32_t *const destination = quadDestinations[quadIndex];

        switch (command) {
        case ROQ_VQ_SKIP:
            quadIndex += 5;
            break;

        case ROQ_VQ_MOTION: {
            const uint8_t motionIndex = *encodedData++;
            const uint32_t *source = (const uint32_t *)((const uint8_t *)destination + roqMotionOffsets[motionIndex]);
            move8_32(source, destination, rowStrideBytes);
            quadIndex += 5;
            break;
        }

        case ROQ_VQ_VECTOR: {
            const uint8_t codebookIndex = *encodedData++;
            blit8_32(&roqVq8Codebook.normalRgba32[codebookIndex][0][0], destination, rowStrideBytes);
            quadIndex += 5;
            break;
        }

        case ROQ_VQ_SUBDIVIDE:
            ++quadIndex;
            for (int32_t child = 0; child < 4; ++child, ++quadIndex) {
                if (commandsRemaining == 0) {
                    commandWord = (uint16_t)(encodedData[0] | ((uint16_t)encodedData[1] << 8));
                    encodedData += 2;
                    commandsRemaining = 7;
                } else {
                    --commandsRemaining;
                }

                const uint16_t childCommand = commandWord & ROQ_VQ_SUBDIVIDE;
                commandWord = (uint16_t)(commandWord << 2);
                uint32_t *const childDestination = quadDestinations[quadIndex];

                switch (childCommand) {
                case ROQ_VQ_SKIP:
                    break;

                case ROQ_VQ_MOTION: {
                    const uint8_t motionIndex = *encodedData++;
                    const uint32_t *source = (const uint32_t *)((const uint8_t *)childDestination + roqMotionOffsets[motionIndex]);
                    move4_32(source, childDestination, rowStrideBytes);
                    break;
                }

                case ROQ_VQ_VECTOR: {
                    const uint8_t codebookIndex = *encodedData++;
                    blit4_32(&roqVq4Codebook.normalRgba32[codebookIndex][0][0], childDestination, rowStrideBytes);
                    break;
                }

                case ROQ_VQ_SUBDIVIDE: {
                    const uint32_t *const topLeft = &roqVq2Codebook.normalRgba32[encodedData[0]][0][0];
                    const uint32_t *const topRight = &roqVq2Codebook.normalRgba32[encodedData[1]][0][0];
                    const uint32_t *const bottomLeft = &roqVq2Codebook.normalRgba32[encodedData[2]][0][0];
                    const uint32_t *const bottomRight = &roqVq2Codebook.normalRgba32[encodedData[3]][0][0];
                    blit2_32(topLeft, childDestination, rowStrideBytes);
                    blit2_32(topRight, childDestination + 2, rowStrideBytes);
                    blit2_32(bottomLeft, childDestination + rowStridePixels * 2, rowStrideBytes);
                    blit2_32(bottomRight, childDestination + rowStridePixels * 2 + 2, rowStrideBytes);
                    encodedData += 4;
                    break;
                }
                }
            }
            break;
        }
    }
}

/* Source: CoDUOMP.exe 0x004074d0..0x004075cf.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004074d0_004075d0.mcode.
 * Name and arguments: exact same-module Mac symbol recurseQuad and its
 * PowerPC entry registers. The root 16-by-16 tile is structural; stored
 * destinations are its 8-by-8 children and their 4-by-4 children. */
void recurseQuad(int32_t startX, int32_t startY, int32_t quadSize, int32_t xOffset, int32_t yOffset)
{
    cinematic_t *const cinematic = &cinematics[currentCinematicHandle];
    const int32_t effectiveWidth = cinematic->roqWidth < cinematic->width ? cinematic->roqWidth : cinematic->width;
    const int32_t effectiveHeight = cinematic->roqHeight < cinematic->height ? cinematic->roqHeight : cinematic->height;

    if (startX >= 0 && startX + quadSize <= effectiveWidth && startY >= 0 && startY + quadSize <= effectiveHeight &&
        quadSize <= ROQ_MAX_STORED_QUAD_SIZE) {
        const int32_t destinationY = (cinematic->height - effectiveHeight) / 2 + startY + yOffset;
        const int32_t destinationX = startX + xOffset;
        uint8_t *const destinationBytes =
            roqFrameBuffer + destinationY * cinematic->samplesPerLine + destinationX * cinematic->samplesPerPixel;
        uint32_t *const destination = (uint32_t *)destinationBytes;

        roqQuadDestinations[0][cinematic->quadCount] = destination;
        roqQuadDestinations[1][cinematic->quadCount] = (uint32_t *)(destinationBytes + cinematic->frameSize);
        ++cinematic->quadCount;
    }

    if (quadSize == ROQ_MIN_QUAD_SIZE)
        return;

    const int32_t childSize = quadSize / 2;
    recurseQuad(startX, startY, childSize, xOffset, yOffset);
    recurseQuad(startX + childSize, startY, childSize, xOffset, yOffset);
    recurseQuad(startX, startY + childSize, childSize, xOffset, yOffset);
    recurseQuad(startX + childSize, startY + childSize, childSize, xOffset, yOffset);
}

/* Source: CoDUOMP.exe 0x004075d0..0x004076d7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004075d0_004076d8.mcode.
 * Name and arguments: exact same-module Mac symbol setupQuad and its PowerPC
 * entry registers. Sixty-four null entries terminate and guard each rebuilt
 * list exactly as in the executable. */
void setupQuad(int32_t xOffset, int32_t yOffset)
{
    cinematic_t *const cinematic = &cinematics[currentCinematicHandle];
    if (roqQuadSetupCache.xOffset == xOffset && roqQuadSetupCache.yOffset == yOffset && roqQuadSetupCache.height == cinematic->roqHeight &&
        roqQuadSetupCache.width == cinematic->roqWidth) {
        return;
    }

    roqQuadSetupCache.xOffset = xOffset;
    roqQuadSetupCache.yOffset = yOffset;
    roqQuadSetupCache.height = cinematic->roqHeight;
    roqQuadSetupCache.width = cinematic->roqWidth;
    cinematic->quadCount = 0;

    const int32_t sixteenthPixels = (cinematic->roqWidth * cinematic->roqHeight) >> 4;
    const int32_t listEnd = sixteenthPixels + sixteenthPixels / 4 + ROQ_QUAD_TERMINATOR_COUNT;

    for (int32_t startY = 0; startY < cinematic->roqHeight; startY += ROQ_ROOT_QUAD_SIZE) {
        for (int32_t startX = 0; startX < cinematic->roqWidth; startX += ROQ_ROOT_QUAD_SIZE) {
            recurseQuad(startX, startY, ROQ_ROOT_QUAD_SIZE, xOffset, yOffset);
        }
    }

    for (int32_t index = listEnd - ROQ_QUAD_TERMINATOR_COUNT; index < listEnd; ++index) {
        roqQuadDestinations[0][index] = NULL;
        roqQuadDestinations[1][index] = NULL;
    }
}

/* Source: CoDUOMP.exe 0x004076e0..0x0040781a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004076e0_0040781b.mcode.
 * Name and data layout: exact same-module Mac symbol readQuadInfo. The eight
 * input bytes are four little-endian RoQ stream dimensions/limits. */
void readQuadInfo(const uint8_t *data)
{
    if (currentCinematicHandle < 0)
        return;

    cinematic_t *const cinematic = &cinematics[currentCinematicHandle];
    cinematic->roqWidth = (int32_t)(data[0] | ((uint16_t)data[1] << 8));
    cinematic->roqHeight = (int32_t)(data[2] | ((uint16_t)data[3] << 8));
    cinematic->roqMaxSize = (int32_t)(data[4] | ((uint16_t)data[5] << 8));
    cinematic->roqMinSize = (int32_t)(data[6] | ((uint16_t)data[7] << 8));

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    const uint64_t pixelCount = (uint64_t)(uint32_t)cinematic->roqWidth * (uint64_t)(uint32_t)cinematic->roqHeight;
    const uint64_t frameSize = pixelCount * (uint64_t)(uint32_t)cinematic->samplesPerPixel;
    const uint64_t sixteenthPixels = pixelCount >> 4;
    const uint64_t quadListEnd = sixteenthPixels + sixteenthPixels / 4 + ROQ_QUAD_TERMINATOR_COUNT;
    if (cinematic->roqWidth == 0 || cinematic->roqHeight == 0 || frameSize > ROQ_FRAME_BUFFER_BYTES / 2 ||
        quadListEnd > ROQ_QUAD_POINTER_CAPACITY) {
        Com_Printf("WARNING: rejecting RoQ dimensions %i x %i\n", cinematic->roqWidth, cinematic->roqHeight);
        cinematic->looping = qfalse;
        cinematic->status = FMV_EOF;
        return;
    }

    cinematic->width = cinematic->roqWidth;
    cinematic->height = cinematic->roqHeight;
    cinematic->samplesPerLine = cinematic->width * cinematic->samplesPerPixel;
    cinematic->frameSize = cinematic->height * cinematic->samplesPerLine;
    cinematic->half = qfalse;
    cinematic->smoothedDouble = qfalse;
    cinematic->frameBlitters[0] = cinematic->configuredFrameBlitters[0];
    cinematic->frameBlitters[1] = cinematic->configuredFrameBlitters[1];
    cinematic->frameOffsets[0] = cinematic->frameSize;
    cinematic->frameOffsets[1] = -cinematic->frameSize;
    cinematic->displayWidth = cinematic->width;
    cinematic->displayHeight = cinematic->height;

    if (glConfig.vidWidth <= ROQ_LEGACY_MAX_DIMENSION) {
        if (cinematic->displayWidth > ROQ_LEGACY_MAX_DIMENSION)
            cinematic->displayWidth = ROQ_LEGACY_MAX_DIMENSION;
        if (cinematic->displayHeight > ROQ_LEGACY_MAX_DIMENSION)
            cinematic->displayHeight = ROQ_LEGACY_MAX_DIMENSION;
        if (cinematic->width != ROQ_LEGACY_MAX_DIMENSION || cinematic->height != ROQ_LEGACY_MAX_DIMENSION) {
            Com_Printf("HACK: approxmimating cinematic for Rage Pro or Voodoo\n");
        }
    }
}

/* Source: CoDUOMP.exe 0x00407820..0x004078ca.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00407820_004078cb.mcode.
 * Name and argument order: exact same-module Mac symbol RoQPrepMcomp and its
 * PowerPC entry registers. Motion entries remain signed byte displacements,
 * because blitVQQuad32fs applies them to decoded-frame addresses. */
void RoQPrepMcomp(int32_t xOffset, int32_t yOffset)
{
    const cinematic_t *const cinematic = &cinematics[currentCinematicHandle];
    int32_t pixelStride = cinematic->samplesPerPixel;
    int32_t rowStride = cinematic->samplesPerLine;

    if (cinematic->roqWidth == cinematic->roqHeight * 4 && cinematic->half == qfalse) {
        pixelStride *= 2;
        rowStride *= 2;
    }

    const int32_t initialHorizontalOffset = (xOffset - 8) * pixelStride;
    int32_t verticalOffset = (yOffset - 8) * rowStride;

    for (int32_t verticalIndex = 0; verticalIndex < ROQ_MOTION_DIMENSION; ++verticalIndex) {
        int32_t horizontalOffset = initialHorizontalOffset;
        for (int32_t horizontalIndex = 0; horizontalIndex < ROQ_MOTION_DIMENSION; ++horizontalIndex) {
            roqMotionOffsets[horizontalIndex * ROQ_MOTION_DIMENSION + verticalIndex] =
                cinematic->motionBaseOffset - verticalOffset - horizontalOffset;
            horizontalOffset += pixelStride;
        }
        verticalOffset += rowStride;
    }
}

/* Source: CoDUOMP.exe 0x004078d0..0x00407904, recovered from an exporter
 * function-boundary gap.
 * Name: exact same-module Mac symbol initRoQ. The Windows optimizer also
 * emits this initialization inline in CIN_PlayCinematic. */
void initRoQ(void)
{
    if (currentCinematicHandle < 0)
        return;

    cinematic_t *const cinematic = &cinematics[currentCinematicHandle];
    cinematic->configuredFrameBlitters[0] = blitVQQuad32fs;
    cinematic->configuredFrameBlitters[1] = blitVQQuad32fs;
    cinematic->samplesPerPixel = ROQ_PIXEL_BYTES_RGBA32;
    ROQ_GenYUVTables();
    RllSetupTable();
}

/* Source: CoDUOMP.exe 0x00407ee0..0x00407fe8.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00407ee0_00407fe9.mcode.
 * Name and header layout: exact same-module Mac symbol RoQ_init. The Windows
 * build applies com_timescale twice, with an integer truncation after each
 * multiplication; this differs from the Mac build and is retained because the
 * Windows executable is the behavioral authority for this reconstruction. */
void RoQ_init(void)
{
    cinematic_t *const cinematic = &cinematics[currentCinematicHandle];
    int32_t scaledTime = CL_ScaledMilliseconds();
    scaledTime = (int32_t)((float)scaledTime * com_timescale->value);

    cinematic->startTime = scaledTime;
    cinematic->currentTime = scaledTime;
    cinematic->lastTime = cinematic->startTime;
    cinematic->fileOffset = 24;

    cinematic->frameRate = (int32_t)(roqFileBuffer[6] | ((uint16_t)roqFileBuffer[7] << 8));
    if (cinematic->frameRate == 0)
        cinematic->frameRate = 30;

    cinematic->chunkId = (int32_t)(roqFileBuffer[8] | ((uint16_t)roqFileBuffer[9] << 8));
    cinematic->frameNumber = -1;
    cinematic->chunkSize = (int32_t)roqFileBuffer[10] | ((int32_t)roqFileBuffer[11] << 8) | ((int32_t)roqFileBuffer[12] << 16);
    cinematic->chunkArgument = (int32_t)(roqFileBuffer[14] | ((uint16_t)roqFileBuffer[15] << 8));
}

/* Source: CoDUOMP.exe 0x00407910..0x00407998.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00407910_00407999.mcode.
 * Name: exact same-module Mac symbol RoQReset. The original closes and reopens
 * the same file, reloads its 16-byte header, reinitializes stream state, and
 * returns the cinematic to playing status. */
void RoQReset(void)
{
    if (currentCinematicHandle < 0)
        return;

    cinematic_t *const cinematic = &cinematics[currentCinematicHandle];
    FS_FCloseFile(cinematic->fileHandle);
    cinematic->fileHandle = 0;
    fs_fileAccessed = 1;
    (void)FS_FOpenFileRead(cinematic->fileName, &cinematic->fileHandle, qtrue);
    FS_Read(roqFileBuffer, ROQ_FILE_HEADER_BYTES, cinematic->fileHandle);
    RoQ_init();
    cinematic->status = FMV_LOOPED;
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the pixel stores in the
 * nine unrolled format/geometry branches of decodeCodeBook. */
static void RoQ_StoreCodebookPixel(uint8_t *destination, int32_t pixelBytes, uint8_t y, uint8_t u, uint8_t v, const uint8_t *lumaTable)
{
    switch (pixelBytes) {
    case ROQ_PIXEL_BYTES_LUMA:
        destination[0] = lumaTable[y];
        break;

    case ROQ_PIXEL_BYTES_RGB565: {
        const uint16_t pixel = yuv_to_rgb(y, u, v);
        memcpy(destination, &pixel, sizeof(pixel));
        break;
    }

    case ROQ_PIXEL_BYTES_RGBA32: {
        const uint32_t pixel = yuv_to_rgba32(y, u, v);
        memcpy(destination, &pixel, sizeof(pixel));
        break;
    }
    }
}

/* Source: CoDUOMP.exe 0x00406630..0x004074c8.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00406630_004074c9.mcode.
 * Name: exact same-module Mac symbol decodeCodeBook. The executable emits
 * nine separately unrolled paths for 1/2/4-byte pixels combined with normal,
 * horizontally halved, or vertically smoothed geometry. They all implement
 * the same RoQ hierarchy retained below: four VQ2 entries make one VQ4 entry,
 * and each VQ4 pixel is doubled in both axes to make its VQ8 entry. */
void decodeCodeBook(const uint8_t *data, uint16_t chunkArgument)
{
    cinematic_t *const cinematic = &cinematics[currentCinematicHandle];
    int32_t vq2EntryCount;
    int32_t vq4EntryCount;

    if (chunkArgument == 0) {
        vq2EntryCount = ROQ_CODEBOOK_ENTRY_COUNT;
        vq4EntryCount = ROQ_CODEBOOK_ENTRY_COUNT;
    } else {
        vq2EntryCount = (int32_t)(chunkArgument >> ROQ_CODEBOOK_VQ2_COUNT_SHIFT);
        if (vq2EntryCount == 0)
            vq2EntryCount = ROQ_CODEBOOK_ENTRY_COUNT;
        vq4EntryCount = (int32_t)(chunkArgument & ROQ_CODEBOOK_COUNT_MASK);
    }

    const int32_t pixelBytes = cinematic->samplesPerPixel;
    if (pixelBytes != ROQ_PIXEL_BYTES_LUMA && pixelBytes != ROQ_PIXEL_BYTES_RGB565 && pixelBytes != ROQ_PIXEL_BYTES_RGBA32) {
        return;
    }

    int32_t vq2Width;
    int32_t vq2Height;
    if (cinematic->half != qfalse) {
        vq2Width = ROQ_VQ2_HALF_WIDTH;
        vq2Height = ROQ_VQ2_NORMAL_HEIGHT;
    } else if (cinematic->smoothedDouble != qfalse) {
        vq2Width = ROQ_VQ2_NORMAL_WIDTH;
        vq2Height = ROQ_VQ2_SMOOTHED_HEIGHT;
    } else {
        vq2Width = ROQ_VQ2_NORMAL_WIDTH;
        vq2Height = ROQ_VQ2_NORMAL_HEIGHT;
    }

    const int32_t vq2EntryBytes = vq2Width * vq2Height * pixelBytes;
    for (int32_t entry = 0; entry < vq2EntryCount; ++entry) {
        uint8_t luma[ROQ_VQ2_SMOOTHED_PIXEL_COUNT];
        int32_t lumaCount;

        if (cinematic->half != qfalse) {
            luma[0] = data[0];
            luma[1] = data[2];
            lumaCount = ROQ_VQ2_NORMAL_HEIGHT;
        } else if (cinematic->smoothedDouble != qfalse) {
            const int32_t y0 = data[0];
            const int32_t y1 = data[1];
            const int32_t y2 = data[2];
            const int32_t y3 = data[3];
            luma[0] = (uint8_t)y0;
            luma[1] = (uint8_t)y1;
            luma[2] = (uint8_t)((y0 * 3 + y2) / 4);
            luma[3] = (uint8_t)((y1 * 3 + y3) / 4);
            luma[4] = (uint8_t)((y2 * 3 + y0) / 4);
            luma[5] = (uint8_t)((y3 * 3 + y1) / 4);
            luma[6] = (uint8_t)y2;
            luma[7] = (uint8_t)y3;
            lumaCount = ROQ_VQ2_SMOOTHED_PIXEL_COUNT;
        } else {
            luma[0] = data[0];
            luma[1] = data[1];
            luma[2] = data[2];
            luma[3] = data[3];
            lumaCount = ROQ_CODEBOOK_LUMA_COUNT;
        }

        const uint8_t u = data[4];
        const uint8_t v = data[5];
        uint8_t *const destination = roqVq2Codebook.bytes + entry * vq2EntryBytes;
        for (int32_t pixel = 0; pixel < lumaCount; ++pixel) {
            RoQ_StoreCodebookPixel(destination + pixel * pixelBytes, pixelBytes, luma[pixel], u, v, cinematic->lumaTable);
        }
        data += ROQ_CODEBOOK_RECORD_BYTES;
    }

    const int32_t vq4Width = vq2Width * ROQ_VQ_SCALE;
    const int32_t vq4Height = vq2Height * ROQ_VQ_SCALE;
    const int32_t vq4EntryBytes = vq4Width * vq4Height * pixelBytes;
    const int32_t vq8Width = vq4Width * ROQ_VQ_SCALE;
    const int32_t vq8Height = vq4Height * ROQ_VQ_SCALE;
    const int32_t vq8EntryBytes = vq8Width * vq8Height * pixelBytes;

    for (int32_t entry = 0; entry < vq4EntryCount; ++entry) {
        const uint8_t sourceIndices[ROQ_CODEBOOK_INDEX_COUNT] = {data[0], data[1], data[2], data[3]};
        data += ROQ_CODEBOOK_INDEX_COUNT;

        uint8_t *const vq4Destination = roqVq4Codebook.bytes + entry * vq4EntryBytes;
        for (int32_t quadrantY = 0; quadrantY < ROQ_VQ_SCALE; ++quadrantY) {
            for (int32_t quadrantX = 0; quadrantX < ROQ_VQ_SCALE; ++quadrantX) {
                const int32_t sourceIndex = sourceIndices[quadrantY * ROQ_VQ_SCALE + quadrantX];
                const uint8_t *const source = roqVq2Codebook.bytes + sourceIndex * vq2EntryBytes;
                for (int32_t y = 0; y < vq2Height; ++y) {
                    for (int32_t x = 0; x < vq2Width; ++x) {
                        const uint8_t *const sourcePixel = source + (y * vq2Width + x) * pixelBytes;
                        uint8_t *const destinationPixel =
                            vq4Destination + ((quadrantY * vq2Height + y) * vq4Width + quadrantX * vq2Width + x) * pixelBytes;
                        memcpy(destinationPixel, sourcePixel, (size_t)pixelBytes);
                    }
                }
            }
        }

        uint8_t *const vq8Destination = roqVq8Codebook.bytes + entry * vq8EntryBytes;
        for (int32_t y = 0; y < vq4Height; ++y) {
            for (int32_t x = 0; x < vq4Width; ++x) {
                const uint8_t *const sourcePixel = vq4Destination + (y * vq4Width + x) * pixelBytes;
                for (int32_t duplicateY = 0; duplicateY < ROQ_VQ_SCALE; ++duplicateY) {
                    for (int32_t duplicateX = 0; duplicateX < ROQ_VQ_SCALE; ++duplicateX) {
                        uint8_t *const destinationPixel =
                            vq8Destination + (((y * ROQ_VQ_SCALE + duplicateY) * vq8Width) + x * ROQ_VQ_SCALE + duplicateX) * pixelBytes;
                        memcpy(destinationPixel, sourcePixel, (size_t)pixelBytes);
                    }
                }
            }
        }
    }
}

/* Source: CoDUOMP.exe 0x004079a0..0x00407e88.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004079a0_00407e89.mcode.
 * Name and chunk IDs: exact same-module Mac symbol RoQInterrupt and the RoQ
 * dispatch table at PE 0x00407e8c. The Windows mono-audio path deliberately
 * retains its observed mono channel count despite decoding duplicated output. */
void RoQInterrupt(void)
{
    int16_t audioSamples[ROQ_FILE_BUFFER_BYTES / sizeof(int16_t)];

    if (currentCinematicHandle < 0)
        return;

    cinematic_t *cinematic = &cinematics[currentCinematicHandle];
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if ((uint32_t)cinematic->chunkSize > ROQ_MAX_CHUNK_PAYLOAD_BYTES) {
        Com_Printf("WARNING: rejecting oversized RoQ chunk (%i bytes)\n", cinematic->chunkSize);
        cinematic->looping = qfalse;
        cinematic->status = FMV_EOF;
        return;
    }
    const int32_t stagedByteCount = cinematic->chunkSize + ROQ_CHUNK_HEADER_BYTES;
    if (FS_Read(roqFileBuffer, stagedByteCount, cinematic->fileHandle) != stagedByteCount) {
        Com_Printf("WARNING: rejecting truncated RoQ chunk\n");
        cinematic->looping = qfalse;
        cinematic->status = FMV_EOF;
        return;
    }

    cinematic = &cinematics[currentCinematicHandle];
    if (cinematic->fileOffset >= cinematic->fileSize) {
        if (cinematic->holdAtEnd != qfalse) {
            cinematic->status = FMV_IDLE;
        } else if (cinematic->looping != qfalse) {
            RoQReset();
        } else {
            cinematic->status = FMV_EOF;
        }
        return;
    }

    uint8_t *chunkData = roqFileBuffer;
    size_t packetEndOffset = 0;
    for (;;) {
        switch ((roq_chunk_id_t)cinematic->chunkId) {
        case ROQ_QUAD_INFO:
            if (cinematic->frameNumber == -1) {
                if (cinematic->chunkSize < 8) {
                    Com_Printf("WARNING: rejecting truncated RoQ quad info\n");
                    cinematic->looping = qfalse;
                    cinematic->status = FMV_EOF;
                    return;
                }
                readQuadInfo(chunkData);
                cinematic = &cinematics[currentCinematicHandle];
                if (cinematic->status == FMV_EOF)
                    return;
                setupQuad(0, 0);
                int32_t scaledTime = CL_ScaledMilliseconds();
                scaledTime = (int32_t)((float)scaledTime * com_timescale->value);
                cinematic = &cinematics[currentCinematicHandle];
                cinematic->currentTime = scaledTime;
                cinematic->startTime = scaledTime;
            }
            if (cinematic->frameNumber != 1)
                cinematic->frameNumber = 0;
            break;

        case ROQ_CODEBOOK:
            decodeCodeBook(chunkData, (uint16_t)cinematic->chunkArgument);
            break;

        case ROQ_QUAD_VQ:
            if ((cinematic->frameNumber & 1) != 0) {
                cinematic->motionBaseOffset = cinematic->frameOffsets[1];
                RoQPrepMcomp(cinematic->motionXOffset, cinematic->motionYOffset);
                cinematic->frameBlitters[1](roqQuadDestinations[1], chunkData);
                cinematic = &cinematics[currentCinematicHandle];
                cinematic->decodedFrame = roqFrameBuffer + cinematic->frameSize;
            } else {
                cinematic->motionBaseOffset = cinematic->frameOffsets[0];
                RoQPrepMcomp(cinematic->motionXOffset, cinematic->motionYOffset);
                cinematic->frameBlitters[0](roqQuadDestinations[0], chunkData);
                cinematic = &cinematics[currentCinematicHandle];
                cinematic->decodedFrame = roqFrameBuffer;
            }

            if (cinematic->frameNumber == 0) {
                /* The i386 IMUL retains the low 32 bits before Com_Memcpy
                 * widens the byte count to the native size_t. */
                const uint32_t frameBytes = (uint32_t)cinematic->roqHeight * (uint32_t)cinematic->samplesPerLine;
                Com_Memcpy(roqFrameBuffer + cinematic->frameSize, roqFrameBuffer, (size_t)frameBytes);
            }
            ++cinematic->frameNumber;
            cinematic->dirty = qtrue;
            break;

        case ROQ_QUAD_JPEG:
            break;

        case ROQ_QUAD_HANG:
            cinematic->chunkSize = 0;
            break;

        case ROQ_SOUND_MONO:
            if (cinematic->silent == qfalse) {
                /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
                if ((uint32_t)cinematic->chunkSize > ROQ_AUDIO_MONO_MAX_ENCODED_BYTES) {
                    Com_Printf("WARNING: rejecting oversized RoQ mono audio chunk\n");
                    cinematic->looping = qfalse;
                    cinematic->status = FMV_EOF;
                    return;
                }
                const int32_t sampleFrames = RllDecodeMonoToStereo(chunkData, audioSamples, (uint32_t)cinematic->chunkSize, qfalse,
                                                                   (uint16_t)cinematic->chunkArgument);
                MSS_RawSamples(sampleFrames, ROQ_AUDIO_SAMPLE_RATE, ROQ_AUDIO_SAMPLE_WIDTH_BYTES, ROQ_AUDIO_MONO_CHANNELS, audioSamples);
                cinematic = &cinematics[currentCinematicHandle];
                cinematic->rawAudioActive = qtrue;
            }
            break;

        case ROQ_SOUND_STEREO:
            if (cinematic->silent == qfalse) {
                /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
                if ((uint32_t)cinematic->chunkSize > ROQ_AUDIO_STEREO_MAX_ENCODED_BYTES || (cinematic->chunkSize & 1) != 0) {
                    Com_Printf("WARNING: rejecting invalid RoQ stereo audio chunk\n");
                    cinematic->looping = qfalse;
                    cinematic->status = FMV_EOF;
                    return;
                }
                const int32_t sampleFrames = RllDecodeStereoToStereo(chunkData, audioSamples, (uint32_t)cinematic->chunkSize, qfalse,
                                                                     (uint16_t)cinematic->chunkArgument);
                MSS_RawSamples(sampleFrames, ROQ_AUDIO_SAMPLE_RATE, ROQ_AUDIO_SAMPLE_WIDTH_BYTES, ROQ_AUDIO_STEREO_CHANNELS, audioSamples);
                cinematic = &cinematics[currentCinematicHandle];
                cinematic->rawAudioActive = qtrue;
            }
            break;

        case ROQ_PACKET: {
            const size_t packetOffset = (size_t)(chunkData - roqFileBuffer);
            /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
            if (packetOffset > (size_t)stagedByteCount || (uint32_t)cinematic->chunkSize > (size_t)stagedByteCount - packetOffset) {
                Com_Printf("WARNING: rejecting invalid RoQ packet extent\n");
                cinematic->looping = qfalse;
                cinematic->status = FMV_EOF;
                return;
            }
            packetEndOffset = packetOffset + (uint32_t)cinematic->chunkSize;
            cinematic->packetChunkCount = cinematic->chunkArgument;
            cinematic->chunkSize = 0;
            break;
        }

        default:
            cinematic->status = FMV_EOF;
            break;
        }

        cinematic = &cinematics[currentCinematicHandle];
        if (cinematic->fileOffset >= cinematic->fileSize) {
            if (cinematic->holdAtEnd != qfalse) {
                cinematic->status = FMV_IDLE;
            } else if (cinematic->looping != qfalse) {
                RoQReset();
            } else {
                cinematic->status = FMV_EOF;
            }
            return;
        }

        const size_t chunkOffset = (size_t)(chunkData - roqFileBuffer);
        if (chunkOffset > (size_t)stagedByteCount || (uint32_t)cinematic->chunkSize > (size_t)stagedByteCount - chunkOffset ||
            (size_t)stagedByteCount - chunkOffset - (uint32_t)cinematic->chunkSize < ROQ_CHUNK_HEADER_BYTES) {
            Com_Printf("WARNING: rejecting truncated RoQ chunk header\n");
            cinematic->looping = qfalse;
            cinematic->status = FMV_EOF;
            return;
        }
        const size_t headerOffset = chunkOffset + (uint32_t)cinematic->chunkSize;
        chunkData = roqFileBuffer + headerOffset;
        cinematic->chunkId = (int32_t)(chunkData[0] | ((uint16_t)chunkData[1] << 8));
        cinematic->chunkSize = (int32_t)chunkData[2] | ((int32_t)chunkData[3] << 8) | ((int32_t)chunkData[4] << 16);
        cinematic->chunkArgument = (int32_t)(chunkData[6] | ((uint16_t)chunkData[7] << 8));
        cinematic->motionXOffset = (int8_t)chunkData[7];
        cinematic->motionYOffset = (int8_t)chunkData[6];

        if ((uint32_t)cinematic->chunkSize > ROQ_MAX_CHUNK_PAYLOAD_BYTES) {
            Com_Printf("WARNING: rejecting oversized RoQ chunk (%i bytes)\n", cinematic->chunkSize);
            cinematic->looping = qfalse;
            cinematic->status = FMV_EOF;
            return;
        }
        if (cinematic->chunkId == ROQ_FILE) {
            Com_DPrintf("roq_id==0x1084 (roq_size=%i,roq_id=%i)\n", cinematic->chunkSize, cinematic->chunkId);
            cinematic->status = FMV_EOF;
            if (cinematic->looping != qfalse)
                RoQReset();
            return;
        }

        if (cinematic->packetChunkCount != 0 && cinematic->status != FMV_EOF) {
            const size_t payloadOffset = headerOffset + ROQ_CHUNK_HEADER_BYTES;
            if (payloadOffset > packetEndOffset || (uint32_t)cinematic->chunkSize > packetEndOffset - payloadOffset) {
                Com_Printf("WARNING: rejecting RoQ packet subchunk extent\n");
                cinematic->looping = qfalse;
                cinematic->status = FMV_EOF;
                return;
            }
            --cinematic->packetChunkCount;
            chunkData = roqFileBuffer + payloadOffset;
            continue;
        }

        cinematic->fileOffset += cinematic->chunkSize + ROQ_CHUNK_HEADER_BYTES;
        return;
    }
}

/* Source: CoDUOMP.exe 0x00407ff0..0x004080ee.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00407ff0_004080ef.mcode.
 * Name: exact same-module Mac symbol RoQShutdown. The Mac call target proves
 * the conditional audio teardown is MSS_EndRawSamples. */
void RoQShutdown(void)
{
    cinematic_t *cinematic = &cinematics[currentCinematicHandle];
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (cinematic->fileHandle == 0 && cinematic->fileName[0] == '\0' &&
        (cinematic->decodedFrame == NULL || cinematic->status == FMV_IDLE)) {
        return;
    }

    Com_DPrintf("finished cinematic\n");
    cinematic->status = FMV_IDLE;

    if (cinematic->rawAudioActive != qfalse)
        MSS_EndRawSamples();

    if (cinematic->fileHandle != 0) {
        FS_FCloseFile(cinematic->fileHandle);
        cinematic->fileHandle = 0;
    }

    if (cinematic->alterGameState != qfalse) {
        cls.state = CA_DISCONNECTED;
        cvar_t *const nextMap = Cvar_FindVar("nextmap");
        if (nextMap != NULL && nextMap->string[0] != '\0') {
            Cbuf_AddText(va("%s\n", nextMap->string));
            Cvar_Set("nextmap", "");
        }
        clientCinematicHandle = -1;
    }

    cinematic = &cinematics[currentCinematicHandle];
    cinematic->fileName[0] = '\0';
    currentCinematicHandle = -1;
}

/* Source: CoDUOMP.exe 0x004080f0..0x00408167.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004080f0_00408168.mcode.
 * Name and return type: exact same-module Mac symbol CIN_StopCinematic and
 * the shared FMV status enum. */
cinematic_status_t CIN_StopCinematic(int32_t handle)
{
    if (handle < 0 || handle >= MAX_VIDEO_HANDLES)
        return FMV_EOF;

    cinematic_t *const cinematic = &cinematics[handle];
    if (cinematic->status == FMV_EOF)
        return FMV_EOF;

    currentCinematicHandle = handle;
    Com_DPrintf("trFMV::stop(), closing %s\n", cinematic->fileName);
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */

    if (cinematic->alterGameState != qfalse && cls.state != CA_CINEMATIC)
        return cinematic->status;

    cinematic->status = FMV_EOF;
    RoQShutdown();
    return FMV_EOF;
}

/* Source: CoDUOMP.exe 0x00408170..0x004083b9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00408170_004083ba.mcode.
 * Name and return type: exact same-module Mac symbol CIN_RunCinematic and the
 * shared FMV status enum. Frame arithmetic deliberately retains the original
 * 32-bit wrapping products before division. */
cinematic_status_t CIN_RunCinematic(int32_t handle)
{
    if (handle < 0 || handle >= MAX_VIDEO_HANDLES)
        return FMV_EOF;

    cinematic_t *cinematic = &cinematics[handle];
    if (cinematic->status == FMV_EOF)
        return FMV_EOF;

    if (lastCinematicHandle != handle) {
        currentCinematicHandle = handle;
        lastCinematicHandle = handle;
        cinematic->status = FMV_EOF;
        RoQReset();
    }

    if (cinematic->playOnWalls < CIN_WALL_VIDEO_FINAL_UPLOAD)
        return cinematic->status;

    currentCinematicHandle = handle;
    if (cinematic->alterGameState != qfalse && cls.state != CA_CINEMATIC)
        return cinematic->status;
    if (cinematic->status == FMV_IDLE)
        return FMV_IDLE;

    const int32_t now = CL_ScaledMilliseconds();
    if (cinematic->shader != qfalse) {
        const int32_t timeDelta = (int32_t)((uint32_t)now - (uint32_t)cinematic->currentTime);
        const int32_t absoluteTimeDelta = timeDelta < 0 ? -timeDelta : timeDelta;
        if (absoluteTimeDelta > 100) {
            cinematic->startTime = (int32_t)((uint32_t)cinematic->startTime + (uint32_t)timeDelta);
        }
    }

    int32_t targetFrame;
    if (cinematic->rawAudioActive != qfalse) {
        const int32_t rawAudioTime = MSS_RawSamplesTime();
        const int32_t scaledAudioTime = (int32_t)((uint32_t)rawAudioTime * (uint32_t)cinematic->frameRate);
        targetFrame = scaledAudioTime / 1000 + 1;
    } else {
        const uint32_t elapsed = (uint32_t)now - (uint32_t)cinematic->startTime;
        targetFrame = (int32_t)((elapsed * (uint32_t)cinematic->frameRate) / UINT32_C(1000));
    }

    if (cinematic->targetFrame < targetFrame) {
        cinematic->targetFrame = targetFrame;
        cinematic->lastTime = now;
    } else {
        const uint32_t stalledTime = (uint32_t)now - (uint32_t)cinematic->lastTime;
        if (stalledTime * (uint32_t)cinematic->frameRate > UINT32_C(4000))
            cinematic->status = FMV_EOF;
    }

    int32_t decoderStartTime = cinematic->startTime;
    while (cinematic->targetFrame != cinematic->frameNumber && cinematic->status == FMV_PLAY) {
        RoQInterrupt();
        cinematic = &cinematics[currentCinematicHandle];
        if (decoderStartTime != cinematic->startTime) {
            const uint32_t elapsed = (uint32_t)CL_ScaledMilliseconds() - (uint32_t)cinematic->startTime;
            cinematic->targetFrame = (int32_t)((elapsed * (uint32_t)cinematic->frameRate) / UINT32_C(1000));
            decoderStartTime = cinematic->startTime;
        }
    }

    cinematic = &cinematics[currentCinematicHandle];
    cinematic->currentTime = now;
    if (cinematic->status == FMV_LOOPED)
        cinematic->status = FMV_PLAY;
    if (cinematic->status == FMV_EOF) {
        if (cinematic->looping != qfalse) {
            RoQReset();
            cinematic = &cinematics[currentCinematicHandle];
        } else {
            RoQShutdown();
            /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
            return FMV_IDLE;
        }
    }
    return cinematic->status;
}
