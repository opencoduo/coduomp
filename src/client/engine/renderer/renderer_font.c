#include "backend.h"

#include "gl_state.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum {
    R_ASIAN_FONT_ATLAS_SIZE = 1024,
    R_ASIAN_FONT_MAX_PAGES = 4,
    R_FONT_CACHE_COUNT = 8,
    R_FONT_GLYPH_COUNT = 256,
    R_FONT_GLYPH_SHADER_COUNT = 255,
    R_FONT_ATLAS_MAX_PAGES = R_FONT_GLYPH_SHADER_COUNT,
    R_FONT_PATH_SIZE = 1024,
    R_LOCALIZED_FONT_PATH_SIZE = 256
};

typedef struct coduomp_font_atlas_page_s {
    char imageName[MAX_QPATH];
    uint8_t *pixels;
    uint16_t width;
    uint16_t height;
    uint16_t x;
    uint16_t y;
    uint32_t format;
} coduomp_font_atlas_page_t;

/* Original storage at 0x0387ba20..0x0387ba67. R_LoadAsianFont initializes the
 * page handles and the shared glyph record; R_GetCharacterGlyph rewrites only
 * the glyph's atlas coordinates and selected page handle. */
static int32_t rendererAsianFontPageHandles[R_ASIAN_FONT_MAX_PAGES];
static int32_t rendererAsianFontPageCount;
glyphInfo_t rendererAsianGlyph;

/* Original 0x0387be90 and 0x038537d8. The page dimension is 32 or 64 glyphs;
 * the last page uses half the ordinary row count for the proved encodings. */
static int32_t rendererAsianFontPageDimension;
static qboolean rendererAsianFontLastPageHalfHeight;

/* Original 0x005ce95c is initialized to -1 in the PE and compared with the
 * active cl_language before an already-loaded atlas may be reused. No direct
 * writer survives in the Windows image. */
static int32_t rendererAsianFontLoadedLanguage = -1;

/* Original storage at 0x038537e0..0x0387b9ff. RE_RegisterFont owns this
 * eight-entry cache and rendererRegisteredFontCount. */
static fontInfo_t rendererRegisteredFonts[R_FONT_CACHE_COUNT];

/* Source: CoDUOMP.exe 0x004e9460..0x004e946f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e9460_004e9470.mcode.
 * Name: exact same-module Mac symbol R_GetFontInfo. */
fontInfo_t *R_GetFontInfo(int32_t fontHandle, float scale)
{
    return ri.CL_GetFontInfo(fontHandle, scale);
}

/* Source: CoDUOMP.exe 0x004e8850..0x004e89b1.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e8850_004e89b2.mcode.
 * Name: exact same-module Mac symbol R_LoadAsianFont. The Windows LTCG body
 * inlines Language_IsAsian and SEH_GetCurrentLanguage, then registers the
 * language atlas pages through RE_RegisterShaderNoMip. */
void R_LoadAsianFont(int32_t loadMode)
{
    const char *languagePrefix;
    int32_t currentLanguage;
    int32_t glyphCellSize;

    if (Language_IsAsian() == qfalse) {
        rendererAsianFontLoaded = qfalse;
        return;
    }

    currentLanguage = cl_language->integer;
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (rendererAsianFontLoaded != qfalse &&
        rendererAsianFontLoadedLanguage == currentLanguage) {
        return;
    }

    switch ((language_t)currentLanguage) {
    case LANGUAGE_KOREAN:
        rendererAsianFontPageDimension = 32;
        rendererAsianFontPageCount = 3;
        languagePrefix = "kor";
        break;
    case LANGUAGE_TAIWANESE:
        rendererAsianFontPageDimension = 64;
        rendererAsianFontPageCount = 4;
        languagePrefix = "tai";
        break;
    case LANGUAGE_JAPANESE:
        rendererAsianFontPageDimension = 64;
        rendererAsianFontPageCount = 3;
        languagePrefix = "jap";
        break;
    case LANGUAGE_CHINESE:
        rendererAsianFontPageDimension = 64;
        rendererAsianFontPageCount = 3;
        languagePrefix = "chi";
        break;
    default:
        rendererAsianFontLoaded = qfalse;
        return;
    }

    glyphCellSize =
        R_ASIAN_FONT_ATLAS_SIZE / rendererAsianFontPageDimension;
    for (int32_t pageIndex = 0;
         pageIndex < rendererAsianFontPageCount;
         ++pageIndex) {
        char shaderName[MAX_QPATH];

        Com_sprintf(shaderName, sizeof(shaderName),
                    "font/%s_%d_1024_%d.tga",
                    languagePrefix, glyphCellSize, pageIndex);
        rendererAsianFontPageHandles[pageIndex] =
            RE_RegisterShaderNoMip(shaderName, loadMode);
    }

    rendererAsianFontLastPageHalfHeight = qtrue;
    rendererAsianGlyph.height = glyphCellSize;
    rendererAsianGlyph.width = glyphCellSize;
    rendererAsianGlyph.top = (float)glyphCellSize;
    rendererAsianGlyph.left = 0.0f;
    rendererAsianGlyph.xSkip = (float)glyphCellSize;
    rendererAsianGlyph.imageWidth = glyphCellSize;
    rendererAsianGlyph.imageHeight = glyphCellSize;
    rendererAsianFontLoaded = qtrue;
}

/* Source: CoDUOMP.exe 0x004e89d0..0x004e8a08.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e89d0_004e8a09.mcode.
 * Name and language-specific callees: exact same-module Mac symbols
 * R_GetAsianCode and the four Collapse*Code routines. */
int32_t R_GetAsianCode(int32_t character)
{
    if (rendererAsianFontLoaded == qfalse)
        return 0;

    switch ((language_t)cl_language->integer) {
    case LANGUAGE_KOREAN:
        return Korean_CollapseKSC5601HangulCode(character);
    case LANGUAGE_TAIWANESE:
        return Taiwanese_CollapseBig5Code(character);
    case LANGUAGE_JAPANESE:
        return Japanese_CollapseShiftJISCode(character);
    case LANGUAGE_CHINESE:
        return Chinese_CollapseGBCode(character);
    default:
        return 0;
    }
}

/* Source: CoDUOMP.exe 0x004e8a20..0x004e8c0d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e8a20_004e8c0e.mcode.
 * Name: exact same-module Mac symbol R_GetCharacterGlyph. */
glyphInfo_t *R_GetCharacterGlyph(int32_t character,
                                    fontInfo_t *font)
{
    const int32_t asianCode = R_GetAsianCode(character);

    if (asianCode != 0) {
        const int32_t pageArea =
            rendererAsianFontPageDimension *
            rendererAsianFontPageDimension;
        int32_t pageIndex = asianCode / pageArea;
        int32_t pageOffset;
        int32_t column;
        int32_t row;
        int32_t pageRowCount;

        if (pageIndex > rendererAsianFontPageCount)
            pageIndex = 0;

        pageOffset = asianCode - pageIndex * pageArea;
        row = pageOffset / rendererAsianFontPageDimension;
        column = pageOffset % rendererAsianFontPageDimension;
        pageRowCount = rendererAsianFontPageDimension;
        if (pageIndex == rendererAsianFontPageCount - 1 &&
            rendererAsianFontLastPageHalfHeight != qfalse) {
            pageRowCount = rendererAsianFontPageDimension / 2;
        }

        if (cl_language->integer == LANGUAGE_TAIWANESE) {
            const int32_t cellPixels =
                R_ASIAN_FONT_ATLAS_SIZE /
                rendererAsianFontPageDimension;

            rendererAsianGlyph.s =
                (float)(cellPixels * column + 1) * 0.0009765625f;
            rendererAsianGlyph.t =
                (float)(cellPixels * row + 1) * 0.0009765625f;
            rendererAsianGlyph.s2 =
                (float)(cellPixels * (column + 1)) * 0.0009765625f;
            rendererAsianGlyph.t2 =
                (float)(cellPixels * (row + 1)) * 0.0009765625f;
        } else if (cl_language->integer == LANGUAGE_JAPANESE ||
                   cl_language->integer == LANGUAGE_CHINESE) {
            const int32_t cellPixels =
                R_ASIAN_FONT_ATLAS_SIZE /
                rendererAsianFontPageDimension;

            rendererAsianGlyph.s =
                (float)(cellPixels * column) * 0.0009765625f;
            rendererAsianGlyph.t =
                (float)(cellPixels * row) * 0.0009765625f;
            rendererAsianGlyph.s2 =
                (float)(cellPixels * (column + 1) - 1) * 0.0009765625f;
            rendererAsianGlyph.t2 =
                (float)(cellPixels * (row + 1) - 1) * 0.0009765625f;
        } else {
            rendererAsianGlyph.s =
                (float)column / (float)rendererAsianFontPageDimension;
            rendererAsianGlyph.t =
                (float)row / (float)pageRowCount;
            rendererAsianGlyph.s2 =
                (float)(column + 1) /
                (float)rendererAsianFontPageDimension;
            rendererAsianGlyph.t2 =
                (float)(row + 1) / (float)pageRowCount;
        }

        rendererAsianGlyph.glyph =
            rendererAsianFontPageHandles[pageIndex];
        return &rendererAsianGlyph;
    }

    return &font->glyphs[(uint8_t)character];
}

/* Source: CoDUOMP.exe 0x004e8c10..0x004e8c55.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e8c10_004e8c55.mcode.
 * Name: exact same-module Mac symbol R_GetGlyphHorizAdvance. Windows also
 * inlines this decision tree into RE_Text_Width at 0x004e967f..0x004e96f5. */
float R_GetGlyphHorizAdvance(fontInfo_t *font, int32_t character)
{
    glyphInfo_t *glyph;

    if (R_GetAsianCode(character) != 0)
        return rendererAsianGlyph.xSkip;

    glyph = &font->glyphs[(uint8_t)character];
    if (glyph->xSkip != 0.0f)
        return glyph->xSkip;

    return font->glyphs[(uint8_t)'.'].xSkip;
}

/* Source: CoDUOMP.exe 0x004e8c60..0x004e8cd7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e8c60_004e8cd8.mcode.
 * Name: exact same-module Mac symbol R_GetAsianScale. The apparently unused
 * AdjustFrom640 result is intentional: the Windows body performs the import
 * call, then returns the independently retained scale value. */
float R_GetAsianScale(fontInfo_t *font, float scale)
{
    float asianScale =
        48.0f / font->glyphScale / (float)rendererAsianGlyph.height;

    if (scale > 0.25f) {
        asianScale *=
            ((scale - 0.25f) * 0.40000000596046448f + 0.25f) / scale;
    }

    float adjustedHeight =
        (float)rendererAsianGlyph.imageHeight * asianScale * scale *
        font->glyphScale;
    ri.AdjustFrom640(NULL, NULL, NULL, &adjustedHeight);
    return asianScale;
}

/* Source: CoDUOMP.exe 0x004e8ce0..0x004e8ce7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e8ce0_004e8ce7.mcode.
 * Name: exact same-module Mac symbol R_GetAsianGlyphHeight. */
float R_GetAsianGlyphHeight(void)
{
    return (float)rendererAsianGlyph.height;
}

/* NOT_FROM_ORIGINAL_SOURCE: shared spelling of the two proved in-function
 * forward-slash scans. */
static const char *R_FontBaseName(const char *path)
{
    const char *baseName = path;

    for (const char *cursor = path; *cursor != '\0'; ++cursor) {
        if (*cursor == '/')
            baseName = cursor + 1;
    }
    return baseName;
}

/* Source: CoDUOMP.exe 0x004e8d70..0x004e8e07.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e8d70_004e8e08.mcode.
 * Name: exact same-module Mac symbol RE_GetFontLanguageDAT. Calls to
 * Com_StripFilename, SEH_GetCurrentLanguage/SEH_GetLanguageName, va, and the
 * renderer file import are all present inlined or directly in the PE body. */
const char *RE_GetFontLanguageDAT(const char *fontDataName)
{
    const int32_t language = SEH_GetCurrentLanguage();
    char directory[R_LOCALIZED_FONT_PATH_SIZE];
    const char *localizedName;

    if (language == 0)
        return fontDataName;

    Com_StripFilename(fontDataName, directory);
    localizedName = va("%s%s/%s", directory,
                       SEH_GetLanguageName(language),
                       R_FontBaseName(fontDataName));
    if (ri.FS_ReadFile(localizedName, NULL) >= 0)
        return localizedName;
    return fontDataName;
}

/* Source: CoDUOMP.exe 0x004e8e10..0x004e8f3d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e8e10_004e8f3e.mcode.
 * Name: exact same-module Mac symbol RE_GetFontLanguageTGA. The localized
 * lookup tries the original TGA extension first, then DDS, before falling
 * back to the caller's name. */
const char *RE_GetFontLanguageTGA(const char *shaderName)
{
    const int32_t language = SEH_GetCurrentLanguage();
    char baseName[R_LOCALIZED_FONT_PATH_SIZE];
    char directory[R_LOCALIZED_FONT_PATH_SIZE];
    const char *source;
    char *destination;
    const char *localizedName;

    if (language == 0)
        return shaderName;

    Com_StripFilename(shaderName, directory);
    source = R_FontBaseName(shaderName);
    destination = baseName;
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    while (*source != '\0' && *source != '.') {
        if ((size_t)(destination - baseName) == sizeof(baseName) - 1u) {
            ri.Error(ERR_DROP, "\x15RE_GetFontLanguageTGA: name is too long");
            return shaderName;
        }
        *destination++ = *source++;
    }
    *destination = '\0';

    localizedName = va("%s%s/%s%s", directory,
                       SEH_GetLanguageName(language), baseName, ".tga");
    if (ri.FS_ReadFile(localizedName, NULL) >= 0)
        return localizedName;

    localizedName = va("%s%s/%s%s", directory,
                       SEH_GetLanguageName(language), baseName, ".dds");
    if (ri.FS_ReadFile(localizedName, NULL) >= 0)
        return localizedName;
    return shaderName;
}

/* Original font-file read cursor at 0x0387be94, relative to the temporary
 * file-buffer base at 0x0387ba88. Maintained source keeps the equivalent
 * native pointer so it remains valid on 64-bit hosts. */
static const uint8_t *rendererFontDataCursor;

/* Source: CoDUOMP.exe 0x004e8cf0..0x004e8d29.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e8cf0_004e8d2a.mcode.
 * Name and no-argument cursor contract: exact same-module Mac symbol
 * readInt. */
static uint32_t readInt(void)
{
    const uint8_t *bytes = rendererFontDataCursor;
    const uint32_t value =
        (uint32_t)bytes[0] |
        ((uint32_t)bytes[1] << 8) |
        ((uint32_t)bytes[2] << 16) |
        ((uint32_t)bytes[3] << 24);

    rendererFontDataCursor += sizeof(value);
    return value;
}

/* Source: CoDUOMP.exe 0x004e8d30..0x004e8d66.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e8d30_004e8d67.mcode.
 * Name and no-argument cursor contract: exact same-module Mac symbol
 * readFloat. */
static float readFloat(void)
{
    const uint32_t bits = readInt();
    float value;

    memcpy(&value, &bits, sizeof(value));
    return value;
}

/* NOT_FROM_ORIGINAL_SOURCE: readable factoring of the 256 iterations of the
 * inlined readInt/readFloat field sequence in the Windows body. */
static void R_ReadFontGlyph(glyphInfo_t *glyph)
{
    glyph->height = (int32_t)readInt();
    glyph->width = (int32_t)readInt();
    glyph->top = readFloat();
    glyph->left = readFloat();
    glyph->xSkip = readFloat();
    glyph->imageWidth = (int32_t)readInt();
    glyph->imageHeight = (int32_t)readInt();
    glyph->s = readFloat();
    glyph->t = readFloat();
    glyph->s2 = readFloat();
    glyph->t2 = readFloat();
    glyph->glyph = (int32_t)readInt();
    memcpy(glyph->shaderName, rendererFontDataCursor,
           sizeof(glyph->shaderName));
    rendererFontDataCursor += sizeof(glyph->shaderName);
}

/* OPTIMIZATION (NOT_FROM_ORIGINAL_SOURCE): copy one font page into a shared
 * load-time atlas without changing the page's compressed representation. */
static qboolean coduomp_font_copy_atlas_page(
    const image_t *atlasLayout, uint8_t *atlasPixels,
    const coduomp_font_atlas_page_t *page)
{
    switch (page->format) {
    case GL_RGBA:
        CopyImageTile_RGBA(atlasLayout, atlasPixels, page->pixels,
                           page->x, page->y, page->width, page->height);
        return qtrue;
    case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        CopyImageTile_DXT1(atlasLayout, atlasPixels, page->pixels,
                           page->x, page->y, page->width, page->height);
        return qtrue;
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
        CopyImageTile_DXT3(atlasLayout, atlasPixels, page->pixels,
                           page->x, page->y, page->width, page->height);
        return qtrue;
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        CopyImageTile_DXT5(atlasLayout, atlasPixels, page->pixels,
                           page->x, page->y, page->width, page->height);
        return qtrue;
    default:
        return qfalse;
    }
}

/* OPTIMIZATION (NOT_FROM_ORIGINAL_SOURCE): Latin font data can alternate
 * between several texture pages inside one string. Build one synchronous
 * atlas and remap the serialized glyph UVs once, so every glyph uses the same
 * shader without buffering or reordering any draw. */
static qboolean coduomp_font_build_atlas(fontInfo_t *font, int32_t pointSize,
                                         int32_t loadMode)
{
    coduomp_font_atlas_page_t pages[R_FONT_ATLAS_MAX_PAGES];
    int32_t glyphPages[R_FONT_GLYPH_SHADER_COUNT];
    int32_t pageCount = 0;
    int32_t columnCount;
    int32_t rowCount;
    int32_t atlasWidth;
    int32_t atlasHeight;
    int32_t atlasBytes;
    image_t atlasLayout;
    uint8_t *atlasPixels;
    image_t *atlasImage;
    int32_t atlasShader;
    char atlasName[MAX_QPATH];

    memset(pages, 0, sizeof(pages));
    for (int32_t glyphIndex = 0;
         glyphIndex < R_FONT_GLYPH_SHADER_COUNT;
         ++glyphIndex) {
        const char *localizedName = RE_GetFontLanguageTGA(
            font->glyphs[glyphIndex].shaderName);
        int32_t pageIndex;

        for (pageIndex = 0; pageIndex < pageCount; ++pageIndex) {
            if (Q_stricmp(pages[pageIndex].imageName, localizedName) == 0)
                break;
        }
        if (pageIndex == pageCount) {
            if (pageCount >= R_FONT_ATLAS_MAX_PAGES)
                return qfalse;
            Q_strncpyz(pages[pageCount].imageName, localizedName,
                       sizeof(pages[pageCount].imageName));
            ++pageCount;
        }
        glyphPages[glyphIndex] = pageIndex;
    }

    if (pageCount < 2)
        return qfalse;

    for (int32_t pageIndex = 0; pageIndex < pageCount; ++pageIndex) {
        qboolean mipMapsAvailable;

        R_LoadImage(pages[pageIndex].imageName,
                    &pages[pageIndex].pixels,
                    &pages[pageIndex].width,
                    &pages[pageIndex].height,
                    &pages[pageIndex].format,
                    &mipMapsAvailable, R_IMAGE_LOAD_PIXELS);
        if (pages[pageIndex].pixels == NULL ||
            pages[pageIndex].width == 0 ||
            pages[pageIndex].height == 0 ||
            (pageIndex != 0 &&
             (pages[pageIndex].width != pages[0].width ||
              pages[pageIndex].height != pages[0].height ||
              pages[pageIndex].format != pages[0].format))) {
            R_FreeImageAllocations();
            return qfalse;
        }
    }

    columnCount = 1;
    while (columnCount * columnCount < pageCount)
        columnCount <<= 1;
    rowCount = (pageCount + columnCount - 1) / columnCount;
    atlasWidth = SmallestTextureSizeFitting(
        (int32_t)pages[0].width * columnCount);
    atlasHeight = SmallestTextureSizeFitting(
        (int32_t)pages[0].height * rowCount);
    if (atlasWidth > glConfig.maxTextureSize ||
        atlasHeight > glConfig.maxTextureSize) {
        R_FreeImageAllocations();
        return qfalse;
    }

    atlasBytes = GetCardMemoryAmount(pages[0].format,
                                     atlasWidth, atlasHeight);
    if (atlasBytes <= 0) {
        R_FreeImageAllocations();
        return qfalse;
    }
    atlasPixels = R_AllocTempMemory((size_t)atlasBytes);
    if (atlasPixels == NULL) {
        R_FreeImageAllocations();
        return qfalse;
    }
    memset(atlasPixels, 0, (size_t)atlasBytes);

    memset(&atlasLayout, 0, sizeof(atlasLayout));
    atlasLayout.width = (uint16_t)atlasWidth;
    atlasLayout.height = (uint16_t)atlasHeight;
    atlasLayout.internalFormat = pages[0].format;
    for (int32_t pageIndex = 0; pageIndex < pageCount; ++pageIndex) {
        pages[pageIndex].x = (uint16_t)(
            (pageIndex % columnCount) * pages[0].width);
        pages[pageIndex].y = (uint16_t)(
            (pageIndex / columnCount) * pages[0].height);
        if (coduomp_font_copy_atlas_page(
                &atlasLayout, atlasPixels, &pages[pageIndex]) == qfalse) {
            R_FreeImageAllocations();
            return qfalse;
        }
    }

    Com_sprintf(atlasName, sizeof(atlasName), "*fontAtlas_%d_%d",
                pointSize, cl_language->integer);
    atlasImage = R_CreateImage(
        atlasName, atlasPixels, atlasWidth, atlasHeight, pages[0].format,
        IMAGE_FLAG_CLAMP_S | IMAGE_FLAG_CLAMP_T, loadMode, NULL);
    R_FreeImageAllocations();
    if (atlasImage == NULL)
        return qfalse;

    atlasShader = R_RegisterShaderFromImage(
        atlasName, LIGHTMAP_2D, atlasImage);
    if (atlasShader == 0)
        return qfalse;

    for (int32_t glyphIndex = 0;
         glyphIndex < R_FONT_GLYPH_SHADER_COUNT;
         ++glyphIndex) {
        glyphInfo_t *glyph = &font->glyphs[glyphIndex];
        const coduomp_font_atlas_page_t *page =
            &pages[glyphPages[glyphIndex]];
        const float scaleS = (float)page->width / (float)atlasWidth;
        const float scaleT = (float)page->height / (float)atlasHeight;
        const float offsetS = (float)page->x / (float)atlasWidth;
        const float offsetT = (float)page->y / (float)atlasHeight;

        glyph->s = glyph->s * scaleS + offsetS;
        glyph->s2 = glyph->s2 * scaleS + offsetS;
        glyph->t = glyph->t * scaleT + offsetT;
        glyph->t2 = glyph->t2 * scaleT + offsetT;
        glyph->glyph = atlasShader;
    }
    return qtrue;
}

/* Source: CoDUOMP.exe 0x004e8f40..0x004e9435.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e8f40_004e9436.mcode.
 * Name: exact same-module Mac symbol RE_RegisterFont. The Windows body
 * inlines readInt/readFloat and RE_RegisterShaderNoMip while decoding the
 * fixed little-endian font record. */
void RE_RegisterFont(const char *name, int32_t pointSize,
                     fontInfo_t *font, int32_t loadMode)
{
    char fontDataName[R_FONT_PATH_SIZE];
    void *fileBuffer = NULL;

    /* The Windows body never reads this legacy renderer-export argument. */
    (void)name;
    if (pointSize <= 0)
        pointSize = 12;

    if (tr.registered != qfalse)
        R_IssueRenderCommands(qfalse);
    R_LoadAsianFont(loadMode);

    if (rendererRegisteredFontCount >= R_FONT_CACHE_COUNT) {
        ri.Printf(R_PRINT_ALL,
                  "RE_RegisterFont: Too many fonts registered already.\n");
        return;
    }

    Com_sprintf(fontDataName, sizeof(fontDataName),
                "fonts/fontImage_%i.dat", pointSize);
    for (int32_t fontIndex = 0;
         fontIndex < rendererRegisteredFontCount;
         ++fontIndex) {
        if (Q_stricmp(rendererRegisteredFonts[fontIndex].fontDataName,
                      fontDataName) == 0) {
            *font = rendererRegisteredFonts[fontIndex];
            return;
        }
    }

    if (ri.FS_ReadFile(RE_GetFontLanguageDAT(fontDataName), NULL) !=
        (int32_t)sizeof(*font)) {
        ri.Printf(R_PRINT_ALL,
                  "RE_RegisterFont: Unable to load font data %s\n",
                  fontDataName);
        return;
    }

    const int32_t loadedFontDataSize =
        ri.FS_ReadFile(RE_GetFontLanguageDAT(fontDataName), &fileBuffer);
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if (loadedFontDataSize != (int32_t)sizeof(*font)) {
        if (fileBuffer != NULL)
            ri.FS_FreeFile(fileBuffer);
        ri.Printf(R_PRINT_ALL,
                  "RE_RegisterFont: Unable to load font data %s\n",
                  fontDataName);
        return;
    }
    rendererFontDataCursor = fileBuffer;
    for (int32_t glyphIndex = 0;
         glyphIndex < R_FONT_GLYPH_COUNT;
         ++glyphIndex) {
        R_ReadFontGlyph(&font->glyphs[glyphIndex]);
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
        if (memchr(font->glyphs[glyphIndex].shaderName, '\0',
                   sizeof(font->glyphs[glyphIndex].shaderName)) == NULL) {
            ri.FS_FreeFile(fileBuffer);
            ri.Error(ERR_DROP,
                     "\x15RE_RegisterFont: unterminated glyph shader name");
            return;
        }
    }
    font->glyphScale = readFloat();
    font->lineHeight = readFloat();
    rendererFontDataCursor += sizeof(font->fontDataName);
    Q_strncpyz(font->fontDataName, fontDataName,
               sizeof(font->fontDataName));

    if (coduomp_font_build_atlas(font, pointSize, loadMode) == qfalse) {
        for (int32_t glyphIndex = 0;
             glyphIndex < R_FONT_GLYPH_SHADER_COUNT;
             ++glyphIndex) {
            glyphInfo_t *glyph = &font->glyphs[glyphIndex];
            glyph->glyph = RE_RegisterShaderNoMip(
                RE_GetFontLanguageTGA(glyph->shaderName), loadMode);
        }
    }

    rendererRegisteredFonts[rendererRegisteredFontCount++] = *font;
    ri.FS_FreeFile(fileBuffer);
}
