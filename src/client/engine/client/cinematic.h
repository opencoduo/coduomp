#ifndef CODUOMP_CLIENT_CINEMATIC_H
#define CODUOMP_CLIENT_CINEMATIC_H

#include <stddef.h>
#include <stdint.h>

#include "../q_shared.h"

enum {
    MAX_VIDEO_HANDLES = 16,
    CINEMATIC_NAME_SIZE = 256
};

typedef enum cinematic_flag_e {
    CIN_SYSTEM = 1 << 0,
    CIN_LOOP = 1 << 1,
    CIN_HOLD = 1 << 2,
    CIN_SILENT = 1 << 3,
    CIN_SHADER = 1 << 4,
    CIN_LETTERBOX = 1 << 5
} cinematic_flag_t;

/* playOnWalls states used by CIN_UploadCinematic. Disabling wall video permits
 * one final dirty upload before CIN_RunCinematic stops advancing the stream. */
typedef enum cinematic_wall_state_e {
    CIN_WALL_VIDEO_STOPPED = -2,
    CIN_WALL_VIDEO_FINAL_UPLOAD = -1,
    CIN_WALL_VIDEO_DISABLED = 0,
    CIN_WALL_VIDEO_ENABLED = 1
} cinematic_wall_state_t;

typedef void (*roq_vq_blitter_t)(uint32_t *const *quadDestinations,
                                 const uint8_t *encodedData);

/* CoDUOMP.exe and the PowerPC Mac client independently use 16 records at an
 * original 0x1d0-byte stride. Named producers and consumers prove every field;
 * the two QUAD_INFO size limits are store-only metadata as noted below. */
typedef struct cinematic_s {
    char fileName[CINEMATIC_NAME_SIZE]; /* original +0x000 */
    int32_t width;                      /* original +0x100 */
    int32_t height;                     /* original +0x104 */
    int32_t screenX;                    /* original +0x108 */
    int32_t screenY;                    /* original +0x10c */
    int32_t screenWidth;                /* original +0x110 */
    int32_t screenHeight;               /* original +0x114 */
    qboolean looping;                   /* original +0x118 */
    qboolean holdAtEnd;                 /* original +0x11c */
    qboolean dirty;                     /* original +0x120 */
    qboolean alterGameState;            /* original +0x124 */
    qboolean silent;                    /* original +0x128 */
    qboolean shader;                    /* original +0x12c */
    qboolean letterBox;                 /* original +0x130 */
    qboolean rawAudioActive;             /* original +0x134 */
    int32_t fileHandle;                 /* original +0x138 */
    cinematic_status_t status;          /* original +0x13c */
    int32_t startTime;                  /* original +0x140 */
    int32_t lastTime;                   /* original +0x144 */
    int32_t currentTime;                /* original +0x148 */
    int32_t targetFrame;                /* original +0x14c */
    int32_t fileOffset;                 /* original +0x150 */
    int32_t fileSize;                   /* original +0x154 */
    int32_t chunkSize;                  /* original +0x158 */
    int32_t quadCount;                  /* original +0x15c */
    int32_t frameNumber;                /* original +0x160 */
    int32_t samplesPerLine;             /* original +0x164 */
    int32_t chunkId;                    /* original +0x168 */
    int32_t frameSize;                  /* original +0x16c */
    roq_vq_blitter_t frameBlitters[2];  /* original +0x170 */
    roq_vq_blitter_t configuredFrameBlitters[2]; /* original +0x178 */
    int32_t samplesPerPixel;            /* original +0x180 */
    const uint8_t *lumaTable;           /* original +0x184 */
    int32_t roqWidth;                   /* original +0x188 */
    int32_t roqHeight;                  /* original +0x18c */
    int32_t roqMaxSize; /* +0x190; stored metadata unused by CoDUOMP.exe. */
    int32_t roqMinSize; /* +0x194; stored metadata unused by CoDUOMP.exe. */
    qboolean half;                      /* original +0x198 */
    qboolean smoothedDouble;            /* original +0x19c */
    int32_t packetChunkCount;           /* original +0x1a0 */
    int32_t motionBaseOffset;           /* original +0x1a4 */
    int32_t chunkArgument;              /* original +0x1a8 */
    int32_t motionXOffset;              /* original +0x1ac */
    int32_t motionYOffset;              /* original +0x1b0 */
    int32_t frameOffsets[2];            /* original +0x1b4 */
    int32_t frameRate;                  /* original +0x1bc */
    int32_t playOnWalls;                /* cinematic_wall_state_t, +0x1c0 */
    uint8_t *decodedFrame;              /* original +0x1c4 */
    int32_t displayWidth;               /* original +0x1c8 */
    int32_t displayHeight;              /* original +0x1cc */
} cinematic_t;

extern cinematic_t cinematics[MAX_VIDEO_HANDLES];

int32_t CIN_HandleForVideo(void);
int32_t CIN_PlayCinematic(const char *name, int32_t x, int32_t y,
                          int32_t width, int32_t height, int32_t flags);
void CIN_SetExtents(int32_t handle, int32_t x, int32_t y,
                    int32_t width, int32_t height);
void CIN_SetLooping(int32_t handle, qboolean looping);
void CIN_DrawCinematic(int32_t handle);
void CIN_UploadCinematic(int32_t handle);
void CIN_CloseAllVideos(void);
cinematic_status_t CIN_StopCinematic(int32_t handle);
cinematic_status_t CIN_RunCinematic(int32_t handle);
void SCR_DrawCinematic(void);
void SCR_RunCinematic(void);
void SCR_StopCinematic(void);
void CL_PlayCinematic_f(void);

void RllSetupTable(void);
int32_t RllDecodeMonoToMono(const uint8_t *input, int16_t *output,
                            uint32_t sampleCount, qboolean signedOutput,
                            uint16_t initialSample);
int32_t RllDecodeMonoToStereo(const uint8_t *input, int16_t *output,
                              uint32_t sampleCount, qboolean signedOutput,
                              uint16_t initialSample);
int32_t RllDecodeStereoToStereo(const uint8_t *input, int16_t *output,
                                uint32_t encodedByteCount,
                                qboolean signedOutput,
                                uint16_t initialSamples);
int32_t RllDecodeStereoToMono(const uint8_t *input, int16_t *output,
                              uint32_t sampleFrameCount,
                              qboolean signedOutput,
                              uint16_t initialSamples);

void blit2_32(const uint32_t *source, uint32_t *destination,
              int32_t rowStrideBytes);
void blit4_32(const uint32_t *source, uint32_t *destination,
              int32_t rowStrideBytes);
void blit8_32(const uint32_t *source, uint32_t *destination,
              int32_t rowStrideBytes);
void move4_32(const uint32_t *source, uint32_t *destination,
              int32_t rowStrideBytes);
void move8_32(const uint32_t *source, uint32_t *destination,
              int32_t rowStrideBytes);

void ROQ_GenYUVTables(void);
uint16_t yuv_to_rgb(int32_t y, int32_t u, int32_t v);
uint32_t yuv_to_rgba32(int32_t y, int32_t u, int32_t v);
void blitVQQuad32fs(uint32_t *const *quadDestinations,
                    const uint8_t *encodedData);
void recurseQuad(int32_t startX, int32_t startY, int32_t quadSize,
                 int32_t xOffset, int32_t yOffset);
void setupQuad(int32_t xOffset, int32_t yOffset);
void readQuadInfo(const uint8_t *data);
void RoQPrepMcomp(int32_t xOffset, int32_t yOffset);
void initRoQ(void);
void RoQ_init(void);
void RoQReset(void);
void RoQShutdown(void);
void RoQInterrupt(void);
void decodeCodeBook(const uint8_t *data, uint16_t chunkArgument);

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(cinematic_t) == 464,
               "i386 cinematic record size changed");
_Static_assert(offsetof(cinematic_t, samplesPerLine) == 0x164,
               "i386 cinematic sample-stride offset changed");
_Static_assert(offsetof(cinematic_t, targetFrame) == 0x14c,
               "i386 cinematic target-frame offset changed");
_Static_assert(offsetof(cinematic_t, lumaTable) == 0x184,
               "i386 cinematic luma-table offset changed");
_Static_assert(offsetof(cinematic_t, playOnWalls) == 0x1c0,
               "i386 cinematic play-on-walls offset changed");
_Static_assert(offsetof(cinematic_t, roqWidth) == 0x188,
               "i386 cinematic RoQ-width offset changed");
_Static_assert(offsetof(cinematic_t, displayHeight) == 0x1cc,
               "i386 cinematic display-height offset changed");
#endif

#endif
