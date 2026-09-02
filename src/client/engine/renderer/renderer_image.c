#include "backend.h"
#include "platform_gamma.h"

#include "../math/vector_math.h"
#include "../platform/crt_boundary.h"
#include "gl_api.h"
#include "gl_state.h"
#include "qcommon/com_sprintf.h"
#include "compat/crt/qsort_compat.h"

#include <math.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <jpeglib.h>

#if !defined(JCS_EXTENSIONS)
#error "CoDUOMP JPEG recovery requires libjpeg-turbo RGBX output support"
#endif

enum {
    R_IMAGE_NAME_SIZE = 64,
    R_IMAGE_HASH_SIZE = 4096,
    R_IMAGE_FIRST_TEXTURE_ID = 1024,
    R_IMAGE_HASH_CHARACTER_WEIGHT = 119,
    R_CUBE_FACE_COUNT = 6,
    R_RENORMALIZE_CUBE_SIZE = 64,
    R_PIXEL_COMPONENT_COUNT = 4,
    /* Original storage 0x038878c0..0x038879bf: 32 eight-byte i386 allocation
     * records. The following 0x100 bytes belong to rendererGammaTable; the
     * active allocation count is discontiguous at 0x0389fec0. */
    R_MAX_IMAGE_ALLOCATIONS = 32
};

enum {
    R_DEFAULT_IMAGE_SIZE = 16,
    R_BUILTIN_IMAGE_SIZE = 16,
    R_BUILTIN_UPLOAD_SIZE = 8,
    R_DLIGHT_IMAGE_SIZE = 64,
    R_LIGHTMAP_SIZE = 512,
    R_LIGHTMAP_SOURCE_COMPONENTS = 3,
    R_LIGHTMAP_DESTINATION_COMPONENTS = 4
};

enum {
    R_USED_IMAGE_WARNING_BYTES = 20 * 1024 * 1024,
    R_USED_IMAGE_WARNING_STATMON_ENTRY = 6,
    R_USED_IMAGE_WARNING_DURATION_MSEC = 3000
};

static const char rendererUsedImageWarningShader[] = "gfx/2d/warning@textures.jpg";

/* LIFO ownership record used by R_FreeImageAllocations. kind selects the
 * matching file-buffer or hunk-temp-memory release API; memory is the exact
 * pointer returned by that allocator. The pointer widens only in the native
 * private table and does not cross an original ABI. */
typedef struct renderer_image_allocation_s {
    renderer_image_allocation_kind_t kind;
    void *memory;
} renderer_image_allocation_t;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(renderer_image_allocation_t) == 4, "renderer image-allocation alignment changed");
_Static_assert(offsetof(renderer_image_allocation_t, kind) == 0, "renderer image-allocation kind moved");
_Static_assert(offsetof(renderer_image_allocation_t, memory) == 4, "renderer image-allocation pointer moved");
_Static_assert(sizeof(renderer_image_allocation_t) == 8, "renderer image-allocation layout changed");
#endif

/* The original caches this scan by tr.frameCount in four file-static words:
 * 0x03887ac4, 0x0388bdc8, 0x03887ac0, and 0x0388bed4. */
static int32_t rendererUsedImageFrame;
static int32_t rendererUsedImageMemory;
static int32_t rendererUsedLightmapMemory;
static int32_t rendererUsedTextureMemory;

/* Source: CoDUOMP.exe 0x005049c0..0x00504b19.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005049c0_00504b1a.mcode.
 * Name: exact same-module Mac symbol R_SumOfUsedImages. Images touched in
 * either the current or immediately preceding renderer frame contribute to
 * the cached totals. */
void R_SumOfUsedImages(int32_t *imageMemory, int32_t *lightmapMemory, int32_t *textureMemory)
{
    if (rendererUsedImageFrame != tr.frameCount) {
        const int32_t previousFrame = (int32_t)((uint32_t)tr.frameCount - 1u);

        rendererUsedImageFrame = tr.frameCount;
        rendererUsedImageMemory = 0;
        rendererUsedLightmapMemory = 0;
        rendererUsedTextureMemory = 0;

        for (int32_t imageIndex = 0; imageIndex < tr.imageCount; ++imageIndex) {
            const image_t *const image = tr.images[imageIndex];
            if (image->frameUsed != tr.frameCount && image->frameUsed != previousFrame) {
                continue;
            }

            rendererUsedImageMemory = (int32_t)((uint32_t)rendererUsedImageMemory + (uint32_t)image->cardMemory);
            if (image->imageTrack == R_IMAGE_TRACK_LIGHTMAP) {
                rendererUsedLightmapMemory = (int32_t)((uint32_t)rendererUsedLightmapMemory + (uint32_t)image->cardMemory);
            }
            rendererUsedTextureMemory = (int32_t)((uint32_t)rendererUsedTextureMemory + (uint32_t)image->textureMemory);
        }

        if (com_statmon->integer != 0 && rendererUsedTextureMemory > R_USED_IMAGE_WARNING_BYTES) {
            StatMon_Warning(R_USED_IMAGE_WARNING_STATMON_ENTRY, R_USED_IMAGE_WARNING_DURATION_MSEC, rendererUsedImageWarningShader);
        }
    }

    if (imageMemory != NULL)
        *imageMemory = rendererUsedImageMemory;
    if (lightmapMemory != NULL)
        *lightmapMemory = rendererUsedLightmapMemory;
    if (textureMemory != NULL)
        *textureMemory = rendererUsedTextureMemory;
}

/* One GL_TextureMode lookup-table row: name is the accepted cvar spelling,
 * minFilter is passed for GL_TEXTURE_MIN_FILTER, and magFilter is passed for
 * GL_TEXTURE_MAG_FILTER. Native pointer widening is private to this table. */
typedef struct renderer_texture_filter_mode_s {
    const char *name;
    uint32_t minFilter;
    uint32_t magFilter;
} renderer_texture_filter_mode_t;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(renderer_texture_filter_mode_t) == 4, "renderer texture-filter mode alignment changed");
_Static_assert(offsetof(renderer_texture_filter_mode_t, name) == 0, "renderer texture-filter mode name moved");
_Static_assert(offsetof(renderer_texture_filter_mode_t, minFilter) == 4, "renderer texture-filter minimum moved");
_Static_assert(offsetof(renderer_texture_filter_mode_t, magFilter) == 8, "renderer texture-filter magnification mode moved");
_Static_assert(sizeof(renderer_texture_filter_mode_t) == 12, "renderer texture-filter mode layout changed");
#endif

/* Source: CoDUOMP.exe 0x005cea50, pointer table to 0x005b7140..0x005b7195.
 * The fixed-width labels are used verbatim by R_FindExistingImage's reuse
 * diagnostic. PE_RELOCATION_VALUES_VERIFIED: all eleven labels match. */
static const char *const rendererImageTrackNames[R_IMAGE_TRACK_COUNT] = {"misc ",  "debug ", "ui    ", "lmap  ", "effect", "hud   ",
                                                                         "vmodel", "model ", "world ", "f/x ",   "$tex+?"};

/* Source: CoDUOMP.exe 0x00504b20..0x00504b5c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00504b20_00504b5d.mcode.
 * Name and qsort-compatible signature: exact same-module Mac symbol
 * imagecompare. Windows proves that the index array is ordered first by
 * image-tracking category and then by allocated card-memory size. */
static int imagecompare(const void *firstIndexPointer, const void *secondIndexPointer)
{
    const int32_t firstIndex = *(const int32_t *)firstIndexPointer;
    const int32_t secondIndex = *(const int32_t *)secondIndexPointer;
    const image_t *const firstImage = tr.images[firstIndex];
    const image_t *const secondImage = tr.images[secondIndex];

    if (firstImage->imageTrack < secondImage->imageTrack)
        return -1;
    if (firstImage->imageTrack > secondImage->imageTrack)
        return 1;
    return firstImage->cardMemory - secondImage->cardMemory;
}

/* Source: CoDUOMP.exe 0x00504b60..0x00504b66, recovered from an executable
 * gap after repairing the missing Ghidra function boundary.
 * Name and signature: exact same-module Mac symbol RE_GetImageMemory. */
uint32_t RE_GetImageMemory(void)
{
    return (uint32_t)tr.imageMemory;
}

/* Source: CoDUOMP.exe 0x00504b70..0x00504b76, recovered from an executable
 * gap after repairing the missing Ghidra function boundary.
 * Name and signature: exact same-module Mac symbol RE_GetFXImageMemory. */
uint32_t RE_GetFXImageMemory(void)
{
    return tr.fxImageMemory;
}

/* Source: CoDUOMP.exe 0x00504b80..0x00504b8a, recovered from an executable
 * gap after repairing the missing Ghidra function boundary.
 * Name and signature: exact same-module Mac symbol RE_SetFXImageMemory. */
void RE_SetFXImageMemory(uint32_t imageMemory)
{
    tr.fxImageMemory = imageMemory;
}

/* Source: CoDUOMP.exe 0x00504b90..0x00504e28.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00504b90_00504e29.mcode.
 * Name and void signature: exact same-module Mac symbol R_ImageList_f.
 * The PE proves the complete output order, the image sorting keys, each
 * internal-format and wrapping label, and the per-tracking-category totals. */
void R_ImageList_f(void)
{
    int32_t sortedImageIndices[R_MAX_IMAGES];
    int32_t imageMemoryByTrack[R_IMAGE_TRACK_COUNT] = {0};
    int32_t totalImageMemory = 0;
    int32_t totalTexels = 0;

    for (int32_t imageIndex = 0; imageIndex < tr.imageCount; ++imageIndex) {
        sortedImageIndices[imageIndex] = imageIndex;
    }
    coduo_crt_qsort(sortedImageIndices, (size_t)(uint32_t)tr.imageCount, sizeof(sortedImageIndices[0]), imagecompare);

    ri.Printf(R_PRINT_ALL, "\n      -w-- -h-- -mm- -if--- wrap- -type- -KB--- --name-------\n");

    for (int32_t sortedIndex = 0; sortedIndex < tr.imageCount; ++sortedIndex) {
        const int32_t imageIndex = sortedImageIndices[sortedIndex];
        const image_t *const image = tr.images[imageIndex];

        totalTexels = (int32_t)((uint32_t)totalTexels + (uint32_t)image->uploadWidth * (uint32_t)image->uploadHeight);
        ri.Printf(R_PRINT_ALL, "%4i: %4i %4i  %s  ", imageIndex, (int32_t)image->uploadWidth, (int32_t)image->uploadHeight,
                  (image->flags & IMAGE_FLAG_MIPMAP) != 0 ? "yes" : "no ");

        switch (image->internalFormat) {
        case 1:
            ri.Printf(R_PRINT_ALL, "I    ");
            break;
        case 2:
            ri.Printf(R_PRINT_ALL, "IA   ");
            break;
        case 3:
            ri.Printf(R_PRINT_ALL, "RGB  ");
            break;
        case 4:
        case GL_RGBA:
            ri.Printf(R_PRINT_ALL, "RGBA ");
            break;
        case GL_RGB5:
            ri.Printf(R_PRINT_ALL, "RGB5 ");
            break;
        case GL_RGB8:
            ri.Printf(R_PRINT_ALL, "RGB8 ");
            break;
        case GL_RGBA4:
            ri.Printf(R_PRINT_ALL, "RGBA4");
            break;
        case GL_RGBA8:
            ri.Printf(R_PRINT_ALL, "RGBA8");
            break;
        case GL_RGB4_S3TC:
            ri.Printf(R_PRINT_ALL, "S3TC4");
            break;
        case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
            ri.Printf(R_PRINT_ALL, "DXT1 ");
            break;
        case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
            ri.Printf(R_PRINT_ALL, "DXT3 ");
            break;
        case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
            ri.Printf(R_PRINT_ALL, "DXT5 ");
            break;
        default:
            ri.Printf(R_PRINT_ALL, "?????");
            break;
        }

        switch (image->flags & (IMAGE_FLAG_CLAMP_S | IMAGE_FLAG_CLAMP_T)) {
        case IMAGE_FLAG_NONE:
            ri.Printf(R_PRINT_ALL, " repeat ");
            break;
        case IMAGE_FLAG_CLAMP_S:
            ri.Printf(R_PRINT_ALL, " clampx ");
            break;
        case IMAGE_FLAG_CLAMP_T:
            ri.Printf(R_PRINT_ALL, " clampy ");
            break;
        default:
            ri.Printf(R_PRINT_ALL, " clamp  ");
            break;
        }

        ri.Printf(R_PRINT_ALL, " %s %6.1f %s\n", rendererImageTrackNames[image->imageTrack], (double)image->cardMemory / 1024.0,
                  image->imgName);
        totalImageMemory = (int32_t)((uint32_t)totalImageMemory + (uint32_t)image->cardMemory);
        imageMemoryByTrack[image->imageTrack] = (int32_t)((uint32_t)imageMemoryByTrack[image->imageTrack] + (uint32_t)image->cardMemory);
    }

    ri.Printf(R_PRINT_ALL, " ---------\n");
    ri.Printf(R_PRINT_ALL, " %i total texels (not including mipmaps)\n", totalTexels);
    ri.Printf(R_PRINT_ALL, " %i total images\n", tr.imageCount);
    ri.Printf(R_PRINT_ALL, " %.2f MB total image size\n\n", (double)totalImageMemory / (1024.0 * 1024.0));

    for (int32_t imageTrack = 0; imageTrack < R_IMAGE_TRACK_COUNT; ++imageTrack) {
        ri.Printf(R_PRINT_ALL, "%s: %.2f MB\n", rendererImageTrackNames[imageTrack],
                  (double)imageMemoryByTrack[imageTrack] / (1024.0 * 1024.0));
    }
}

/* The color portion shared by DXT1, DXT3, and DXT5 blocks: two RGB565
 * endpoints followed by sixteen packed two-bit color-selector indices. */
typedef struct renderer_dxt_color_block_s {
    uint16_t color0;
    uint16_t color1;
    uint32_t colorIndices;
} renderer_dxt_color_block_t;

/* Serialized DDS_PIXELFORMAT. CoDUOMP.exe field-addresses only fourCC; the
 * remaining format-defined lanes are retained verbatim but otherwise unused
 * by CoDUOMP.exe. */
typedef struct renderer_dds_pixel_format_s {
    uint32_t size;
    uint32_t flags;
    uint32_t fourCC;
    uint32_t rgbBitCount;
    uint32_t redMask;
    uint32_t greenMask;
    uint32_t blueMask;
    uint32_t alphaMask;
} renderer_dds_pixel_format_t;

/* Complete 128-byte DDS file prefix, including the four-byte magic. LoadDDS
 * consumes magic, size, height, width, mipMapCount, and pixelFormat.fourCC;
 * all other format-defined lanes are unused by CoDUOMP.exe. */
typedef struct renderer_dds_header_s {
    uint32_t magic;
    uint32_t size;
    uint32_t flags;
    uint32_t height;
    uint32_t width;
    uint32_t pitchOrLinearSize;
    uint32_t depth;
    uint32_t mipMapCount;
    uint32_t reserved1[11];                 /* DDS_HEADER.dwReserved1[11] */
    renderer_dds_pixel_format_t pixelFormat;
    uint32_t caps[4];
    uint32_t reserved2;
} renderer_dds_header_t;

/* The TGA header is byte-shaped so its little-endian 16-bit fields retain the
 * file layout without relying on host packing or unaligned integer loads.
 * LoadTGA uses idLength, type, dimensions, pixelSize, and the vertical-origin
 * bit in imageDescriptor. The color-map descriptor and x/y-origin lanes are
 * retained file fields but otherwise unused by CoDUOMP.exe. */
typedef struct renderer_tga_header_s {
    uint8_t idLength;
    uint8_t colorMapType;
    uint8_t imageType;
    uint8_t colorMapIndex[2];
    uint8_t colorMapLength[2];
    uint8_t colorMapSize;
    uint8_t xOrigin[2];
    uint8_t yOrigin[2];
    uint8_t width[2];
    uint8_t height[2];
    uint8_t pixelSize;
    uint8_t imageDescriptor;
} renderer_tga_header_t;

/* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
typedef struct coduomp_jpeg_error_manager_s {
    struct jpeg_error_mgr callbacks;
    jmp_buf failure;
} coduomp_jpeg_error_manager_t;

static void coduomp_jpeg_error_exit(j_common_ptr jpeg)
{
    coduomp_jpeg_error_manager_t *errorManager = (coduomp_jpeg_error_manager_t *)jpeg->err;

    longjmp(errorManager->failure, 1);
}

/* R_CreateDlightImage's original stack object is one contiguous 0x4012-byte
 * TGA record: an 18-byte header followed immediately by 0x4000 image bytes.
 * The byte-shaped header gives this aggregate alignment one without packing. */
typedef struct renderer_dlight_tga_s {
    renderer_tga_header_t header;
    uint8_t imageData[R_DLIGHT_IMAGE_SIZE][R_DLIGHT_IMAGE_SIZE][4];
} renderer_dlight_tga_t;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(renderer_dxt_color_block_t) == 4, "DXT color-block alignment changed");
_Static_assert(offsetof(renderer_dxt_color_block_t, color0) == 0, "DXT first endpoint moved");
_Static_assert(offsetof(renderer_dxt_color_block_t, color1) == 2, "DXT second endpoint moved");
_Static_assert(offsetof(renderer_dxt_color_block_t, colorIndices) == 4, "DXT color selectors moved");
_Static_assert(sizeof(renderer_dxt_color_block_t) == 8, "DXT color-block layout changed");

_Static_assert(_Alignof(renderer_dds_pixel_format_t) == 4, "DDS pixel-format alignment changed");
_Static_assert(offsetof(renderer_dds_pixel_format_t, size) == 0x00, "DDS pixel-format size moved");
_Static_assert(offsetof(renderer_dds_pixel_format_t, flags) == 0x04, "DDS pixel-format flags moved");
_Static_assert(offsetof(renderer_dds_pixel_format_t, fourCC) == 0x08, "DDS pixel-format FourCC moved");
_Static_assert(offsetof(renderer_dds_pixel_format_t, rgbBitCount) == 0x0c, "DDS pixel-format bit count moved");
_Static_assert(offsetof(renderer_dds_pixel_format_t, redMask) == 0x10, "DDS pixel-format red mask moved");
_Static_assert(offsetof(renderer_dds_pixel_format_t, greenMask) == 0x14, "DDS pixel-format green mask moved");
_Static_assert(offsetof(renderer_dds_pixel_format_t, blueMask) == 0x18, "DDS pixel-format blue mask moved");
_Static_assert(offsetof(renderer_dds_pixel_format_t, alphaMask) == 0x1c, "DDS pixel-format alpha mask moved");
_Static_assert(sizeof(renderer_dds_pixel_format_t) == 0x20, "DDS pixel-format layout changed");

_Static_assert(_Alignof(renderer_dds_header_t) == 4, "DDS header alignment changed");
_Static_assert(offsetof(renderer_dds_header_t, magic) == 0x00, "DDS magic moved");
_Static_assert(offsetof(renderer_dds_header_t, size) == 0x04, "DDS header size moved");
_Static_assert(offsetof(renderer_dds_header_t, flags) == 0x08, "DDS header flags moved");
_Static_assert(offsetof(renderer_dds_header_t, height) == 0x0c, "DDS height moved");
_Static_assert(offsetof(renderer_dds_header_t, width) == 0x10, "DDS width moved");
_Static_assert(offsetof(renderer_dds_header_t, pitchOrLinearSize) == 0x14, "DDS pitch or linear size moved");
_Static_assert(offsetof(renderer_dds_header_t, depth) == 0x18, "DDS depth moved");
_Static_assert(offsetof(renderer_dds_header_t, mipMapCount) == 0x1c, "DDS mip-map count moved");
_Static_assert(offsetof(renderer_dds_header_t, reserved1) == 0x20, "DDS reserved lanes moved");
_Static_assert(offsetof(renderer_dds_header_t, pixelFormat) == 0x4c, "DDS pixel format moved");
_Static_assert(offsetof(renderer_dds_header_t, caps) == 0x6c, "DDS capabilities moved");
_Static_assert(offsetof(renderer_dds_header_t, reserved2) == 0x7c, "DDS trailing reserved lane moved");
_Static_assert(sizeof(renderer_dds_header_t) == 128, "DDS header layout changed");
_Static_assert(offsetof(renderer_dds_header_t, pixelFormat.fourCC) == 0x54, "DDS FourCC offset changed");

_Static_assert(_Alignof(renderer_tga_header_t) == 1, "TGA header alignment changed");
_Static_assert(offsetof(renderer_tga_header_t, idLength) == 0x00, "TGA ID length moved");
_Static_assert(offsetof(renderer_tga_header_t, colorMapType) == 0x01, "TGA color-map type moved");
_Static_assert(offsetof(renderer_tga_header_t, imageType) == 0x02, "TGA image type moved");
_Static_assert(offsetof(renderer_tga_header_t, colorMapIndex) == 0x03, "TGA color-map index moved");
_Static_assert(offsetof(renderer_tga_header_t, colorMapLength) == 0x05, "TGA color-map length moved");
_Static_assert(offsetof(renderer_tga_header_t, colorMapSize) == 0x07, "TGA color-map size moved");
_Static_assert(offsetof(renderer_tga_header_t, xOrigin) == 0x08, "TGA x origin moved");
_Static_assert(offsetof(renderer_tga_header_t, yOrigin) == 0x0a, "TGA y origin moved");
_Static_assert(offsetof(renderer_tga_header_t, width) == 0x0c, "TGA width moved");
_Static_assert(offsetof(renderer_tga_header_t, height) == 0x0e, "TGA height moved");
_Static_assert(offsetof(renderer_tga_header_t, pixelSize) == 0x10, "TGA pixel size moved");
_Static_assert(offsetof(renderer_tga_header_t, imageDescriptor) == 0x11, "TGA image descriptor moved");
_Static_assert(sizeof(renderer_tga_header_t) == 18, "TGA header layout changed");
#endif

/* Source: CoDUOMP.exe 0x0050a260..0x0050a359.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050a260_0050a35a.mcode.
 * Name: exact same-module Mac symbol R_CreateDefaultImage. The Mac body proves
 * that the Windows optimizer inlined the source-level R_CreateImage call. */
static void R_CreateDefaultImage(void)
{
    uint8_t pixels[R_DEFAULT_IMAGE_SIZE][R_DEFAULT_IMAGE_SIZE][4];

    memset(pixels, 32, sizeof(pixels));
    for (int32_t coordinate = 0; coordinate < R_DEFAULT_IMAGE_SIZE; ++coordinate) {
        for (int32_t component = 0; component < 3; ++component) {
            pixels[0][coordinate][component] = 0;
            pixels[R_DEFAULT_IMAGE_SIZE - 1][coordinate][component] = 0;
            pixels[coordinate][0][component] = 0;
            pixels[coordinate][R_DEFAULT_IMAGE_SIZE - 1][component] = 0;
        }
        pixels[0][coordinate][3] = 255;
        pixels[R_DEFAULT_IMAGE_SIZE - 1][coordinate][3] = 255;
        pixels[coordinate][0][3] = 255;
        pixels[coordinate][R_DEFAULT_IMAGE_SIZE - 1][3] = 255;
    }

    tr.defaultImage = R_CreateImage("*default", &pixels[0][0][0], R_DEFAULT_IMAGE_SIZE, R_DEFAULT_IMAGE_SIZE, GL_RGBA, IMAGE_FLAG_MIPMAP,
                                    R_IMAGE_TRACK_MISC, NULL);
}

/* Source: CoDUOMP.exe 0x0050a360..0x0050a370.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050a360_0050a371.mcode.
 * Name and argument: exact same-module Mac symbol
 * SmallestTextureSizeFitting. Callers supply positive dimensions. */
int32_t SmallestTextureSizeFitting(int32_t dimension)
{
    int32_t textureSize = 1;

    --dimension;
    if (dimension == 0)
        return textureSize;

    do {
        dimension >>= 1;
        textureSize <<= 1;
    } while (dimension != 0);
    return textureSize;
}

/* Source: CoDUOMP.exe 0x0050a380..0x0050a435.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050a380_0050a436.mcode.
 * Name: exact same-module Mac symbol R_CreateScreenImage. */
static void R_CreateScreenImage(void)
{
    const int32_t textureWidth = SmallestTextureSizeFitting(glConfig.vidWidth);
    const int32_t textureHeight = SmallestTextureSizeFitting(glConfig.vidHeight);

    /* The original IMUL/SHL size calculation wraps in 32 bits before the
     * allocation and memset calls receive it. */
    const uint32_t pixelBytes = (uint32_t)textureWidth * (uint32_t)textureHeight * 4u;
    uint8_t *pixels;
#if UINTPTR_MAX > UINT32_MAX
    /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): retail i386 places the
     * power-of-two screen texture staging buffer in the temporary hunk. A
     * 5120x2880 Retina drawable rounds to 8192x4096, making this one buffer
     * exactly 128 MiB and therefore impossible to fit in the default 128 MiB
     * hunk beside any permanent allocations. Match the native-wide screenshot
     * scratch policy by changing only this buffer's ownership; the proved
     * dimensions, byte count, fill, upload, and cleanup order stay intact. */
    pixels = ri.Z_Malloc(pixelBytes);
#else
    pixels = ri.Hunk_AllocateTempMemory(pixelBytes);
#endif
    memset(pixels, 255, pixelBytes);

    tr.screenImage = R_CreateImage("*screen", pixels, textureWidth, textureHeight, GL_RGBA,
                                   IMAGE_FLAG_COLOR_DEPTH | IMAGE_FLAG_CLAMP_S | IMAGE_FLAG_CLAMP_T, R_IMAGE_TRACK_MISC, NULL);
    tr.screenImageWidth = textureWidth;
    tr.screenImageHeight = textureHeight;

#if UINTPTR_MAX > UINT32_MAX
    ri.Z_Free(pixels);
#else
    ri.Hunk_FreeTempMemory(pixels);
#endif
}

/* Source: CoDUOMP.exe 0x0050a030..0x0050a256.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050a030_0050a257.mcode.
 * Name: exact same-module Mac symbol R_CreateDlightImage. The radial formula,
 * cvar defaults, truncation and clamp, image flags, and optional diagnostic TGA
 * are all proved independently by the Windows instructions. */
static void R_CreateDlightImage(void)
{
    uint8_t pixels[R_DLIGHT_IMAGE_SIZE][R_DLIGHT_IMAGE_SIZE][4];
    renderer_dlight_tga_t tga;
    cvar_t *radius = ri.Cvar_Get("r_dlightRadius", "0.1124278", CVAR_NONE);
    cvar_t *shift = ri.Cvar_Get("r_dlightShift", "0.3162278", CVAR_NONE);

    const float shiftSquared = shift->value * shift->value;
    const float radiusAndShiftSquared = radius->value * radius->value + shiftSquared;
    const float shiftedUnitRadius = 1.0f + shiftSquared;
    const float intensityScale = 255.0f / (1.0f / radiusAndShiftSquared - 1.0f / shiftedUnitRadius);
    const float intensityOffset = intensityScale / shiftedUnitRadius;

    /* Exact Windows float 0x3a841883, mathematically 1/(31.5*31.5). */
    const float normalizedDistanceScale = 0.001007810584269464f;
    for (int32_t x = 0; x < R_DLIGHT_IMAGE_SIZE; ++x) {
        const float centeredX = 31.5f - (float)x;
        for (int32_t y = 0; y < R_DLIGHT_IMAGE_SIZE; ++y) {
            const float centeredY = 31.5f - (float)y;
            const float normalizedDistanceSquared = (centeredX * centeredX + centeredY * centeredY) * normalizedDistanceScale;
            int32_t intensity = (int32_t)(intensityScale / (normalizedDistanceSquared + shiftSquared) - intensityOffset);

            if (intensity > 255)
                intensity = 255;
            else if (intensity < 0)
                intensity = 0;

            pixels[y][x][0] = (uint8_t)intensity;
            pixels[y][x][1] = (uint8_t)intensity;
            pixels[y][x][2] = (uint8_t)intensity;
            pixels[y][x][3] = 255;
        }
    }

    tr.dlightImage = R_CreateImage("*dlight", &pixels[0][0][0], R_DLIGHT_IMAGE_SIZE, R_DLIGHT_IMAGE_SIZE, GL_RGBA,
                                   IMAGE_FLAG_CLAMP_S | IMAGE_FLAG_CLAMP_T, R_IMAGE_TRACK_MISC, NULL);

    if (radius->integer > 1 || shift->integer > 1) {
        memset(&tga.header, 0, sizeof(tga.header));
        tga.header.imageType = 2;
        tga.header.width[0] = R_DLIGHT_IMAGE_SIZE;
        tga.header.height[0] = R_DLIGHT_IMAGE_SIZE;
        tga.header.pixelSize = 32;
        memcpy(tga.imageData, pixels, sizeof(pixels));
        ri.FS_WriteFile("dlight.tga", &tga, (int32_t)sizeof(tga));
    }
}

/* Source: CoDUOMP.exe 0x0050a860..0x0050a87a.
 * Name: exact same-module Mac symbol R_InitImages. The Windows R_Init copy is
 * inlined, while this retained out-of-line body performs the same sequence. */
void R_InitImages(void)
{
    memset(imageHashTable, 0, sizeof(imageHashTable));
    R_SetColorMappings();
    R_CreateBuiltinImages();
}

/* Source: CoDUOMP.exe 0x0050a440..0x0050a648.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050a440_0050a649.mcode.
 * Name: exact same-module Mac symbol R_CreateBuiltinImages. The Mac body proves
 * the source-level R_CreateImage calls that MSVC expanded inline. */
void R_CreateBuiltinImages(void)
{
    uint8_t pixels[R_BUILTIN_IMAGE_SIZE][R_BUILTIN_IMAGE_SIZE][4];

    R_CreateDefaultImage();

    memset(pixels, 255, sizeof(pixels));
    tr.whiteImage = R_CreateImage("*white", &pixels[0][0][0], R_BUILTIN_UPLOAD_SIZE, R_BUILTIN_UPLOAD_SIZE, GL_RGBA, IMAGE_FLAG_NONE,
                                  R_IMAGE_TRACK_MISC, NULL);

    for (int32_t row = 0; row < R_BUILTIN_IMAGE_SIZE; ++row) {
        for (int32_t column = 0; column < R_BUILTIN_IMAGE_SIZE; ++column) {
            pixels[row][column][0] = (uint8_t)tr.identityLightByte;
            pixels[row][column][1] = (uint8_t)tr.identityLightByte;
            pixels[row][column][2] = (uint8_t)tr.identityLightByte;
            pixels[row][column][3] = 255;
        }
    }
    tr.identityLightImage = R_CreateImage("*identityLight", &pixels[0][0][0], R_BUILTIN_UPLOAD_SIZE, R_BUILTIN_UPLOAD_SIZE, GL_RGBA,
                                          IMAGE_FLAG_NONE, R_IMAGE_TRACK_MISC, NULL);

    memset(pixels, 192, sizeof(pixels));
    tr.grayImage = R_CreateImage("*gray", &pixels[0][0][0], R_BUILTIN_UPLOAD_SIZE, R_BUILTIN_UPLOAD_SIZE, GL_RGBA, IMAGE_FLAG_NONE,
                                 R_IMAGE_TRACK_MISC, NULL);

    for (int32_t imageIndex = 0; imageIndex < R_MAX_SCRATCH_IMAGES; ++imageIndex) {
        tr.scratchImages[imageIndex] =
            R_CreateImage("*scratch", &pixels[0][0][0], R_BUILTIN_IMAGE_SIZE, R_BUILTIN_IMAGE_SIZE, GL_RGBA,
                          IMAGE_FLAG_ALLOW_PICMIP | IMAGE_FLAG_CLAMP_S | IMAGE_FLAG_CLAMP_T, R_IMAGE_TRACK_MISC, NULL);
    }

    R_CreateDlightImage();
    R_CreateScreenImage();
}

/* Source: CoDUOMP.exe 0x0050a880..0x0050a99d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050a880_0050a99e.mcode.
 * Name: exact same-module Mac symbol R_DeleteTextures. Mac proves the two
 * source-level GL_SelectTexture calls that MSVC expanded in the Windows body. */
void R_DeleteTextures(void)
{
    for (int32_t imageIndex = 0; imageIndex < tr.imageCount; ++imageIndex) {
        qglDeleteTextures(1, &tr.images[imageIndex]->texnum);
    }

    memset(tr.images, 0, sizeof(tr.images));
    memset(glState.currenttextures, 0, sizeof(glState.currenttextures));

    if (qglBindTexture == NULL)
        return;

    if (qglActiveTextureARB != NULL) {
        GL_SelectTexture(1);
        qglBindTexture(GL_TEXTURE_2D, 0);
        GL_SelectTexture(0);
        qglBindTexture(GL_TEXTURE_2D, 0);
    } else {
        qglBindTexture(GL_TEXTURE_2D, 0);
    }
}

/* Source: CoDUOMP.exe 0x0050aad0..0x0050ab66.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050aad0_0050ab67.mcode.
 * Name and signature: exact same-module Mac symbol
 * R_ColorShiftLightingBytes. The Windows OR gate, maximum-channel selection,
 * integer scaling, and alpha copy are retained verbatim. */
void R_ColorShiftLightingBytes(const uint8_t input[4], uint8_t output[4])
{
    /* IA-32 masks the variable shift count to five bits, and the following
     * integer products retain their modulo-2^32 results. */
    const uint32_t shift = (1u - (uint32_t)tr.overbrightBits) & 31u;
    int32_t red = (int32_t)((uint32_t)input[0] << shift);
    int32_t green = (int32_t)((uint32_t)input[1] << shift);
    int32_t blue = (int32_t)((uint32_t)input[2] << shift);

    if ((red | green | blue) > 255) {
        int32_t maximum = red > green ? red : green;
        if (blue > maximum)
            maximum = blue;

        red = (int32_t)((uint32_t)red * 255u) / maximum;
        green = (int32_t)((uint32_t)green * 255u) / maximum;
        blue = (int32_t)((uint32_t)blue * 255u) / maximum;
    }

    output[0] = (uint8_t)red;
    output[1] = (uint8_t)green;
    output[2] = (uint8_t)blue;
    output[3] = input[3];
}

/* Source: CoDUOMP.exe 0x0050af10..0x0050af70.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050af10_0050af71.mcode.
 * Name and five-argument source signature: exact same-module Mac symbol
 * R_CopyLightmap. The source is packed RGB; the copied alpha byte is replaced
 * with the proved opaque value after every color-shift call. */
void R_CopyLightmap(uint8_t *destination, const uint8_t *source, int32_t destinationX, int32_t destinationY, int32_t destinationWidth)
{
    const uint32_t destinationPixelOffset =
        ((uint32_t)destinationY * (uint32_t)destinationWidth + (uint32_t)destinationX) * R_LIGHTMAP_DESTINATION_COMPONENTS;
    uint8_t *destinationRow = destination + (size_t)destinationPixelOffset;
    const uint32_t destinationRowBytes = (uint32_t)destinationWidth * R_LIGHTMAP_DESTINATION_COMPONENTS;

    for (int32_t row = 0; row < R_LIGHTMAP_SIZE; ++row) {
        uint8_t *destinationPixel = destinationRow;
        for (int32_t column = 0; column < R_LIGHTMAP_SIZE; ++column) {
            R_ColorShiftLightingBytes(source, destinationPixel);
            destinationPixel[3] = 255;
            source += R_LIGHTMAP_SOURCE_COMPONENTS;
            destinationPixel += R_LIGHTMAP_DESTINATION_COMPONENTS;
        }
        destinationRow += destinationRowBytes;
    }
}

enum renderer_dds_constant_e {
    R_DDS_HEADER_SIZE = 124,
    R_DDS_DATA_OFFSET = 128,
    R_DDS_MAX_DIMENSION = 32768,
    R_DDS_MAGIC = 0x20534444, /* little-endian "DDS " */
    R_DDS_FOURCC_DXT1 = 0x31545844,
    R_DDS_FOURCC_DXT3 = 0x33545844,
    R_DDS_FOURCC_DXT5 = 0x35545844
};

enum renderer_tga_constant_e {
    R_TGA_TYPE_UNCOMPRESSED_TRUECOLOR = 2,
    R_TGA_TYPE_UNCOMPRESSED_GRAYSCALE = 3,
    R_TGA_TYPE_RLE_TRUECOLOR = 10,
    R_TGA_PIXEL_SIZE_GRAYSCALE = 8,
    R_TGA_PIXEL_SIZE_RGB = 24,
    R_TGA_PIXEL_SIZE_RGBA = 32,
    R_TGA_ORIGIN_TOP = 0x20,
    R_TGA_RLE_PACKET = 0x80,
    R_TGA_PACKET_LENGTH_MASK = 0x7f
};

/* Source: CoDUOMP.exe 0x005cea80..0x005ceabf. UploadImage advances one row
 * per generated mip level before optionally tinting r_colorMipLevels output. */
static const uint8_t mipBlendColors[16][4] = {{0, 0, 0, 0},       {255, 0, 0, 128},   {0, 255, 0, 128},   {0, 0, 255, 128},
                                              {255, 255, 0, 128}, {0, 255, 255, 128}, {255, 0, 255, 128}, {255, 0, 0, 128},
                                              {0, 255, 0, 128},   {0, 0, 255, 128},   {255, 128, 0, 128}, {0, 255, 128, 128},
                                              {128, 0, 255, 128}, {255, 0, 0, 128},   {0, 255, 0, 128},   {0, 0, 255, 128}};

/* Original array at 0x038878c0..0x038879bf and discontiguous count at
 * 0x0389fec0. Native pointers intentionally widen with the host; this tracker
 * never crosses a serialized or module ABI. */
static renderer_image_allocation_t rendererImageAllocations[R_MAX_IMAGE_ALLOCATIONS];
static int32_t rendererImageAllocationCount;

/* Source: CoDUOMP.exe 0x00504780..0x00504799, repaired from the executable
 * gap inventory. Name: same-module Mac symbol R_RememberImageAllocation. */
void R_RememberImageAllocation(void *memory, renderer_image_allocation_kind_t kind)
{
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if ((uint32_t)rendererImageAllocationCount >= R_MAX_IMAGE_ALLOCATIONS) {
        if (kind == R_IMAGE_ALLOCATION_FILE)
            ri.FS_FreeFile(memory);
        else if (kind == R_IMAGE_ALLOCATION_TEMP_MEMORY)
            ri.Hunk_FreeTempMemory(memory);
        R_FreeImageAllocations();
        R_ResetImageAllocations();
        ri.Error(ERR_DROP, "\x15R_RememberImageAllocation: too many transient image "
                           "allocations\n");
        return;
    }

    rendererImageAllocations[rendererImageAllocationCount].kind = kind;
    rendererImageAllocations[rendererImageAllocationCount].memory = memory;
    ++rendererImageAllocationCount;
}

/* Source: CoDUOMP.exe 0x005047a0..0x005047aa, repaired from the executable
 * gap inventory. Name: same-module Mac symbol R_ResetImageAllocations. */
void R_ResetImageAllocations(void)
{
    rendererImageAllocationCount = 0;
}

/* Source: CoDUOMP.exe 0x005047b0..0x005047ff.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005047b0_00504800.mcode.
 * Name: same-module Mac symbol R_FreeImageAllocations. Allocations are
 * released in strict reverse registration order through their owning API. */
void R_FreeImageAllocations(void)
{
    while (rendererImageAllocationCount > 0) {
        renderer_image_allocation_t *allocation = &rendererImageAllocations[--rendererImageAllocationCount];

        if (allocation->kind == R_IMAGE_ALLOCATION_FILE)
            ri.FS_FreeFile(allocation->memory);
        else if (allocation->kind == R_IMAGE_ALLOCATION_TEMP_MEMORY)
            ri.Hunk_FreeTempMemory(allocation->memory);
    }
}

/* Source: CoDUOMP.exe 0x00504800..0x0050482d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00504800_0050482e.mcode.
 * Name: same-module Mac symbol R_AllocTempMemory. */
void *R_AllocTempMemory(size_t size)
{
    void *memory = ri.Hunk_AllocateTempMemory(size);

    if (memory != NULL) {
        R_RememberImageAllocation(memory, R_IMAGE_ALLOCATION_TEMP_MEMORY);
    }
    return memory;
}

/* Source: CoDUOMP.exe 0x00504830..0x00504860, repaired from the executable
 * gap inventory. Name: same-module Mac symbol R_ReadFile. */
int32_t R_ReadFile(const char *name, void **buffer)
{
    const int32_t fileSize = ri.FS_ReadFile(name, buffer);

    if (fileSize >= 0)
        R_RememberImageAllocation(*buffer, R_IMAGE_ALLOCATION_FILE);
    return fileSize;
}

/* Source: CoDUOMP.exe 0x005cea08..0x005cea4f.
 * PE_RELOCATION_VALUES_VERIFIED: all six filter-name pointers match the PE. */
static const renderer_texture_filter_mode_t rendererTextureFilterModes[] = {
    {"GL_NEAREST", GL_NEAREST, GL_NEAREST},
    {"GL_LINEAR", GL_LINEAR, GL_LINEAR},
    {"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
    {"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
    {"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
    {"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}};

/* Source: CoDUOMP.exe 0x00505d70..0x00505e2e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00505d70_00505e2f.mcode.
 * Name and source boundary: exact same-module Mac symbol PickInternalFormat.
 * The Windows jump table has distinguished color-depth cases only for 15/16,
 * 24, and 32 bits; other values continue through the alpha scan. */
uint32_t PickInternalFormat(const uint8_t *pixels, uint32_t format, int32_t width, int32_t height, uint32_t flags, qboolean isLightmap)
{
    if (format != GL_RGBA)
        return format;
    if (isLightmap)
        return GL_RGBA8;

    if ((flags & IMAGE_FLAG_COLOR_DEPTH) != 0) {
        switch (glConfig.colorBits) {
        case 15:
        case 16:
            return GL_RGB5;
        case 24:
            return GL_RGB;
        case 32:
            return GL_RGBA;
        default:
            break;
        }
    }

    const int32_t pixelCount = (int32_t)((uint32_t)width * (uint32_t)height);
    qboolean hasAlpha = qfalse;

    for (int32_t pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex) {
        if (pixels[pixelIndex * 4 + 3] != 255) {
            hasAlpha = qtrue;
            break;
        }
    }

    if (hasAlpha) {
        if (glConfig.colorBits == 16)
            return GL_RGBA4;
        if (glConfig.colorBits == 32)
            return GL_RGBA8;
        return 4;
    }

    if (glConfig.colorBits == 16)
        return GL_RGB5;
    if (glConfig.colorBits == 32)
        return GL_RGB8;
    return 3;
}

/* Source: CoDUOMP.exe 0x005050b0..0x00505189.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005050b0_0050518a.mcode.
 * Name and source boundary: exact same-module Mac symbol
 * GetCardMemoryAmount. These are accounting values rather than format-size
 * declarations, but the original RGBA8 two-byte result is still an erroneous
 * undercount and is deliberately preserved below. */
int32_t GetCardMemoryAmount(uint32_t internalFormat, int32_t width, int32_t height)
{
    const int32_t pixelCount = (int32_t)((uint32_t)width * (uint32_t)height);

    switch (internalFormat) {
    case 3:
    case 4:
    case GL_RGBA:
    case GL_RGB8:
    case GL_BGR:
    case GL_BGRA:
        return (int32_t)((uint32_t)pixelCount * 4U);
    case GL_RGB:
        return (int32_t)((uint32_t)pixelCount * (uint32_t)(glConfig.colorBits > 16 ? 4 : 2));
    case GL_LUMINANCE:
        return pixelCount;
    case GL_RGB5:
    case GL_RGBA4:
        return (int32_t)((uint32_t)pixelCount * 2U);
    case GL_RGBA8:
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        return (int32_t)((uint32_t)pixelCount * 2U);
    case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT: {
        const int32_t blockWidth = (int32_t)((uint32_t)width + 3U) / 4;
        const int32_t blockHeight = (int32_t)((uint32_t)height + 3U) / 4;
        return (int32_t)((uint32_t)blockWidth * (uint32_t)blockHeight * 8U);
    }
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT: {
        const int32_t blockWidth = (int32_t)((uint32_t)width + 3U) / 4;
        const int32_t blockHeight = (int32_t)((uint32_t)height + 3U) / 4;
        return (int32_t)((uint32_t)blockWidth * (uint32_t)blockHeight * 16U);
    }
    default:
        return 0;
    }
}

/* Source: CoDUOMP.exe 0x00505d00..0x00505d62.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00505d00_00505d68.mcode.
 * Name and source boundary: exact same-module Mac symbol R_TexImage2D. */
void R_TexImage2D(uint32_t target, int32_t level, uint32_t internalFormat, int32_t width, int32_t height, uint32_t format,
                  const uint8_t *pixels)
{
    (void)qglGetError();
    if (internalFormat >= GL_COMPRESSED_RGB_S3TC_DXT1_EXT && internalFormat <= GL_COMPRESSED_RGBA_S3TC_DXT5_EXT) {
        qglCompressedTexImage2DARB(target, level, internalFormat, width, height, 0, GetCardMemoryAmount(internalFormat, width, height),
                                   pixels);
    } else {
        qglTexImage2D(target, level, (int32_t)internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, pixels);
    }
    (void)qglGetError();
}

/* Source: CoDUOMP.exe 0x00505ad0..0x00505b35.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00505ad0_00505b36.mcode.
 * Name and source boundary: exact same-module Mac symbol
 * R_BlendOverTexture_RGBA. Alpha itself is deliberately left unchanged. */
void R_BlendOverTexture_RGBA(uint8_t *pixels, int32_t pixelCount, const uint8_t blendColor[4])
{
    const int32_t alpha = blendColor[3];
    const int32_t inverseAlpha = 255 - alpha;
    const int32_t red = blendColor[0] * alpha;
    const int32_t green = blendColor[1] * alpha;
    const int32_t blue = blendColor[2] * alpha;

    for (int32_t pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex) {
        pixels[0] = (uint8_t)((pixels[0] * inverseAlpha + red) >> 8);
        pixels[1] = (uint8_t)((pixels[1] * inverseAlpha + green) >> 8);
        pixels[2] = (uint8_t)((pixels[2] * inverseAlpha + blue) >> 8);
        pixels += 4;
    }
}

/* Source: CoDUOMP.exe 0x00505b40..0x00505c9c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00505b40_00505c9d.mcode.
 * Name and source boundary: exact same-module Mac symbol
 * R_BlendOverTexture_S3TC. DXT1 preserves endpoint order unconditionally;
 * DXT3/5 swap endpoints when blending reverses their ordering and remap the
 * two direct-color selector values exactly as the original bit operation. */
void R_BlendOverTexture_S3TC(uint8_t *blocks, int32_t pixelCount, const uint8_t blendColor[4], int32_t blockStride)
{
    const int32_t blockCount = pixelCount > 0 ? ((pixelCount - 1) >> 4) + 1 : 0;
    const int32_t alpha = blendColor[3];
    const int32_t inverseAlpha = 255 - alpha;
    const int32_t red = ((blendColor[0] & 0xf8) << 8) * alpha;
    const int32_t green = ((blendColor[1] & 0xfc) << 3) * alpha;
    const int32_t blue = (blendColor[2] >> 3) * alpha;

    for (int32_t blockIndex = 0; blockIndex < blockCount; ++blockIndex) {
        renderer_dxt_color_block_t *block = (renderer_dxt_color_block_t *)blocks;
        const uint16_t oldColor0 = block->color0;
        const uint16_t oldColor1 = block->color1;
        const uint16_t newColor0 = (uint16_t)((((oldColor0 & 0xf800) * inverseAlpha + red) >> 8 & 0xf800) |
                                              (((oldColor0 & 0x07e0) * inverseAlpha + green) >> 8 & 0x07e0) |
                                              (((oldColor0 & 0x001f) * inverseAlpha + blue) >> 8 & 0x001f));
        const uint16_t newColor1 = (uint16_t)((((oldColor1 & 0xf800) * inverseAlpha + red) >> 8 & 0xf800) |
                                              (((oldColor1 & 0x07e0) * inverseAlpha + green) >> 8 & 0x07e0) |
                                              (((oldColor1 & 0x001f) * inverseAlpha + blue) >> 8 & 0x001f));

        if (blockStride == 8 || (oldColor1 < oldColor0) == (newColor1 < newColor0)) {
            block->color0 = newColor0;
            block->color1 = newColor1;
        } else {
            block->color0 = newColor1;
            block->color1 = newColor0;
            block->colorIndices ^= (~block->colorIndices >> 1) & UINT32_C(0x55555555);
        }
        blocks += blockStride;
    }
}

/* Source: CoDUOMP.exe 0x00505ca0..0x00505cf6.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00505ca0_00505cf7.mcode.
 * Name and source boundary: exact same-module Mac symbol
 * R_BlendOverTexture. */
void R_BlendOverTexture(uint8_t *pixels, int32_t pixelCount, const uint8_t blendColor[4], uint32_t format)
{
    switch (format) {
    case GL_RGBA:
        R_BlendOverTexture_RGBA(pixels, pixelCount, blendColor);
        break;
    case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        R_BlendOverTexture_S3TC(pixels, pixelCount, blendColor, 8);
        break;
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        R_BlendOverTexture_S3TC(pixels + 8, pixelCount, blendColor, 16);
        break;
    default:
        break;
    }
}

/* Source: CoDUOMP.exe 0x00504e80..0x005050a1.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00504e80_005050a2.mcode.
 * Name and source boundary: exact same-module Mac symbol
 * R_LightScaleTexture. R_SetColorMappings owns the four lookup tables; this
 * routine selects and composes them according to output-gamma support and the
 * image's overbright policy. */
void R_LightScaleTexture(uint8_t *pixels, int32_t width, int32_t height, qboolean onlyGamma, qboolean noOverbright, uint32_t format)
{
    const uint8_t *firstTable = NULL;
    const uint8_t *secondTable = NULL;

    if (format != GL_RGBA)
        return;
    if (!onlyGamma)
        firstTable = rendererIntensityTable;

    if (coduomp_gamma_output_available() != qfalse) {
        if (noOverbright)
            secondTable = rendererInverseOverbrightTable;
    } else {
        secondTable = noOverbright ? rendererGammaTable : rendererGammaOverbrightTable;
    }

    if (firstTable == NULL && secondTable == NULL)
        return;

    const int32_t pixelCount = (int32_t)((uint32_t)width * (uint32_t)height);
    for (int32_t pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex) {
        for (int32_t channel = 0; channel < 3; ++channel) {
            uint8_t value = pixels[channel];

            if (firstTable != NULL)
                value = firstTable[value];
            if (secondTable != NULL)
                value = secondTable[value];
            pixels[channel] = value;
        }
        pixels += 4;
    }
}

/* Source: CoDUOMP.exe 0x00505470..0x00505519.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00505470_0050551a.mcode.
 * Name and source boundary: exact same-module Mac symbol R_MipMap8. */
void R_MipMap8(uint8_t *pixels, int32_t width, int32_t height)
{
    if (width == 1 && height == 1)
        return;

    const int32_t outputWidth = width >> 1;
    const int32_t outputHeight = height >> 1;
    uint8_t *source = pixels;
    uint8_t *destination = pixels;

    if (outputWidth == 0 || outputHeight == 0) {
        const int32_t outputCount = outputWidth + outputHeight;

        for (int32_t outputIndex = 0; outputIndex < outputCount; ++outputIndex) {
            *destination++ = (uint8_t)(((int32_t)source[0] + source[1]) >> 1);
            source += 2;
        }
        return;
    }

    for (int32_t outputY = 0; outputY < outputHeight; ++outputY) {
        for (int32_t outputX = 0; outputX < outputWidth; ++outputX) {
            *destination++ = (uint8_t)(((int32_t)source[0] + source[1] + source[width] + source[width + 1]) >> 2);
            source += 2;
        }
        source += width;
    }
}

/* Source: CoDUOMP.exe 0x005051a0..0x00505462.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005051a0_00505463.mcode.
 * Name and source boundary: exact same-module Mac symbol R_MipMap2. The
 * original samples a wrapping 4x4 neighborhood with the separable
 * 1,2,2,1 kernel and writes through temporary hunk storage so input samples
 * are never overwritten before their final use. */
void R_MipMap2(uint8_t *pixels, int32_t width, int32_t height)
{
    const int32_t outputWidth = width >> 1;
    const int32_t outputHeight = height >> 1;
    const uint32_t outputSize = (uint32_t)outputWidth * (uint32_t)outputHeight * 4u;
    uint8_t *output = ri.Hunk_AllocateTempMemory((size_t)outputSize);

    for (int32_t outputY = 0; outputY < outputHeight; ++outputY) {
        for (int32_t outputX = 0; outputX < outputWidth; ++outputX) {
            const uint32_t destinationPixel = (uint32_t)outputY * (uint32_t)outputWidth + (uint32_t)outputX;
            uint8_t *destination = &output[destinationPixel * 4u];

            for (int32_t channel = 0; channel < 4; ++channel) {
                int32_t sum = 0;

                for (int32_t kernelY = 0; kernelY < 4; ++kernelY) {
                    const uint32_t sourceY = ((uint32_t)outputY * 2u + (uint32_t)kernelY - 1u) & ((uint32_t)height - 1u);

                    for (int32_t kernelX = 0; kernelX < 4; ++kernelX) {
                        const uint32_t sourceX = ((uint32_t)outputX * 2u + (uint32_t)kernelX - 1u) & ((uint32_t)width - 1u);
                        const uint32_t sourcePixel = sourceY * (uint32_t)width + sourceX;
                        /* The optimized original emits the 1/2/4 weighted
                         * additions directly and has no kernel object. */
                        const int32_t weight = (1 + (kernelX == 1 || kernelX == 2)) * (1 + (kernelY == 1 || kernelY == 2));
                        sum += weight * pixels[sourcePixel * 4u + (uint32_t)channel];
                    }
                }
                destination[channel] = (uint8_t)(sum / 36);
            }
        }
    }

    memcpy(pixels, output, outputSize);
    ri.Hunk_FreeTempMemory(output);
}

/* Source: CoDUOMP.exe 0x00505520..0x00505acf.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00505520_00505ad0.mcode.
 * Name and source boundary: exact same-module Mac symbol R_MipMap. Custom
 * mip names use the mutable "#N" suffix carried by UploadImage: this routine
 * advances it before asking R_LoadImage for the next level. Compressed images
 * already contain their mip chain, so their result advances over one packed
 * level rather than modifying its bytes. */
uint8_t *R_MipMap(char *name, uint8_t *pixels, int32_t width, int32_t height, uint32_t format, renderer_image_load_mode_t loadMode)
{
    if (format == GL_LUMINANCE) {
        R_MipMap8(pixels, width, height);
        return pixels;
    }

    if (format >= GL_COMPRESSED_RGB_S3TC_DXT1_EXT && format <= GL_COMPRESSED_RGBA_S3TC_DXT5_EXT) {
        return pixels + GetCardMemoryAmount(format, width, height);
    }

    if (format != GL_RGBA)
        return pixels;

    char *mipMarker = strstr(name, "#");
    if (mipMarker != NULL) {
        char *suffix = mipMarker + 1;
        char nextMipName[R_IMAGE_NAME_SIZE];
        uint8_t *loadedPixels;
        uint16_t loadedWidth;
        uint16_t loadedHeight;
        uint32_t loadedFormat;
        qboolean mipMapsAvailable;

        while (coduo_crt_isalnum((unsigned char)*suffix))
            ++suffix;

        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
        const int32_t nextMipNameLength = Com_sprintf(nextMipName, sizeof(nextMipName), "%.*s%i%s", (int32_t)(mipMarker + 1 - name), name,
                                                      coduo_crt_atoi(mipMarker + 1) + 1, suffix);
        if (nextMipNameLength < 0 || (size_t)nextMipNameLength >= sizeof(nextMipName)) {
            ri.Printf(R_PRINT_WARNING, "WARNING: custom mipmap image name '%s' is too long\n", name);
            return NULL;
        }
        strcpy(name, nextMipName);

        R_LoadImage(name, &loadedPixels, &loadedWidth, &loadedHeight, &loadedFormat, &mipMapsAvailable, loadMode);
        if (loadedPixels != NULL) {
            const int32_t expectedWidth = (int32_t)((uint32_t)width + 1u) >> 1;
            const int32_t expectedHeight = (int32_t)((uint32_t)height + 1u) >> 1;

            if (loadedWidth == expectedWidth && loadedHeight == expectedHeight) {
                const uint32_t loadedBytes = (uint32_t)loadedWidth * (uint32_t)loadedHeight * 4u;
                memcpy(pixels, loadedPixels, (size_t)loadedBytes);
                return pixels;
            }

            ri.Printf(R_PRINT_WARNING,
                      "WARNING: custom mipmap image '%s' should have dimensions "
                      "%i x %i instead of %i x %i, refusing to load\n",
                      name, expectedWidth, expectedHeight, (int32_t)loadedWidth, (int32_t)loadedHeight);
        }
    }

    if (r_simpleMipMaps->integer == 0) {
        R_MipMap2(pixels, width, height);
        return pixels;
    }

    if (width == 1 && height == 1)
        return pixels;

    const int32_t sourceRowBytes = (int32_t)((uint32_t)width * 4u);
    const int32_t outputWidth = width >> 1;
    const int32_t outputHeight = height >> 1;
    uint8_t *source = pixels;
    uint8_t *destination = pixels;

    if (outputWidth == 0 || outputHeight == 0) {
        const int32_t outputCount = (int32_t)((uint32_t)outputWidth + (uint32_t)outputHeight);

        for (int32_t outputIndex = 0; outputIndex < outputCount; ++outputIndex) {
            for (int32_t channel = 0; channel < 4; ++channel) {
                destination[channel] = (uint8_t)(((int32_t)source[channel] + source[channel + 4]) >> 1);
            }
            source += 8;
            destination += 4;
        }
        return pixels;
    }

    const float centerWeight = r_weightMipMaps->value;
    if (centerWeight <= 0.25f || centerWeight > 1.0f) {
        for (int32_t outputY = 0; outputY < outputHeight; ++outputY) {
            for (int32_t outputX = 0; outputX < outputWidth; ++outputX) {
                for (int32_t channel = 0; channel < 4; ++channel) {
                    destination[channel] = (uint8_t)(((int32_t)source[channel] + source[channel + 4] + source[sourceRowBytes + channel] +
                                                      source[sourceRowBytes + channel + 4]) >>
                                                     2);
                }
                source += 8;
                destination += 4;
            }
            source += sourceRowBytes;
        }
        return pixels;
    }

    const float neighborWeight = (float)(0.25L * (1.0L - (long double)centerWeight));
    for (int32_t outputY = 0; outputY < outputHeight; ++outputY) {
        for (int32_t outputX = 0; outputX < outputWidth; ++outputX) {
            for (int32_t channel = 0; channel < 4; ++channel) {
                const int32_t sampleSum = (int32_t)source[channel] + source[channel + 4] + source[sourceRowBytes + channel] +
                                          source[sourceRowBytes + channel + 4];
                destination[channel] = coduo_fp_to_u8_extended((long double)centerWeight * (long double)source[channel] +
                                                               (long double)neighborWeight * (long double)sampleSum);
            }
            source += 8;
            destination += 4;
        }
        source += sourceRowBytes;
    }

    return pixels;
}

/* Source: CoDUOMP.exe 0x00505e60..0x0050625a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00505e60_0050625b.mcode.
 * Name and source boundary: exact same-module Mac symbol UploadImage. The
 * card-memory result counts every GL level actually uploaded. Texture-memory
 * accounting deliberately omits the first requested source mips according to
 * the original 2-picmip rule; it is therefore not just an alias of the card
 * allocation total. */
qboolean UploadImage(const char *name, uint8_t *pixels, uint32_t textureTarget, uint32_t uploadTarget, uint32_t format, int32_t width,
                     int32_t height, uint32_t flags, qboolean isLightmap, uint32_t *internalFormat, uint16_t *uploadWidth,
                     uint16_t *uploadHeight, int32_t *cardMemory, int32_t *textureMemory)
{
    char mipName[MAX_QPATH];
    int32_t sourceWidth = width;
    int32_t sourceHeight = height;

    strcpy(mipName, name);
    *cardMemory = 0;
    *textureMemory = 0;

    if ((width & (width - 1)) != 0 || (height & (height - 1)) != 0) {
        ri.Printf(R_PRINT_WARNING,
                  "WARNING: image '%s' is %i x %i, which is not a power of "
                  "2 on both sides\n",
                  name, width, height);
        return qfalse;
    }

    const int32_t picmip = R_PicmipForImageFlags(flags);
    int32_t textureMipSkips = 0;
    if ((flags & IMAGE_FLAG_ALLOW_PICMIP) != 0) {
        textureMipSkips = 2 - picmip;
        if (textureMipSkips < 0)
            textureMipSkips = 0;
    }

    int32_t scaledWidth = width >> picmip;
    int32_t scaledHeight = height >> picmip;
    while (scaledWidth > glConfig.maxTextureSize || scaledHeight > glConfig.maxTextureSize) {
        scaledWidth >>= 1;
        scaledHeight >>= 1;
    }
    if (scaledWidth < 1)
        scaledWidth = 1;
    if (scaledHeight < 1)
        scaledHeight = 1;

    const uint32_t selectedInternalFormat = PickInternalFormat(pixels, format, width, height, flags, isLightmap);
    const qboolean generateMipMaps = (flags & IMAGE_FLAG_MIPMAP) != 0 ? qtrue : qfalse;

    if (scaledWidth == width && scaledHeight == height && generateMipMaps == qfalse) {
        R_TexImage2D(uploadTarget, 0, selectedInternalFormat, scaledWidth, scaledHeight, format, pixels);

        *uploadWidth = (uint16_t)scaledWidth;
        *uploadHeight = (uint16_t)scaledHeight;
        *internalFormat = selectedInternalFormat;
        const int32_t levelMemory = GetCardMemoryAmount(selectedInternalFormat, scaledWidth, scaledHeight);
        *cardMemory = (int32_t)((uint32_t)*cardMemory + (uint32_t)levelMemory);
        *textureMemory = (int32_t)((uint32_t)*textureMemory + (uint32_t)levelMemory);
    } else {
        while (sourceWidth > scaledWidth || sourceHeight > scaledHeight) {
            pixels = R_MipMap(mipName, pixels, sourceWidth, sourceHeight, format, R_IMAGE_LOAD_PIXELS);
            if (pixels == NULL)
                return qfalse;

            sourceWidth >>= 1;
            sourceHeight >>= 1;
            if (sourceWidth < 1)
                sourceWidth = 1;
            if (sourceHeight < 1)
                sourceHeight = 1;
        }

        R_LightScaleTexture(pixels, scaledWidth, scaledHeight, generateMipMaps == qfalse ? qtrue : qfalse,
                            (flags & IMAGE_FLAG_NO_OVERBRIGHT) != 0 ? qtrue : qfalse, format);

        *uploadWidth = (uint16_t)scaledWidth;
        *uploadHeight = (uint16_t)scaledHeight;
        *internalFormat = selectedInternalFormat;
        int32_t levelMemory = GetCardMemoryAmount(selectedInternalFormat, scaledWidth, scaledHeight);
        *cardMemory = (int32_t)((uint32_t)*cardMemory + (uint32_t)levelMemory);
        if (textureMipSkips == 0) {
            *textureMemory = (int32_t)((uint32_t)*textureMemory + (uint32_t)levelMemory);
        } else if (generateMipMaps == qfalse) {
            const int32_t skippedLevelMemory = levelMemory >> (textureMipSkips * 2);
            *textureMemory = (int32_t)((uint32_t)*textureMemory + (uint32_t)skippedLevelMemory);
        } else {
            --textureMipSkips;
        }

        R_TexImage2D(uploadTarget, 0, selectedInternalFormat, scaledWidth, scaledHeight, format, pixels);

        if (generateMipMaps != qfalse) {
            int32_t mipLevel = 0;

            while (scaledWidth > 1 || scaledHeight > 1) {
                pixels = R_MipMap(mipName, pixels, scaledWidth, scaledHeight, format, R_IMAGE_LOAD_PIXELS);
                if (pixels == NULL)
                    return qfalse;

                scaledWidth >>= 1;
                scaledHeight >>= 1;
                if (scaledWidth < 1)
                    scaledWidth = 1;
                if (scaledHeight < 1)
                    scaledHeight = 1;
                ++mipLevel;

                levelMemory = GetCardMemoryAmount(selectedInternalFormat, scaledWidth, scaledHeight);
                *cardMemory = (int32_t)((uint32_t)*cardMemory + (uint32_t)levelMemory);
                if (textureMipSkips == 0) {
                    *textureMemory = (int32_t)((uint32_t)*textureMemory + (uint32_t)levelMemory);
                } else {
                    --textureMipSkips;
                }

                if (r_colorMipLevels->integer != 0) {
                    const int32_t mipPixelCount = (int32_t)((uint32_t)scaledWidth * (uint32_t)scaledHeight);
                    R_BlendOverTexture(pixels, mipPixelCount, mipBlendColors[mipLevel], format);
                }
                R_TexImage2D(uploadTarget, mipLevel, selectedInternalFormat, scaledWidth, scaledHeight, format, pixels);
            }
        }
    }

    if (generateMipMaps == qfalse) {
        qglTexParameteri(textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        qglTexParameteri(textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
        qglTexParameteri(textureTarget, GL_TEXTURE_MIN_FILTER, rendererTextureMinFilter);
        qglTexParameteri(textureTarget, GL_TEXTURE_MAG_FILTER, rendererTextureMagFilter);
    }
    GL_CheckErrors("uploading an image");
    return qtrue;
}

/* Source: CoDUOMP.exe 0x00507f30..0x0050861d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00507f30_0050861e.mcode.
 * Name and ordinary four-argument signature: exact same-module Mac symbol
 * R_LoadCubeMapImage. The special $renormalize image is generated in memory;
 * ordinary cube maps load six named faces, validate a common square layout,
 * and apply the original per-face orientation transforms before upload. */
image_t *R_LoadCubeMapImage(const char *name, uint32_t flags, renderer_image_track_t imageTrack, const float colorScale[4])
{
    /* The original constructs these tables in its stack frame at
     * 0x00507f51..0x00508132. The suffix pointers target the six literals at
     * 0x005b22fc..0x005b2313 in rt/lf/bk/ft/up/dn order. */
    const char *const rendererCubeFaceSuffixes[R_CUBE_FACE_COUNT] = {"rt", "lf", "bk", "ft", "up", "dn"};
    const int32_t rendererCubeDirectionAxes[R_CUBE_FACE_COUNT][3] = {{0, 2, 1}, {0, 2, 1}, {1, 0, 2}, {1, 0, 2}, {2, 0, 1}, {2, 0, 1}};
    const float rendererCubeDirectionSigns[R_CUBE_FACE_COUNT][3] = {{1.0f, -1.0f, -1.0f}, {-1.0f, 1.0f, -1.0f}, {1.0f, 1.0f, 1.0f},
                                                                    {-1.0f, 1.0f, -1.0f}, {1.0f, 1.0f, -1.0f},  {-1.0f, -1.0f, -1.0f}};
    uint8_t *facePixels[R_CUBE_FACE_COUNT];
    int32_t width;
    int32_t height;

    if (Q_stricmp(name, "$renormalize") == 0) {
        const float coordinateScale = 1.0f / R_RENORMALIZE_CUBE_SIZE;
        const float encodeScale = 127.5f;

        width = R_RENORMALIZE_CUBE_SIZE;
        height = R_RENORMALIZE_CUBE_SIZE;
        flags &= ~(IMAGE_FLAG_MIPMAP | IMAGE_FLAG_ALLOW_PICMIP | IMAGE_FLAG_USE_PICMIP2);

        for (int32_t face = 0; face < R_CUBE_FACE_COUNT; ++face) {
            facePixels[face] = R_AllocTempMemory((size_t)R_RENORMALIZE_CUBE_SIZE * R_RENORMALIZE_CUBE_SIZE * R_PIXEL_COMPONENT_COUNT);

            for (int32_t row = 0; row < R_RENORMALIZE_CUBE_SIZE; ++row) {
                const float rowCoordinate = ((float)(row * 2 + 1) * coordinateScale) - 1.0f;

                for (int32_t column = 0; column < R_RENORMALIZE_CUBE_SIZE; ++column) {
                    const float columnCoordinate = ((float)(column * 2 + 1) * coordinateScale) - 1.0f;
                    vec3_t direction;
                    vec3_t normal;

                    direction[rendererCubeDirectionAxes[face][0]] = rendererCubeDirectionSigns[face][0];
                    direction[rendererCubeDirectionAxes[face][2]] = rendererCubeDirectionSigns[face][2] * rowCoordinate;
                    direction[rendererCubeDirectionAxes[face][1]] = rendererCubeDirectionSigns[face][1] * columnCoordinate;
                    VectorNormalize2(direction, normal);

                    uint8_t *pixel = &facePixels[face][(row * R_RENORMALIZE_CUBE_SIZE + column) * R_PIXEL_COMPONENT_COUNT];
                    pixel[0] = (uint8_t)((normal[0] + 1.0f) * encodeScale);
                    pixel[1] = (uint8_t)((normal[1] + 1.0f) * encodeScale);
                    pixel[2] = (uint8_t)((normal[2] + 1.0f) * encodeScale);
                    pixel[3] = UINT8_MAX;
                }
            }
        }
    } else {
        const size_t baseNameLength = strlen(name);
        char faceName[R_IMAGE_NAME_SIZE];
        uint32_t commonFormat = 0;

        if (baseNameLength + sizeof("_**.tga") - 1 >= R_IMAGE_NAME_SIZE) {
            ri.Error(ERR_DROP, "Cube map name will exceed max qpath\n");
            return NULL;
        }

        strcpy(faceName, name);
        strcat(faceName, "_**.tga");
        char *faceSuffix = faceName + baseNameLength + 1;
        width = 0;
        height = 0;

        for (int32_t face = 0; face < R_CUBE_FACE_COUNT; ++face) {
            uint16_t faceWidth;
            uint16_t faceHeight;
            uint32_t faceFormat;
            qboolean mipMapsAvailable;

            strcpy(faceSuffix, rendererCubeFaceSuffixes[face]);
            R_LoadImage(faceName, &facePixels[face], &faceWidth, &faceHeight, &faceFormat, &mipMapsAvailable, R_IMAGE_LOAD_PIXELS);
            if (facePixels[face] == NULL)
                return NULL;

            if (face == 0) {
                width = faceWidth;
                height = faceHeight;
                commonFormat = faceFormat;
                if (width != height) {
                    ri.Error(ERR_DROP, "Cube map face images should be square ('%s')\n", faceName);
                    return NULL;
                }
            } else {
                if (width != faceWidth || height != faceHeight) {
                    ri.Error(ERR_DROP,
                             "Cube map face images are not all the same size "
                             "('%s')\n",
                             faceName);
                    return NULL;
                }
                if (commonFormat != faceFormat) {
                    ri.Error(ERR_DROP,
                             "Cube map face images are not all the same format "
                             "('%s')\n",
                             faceName);
                    return NULL;
                }
            }
        }

        R_FlipImageDiagonally(facePixels[0], width, height);
        R_FlipImageDiagonally(facePixels[1], width, height);
        R_FlipImageHorizontally(facePixels[1], width, height);
        R_FlipImageVertically(facePixels[1], width, height);
        R_FlipImageVertically(facePixels[2], width, height);
        R_FlipImageHorizontally(facePixels[3], width, height);
        R_FlipImageDiagonally(facePixels[4], width, height);
        R_FlipImageDiagonally(facePixels[5], width, height);
    }

    image_t *image =
        R_AllocImage(name, GL_TEXTURE_CUBE_MAP_ARB, width, height, flags | IMAGE_FLAG_CLAMP_S | IMAGE_FLAG_CLAMP_T, (int32_t)imageTrack);

    for (int32_t face = 0; face < R_CUBE_FACE_COUNT; ++face) {
        if (R_CreateImageInternal(image, facePixels[face], GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB + (uint32_t)face, GL_RGBA, colorScale) ==
            qfalse) {
            R_FreeImage(image);
            return NULL;
        }
    }
    return image;
}

/* Source: CoDUOMP.exe 0x00508620..0x00508740.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00508620_00508741.mcode.
 * Name: exact same-module Mac symbol R_FindExistingImage. Existing image
 * records are keyed by case-insensitive extensionless hash/name matching. */
image_t *R_FindExistingImage(const char *name, uint32_t textureTarget, uint32_t flags, renderer_image_track_t imageTrack)
{
    const int32_t hash = generateHashValue(name);
    image_t *image = imageHashTable[hash];

    while (image != NULL) {
        if (Q_stricmpn(image->imgName, name, 99999) == 0)
            break;
        image = image->hashNext;
    }
    if (image == NULL)
        return NULL;

    const uint32_t changedFlags = image->flags ^ flags;
    if (changedFlags != 0 && strcmp(name, "*white") != 0 && strcmp(name, "*gray") != 0) {
        if ((changedFlags & IMAGE_FLAG_MIPMAP) != 0) {
            ri.Printf(R_PRINT_DEVELOPER, "WARNING: reused image %s with mixed mipmap allowance\n", name);
        }
        if ((changedFlags & IMAGE_FLAG_ALLOW_PICMIP) != 0) {
            ri.Printf(R_PRINT_DEVELOPER, "WARNING: reused image %s with mixed picmip allowance\n", name);
        }
        if ((changedFlags & IMAGE_FLAG_USE_PICMIP2) != 0) {
            ri.Printf(R_PRINT_DEVELOPER,
                      "WARNING: reused image %s with mixed picmip type "
                      "(model / world)\n",
                      name);
        }
        if ((changedFlags & (IMAGE_FLAG_CLAMP_S | IMAGE_FLAG_CLAMP_T)) != 0) {
            ri.Printf(R_PRINT_ALL, "WARNING: reused image %s with mixed clamp modes\n", name);
        }
    }

    if (image->target != textureTarget) {
        ri.Printf(R_PRINT_WARNING,
                  "ERROR: image '%s' cannot be used in a cube map and in a "
                  "normal texture",
                  name);
    }

    if (image->imageTrack != imageTrack) {
        ri.Printf(R_PRINT_ALL, "WARNING: image '%s' changed type from %s to %s.\n", name, rendererImageTrackNames[image->imageTrack],
                  rendererImageTrackNames[imageTrack]);
    }
    image->imageTrack = imageTrack;
    return image;
}

/* Source: CoDUOMP.exe 0x00508750..0x005088fa.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00508750_005088fb.mcode.
 * Name and source-level record fallback: exact same-module Mac symbol
 * R_LoadSingleDelayedImage. A failed upload retains the hash-chain identity
 * and requested name while making the logical image behave like the default
 * image. */
void R_LoadSingleDelayedImage(image_t *image)
{
    uint8_t *pixels;
    uint16_t width;
    uint16_t height;
    uint32_t format;
    qboolean mipMapsAvailable;

    image->flags &= ~IMAGE_FLAG_DELAYED_UPLOAD;
    image->link.textureSheet = NULL;
    image->state.sheet.x = 0;
    image->state.sheet.y = 0;
    image->state.sheet.rotated = 0;
    --tr.delayedImageCount;

    R_LoadImage(image->imgName, &pixels, &width, &height, &format, &mipMapsAvailable, R_IMAGE_LOAD_PIXELS);

    if (pixels == NULL || width != image->width || height != image->height || format != image->internalFormat ||
        (format != GL_RGBA && mipMapsAvailable == qfalse && (image->flags & IMAGE_FLAG_MIPMAP) != 0)) {
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
        R_FreeImageAllocations();
        ri.Printf(R_PRINT_DEVELOPER,
                  "\x15"
                  "delayed-load image '%s' changed from %ix%i type 0x%04x %s "
                  "mipmaps to %ix%i type 0x%04x %s mipmaps\n",
                  image->imgName, width, height, format, (image->flags & IMAGE_FLAG_MIPMAP) != 0 ? "with" : "without", image->width,
                  image->height, image->internalFormat, mipMapsAvailable != qfalse ? "with" : "without");
        pixels = NULL;
    }

    if (pixels == NULL || R_CreateImageInternal(image, pixels, GL_TEXTURE_2D, format, NULL) == qfalse) {
        char requestedName[MAX_QPATH];
        image_t *hashNext;

        strcpy(requestedName, image->imgName);
        hashNext = image->hashNext;
        if (glState.currenttextures[glState.currenttmu] == image->texnum) {
            qglBindTexture(image->target, 0);
            glState.currenttextures[glState.currenttmu] = 0;
        }
        qglDeleteTextures(1, &image->texnum);

        *image = *tr.defaultImage;
        strcpy(image->imgName, requestedName);
        image->hashNext = hashNext;
    }

    R_FreeImageAllocations();
}

/* Source: CoDUOMP.exe 0x00508900..0x0050897c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00508900_0050897d.mcode.
 * Name and source-level signature: exact same-module Mac symbol
 * R_UpdateDelayLoadImage. Images in the same delayed group move together when
 * the current shader cannot be merged with the shader already recorded. */
void R_UpdateDelayLoadImage(image_t *image, shader_t *shader, qboolean forceLoad)
{
    if ((image->flags & IMAGE_FLAG_DELAYED_UPLOAD) == 0)
        return;

    if (forceLoad != qfalse) {
        R_LoadSingleDelayedImage(image);
        return;
    }

    if (image->link.delayedShader == shader)
        return;

    if (image->link.delayedShader == NULL) {
        image->link.delayedShader = shader;
    } else if (CompareMergableShaders(image->link.delayedShader, shader, image, image) != 0) {
        R_LoadSingleDelayedImage(image);
        return;
    }

    const int32_t previousGroup = image->state.delay.group;
    if (previousGroup == tr.delayedImageGroup)
        return;

    for (int32_t imageIndex = 0; imageIndex < tr.imageCount; ++imageIndex) {
        image_t *groupImage = tr.images[imageIndex];
        if (groupImage != NULL && (groupImage->flags & IMAGE_FLAG_DELAYED_UPLOAD) != 0 && groupImage->state.delay.group == previousGroup) {
            groupImage->state.delay.group = tr.delayedImageGroup;
        }
    }
}

/* Source: CoDUOMP.exe 0x00508980..0x00508c4d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00508980_00508c4e.mcode.
 * Name and ordinary six-argument signature: exact same-module Mac symbol
 * R_FindImageFile. Windows instructions prove delayed-metadata selection,
 * existing-sheet rejection, height-map conversion gates, and the cleanup on
 * every new image-load path. */
image_t *R_FindImageFile(const char *name, uint32_t textureTarget, uint32_t flags, renderer_image_track_t imageTrack,
                         const float colorScale[4], float heightScale)
{
    if (name == NULL)
        return NULL;

    if (r_picmip->integer == 0 || r_picmip2->integer == 0)
        flags &= ~IMAGE_FLAG_DELAYED_UPLOAD;

    if (imageTrack == R_IMAGE_TRACK_GENERATED_TEXTURE) {
        name = R_MangleTextureName(name, textureTarget, flags, colorScale, heightScale);
        if (name == NULL)
            return NULL;
    }

    image_t *image = R_FindExistingImage(name, textureTarget, flags, imageTrack);
    if (image != NULL) {
        if ((image->flags & IMAGE_FLAG_DELAYED_UPLOAD) != 0) {
            R_UpdateDelayLoadImage(image, NULL, (flags & IMAGE_FLAG_DELAYED_UPLOAD) == 0 ? qtrue : qfalse);
            if ((image->flags & IMAGE_FLAG_DELAYED_UPLOAD) != 0) {
                image->state.delay.groupTriCount += tr.delayedImageGroupTriCount;
            }
            return image;
        }

        if (image->link.textureSheet != NULL) {
            /* 0x00508a3c..0x00508a4a calls renderer import slot 1
             * (Error) with drop level 1. */
            ri.Error(ERR_DROP,
                     "\x15"
                     "tried to load image '%s', which has been merged with "
                     "other textures, after the map finished loading\n",
                     name);
            return NULL;
        }
        return image;
    }

    if (imageTrack == R_IMAGE_TRACK_GENERATED_TEXTURE) {
        image = R_AllocImage(name, textureTarget, 0, 0, flags, (int32_t)imageTrack);
        R_FreeImageAllocations();
        return image;
    }

    if (textureTarget == GL_TEXTURE_2D) {
        if ((flags & IMAGE_FLAG_DELAYED_UPLOAD) != 0) {
            if ((flags & IMAGE_FLAG_HEIGHT_TO_NORMAL) != 0) {
                flags &= ~IMAGE_FLAG_DELAYED_UPLOAD;
            } else if (colorScale != NULL) {
                qboolean identityScale = qtrue;
                for (int32_t channel = 0; channel < 4; ++channel) {
                    uint32_t componentBits;
                    memcpy(&componentBits, &colorScale[channel], sizeof(componentBits));
                    if (componentBits != UINT32_C(0x3f800000)) {
                        identityScale = qfalse;
                        break;
                    }
                }
                if (identityScale == qfalse)
                    flags &= ~IMAGE_FLAG_DELAYED_UPLOAD;
            }
        }

        uint8_t *pixels;
        uint16_t width;
        uint16_t height;
        uint32_t format;
        qboolean mipMapsAvailable;
        const renderer_image_load_mode_t loadMode = (flags & IMAGE_FLAG_DELAYED_UPLOAD) != 0 ? R_IMAGE_LOAD_METADATA : R_IMAGE_LOAD_PIXELS;

        R_LoadImage(name, &pixels, &width, &height, &format, &mipMapsAvailable, loadMode);


        if (mipMapsAvailable == qfalse && (flags & IMAGE_FLAG_MIPMAP) != 0) {
            /* 0x00508b01..0x00508b20 calls renderer import slot 0
             * (Printf), not slot 1 (Error), at warning level 2. The original
             * then disables mipmapping for this image and continues loading. */
            ri.Printf(R_PRINT_WARNING, "image '%s' requested mipmaps, but they aren't available\n", name);
            flags &= ~(IMAGE_FLAG_MIPMAP | IMAGE_FLAG_ALLOW_PICMIP | IMAGE_FLAG_USE_PICMIP2);

            /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
            image = R_CreateImage(name, pixels, width, height, format, flags, (int32_t)imageTrack, colorScale);
            R_FreeImageAllocations();
            return image;
        }

        image = NULL;
        if (format != 0) {
            if ((flags & IMAGE_FLAG_HEIGHT_TO_NORMAL) == 0) {
                image = R_CreateImage(name, pixels, width, height, format, flags, (int32_t)imageTrack, colorScale);
            } else if (format == GL_RGBA) {
                if ((flags & IMAGE_FLAG_DELAYED_UPLOAD) == 0)
                    R_HeightmapImage(pixels, width, height, heightScale);
                image = R_CreateImage(name, pixels, width, height, GL_RGBA, flags, (int32_t)imageTrack, colorScale);
            } else if (format >= GL_COMPRESSED_RGB_S3TC_DXT1_EXT && format <= GL_COMPRESSED_RGBA_S3TC_DXT5_EXT) {
                /* 0x00508b75..0x00508c3a joins the warning-only renderer
                 * Printf tail and returns no image. */
                ri.Printf(R_PRINT_WARNING,
                          "image '%s': heightToNormal not valid for DDS "
                          "textures\n",
                          name);
            }
        }

        R_FreeImageAllocations();
        return image;
    }

    if (textureTarget == GL_TEXTURE_CUBE_MAP_ARB) {
        image = R_LoadCubeMapImage(name, flags, imageTrack, colorScale);
        R_FreeImageAllocations();
        return image;
    }

    /* 0x00508c2c..0x00508c3a reports this through renderer import slot 0
     * at warning level 2, then frees transient image allocations. */
    ri.Printf(R_PRINT_WARNING, "Unknown texture target 0x%04x\n", textureTarget);
    R_FreeImageAllocations();
    return NULL;
}

/* Source: CoDUOMP.exe 0x00508c50..0x00508d50.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00508c50_00508d51.mcode.
 * Name and ordinary five-argument signature: exact same-module Mac symbol
 * R_FindImageInstance. The mangled key carries target, flags, color scale, and
 * height scale; identity color scale is passed onward as NULL. */
image_t *R_FindImageInstance(const char *originalName, renderer_image_track_t imageTrack, const char *mangledName, shader_t *shader,
                             qboolean forceLoad)
{
    char imageName[R_IMAGE_NAME_SIZE];
    uint32_t textureTarget;
    uint32_t flags;
    float colorScale[4];
    float heightScale;

    R_UnmangleTextureName(originalName, mangledName, imageName, &textureTarget, &flags, colorScale, &heightScale);

    const float *effectiveColorScale = colorScale;
    if (colorScale[0] == 1.0f && colorScale[1] == 1.0f && colorScale[2] == 1.0f && colorScale[3] == 1.0f) {
        effectiveColorScale = NULL;
    }

    if (forceLoad == qfalse && r_picmip2->integer != 0 && r_picmip->integer != 0 && (flags & IMAGE_FLAG_NO_TEXTURE_SHEET) == 0) {
        flags |= IMAGE_FLAG_DELAYED_UPLOAD;
    } else {
        flags &= ~IMAGE_FLAG_DELAYED_UPLOAD;
    }

    image_t *image = R_FindImageFile(imageName, textureTarget, flags, imageTrack, effectiveColorScale, heightScale);
    if (image != NULL)
        R_UpdateDelayLoadImage(image, shader, forceLoad);
    return image;
}

/* Source: CoDUOMP.exe 0x00509410..0x00509469.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00509410_0050946a.mcode.
 * Name and seven-argument signature: exact same-module Mac symbol
 * CopyImageTile_RGBA. Source rows are tightly packed; destination rows use the
 * owning texture-sheet image width. */
void CopyImageTile_RGBA(const image_t *destinationImage, uint8_t *destinationPixels, const uint8_t *sourcePixels, uint16_t destinationX,
                        uint16_t destinationY, uint16_t sourceWidth, uint16_t sourceHeight)
{
    const uint32_t sourceRowBytes = (uint32_t)sourceWidth * R_PIXEL_COMPONENT_COUNT;
    const uint32_t destinationRowBytes = (uint32_t)destinationImage->width * R_PIXEL_COMPONENT_COUNT;
    const uint32_t destinationPixelOffset =
        ((uint32_t)destinationY * (uint32_t)destinationImage->width + (uint32_t)destinationX) * R_PIXEL_COMPONENT_COUNT;
    uint8_t *destination = destinationPixels + (size_t)destinationPixelOffset;

    for (uint16_t row = 0; row < sourceHeight; ++row) {
        memcpy(destination, sourcePixels, sourceRowBytes);
        sourcePixels += sourceRowBytes;
        destination += destinationRowBytes;
    }
}

/* Source: CoDUOMP.exe 0x00508e00..0x00508ef0.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00508e00_00508ef1.mcode.
 * Name and signature: exact same-module Mac symbol CopyImageTileLevel_DXT1.
 * DXT tile coordinates and dimensions are pixel units, while each copied or
 * transposed unit is one 4x4 compressed block. A tall tile is stored rotated
 * in the destination sheet and therefore takes the transpose path. */
void CopyImageTileLevel_DXT1(uint16_t destinationWidth, uint8_t *destinationPixels, const uint8_t *sourcePixels, uint16_t destinationX,
                             uint16_t destinationY, uint16_t sourceWidth, uint16_t sourceHeight)
{
    enum {
        DXT1_BLOCK_BYTES = 8,
        DXT_BLOCK_DIMENSION = 4
    };
    uint8_t *destination = destinationPixels + ((size_t)destinationY * destinationWidth / DXT_BLOCK_DIMENSION + destinationX) *
                                                   (DXT1_BLOCK_BYTES / DXT_BLOCK_DIMENSION);

    if (sourceWidth >= sourceHeight) {
        const size_t sourceRowBytes = (size_t)sourceWidth * DXT1_BLOCK_BYTES / DXT_BLOCK_DIMENSION;
        const size_t destinationRowBytes = (size_t)destinationWidth * DXT1_BLOCK_BYTES / DXT_BLOCK_DIMENSION;

        for (uint16_t sourceY = 0; sourceY < sourceHeight; sourceY += DXT_BLOCK_DIMENSION) {
            memcpy(destination, sourcePixels, sourceRowBytes);
            sourcePixels += sourceRowBytes;
            destination += destinationRowBytes;
        }
        return;
    }

    const size_t sourceRowBytes = (size_t)sourceWidth * DXT1_BLOCK_BYTES / DXT_BLOCK_DIMENSION;
    const size_t destinationRowRemainder = (size_t)(destinationWidth - sourceHeight) * DXT1_BLOCK_BYTES / DXT_BLOCK_DIMENSION;

    for (uint16_t sourceX = 0; sourceX < sourceWidth; sourceX += DXT_BLOCK_DIMENSION) {
        const uint8_t *sourceBlock = sourcePixels + (size_t)sourceX * DXT1_BLOCK_BYTES / DXT_BLOCK_DIMENSION;

        for (uint16_t sourceY = 0; sourceY < sourceHeight; sourceY += DXT_BLOCK_DIMENSION) {
            TransposeDDSBlockDXT1(sourceBlock, destination);
            sourceBlock += sourceRowBytes;
            destination += DXT1_BLOCK_BYTES;
        }
        destination += destinationRowRemainder;
    }
}

/* Source: CoDUOMP.exe 0x00508f00..0x0050900d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00508f00_0050900e.mcode.
 * Name and signature: exact same-module Mac symbol CopyImageTile_DXT1. */
void CopyImageTile_DXT1(const image_t *destinationImage, uint8_t *destinationPixels, const uint8_t *sourcePixels, uint16_t destinationX,
                        uint16_t destinationY, uint16_t sourceWidth, uint16_t sourceHeight)
{
    uint16_t destinationWidth = destinationImage->width;
    uint16_t destinationHeight = destinationImage->height;

    CopyImageTileLevel_DXT1(destinationWidth, destinationPixels, sourcePixels, destinationX, destinationY, sourceWidth, sourceHeight);

    if (!(destinationImage->flags & IMAGE_FLAG_MIPMAP))
        return;

    do {
        destinationPixels += GetCardMemoryAmount(destinationImage->internalFormat, destinationWidth, destinationHeight);
        sourcePixels += GetCardMemoryAmount(destinationImage->internalFormat, sourceWidth, sourceHeight);
        destinationX >>= 1;
        destinationY >>= 1;
        sourceWidth >>= 1;
        sourceHeight >>= 1;
        destinationWidth >>= 1;
        destinationHeight >>= 1;

        if ((destinationX & 3) != 0 || (destinationY & 3) != 0)
            break;

        CopyImageTileLevel_DXT1(destinationWidth, destinationPixels, sourcePixels, destinationX, destinationY, sourceWidth, sourceHeight);
    } while (sourceWidth > 1 || sourceHeight > 1);
}

/* Source: CoDUOMP.exe 0x00509010..0x005090ff.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00509010_00509100.mcode.
 * Name and signature: exact same-module Mac symbol CopyImageTileLevel_DXT3. */
void CopyImageTileLevel_DXT3(uint16_t destinationWidth, uint8_t *destinationPixels, const uint8_t *sourcePixels, uint16_t destinationX,
                             uint16_t destinationY, uint16_t sourceWidth, uint16_t sourceHeight)
{
    enum {
        DXT3_BLOCK_BYTES = 16,
        DXT_BLOCK_DIMENSION = 4
    };
    uint8_t *destination = destinationPixels + (size_t)destinationY * destinationWidth + (size_t)destinationX * DXT_BLOCK_DIMENSION;

    if (sourceWidth >= sourceHeight) {
        const size_t sourceRowBytes = (size_t)sourceWidth * DXT_BLOCK_DIMENSION;
        const size_t destinationRowBytes = (size_t)destinationWidth * DXT_BLOCK_DIMENSION;

        for (uint16_t sourceY = 0; sourceY < sourceHeight; sourceY += DXT_BLOCK_DIMENSION) {
            memcpy(destination, sourcePixels, sourceRowBytes);
            sourcePixels += sourceRowBytes;
            destination += destinationRowBytes;
        }
        return;
    }

    const size_t sourceRowBytes = (size_t)sourceWidth * DXT_BLOCK_DIMENSION;
    const size_t destinationRowRemainder = (size_t)(destinationWidth - sourceHeight) * DXT_BLOCK_DIMENSION;

    for (uint16_t sourceX = 0; sourceX < sourceWidth; sourceX += DXT_BLOCK_DIMENSION) {
        const uint8_t *sourceBlock = sourcePixels + (size_t)sourceX * DXT_BLOCK_DIMENSION;

        for (uint16_t sourceY = 0; sourceY < sourceHeight; sourceY += DXT_BLOCK_DIMENSION) {
            TransposeDDSBlockDXT3(sourceBlock, destination);
            sourceBlock += sourceRowBytes;
            destination += DXT3_BLOCK_BYTES;
        }
        destination += destinationRowRemainder;
    }
}

/* Source: CoDUOMP.exe 0x00509100..0x0050920d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00509100_0050920e.mcode.
 * Name and signature: exact same-module Mac symbol CopyImageTile_DXT3. */
void CopyImageTile_DXT3(const image_t *destinationImage, uint8_t *destinationPixels, const uint8_t *sourcePixels, uint16_t destinationX,
                        uint16_t destinationY, uint16_t sourceWidth, uint16_t sourceHeight)
{
    uint16_t destinationWidth = destinationImage->width;
    uint16_t destinationHeight = destinationImage->height;

    CopyImageTileLevel_DXT3(destinationWidth, destinationPixels, sourcePixels, destinationX, destinationY, sourceWidth, sourceHeight);

    if (!(destinationImage->flags & IMAGE_FLAG_MIPMAP))
        return;

    do {
        destinationPixels += GetCardMemoryAmount(destinationImage->internalFormat, destinationWidth, destinationHeight);
        sourcePixels += GetCardMemoryAmount(destinationImage->internalFormat, sourceWidth, sourceHeight);
        destinationX >>= 1;
        destinationY >>= 1;
        sourceWidth >>= 1;
        sourceHeight >>= 1;
        destinationWidth >>= 1;
        destinationHeight >>= 1;

        if ((destinationX & 3) != 0 || (destinationY & 3) != 0)
            break;

        CopyImageTileLevel_DXT3(destinationWidth, destinationPixels, sourcePixels, destinationX, destinationY, sourceWidth, sourceHeight);
    } while (sourceWidth > 1 || sourceHeight > 1);
}

/* Source: CoDUOMP.exe 0x00509210..0x005092ff.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00509210_00509300.mcode.
 * Name and signature: exact same-module Mac symbol CopyImageTileLevel_DXT5. */
void CopyImageTileLevel_DXT5(uint16_t destinationWidth, uint8_t *destinationPixels, const uint8_t *sourcePixels, uint16_t destinationX,
                             uint16_t destinationY, uint16_t sourceWidth, uint16_t sourceHeight)
{
    enum {
        DXT5_BLOCK_BYTES = 16,
        DXT_BLOCK_DIMENSION = 4
    };
    uint8_t *destination = destinationPixels + (size_t)destinationY * destinationWidth + (size_t)destinationX * DXT_BLOCK_DIMENSION;

    if (sourceWidth >= sourceHeight) {
        const size_t sourceRowBytes = (size_t)sourceWidth * DXT_BLOCK_DIMENSION;
        const size_t destinationRowBytes = (size_t)destinationWidth * DXT_BLOCK_DIMENSION;

        for (uint16_t sourceY = 0; sourceY < sourceHeight; sourceY += DXT_BLOCK_DIMENSION) {
            memcpy(destination, sourcePixels, sourceRowBytes);
            sourcePixels += sourceRowBytes;
            destination += destinationRowBytes;
        }
        return;
    }

    const size_t sourceRowBytes = (size_t)sourceWidth * DXT_BLOCK_DIMENSION;
    const size_t destinationRowRemainder = (size_t)(destinationWidth - sourceHeight) * DXT_BLOCK_DIMENSION;

    for (uint16_t sourceX = 0; sourceX < sourceWidth; sourceX += DXT_BLOCK_DIMENSION) {
        const uint8_t *sourceBlock = sourcePixels + (size_t)sourceX * DXT_BLOCK_DIMENSION;

        for (uint16_t sourceY = 0; sourceY < sourceHeight; sourceY += DXT_BLOCK_DIMENSION) {
            TransposeDDSBlockDXT5(sourceBlock, destination);
            sourceBlock += sourceRowBytes;
            destination += DXT5_BLOCK_BYTES;
        }
        destination += destinationRowRemainder;
    }
}

/* Source: CoDUOMP.exe 0x00509300..0x0050940d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00509300_0050940e.mcode.
 * Name and signature: exact same-module Mac symbol CopyImageTile_DXT5. */
void CopyImageTile_DXT5(const image_t *destinationImage, uint8_t *destinationPixels, const uint8_t *sourcePixels, uint16_t destinationX,
                        uint16_t destinationY, uint16_t sourceWidth, uint16_t sourceHeight)
{
    uint16_t destinationWidth = destinationImage->width;
    uint16_t destinationHeight = destinationImage->height;

    CopyImageTileLevel_DXT5(destinationWidth, destinationPixels, sourcePixels, destinationX, destinationY, sourceWidth, sourceHeight);

    if (!(destinationImage->flags & IMAGE_FLAG_MIPMAP))
        return;

    do {
        destinationPixels += GetCardMemoryAmount(destinationImage->internalFormat, destinationWidth, destinationHeight);
        sourcePixels += GetCardMemoryAmount(destinationImage->internalFormat, sourceWidth, sourceHeight);
        destinationX >>= 1;
        destinationY >>= 1;
        sourceWidth >>= 1;
        sourceHeight >>= 1;
        destinationWidth >>= 1;
        destinationHeight >>= 1;

        if ((destinationX & 3) != 0 || (destinationY & 3) != 0)
            break;

        CopyImageTileLevel_DXT5(destinationWidth, destinationPixels, sourcePixels, destinationX, destinationY, sourceWidth, sourceHeight);
    } while (sourceWidth > 1 || sourceHeight > 1);
}

/* Source: CoDUOMP.exe 0x00509470..0x005096cf.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00509470_005096d0.mcode.
 * Name and six-argument source signature: exact same-module Mac symbol
 * LoadImageTile. The allocation-end pointer is carried by the recursive image
 * group upload API but is not dereferenced by this leaf in either binary. */
void LoadImageTile(image_t *image, image_t *destinationImage, uint8_t *destinationPixels, uint8_t *destinationPixelsEnd,
                   uint16_t destinationX, uint16_t destinationY)
{
    uint8_t *sourcePixels;
    uint16_t sourceWidth;
    uint16_t sourceHeight;
    uint32_t format;
    qboolean mipMapsAvailable;

    (void)destinationPixelsEnd;

    image->flags &= ~IMAGE_FLAG_DELAYED_UPLOAD;
    image->texnum = destinationImage->texnum;
    image->link.textureSheet = destinationImage;
    image->state.sheet.x = destinationX;
    image->state.sheet.y = destinationY;
    image->state.sheet.rotated = image->width < image->height;

    --tr.delayedImageCount;
    R_LoadImage(image->imgName, &sourcePixels, &sourceWidth, &sourceHeight, &format, &mipMapsAvailable, R_IMAGE_LOAD_TILE);

    if (sourcePixels == NULL) {
        ri.Error(ERR_DROP,
                 "\x15image '%s' got deleted between initial scan and actual "
                 "load\n",
                 image->imgName);
    }

    if ((uint32_t)sourceWidth + sourceHeight != (uint32_t)image->width + image->height ||
        (sourceWidth != image->width && sourceWidth != image->height) || format != image->internalFormat ||
        (format != GL_RGBA && mipMapsAvailable == qfalse && (image->flags & IMAGE_FLAG_MIPMAP) != 0)) {
        ri.Error(ERR_DROP,
                 "\x15image '%s' changed between initial scan and actual "
                 "load\n",
                 image->imgName);
    }

    int32_t picmip = R_PicmipForImageFlags(image->flags);
    while (picmip != 0 && sourceWidth > 4 && sourceHeight > 4) {
        sourcePixels = R_MipMap(image->imgName, sourcePixels, sourceWidth, sourceHeight, format, R_IMAGE_LOAD_TILE);
        if (sourcePixels == NULL) {
            ri.Error(ERR_DROP,
                     "\x15tried to put image '%s' on a texture sheet, but it "
                     "has an invalid custom mipmap\n",
                     image->imgName);
        }
        sourceWidth >>= 1;
        sourceHeight >>= 1;
        --picmip;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    while (sourceWidth > glConfig.maxTextureSize || sourceHeight > glConfig.maxTextureSize) {
        sourcePixels = R_MipMap(image->imgName, sourcePixels, sourceWidth, sourceHeight, format, R_IMAGE_LOAD_TILE);
        if (sourcePixels == NULL) {
            ri.Error(ERR_DROP,
                     "\x15tried to put image '%s' on a texture sheet, but it "
                     "has an invalid custom mipmap\n",
                     image->imgName);
        }
        sourceWidth >>= 1;
        sourceHeight >>= 1;
        if (sourceWidth < 1)
            sourceWidth = 1;
        if (sourceHeight < 1)
            sourceHeight = 1;
    }

    uint16_t storedWidth = sourceWidth;
    uint16_t storedHeight = sourceHeight;
    switch (format) {
    case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        if (sourceWidth < sourceHeight) {
            storedWidth = sourceHeight;
            storedHeight = sourceWidth;
        }
        break;
    default:
        break;
    }

    if (destinationX > destinationImage->width || destinationY > destinationImage->height ||
        storedWidth > destinationImage->width - destinationX || storedHeight > destinationImage->height - destinationY) {
        ri.Error(ERR_DROP, "\x15image '%s' does not fit its texture sheet\n", image->imgName);
    }

    switch (destinationImage->internalFormat) {
    case GL_RGBA:
        CopyImageTile_RGBA(destinationImage, destinationPixels, sourcePixels, destinationX, destinationY, sourceWidth, sourceHeight);
        break;
    case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        CopyImageTile_DXT1(destinationImage, destinationPixels, sourcePixels, destinationX, destinationY, sourceWidth, sourceHeight);
        break;
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
        CopyImageTile_DXT3(destinationImage, destinationPixels, sourcePixels, destinationX, destinationY, sourceWidth, sourceHeight);
        break;
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        CopyImageTile_DXT5(destinationImage, destinationPixels, sourcePixels, destinationX, destinationY, sourceWidth, sourceHeight);
        break;
    default:
        break;
    }

    R_FreeImageAllocations();
}

/* Source: CoDUOMP.exe 0x005096d0..0x005097a6.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005096d0_005097a7.mcode.
 * Name and signature: exact same-module Mac symbol UploadImageGroup_r. The
 * terminal node is distinguished by next == NULL; preceding elements point
 * to child lists and advance one sheet coordinate by the child's extent. */
void UploadImageGroup_r(renderer_image_group_node_t *group, image_t *destinationImage, uint8_t *destinationPixels,
                        uint8_t *destinationPixelsEnd, uint16_t destinationX, uint16_t destinationY)
{
    while (group->next != NULL) {
        renderer_image_group_node_t *child = group->child;

        UploadImageGroup_r(child, destinationImage, destinationPixels, destinationPixelsEnd, destinationX, destinationY);
        if (group->width == child->width)
            destinationY = (uint16_t)(destinationY + child->height);
        else
            destinationX = (uint16_t)(destinationX + child->width);
        group = group->next;
    }

    if (r_showImages->integer != 0) {
        ri.Printf(R_PRINT_ALL, "  %-36s(%4i,%4i)%4i x %4i from %4i x %4i\n", group->image->imgName, destinationX, destinationY,
                  group->width, group->height, group->image->width, group->image->height);
    }

    LoadImageTile(group->image, destinationImage, destinationPixels, destinationPixelsEnd, destinationX, destinationY);
}

/* Source: CoDUOMP.exe 0x005097b0..0x00509929.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005097b0_0050992a.mcode.
 * Name and signature: exact same-module Mac symbol UploadImageGroup. A lone
 * image bypasses sheet creation; a merged group allocates one packed buffer,
 * fills it recursively, uploads it as one GL texture, then releases the
 * temporary zone allocation. */
void UploadImageGroup(renderer_image_group_node_t *group, int32_t groupIndex, uint32_t format, uint32_t flags)
{
    if (group->next == NULL) {
        if (r_showImages->integer != 0) {
            ri.Printf(R_PRINT_ALL, "image %2i: %4i x %4i from %4i x %4i -- %s\n", groupIndex, group->width, group->height,
                      group->image->width, group->image->height, group->image->imgName);
        }
        R_LoadSingleDelayedImage(group->image);
        return;
    }

    if (r_showImages->integer != 0) {
        ri.Printf(R_PRINT_ALL, "sheet %2i: %4i x %4i\n", groupIndex, group->width, group->height);
    }

    image_t *sheetImage =
        R_AllocImage(va("*sheet%03i", groupIndex), GL_TEXTURE_2D, group->width, group->height, flags, R_IMAGE_TRACK_MODEL);
    sheetImage->internalFormat = format;

    int32_t allocationBytes = GetCardMemoryAmount(format, group->width, group->height);
    if (allocationBytes <= 0) {
        ri.Error(ERR_DROP, "\x15texture sheet %i has an invalid allocation size\n", groupIndex);
        return;
    }
    if (format != GL_RGBA && (sheetImage->flags & IMAGE_FLAG_MIPMAP) != 0) {
        int32_t mipWidth = group->width;
        int32_t mipHeight = group->height;

        do {
            mipWidth = (mipWidth + 1) >> 1;
            mipHeight = (mipHeight + 1) >> 1;
            const int32_t levelBytes = GetCardMemoryAmount(format, mipWidth, mipHeight);
            if (levelBytes <= 0 || allocationBytes > INT32_MAX - levelBytes) {
                ri.Error(ERR_DROP, "\x15texture sheet %i mip allocation size overflow\n", groupIndex);
                return;
            }
            allocationBytes += levelBytes;
        } while (mipWidth != 1 || mipHeight != 1);
    }

    uint8_t *pixels = ri.Z_Malloc((size_t)allocationBytes);
    UploadImageGroup_r(group, sheetImage, pixels, pixels + allocationBytes, 0, 0);

    if (format == GL_RGBA && r_showImages->integer != 0) {
        SaveJPG(va("%s.jpg", sheetImage->imgName + 1), 100, sheetImage->width, sheetImage->height, pixels, qfalse);
    }

    (void)R_CreateImageInternal(sheetImage, pixels, GL_TEXTURE_2D, format, NULL);
    ri.Z_Free(pixels);
}

/* Source: CoDUOMP.exe 0x00509930..0x00509974.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00509930_00509975.mcode.
 * Name and signature: exact same-module Mac symbol
 * compare_mergable_common. The spelling is retained from the original symbol. */
int compare_mergable_common(const renderer_image_group_node_t *left, const renderer_image_group_node_t *right)
{
    const int32_t leftShortSide = left->width < left->height ? left->width : left->height;
    const int32_t rightShortSide = right->width < right->height ? right->width : right->height;
    int32_t difference = leftShortSide - rightShortSide;

    if (difference == 0) {
        difference = (left->width + left->height) - (right->width + right->height);
        if (difference == 0)
            difference = left->triangleCount - right->triangleCount;
    }
    return difference;
}

/* Source: CoDUOMP.exe 0x00509980..0x005099a1.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00509980_005099a2.mcode.
 * Name: exact same-module Mac symbol compare_mergable_grouped. */
int compare_mergable_grouped(const void *leftElement, const void *rightElement)
{
    const renderer_image_group_node_t *left = *(renderer_image_group_node_t *const *)leftElement;
    const renderer_image_group_node_t *right = *(renderer_image_group_node_t *const *)rightElement;
    const int32_t difference = left->group - right->group;

    return difference != 0 ? difference : compare_mergable_common(left, right);
}

/* Source: CoDUOMP.exe 0x005099b0..0x005099dc.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005099b0_005099dd.mcode.
 * Name: exact same-module Mac symbol compare_mergable_ungrouped. Null array
 * elements sort after live group nodes before the common size/triangle-count
 * ordering. */
int compare_mergable_ungrouped(const void *leftElement, const void *rightElement)
{
    const renderer_image_group_node_t *left = *(renderer_image_group_node_t *const *)leftElement;
    const renderer_image_group_node_t *right = *(renderer_image_group_node_t *const *)rightElement;

    if (left == NULL)
        return right != NULL;
    if (right == NULL)
        return -1;
    return compare_mergable_common(left, right);
}

/* Source: CoDUOMP.exe 0x005099e0..0x00509a8d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005099e0_00509a8e.mcode.
 * Name and signature: exact same-module Mac symbol PickMergeDirection. The
 * lookahead favors a vertical merge when doing so preserves a later matching
 * pair; zero means side-by-side and one means stacked. */
renderer_image_merge_direction_t PickMergeDirection(renderer_image_group_node_t *const *groups, int32_t groupCount, int32_t groupIndex)
{
    const renderer_image_group_node_t *group = groups[groupIndex];
    const int32_t width = group->width;
    const int32_t height = group->height;
    const int32_t doubledWidth = (int32_t)((uint32_t)width * 2u);
    const int32_t doubledHeight = (int32_t)((uint32_t)height * 2u);

    if (width == height)
        return R_IMAGE_MERGE_HORIZONTAL;
    if (width == glConfig.maxTextureSize)
        return R_IMAGE_MERGE_VERTICAL;

    int32_t scanIndex = groupIndex + 1;
    while (scanIndex < groupCount && groups[scanIndex]->height == height) {
        const int32_t scanWidth = groups[scanIndex]->width;
        if (scanWidth > doubledWidth)
            return R_IMAGE_MERGE_VERTICAL;
        if (scanWidth == doubledWidth) {
            while (scanIndex < groupCount && groups[scanIndex]->height == height) {
                ++scanIndex;
            }
            while (scanIndex < groupCount && groups[scanIndex]->width < width && groups[scanIndex]->height == doubledHeight) {
                ++scanIndex;
            }
            if (scanIndex < groupCount && groups[scanIndex]->width == width && groups[scanIndex]->height == doubledHeight) {
                return R_IMAGE_MERGE_VERTICAL;
            }
            return R_IMAGE_MERGE_HORIZONTAL;
        }
        ++scanIndex;
    }

    return R_IMAGE_MERGE_VERTICAL;
}

/* Source: CoDUOMP.exe 0x00509a90..0x00509bd1.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00509a90_00509bd2.mcode.
 * Name and signature: exact same-module Mac symbol MergeImageList. Equal-size
 * adjacent groups are replaced by a node from caller-owned merge storage and
 * reinserted into the list's height/width ordering. */
int32_t MergeImageList(renderer_image_group_node_t **groups, int32_t groupCount, renderer_image_group_node_t *mergeNodes,
                       int32_t mergeNodeCount)
{
    if (groupCount == 1)
        return mergeNodeCount;

    int32_t groupIndex = 1;
    while (groupIndex < groupCount) {
        renderer_image_group_node_t *right = groups[groupIndex];
        if (right->height == glConfig.maxTextureSize)
            return mergeNodeCount;

        renderer_image_group_node_t *left = groups[groupIndex - 1];
        if (right->width != left->width || right->height != left->height) {
            ++groupIndex;
            continue;
        }

        renderer_image_group_node_t *merged = &mergeNodes[mergeNodeCount];
        merged->child = left;
        merged->next = right;
        merged->triangleCount = left->triangleCount + right->triangleCount;
        merged->group = right->group;

        const renderer_image_merge_direction_t direction = PickMergeDirection(groups, groupCount, groupIndex);
        if (direction == R_IMAGE_MERGE_VERTICAL) {
            merged->width = right->width;
            merged->height = right->height * 2;
        } else {
            merged->width = right->width * 2;
            merged->height = right->height;
        }

        int32_t insertionScan = groupIndex + 1;
        while (insertionScan < groupCount) {
            renderer_image_group_node_t *candidate = groups[insertionScan];
            if (candidate->height > merged->height || (candidate->height == merged->height && candidate->width >= merged->width)) {
                break;
            }
            groups[insertionScan - 2] = candidate;
            ++insertionScan;
        }
        groups[insertionScan - 2] = merged;

        for (int32_t moveIndex = insertionScan; moveIndex < groupCount; ++moveIndex) {
            groups[moveIndex - 1] = groups[moveIndex];
        }
        groups[groupCount - 1] = NULL;

        ++mergeNodeCount;
        --groupCount;
    }

    return mergeNodeCount;
}

/* Source: CoDUOMP.exe 0x00509be0..0x00509c91.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00509be0_00509c92.mcode.
 * Name and signature: exact same-module Mac symbol CombineImageGroups. One
 * equal-size adjacent pair is enough to unify the pair's group ids throughout
 * the list; regrouping the sorted array makes the next combination discoverable. */
int32_t CombineImageGroups(renderer_image_group_node_t **groups, int32_t *groupCount)
{
    coduo_crt_qsort(groups, (size_t)*groupCount, sizeof(*groups), compare_mergable_ungrouped);
    while (*groupCount != 0 && groups[*groupCount - 1] == NULL)
        --*groupCount;

    for (int32_t groupIndex = 1; groupIndex < *groupCount; ++groupIndex) {
        renderer_image_group_node_t *right = groups[groupIndex];
        if (right->height == glConfig.maxTextureSize)
            break;

        renderer_image_group_node_t *left = groups[groupIndex - 1];
        if (right->width != left->width || right->height != left->height) {
            continue;
        }

        const int32_t survivingGroup = left->group;
        const int32_t replacedGroup = right->group;
        for (int32_t scanIndex = 0; scanIndex < *groupCount; ++scanIndex) {
            if (groups[scanIndex]->group == replacedGroup)
                groups[scanIndex]->group = survivingGroup;
        }

        coduo_crt_qsort(groups, (size_t)*groupCount, sizeof(*groups), compare_mergable_grouped);
        return survivingGroup;
    }

    return 0;
}

/* Source: CoDUOMP.exe 0x00509ca0..0x00509f0a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00509ca0_00509f0b.mcode.
 * Name and signature: exact same-module Mac symbol
 * MergeAndLoadDelayedImages. Stack-owned leaf and merge nodes describe every
 * packed group; group ids are first merged independently, then compatible
 * groups are combined until no equal-size pair remains. */
void MergeAndLoadDelayedImages(image_t **images, int32_t imageCount, uint32_t format, uint32_t flags)
{
    if (r_showImages->integer != 0)
        ri.Printf(R_PRINT_ALL, "%i groups -> ", imageCount);

    const int32_t nodeCapacity = imageCount * 2 - 1;
    renderer_image_group_node_t *nodes = CODUOMP_ALLOCA((size_t)nodeCapacity * sizeof(*nodes));
    renderer_image_group_node_t **groups = CODUOMP_ALLOCA((size_t)nodeCapacity * sizeof(*groups));

    for (int32_t imageIndex = 0; imageIndex < imageCount; ++imageIndex) {
        image_t *image = images[imageIndex];
        renderer_image_group_node_t *node = &nodes[imageIndex];

        node->image = image;
        node->next = NULL;
        node->triangleCount = image->state.delay.groupTriCount;
        node->group = image->state.delay.group;
        if (image->width > image->height) {
            node->width = image->width;
            node->height = image->height;
        } else {
            node->width = image->height;
            node->height = image->width;
        }

        int32_t picmip = R_PicmipForImageFlags(image->flags);
        while (picmip != 0 && node->width > 4 && node->height > 4) {
            node->width >>= 1;
            node->height >>= 1;
            --picmip;
        }
        while (node->width > glConfig.maxTextureSize) {
            node->width >>= 1;
            node->height >>= 1;
        }
        groups[imageIndex] = node;
    }

    coduo_crt_qsort(groups, (size_t)imageCount, sizeof(*groups), compare_mergable_grouped);

    int32_t mergeNodeCount = imageCount;
    for (int32_t groupStart = 0; groupStart < imageCount;) {
        int32_t groupEnd = groupStart + 1;
        while (groupEnd < imageCount && groups[groupEnd]->group == groups[groupStart]->group) {
            ++groupEnd;
        }
        mergeNodeCount = MergeImageList(&groups[groupStart], groupEnd - groupStart, nodes, mergeNodeCount);
        groupStart = groupEnd;
    }

    int32_t combinedGroup;
    while ((combinedGroup = CombineImageGroups(groups, &imageCount)) != 0) {
        int32_t groupStart = 0;
        while (groups[groupStart]->group != combinedGroup)
            ++groupStart;

        int32_t groupEnd = groupStart + 1;
        while (groupEnd < imageCount && groups[groupEnd]->group == combinedGroup) {
            ++groupEnd;
        }
        mergeNodeCount = MergeImageList(&groups[groupStart], groupEnd - groupStart, nodes, mergeNodeCount);
    }

    if (r_showImages->integer != 0)
        ri.Printf(R_PRINT_ALL, "%i groups\n", imageCount);

    for (int32_t groupIndex = 0; groupIndex < imageCount; ++groupIndex) {
        UploadImageGroup(groups[groupIndex], groupIndex, format, flags);
    }
}

/* Source: CoDUOMP.exe 0x00509f10..0x00509f32.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00509f10_00509f33.mcode.
 * Name and signature: exact same-module Mac symbol compare_image_types. */
int compare_image_types(const void *leftElement, const void *rightElement)
{
    const image_t *left = *(image_t *const *)leftElement;
    const image_t *right = *(image_t *const *)rightElement;
    const int32_t formatDifference = (int32_t)left->internalFormat - (int32_t)right->internalFormat;

    if (formatDifference != 0)
        return formatDifference;
    return (int32_t)(left->flags & IMAGE_FLAG_MIPMAP) - (int32_t)(right->flags & IMAGE_FLAG_MIPMAP);
}

/* Source: CoDUOMP.exe 0x00509f40..0x0050a029.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00509f40_0050a02a.mcode.
 * Name and signature: exact same-module Mac symbol R_LoadDelayedImages. The
 * working list contains only delayed images and is partitioned by internal
 * format and mip-chain requirement before texture-sheet packing. */
void R_LoadDelayedImages(void)
{
    tr.delayedImageGroup = 0;
    if (tr.delayedImageCount == 0)
        return;

    image_t **delayedImages = CODUOMP_ALLOCA((size_t)tr.delayedImageCount * sizeof(*delayedImages));
    int32_t delayedImageCount = 0;

    for (int32_t imageIndex = 0; imageIndex < tr.imageCount; ++imageIndex) {
        image_t *image = tr.images[imageIndex];
        if (image != NULL && (image->flags & IMAGE_FLAG_DELAYED_UPLOAD) != 0) {
            delayedImages[delayedImageCount++] = image;
        }
    }

    coduo_crt_qsort(delayedImages, (size_t)delayedImageCount, sizeof(*delayedImages), compare_image_types);

    for (int32_t typeStart = 0; typeStart < delayedImageCount;) {
        int32_t typeEnd = typeStart + 1;
        while (typeEnd < delayedImageCount && delayedImages[typeEnd]->internalFormat == delayedImages[typeStart]->internalFormat &&
               ((delayedImages[typeEnd]->flags ^ delayedImages[typeStart]->flags) & IMAGE_FLAG_MIPMAP) == 0) {
            ++typeEnd;
        }

        image_t *firstImage = delayedImages[typeStart];
        MergeAndLoadDelayedImages(&delayedImages[typeStart], typeEnd - typeStart, firstImage->internalFormat,
                                  firstImage->flags & IMAGE_FLAG_MIPMAP);
        typeStart = typeEnd;
    }

    tr.delayedImageCount = 0;
}

/* Source: CoDUOMP.exe 0x005079a0..0x00507c24.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005079a0_00507c25.mcode.
 * Name: exact same-module Mac symbol R_HeightmapImage. Each normal uses wrapped
 * central differences of the summed RGB height channels. The first literal is
 * the exact Windows float at 0x005b9d50 (mathematically 1/(2*3*255)); keeping
 * the encoded value avoids platform-dependent constant folding. */
void R_HeightmapImage(uint8_t *pixels, int32_t width, int32_t height, float heightScale)
{
    const float gradientScale = 0.0006535947904922068f;
    const float encodeScale = 127.5f;
    float normalZ = 1.0f;
    if (heightScale > 0.0f)
        normalZ = 1.0f / heightScale;

    const uint32_t byteCount = (uint32_t)width * (uint32_t)height * 4U;
    uint8_t *normalPixels = ri.Hunk_AllocateTempMemory(byteCount);

    for (int32_t row = 0; row < height; ++row) {
        const int32_t previousRow = row == 0 ? height - 1 : row - 1;
        const int32_t nextRow = row + 1 == height ? 0 : row + 1;

        for (int32_t column = 0; column < width; ++column) {
            const int32_t previousColumn = column == 0 ? width - 1 : column - 1;
            const int32_t nextColumn = column + 1 == width ? 0 : column + 1;

            const size_t left = ((size_t)row * (size_t)width + (size_t)previousColumn) * 4U;
            const size_t right = ((size_t)row * (size_t)width + (size_t)nextColumn) * 4U;
            const size_t above = ((size_t)previousRow * (size_t)width + (size_t)column) * 4U;
            const size_t below = ((size_t)nextRow * (size_t)width + (size_t)column) * 4U;
            const size_t pixel = ((size_t)row * (size_t)width + (size_t)column) * 4U;

            const int32_t horizontalDifference =
                pixels[right] + pixels[right + 1] + pixels[right + 2] - pixels[left] - pixels[left + 1] - pixels[left + 2];
            const int32_t verticalDifference =
                pixels[below] + pixels[below + 1] + pixels[below + 2] - pixels[above] - pixels[above + 1] - pixels[above + 2];
            const long double normalXRaw = (long double)horizontalDifference * (long double)gradientScale;
            const long double normalYRaw = (long double)verticalDifference * (long double)gradientScale;
            const float normalY = (float)normalYRaw;
            const float normalZSquared = normalZ * normalZ;
            /* 0x00507b3c stores normalY without popping. Its length term is
             * retained-Y times rounded-Y, while X remains wholly retained. */
            const long double inverseLengthRaw =
                1.0L / sqrtl(normalYRaw * (long double)normalY + normalXRaw * normalXRaw + (long double)normalZSquared);
            const float inverseLength = (float)inverseLengthRaw;

            normalPixels[pixel] = (uint8_t)((normalXRaw * inverseLengthRaw + 1.0L) * (long double)encodeScale);
            normalPixels[pixel + 1] = (uint8_t)((normalY * inverseLength + 1.0f) * encodeScale);
            normalPixels[pixel + 2] = (uint8_t)((normalZ * inverseLength + 1.0f) * encodeScale);
            normalPixels[pixel + 3] = pixels[pixel + 3];
        }
    }

    memcpy(pixels, normalPixels, byteCount);
    ri.Hunk_FreeTempMemory(normalPixels);
}

/* Source: CoDUOMP.exe 0x00507860..0x00507991.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00507860_00507992.mcode.
 * Name and intended parameter roles: exact same-module Mac symbol
 * R_UnmangleTextureName. At Windows 0x00507877 the two strchr arguments are
 * still on the stack, so [esp+0x20] resolves to mangledName; the independent
 * Mac build expresses the same separator-versus-prefix comparison directly. */
void R_UnmangleTextureName(const char *originalName, const char *mangledName, char *outputName, uint32_t *textureTarget, uint32_t *flags,
                           float colorScale[4], float *heightScale)
{
    const char *mangledTextureName = mangledName + 5;
    const char *separator = strchr(mangledTextureName, ':');
    const size_t textureNameLength = (size_t)(separator - mangledTextureName);

    if (separator == mangledTextureName) {
        strcpy(outputName, originalName);
    } else {
        const char *source = originalName;
        char *destination = outputName;
        while (*source != '\0' && *source != '.')
            *destination++ = *source++;
        *destination = '\0';

        const size_t baseNameLength = strlen(outputName);
        memcpy(outputName + baseNameLength, mangledTextureName, textureNameLength);
        outputName[baseNameLength + textureNameLength] = '\0';
    }

    if (strncmp(mangledName, "$h2n", 4) == 0) {
        (void)sscanf(separator + 1, "%x/%x/%g", textureTarget, flags, heightScale);
        colorScale[0] = 1.0f;
        colorScale[1] = 1.0f;
        colorScale[2] = 1.0f;
        colorScale[3] = 1.0f;
    } else {
        const int32_t fieldCount = sscanf(separator + 1, "%x/%x/%g/%g/%g/%g", textureTarget, flags, &colorScale[0], &colorScale[1],
                                          &colorScale[2], &colorScale[3]);
        if (fieldCount < 3)
            colorScale[0] = 1.0f;
        if (fieldCount < 5) {
            colorScale[1] = colorScale[0];
            colorScale[2] = colorScale[0];
        }
        if (fieldCount < 6)
            colorScale[3] = 1.0f;
        *heightScale = 1.0f;
    }
}

/* Source: CoDUOMP.exe 0x00507750..0x00507853.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00507750_00507854.mcode.
 * Name: exact same-module Mac symbol R_MangleTextureName. The compact forms
 * omit identity color channels, but texture target and image flags remain in
 * every key so differently configured images cannot alias in the image hash. */
const char *R_MangleTextureName(const char *name, uint32_t textureTarget, uint32_t flags, const float colorScale[4], float heightScale)
{
    const char *mangledName;

    if ((flags & IMAGE_FLAG_HEIGHT_TO_NORMAL) != 0) {
        mangledName = va("$h2n_%s:%x/%x/%g", name, textureTarget, flags, heightScale);
    } else if (colorScale == NULL) {
        mangledName = va("$tex_%s:%x/%x", name, textureTarget, flags);
    } else if (colorScale[3] != 1.0f) {
        mangledName =
            va("$tex_%s:%x/%x/%g/%g/%g/%g", name, textureTarget, flags, colorScale[0], colorScale[1], colorScale[2], colorScale[3]);
    } else if (colorScale[0] == colorScale[1] && colorScale[0] == colorScale[2]) {
        if (colorScale[0] == 1.0f) {
            mangledName = va("$tex_%s:%x/%x", name, textureTarget, flags);
        } else {
            mangledName = va("$tex_%s:%x/%x/%g", name, textureTarget, flags, colorScale[0]);
        }
    } else {
        mangledName = va("$tex_%s:%x/%x/%g/%g/%g", name, textureTarget, flags, colorScale[0], colorScale[1], colorScale[2]);
    }

    if (strlen(mangledName) < MAX_QPATH)
        return mangledName;

    ri.Printf(R_PRINT_WARNING, "WARNING: mangled name for $texturename + %s is too long\n", name);
    return NULL;
}

/* Source: CoDUOMP.exe 0x00507c30..0x00507ca5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00507c30_00507ca6.mcode.
 * Name: exact same-module Mac symbol R_FlipImageDiagonally. The operation is
 * defined only for square RGBA images; nonsquare input is left untouched. */
void R_FlipImageDiagonally(uint8_t *pixels, int32_t width, int32_t height)
{
    if (width != height)
        return;

    uint32_t *pixelWords = (uint32_t *)pixels;
    for (int32_t row = 1; row < height; ++row) {
        for (int32_t column = 0; column < row; ++column) {
            const int32_t first = row * width + column;
            const int32_t second = column * width + row;
            const uint32_t temporary = pixelWords[first];
            pixelWords[first] = pixelWords[second];
            pixelWords[second] = temporary;
        }
    }
}

/* Source: CoDUOMP.exe 0x00507cb0..0x00507d04.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00507cb0_00507d05.mcode.
 * Name: exact same-module Mac symbol R_FlipImageHorizontally. */
void R_FlipImageHorizontally(uint8_t *pixels, int32_t width, int32_t height)
{
    uint32_t *pixelWords = (uint32_t *)pixels;
    for (int32_t row = 0; row < height; ++row) {
        for (int32_t column = 0; column < width / 2; ++column) {
            const int32_t first = row * width + column;
            const int32_t second = row * width + width - 1 - column;
            const uint32_t temporary = pixelWords[first];
            pixelWords[first] = pixelWords[second];
            pixelWords[second] = temporary;
        }
    }
}

/* Source: CoDUOMP.exe 0x00507d10..0x00507d8b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00507d10_00507d8c.mcode.
 * Name: exact same-module Mac symbol R_FlipImageVertically. */
void R_FlipImageVertically(uint8_t *pixels, int32_t width, int32_t height)
{
    uint32_t *pixelWords = (uint32_t *)pixels;
    for (int32_t column = 0; column < width; ++column) {
        for (int32_t row = 0; row < height / 2; ++row) {
            const int32_t first = row * width + column;
            const int32_t second = (height - 1 - row) * width + column;
            const uint32_t temporary = pixelWords[first];
            pixelWords[first] = pixelWords[second];
            pixelWords[second] = temporary;
        }
    }
}

/* Source: CoDUOMP.exe 0x00507d90..0x00507f23.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00507d90_00507f24.mcode.
 * Name and source boundary: exact same-module Mac symbol R_LoadImage. The
 * caller may supply any four-character extension, but format probing always
 * replaces it with .dds followed by the uncompressed .tga/.jpg fallbacks. */
void R_LoadImage(const char *name, uint8_t **pixels, uint16_t *width, uint16_t *height, uint32_t *format, qboolean *mipMapsAvailable,
                 renderer_image_load_mode_t loadMode)
{
    char imageName[MAX_QPATH];

    *pixels = NULL;
    *width = 0;
    *height = 0;
    *format = 0;
    *mipMapsAvailable = qfalse;

    uint32_t baseNameLength = (uint32_t)strlen(name);
    if ((int32_t)baseNameLength <= 4)
        return;

    if ((int32_t)baseNameLength >= MAX_QPATH) {
        ri.Error(ERR_DROP, "\x15image name '%s' is longer than %i characters\n", name, MAX_QPATH - 1);
    }
    if (name[baseNameLength - 4] == '.')
        baseNameLength -= 4;
    if ((int32_t)baseNameLength >= MAX_QPATH - 4) {
        ri.Error(ERR_DROP,
                 "\x15image name '%s' with its extension is longer than %i "
                 "characters\n",
                 name, MAX_QPATH - 1);
    }

    memcpy(imageName, name, (size_t)baseNameLength);
    imageName[baseNameLength] = '.';
    strcpy(&imageName[baseNameLength + 1], "dds");
    LoadDDS(imageName, pixels, width, height, format, mipMapsAvailable, loadMode);

    if (*format == 0) {
        *mipMapsAvailable = qtrue;
        strcpy(&imageName[baseNameLength + 1], "tga");
        LoadTGA(imageName, pixels, width, height, format, loadMode);

        if (*format == 0) {
            strcpy(&imageName[baseNameLength + 1], "jpg");
            LoadJPG(imageName, pixels, width, height, format, loadMode);
        }
    }

    if (*format != 0 && *pixels == NULL)
        ++tr.delayedImageCount;
}

/* Source: CoDUOMP.exe 0x00506710..0x005067b5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00506710_005067b6.mcode.
 * Name: same-module Mac symbol TransposeDDSBlockDXT1. The explicit byte
 * packing preserves the DDS little-endian selector layout on every host. */
void TransposeDDSBlockDXT1(const uint8_t source[8], uint8_t destination[8])
{
    uint32_t selectors = 0;
    uint32_t transposedSelectors = 0;

    memcpy(destination, source, 4);
    for (int32_t byteIndex = 0; byteIndex < 4; ++byteIndex)
        selectors |= (uint32_t)source[4 + byteIndex] << (byteIndex * 8);

    for (int32_t row = 0; row < 4; ++row) {
        for (int32_t column = 0; column < 4; ++column) {
            const uint32_t selector = (selectors >> ((column * 4 + row) * 2)) & 3;
            transposedSelectors |= selector << ((row * 4 + column) * 2);
        }
    }
    for (int32_t byteIndex = 0; byteIndex < 4; ++byteIndex) {
        destination[4 + byteIndex] = (uint8_t)(transposedSelectors >> (byteIndex * 8));
    }
}

/* Source: CoDUOMP.exe 0x005067c0..0x00506850.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005067c0_00506851.mcode.
 * Name: same-module Mac symbol TransposeDDSBlockDXT3. */
void TransposeDDSBlockDXT3(const uint8_t source[16], uint8_t destination[16])
{
    uint64_t alphaSelectors = 0;
    uint64_t transposedAlphaSelectors = 0;

    TransposeDDSBlockDXT1(source + 8, destination + 8);
    for (int32_t byteIndex = 0; byteIndex < 8; ++byteIndex)
        alphaSelectors |= (uint64_t)source[byteIndex] << (byteIndex * 8);

    for (int32_t row = 0; row < 4; ++row) {
        for (int32_t column = 0; column < 4; ++column) {
            const uint64_t selector = (alphaSelectors >> ((column * 4 + row) * 4)) & 15;
            transposedAlphaSelectors |= selector << ((row * 4 + column) * 4);
        }
    }
    for (int32_t byteIndex = 0; byteIndex < 8; ++byteIndex) {
        destination[byteIndex] = (uint8_t)(transposedAlphaSelectors >> (byteIndex * 8));
    }
}

/* Source: CoDUOMP.exe 0x00506860..0x00506986.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00506860_00506987.mcode.
 * Name: same-module Mac symbol TransposeDDSBlockDXT5. */
void TransposeDDSBlockDXT5(const uint8_t source[16], uint8_t destination[16])
{
    uint64_t alphaSelectors = 0;
    uint64_t transposedAlphaSelectors = 0;

    TransposeDDSBlockDXT1(source + 8, destination + 8);
    destination[0] = source[0];
    destination[1] = source[1];
    for (int32_t byteIndex = 0; byteIndex < 6; ++byteIndex) {
        alphaSelectors |= (uint64_t)source[2 + byteIndex] << (byteIndex * 8);
    }

    for (int32_t row = 0; row < 4; ++row) {
        for (int32_t column = 0; column < 4; ++column) {
            const uint64_t selector = (alphaSelectors >> ((column * 4 + row) * 3)) & 7;
            transposedAlphaSelectors |= selector << ((row * 4 + column) * 3);
        }
    }
    for (int32_t byteIndex = 0; byteIndex < 6; ++byteIndex) {
        destination[2 + byteIndex] = (uint8_t)(transposedAlphaSelectors >> (byteIndex * 8));
    }
}

/* Source: CoDUOMP.exe 0x00506990..0x00506b7d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00506990_00506b7e.mcode.
 * Name and source boundary: exact same-module Mac symbol LoadDDS. A metadata-
 * only request reads and closes just the 128-byte header. Pixel-loading keeps
 * the tracked file allocation alive because the returned compressed data is a
 * view beginning immediately after that header. */
void LoadDDS(const char *name, uint8_t **pixels, uint16_t *width, uint16_t *height, uint32_t *format, qboolean *mipMapsAvailable,
             renderer_image_load_mode_t loadMode)
{
    renderer_dds_header_t localHeader;
    renderer_dds_header_t *header;
    int32_t fileSize;

    if (loadMode == R_IMAGE_LOAD_METADATA) {
        int32_t fileHandle;

        fileSize = ri.FS_FOpenFileRead(name, &fileHandle, qfalse);
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (fileSize <= R_DDS_DATA_OFFSET) {
            if (fileSize >= 0)
                ri.FS_FCloseFile(fileHandle);
            return;
        }
        ri.FS_Read(&localHeader, sizeof(localHeader), fileHandle);
        ri.FS_FCloseFile(fileHandle);
        header = &localHeader;
    } else {
        void *fileBuffer;

        fileSize = R_ReadFile(name, &fileBuffer);
        if (fileSize <= R_DDS_DATA_OFFSET)
            return;
        header = fileBuffer;
    }

    if (header->magic != R_DDS_MAGIC)
        return;

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (header->size != R_DDS_HEADER_SIZE)
        return;

    switch (header->pixelFormat.fourCC) {
    case R_DDS_FOURCC_DXT1:
        *format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
        break;
    case R_DDS_FOURCC_DXT3:
        *format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
        break;
    case R_DDS_FOURCC_DXT5:
        *format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
        break;
    default:
        return;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (header->width == 0 || header->height == 0 || (header->width & (header->width - 1)) != 0 ||
        (header->height & (header->height - 1)) != 0) {
        ri.Printf(R_PRINT_WARNING,
                  "WARNING: image '%s' is %i x %i, which is not a power of "
                  "2 on both sides\n",
                  name, (int32_t)header->width, (int32_t)header->height);
        *format = 0;
        return;
    }

    *mipMapsAvailable = qfalse;
    if (header->mipMapCount != 0 && header->mipMapCount != 1) {
        const uint32_t largestDimension = header->width > header->height ? header->width : header->height;
        const uint32_t levelExtent = 1u << (header->mipMapCount & 31);

        if (levelExtent != largestDimension * 2) {
            ri.Printf(R_PRINT_WARNING,
                      "WARNING: image '%s' does not have the correct number "
                      "of mipmaps",
                      name);
            *format = 0;
            return;
        }
        *mipMapsAvailable = qtrue;
    }

    if (header->width > R_DDS_MAX_DIMENSION || header->height > R_DDS_MAX_DIMENSION) {
        ri.Printf(R_PRINT_WARNING,
                  "WARNING: image '%s' is greater than %i on one or more "
                  "axes",
                  name, R_DDS_MAX_DIMENSION);
        *format = 0;
        return;
    }

    if (loadMode != R_IMAGE_LOAD_METADATA) {
        uint32_t levelWidth = header->width;
        uint32_t levelHeight = header->height;
        const uint32_t blockBytes = *format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT ? 8u : 16u;
        uint64_t requiredDataBytes = 0;

        do {
            requiredDataBytes += (uint64_t)((levelWidth + 3u) / 4u) * (uint64_t)((levelHeight + 3u) / 4u) * blockBytes;
            if (*mipMapsAvailable == qfalse || (levelWidth == 1 && levelHeight == 1)) {
                break;
            }
            if (levelWidth > 1)
                levelWidth >>= 1;
            if (levelHeight > 1)
                levelHeight >>= 1;
        } while (qtrue);

        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
        if (requiredDataBytes > (uint64_t)(fileSize - R_DDS_DATA_OFFSET)) {
            ri.Printf(R_PRINT_WARNING, "WARNING: image '%s' has truncated DDS data\n", name);
            *format = 0;
            return;
        }
    }

    *width = (uint16_t)header->width;
    *height = (uint16_t)header->height;
    if (loadMode != R_IMAGE_LOAD_METADATA)
        *pixels = (uint8_t *)header + R_DDS_DATA_OFFSET;
}

/* Source: CoDUOMP.exe 0x00506b80..0x005070ba.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00506b80_005070bb.mcode.
 * Name and source boundary: exact same-module Mac symbol LoadTGA. TGA stores
 * color channels as BGR(A); this decoder produces RGBA and honors the vertical
 * origin bit through destination strides. R_IMAGE_LOAD_TILE additionally
 * transposes portrait images during the copy, matching LoadImageTile's sole
 * use of mode 2. */
void LoadTGA(const char *name, uint8_t **pixels, uint16_t *width, uint16_t *height, uint32_t *format, renderer_image_load_mode_t loadMode)
{
    renderer_tga_header_t localHeader;
    const renderer_tga_header_t *header;
    const uint8_t *source = NULL;
    const uint8_t *sourceEnd = NULL;
    int32_t fileSize;

    if (loadMode == R_IMAGE_LOAD_METADATA) {
        int32_t fileHandle;
        fileSize = ri.FS_FOpenFileRead(name, &fileHandle, qfalse);

        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
        if (fileSize < (int32_t)sizeof(localHeader)) {
            if (fileSize >= 0)
                ri.FS_FCloseFile(fileHandle);
            return;
        }
        ri.FS_Read(&localHeader, sizeof(localHeader), fileHandle);
        ri.FS_FCloseFile(fileHandle);
        header = &localHeader;
    } else {
        void *fileBuffer;

        fileSize = R_ReadFile(name, &fileBuffer);
        if (fileBuffer == NULL || fileSize < (int32_t)sizeof(renderer_tga_header_t)) {
            return;
        }
        header = fileBuffer;
        source = (const uint8_t *)fileBuffer + sizeof(*header);
        sourceEnd = (const uint8_t *)fileBuffer + (size_t)fileSize;
    }

    if (header->imageType != R_TGA_TYPE_UNCOMPRESSED_TRUECOLOR && header->imageType != R_TGA_TYPE_RLE_TRUECOLOR &&
        header->imageType != R_TGA_TYPE_UNCOMPRESSED_GRAYSCALE) {
        ri.Error(ERR_DROP,
                 "\x15LoadTGA: %s Only type 2 (RGB), 3 (gray), and 10 "
                 "(RGB) TGA images supported\n",
                 name);
    }
    if (header->colorMapType != 0) {
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        ri.Error(ERR_DROP, "\x15LoadTGA: %s colormaps not supported\n", name);
    }
    if ((header->pixelSize != R_TGA_PIXEL_SIZE_RGBA && header->pixelSize != R_TGA_PIXEL_SIZE_RGB &&
         header->pixelSize != R_TGA_PIXEL_SIZE_GRAYSCALE) ||
        (header->imageType != R_TGA_TYPE_UNCOMPRESSED_GRAYSCALE && header->pixelSize == R_TGA_PIXEL_SIZE_GRAYSCALE)) {
        ri.Error(ERR_DROP,
                 "\x15LoadTGA: %s Only 24/32 bit RGB or 8 bit grayscale "
                 "images supported (no colormaps)\n",
                 name);
    }

    const uint16_t sourceWidth = (uint16_t)(header->width[0] | ((uint16_t)header->width[1] << 8));
    const uint16_t sourceHeight = (uint16_t)(header->height[0] | ((uint16_t)header->height[1] << 8));

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    const uint64_t pixelCountWide = (uint64_t)sourceWidth * (uint64_t)sourceHeight;
    const uint64_t outputByteCountWide = pixelCountWide * 4u;
    if (sourceWidth == 0 || sourceHeight == 0 || outputByteCountWide > INT32_MAX) {
        ri.Printf(R_PRINT_WARNING, "WARNING: image '%s' has invalid TGA dimensions %u x %u\n", name, (unsigned)sourceWidth,
                  (unsigned)sourceHeight);
        return;
    }

    *width = sourceWidth;
    *height = sourceHeight;
    *format = GL_RGBA;

    if (loadMode == R_IMAGE_LOAD_METADATA)
        return;

    const uint32_t sourceBytesPerPixel = (uint32_t)header->pixelSize / 8u;

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if ((size_t)(sourceEnd - source) < (size_t)header->idLength) {
        ri.Printf(R_PRINT_WARNING, "WARNING: image '%s' has truncated TGA data\n", name);
        *format = 0;
        return;
    }
    source += header->idLength;

    if (header->imageType == R_TGA_TYPE_UNCOMPRESSED_TRUECOLOR || header->imageType == R_TGA_TYPE_UNCOMPRESSED_GRAYSCALE) {
        const uint64_t requiredSourceBytes = pixelCountWide * sourceBytesPerPixel;
        if (requiredSourceBytes > (uint64_t)(sourceEnd - source)) {
            ri.Printf(R_PRINT_WARNING, "WARNING: image '%s' has truncated TGA data\n", name);
            *format = 0;
            return;
        }
    } else {
        const uint8_t *packetSource = source;
        uint64_t decodedPixels = 0;

        while (decodedPixels < pixelCountWide) {
            if (packetSource == sourceEnd) {
                ri.Printf(R_PRINT_WARNING, "WARNING: image '%s' has truncated TGA data\n", name);
                *format = 0;
                return;
            }

            const uint8_t packetHeader = *packetSource++;
            const uint32_t packetLength = (packetHeader & R_TGA_PACKET_LENGTH_MASK) + 1u;
            if ((uint64_t)packetLength > pixelCountWide - decodedPixels) {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: image '%s' has an oversized TGA RLE "
                          "packet\n",
                          name);
                *format = 0;
                return;
            }

            const uint64_t packetBytes =
                (packetHeader & R_TGA_RLE_PACKET) != 0 ? sourceBytesPerPixel : (uint64_t)packetLength * sourceBytesPerPixel;
            if (packetBytes > (uint64_t)(sourceEnd - packetSource)) {
                ri.Printf(R_PRINT_WARNING, "WARNING: image '%s' has truncated TGA data\n", name);
                *format = 0;
                return;
            }
            packetSource += (size_t)packetBytes;
            decodedPixels += packetLength;
        }
    }

    uint8_t *output = R_AllocTempMemory((size_t)outputByteCountWide);
    *pixels = output;

    int32_t pixelStride = 4;
    int32_t rowAdvance;
    uint8_t *destination;

    if ((header->imageDescriptor & R_TGA_ORIGIN_TOP) != 0) {
        rowAdvance = 0;
        destination = output;
    } else {
        rowAdvance = -(int32_t)sourceWidth * 8;
        destination = output + (uint32_t)sourceWidth * (sourceHeight - 1u) * 4u;
    }

    if (loadMode == R_IMAGE_LOAD_TILE && sourceWidth < sourceHeight) {
        *width = sourceHeight;
        *height = sourceWidth;
        pixelStride = (int32_t)sourceHeight * 4;

        if ((header->imageDescriptor & R_TGA_ORIGIN_TOP) != 0) {
            rowAdvance = 4 - (int32_t)sourceWidth * pixelStride;
            destination = output;
        } else {
            rowAdvance = -4 - (int32_t)sourceWidth * pixelStride;
            destination = output + (sourceHeight - 1u) * 4u;
        }
    }

    if (header->imageType == R_TGA_TYPE_UNCOMPRESSED_TRUECOLOR || header->imageType == R_TGA_TYPE_UNCOMPRESSED_GRAYSCALE) {
        for (int32_t row = 0; row < sourceHeight; ++row) {
            for (int32_t column = 0; column < sourceWidth; ++column) {
                if (header->pixelSize == R_TGA_PIXEL_SIZE_RGBA) {
                    const uint8_t blue = *source++;
                    const uint8_t green = *source++;
                    const uint8_t red = *source++;
                    const uint8_t alpha = *source++;
                    destination[0] = red;
                    destination[1] = green;
                    destination[2] = blue;
                    destination[3] = alpha;
                } else if (header->pixelSize == R_TGA_PIXEL_SIZE_RGB) {
                    const uint8_t blue = *source++;
                    const uint8_t green = *source++;
                    const uint8_t red = *source++;
                    destination[0] = red;
                    destination[1] = green;
                    destination[2] = blue;
                    destination[3] = 255;
                } else if (header->pixelSize == R_TGA_PIXEL_SIZE_GRAYSCALE) {
                    const uint8_t gray = *source++;
                    destination[0] = gray;
                    destination[1] = gray;
                    destination[2] = gray;
                    destination[3] = 255;
                } else {
                    ri.Error(ERR_DROP,
                             "\x15LoadTGA: illegal pixel_size '%d' in file "
                             "'%s'\n",
                             (int32_t)header->pixelSize, name);
                }
                destination += pixelStride;
            }
            destination += rowAdvance;
        }
        return;
    }

    if (header->imageType == R_TGA_TYPE_RLE_TRUECOLOR) {
        int32_t row = 0;
        int32_t column = 0;

        while (row < sourceHeight) {
            const uint8_t packetHeader = *source++;
            const int32_t packetLength = (packetHeader & R_TGA_PACKET_LENGTH_MASK) + 1;

            if ((packetHeader & R_TGA_RLE_PACKET) == 0) {
                for (int32_t packetPixel = 0; packetPixel < packetLength; ++packetPixel) {
                    uint8_t blue;
                    uint8_t green;
                    uint8_t red;
                    uint8_t alpha;

                    if (header->pixelSize == R_TGA_PIXEL_SIZE_RGBA) {
                        blue = *source++;
                        green = *source++;
                        red = *source++;
                        alpha = *source++;
                    } else if (header->pixelSize == R_TGA_PIXEL_SIZE_RGB) {
                        blue = *source++;
                        green = *source++;
                        red = *source++;
                        alpha = 255;
                    } else {
                        ri.Error(ERR_DROP,
                                 "\x15LoadTGA: illegal pixel_size '%d' in file "
                                 "'%s'\n",
                                 (int32_t)header->pixelSize, name);
                    }

                    destination[0] = red;
                    destination[1] = green;
                    destination[2] = blue;
                    destination[3] = alpha;
                    destination += pixelStride;

                    if (++column == sourceWidth) {
                        column = 0;
                        if (++row == sourceHeight)
                            return;
                        destination += rowAdvance;
                    }
                }
            } else {
                uint8_t blue;
                uint8_t green;
                uint8_t red;
                uint8_t alpha;

                if (header->pixelSize == R_TGA_PIXEL_SIZE_RGBA) {
                    blue = *source++;
                    green = *source++;
                    red = *source++;
                    alpha = *source++;
                } else if (header->pixelSize == R_TGA_PIXEL_SIZE_RGB) {
                    blue = *source++;
                    green = *source++;
                    red = *source++;
                    alpha = 255;
                } else {
                    ri.Error(ERR_DROP,
                             "\x15LoadTGA: illegal pixel_size '%d' in file "
                             "'%s'\n",
                             (int32_t)header->pixelSize, name);
                }

                for (int32_t packetPixel = 0; packetPixel < packetLength; ++packetPixel) {
                    destination[0] = red;
                    destination[1] = green;
                    destination[2] = blue;
                    destination[3] = alpha;
                    destination += pixelStride;

                    if (++column == sourceWidth) {
                        column = 0;
                        if (++row == sourceHeight)
                            return;
                        destination += rowAdvance;
                    }
                }
            }
        }
    }
}

/* Source: CoDUOMP.exe 0x005070c0..0x005073e2.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005070c0_005073e3.mcode.
 * Name and source boundary: exact same-module Mac symbol LoadJPG. The original
 * executable embeds IJG libjpeg configured for four-byte RGB samples. Modern
 * builds select libjpeg-turbo RGBX explicitly at that library boundary, then
 * retain the original RGBA copy, size limit, metadata mode, and portrait tile
 * transpose. */
void LoadJPG(const char *name, uint8_t **pixels, uint16_t *width, uint16_t *height, uint32_t *format, renderer_image_load_mode_t loadMode)
{
    void *fileBuffer;
    const int32_t fileSize = R_ReadFile(name, &fileBuffer);

    if (fileBuffer == NULL || fileSize <= 0)
        return;

    coduomp_jpeg_error_manager_t errorManager;
    struct jpeg_decompress_struct decoder;
    uint8_t *volatile heapRowBuffer = NULL;
    volatile qboolean decoderCreated = qfalse;

    memset(&decoder, 0, sizeof(decoder));
    decoder.err = jpeg_std_error(&errorManager.callbacks);
    errorManager.callbacks.error_exit = coduomp_jpeg_error_exit;
    if (setjmp(errorManager.failure) != 0) {
        free((void *)heapRowBuffer);
        if (decoderCreated != qfalse)
            jpeg_destroy_decompress(&decoder);
        *pixels = NULL;
        *width = 0;
        *height = 0;
        *format = 0;
        ri.Printf(R_PRINT_WARNING, "WARNING: image '%s' has invalid JPEG data\n", name);
        return;
    }

    jpeg_create_decompress(&decoder);
    decoderCreated = qtrue;
    jpeg_mem_src(&decoder, fileBuffer, (unsigned long)fileSize);
    if (jpeg_read_header(&decoder, TRUE) != JPEG_HEADER_OK) {
        jpeg_destroy_decompress(&decoder);
        ri.Printf(R_PRINT_WARNING, "WARNING: image '%s' has invalid JPEG data\n", name);
        return;
    }

    /* Compatibility adaptation: CoD's embedded IJG build used four-byte RGB
     * output. libjpeg-turbo exposes that same layout as JCS_EXT_RGBX. */
    decoder.out_color_space = JCS_EXT_RGBX;
    jpeg_calc_output_dimensions(&decoder);

    enum {
        R_JPEG_MAX_DIMENSION = 32768
    };
    const uint64_t outputByteCountWide = (uint64_t)decoder.output_width * (uint64_t)decoder.output_height * 4u;
    if (decoder.output_width == 0 || decoder.output_height == 0 || decoder.output_width > R_JPEG_MAX_DIMENSION ||
        decoder.output_height > R_JPEG_MAX_DIMENSION || outputByteCountWide > INT32_MAX) {
        ri.Printf(R_PRINT_WARNING,
                  "WARNING: image '%s' has unsupported JPEG dimensions "
                  "%u x %u\n",
                  name, (unsigned)decoder.output_width, (unsigned)decoder.output_height);
        jpeg_destroy_decompress(&decoder);
        return;
    }

    if (jpeg_start_decompress(&decoder) == FALSE || decoder.output_components != 4) {
        jpeg_destroy_decompress(&decoder);
        ri.Printf(R_PRINT_WARNING, "WARNING: image '%s' has invalid JPEG output\n", name);
        return;
    }

    const uint16_t sourceWidth = (uint16_t)decoder.output_width;
    const uint16_t sourceHeight = (uint16_t)decoder.output_height;
    *width = sourceWidth;
    *height = sourceHeight;
    *format = GL_RGBA;

    if (loadMode == R_IMAGE_LOAD_METADATA) {
        jpeg_destroy_decompress(&decoder);
        return;
    }

    uint8_t *destination = R_AllocTempMemory((size_t)outputByteCountWide);
    *pixels = destination;

    const size_t rowBytes = (size_t)sourceWidth * 4u;
    if (loadMode == R_IMAGE_LOAD_TILE && sourceWidth < sourceHeight) {
        uint8_t *rowBuffer = CODUOMP_ALLOCA(rowBytes);

        while (decoder.output_scanline < decoder.output_height) {
            JSAMPROW scanline = rowBuffer;
            if (jpeg_read_scanlines(&decoder, &scanline, 1) != 1)
                (*decoder.err->error_exit)((j_common_ptr)&decoder);

            const uint8_t *source = rowBuffer;
            for (uint32_t column = 0; column < sourceWidth; ++column) {
                destination[0] = source[0];
                destination[1] = source[1];
                destination[2] = source[2];
                destination[3] = 255;
                source += 4;
                destination += rowBytes;
            }
            destination += 4 - (ptrdiff_t)sourceWidth * rowBytes;
        }

        *width = sourceHeight;
        *height = sourceWidth;
    } else {
        heapRowBuffer = malloc(rowBytes);
        if (heapRowBuffer == NULL)
            (*decoder.err->error_exit)((j_common_ptr)&decoder);
        uint8_t *rowBuffer = (uint8_t *)heapRowBuffer;

        while (decoder.output_scanline < decoder.output_height) {
            JSAMPROW scanline = rowBuffer;
            if (jpeg_read_scanlines(&decoder, &scanline, 1) != 1)
                (*decoder.err->error_exit)((j_common_ptr)&decoder);

            size_t sourceOffset = 0;
            for (uint32_t column = 0; column < sourceWidth; ++column) {
                destination[0] = rowBuffer[sourceOffset + 0];
                destination[1] = rowBuffer[sourceOffset + 1];
                destination[2] = rowBuffer[sourceOffset + 2];
                destination[3] = 255;
                sourceOffset += (size_t)decoder.output_components;
                destination += 4;
            }
        }
        free((void *)heapRowBuffer);
        heapRowBuffer = NULL;
    }

    (void)jpeg_finish_decompress(&decoder);
    jpeg_destroy_decompress(&decoder);
}

/* Source: CoDUOMP.exe 0x005048f0..0x005049b9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005048f0_005049ba.mcode.
 * Name and source boundary: exact same-module Mac symbol GL_TextureMode. */
void GL_TextureMode(const char *textureMode)
{
    size_t modeIndex;

    for (modeIndex = 0; modeIndex < sizeof(rendererTextureFilterModes) / sizeof(rendererTextureFilterModes[0]); ++modeIndex) {
        if (Q_stricmp(rendererTextureFilterModes[modeIndex].name, textureMode) == 0) {
            break;
        }
    }

    if (modeIndex == sizeof(rendererTextureFilterModes) / sizeof(rendererTextureFilterModes[0])) {
        ri.Printf(R_PRINT_ALL, "bad filter name\n");
        return;
    }

    rendererTextureMinFilter = rendererTextureFilterModes[modeIndex].minFilter;
    rendererTextureMagFilter = rendererTextureFilterModes[modeIndex].magFilter;
    for (int32_t imageIndex = 0; imageIndex < tr.imageCount; ++imageIndex) {
        image_t *image = tr.images[imageIndex];

        if ((image->flags & IMAGE_FLAG_MIPMAP) == 0)
            continue;
        GL_Bind(image);
        qglTexParameteri(image->target, GL_TEXTURE_MIN_FILTER, rendererTextureMinFilter);
        qglTexParameteri(image->target, GL_TEXTURE_MAG_FILTER, rendererTextureMagFilter);
    }
}

/* Source: CoDUOMP.exe 0x005048a0..0x005048e9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005048a0_005048ea.mcode.
 * Name and source boundary: exact same-module Mac symbol generateHashValue.
 * Hashing stops at the extension, folds case, and normalizes backslashes so
 * equivalent renderer paths share one of the 4096 buckets. */
int32_t generateHashValue(const char *name)
{
    uint32_t hash = 0;

    for (uint32_t index = 0; name[index] != '\0'; ++index) {
        /* MOVSX at 0x005048b4 passes the original signed byte to tolower. */
        int32_t character = coduo_crt_tolower((int8_t)(uint8_t)name[index]);

        if (character == '.')
            break;
        if (character == '\\')
            character = '/';
        hash += (uint32_t)character * (R_IMAGE_HASH_CHARACTER_WEIGHT + index);
    }

    return (int32_t)(hash & (R_IMAGE_HASH_SIZE - 1));
}

/* Source: CoDUOMP.exe 0x0050aa90..0x0050aa97.
 * The original retained helper receives the index in EAX and performs an
 * unchecked lookup in tr.images. No corresponding Mac symbol survived, so
 * the name states the complete proven operation without implying validation. */
image_t *R_GetImageByIndex(int32_t imageIndex)
{
    return tr.images[imageIndex];
}

/* Source: CoDUOMP.exe 0x00506260..0x0050633a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00506260_0050633b.mcode.
 * Name and source boundary: exact same-module Mac symbol R_AllocImage. The
 * engine hunk owns image records for the renderer lifetime; only the two
 * upload-accounting fields are initialized here because the original does
 * not blanket-clear the record. */
image_t *R_AllocImage(const char *name, uint32_t target, int32_t width, int32_t height, uint32_t flags, int32_t imageTrack)
{
    if (strlen(name) >= R_IMAGE_NAME_SIZE)
        ri.Error(ERR_DROP, "\x15R_AllocImage: \"%s\" is too long\n", name);
    if (tr.imageCount == R_MAX_IMAGES)
        ri.Error(ERR_DROP, "\x15R_AllocImage: MAX_DRAWIMAGES hit\n");

    image_t *image = ri.Hunk_Alloc(sizeof(*image));
    tr.images[tr.imageCount] = image;
    image->texnum = (uint32_t)tr.imageCount + (uint32_t)R_IMAGE_FIRST_TEXTURE_ID;
    ++tr.imageCount;

    strcpy(image->imgName, name);
    image->width = (uint16_t)width;
    image->height = (uint16_t)height;
    image->flags = flags;
    image->target = target;
    image->cardMemory = 0;
    image->textureMemory = 0;
    image->imageTrack = (renderer_image_track_t)imageTrack;

    const int32_t hash = generateHashValue(name);
    image->hashNext = imageHashTable[hash];
    imageHashTable[hash] = image;
    return image;
}

/* Source: CoDUOMP.exe 0x00506340..0x0050637f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00506340_00506380.mcode.
 * Name and source boundary: exact same-module Mac symbol R_DeleteImage. */
void R_DeleteImage(image_t *image)
{
    if (glState.currenttextures[glState.currenttmu] == image->texnum) {
        qglBindTexture(image->target, 0);
        glState.currenttextures[glState.currenttmu] = 0;
    }
    qglDeleteTextures(1, &image->texnum);
}

/* Source: CoDUOMP.exe 0x00506380..0x005063dd.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00506380_005063de.mcode.
 * Name and source boundary: exact same-module Mac symbol R_FreeImage.
 * Hunk-owned image record storage itself is not individually freed. */
void R_FreeImage(image_t *image)
{
    const int32_t hash = generateHashValue(image->imgName);

    imageHashTable[hash] = image->hashNext;
    --tr.imageCount;
    R_DeleteImage(image);
}

/* Source: CoDUOMP.exe 0x005063e0..0x00506677.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005063e0_00506678.mcode.
 * Name and source boundary: exact same-module Mac symbol
 * R_CreateImageInternal. UploadImage remains its separately bounded original
 * callee; the image target and upload target are distinct for cube-map faces.
 */
qboolean R_CreateImageInternal(image_t *image, uint8_t *pixels, uint32_t uploadTarget, uint32_t format, const float *colorScale)
{
    const qboolean isLightmap = strncmp(image->imgName, "*lightmap", 9) == 0 ? qtrue : qfalse;

    if (colorScale != NULL) {
        const int32_t pixelCount = (int32_t)((uint32_t)image->width * (uint32_t)image->height);

        for (int32_t pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex) {
            for (int32_t channel = 0; channel < 4; ++channel) {
                const int32_t componentIndex = pixelIndex * 4 + channel;
                long double scaled = (long double)colorScale[channel] * (long double)pixels[componentIndex];

                if (scaled < 0.0L)
                    scaled = 0.0L;
                if (scaled > 255.0L)
                    scaled = 255.0L;
                pixels[componentIndex] = coduo_fp_to_u8_extended(scaled);
            }
        }
    }

    GL_Bind(image);

    int32_t cardMemory = 0;
    int32_t textureMemory = 0;
    if (UploadImage(image->imgName, pixels, image->target, uploadTarget, format, image->width, image->height, image->flags, isLightmap,
                    &image->internalFormat, &image->uploadWidth, &image->uploadHeight, &cardMemory, &textureMemory) == qfalse) {
        return qfalse;
    }

    image->cardMemory += cardMemory;
    image->textureMemory += textureMemory;
    tr.imageMemory += cardMemory;
    qglTexParameteri(image->target, GL_TEXTURE_WRAP_S, (image->flags & IMAGE_FLAG_CLAMP_S) != 0 ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    qglTexParameteri(image->target, GL_TEXTURE_WRAP_T, (image->flags & IMAGE_FLAG_CLAMP_T) != 0 ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    return qtrue;
}

/* Source: CoDUOMP.exe 0x00506680..0x0050670d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00506680_0050670e.mcode.
 * Name and source boundary: exact same-module Mac symbol R_CreateImage. The
 * delayed-upload branch records the active model image group instead of
 * creating a GL texture; the later sheet builder consumes those fields. */
image_t *R_CreateImage(const char *name, uint8_t *pixels, int32_t width, int32_t height, uint32_t format, uint32_t flags,
                       int32_t imageTrack, const float *colorScale)
{
    image_t *image = R_AllocImage(name, GL_TEXTURE_2D, width, height, flags, imageTrack);

    if ((flags & IMAGE_FLAG_DELAYED_UPLOAD) != 0) {
        if (format == GL_RGBA || (width > 3 && height > 3)) {
            image->internalFormat = format;
            image->link.delayedShader = NULL;
            image->state.delay.group = tr.delayedImageGroup;
            image->state.delay.groupTriCount = tr.delayedImageGroupTriCount;
            return image;
        }

        image->flags &= ~IMAGE_FLAG_DELAYED_UPLOAD;
        tr.delayedImageCount = (int32_t)((uint32_t)tr.delayedImageCount - 1u);
    }

    if (R_CreateImageInternal(image, pixels, GL_TEXTURE_2D, format, colorScale) == qfalse) {
        R_FreeImage(image);
        return NULL;
    }
    return image;
}
