#include "backend.h"

#include "gl_api.h"
#include "gl_state.h"
#include "output_gamma_compat.h"
#include "platform_gamma.h"
#include "../math/vector_math.h"
#include "qcommon/com_sprintf.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <jpeglib.h>

#if !defined(JCS_EXTENSIONS)
#error "CoDUOMP JPEG recovery requires libjpeg-turbo RGBX input support"
#endif

enum {
    R_SCREENSHOT_PATH_SIZE = 256,
    R_MAX_SCREENSHOT_NUMBER = 9999,
    R_SCREENSHOT_NUMBER_LIMIT = R_MAX_SCREENSHOT_NUMBER + 1,
    TGA_HEADER_SIZE = 18,
    TGA_HEADER_IMAGE_TYPE_OFFSET = 2,
    TGA_HEADER_WIDTH_LOW_OFFSET = 12,
    TGA_HEADER_WIDTH_HIGH_OFFSET = 13,
    TGA_HEADER_HEIGHT_LOW_OFFSET = 14,
    TGA_HEADER_HEIGHT_HIGH_OFFSET = 15,
    TGA_HEADER_PIXEL_DEPTH_OFFSET = 16,
    TGA_IMAGE_TYPE_UNCOMPRESSED_TRUECOLOR = 2,
    TGA_TRUECOLOR_PIXEL_DEPTH = 24,
    TGA_BYTES_PER_PIXEL = 3,
    JPEG_BYTES_PER_PIXEL = 4,
    JPEG_SCREENSHOT_QUALITY = 95,
    JPEG_PATH_PRIME_BYTES = 1,
    R_LEVELSHOT_SOURCE_WIDTH = 512,
    R_LEVELSHOT_SOURCE_HEIGHT = 384,
    R_LEVELSHOT_WIDTH = 128,
    R_LEVELSHOT_HEIGHT = 128,
    R_DOWNSAMPLE_BLOCK_WIDTH = 4,
    R_DOWNSAMPLE_BLOCK_HEIGHT = 3,
    R_DOWNSAMPLE_BLOCK_SAMPLES =
        R_DOWNSAMPLE_BLOCK_WIDTH * R_DOWNSAMPLE_BLOCK_HEIGHT,
    R_LEVELSHOT_PIXEL_BYTES =
        R_LEVELSHOT_WIDTH * R_LEVELSHOT_HEIGHT * TGA_BYTES_PER_PIXEL,
    R_LEVELSHOT_FILE_BYTES = TGA_HEADER_SIZE + R_LEVELSHOT_PIXEL_BYTES,
    R_SAVEGAME_SOURCE_WIDTH = 2048,
    R_SAVEGAME_SOURCE_HEIGHT = 1536,
    R_SAVEGAME_WIDTH = 512,
    R_SAVEGAME_HEIGHT = 512,
    R_SAVEGAME_BYTES_PER_PIXEL = 4,
    R_SAVEGAME_PIXEL_BYTES =
        R_SAVEGAME_WIDTH * R_SAVEGAME_HEIGHT * R_SAVEGAME_BYTES_PER_PIXEL,
    R_SAVEGAME_JPEG_QUALITY = 90,
    R_CUBEMAP_FIRST_FACE = 1,
    R_CUBEMAP_FACE_COUNT = 6,
    R_CUBEMAP_BYTES_PER_SOURCE_PIXEL = 3,
    R_CUBEMAP_BYTES_PER_OUTPUT_PIXEL = 4
};

typedef struct renderer_jpeg_destination_s {
    struct jpeg_destination_mgr callbacks; /* original +0x00 */
    JOCTET *buffer;                         /* original +0x14 */
    size_t capacity;                        /* original +0x18 */
} renderer_jpeg_destination_t;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(renderer_jpeg_destination_t) == 0x4,
               "renderer_jpeg_destination_t original alignment");
_Static_assert(offsetof(renderer_jpeg_destination_t, callbacks) == 0x00,
               "renderer_jpeg_destination_t callbacks offset");
_Static_assert(sizeof(((renderer_jpeg_destination_t *)0)->callbacks) == 0x14,
               "renderer_jpeg_destination_t callbacks extent");
_Static_assert(offsetof(struct jpeg_destination_mgr, next_output_byte) == 0x00,
               "jpeg destination next_output_byte original offset");
_Static_assert(offsetof(struct jpeg_destination_mgr, free_in_buffer) == 0x04,
               "jpeg destination free_in_buffer original offset");
_Static_assert(offsetof(struct jpeg_destination_mgr, init_destination) == 0x08,
               "jpeg destination init_destination original offset");
_Static_assert(offsetof(struct jpeg_destination_mgr, empty_output_buffer) ==
                   0x0c,
               "jpeg destination empty_output_buffer original offset");
_Static_assert(offsetof(struct jpeg_destination_mgr, term_destination) == 0x10,
               "jpeg destination term_destination original offset");
_Static_assert(offsetof(renderer_jpeg_destination_t, buffer) == 0x14,
               "renderer_jpeg_destination_t buffer offset");
_Static_assert(sizeof(((renderer_jpeg_destination_t *)0)->buffer) == 0x04,
               "renderer_jpeg_destination_t buffer extent");
_Static_assert(offsetof(renderer_jpeg_destination_t, capacity) == 0x18,
               "renderer_jpeg_destination_t capacity offset");
_Static_assert(sizeof(((renderer_jpeg_destination_t *)0)->capacity) == 0x04,
               "renderer_jpeg_destination_t capacity extent");
_Static_assert(sizeof(renderer_jpeg_destination_t) == 0x1c,
               "renderer_jpeg_destination_t original size");
#endif

/* Original 0x0388bed0. jpeg_finish_compress invokes JpegTermDestination before
 * SaveJPG passes the completed byte count to FS_WriteFile. */
static uint32_t rendererJpegOutputSize;

/* NOT_FROM_ORIGINAL_SOURCE_STORAGE: records exhaustion of SaveJPG's fixed
 * destination so the callback can provide a safe discard sink and the caller
 * can reject the incomplete file. The original JPEG path is not reentrant. */
static qboolean coduompJpegOutputOverflowed;

/* Source: CoDUOMP.exe 0x005073f0..0x00507402, recovered from the executable-gap
 * ledger. Role and signature are proved by jpegDest's destination-manager
 * callback table and the standard libjpeg destination contract. */
static void JpegInitDestination(j_compress_ptr compressor)
{
    renderer_jpeg_destination_t *destination =
        (renderer_jpeg_destination_t *)compressor->dest;

    destination->callbacks.next_output_byte = destination->buffer;
    destination->callbacks.free_in_buffer = destination->capacity;
}

/* Source: CoDUOMP.exe 0x00507410..0x00507412, recovered from the executable-gap
 * ledger. */
static boolean JpegEmptyOutputBuffer(j_compress_ptr compressor)
{
    renderer_jpeg_destination_t *destination =
        (renderer_jpeg_destination_t *)compressor->dest;

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    coduompJpegOutputOverflowed = qtrue;
    destination->callbacks.next_output_byte = destination->buffer;
    destination->callbacks.free_in_buffer = destination->capacity;
    return TRUE;
}

/* Source: CoDUOMP.exe 0x00507420..0x00507433, recovered from the executable-gap
 * ledger. The byte count is capacity minus libjpeg's unused tail. */
static void JpegTermDestination(j_compress_ptr compressor)
{
    const renderer_jpeg_destination_t *destination =
        (const renderer_jpeg_destination_t *)compressor->dest;

    rendererJpegOutputSize =
        (uint32_t)(destination->capacity -
                   destination->callbacks.free_in_buffer);
}

/* Source: CoDUOMP.exe 0x00507440..0x0050747d, recovered from the executable-gap
 * ledger. Name: exact same-module Mac symbol jpegDest. Native libjpeg owns the
 * destination-manager object's layout; only its caller-supplied output buffer
 * and capacity are engine state. */
static void jpegDest(j_compress_ptr compressor, JOCTET *buffer,
                     size_t capacity)
{
    if (compressor->dest == NULL) {
        compressor->dest =
            (struct jpeg_destination_mgr *)(*compressor->mem->alloc_small)(
                (j_common_ptr)compressor, JPOOL_PERMANENT,
                sizeof(renderer_jpeg_destination_t));
    }

    renderer_jpeg_destination_t *destination =
        (renderer_jpeg_destination_t *)compressor->dest;
    destination->callbacks.init_destination = JpegInitDestination;
    destination->callbacks.empty_output_buffer = JpegEmptyOutputBuffer;
    destination->callbacks.term_destination = JpegTermDestination;
    destination->buffer = buffer;
    destination->capacity = capacity;
    coduompJpegOutputOverflowed = qfalse;
}

/* Source: CoDUOMP.exe 0x004c19d0..0x004c1a0a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c19d0_004c1a0a.mcode.
 * This complete function was omitted from Ghidra's function set and recovered
 * from the executable-gap ledger. Name and two-argument source signature:
 * exact same-module Mac R_ScreenshotFilename symbol and its caller. */
void R_ScreenshotFilename(int32_t screenshotNumber, char *fileName)
{
    if (screenshotNumber < 0 ||
        screenshotNumber > R_MAX_SCREENSHOT_NUMBER) {
        Com_sprintf(fileName, R_SCREENSHOT_PATH_SIZE,
                    "screenshots/shot9999.tga");
        return;
    }

    Com_sprintf(fileName, R_SCREENSHOT_PATH_SIZE,
                "screenshots/shot%04i.tga", screenshotNumber);
}

/* Source: CoDUOMP.exe 0x004c1a10..0x004c1a4a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c1a10_004c1a4a.mcode.
 * This second complete gap function is the JPEG counterpart, proved by its
 * two embedded path strings and exact Mac R_ScreenshotFilenameJPEG symbol. */
void R_ScreenshotFilenameJPEG(int32_t screenshotNumber, char *fileName)
{
    if (screenshotNumber < 0 ||
        screenshotNumber > R_MAX_SCREENSHOT_NUMBER) {
        Com_sprintf(fileName, R_SCREENSHOT_PATH_SIZE,
                    "screenshots/shot9999.jpg");
        return;
    }

    Com_sprintf(fileName, R_SCREENSHOT_PATH_SIZE,
                "screenshots/shot%04i.jpg", screenshotNumber);
}

/* Source: CoDUOMP.exe 0x004c2d60..0x004c2f53.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c2d60_004c2f53.mcode.
 * Name and command-level structure: exact same-module Mac R_ScreenShot_f
 * symbol. Windows instructions prove all command-import calls, filename
 * limits, print conditions, and the TGA-specific last-number comparison. */
void R_ScreenShot_f(void)
{
    static int32_t lastNumber = -1;
    char fileName[R_SCREENSHOT_PATH_SIZE];
    qboolean silent;

    if (strcmp(ri.Cmd_Argv(1), "levelshot") == 0) {
        R_LevelShot();
        return;
    }

    if (strcmp(ri.Cmd_Argv(1), "savegame") == 0 &&
        ri.Cmd_Argc() == 3 && ri.Cmd_Argv(2)[0] != '\0') {
        R_SaveGameShot(ri.Cmd_Argv(2));
        return;
    }

    silent = strcmp(ri.Cmd_Argv(1), "silent") == 0;
    if (ri.Cmd_Argc() == 2 && silent == qfalse) {
        Com_sprintf(fileName, sizeof(fileName),
                    "screenshots/%s.tga", ri.Cmd_Argv(1));
    } else {
        if (lastNumber == -1)
            lastNumber = 0;

        while (lastNumber <= R_MAX_SCREENSHOT_NUMBER) {
            R_ScreenshotFilename(lastNumber, fileName);
            if (ri.FS_FileExists(fileName) == qfalse)
                break;
            ++lastNumber;
        }

        /* The Windows and Mac TGA bodies both use >= 9999 here, so the
         * automatic TGA sequence intentionally never writes shot9999.tga.
         * The adjacent JPEG handler has the distinct == 10000 check below. */
        if (lastNumber >= R_MAX_SCREENSHOT_NUMBER) {
            ri.Printf(R_PRINT_ALL,
                      "ScreenShot: Couldn't create a file\n");
            return;
        }
        ++lastNumber;
    }

    R_TakeScreenshot(0, 0, glConfig.vidWidth, glConfig.vidHeight, fileName);
    if (silent == qfalse)
        ri.Printf(R_PRINT_ALL, "Wrote %s\n", fileName);
}

/* Source: CoDUOMP.exe 0x004c2f60..0x004c3160.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c2f60_004c3160.mcode.
 * Exact same-module Mac R_ScreenShotJPEG_f symbol corroborates the duplicated
 * command structure. Windows proves the separate JPEG counter, extension,
 * capture helper, and its equality-only 10000 exhaustion test. */
void R_ScreenShotJPEG_f(void)
{
    static int32_t lastNumber = -1;
    char fileName[R_SCREENSHOT_PATH_SIZE];
    qboolean silent;

    if (strcmp(ri.Cmd_Argv(1), "levelshot") == 0) {
        R_LevelShot();
        return;
    }

    if (strcmp(ri.Cmd_Argv(1), "savegame") == 0 &&
        ri.Cmd_Argc() == 3 && ri.Cmd_Argv(2)[0] != '\0') {
        R_SaveGameShot(ri.Cmd_Argv(2));
        return;
    }

    silent = strcmp(ri.Cmd_Argv(1), "silent") == 0;
    if (ri.Cmd_Argc() == 2 && silent == qfalse) {
        Com_sprintf(fileName, sizeof(fileName),
                    "screenshots/%s.jpg", ri.Cmd_Argv(1));
    } else {
        if (lastNumber == -1)
            lastNumber = 0;

        while (lastNumber <= R_MAX_SCREENSHOT_NUMBER) {
            R_ScreenshotFilenameJPEG(lastNumber, fileName);
            if (ri.FS_FileExists(fileName) == qfalse)
                break;
            lastNumber = (int32_t)((uint32_t)lastNumber + 1u);
        }

        if (lastNumber == R_SCREENSHOT_NUMBER_LIMIT) {
            ri.Printf(R_PRINT_ALL,
                      "ScreenShot: Couldn't create a file\n");
            return;
        }
        lastNumber = (int32_t)((uint32_t)lastNumber + 1u);
    }

    R_TakeScreenshotJPEG(0, 0, glConfig.vidWidth, glConfig.vidHeight,
                         fileName);
    if (silent == qfalse)
        ri.Printf(R_PRINT_ALL, "Wrote %s\n", fileName);
}

/* Source: CoDUOMP.exe 0x004c1a50..0x004c1d15.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c1a50_004c1d15.mcode.
 * Name and source-level R_GammaCorrect boundary: exact same-module Mac
 * R_LevelShot symbol and call graph. Windows MSVC inlines the 49,152-byte
 * gamma-table pass and uses its static float-to-int helper for the two positive
 * scaled sample coordinates; ordinary C casts preserve that truncation. */
void R_LevelShot(void)
{
    long double xScale;
    long double yScale;
    char fileName[R_SCREENSHOT_PATH_SIZE];
    uint8_t *source;
    uint8_t *tga;
    uint8_t *destination;
    int32_t outputX;
    int32_t outputY;

    sprintf(fileName, "levelshots/%s.tga", tr.world->baseName);

    const uint32_t sourceByteCount =
        (uint32_t)glConfig.vidWidth *
        (uint32_t)glConfig.vidHeight *
        (uint32_t)TGA_BYTES_PER_PIXEL;
    source = ri.Hunk_AllocateTempMemory((size_t)sourceByteCount);
    tga = ri.Hunk_AllocateTempMemory(R_LEVELSHOT_FILE_BYTES);
    memset(tga, 0, TGA_HEADER_SIZE);
    tga[TGA_HEADER_IMAGE_TYPE_OFFSET] = TGA_IMAGE_TYPE_UNCOMPRESSED_TRUECOLOR;
    tga[TGA_HEADER_WIDTH_LOW_OFFSET] = R_LEVELSHOT_WIDTH;
    tga[TGA_HEADER_HEIGHT_LOW_OFFSET] = R_LEVELSHOT_HEIGHT;
    tga[TGA_HEADER_PIXEL_DEPTH_OFFSET] = TGA_TRUECOLOR_PIXEL_DEPTH;

    /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): when the composited
     * output presentation owns the presented image, GL_FRONT holds the
     * scaled hardware drawable, not this render-sized frame. */
    if (coduomp_capture_presented_frame_compat(
            0, 0, glConfig.vidWidth, glConfig.vidHeight,
            GL_RGB, source) == qfalse)
    {
        qglReadBuffer(GL_FRONT);
        qglReadPixels(0, 0, glConfig.vidWidth, glConfig.vidHeight,
                      GL_RGB, GL_UNSIGNED_BYTE, source);
        qglReadBuffer(GL_BACK);
    }

    xScale = (long double)glConfig.vidWidth *
             (long double)0.001953125f; /* 1 / 512 */
    yScale = (long double)glConfig.vidHeight *
             (long double)0.0026041667442768812f; /* 1 / 384 */

    destination = tga + TGA_HEADER_SIZE;
    for (outputY = 0;
         outputY < R_LEVELSHOT_SOURCE_HEIGHT;
         outputY += R_DOWNSAMPLE_BLOCK_HEIGHT) {
        for (outputX = 0;
             outputX < R_LEVELSHOT_SOURCE_WIDTH;
             outputX += R_DOWNSAMPLE_BLOCK_WIDTH) {
            int32_t red = 0;
            int32_t green = 0;
            int32_t blue = 0;
            int32_t sampleY;

            for (sampleY = 0;
                 sampleY < R_DOWNSAMPLE_BLOCK_HEIGHT;
                 ++sampleY) {
                int32_t sampleX;

                for (sampleX = 0;
                     sampleX < R_DOWNSAMPLE_BLOCK_WIDTH;
                     ++sampleX) {
                    const int32_t sourceX =
                        (int32_t)(
                            (long double)(outputX + sampleX) * xScale);
                    const int32_t sourceY =
                        (int32_t)(
                            (long double)(outputY + sampleY) * yScale);
                    const uint32_t sourcePixel =
                        (uint32_t)sourceY *
                            (uint32_t)glConfig.vidWidth +
                        (uint32_t)sourceX;
                    const uint8_t *sample =
                        source + sourcePixel *
                                     (uint32_t)TGA_BYTES_PER_PIXEL;

                    red += sample[0];
                    green += sample[1];
                    blue += sample[2];
                }
            }

            *destination++ =
                (uint8_t)(blue / R_DOWNSAMPLE_BLOCK_SAMPLES);
            *destination++ =
                (uint8_t)(green / R_DOWNSAMPLE_BLOCK_SAMPLES);
            *destination++ =
                (uint8_t)(red / R_DOWNSAMPLE_BLOCK_SAMPLES);
        }
    }

    if (tr.overbrightBits > 0 &&
        coduomp_gamma_output_available() != qfalse) {
        R_GammaCorrect(tga + TGA_HEADER_SIZE, R_LEVELSHOT_PIXEL_BYTES);
    }

    ri.FS_WriteFile(fileName, tga, R_LEVELSHOT_FILE_BYTES);
    ri.Hunk_FreeTempMemory(tga);
    ri.Hunk_FreeTempMemory(source);
    ri.Printf(R_PRINT_ALL, "Wrote %s\n", fileName);
}

/* Source: CoDUOMP.exe 0x004c1d20..0x004c1f7f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c1d20_004c1f7f.mcode.
 * Name, one-argument source signature, and R_GammaCorrect/SaveJPG boundaries:
 * exact same-module Mac R_SaveGameShot symbol and call graph. The Windows body
 * inlines the complete 1,048,576-byte gamma pass and uses the static float-to-
 * int helper for positive sample coordinates. */
void R_SaveGameShot(const char *name)
{
    char fileName[R_SCREENSHOT_PATH_SIZE];
    uint8_t *source;
    uint8_t *output;
    uint8_t *destination;
    long double xScale;
    long double yScale;
    int32_t outputX;
    int32_t outputY;

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if (strlen(name) > sizeof(fileName) - sizeof(".jpg")) {
        ri.Printf(R_PRINT_ALL,
                  "WARNING: savegame screenshot filename is too long\n");
        return;
    }
    Com_sprintf(fileName, sizeof(fileName), "%s.jpg", name);

    const uint32_t sourceByteCount =
        (uint32_t)glConfig.vidWidth *
        (uint32_t)glConfig.vidHeight *
        (uint32_t)R_SAVEGAME_BYTES_PER_PIXEL;
    source = ri.Hunk_AllocateTempMemory((size_t)sourceByteCount);
    /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): capture the presented
     * render-sized frame when the output compositor owns GL_FRONT. */
    if (coduomp_capture_presented_frame_compat(
            0, 0, glConfig.vidWidth, glConfig.vidHeight,
            GL_RGBA, source) == qfalse)
    {
        qglReadBuffer(GL_FRONT);
        qglReadPixels(0, 0, glConfig.vidWidth, glConfig.vidHeight,
                      GL_RGBA, GL_UNSIGNED_BYTE, source);
        qglReadBuffer(GL_BACK);
    }

    output = ri.Hunk_AllocateTempMemory(R_SAVEGAME_PIXEL_BYTES);
    xScale = (long double)glConfig.vidWidth *
             (long double)0.00048828125f; /* 1 / 2048 */
    yScale = (long double)glConfig.vidHeight *
             (long double)0.0006510416860692203f; /* 1 / 1536 */

    destination = output;
    for (outputY = 0;
         outputY < R_SAVEGAME_SOURCE_HEIGHT;
         outputY += R_DOWNSAMPLE_BLOCK_HEIGHT) {
        for (outputX = 0;
             outputX < R_SAVEGAME_SOURCE_WIDTH;
             outputX += R_DOWNSAMPLE_BLOCK_WIDTH) {
            int32_t red = 0;
            int32_t green = 0;
            int32_t blue = 0;
            int32_t sampleY;

            for (sampleY = 0;
                 sampleY < R_DOWNSAMPLE_BLOCK_HEIGHT;
                 ++sampleY) {
                int32_t sampleX;

                for (sampleX = 0;
                     sampleX < R_DOWNSAMPLE_BLOCK_WIDTH;
                     ++sampleX) {
                    const int32_t sourceX =
                        (int32_t)(
                            (long double)(outputX + sampleX) * xScale);
                    const int32_t sourceY =
                        (int32_t)(
                            (long double)(outputY + sampleY) * yScale);
                    const uint32_t sourcePixel =
                        (uint32_t)sourceY *
                            (uint32_t)glConfig.vidWidth +
                        (uint32_t)sourceX;
                    const uint8_t *sample =
                        source + sourcePixel *
                                     (uint32_t)
                                         R_SAVEGAME_BYTES_PER_PIXEL;

                    red += sample[0];
                    green += sample[1];
                    blue += sample[2];
                }
            }

            destination[0] =
                (uint8_t)(red / R_DOWNSAMPLE_BLOCK_SAMPLES);
            destination[1] =
                (uint8_t)(green / R_DOWNSAMPLE_BLOCK_SAMPLES);
            destination[2] =
                (uint8_t)(blue / R_DOWNSAMPLE_BLOCK_SAMPLES);
            destination[3] = 0;
            destination += R_SAVEGAME_BYTES_PER_PIXEL;
        }
    }

    if (tr.overbrightBits > 0 &&
        coduomp_gamma_output_available() != qfalse) {
        R_GammaCorrect(output, R_SAVEGAME_PIXEL_BYTES);
    }

    SaveJPG(fileName, R_SAVEGAME_JPEG_QUALITY,
            R_SAVEGAME_WIDTH, R_SAVEGAME_HEIGHT,
            output, qtrue);
    ri.Hunk_FreeTempMemory(output);
    ri.Hunk_FreeTempMemory(source);
}

/* Source: CoDUOMP.exe 0x004c1f80..0x004c25e8.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c1f80_004c25e8.mcode.
 * Name and five-argument boundary: exact same-module Mac RE_CubemapShot
 * symbol and CL_CubemapShot_f call sites. The Windows caller's suffix table
 * proves face values 1..6 are up, down, left, right, front, and back. */
void RE_CubemapShot(const char *fileName, int32_t faceSize,
                    cubemap_face_t face,
                    float fresnelN0, float fresnelN1)
{
    /* The original automatic initializer contains a deliberately unused
     * zero entry because public cubemap face indices begin at one. */
    const vec3_t faceAxes[R_CUBEMAP_FACE_COUNT + R_CUBEMAP_FIRST_FACE][3] = {
        {
            { 0.0f,  0.0f,  0.0f},
            { 0.0f,  0.0f,  0.0f},
            { 0.0f,  0.0f,  0.0f}
        },
        {
            { 0.0f,  0.0f,  1.0f},
            { 0.0f,  1.0f,  0.0f},
            {-1.0f,  0.0f,  0.0f}
        },
        {
            { 0.0f,  0.0f, -1.0f},
            { 0.0f,  1.0f,  0.0f},
            { 1.0f,  0.0f,  0.0f}
        },
        {
            {-1.0f,  0.0f,  0.0f},
            { 0.0f, -1.0f,  0.0f},
            { 0.0f,  0.0f,  1.0f}
        },
        {
            { 1.0f,  0.0f,  0.0f},
            { 0.0f,  1.0f,  0.0f},
            { 0.0f,  0.0f,  1.0f}
        },
        {
            { 0.0f, -1.0f,  0.0f},
            { 1.0f,  0.0f,  0.0f},
            { 0.0f,  0.0f,  1.0f}
        },
        {
            { 0.0f,  1.0f,  0.0f},
            {-1.0f,  0.0f,  0.0f},
            { 0.0f,  0.0f,  1.0f}
        }
    };
    const uint32_t pixelCount =
        (uint32_t)faceSize * (uint32_t)faceSize;
    const uint32_t sourceByteCount =
        pixelCount * (uint32_t)R_CUBEMAP_BYTES_PER_SOURCE_PIXEL;
    const uint32_t fileByteCount =
        pixelCount * (uint32_t)R_CUBEMAP_BYTES_PER_OUTPUT_PIXEL +
        (uint32_t)TGA_HEADER_SIZE;
    uint8_t *source =
        ri.Hunk_AllocateTempMemory((size_t)sourceByteCount);
    uint8_t *tga = ri.Hunk_AllocateTempMemory((size_t)fileByteCount);
    const int32_t halfSize = faceSize / 2;
    const float faceDistance = (float)halfSize - 0.5f;
    uint8_t *destination;
    int32_t row;

    memset(tga, 0, TGA_HEADER_SIZE);
    tga[TGA_HEADER_IMAGE_TYPE_OFFSET] = TGA_IMAGE_TYPE_UNCOMPRESSED_TRUECOLOR;
    tga[TGA_HEADER_WIDTH_LOW_OFFSET] = (uint8_t)faceSize;
    tga[TGA_HEADER_WIDTH_HIGH_OFFSET] = (uint8_t)(faceSize >> 8);
    tga[TGA_HEADER_HEIGHT_LOW_OFFSET] = (uint8_t)faceSize;
    tga[TGA_HEADER_HEIGHT_HIGH_OFFSET] = (uint8_t)(faceSize >> 8);
    tga[TGA_HEADER_PIXEL_DEPTH_OFFSET] = 32;

    qglFinish();
    qglReadPixels(1, glConfig.vidHeight - faceSize - 1,
                  faceSize, faceSize, GL_RGB, GL_UNSIGNED_BYTE, source);

    if (tr.overbrightBits > 0 &&
        coduomp_gamma_output_available() != qfalse) {
        R_GammaCorrect(source, (int32_t)sourceByteCount);
    }

    destination = tga + TGA_HEADER_SIZE;
    for (row = 0; row < faceSize; ++row) {
        const int32_t y = row - halfSize;
        const float yCoordinate = (float)y + 0.5f;
        int32_t column;

        for (column = 0; column < faceSize; ++column) {
            const int32_t x = column - halfSize;
            /* 0x4c2426..0x4c245e uses the retained x87 sum for direction X,
             * then reloads its float copy for direction Y and Z. */
            const long double xCoordinateRaw =
                (long double)x + 0.5L;
            const float xCoordinate =
                (float)xCoordinateRaw;
            const uint32_t sourcePixelIndex =
                (uint32_t)row * (uint32_t)faceSize +
                (uint32_t)column;
            const uint8_t *sourcePixel =
                source + sourcePixelIndex *
                             (uint32_t)
                                 R_CUBEMAP_BYTES_PER_SOURCE_PIXEL;
            vec3_t direction;
            double incidentAngle;
            double refractedAngle;
            double angleDifference;
            double angleSum;
            double sPolarized;
            double pPolarized;
            float reflectionFactor;
            float scaledAlpha;
            int32_t component;

            for (component = 0; component < 3; ++component) {
                const long double componentX =
                    component == 0
                        ? xCoordinateRaw
                        : (long double)xCoordinate;
                direction[component] = (float)(
                    (long double)faceAxes[face][0][component] *
                        (long double)faceDistance +
                    (long double)faceAxes[face][1][component] *
                        componentX +
                    (long double)faceAxes[face][2][component] *
                        (long double)yCoordinate);
            }
            (void)VectorNormalize(direction);

            if (direction[2] > 0.0f) {
                /* 0x4c24a2..0x4c24cc stores the acos result as double but
                 * feeds the retained x87 value directly to FSIN. */
                const long double incidentAngleRaw =
                    acosl((long double)direction[2]);
                incidentAngle = (double)incidentAngleRaw;
                refractedAngle = (double)asinl(
                    ((long double)fresnelN0 /
                     (long double)fresnelN1) *
                    sinl(incidentAngleRaw));
            } else {
                const long double incidentAngleRaw =
                    acosl(-(long double)direction[2]);
                incidentAngle = (double)incidentAngleRaw;
                refractedAngle = (double)asinl(
                    ((long double)fresnelN1 /
                     (long double)fresnelN0) *
                    sinl(incidentAngleRaw));
            }

            angleDifference = incidentAngle - refractedAngle;
            angleSum = incidentAngle + refractedAngle;
            sPolarized = sin(angleDifference) / sin(angleSum);
            pPolarized = tan(angleDifference) / tan(angleSum);
            reflectionFactor = (float)(
                0.5 * (sPolarized * sPolarized + pPolarized * pPolarized));
            if (reflectionFactor < 0.0f)
                reflectionFactor = 0.0f;
            else if (reflectionFactor > 1.0f)
                reflectionFactor = 1.0f;

            destination[0] = sourcePixel[2];
            destination[1] = sourcePixel[1];
            destination[2] = sourcePixel[0];
            scaledAlpha = reflectionFactor * 255.0f;
            destination[3] = (uint8_t)lrint(
                (double)scaledAlpha + 9.31322574615478515625e-10);
            destination += R_CUBEMAP_BYTES_PER_OUTPUT_PIXEL;
        }
    }

    ri.FS_WriteFile(fileName, tga, (int32_t)fileByteCount);
    ri.Hunk_FreeTempMemory(tga);
    ri.Hunk_FreeTempMemory(source);
}

/* Source: CoDUOMP.exe 0x004c25f0..0x004c2d5c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c25f0_004c2d5c.mcode.
 * This complete function was absent from Ghidra's original function set.
 * Name and five-argument boundary: exact same-module Mac
 * RE_CubemapWaterShot symbol and the Windows CL_CubemapShot_f call through
 * renderer export slot 0x04958164. The command's r0/g0/b0 and r90/g90/b90
 * vectors are the horizon and zenith colors selected by direction.z. */
void RE_CubemapWaterShot(const char *fileName, int32_t faceSize,
                         cubemap_face_t face,
                         const vec3_t horizonColor,
                         const vec3_t zenithColor)
{
    /* Like RE_CubemapShot, the original automatic initializer includes the
     * deliberately unused face-zero entry. Keeping a local table reflects the
     * two separate source functions proved by both Windows bodies. */
    const vec3_t faceAxes[R_CUBEMAP_FACE_COUNT + R_CUBEMAP_FIRST_FACE][3] = {
        {
            { 0.0f,  0.0f,  0.0f},
            { 0.0f,  0.0f,  0.0f},
            { 0.0f,  0.0f,  0.0f}
        },
        {
            { 0.0f,  0.0f,  1.0f},
            { 0.0f,  1.0f,  0.0f},
            {-1.0f,  0.0f,  0.0f}
        },
        {
            { 0.0f,  0.0f, -1.0f},
            { 0.0f,  1.0f,  0.0f},
            { 1.0f,  0.0f,  0.0f}
        },
        {
            {-1.0f,  0.0f,  0.0f},
            { 0.0f, -1.0f,  0.0f},
            { 0.0f,  0.0f,  1.0f}
        },
        {
            { 1.0f,  0.0f,  0.0f},
            { 0.0f,  1.0f,  0.0f},
            { 0.0f,  0.0f,  1.0f}
        },
        {
            { 0.0f, -1.0f,  0.0f},
            { 1.0f,  0.0f,  0.0f},
            { 0.0f,  0.0f,  1.0f}
        },
        {
            { 0.0f,  1.0f,  0.0f},
            {-1.0f,  0.0f,  0.0f},
            { 0.0f,  0.0f,  1.0f}
        }
    };
    const uint32_t fileByteCount =
        ((uint32_t)faceSize * (uint32_t)faceSize +
         (uint32_t)(TGA_HEADER_SIZE / TGA_BYTES_PER_PIXEL)) *
        (uint32_t)TGA_BYTES_PER_PIXEL;
    uint8_t *tga =
        ri.Hunk_AllocateTempMemory((size_t)fileByteCount);
    const int32_t halfSize = faceSize / 2;
    const float faceDistance = (float)halfSize - 0.5f;
    const float maximumColorComponent = 2.0f;
    const float byteScale = 255.0f;
    const double integerRoundingBias = 9.31322574615478515625e-10;
    uint8_t *destination;
    int32_t row;

    memset(tga, 0, TGA_HEADER_SIZE);
    tga[TGA_HEADER_IMAGE_TYPE_OFFSET] = TGA_IMAGE_TYPE_UNCOMPRESSED_TRUECOLOR;
    tga[TGA_HEADER_WIDTH_LOW_OFFSET] = (uint8_t)faceSize;
    tga[TGA_HEADER_WIDTH_HIGH_OFFSET] = (uint8_t)(faceSize >> 8);
    tga[TGA_HEADER_HEIGHT_LOW_OFFSET] = (uint8_t)faceSize;
    tga[TGA_HEADER_HEIGHT_HIGH_OFFSET] = (uint8_t)(faceSize >> 8);
    tga[TGA_HEADER_PIXEL_DEPTH_OFFSET] = TGA_TRUECOLOR_PIXEL_DEPTH;

    destination = tga + TGA_HEADER_SIZE;
    for (row = 0; row < faceSize; ++row) {
        const int32_t y = row - halfSize;
        const float yCoordinate = (float)y + 0.5f;
        int32_t column;

        for (column = 0; column < faceSize; ++column) {
            const int32_t x = column - halfSize;
            const long double xCoordinateRaw =
                (long double)x + 0.5L;
            const float xCoordinate =
                (float)xCoordinateRaw;
            const renderer_light_t *sunLight = tr.world->sunLight;
            vec3_t direction;
            vec3_t color;
            float height;
            int32_t component;

            for (component = 0; component < 3; ++component) {
                const long double componentX =
                    component == 0
                        ? xCoordinateRaw
                        : (long double)xCoordinate;
                direction[component] = (float)(
                    (long double)faceAxes[face][0][component] *
                        (long double)faceDistance +
                    (long double)faceAxes[face][1][component] *
                        componentX +
                    (long double)faceAxes[face][2][component] *
                        (long double)yCoordinate);
            }
            (void)VectorNormalize(direction);

            height = direction[2];
            if (height < 0.0f)
                height = 0.0f;

            for (component = 0; component < 3; ++component) {
                color[component] =
                    horizonColor[component] * (1.0f - height) +
                    zenithColor[component] * height;
            }

            if (sunLight != NULL) {
                float sunAmount =
                    direction[0] * sunLight->position[0] +
                    direction[1] * sunLight->position[1] +
                    direction[2] * sunLight->position[2];

                if (sunAmount < 0.0f)
                    sunAmount = 0.0f;

                for (component = 0; component < 3; ++component) {
                    const float lightFactor =
                        sunLight->ambient[component] +
                        sunLight->diffuse[component] +
                        sunAmount * sunLight->diffuse[component];

                    color[component] *= lightFactor;
                }
            }

            for (component = 0; component < 3; ++component) {
                if (color[component] < 0.0f)
                    color[component] = 0.0f;
                else if (color[component] > maximumColorComponent)
                    color[component] = maximumColorComponent;
            }

            destination[0] = (uint8_t)lrint(
                (double)(color[2] * byteScale) + integerRoundingBias);
            destination[1] = (uint8_t)lrint(
                (double)(color[1] * byteScale) + integerRoundingBias);
            destination[2] = (uint8_t)lrint(
                (double)(color[0] * byteScale) + integerRoundingBias);
            destination += TGA_BYTES_PER_PIXEL;
        }
    }

    ri.FS_WriteFile(fileName, tga, (int32_t)fileByteCount);
    ri.Hunk_FreeTempMemory(tga);
}

/* Source: CoDUOMP.exe 0x00504870..0x00504893.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00504870_00504893.mcode.
 * Name and two-argument source boundary: exact same-module Mac symbol
 * R_GammaCorrect and its calls from the screenshot/cubemap paths. MSVC carries
 * buffer in ECX and byteCount in ESI in the Windows body. */
void R_GammaCorrect(uint8_t *buffer, int32_t byteCount)
{
    int32_t byteIndex;

    for (byteIndex = 0; byteIndex < byteCount; ++byteIndex)
        buffer[byteIndex] =
            rendererGammaOverbrightTable[buffer[byteIndex]];
}

/* Source: CoDUOMP.exe 0x004c1800..0x004c191e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c1800_004c191e.mcode.
 * Name, five-argument source signature, and R_GammaCorrect helper boundary:
 * same-module Mac R_TakeScreenshot symbol, call graph, and caller. Windows
 * MSVC carries width in EAX, inlines the 18-byte memset and R_GammaCorrect,
 * and leaves a six-byte alignment NOP at 0x004c18ea..0x004c18ef. */
void R_TakeScreenshot(int32_t x, int32_t y,
                      int32_t width, int32_t height,
                      const char *fileName)
{
    uint32_t allocationSize;
    int32_t fileSize;
    int32_t byteOffset;
    uint8_t *tga;
    uint8_t *pixels;

    /* The original reserves a full-screen RGB payload even when the requested
     * rectangle is smaller. Preserve its exact multiply/add ordering. */
    allocationSize =
        ((uint32_t)glConfig.vidWidth * (uint32_t)glConfig.vidHeight +
         TGA_HEADER_SIZE / TGA_BYTES_PER_PIXEL) * TGA_BYTES_PER_PIXEL;
    tga = ri.Hunk_AllocateTempMemory((size_t)allocationSize);
    memset(tga, 0, TGA_HEADER_SIZE);

    tga[TGA_HEADER_IMAGE_TYPE_OFFSET] = TGA_IMAGE_TYPE_UNCOMPRESSED_TRUECOLOR;
    tga[TGA_HEADER_WIDTH_LOW_OFFSET] = (uint8_t)width;
    tga[TGA_HEADER_WIDTH_HIGH_OFFSET] = (uint8_t)(width >> 8);
    tga[TGA_HEADER_HEIGHT_LOW_OFFSET] = (uint8_t)height;
    tga[TGA_HEADER_HEIGHT_HIGH_OFFSET] = (uint8_t)(height >> 8);
    tga[TGA_HEADER_PIXEL_DEPTH_OFFSET] = TGA_TRUECOLOR_PIXEL_DEPTH;

    pixels = tga + TGA_HEADER_SIZE;
    /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): capture the presented
     * render-sized frame when the output compositor owns GL_FRONT. */
    if (coduomp_capture_presented_frame_compat(
            x, y, width, height, GL_RGB, pixels) == qfalse)
    {
        qglReadBuffer(GL_FRONT);
        qglReadPixels(x, y, width, height,
                      GL_RGB, GL_UNSIGNED_BYTE, pixels);
        qglReadBuffer(GL_BACK);
    }

    fileSize = (int32_t)(
        ((uint32_t)width * (uint32_t)height +
         TGA_HEADER_SIZE / TGA_BYTES_PER_PIXEL) * TGA_BYTES_PER_PIXEL);
    for (byteOffset = TGA_HEADER_SIZE;
         byteOffset < fileSize;
         byteOffset += TGA_BYTES_PER_PIXEL) {
        const uint8_t red = tga[byteOffset];

        tga[byteOffset] = tga[byteOffset + 2];
        tga[byteOffset + 2] = red;
    }

    if (tr.overbrightBits > 0 &&
        coduomp_gamma_output_available() != qfalse) {
        const int32_t gammaByteCount = (int32_t)(
            (uint32_t)glConfig.vidWidth *
            (uint32_t)glConfig.vidHeight * TGA_BYTES_PER_PIXEL);
        R_GammaCorrect(pixels, gammaByteCount);
    }

    ri.FS_WriteFile(fileName, tga, fileSize);
    ri.Hunk_FreeTempMemory(tga);
}

/* Source: CoDUOMP.exe 0x00507480..0x0050774f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00507480_00507750.mcode.
 * Name and source boundary: exact same-module Mac symbol SaveJPG. The original
 * embeds an IJG build configured for four-byte RGB input; the maintained
 * libjpeg-turbo boundary requests the equivalent RGBX layout explicitly. */
void SaveJPG(const char *fileName, int32_t quality,
             int32_t imageWidth, int32_t imageHeight,
             uint8_t *imageBuffer, qboolean flipVertical)
{
    struct jpeg_error_mgr errorManager;
    struct jpeg_compress_struct compressor;
    memset(&compressor, 0, sizeof(compressor));
    compressor.err = jpeg_std_error(&errorManager);
    jpeg_create_compress(&compressor);

    const uint32_t outputCapacity =
        (uint32_t)imageWidth * (uint32_t)imageHeight *
        JPEG_BYTES_PER_PIXEL;
    JOCTET *outputBuffer;
#if UINTPTR_MAX > UINT32_MAX
    /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): retail i386 allocates
     * this raw-frame-sized JPEG destination from the temporary hunk while its
     * caller's capture buffer is still live. Native 64-bit clients expose
     * modern framebuffer sizes for which those two overlapping allocations
     * can exhaust the original fixed hunk. Keep the proved capacity,
     * compression, write, and cleanup ordering, but place only the destination
     * buffer in the independently owned zone allocation on native-wide builds.
     * Original-width builds retain the exact hunk path. */
    outputBuffer = ri.Z_Malloc(outputCapacity);
#else
    outputBuffer = ri.Hunk_AllocateTempMemory(outputCapacity);
#endif
    jpegDest(&compressor, outputBuffer, outputCapacity);

    compressor.image_width = (JDIMENSION)imageWidth;
    compressor.image_height = (JDIMENSION)imageHeight;
    compressor.input_components = JPEG_BYTES_PER_PIXEL;
    compressor.in_color_space = JCS_EXT_RGBX;
    jpeg_set_defaults(&compressor);
    jpeg_set_quality(&compressor, quality, TRUE);
    jpeg_start_compress(&compressor, TRUE);

    const uint32_t rowBytes =
        (uint32_t)imageWidth * JPEG_BYTES_PER_PIXEL;
    while (compressor.next_scanline < compressor.image_height) {
        JDIMENSION sourceRow = compressor.next_scanline;
        if (flipVertical != qfalse) {
            sourceRow =
                compressor.image_height - 1 - compressor.next_scanline;
        }

        JSAMPROW scanline = imageBuffer + sourceRow * rowBytes;
        (void)jpeg_write_scanlines(&compressor, &scanline, 1);
    }

    jpeg_finish_compress(&compressor);
    if (coduompJpegOutputOverflowed == qfalse) {
        ri.FS_WriteFile(fileName, outputBuffer,
                        (int32_t)rendererJpegOutputSize);
    } else {
        ri.Printf(R_PRINT_WARNING,
                  "WARNING: JPEG output for '%s' exceeded its buffer\n",
                  fileName);
    }
#if UINTPTR_MAX > UINT32_MAX
    ri.Z_Free(outputBuffer);
#else
    ri.Hunk_FreeTempMemory(outputBuffer);
#endif
    jpeg_destroy_compress(&compressor);
}

/* Source: CoDUOMP.exe 0x004c1920..0x004c19d0.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c1920_004c19d0.mcode.
 * Name and five-argument source signature: exact same-module Mac symbol
 * R_TakeScreenshotJPEG and its caller. Windows MSVC carries fileName in EBX,
 * inlines the R_GammaCorrect condition, and passes imageWidth to SaveJPG in
 * ECX. The one-byte FS_WriteFile call is present on both architectures before
 * SaveJPG and is therefore retained as original behavior. */
void R_TakeScreenshotJPEG(int32_t x, int32_t y,
                          int32_t width, int32_t height,
                          const char *fileName)
{
    const uint32_t byteCountBits =
        (uint32_t)glConfig.vidWidth *
        (uint32_t)glConfig.vidHeight *
        (uint32_t)JPEG_BYTES_PER_PIXEL;
    const int32_t byteCount = (int32_t)byteCountBits;
    uint8_t *pixels;
#if UINTPTR_MAX > UINT32_MAX
    /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): retail i386 places the
     * full-frame capture in the temporary hunk. Native 64-bit clients can use
     * modern framebuffer sizes larger than the hunk headroom remaining after
     * a map load. Match SaveJPG's native-wide scratch ownership so neither
     * full-frame JPEG buffer depends on that fixed hunk. */
    pixels = ri.Z_Malloc((size_t)byteCountBits);
#else
    pixels = ri.Hunk_AllocateTempMemory((size_t)byteCountBits);
#endif

    /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): capture the presented
     * render-sized frame when the output compositor owns GL_FRONT. */
    if (coduomp_capture_presented_frame_compat(
            x, y, width, height, GL_RGBA, pixels) == qfalse)
    {
        qglReadBuffer(GL_FRONT);
        qglReadPixels(x, y, width, height,
                      GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        qglReadBuffer(GL_BACK);
    }

    if (tr.overbrightBits > 0 &&
        coduomp_gamma_output_available() != qfalse) {
        R_GammaCorrect(pixels, byteCount);
    }

    ri.FS_WriteFile(fileName, pixels, JPEG_PATH_PRIME_BYTES);
    SaveJPG(fileName, JPEG_SCREENSHOT_QUALITY,
            glConfig.vidWidth, glConfig.vidHeight,
            pixels, qtrue);
#if UINTPTR_MAX > UINT32_MAX
    ri.Z_Free(pixels);
#else
    ri.Hunk_FreeTempMemory(pixels);
#endif
}
