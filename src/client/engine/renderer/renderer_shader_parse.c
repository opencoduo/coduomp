#include "backend.h"
#include "gl_api.h"

#include "../client/cinematic.h"
#include "../math/vector_math.h"
#include "../platform/crt_boundary.h"
#include "../surface_types.h"
#include "compat/crt/qsort_compat.h"

#include <ctype.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

enum {
    R_SHADER_HASH_SIZE = 4096,
    R_SHADER_HASH_CHARACTER_WEIGHT = 119,
    R_MAX_SHADER_FILES = 4096,
    R_MAX_SHADER_STRING_POINTERS = 100000,
    R_MAX_SHADER_BRACE_DEPTH = 5,
    R_INTERNAL_SHADER_TEXCOORD_COMPONENTS = 2,
    R_MAX_TEXTURE_MODIFIERS = 4,
    R_SHADER_TOKEN_COMPARE_LIMIT = 99999,
    R_PARSE_IMAGE_COLOR_SOURCE_FLAG = 0x10,
    R_PARSE_IMAGE_LIGHTMAP_SOURCE_FLAG = 0x20,
    R_PARSE_IMAGE_16_BIT_COLOR_DEPTH = 16,
    R_PARSE_IMAGE_NO_LIGHTMAP_MODE = -1,
    R_PARSE_IMAGE_GRAYMAP_EXCLUDED_MODE_A = -4,
    R_PARSE_IMAGE_GRAYMAP_EXCLUDED_MODE_B = -2,
    R_WATER_MAP_MIN_TEXTURE_SIZE = 4,
    R_WATER_MAP_MAX_TEXTURE_SIZE = 256,
    R_STAGE_TEXMOD_TEXT_SIZE = 1024,
    R_SHADER_TYPE_PATH_CHARACTER_OVERHEAD = (sizeof("shadertypes/") - 1u) + (sizeof(".stype") - 1u)
};

/* Original shader-parser scratch storage at 0x038802d0 and 0x03880478. A
 * parsed or cloned shader redirects its live stage pointers into this
 * eight-stage bank before GeneratePermanentShader consumes it. */
shader_t rendererParsedShader; /* 0x038802d0 */
uint8_t rendererShaderRequirements[SHADER_REQUIREMENT_COUNT]; /* 0x03880468 */
shaderStage_t rendererParsedShaderStages[R_MAX_SHADER_STAGES]; /* 0x03880478 */
static texModInfo_t rendererParsedShaderTexMods[R_MAX_SHADER_STAGES][R_MAX_TEXTURE_UNITS][R_MAX_TEXTURE_MODIFIERS]; /* 0x0387bed0 */
static shader_t *rendererShaderHashTable[R_SHADER_HASH_SIZE]; /* 0x038838b8 */

typedef struct shader_text_hash_node_s {
    char *definitionText;
    struct shader_text_hash_node_s *next;
} shader_text_hash_node_t;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(shader_text_hash_node_t) == 4, "i386 shader-text hash node alignment changed");
_Static_assert(offsetof(shader_text_hash_node_t, definitionText) == 0x00, "i386 shader-text definition pointer moved");
_Static_assert(offsetof(shader_text_hash_node_t, next) == 0x04, "i386 shader-text collision link moved");
_Static_assert(sizeof(shader_text_hash_node_t) == 0x08, "i386 shader-text hash node size changed");
#endif

/* Original combined shader-text pointer at 0x038878b8 and 4096-bucket
 * definition index at 0x0397a6c0. The buckets point into the combined text;
 * their collision nodes are recovered with the index builder at 0x00503f30. */
static char *rendererShaderText;
/* Original collision-node bank 0x038b71c0..0x0397a6c0. The 0xc3500-byte
 * i386 range contains exactly 100000 two-pointer records. */
static shader_text_hash_node_t rendererShaderTextHashNodes[R_MAX_SHADER_STRING_POINTERS];
static shader_text_hash_node_t rendererShaderTextHashTable[R_SHADER_HASH_SIZE]; /* 0x0397a6c0 */

typedef struct shader_multitexture_collapse_rule_s {
    uint32_t firstBlendBits;
    uint32_t secondBlendBits;
    uint32_t textureEnvMode;
    uint32_t collapsedBlendBits;
} shader_multitexture_collapse_rule_t;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(shader_multitexture_collapse_rule_t) == 4, "i386 multitexture-collapse rule alignment changed");
_Static_assert(offsetof(shader_multitexture_collapse_rule_t, firstBlendBits) == 0x00, "i386 first multitexture blend bits moved");
_Static_assert(offsetof(shader_multitexture_collapse_rule_t, secondBlendBits) == 0x04, "i386 second multitexture blend bits moved");
_Static_assert(offsetof(shader_multitexture_collapse_rule_t, textureEnvMode) == 0x08, "i386 multitexture environment mode moved");
_Static_assert(offsetof(shader_multitexture_collapse_rule_t, collapsedBlendBits) == 0x0c, "i386 collapsed multitexture blend bits moved");
_Static_assert(sizeof(shader_multitexture_collapse_rule_t) == 0x10, "i386 multitexture-collapse rule size changed");
#endif

/* Original CoDUOMP.exe 0x005912f0..0x0059137f. */
static const shader_multitexture_collapse_rule_t shaderMultitextureCollapseRules[] = {
    {0, GLS_SRCBLEND_ZERO | GLS_DSTBLEND_SRC_COLOR, GL_MODULATE, 0},
    {0, GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO, GL_MODULATE, 0},
    {GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO, GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO, GL_MODULATE,
     GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO},
    {GLS_SRCBLEND_ZERO | GLS_DSTBLEND_SRC_COLOR, GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO, GL_MODULATE,
     GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO},
    {GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO, GLS_SRCBLEND_ZERO | GLS_DSTBLEND_SRC_COLOR, GL_MODULATE,
     GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO},
    {GLS_SRCBLEND_ZERO | GLS_DSTBLEND_SRC_COLOR, GLS_SRCBLEND_ZERO | GLS_DSTBLEND_SRC_COLOR, GL_MODULATE,
     GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO},
    {0, GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE, GL_ADD, 0},
    {GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE, GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE, GL_ADD, GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE},
    {0, GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA, GL_DECAL, 0}};

/* Source: CoDUOMP.exe 0x004f70f0..0x004f70fa.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f70f0_004f70fb.mcode.
 * Role name: the Mac traceback table has no separate symbol. The helper uses
 * the original power-of-two predicate; zero also satisfies it, while callers
 * constrain the accepted range separately. */
static qboolean R_IsPowerOfTwo(int32_t value)
{
    return (value & (value - 1)) == 0 ? qtrue : qfalse;
}

/* Source: CoDUOMP.exe 0x004f7100..0x004f7149.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f7100_004f714a.mcode.
 * The Mac renderer calls each subsystem-local version generateHashValue;
 * this role-qualified name distinguishes the shader-text table hash from the
 * separately recovered image hash with the same original static name. */
uint32_t R_GenerateShaderHashValue(const char *name)
{
    uint32_t hash = 0;

    for (uint32_t index = 0; name[index] != '\0'; ++index) {
        /* MOVSX at 0x004f7114 passes the original signed byte to tolower. */
        int32_t character = coduo_crt_tolower((int8_t)(uint8_t)name[index]);
        if (character == '.')
            break;
        if (character == '\\')
            character = '/';
        hash += (uint32_t)character * (R_SHADER_HASH_CHARACTER_WEIGHT + index);
    }

    return hash & (R_SHADER_HASH_SIZE - 1);
}

/* Source: CoDUOMP.exe 0x00503f30..0x0050409b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00503f30_0050409c.mcode.
 * Name and void signature: exact same-module Mac symbol
 * BuildShaderChecksumLookup.
 *
 * Each entry points to the beginning of a shader-name token in the combined
 * shader text. A bucket's first definition lives directly in the bucket;
 * later collisions are prepended from the fixed 100000-node bank. The parser
 * mark is used only to retain the exact pre-token cursor. In particular, an
 * outstanding Com_UngetToken means the true token start is
 * savedParse, not the caller's current cursor. */
void BuildShaderChecksumLookup(void)
{
    char *parseCursor;
    size_t collisionNodeCount = 0;
    qboolean excessiveBraceDepth = qfalse;

    memset(rendererShaderTextHashTable, 0, sizeof(rendererShaderTextHashTable));
    if (rendererShaderText == NULL)
        return;

    parseCursor = rendererShaderText;
    for (;;) {
        com_parse_mark_t tokenMark;
        char *definitionText;
        char *token;
        shader_text_hash_node_t *bucket;

        Com_ParseSetMark(&parseCursor, &tokenMark);
        token = Com_Parse(&parseCursor);
        definitionText = tokenMark.ungetToken != qfalse ? tokenMark.savedParse : tokenMark.parse;

        if (token[0] == '\0')
            return;

        if (Q_stricmpn(token, "{", R_SHADER_TOKEN_COMPARE_LIMIT) == 0) {
            Com_UngetToken();
            if (Com_SkipBracedSection(&parseCursor, R_MAX_SHADER_BRACE_DEPTH) != qfalse) {
                ri.Printf(R_PRINT_WARNING, "Shader brace nesting depth exceeded... missing '}'?\n");
                excessiveBraceDepth = qtrue;
            }
            continue;
        }

        if (excessiveBraceDepth != qfalse) {
            ri.Printf(R_PRINT_WARNING, "Look before shader '%s'\n", token);
            excessiveBraceDepth = qfalse;
        }

        bucket = &rendererShaderTextHashTable[R_GenerateShaderHashValue(token)];
        if (bucket->definitionText == NULL) {
            bucket->definitionText = definitionText;
        } else {
            shader_text_hash_node_t *node;

            if (collisionNodeCount >= R_MAX_SHADER_STRING_POINTERS) {
                ri.Error(ERR_DROP, "\x15"
                                   "MAX_SHADER_STRING_POINTERS exceeded, "
                                   "too many shaders");
            }

            node = &rendererShaderTextHashNodes[collisionNodeCount++];
            node->definitionText = definitionText;
            node->next = bucket->next;
            bucket->next = node;
        }
    }
}

/* Source: CoDUOMP.exe 0x005040a0..0x005042ea.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005040a0_005042eb.mcode.
 * Name and void signature: exact same-module Mac symbol
 * ScanAndLoadShaderFiles.
 *
 * Ordinary renderer shaders are loaded first, followed by effect shaders.
 * The Windows function caps the combined list at 4096 entries, retains every
 * file buffer until the combined hunk string has been assembled, then frees
 * the buffers and both filesystem lists before building the definition
 * lookup. */
void ScanAndLoadShaderFiles(void)
{
    char **shaderFiles;
    char **fxShaderFiles;
    char *fileBuffers[R_MAX_SHADER_FILES];
    char fileName[MAX_QPATH];
    int32_t shaderFileCount;
    int32_t fxShaderFileCount;
    int32_t fileCount;
    int32_t shaderTextLength = 0;

    shaderFiles = ri.FS_ListFiles("scripts", ".shader", &shaderFileCount);
    fxShaderFiles = ri.FS_ListFiles("fxshaders", ".shader", &fxShaderFileCount);
    fileCount = shaderFileCount + fxShaderFileCount;

    if (shaderFiles == NULL || shaderFileCount == 0) {
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        ri.Printf(R_PRINT_WARNING, "WARNING: no shader files found\n");
        ri.FS_FreeFileList(fxShaderFiles);
        ri.FS_FreeFileList(shaderFiles);
        return;
    }

    if (fileCount > R_MAX_SHADER_FILES)
        fileCount = R_MAX_SHADER_FILES;

    for (int32_t fileIndex = 0; fileIndex < fileCount; ++fileIndex) {
        if (fileIndex < shaderFileCount) {
            Com_sprintf(fileName, sizeof(fileName), "scripts/%s", shaderFiles[fileIndex]);
        } else {
            Com_sprintf(fileName, sizeof(fileName), "fxshaders/%s", fxShaderFiles[fileIndex - shaderFileCount]);
        }

        if (r_debugOptTex->integer != 0)
            ri.Printf(R_PRINT_ALL, "...loading '%s'\n", fileName);

        shaderTextLength += ri.FS_ReadFile(fileName, (void **)&fileBuffers[fileIndex]);
        if (fileBuffers[fileIndex] == NULL) {
            ri.Error(ERR_DROP, va("EXE_ERR_COULDNT_LOAD\x15%s", fileName));
        }
    }

    rendererShaderText = ri.Hunk_Alloc((size_t)(shaderTextLength + fileCount * 2));
    for (int32_t fileIndex = 0; fileIndex < fileCount; ++fileIndex) {
        strcat(rendererShaderText, "\n");
        strcat(rendererShaderText, fileBuffers[fileIndex]);
    }

    for (int32_t fileIndex = fileCount - 1; fileIndex >= 0; --fileIndex) {
        ri.FS_FreeFile(fileBuffers[fileIndex]);
    }

    ri.FS_FreeFileList(fxShaderFiles);
    ri.FS_FreeFileList(shaderFiles);
    BuildShaderChecksumLookup();
}

/* Source: CoDUOMP.exe 0x005042f0..0x0050437e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005042f0_0050437f.mcode.
 * Name and void signature: exact same-module Mac symbol
 * CreateDefaultShader. */
void CreateDefaultShader(void)
{
    shaderStage_t *const stage = &rendererParsedShaderStages[0];
    textureBundle_t *const bundle = &stage->bundle[0];

    memset(&rendererParsedShader, 0, sizeof(rendererParsedShader));
    memset(rendererParsedShaderStages, 0, sizeof(rendererParsedShaderStages));
    strncpy(rendererParsedShader.name, "<default>", sizeof(rendererParsedShader.name) - 1);
    rendererParsedShader.name[sizeof(rendererParsedShader.name) - 1] = '\0';

    rendererParsedShader.lightmapIndex = LIGHTMAP_NONE;
    rendererParsedShader.flags = SHADER_FLAG_DEFAULTED;
    rendererParsedShader.surfaceFlags = SURF_NOIMPACT;
    stage->flags = SHADER_STAGE_ACTIVE;
    bundle->image[0] = tr.defaultImage;
    bundle->texCoordComponentCount = R_INTERNAL_SHADER_TEXCOORD_COMPONENTS;
    bundle->textureEnvMode = GL_MODULATE;
    stage->stateBits = GLS_DEPTHMASK_TRUE;

    tr.defaultShader = FinishShader();
}

/* Source: CoDUOMP.exe 0x00504380..0x005043d5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00504380_005043d6.mcode.
 * Name and void signature: exact same-module Mac symbol
 * CreateShadowShader. */
void CreateShadowShader(void)
{
    memset(&rendererParsedShader, 0, sizeof(rendererParsedShader));
    memset(rendererParsedShaderStages, 0, sizeof(rendererParsedShaderStages));
    strncpy(rendererParsedShader.name, "<stencil shadow>", sizeof(rendererParsedShader.name) - 1);
    rendererParsedShader.name[sizeof(rendererParsedShader.name) - 1] = '\0';

    rendererParsedShader.lightmapIndex = LIGHTMAP_NONE;
    rendererParsedShader.sort = SHADER_SORT_STENCIL_SHADOW;
    tr.stencilShadowShader = FinishShader();
}

/* Source: CoDUOMP.exe 0x005043e0..0x005044af.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005043e0_005044b0.mcode.
 * Name and void signature: exact same-module Mac symbol
 * CreateShowTrisShader. The blue channel distinguishes the optimized
 * renderer path: yellow when optimization is enabled, white otherwise. */
void CreateShowTrisShader(void)
{
    shaderStage_t *const stage = &rendererParsedShaderStages[0];
    textureBundle_t *const bundle = &stage->bundle[0];
    const uint8_t identityLight = (uint8_t)tr.identityLightByte;

    memset(&rendererParsedShader, 0, sizeof(rendererParsedShader));
    memset(rendererParsedShaderStages, 0, sizeof(rendererParsedShaderStages));
    strncpy(rendererParsedShader.name, "<showtris>", sizeof(rendererParsedShader.name) - 1);
    rendererParsedShader.name[sizeof(rendererParsedShader.name) - 1] = '\0';

    rendererParsedShader.lightmapIndex = LIGHTMAP_NONE;
    rendererParsedShader.flags = SHADER_FLAG_NO_FOG;
    rendererParsedShader.surfaceFlags = SURF_NOIMPACT;
    rendererParsedShader.cullType = CT_TWO_SIDED;

    stage->flags = SHADER_STAGE_ACTIVE;
    bundle->image[0] = tr.whiteImage;
    bundle->texCoordComponentCount = R_INTERNAL_SHADER_TEXCOORD_COMPONENTS;
    bundle->textureEnvMode = GL_MODULATE;
    stage->rgbGen = CGEN_DEBUG_SURFACE_COUNT;
    stage->constantColor[0] = identityLight;
    stage->constantColor[1] = identityLight;
    stage->constantColor[2] = 0;
    if (r_optimize->integer == 0)
        stage->constantColor[2] = identityLight;
    stage->stateBits = GLS_DEPTHMASK_TRUE | GLS_POLYMODE_LINE;

    tr.showTrisShader = FinishShader();
}

/* Source: CoDUOMP.exe 0x005044b0..0x00504568.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005044b0_00504569.mcode.
 * Name and void signature: exact same-module Mac symbol
 * CreateShowImagesShader. */
void CreateShowImagesShader(void)
{
    shaderStage_t *const stage = &rendererParsedShaderStages[0];
    textureBundle_t *const bundle = &stage->bundle[0];
    const uint8_t identityLight = (uint8_t)tr.identityLightByte;

    memset(&rendererParsedShader, 0, sizeof(rendererParsedShader));
    memset(rendererParsedShaderStages, 0, sizeof(rendererParsedShaderStages));
    strncpy(rendererParsedShader.name, "<showimages>", sizeof(rendererParsedShader.name) - 1);
    rendererParsedShader.name[sizeof(rendererParsedShader.name) - 1] = '\0';

    rendererParsedShader.lightmapIndex = LIGHTMAP_NONE;
    rendererParsedShader.flags = SHADER_FLAG_NO_FOG;
    rendererParsedShader.surfaceFlags = SURF_NOIMPACT;
    rendererParsedShader.cullType = CT_TWO_SIDED;

    stage->flags = SHADER_STAGE_ACTIVE;
    bundle->image[0] = tr.whiteImage;
    bundle->texCoordComponentCount = R_INTERNAL_SHADER_TEXCOORD_COMPONENTS;
    bundle->textureEnvMode = GL_MODULATE;
    stage->rgbGen = CGEN_IDENTITY_LIGHTING;
    stage->constantColor[0] = identityLight;
    stage->constantColor[1] = identityLight;
    stage->constantColor[2] = identityLight;
    stage->stateBits = GLS_DEPTHTEST_DISABLE;

    tr.showImagesShader = FinishShader();
}

/* Source: CoDUOMP.exe 0x00504570..0x0050461c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00504570_0050461d.mcode.
 * Name and void signature: exact same-module Mac symbol
 * CreateScreenShader. */
void CreateScreenShader(void)
{
    shaderStage_t *const stage = &rendererParsedShaderStages[0];
    textureBundle_t *const bundle = &stage->bundle[0];

    memset(&rendererParsedShader, 0, sizeof(rendererParsedShader));
    memset(rendererParsedShaderStages, 0, sizeof(rendererParsedShaderStages));
    strncpy(rendererParsedShader.name, "<screen>", sizeof(rendererParsedShader.name) - 1);
    rendererParsedShader.name[sizeof(rendererParsedShader.name) - 1] = '\0';

    rendererParsedShader.lightmapIndex = LIGHTMAP_2D;
    rendererParsedShader.flags = SHADER_FLAG_NO_FOG;
    rendererParsedShader.surfaceFlags = SHADER_SURFACE_BASE_TEXCOORDS | SHADER_SURFACE_VERTEX_COLORS;
    rendererParsedShader.cullType = CT_TWO_SIDED;

    stage->flags = SHADER_STAGE_ACTIVE;
    bundle->image[0] = tr.screenImage;
    bundle->texCoordComponentCount = R_INTERNAL_SHADER_TEXCOORD_COMPONENTS;
    bundle->textureEnvMode = GL_MODULATE;
    stage->rgbGen = CGEN_EXACT_VERTEX;
    stage->alphaGen = AGEN_VERTEX;
    stage->stateBits = GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;

    tr.screenImageShader = FinishShader();
}

/* Source: CoDUOMP.exe 0x00504620..0x00504642, recovered from the executable
 * gap after repairing the missing Ghidra function boundary.
 * Name and void signature: exact same-module Mac symbol
 * CreateInternalShaders. */
void CreateInternalShaders(void)
{
    tr.numShaders = 0;
    CreateDefaultShader();
    CreateShadowShader();
    CreateShowTrisShader();
    CreateShowImagesShader();
    CreateScreenShader();
}

/* Source: CoDUOMP.exe 0x00504650..0x00504692, recovered from the executable
 * gap after repairing the missing Ghidra function boundary.
 * Name and void signature: exact same-module Mac symbol
 * CreateExternalShaders. */
void CreateExternalShaders(void)
{
    tr.flareShader = R_FindShader("flareShader", LIGHTMAP_NONE, qtrue, R_IMAGE_TRACK_EFFECT);
    tr.spotLightShader = R_FindShader("spotLight", LIGHTMAP_NONE, qtrue, R_IMAGE_TRACK_EFFECT);
    tr.dlightShader = R_FindShader("dlightshader", LIGHTMAP_NONE, qtrue, R_IMAGE_TRACK_EFFECT);
}

/* Source: CoDUOMP.exe 0x005046a0..0x0050472c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005046a0_0050472d.mcode.
 * Name and void signature: exact same-module Mac symbol R_InitShaders.
 * The Windows LTCG body inlines CreateInternalShaders and
 * CreateExternalShaders; the separately retained helper bodies above and
 * same-module Mac symbols prove the original source-level factoring. */
void R_InitShaders(void)
{
    rendererCurrentFogIndex = 0;
    ri.Printf(R_PRINT_ALL, "Initializing Shaders\n");
    memset(rendererShaderHashTable, 0, sizeof(rendererShaderHashTable));

    CreateInternalShaders();
    ScanAndLoadShaderFiles();
    CreateExternalShaders();
}

/* Source: CoDUOMP.exe 0x005027b0..0x0050284a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005027b0_0050284b.mcode.
 * Name and return type: exact same-module Mac symbol
 * FindShaderInShaderText.
 *
 * Each hash entry retains the cursor at the definition's shader-name token.
 * Parsing a private cursor obtains that token without changing the indexed
 * text. A match returns the advanced cursor immediately after the name, which
 * is the opening-brace position consumed by ParseShader. */
char *FindShaderInShaderText(const char *shaderName)
{
    shader_text_hash_node_t *node;

    if (rendererShaderText == NULL)
        return NULL;

    node = &rendererShaderTextHashTable[R_GenerateShaderHashValue(shaderName)];
    while (node != NULL && node->definitionText != NULL) {
        char *parseCursor = node->definitionText;
        const char *const token = Com_Parse(&parseCursor);

        if (token[0] != '\0' && shaderName != NULL && Q_stricmpn(token, shaderName, R_SHADER_TOKEN_COMPARE_LIMIT) == 0) {
            return parseCursor;
        }
        node = node->next;
    }
    return NULL;
}

/* Source: CoDUOMP.exe 0x00502850..0x005028e8.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00502850_005028e9.mcode.
 * Name and signature: exact same-module Mac symbol R_FindShaderByName.
 *
 * Shader registry keys omit the filename extension. The original copies up
 * to the first dot into a MAX_QPATH local and then searches the corresponding
 * collision chain case-insensitively. */
shader_t *R_FindShaderByName(const char *name)
{
    char strippedName[MAX_QPATH];
    shader_t *shader;
    size_t strippedLength = 0;

    if (name == NULL || name[0] == '\0')
        return tr.defaultShader;

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    while (*name != '\0' && *name != '.') {
        if (strippedLength == sizeof(strippedName) - 1u)
            return tr.defaultShader;
        strippedName[strippedLength++] = *name++;
    }
    strippedName[strippedLength] = '\0';

    shader = rendererShaderHashTable[R_GenerateShaderHashValue(strippedName)];
    while (shader != NULL) {
        if (Q_stricmpn(shader->name, strippedName, R_SHADER_TOKEN_COMPARE_LIMIT) == 0) {
            return shader;
        }
        shader = shader->next;
    }
    return tr.defaultShader;
}

/* Source: CoDUOMP.exe 0x005028f0..0x00502a7d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005028f0_00502a7e.mcode.
 * Name and signature: exact same-module Mac symbol R_LoadShaderType.
 *
 * A shader name beginning with '@' names a reusable shader-type definition.
 * Its body is loaded from shadertypes/<name>.stype into the normal parser
 * scratch state, then finalized under the original '@'-prefixed shader name.
 * The machine-code length preflight undercounts the full pathname overhead by
 * one byte. */
shader_t *R_LoadShaderType(const char *shaderTypeName, renderer_image_track_t imageTrack)
{
    char fileName[MAX_QPATH] = "shadertypes/";
    void *fileBuffer;
    char *parseCursor;
    const size_t shaderTypeLength = strlen(shaderTypeName + 1);

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (shaderTypeLength >= sizeof(fileName) - R_SHADER_TYPE_PATH_CHARACTER_OVERHEAD) {
        ri.Printf(R_PRINT_WARNING, "WARNING: shader type '%s' is too long\n", shaderTypeName);
        return NULL;
    }

    strcat(fileName, shaderTypeName + 1);
    strcat(fileName, ".stype");
    if (ri.FS_ReadFile(fileName, &fileBuffer) < 0) {
        ri.Printf(R_PRINT_WARNING, "WARNING: could not read shader type file '%s'\n", fileName);
        return NULL;
    }

    parseCursor = (char *)fileBuffer;
    if (ParseShader(&parseCursor, qtrue, imageTrack) == qfalse) {
        ri.FS_FreeFile(fileBuffer);
        ri.Error(ERR_DROP,
                 "\x15"
                 "ERROR: invalid shader type in file '%s'\n",
                 fileName);
        return NULL;
    }

    ri.FS_FreeFile(fileBuffer);
    strcpy(rendererParsedShader.name, shaderTypeName);
    return FinishShader();
}

/* Source: CoDUOMP.exe 0x00502a80..0x00502de6.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00502a80_00502de7.mcode.
 * Name and signature: exact same-module Mac symbol ShaderFromShaderType.
 *
 * Shader-type instances derive their template name from the basename before
 * the final '@'. The use site selects the world/model/2D template directory.
 * After cloning the permanent template into parser scratch storage, generated
 * image placeholders are instantiated from the complete requested name and a
 * world-template lightmap is remapped to the requested lightmap index. */
shader_t *ShaderFromShaderType(const char *name, int32_t lightmapIndex, renderer_image_track_t imageTrack)
{
    char shaderTypeName[MAX_QPATH];
    const char *const typeMarker = strrchr(name, '@');
    const char *lastSeparator = name;
    const char *baseName;
    size_t baseNameLength;
    size_t prefixLength;
    shader_t *shaderType;

    for (const char *cursor = name; *cursor != '\0'; ++cursor) {
        if (*cursor == '/' || *cursor == '\\')
            lastSeparator = cursor;
    }
    if (typeMarker == NULL || lastSeparator > typeMarker) {
        ri.Printf(R_PRINT_WARNING, "WARNING: found '/' or '\\' after '@' in shader '%s'\n", name);
        return NULL;
    }

    baseName = lastSeparator + 1;
    baseNameLength = (size_t)(typeMarker - baseName);
    shaderTypeName[0] = '@';

    if (lightmapIndex == LIGHTMAP_NONE) {
        strcpy(&shaderTypeName[1], "model/");
    } else if (lightmapIndex == LIGHTMAP_2D) {
        strcpy(&shaderTypeName[1], "2d/");
    } else if (lightmapIndex == LIGHTMAP_WHITEIMAGE || lightmapIndex >= 0) {
        strcpy(&shaderTypeName[1], "world/");
    } else {
        shaderTypeName[1] = '\0';
        ri.Error(ERR_DROP,
                 "\x15"
                 "Bad lightmap index %i loading shader %s\n",
                 lightmapIndex, name);
    }

    prefixLength = strlen(shaderTypeName);
    if (prefixLength + baseNameLength >= sizeof(shaderTypeName)) {
        ri.Printf(R_PRINT_WARNING, "WARNING: shader type in '%s' is too long\n", name);
        return NULL;
    }
    memcpy(&shaderTypeName[prefixLength], baseName, baseNameLength);
    shaderTypeName[prefixLength + baseNameLength] = '\0';

    shaderType = R_FindShaderByName(shaderTypeName);
    if (shaderType == tr.defaultShader) {
        shaderType = R_LoadShaderType(shaderTypeName, imageTrack);
        if (shaderType == NULL)
            return NULL;
    }

    rendererParsedShader = *shaderType;
    for (int32_t stageIndex = 0; stageIndex < shaderType->numUnfoggedPasses; ++stageIndex) {
        shaderStage_t *const stage = &rendererParsedShaderStages[stageIndex];
        *stage = *shaderType->stages[stageIndex];

        for (int32_t bundleIndex = 0; bundleIndex < R_MAX_TEXTURE_UNITS; ++bundleIndex) {
            textureBundle_t *const bundle = &stage->bundle[bundleIndex];
            int32_t imageIndex = bundle->numImageAnimations - 1;

            if (imageIndex < 0)
                imageIndex = 0;

            for (;;) {
                image_t *image = bundle->image[imageIndex];

                if (image != NULL && image->imageTrack == R_IMAGE_TRACK_GENERATED_TEXTURE) {
                    image = R_FindImageInstance(name, imageTrack, image->imgName, NULL, tr.delayedImageGroup == 0);
                    bundle->image[imageIndex] = image;
                    if (image == NULL) {
                        ri.Printf(R_PRINT_WARNING,
                                  "WARNING: shadertype '%s' couldn't load "
                                  "image for '%s'\n",
                                  shaderTypeName, name);
                        return NULL;
                    }
                } else if (shaderType->lightmapIndex >= 0 && image == tr.lightmaps[shaderType->lightmapIndex]) {
                    if (lightmapIndex < 0) {
                        const char *usage;

                        if (lightmapIndex == LIGHTMAP_NONE) {
                            usage = "used as a model skin";
                        } else if (lightmapIndex == LIGHTMAP_WHITEIMAGE) {
                            usage = "not used as a world shader";
                        } else if (lightmapIndex == LIGHTMAP_BY_VERTEX) {
                            usage = "used as a vertex-lit surface";
                        } else if (lightmapIndex == LIGHTMAP_2D) {
                            usage = "used as a 2D image";
                        } else {
                            usage = "used some other way";
                        }
                        ri.Printf(R_PRINT_WARNING,
                                  "WARNING: shader type '%s' is a world "
                                  "shader type, but '%s' is %s\n",
                                  shaderTypeName, name, usage);
                        return NULL;
                    }
                    bundle->image[imageIndex] = tr.lightmaps[lightmapIndex];
                }

                --imageIndex;
                if (imageIndex <= 0)
                    break;
            }
        }
    }

    strcpy(rendererParsedShader.name, name);
    rendererParsedShader.lightmapIndex = lightmapIndex;
    return GeneratePermanentShader();
}

/* Source: CoDUOMP.exe 0x00502df0..0x00502f6f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00502df0_00502f70.mcode.
 * Name and signature: exact same-module Mac symbol R_BuildShaderFromImage.
 *
 * Construct the fallback shader stages used when no authored shader text
 * exists. Model, vertex-lit, 2D, white-lightmap, and indexed-lightmap uses
 * receive distinct fixed-function color generation and render state. */
void R_BuildShaderFromImage(int32_t lightmapIndex, image_t *image)
{
    enum {
        R_FALLBACK_SURFACE_MODEL = 0x00000010,
        R_FALLBACK_SURFACE_WORLD = 0x00000030,
        R_FALLBACK_SURFACE_VERTEX_OR_2D = 0x00020050,
        R_FALLBACK_TEXCOORD_COMPONENTS = 2
    };
    shaderStage_t *const firstStage = &rendererParsedShaderStages[0];
    shaderStage_t *const secondStage = &rendererParsedShaderStages[1];
    textureBundle_t *const firstBundle = &firstStage->bundle[0];
    textureBundle_t *const secondBundle = &secondStage->bundle[0];

    if (lightmapIndex == LIGHTMAP_NONE) {
        rendererParsedShader.surfaceFlags = R_FALLBACK_SURFACE_MODEL;
        firstBundle->image[0] = image;
        firstBundle->texCoordComponentCount = R_FALLBACK_TEXCOORD_COMPONENTS;
        firstStage->flags = SHADER_STAGE_ACTIVE;
        firstStage->rgbGen = CGEN_LIGHTING_DIFFUSE;
        firstStage->alphaGen = AGEN_IDENTITY;
        firstStage->stateBits = GLS_LIGHTING | GLS_DEPTHMASK_TRUE;
    } else if (lightmapIndex == LIGHTMAP_BY_VERTEX) {
        rendererParsedShader.surfaceFlags = R_FALLBACK_SURFACE_VERTEX_OR_2D;
        firstBundle->image[0] = image;
        firstBundle->texCoordComponentCount = R_FALLBACK_TEXCOORD_COMPONENTS;
        firstStage->flags = SHADER_STAGE_ACTIVE;
        firstStage->rgbGen = CGEN_EXACT_VERTEX;
        firstStage->alphaGen = AGEN_IDENTITY;
        firstStage->stateBits = GLS_DEPTHMASK_TRUE;
    } else if (lightmapIndex == LIGHTMAP_2D) {
        rendererParsedShader.surfaceFlags = R_FALLBACK_SURFACE_VERTEX_OR_2D;
        firstBundle->image[0] = image;
        firstBundle->texCoordComponentCount = R_FALLBACK_TEXCOORD_COMPONENTS;
        firstStage->flags = SHADER_STAGE_ACTIVE;
        firstStage->rgbGen = CGEN_VERTEX;
        firstStage->alphaGen = AGEN_VERTEX;
        firstStage->stateBits = GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
        rendererParsedShader.cullType = CT_TWO_SIDED;
    } else {
        secondBundle->textureEnvMode = GL_MODULATE;
        secondBundle->image[0] = image;
        rendererParsedShader.surfaceFlags = R_FALLBACK_SURFACE_WORLD;

        if (lightmapIndex == LIGHTMAP_WHITEIMAGE) {
            firstBundle->image[0] = tr.identityLightImage;
            firstStage->rgbGen = CGEN_IDENTITY_LIGHTING;
        } else {
            firstBundle->image[0] = tr.lightmaps[lightmapIndex];
            firstBundle->isLightmap = qtrue;
            firstStage->rgbGen = CGEN_IDENTITY;
        }

        /* 0x00502f23 stores value 1 to only THREE fields: firstStage->alphaGen
         * (0x3880af4), secondStage->flags (0x3880b00) and firstStage->flags
         * (0x3880478). The DLL never writes secondStage->alphaGen (0x0388117c); since
         * R_FindShader memsets the parsed stages to 0 it stays AGEN_UNSPECIFIED. A
         * prior pass added a spurious secondStage->alphaGen = AGEN_IDENTITY here. */
        firstStage->alphaGen = AGEN_IDENTITY;
        firstStage->flags = SHADER_STAGE_ACTIVE;
        secondStage->flags = SHADER_STAGE_ACTIVE;
        firstBundle->texCoordComponentCount = R_FALLBACK_TEXCOORD_COMPONENTS;
        secondBundle->texCoordComponentCount = R_FALLBACK_TEXCOORD_COMPONENTS;
        secondStage->rgbGen = CGEN_IDENTITY;
        secondStage->stateBits |= GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO;
        firstStage->stateBits = GLS_DEPTHMASK_TRUE;
    }

    rendererParsedShader.flags = 0;
    firstBundle->textureEnvMode = GL_MODULATE;
}

/* Source: CoDUOMP.exe 0x00502f70..0x00503288.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00502f70_00503288.mcode.
 * Name and signature: exact same-module Mac symbol R_FindShader.
 *
 * Resolve a shader through the permanent registry, shader-type templates,
 * authored shader text, or the raw-image fallback. The Windows LTCG body
 * inlines R_SyncRenderThread before touching parser scratch storage. */
shader_t *R_FindShader(const char *name, int32_t lightmapIndex, qboolean mipRawImage, int32_t shaderUsage)
{
    char strippedName[MAX_QPATH];
    char imageName[MAX_QPATH];
    shader_t *shader;
    char *definitionText;

    if (name[0] == '\0')
        return tr.defaultShader;

    if (lightmapIndex >= 0 && lightmapIndex >= tr.lightmapCount) {
        lightmapIndex = LIGHTMAP_WHITEIMAGE;
    }

    Com_StripExtension(name, strippedName);
    shader = rendererShaderHashTable[R_GenerateShaderHashValue(strippedName)];
    while (shader != NULL) {
        if ((shader->lightmapIndex == lightmapIndex ||
             (shader->lightmapIndex == LIGHTMAP_NONE && lightmapIndex >= 0 && shaderUsage == R_IMAGE_TRACK_FX)) &&
            Q_stricmpn(shader->name, strippedName, R_SHADER_TOKEN_COMPARE_LIMIT) == 0) {
            UpdateDelayLoadImagesForShader(shader, qfalse);
            return shader;
        }
        shader = shader->next;
    }

    R_SyncRenderThread();
    memset(&rendererParsedShader, 0, sizeof(rendererParsedShader));
    memset(rendererParsedShaderStages, 0, sizeof(rendererParsedShaderStages));
    Q_strncpyz(rendererParsedShader.name, strippedName, sizeof(rendererParsedShader.name));
    rendererParsedShader.lightmapIndex = lightmapIndex;
    if (lightmapIndex == LIGHTMAP_NONE)
        rendererParsedShader.flags = SHADER_FLAG_USE_PICMIP2;

    for (int32_t stageIndex = 0; stageIndex < R_MAX_SHADER_STAGES; ++stageIndex) {
        for (int32_t bundleIndex = 0; bundleIndex < R_MAX_TEXTURE_UNITS; ++bundleIndex) {
            rendererParsedShaderStages[stageIndex].bundle[bundleIndex].texMods = rendererParsedShaderTexMods[stageIndex][bundleIndex];
        }
    }

    if (strrchr(strippedName, '@') != NULL) {
        shader = ShaderFromShaderType(strippedName, lightmapIndex, (renderer_image_track_t)shaderUsage);
        return shader != NULL ? shader : tr.defaultShader;
    }

    definitionText = FindShaderInShaderText(strippedName);
    if (definitionText != NULL) {
        if (r_printShaders->integer != 0)
            ri.Printf(R_PRINT_ALL, "*SHADER* %s\n", name);

        if (ParseShader(&definitionText, qfalse, (renderer_image_track_t)shaderUsage) == qfalse) {
            rendererParsedShader.flags = SHADER_FLAG_DEFAULTED | SHADER_FLAG_EXPLICITLY_DEFINED;
            rendererParsedShader.surfaceFlags = SURF_NOIMPACT;
        }
        return FinishShader();
    }

    /* 0x005031a8 reads 0x04899d54. R_Register stores the return from
     * Cvar_Get("r_graymap", "0", 0x220) into that slot at 0x004c4494.
     * The previous reconstruction mislabeled this access as r_ignore, whose
     * original slot is populated later at 0x004c45cc. */
    if (r_graymap->integer != 0 && lightmapIndex != LIGHTMAP_2D && lightmapIndex != LIGHTMAP_WHITEIMAGE) {
        R_BuildShaderFromImage(lightmapIndex, tr.grayImage);
        return FinishShader();
    }

    uint32_t imageFlags = mipRawImage != qfalse ? IMAGE_FLAG_MIPMAP | IMAGE_FLAG_ALLOW_PICMIP : IMAGE_FLAG_CLAMP_S | IMAGE_FLAG_CLAMP_T;
    if (tr.delayedImageGroup != 0 && lightmapIndex == LIGHTMAP_NONE) {
        imageFlags &= ~IMAGE_FLAG_DELAYED_UPLOAD;
    }

    Q_strncpyz(imageName, name, sizeof(imageName));
    Com_DefaultExtension(imageName, sizeof(imageName), ".tga");
    image_t *const image = R_FindImageFile(imageName, GL_TEXTURE_2D, imageFlags, (renderer_image_track_t)shaderUsage, NULL, 1.0f);
    if (image != NULL) {
        R_BuildShaderFromImage(lightmapIndex, image);
    } else {
        ri.Printf(R_PRINT_DEVELOPER, "Couldn't find image for shader %s\n", name);
        rendererParsedShader.flags |= SHADER_FLAG_DEFAULTED;
    }
    return FinishShader();
}

/* Source: CoDUOMP.exe 0x00503290..0x00503474.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00503290_00503474.mcode.
 * Name inferred from its proven role; no same-module Mac traceback symbol
 * names this retained Windows helper.
 *
 * Register an already-created image as a permanent shader. A defaulted shader
 * with the same name is reused across lightmap modes, matching R_FindShader's
 * failure-cache behavior. */
int32_t R_RegisterShaderFromImage(const char *name, int32_t lightmapIndex, image_t *image)
{
    shader_t *shader = rendererShaderHashTable[R_GenerateShaderHashValue(name)];

    while (shader != NULL) {
        if ((shader->lightmapIndex == lightmapIndex || (shader->flags & SHADER_FLAG_DEFAULTED) != 0) &&
            Q_stricmpn(shader->name, name, R_SHADER_TOKEN_COMPARE_LIMIT) == 0) {
            UpdateDelayLoadImagesForShader(shader, qfalse);
            return shader->index;
        }
        shader = shader->next;
    }

    R_SyncRenderThread();
    memset(&rendererParsedShader, 0, sizeof(rendererParsedShader));
    memset(rendererParsedShaderStages, 0, sizeof(rendererParsedShaderStages));
    Q_strncpyz(rendererParsedShader.name, name, sizeof(rendererParsedShader.name));
    rendererParsedShader.lightmapIndex = lightmapIndex;

    for (int32_t stageIndex = 0; stageIndex < R_MAX_SHADER_STAGES; ++stageIndex) {
        for (int32_t bundleIndex = 0; bundleIndex < R_MAX_TEXTURE_UNITS; ++bundleIndex) {
            rendererParsedShaderStages[stageIndex].bundle[bundleIndex].texMods = rendererParsedShaderTexMods[stageIndex][bundleIndex];
        }
    }

    R_BuildShaderFromImage(lightmapIndex, image);
    return FinishShader()->index;
}

/* Source: CoDUOMP.exe 0x00503480..0x005034c6.
 * Evidence: manually repaired function boundary in
 * coduomp/mcode/CoDUOMP/FUN_00503480_005034c6.mcode.
 * Name inferred from the ordinary source role retained in the Windows image;
 * the helper has no direct call after LTCG inlining and no Mac traceback
 * symbol. */
int32_t RE_RegisterShaderLightMap(const char *name, int32_t lightmapIndex, int32_t shaderUsage)
{
    shader_t *shader;

    if (strlen(name) >= MAX_QPATH) {
        Com_Printf("Shader name exceeds MAX_QPATH\n");
        return 0;
    }

    shader = R_FindShader(name, lightmapIndex, qtrue, shaderUsage);
    if ((shader->flags & SHADER_FLAG_DEFAULTED) != 0)
        return 0;
    return shader->index;
}

/* Source: CoDUOMP.exe 0x005034d0..0x00503524.
 * Evidence: manually repaired function boundary in
 * coduomp/mcode/CoDUOMP/FUN_005034d0_00503524.mcode.
 * Name and signature: same-module Mac symbol RE_RegisterShader and renderer
 * export slot 4. */
int32_t RE_RegisterShader(const char *name, int32_t shaderUsage)
{
    shader_t *shader;

    if (tr.registered == qfalse)
        return 0;
    if (strlen(name) >= MAX_QPATH) {
        Com_Printf("Shader name exceeds MAX_QPATH\n");
        return 0;
    }

    shader = R_FindShader(name, LIGHTMAP_2D, qtrue, shaderUsage);
    if ((shader->flags & SHADER_FLAG_DEFAULTED) != 0)
        return 0;
    return shader->index;
}

/* Source: CoDUOMP.exe 0x00503530..0x00503584.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00503530_00503584.mcode.
 * Name and signature: same-module Mac symbol RE_RegisterShaderNoMip and
 * renderer export slot 5. */
int32_t RE_RegisterShaderNoMip(const char *name, int32_t shaderUsage)
{
    shader_t *shader;

    if (tr.registered == qfalse)
        return 0;
    if (strlen(name) >= MAX_QPATH) {
        Com_Printf("Shader name exceeds MAX_QPATH\n");
        return 0;
    }

    shader = R_FindShader(name, LIGHTMAP_2D, qfalse, shaderUsage);
    if ((shader->flags & SHADER_FLAG_DEFAULTED) != 0)
        return 0;
    return shader->index;
}

/* Source: CoDUOMP.exe 0x00503850..0x00503980.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00503850_00503980.mcode.
 * Name and signature: exact same-module Mac symbol R_MergeShaderList.
 *
 * A one-element compatibility group keeps its original shader. Larger groups
 * clone the first shader under an internal sheet name. Every occurrence of
 * the first logical image in the target is changed to its physical sheet
 * image; the other logical shaders retain their own textureSheet links. */
void R_MergeShaderList(shader_t **shaders, int32_t shaderCount, int32_t mergeIndex)
{
    shader_t *targetShader;
    image_t *const logicalImage = shaders[0]->primaryImage;
    image_t *const textureSheet = logicalImage->link.textureSheet;

    if (shaderCount == 1) {
        targetShader = shaders[0];
    } else {
        CloneShader(shaders[0]);
        Com_sprintf(rendererParsedShader.name, sizeof(rendererParsedShader.name), "*sheet%03i", mergeIndex);
        targetShader = GeneratePermanentShader();
        if (targetShader == tr.defaultShader) {
            ri.Printf(R_PRINT_WARNING, "WARNING: too many shaders after building "
                                       "texture sheets\n");
            return;
        }
    }

    targetShader->flags &= ~SHADER_FLAG_DELAYED_IMAGES;
    for (int32_t stageIndex = 0; stageIndex < targetShader->numUnfoggedPasses; ++stageIndex) {
        shaderStage_t *const stage = targetShader->stages[stageIndex];

        for (int32_t bundleIndex = 0; bundleIndex < R_MAX_TEXTURE_UNITS; ++bundleIndex) {
            textureBundle_t *const bundle = &stage->bundle[bundleIndex];

            if (bundle->image[0] == NULL)
                break;
            /* 0x0050389e CMP images[0],logicalImage; JNE skip; 0x5038a4/a8 store
             * textureSheet into images[0]. A prior pass swapped the compare key and
             * the assigned value (compared textureSheet, assigned logicalImage). */
            if (bundle->image[0] == logicalImage)
                bundle->image[0] = textureSheet;
        }
    }

    if (r_debugOptTex->integer != 0) {
        ri.Printf(R_PRINT_ALL, "merging %i shader(s):\n", shaderCount);
    }
    for (int32_t shaderIndex = 0; shaderIndex < shaderCount; ++shaderIndex) {
        if (r_debugOptTex->integer != 0)
            ri.Printf(R_PRINT_ALL, "-> %s\n", shaders[shaderIndex]->name);

        shaders[shaderIndex]->flags |= SHADER_FLAG_REMAPPED;
        shaders[shaderIndex]->remappedShader = targetShader;
    }
}

/* Source: CoDUOMP.exe 0x00503980..0x005039ca.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00503980_005039ca.mcode.
 * Name and qsort signature: exact same-module Mac symbol
 * compare_mergable_shaders.
 *
 * Images are hunk-allocated as fixed-size records. Their physical sheet
 * address therefore provides the original primary ordering key; shaders on
 * the same sheet are then ordered by their complete merge compatibility. */
int32_t compare_mergable_shaders(const void *left, const void *right)
{
    shader_t *const leftShader = *(shader_t *const *)left;
    shader_t *const rightShader = *(shader_t *const *)right;
    image_t *const leftImage = leftShader->primaryImage;
    image_t *const rightImage = rightShader->primaryImage;
    const ptrdiff_t sheetOrder =
        ((const uint8_t *)leftImage->link.textureSheet - (const uint8_t *)rightImage->link.textureSheet) / (ptrdiff_t)sizeof(image_t);

    if (sheetOrder != 0)
        return (int32_t)sheetOrder;
    return CompareMergableShaders(leftShader, rightShader, leftImage, rightImage);
}

/* Source: CoDUOMP.exe 0x005039d0..0x00503b15.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005039d0_00503b15.mcode.
 * Name and signature: exact same-module Mac symbol
 * R_MergeShadersForImageSheets.
 *
 * Consume the delayed-image marker once, discard shaders whose logical image
 * was not packed, sort the remaining shaders by physical sheet and compatible
 * state, then remap each equal run through one shared shader. */
void R_MergeShadersForImageSheets(void)
{
    shader_t *mergeCandidates[R_MAX_SHADERS];
    int32_t candidateCount = 0;
    int32_t mergeIndex = 0;

    for (int32_t shaderIndex = 0; shaderIndex < tr.numShaders; ++shaderIndex) {
        shader_t *const shader = tr.shaders[shaderIndex];

        if ((shader->flags & SHADER_FLAG_DELAYED_IMAGES) == 0)
            continue;

        shader->flags &= ~SHADER_FLAG_DELAYED_IMAGES;
        if (shader->primaryImage->link.textureSheet == NULL) {
            shader->primaryImage = NULL;
        } else {
            mergeCandidates[candidateCount++] = shader;
        }
    }

    coduo_crt_qsort(mergeCandidates, (size_t)candidateCount, sizeof(mergeCandidates[0]), compare_mergable_shaders);

    for (int32_t first = 0; first < candidateCount;) {
        int32_t end = first + 1;
        shader_t *const firstShader = mergeCandidates[first];
        image_t *const firstImage = firstShader->primaryImage;

        while (end < candidateCount) {
            shader_t *const candidate = mergeCandidates[end];
            image_t *const candidateImage = candidate->primaryImage;
            const ptrdiff_t sheetOrder =
                ((const uint8_t *)firstImage->link.textureSheet - (const uint8_t *)candidateImage->link.textureSheet) /
                (ptrdiff_t)sizeof(image_t);

            if (sheetOrder != 0 || CompareMergableShaders(firstShader, candidate, firstImage, candidateImage) != 0) {
                break;
            }
            ++end;
        }

        R_MergeShaderList(&mergeCandidates[first], end - first, mergeIndex);
        ++mergeIndex;
        first = end;
    }
}

/* Source: CoDUOMP.exe 0x004f7150..0x004f727c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f7150_004f727c.mcode.
 * Name: same-module Mac symbol MergableShader (original spelling).
 *
 * A mergeable shader must reduce to one ordinary texture image. Images using
 * generated coordinates are remembered separately so the chosen primary image
 * can be rejected if it is also consumed through a non-texture coordinate
 * generator. */
qboolean MergableShader(shader_t *shader)
{
    image_t *nonTextureImages[R_MAX_SHADER_STAGES * R_MAX_TEXTURE_UNITS];
    int32_t nonTextureImageCount = 0;

    shader->primaryImage = NULL;
    if ((shader->flags & (SHADER_FLAG_DEFAULTED | SHADER_FLAG_SKY)) != 0)
        return qfalse;

    shader->flags |= SHADER_FLAG_DELAYED_IMAGES;

    for (int32_t stageIndex = 0; stageIndex < shader->numUnfoggedPasses; ++stageIndex) {
        shaderStage_t *stage = shader->stages[stageIndex];

        if (stage->vertexProgram != NULL) {
            shader->primaryImage = NULL;
            return qfalse;
        }

        for (int32_t bundleIndex = 0; bundleIndex < R_MAX_TEXTURE_UNITS; ++bundleIndex) {
            textureBundle_t *bundle = &stage->bundle[bundleIndex];
            image_t *image = bundle->image[0];

            if (image == NULL)
                break;

            if (bundle->tcGen != TCGEN_TEXTURE) {
                nonTextureImages[nonTextureImageCount++] = image;
                continue;
            }

            if (image == tr.whiteImage && bundle->numImageAnimations > 1) {
                continue;
            }

            if (shader->primaryImage != NULL && shader->primaryImage != image) {
                shader->primaryImage = NULL;
                return qfalse;
            }

            /* These branches intentionally do not clear a primary image
             * selected by an earlier bundle; that state distinction is
             * explicit in the original return paths. */
            if (bundle->numTexMods != 0 || bundle->numImageAnimations > 1 || bundle->textureShader != NULL) {
                return qfalse;
            }

            shader->primaryImage = image;
        }
    }

    if (shader->primaryImage == NULL)
        return qfalse;

    for (int32_t imageIndex = 0; imageIndex < nonTextureImageCount; ++imageIndex) {
        if (nonTextureImages[imageIndex] == shader->primaryImage) {
            shader->primaryImage = NULL;
            return qfalse;
        }
    }

    return qtrue;
}

/* Source: CoDUOMP.exe 0x004f7280..0x004f735e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f7280_004f735e.mcode.
 * Name: exact same-module Mac symbol UpdateDelayLoadImagesForShader.
 *
 * Delayed image grouping is useful only while a model group is active and its
 * tile mode remains below r_picmip2. Outside that case the original forces
 * each referenced image to load immediately. */
void UpdateDelayLoadImagesForShader(shader_t *shader, qboolean forceLoad)
{
    if ((shader->flags & SHADER_FLAG_DELAYED_IMAGES) == 0)
        return;

    if (tr.delayedImageGroup == 0 || tr.delayedImageGroupTileMode >= r_picmip2->integer) {
        forceLoad = qtrue;
    }

    shader->flags &= ~SHADER_FLAG_DELAYED_IMAGES;

    for (int32_t stageIndex = 0; stageIndex < shader->numUnfoggedPasses; ++stageIndex) {
        shaderStage_t *stage = shader->stages[stageIndex];

        for (int32_t bundleIndex = 0; bundleIndex < R_MAX_TEXTURE_UNITS; ++bundleIndex) {
            textureBundle_t *bundle = &stage->bundle[bundleIndex];

            if (bundle->image[0] == NULL)
                break;

            /* The machine loop processes images[0] before comparing the
             * incremented index with numImageAnimations. Valid bundle have
             * at least one image; the do/while preserves that exact ordering. */
            int32_t imageIndex = 0;
            do {
                image_t *image = bundle->image[imageIndex];
                R_UpdateDelayLoadImage(image, shader, forceLoad);
                if ((image->flags & IMAGE_FLAG_DELAYED_UPLOAD) != 0)
                    shader->flags |= SHADER_FLAG_DELAYED_IMAGES;
                ++imageIndex;
            } while (imageIndex < bundle->numImageAnimations);
        }
    }
}

/* Source: CoDUOMP.exe 0x004f7360..0x004f73c0.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f7360_004f73c0.mcode.
 * Name: exact same-module Mac symbol CompareTexEnvCombineArgs. */
int32_t CompareTexEnvCombineArgs(const shader_texture_combine_args_t *first, const shader_texture_combine_args_t *second)
{
    if (second->operation != first->operation)
        return (int32_t)(second->operation - first->operation);

    for (int32_t argumentIndex = 0; argumentIndex < 3; ++argumentIndex) {
        if (second->sources[argumentIndex] != first->sources[argumentIndex]) {
            return (int32_t)(second->sources[argumentIndex] - first->sources[argumentIndex]);
        }
    }
    for (int32_t argumentIndex = 0; argumentIndex < 3; ++argumentIndex) {
        if (second->operands[argumentIndex] != first->operands[argumentIndex]) {
            return (int32_t)(second->operands[argumentIndex] - first->operands[argumentIndex]);
        }
    }

    if (second->scale == first->scale)
        return 0;
    return second->scale < first->scale ? -1 : 1;
}

/* Source: CoDUOMP.exe 0x004f73c0..0x004f746c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f73c0_004f746c.mcode.
 * Name: exact same-module Mac symbol CompareTexEnvCombine. The original first
 * compares the typed-pointer distance; all recovered consumers use only its
 * zero/nonzero result. */
int32_t CompareTexEnvCombine(const shader_texture_combine_t *first, const shader_texture_combine_t *second)
{
    if (first == NULL)
        return second == NULL ? 0 : 1;
    if (second == NULL)
        return -1;

    const ptrdiff_t pointerDistance = second - first;
    if (pointerDistance != 0)
        return (int32_t)pointerDistance;

    int32_t comparison = CompareTexEnvCombineArgs(&first->rgb, &second->rgb);
    if (comparison != 0)
        return comparison;
    comparison = CompareTexEnvCombineArgs(&first->alpha, &second->alpha);
    if (comparison != 0)
        return comparison;

    for (int32_t component = 0; component < 4; ++component) {
        if (second->environmentColor[component] == first->environmentColor[component]) {
            continue;
        }
        return second->environmentColor[component] < first->environmentColor[component] ? -1 : 1;
    }
    return 0;
}

/* Source: CoDUOMP.exe 0x004f7470..0x004f74c4.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f7470_004f74c4.mcode.
 * Name: exact same-module Mac symbol CompareNvTexShaders. */
int32_t CompareNvTexShaders(const shader_texture_shader_t *first, const shader_texture_shader_t *second)
{
    if (first == NULL)
        return second == NULL ? 0 : 1;
    if (second == NULL)
        return -1;

    const ptrdiff_t pointerDistance = second - first;
    if (pointerDistance != 0)
        return (int32_t)pointerDistance;
    return memcmp(first, second, sizeof(*first));
}

/* Source: CoDUOMP.exe 0x004f74d0..0x004f7502.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f74d0_004f7502.mcode.
 * Name: exact same-module Mac symbol CompareNvRegisterCombiners. */
int32_t CompareNvRegisterCombiners(const renderer_register_combiners_t *first, const renderer_register_combiners_t *second)
{
    if (first == NULL)
        return second == NULL ? 0 : 1;
    if (second == NULL)
        return -1;

    const ptrdiff_t pointerDistance = second - first;
    if (pointerDistance != 0)
        return (int32_t)pointerDistance;
    return memcmp(first, second, sizeof(*first));
}

/* Source: CoDUOMP.exe 0x004f7510..0x004f758e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f7510_004f758e.mcode.
 * Name: exact same-module Mac symbol CompareWaveForms. */
int32_t CompareWaveForms(const waveForm_t *first, const waveForm_t *second)
{
    if (second->func != first->func)
        return (int32_t)second->func - (int32_t)first->func;

    if (second->base != first->base)
        return second->base < first->base ? -1 : 1;
    if (second->amplitude != first->amplitude)
        return second->amplitude < first->amplitude ? -1 : 1;
    if (second->phase != first->phase)
        return second->phase < first->phase ? -1 : 1;
    if (second->frequency != first->frequency)
        return second->frequency < first->frequency ? -1 : 1;
    return 0;
}

/* Source: CoDUOMP.exe 0x004f7590..0x004f77d6.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f7590_004f77d6.mcode.
 * Name: exact same-module Mac symbol CompareTextureBundles.
 *
 * The designated image pair permits one image substitution at animation frame
 * zero. Every later frame must retain pointer identity. The original returns
 * typed pointer distances for pointer mismatches; its sole consumer tests only
 * zero/nonzero, which is represented portably here by 1. */
int32_t CompareTextureBundles(const textureBundle_t *first, const textureBundle_t *second, const image_t *firstEquivalentImage,
                              const image_t *secondEquivalentImage)
{
    if (first->image[0] != second->image[0]) {
        if (first->image[0] != firstEquivalentImage || second->image[0] != secondEquivalentImage) {
            return 1;
        }
    }

    if (first->image[0] == NULL || second->image[0] == NULL)
        return first->image[0] == second->image[0] ? 0 : 1;

    if (first->numImageAnimations > 1 || second->numImageAnimations > 1) {
        if (first->numImageAnimations != second->numImageAnimations) {
            return first->numImageAnimations - second->numImageAnimations;
        }
        if (first->imageAnimationSpeed != second->imageAnimationSpeed) {
            return first->imageAnimationSpeed < second->imageAnimationSpeed ? -1 : 1;
        }
        for (int32_t imageIndex = 1; imageIndex < first->numImageAnimations; ++imageIndex) {
            if (first->image[imageIndex] != second->image[imageIndex]) {
                return 1;
            }
        }
    }

    if (first->waterMap != second->waterMap)
        return 1;
    if (first->texCoordComponentCount != second->texCoordComponentCount) {
        return first->texCoordComponentCount - second->texCoordComponentCount;
    }
    if (first->textureEnvMode != second->textureEnvMode) {
        return (int32_t)(first->textureEnvMode - second->textureEnvMode);
    }
    if (first->tcGen != second->tcGen)
        return (int32_t)first->tcGen - (int32_t)second->tcGen;

    if (first->tcGen != TCGEN_TEXTURE) {
        if (first->tcGen == TCGEN_VECTOR && memcmp(first->tcGenVectors, second->tcGenVectors, sizeof(first->tcGenVectors)) != 0) {
            return 1;
        }

        if (first->numTexMods != second->numTexMods)
            return first->numTexMods - second->numTexMods;

        for (int32_t texModIndex = 0; texModIndex < first->numTexMods; ++texModIndex) {
            if (memcmp(&first->texMods[texModIndex], &second->texMods[texModIndex], sizeof(first->texMods[texModIndex])) != 0) {
                return 1;
            }
        }

        const int32_t textureShaderComparison = CompareNvTexShaders(second->textureShader, first->textureShader);
        if (textureShaderComparison != 0)
            return textureShaderComparison;
    }

    if (first->videoMapHandle != second->videoMapHandle)
        return first->videoMapHandle - second->videoMapHandle;
    if (first->isLightmap != second->isLightmap)
        return (int32_t)first->isLightmap - (int32_t)second->isLightmap;
    if (first->isVideoMap != second->isVideoMap)
        return (int32_t)first->isVideoMap - (int32_t)second->isVideoMap;
    if (first->clampAnimation != second->clampAnimation)
        return (int32_t)first->clampAnimation - (int32_t)second->clampAnimation;

    return CompareTexEnvCombine(second->textureCombine, first->textureCombine);
}

/* Source: CoDUOMP.exe 0x004f77e0..0x004f7a17.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f77e0_004f7a17.mcode.
 * Name: exact same-module Mac symbol CompareMergableShaders.
 *
 * Only state that affects merge compatibility is compared. In particular,
 * waveform payloads are conditional on their generators, and constant color
 * channels follow the two separate color/alpha gates proven by the switch
 * branches. */
int32_t CompareMergableShaders(const shader_t *left, const shader_t *right, const image_t *leftImage, const image_t *rightImage)
{
    if (left == right)
        return 0;

    if (left->lightmapIndex != right->lightmapIndex)
        return left->lightmapIndex - right->lightmapIndex;
    if (left->flags != right->flags)
        return (int32_t)(left->flags - right->flags);
    if (left->surfaceFlags != right->surfaceFlags)
        return (int32_t)(left->surfaceFlags - right->surfaceFlags);
    if (left->lightingFlags != right->lightingFlags)
        return (int32_t)(left->lightingFlags - right->lightingFlags);
    if (left->sort != right->sort)
        return left->sort < right->sort ? -1 : 1;
    if (left->cullType != right->cullType)
        return (int32_t)left->cullType - (int32_t)right->cullType;
    if (left->numUnfoggedPasses != right->numUnfoggedPasses)
        return left->numUnfoggedPasses - right->numUnfoggedPasses;

    for (int32_t stageIndex = 0; stageIndex < left->numUnfoggedPasses; ++stageIndex) {
        const shaderStage_t *leftStage = left->stages[stageIndex];
        const shaderStage_t *rightStage = right->stages[stageIndex];

        if (leftStage->flags != rightStage->flags)
            return (int32_t)(leftStage->flags - rightStage->flags);

        for (int32_t bundleIndex = 0; bundleIndex < R_MAX_TEXTURE_UNITS; ++bundleIndex) {
            const int32_t bundleComparison =
                CompareTextureBundles(&leftStage->bundle[bundleIndex], &rightStage->bundle[bundleIndex], leftImage, rightImage);
            if (bundleComparison != 0)
                return bundleComparison;
        }

        if (leftStage->fragmentShaderATI != rightStage->fragmentShaderATI) {
            return (int32_t)(leftStage->fragmentShaderATI - rightStage->fragmentShaderATI);
        }

        const int32_t registerCombinerComparison = CompareNvRegisterCombiners(leftStage->registerCombiners, rightStage->registerCombiners);
        if (registerCombinerComparison != 0)
            return registerCombinerComparison;

        if (leftStage->rgbGen != rightStage->rgbGen)
            return (int32_t)leftStage->rgbGen - (int32_t)rightStage->rgbGen;

        if (leftStage->rgbGen == CGEN_WAVEFORM) {
            const int32_t waveformComparison = CompareWaveForms(&rightStage->rgbWave, &leftStage->rgbWave);
            if (waveformComparison != 0)
                return waveformComparison;
        }

        if (leftStage->rgbGen == CGEN_WAVEFORM || leftStage->rgbGen == CGEN_CONSTANT) {
            for (int32_t component = 0; component < 4; ++component) {
                if (leftStage->constantColor[component] != rightStage->constantColor[component]) {
                    return (int32_t)leftStage->constantColor[component] - (int32_t)rightStage->constantColor[component];
                }
            }
        }

        if (leftStage->alphaGen != rightStage->alphaGen)
            return (int32_t)leftStage->alphaGen - (int32_t)rightStage->alphaGen;

        if (leftStage->alphaGen == AGEN_WAVEFORM) {
            const int32_t waveformComparison = CompareWaveForms(&rightStage->alphaWave, &leftStage->alphaWave);
            if (waveformComparison != 0)
                return waveformComparison;
        }

        if ((leftStage->alphaGen == AGEN_WAVEFORM || leftStage->alphaGen == AGEN_CONSTANT) &&
            leftStage->constantColor[3] != rightStage->constantColor[3]) {
            return (int32_t)leftStage->constantColor[3] - (int32_t)rightStage->constantColor[3];
        }

        if (leftStage->stateBits != rightStage->stateBits)
            return (int32_t)(leftStage->stateBits - rightStage->stateBits);
    }

    return 0;
}

/* Source: CoDUOMP.exe 0x004f7aa0..0x004f7b0d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f7aa0_004f7b0d.mcode.
 * Name: exact same-module Mac symbol MatchShaderToken. */
qboolean MatchShaderToken(char **text, const char *expected, const char *functionName)
{
    const char *token = Com_Parse(text);
    if (strcmp(token, expected) == 0)
        return qtrue;

    ri.Printf(R_PRINT_WARNING, "WARNING: %s missing '%s', found '%s' instead in shader '%s'\n", functionName, expected, token,
              rendererParsedShader.name);
    return qfalse;
}

/* Source: CoDUOMP.exe 0x004f7a20..0x004f7a9a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f7a20_004f7a9a.mcode.
 * Name: exact same-module Mac symbol MatchShaderTokenOnLine. */
qboolean MatchShaderTokenOnLine(char **text, const char *expected, const char *functionName)
{
    const char *token = Com_ParseOnLine(text);
    if (strcmp(token, expected) == 0)
        return qtrue;

    ri.Printf(R_PRINT_WARNING, "WARNING: %s missing '%s', found '%s' instead in shader '%s'\n", functionName, expected, token,
              rendererParsedShader.name);
    return qfalse;
}

/* Source: CoDUOMP.exe 0x004f7b10..0x004f7c66.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f7b10_004f7c66.mcode.
 * Name and ordinary three-argument signature: exact same-module Mac symbol
 * ParseVector. Every element, including both parentheses, must remain on the
 * current line. */
qboolean ParseVector(char **text, int32_t componentCount, float *values)
{
    const char *token = Com_ParseOnLine(text);
    if (strcmp(token, "(") != 0) {
        ri.Printf(R_PRINT_WARNING, "WARNING: missing parenthesis in shader '%s'\n", rendererParsedShader.name);
        return qfalse;
    }

    for (int32_t component = 0; component < componentCount; ++component) {
        token = Com_ParseOnLine(text);
        if (token[0] == '\0') {
            ri.Printf(R_PRINT_WARNING, "WARNING: missing vector element in shader '%s'\n", rendererParsedShader.name);
            return qfalse;
        }
        values[component] = (float)atof(token);
    }

    token = Com_ParseOnLine(text);
    if (strcmp(token, ")") != 0) {
        ri.Printf(R_PRINT_WARNING, "WARNING: missing parenthesis in shader '%s'\n", rendererParsedShader.name);
        return qfalse;
    }

    return qtrue;
}

/* Source: CoDUOMP.exe 0x004f7c70..0x004f7cde.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f7c70_004f7cde.mcode.
 * Name: exact same-module Mac symbol NameToAFunc. */
uint32_t NameToAFunc(const char *name)
{
    if (Q_stricmp(name, "GT0") == 0)
        return GLS_ATEST_GT_0;
    if (Q_stricmp(name, "LT128") == 0)
        return GLS_ATEST_LT_128;
    if (Q_stricmp(name, "GE128") == 0)
        return GLS_ATEST_GE_128;

    ri.Printf(R_PRINT_WARNING, "WARNING: invalid alphaFunc name '%s' in shader '%s'\n", name, rendererParsedShader.name);
    return 0;
}

/* Source: CoDUOMP.exe 0x004f7ce0..0x004f7de1.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f7ce0_004f7de1.mcode.
 * Name: exact same-module Mac symbol NameToSrcBlendMode. */
uint32_t NameToSrcBlendMode(const char *name)
{
    if (Q_stricmp(name, "GL_ONE") == 0)
        return GLS_SRCBLEND_ONE;
    if (Q_stricmp(name, "GL_ZERO") == 0)
        return GLS_SRCBLEND_ZERO;
    if (Q_stricmp(name, "GL_DST_COLOR") == 0)
        return GLS_SRCBLEND_DST_COLOR;
    if (Q_stricmp(name, "GL_ONE_MINUS_DST_COLOR") == 0)
        return GLS_SRCBLEND_ONE_MINUS_DST_COLOR;
    if (Q_stricmp(name, "GL_SRC_ALPHA") == 0)
        return GLS_SRCBLEND_SRC_ALPHA;
    if (Q_stricmp(name, "GL_ONE_MINUS_SRC_ALPHA") == 0)
        return GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA;
    if (Q_stricmp(name, "GL_DST_ALPHA") == 0)
        return GLS_SRCBLEND_DST_ALPHA;
    if (Q_stricmp(name, "GL_ONE_MINUS_DST_ALPHA") == 0)
        return GLS_SRCBLEND_ONE_MINUS_DST_ALPHA;
    if (Q_stricmp(name, "GL_SRC_ALPHA_SATURATE") == 0)
        return GLS_SRCBLEND_ALPHA_SATURATE;

    ri.Printf(R_PRINT_WARNING,
              "WARNING: unknown blend mode '%s' in shader '%s', "
              "substituting GL_ONE\n",
              name, rendererParsedShader.name);
    return GLS_SRCBLEND_ONE;
}

/* Source: CoDUOMP.exe 0x004f7df0..0x004f7edb.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f7df0_004f7edb.mcode.
 * Name: exact same-module Mac symbol NameToDstBlendMode. */
uint32_t NameToDstBlendMode(const char *name)
{
    if (Q_stricmp(name, "GL_ONE") == 0)
        return GLS_DSTBLEND_ONE;
    if (Q_stricmp(name, "GL_ZERO") == 0)
        return GLS_DSTBLEND_ZERO;
    if (Q_stricmp(name, "GL_SRC_ALPHA") == 0)
        return GLS_DSTBLEND_SRC_ALPHA;
    if (Q_stricmp(name, "GL_ONE_MINUS_SRC_ALPHA") == 0)
        return GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
    if (Q_stricmp(name, "GL_DST_ALPHA") == 0)
        return GLS_DSTBLEND_DST_ALPHA;
    if (Q_stricmp(name, "GL_ONE_MINUS_DST_ALPHA") == 0)
        return GLS_DSTBLEND_ONE_MINUS_DST_ALPHA;
    if (Q_stricmp(name, "GL_SRC_COLOR") == 0)
        return GLS_DSTBLEND_SRC_COLOR;
    if (Q_stricmp(name, "GL_ONE_MINUS_SRC_COLOR") == 0)
        return GLS_DSTBLEND_ONE_MINUS_SRC_COLOR;

    ri.Printf(R_PRINT_WARNING,
              "WARNING: unknown blend mode '%s' in shader '%s', "
              "substituting GL_ONE\n",
              name, rendererParsedShader.name);
    return GLS_DSTBLEND_ONE;
}

/* Source: CoDUOMP.exe 0x004f7ee0..0x004f7f9f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f7ee0_004f7f9f.mcode.
 * Name: exact same-module Mac symbol NameToGenFunc. */
shader_wave_func_t NameToGenFunc(const char *name)
{
    if (Q_stricmp(name, "sin") == 0)
        return SHADER_WAVE_SIN;
    if (Q_stricmp(name, "square") == 0)
        return SHADER_WAVE_SQUARE;
    if (Q_stricmp(name, "triangle") == 0)
        return SHADER_WAVE_TRIANGLE;
    if (Q_stricmp(name, "sawtooth") == 0)
        return SHADER_WAVE_SAWTOOTH;
    if (Q_stricmp(name, "inversesawtooth") == 0)
        return SHADER_WAVE_INVERSE_SAWTOOTH;
    if (Q_stricmp(name, "noise") == 0)
        return SHADER_WAVE_NOISE;

    ri.Printf(R_PRINT_WARNING, "WARNING: invalid genfunc name '%s' in shader '%s'\n", name, rendererParsedShader.name);
    return SHADER_WAVE_SIN;
}

/* Source: CoDUOMP.exe 0x004f7fa0..0x004f8069.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f7fa0_004f8069.mcode.
 * Name: exact same-module Mac symbol ParseWaveForm. */
void ParseWaveForm(char **text, waveForm_t *waveform)
{
    const char *token = Com_ParseOnLine(text);
    if (token[0] == '\0')
        goto missing_parameter;
    waveform->func = NameToGenFunc(token);

    token = Com_ParseOnLine(text);
    if (token[0] == '\0')
        goto missing_parameter;
    waveform->base = (float)atof(token);

    token = Com_ParseOnLine(text);
    if (token[0] == '\0')
        goto missing_parameter;
    waveform->amplitude = (float)atof(token);

    token = Com_ParseOnLine(text);
    if (token[0] == '\0')
        goto missing_parameter;
    waveform->phase = (float)atof(token);

    token = Com_ParseOnLine(text);
    if (token[0] == '\0')
        goto missing_parameter;
    waveform->frequency = (float)atof(token);
    return;

missing_parameter:
    ri.Printf(R_PRINT_WARNING, "WARNING: missing waveform parm in shader '%s'\n", rendererParsedShader.name);
}

/* Source: CoDUOMP.exe 0x004f8070..0x004f8099.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f8070_004f8099.mcode.
 * Name: exact same-module Mac symbol MaxWaveFormDeformation. */
float MaxWaveFormDeformation(const waveForm_t *waveform)
{
    const float negativePeak = fabsf(waveform->base - waveform->amplitude);
    const float positivePeak = fabsf(waveform->base + waveform->amplitude);
    return negativePeak > positivePeak ? negativePeak : positivePeak;
}

/* Source: CoDUOMP.exe 0x005004b0..0x00500ab2.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005004b0_00500ab2.mcode.
 * Name and signature: exact same-module Mac symbol ParseDeform.
 *
 * Failed parameter parses deliberately retain any requirement flags and
 * partially written fields established before the failure. ParseWaveForm also
 * reports its own missing parameter without returning a status, after which
 * this function still installs the deformation, matching the original path. */
void ParseDeform(char **text)
{
    const char *token = Com_ParseOnLine(text);
    deformStage_t *deform;

    if (token[0] == '\0') {
        ri.Printf(R_PRINT_WARNING, "WARNING: missing deform parm in shader '%s'\n", rendererParsedShader.name);
        return;
    }

    if (rendererParsedShader.numDeforms == R_MAX_SHADER_DEFORMS) {
        ri.Printf(R_PRINT_WARNING, "WARNING: MAX_SHADER_DEFORMS in '%s'\n", rendererParsedShader.name);
        return;
    }

    deform = &rendererParsedShader.deforms[rendererParsedShader.numDeforms];

    if (Q_stricmpn(token, "projectionShadow", R_SHADER_TOKEN_COMPARE_LIMIT) == 0) {
        deform->deformation = DEFORM_PROJECTION_SHADOW;
    } else if (Q_stricmpn(token, "autosprite", R_SHADER_TOKEN_COMPARE_LIMIT) == 0) {
        deform->deformation = DEFORM_AUTOSPRITE;
    } else if (Q_stricmpn(token, "autosprite2", R_SHADER_TOKEN_COMPARE_LIMIT) == 0) {
        deform->deformation = DEFORM_AUTOSPRITE2;
    } else if (Q_stricmpn(token, "text", 4) == 0) {
        int32_t textIndex = (int32_t)(int8_t)(uint8_t)token[4] - '0';
        if (textIndex < 0 || textIndex > 7)
            textIndex = 0;
        deform->deformation = (shader_deform_type_t)(DEFORM_TEXT0 + textIndex);
    } else if (Q_stricmp(token, "bulge") == 0) {
        rendererParsedShader.surfaceFlags |=
            SHADER_SURFACE_TIME_DEPENDENT | SHADER_SURFACE_REQUIRES_VERTEX_BASIS | SHADER_SURFACE_BASE_TEXCOORDS;

        token = Com_ParseOnLine(text);
        if (token[0] == '\0')
            goto missing_bulge_parameter;
        deform->bulgeWidth = (float)atof(token);

        token = Com_ParseOnLine(text);
        if (token[0] == '\0')
            goto missing_bulge_parameter;
        deform->bulgeHeight = (float)atof(token);

        token = Com_ParseOnLine(text);
        if (token[0] == '\0')
            goto missing_bulge_parameter;
        deform->bulgeSpeed = (float)atof(token);

        rendererParsedShader.boundsExpansion += fabsf(deform->bulgeHeight);
        deform->deformation = DEFORM_BULGE;
    } else if (Q_stricmp(token, "wave") == 0) {
        rendererParsedShader.surfaceFlags |= SHADER_SURFACE_TIME_DEPENDENT | SHADER_SURFACE_REQUIRES_VERTEX_BASIS;

        token = Com_ParseOnLine(text);
        if (token[0] == '\0')
            goto missing_deform_parameter;
        if (atof(token) != 0.0) {
            deform->deformationSpread = (float)(1.0f / atof(token));
        } else {
            deform->deformationSpread = 100.0f;
            ri.Printf(R_PRINT_WARNING,
                      "WARNING: illegal div value of 0 in deformVertexes command "
                      "for shader '%s'\n",
                      rendererParsedShader.name);
        }

        ParseWaveForm(text, &deform->deformationWave);
        rendererParsedShader.boundsExpansion += MaxWaveFormDeformation(&deform->deformationWave);
        deform->deformation = DEFORM_WAVE;
    } else if (Q_stricmp(token, "flap") == 0) {
        rendererParsedShader.surfaceFlags |=
            SHADER_SURFACE_TIME_DEPENDENT | SHADER_SURFACE_REQUIRES_VERTEX_BASIS | SHADER_SURFACE_BASE_TEXCOORDS;

        token = Com_ParseOnLine(text);
        if (token[0] == '\0') {
            ri.Printf(R_PRINT_WARNING, "WARNING: missing flap axis in shader '%s'\n", rendererParsedShader.name);
            return;
        }

        if (Q_stricmp(token, "s") == 0 || Q_stricmp(token, "x") == 0) {
            deform->deformation = DEFORM_FLAP_S;
        } else if (Q_stricmp(token, "t") == 0 || Q_stricmp(token, "y") == 0) {
            deform->deformation = DEFORM_FLAP_T;
        } else {
            ri.Printf(R_PRINT_WARNING,
                      "WARNING: flap axis must be one of (s, t, x, y) "
                      "in shader '%s'\n",
                      rendererParsedShader.name);
            return;
        }

        token = Com_ParseOnLine(text);
        if (token[0] == '\0')
            goto missing_deform_parameter;
        if (atof(token) != 0.0) {
            deform->deformationSpread = (float)(1.0f / atof(token));
        } else {
            deform->deformationSpread = 100.0f;
            ri.Printf(R_PRINT_WARNING,
                      "WARNING: illegal div value of 0 in deformVertexes command "
                      "for shader '%s'\n",
                      rendererParsedShader.name);
        }

        ParseWaveForm(text, &deform->deformationWave);
        rendererParsedShader.boundsExpansion += MaxWaveFormDeformation(&deform->deformationWave);
    } else if (Q_stricmp(token, "normal") == 0) {
        rendererParsedShader.surfaceFlags |= SHADER_SURFACE_TIME_DEPENDENT | SHADER_SURFACE_REQUIRES_VERTEX_BASIS;

        token = Com_ParseOnLine(text);
        if (token[0] == '\0')
            goto missing_deform_parameter;
        deform->deformationWave.amplitude = (float)atof(token);

        token = Com_ParseOnLine(text);
        if (token[0] == '\0')
            goto missing_deform_parameter;
        deform->deformationWave.frequency = (float)atof(token);
        deform->deformation = DEFORM_NORMALS;
    } else if (Q_stricmp(token, "syncnormal") == 0) {
        rendererParsedShader.surfaceFlags |= SHADER_SURFACE_TIME_DEPENDENT | SHADER_SURFACE_REQUIRES_VERTEX_BASIS;

        if (rendererParsedShader.numDeforms == 0 ||
            rendererParsedShader.deforms[rendererParsedShader.numDeforms - 1].deformation != DEFORM_WAVE) {
            ri.Printf(R_PRINT_WARNING,
                      "WARNING: deformVertexes syncNormal must follow "
                      "deformVertexes wave in shader '%s'\n",
                      rendererParsedShader.name);
            return;
        }

        *deform = rendererParsedShader.deforms[rendererParsedShader.numDeforms - 1];
        token = Com_ParseOnLine(text);
        if (token[0] == '\0') {
            ri.Printf(R_PRINT_WARNING, "WARNING: missing scale to syncNormal in shader '%s'\n", rendererParsedShader.name);
            return;
        }
        deform->deformationWave.amplitude *= (float)atof(token);
        deform->deformation = DEFORM_SYNC_NORMALS;
    } else if (Q_stricmp(token, "move") == 0) {
        rendererParsedShader.surfaceFlags |= SHADER_SURFACE_TIME_DEPENDENT;

        for (int32_t component = 0; component < 3; ++component) {
            token = Com_ParseOnLine(text);
            if (token[0] == '\0')
                goto missing_deform_parameter;
            deform->moveVector[component] = (float)atof(token);
        }

        ParseWaveForm(text, &deform->deformationWave);
        const float maximumDeformation = MaxWaveFormDeformation(&deform->deformationWave);
        const long double moveLengthRaw = sqrtl(((long double)deform->moveVector[0] * (long double)deform->moveVector[0] +
                                                 (long double)deform->moveVector[1] * (long double)deform->moveVector[1]) +
                                                (long double)deform->moveVector[2] * (long double)deform->moveVector[2]);
        const float moveLength = (float)moveLengthRaw;

        /* 0x00500a68 stores the vector length to a float spill while the
         * bounds expansion multiplies the retained square root. */
        (void)moveLength;
        rendererParsedShader.boundsExpansion =
            (float)((long double)maximumDeformation * moveLengthRaw + (long double)rendererParsedShader.boundsExpansion);
        deform->deformation = DEFORM_MOVE;
    } else {
        ri.Printf(R_PRINT_WARNING,
                  "WARNING: unknown deformVertexes subtype '%s' found "
                  "in shader '%s'\n",
                  token, rendererParsedShader.name);
        return;
    }

    ++rendererParsedShader.numDeforms;
    return;

missing_bulge_parameter:
    ri.Printf(R_PRINT_WARNING, "WARNING: missing deformVertexes bulge parm in shader '%s'\n", rendererParsedShader.name);
    return;

missing_deform_parameter:
    ri.Printf(R_PRINT_WARNING, "WARNING: missing deformVertexes parm in shader '%s'\n", rendererParsedShader.name);
}

/* Original pointer table at 0x005ce9e4. Its order is the image-slot order in
 * shader_t; backend sky drawing applies its separate face-order permutation.
 * PE_RELOCATION_VALUES_VERIFIED: all six suffix pointers match the PE. */
static const char *const rendererSkyImageSuffixes[R_SKYBOX_FACE_COUNT] = {"rt", "bk", "lf", "ft", "up", "dn"};

/* Source: CoDUOMP.exe 0x00500ac0..0x00500d16.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00500ac0_00500d16.mcode.
 * Name and signature: same-module Mac symbol ParseSkyParms, with the second
 * argument proved by its direct pass-through to R_FindImageFile.
 *
 * A missing token aborts without installing SHADER_FLAG_SKY, even when the
 * outer images and cloud height were already written. Missing cube faces use
 * the renderer's default image exactly as the original does. */
void ParseSkyParms(char **text, renderer_image_track_t imageTrack)
{
    const uint32_t imageFlags = IMAGE_FLAG_MIPMAP | IMAGE_FLAG_ALLOW_PICMIP | IMAGE_FLAG_CLAMP_S | IMAGE_FLAG_CLAMP_T;
    char imageName[MAX_QPATH];
    const char *token = Com_ParseOnLine(text);

    if (token[0] == '\0')
        goto missing_parameter;

    if (strcmp(token, "-") != 0) {
        for (int32_t side = 0; side < R_SKYBOX_FACE_COUNT; ++side) {
            Com_sprintf(imageName, sizeof(imageName), "%s_%s.tga", token, rendererSkyImageSuffixes[side]);
            rendererParsedShader.skyOuterBox[side] = R_FindImageFile(imageName, GL_TEXTURE_2D, imageFlags, imageTrack, NULL, 1.0f);
            if (rendererParsedShader.skyOuterBox[side] == NULL)
                rendererParsedShader.skyOuterBox[side] = tr.defaultImage;
        }
    }

    token = Com_ParseOnLine(text);
    if (token[0] == '\0')
        goto missing_parameter;

    rendererParsedShader.skyCloudHeight = (float)atof(token);
    if (rendererParsedShader.skyCloudHeight == 0.0f)
        rendererParsedShader.skyCloudHeight = 512.0f;
    R_InitSkyTexCoords(rendererParsedShader.skyCloudHeight);

    token = Com_ParseOnLine(text);
    if (token[0] == '\0')
        goto missing_parameter;

    if (strcmp(token, "-") != 0) {
        for (int32_t side = 0; side < R_SKYBOX_FACE_COUNT; ++side) {
            Com_sprintf(imageName, sizeof(imageName), "%s_%s.tga", token, rendererSkyImageSuffixes[side]);
            rendererParsedShader.skyInnerBox[side] = R_FindImageFile(imageName, GL_TEXTURE_2D, imageFlags, imageTrack, NULL, 1.0f);
            if (rendererParsedShader.skyInnerBox[side] == NULL)
                rendererParsedShader.skyInnerBox[side] = tr.defaultImage;
        }
    }

    rendererParsedShader.flags |= SHADER_FLAG_SKY;
    return;

missing_parameter:
    ri.Printf(R_PRINT_WARNING, "WARNING: 'skyParms' missing parameter in shader '%s'\n", rendererParsedShader.name);
}

/* Source: CoDUOMP.exe 0x00500d20..0x00501051.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00500d20_00501051.mcode.
 * Name and signature: exact same-module Mac symbol ParseSort.
 *
 * Numeric zero is accepted only when its token begins with '0'. This preserves
 * the original rejection of ordinary nonnumeric tokens after atof returns
 * zero, while retaining spellings such as "0.0". */
qboolean ParseSort(char **text)
{
    const char *token = Com_ParseOnLine(text);

    if (token[0] == '\0') {
        ri.Printf(R_PRINT_WARNING, "WARNING: missing sort parameter in shader '%s'\n", rendererParsedShader.name);
        return qfalse;
    }

    if (Q_stricmpn(token, "portal", R_SHADER_TOKEN_COMPARE_LIMIT) == 0) {
        rendererParsedShader.sort = SHADER_SORT_PORTAL;
    } else if (Q_stricmpn(token, "sky", R_SHADER_TOKEN_COMPARE_LIMIT) == 0) {
        rendererParsedShader.sort = SHADER_SORT_SKY;
    } else if (Q_stricmpn(token, "ocean", R_SHADER_TOKEN_COMPARE_LIMIT) == 0) {
        rendererParsedShader.sort = SHADER_SORT_OCEAN;
    } else if (Q_stricmpn(token, "boathull", R_SHADER_TOKEN_COMPARE_LIMIT) == 0) {
        rendererParsedShader.sort = SHADER_SORT_BOAT_HULL;
    } else if (Q_stricmp(token, "opaque") == 0) {
        rendererParsedShader.sort = SHADER_SORT_OPAQUE;
    } else if (Q_stricmp(token, "decal") == 0) {
        rendererParsedShader.sort = SHADER_SORT_DECAL;
    } else if (Q_stricmp(token, "seeThrough") == 0) {
        rendererParsedShader.sort = SHADER_SORT_SEE_THROUGH;
    } else if (Q_stricmp(token, "banner") == 0) {
        rendererParsedShader.sort = SHADER_SORT_BANNER;
    } else if (Q_stricmp(token, "underwater") == 0) {
        rendererParsedShader.sort = SHADER_SORT_UNDERWATER;
    } else if (Q_stricmp(token, "water") == 0) {
        rendererParsedShader.sort = SHADER_SORT_WATER;
    } else if (Q_stricmp(token, "corona") == 0) {
        rendererParsedShader.sort = SHADER_SORT_CORONA;
    } else if (Q_stricmp(token, "innerBlend") == 0) {
        rendererParsedShader.sort = SHADER_SORT_INNER_BLEND;
    } else if (Q_stricmp(token, "outerBlend") == 0) {
        rendererParsedShader.sort = SHADER_SORT_OUTER_BLEND;
    } else if (Q_stricmp(token, "blend") == 0) {
        rendererParsedShader.sort = SHADER_SORT_BLEND;
    } else if (Q_stricmp(token, "blend2") == 0) {
        rendererParsedShader.sort = SHADER_SORT_BLEND_2;
    } else if (Q_stricmp(token, "blend3") == 0) {
        rendererParsedShader.sort = SHADER_SORT_BLEND_3;
    } else if (Q_stricmp(token, "blend4") == 0) {
        rendererParsedShader.sort = SHADER_SORT_BLEND_4;
    } else if (Q_stricmp(token, "additive") == 0) {
        rendererParsedShader.sort = SHADER_SORT_ADDITIVE;
    } else if (Q_stricmp(token, "nearest") == 0) {
        rendererParsedShader.sort = SHADER_SORT_NEAREST;
    } else {
        rendererParsedShader.sort = (float)atof(token);
        if (rendererParsedShader.sort == 0.0f && token[0] != '0') {
            ri.Printf(R_PRINT_WARNING, "WARNING: bad shader sort '%s'\n", token);
            return qfalse;
        }
    }

    return qtrue;
}

/* Source: CoDUOMP.exe 0x00501060..0x005010fa.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00501060_005010fa.mcode.
 * Name and signature: exact same-module Mac symbol ParseSurfaceParm.
 *
 * The original loop begins at the material surface-type rows and continues
 * through every general surface parm to the shared null sentinel. Only the
 * row's surfaceFlags word is applied here. RE_PickShader separately consumes
 * contents; no CoDUOMP.exe consumer reads clearSolid. Unknown names are
 * deliberately ignored. */
void ParseSurfaceParm(char **text)
{
    const char *token = Com_ParseOnLine(text);

    for (const surfaceParm_t *parm = surfaceParms; parm->name != NULL; ++parm) {
        if (Q_stricmpn(token, parm->name, R_SHADER_TOKEN_COMPARE_LIMIT) == 0) {
            rendererParsedShader.surfaceParmFlags |= parm->surfaceFlags;
            return;
        }
    }
}

/* Source: CoDUOMP.exe 0x004f80a0..0x004f85f3.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f80a0_004f85f3.mcode.
 * Name: exact same-module Mac symbol ParseTexMod.
 *
 * The Windows register allocation receives stage in ECX and bundleIndex in
 * EAX, so its stage-relative accesses are four bytes beyond the corresponding
 * textureBundle_t offsets. Parameters already parsed before a missing
 * later parameter remain written, while the modifier type and count change
 * only after a complete recognized mode. An unknown mode still consumes one
 * of the four modifier slots, exactly as the original does. */
void ParseTexMod(shaderStage_t *stage, int32_t bundleIndex, char **text)
{
    textureBundle_t *bundle = &stage->bundle[bundleIndex];
    texModInfo_t *texMod;
    const char *token;

    if (bundle->numTexMods == R_MAX_TEXTURE_MODIFIERS) {
        ri.Error(ERR_DROP,
                 "\x15"
                 "ERROR: too many tcMod stages in shader '%s'\n",
                 rendererParsedShader.name);
    }

    texMod = &bundle->texMods[bundle->numTexMods];
    token = Com_ParseOnLine(text);

    if (bundle->image[0] == NULL) {
        ri.Error(ERR_DROP,
                 "\x15"
                 "ERROR: tcMod stage before texture in shader '%s'\n",
                 rendererParsedShader.name);
    }

    if (bundle->image[0]->target == GL_TEXTURE_CUBE_MAP_ARB) {
        if (Q_stricmpn(token, "negate", R_SHADER_TOKEN_COMPARE_LIMIT) == 0 ||
            Q_stricmpn(token, "reverse", R_SHADER_TOKEN_COMPARE_LIMIT) == 0) {
            texMod->type = TMOD_CUBEMAP_NEGATE;
            ++bundle->numTexMods;
            return;
        }
        if (Q_stricmp(token, "bumpmapFrame") == 0) {
            texMod->type = TMOD_CUBEMAP_BUMPMAP_FRAME;
            rendererParsedShader.surfaceFlags |= SHADER_SURFACE_REQUIRES_VERTEX_BASIS;
            ++bundle->numTexMods;
            return;
        }
    }

    if (Q_stricmpn(token, "swap", R_SHADER_TOKEN_COMPARE_LIMIT) == 0) {
        texMod->type = TMOD_SWAP;
        ++bundle->numTexMods;
        return;
    }

    if (Q_stricmpn(token, "turb", R_SHADER_TOKEN_COMPARE_LIMIT) == 0) {
        token = Com_ParseOnLine(text);
        if (token[0] == '\0') {
            ri.Printf(R_PRINT_WARNING, "WARNING: missing tcMod turb in shader '%s'\n", rendererParsedShader.name);
            return;
        }
        texMod->wave.base = (float)atof(token);

        token = Com_ParseOnLine(text);
        if (token[0] == '\0')
            goto missing_turb_parameters;
        texMod->wave.amplitude = (float)atof(token);

        token = Com_ParseOnLine(text);
        if (token[0] == '\0')
            goto missing_turb_parameters;
        texMod->wave.phase = (float)atof(token);

        token = Com_ParseOnLine(text);
        if (token[0] == '\0')
            goto missing_turb_parameters;
        texMod->wave.frequency = (float)atof(token);

        texMod->type = TMOD_TURBULENT;
        rendererParsedShader.surfaceFlags |= SHADER_SURFACE_TIME_DEPENDENT;
        ++bundle->numTexMods;
        return;
    }

    if (Q_stricmp(token, "scale") == 0) {
        token = Com_ParseOnLine(text);
        if (token[0] == '\0')
            goto missing_scale_parameters;
        texMod->scale[0] = (float)atof(token);

        token = Com_ParseOnLine(text);
        if (token[0] == '\0')
            goto missing_scale_parameters;
        texMod->scale[1] = (float)atof(token);

        texMod->type = TMOD_SCALE;
        ++bundle->numTexMods;
        return;
    }

    if (Q_stricmp(token, "scroll") == 0) {
        token = Com_ParseOnLine(text);
        if (token[0] == '\0')
            goto missing_scroll_parameters;
        texMod->scroll[0] = (float)atof(token);

        token = Com_ParseOnLine(text);
        if (token[0] == '\0')
            goto missing_scroll_parameters;
        texMod->scroll[1] = (float)atof(token);

        texMod->type = TMOD_SCROLL;
        rendererParsedShader.surfaceFlags |= SHADER_SURFACE_TIME_DEPENDENT;
        ++bundle->numTexMods;
        return;
    }

    if (Q_stricmp(token, "stretch") == 0) {
        token = Com_ParseOnLine(text);
        if (token[0] == '\0')
            goto missing_stretch_parameters;
        texMod->wave.func = NameToGenFunc(token);

        token = Com_ParseOnLine(text);
        if (token[0] == '\0')
            goto missing_stretch_parameters;
        texMod->wave.base = (float)atof(token);

        token = Com_ParseOnLine(text);
        if (token[0] == '\0')
            goto missing_stretch_parameters;
        texMod->wave.amplitude = (float)atof(token);

        token = Com_ParseOnLine(text);
        if (token[0] == '\0')
            goto missing_stretch_parameters;
        texMod->wave.phase = (float)atof(token);

        token = Com_ParseOnLine(text);
        if (token[0] == '\0')
            goto missing_stretch_parameters;
        texMod->wave.frequency = (float)atof(token);

        texMod->type = TMOD_STRETCH;
        rendererParsedShader.surfaceFlags |= SHADER_SURFACE_TIME_DEPENDENT;
        ++bundle->numTexMods;
        return;
    }

    if (Q_stricmp(token, "transform") == 0) {
        token = Com_ParseOnLine(text);
        if (token[0] == '\0')
            goto missing_transform_parameters;
        texMod->matrix[0][0] = (float)atof(token);

        token = Com_ParseOnLine(text);
        if (token[0] == '\0')
            goto missing_transform_parameters;
        texMod->matrix[0][1] = (float)atof(token);

        token = Com_ParseOnLine(text);
        if (token[0] == '\0')
            goto missing_transform_parameters;
        texMod->matrix[1][0] = (float)atof(token);

        token = Com_ParseOnLine(text);
        if (token[0] == '\0')
            goto missing_transform_parameters;
        texMod->matrix[1][1] = (float)atof(token);

        token = Com_ParseOnLine(text);
        if (token[0] == '\0')
            goto missing_transform_parameters;
        texMod->translate[0] = (float)atof(token);

        token = Com_ParseOnLine(text);
        if (token[0] == '\0')
            goto missing_transform_parameters;
        texMod->translate[1] = (float)atof(token);

        texMod->type = TMOD_TRANSFORM;
        ++bundle->numTexMods;
        return;
    }

    if (Q_stricmp(token, "rotate") == 0) {
        token = Com_ParseOnLine(text);
        if (token[0] == '\0')
            goto missing_rotate_parameters;
        texMod->rotateSpeed = (float)atof(token);

        texMod->type = TMOD_ROTATE;
        rendererParsedShader.surfaceFlags |= SHADER_SURFACE_TIME_DEPENDENT;
        ++bundle->numTexMods;
        return;
    }

    if (Q_stricmp(token, "entityTranslate") == 0) {
        texMod->type = TMOD_ENTITY_TRANSLATE;
        ++bundle->numTexMods;
        return;
    }

    ri.Printf(R_PRINT_WARNING, "WARNING: unknown or invalid tcMod '%s' in shader '%s'\n", token, rendererParsedShader.name);
    ++bundle->numTexMods;
    return;

missing_turb_parameters:
    ri.Printf(R_PRINT_WARNING, "WARNING: missing tcMod turb parms in shader '%s'\n", rendererParsedShader.name);
    return;

missing_scale_parameters:
    ri.Printf(R_PRINT_WARNING, "WARNING: missing scale parms in shader '%s'\n", rendererParsedShader.name);
    return;

missing_scroll_parameters:
    ri.Printf(R_PRINT_WARNING, "WARNING: missing scale scroll parms in shader '%s'\n", rendererParsedShader.name);
    return;

missing_stretch_parameters:
    ri.Printf(R_PRINT_WARNING, "WARNING: missing stretch parms in shader '%s'\n", rendererParsedShader.name);
    return;

missing_transform_parameters:
    ri.Printf(R_PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", rendererParsedShader.name);
    return;

missing_rotate_parameters:
    ri.Printf(R_PRINT_WARNING, "WARNING: missing tcMod rotate parms in shader '%s'\n", rendererParsedShader.name);
}

/* Source: CoDUOMP.exe 0x004f8600..0x004f8b91.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f8600_004f8b91.mcode.
 * Name: exact same-module Mac symbol ParseTexEnvCombineFunction.
 *
 * Sources use a compact two-character grammar: the first character selects
 * the color/alpha operand and the second selects texture, constant, primary,
 * or previous as the source. A leading "1 -" changes only the operand to its
 * one-minus form. Fields written before a later syntax failure intentionally
 * remain written, matching the original parser scratch behavior. */
qboolean ParseTexEnvCombineFunction(shader_texture_combine_args_t *arguments, qboolean alphaChannel, char **text)
{
    int32_t argumentCount;
    const char *token;

    if (MatchShaderTokenOnLine(text, "=", "texEnvCombine funxtion") == qfalse) {
        return qfalse;
    }

    token = Com_ParseOnLine(text);
    if (token[0] == '\0') {
        ri.Printf(R_PRINT_WARNING,
                  "WARNING: missing equation name in texEnvCombine function "
                  "in shader '%s'\n",
                  rendererParsedShader.name);
        return qfalse;
    }

    if (Q_stricmpn(token, "REPLACE", R_SHADER_TOKEN_COMPARE_LIMIT) == 0) {
        arguments->operation = GL_REPLACE;
        argumentCount = 1;
    } else if (Q_stricmpn(token, "MODULATE", R_SHADER_TOKEN_COMPARE_LIMIT) == 0) {
        arguments->operation = GL_MODULATE;
        argumentCount = 2;
    } else if (Q_stricmpn(token, "ADD", R_SHADER_TOKEN_COMPARE_LIMIT) == 0) {
        arguments->operation = GL_ADD;
        argumentCount = 2;
    } else if (Q_stricmp(token, "ADD_SIGNED_ARB") == 0) {
        arguments->operation = GL_ADD_SIGNED_EXT;
        argumentCount = 2;
    } else if (Q_stricmp(token, "SUBTRACT_ARB") == 0) {
        arguments->operation = GL_SUBTRACT_ARB;
        argumentCount = 2;
    } else if (Q_stricmp(token, "INTERPOLATE_ARB") == 0) {
        arguments->operation = GL_INTERPOLATE_EXT;
        argumentCount = 3;
    } else if (Q_stricmp(token, "DOT3_RGB_ARB") == 0) {
        if (rendererShaderRequirements[SHADER_REQUIREMENT_TEXTURE_ENV_DOT3_ARB] == 0) {
            ri.Printf(R_PRINT_WARNING,
                      "WARNING: shader '%s' uses DOT3_RGB_ARB without "
                      "'requires GL_ARB_texture_env_dot3'\n",
                      rendererParsedShader.name);
            return qfalse;
        }
        if (alphaChannel != qfalse) {
            ri.Printf(R_PRINT_WARNING, "WARNING: DOT3_RGB_ARB only valid for rgb channel\n");
            return qfalse;
        }
        arguments->operation = GL_DOT3_RGB_ARB;
        argumentCount = 2;
    } else if (Q_stricmp(token, "DOT3_RGBA_ARB") == 0) {
        if (rendererShaderRequirements[SHADER_REQUIREMENT_TEXTURE_ENV_DOT3_ARB] == 0) {
            ri.Printf(R_PRINT_WARNING,
                      "WARNING: shader '%s' uses DOT3_RGBA_ARB without "
                      "'requires GL_ARB_texture_env_dot3'\n",
                      rendererParsedShader.name);
            return qfalse;
        }
        if (alphaChannel != qfalse) {
            ri.Printf(R_PRINT_WARNING, "WARNING: DOT3_RGBA_ARB only valid for rgb channel\n");
            return qfalse;
        }
        arguments->operation = GL_DOT3_RGBA_ARB;
        argumentCount = 2;
    } else {
        ri.Printf(R_PRINT_WARNING,
                  "WARNING: unknown equation name '%s' in texEnvCombine "
                  "in shader '%s'\n",
                  token, rendererParsedShader.name);
        return qfalse;
    }

    if (MatchShaderTokenOnLine(text, "(", "texEnvCombine function") == qfalse) {
        return qfalse;
    }

    for (int32_t argumentIndex = 0; argumentIndex < argumentCount; ++argumentIndex) {
        qboolean oneMinusSource = qfalse;

        token = Com_ParseOnLine(text);
        if (token[0] == '\0') {
            ri.Printf(R_PRINT_WARNING,
                      "WARNING: missing argument in texEnvCombine function "
                      "in shader '%s'\n",
                      rendererParsedShader.name);
            return qfalse;
        }

        if (Q_stricmpn(token, "1", R_SHADER_TOKEN_COMPARE_LIMIT) == 0) {
            if (MatchShaderTokenOnLine(text, "-", "after 1 for 1 - source in texEnvCombine function:") == qfalse) {
                return qfalse;
            }
            if (argumentIndex == 2) {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: the third argument cannot be 1 - source "
                          "for texEnvCombine function in shader '%s'\n",
                          rendererParsedShader.name);
                return qfalse;
            }

            token = Com_ParseOnLine(text);
            if (token[0] == '\0') {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: missing source for 1 - source in "
                          "texEnvCombine function in shader '%s'\n",
                          rendererParsedShader.name);
                return qfalse;
            }
            oneMinusSource = qtrue;
        }

        if (token[1] == '\0' || token[2] != '\0')
            goto invalid_source;

        if (token[0] == 'C') {
            if (alphaChannel != qfalse) {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: '%s' is not a valid source for "
                          "texEnvCombine alpha function in shader '%s'\n",
                          token, rendererParsedShader.name);
                return qfalse;
            }
            if (argumentIndex == 2) {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: the third argument must be an alpha for "
                          "texEnvCombine function in shader '%s'\n",
                          rendererParsedShader.name);
                return qfalse;
            }
            arguments->operands[argumentIndex] = oneMinusSource != qfalse ? GL_ONE_MINUS_SRC_COLOR : GL_SRC_COLOR;
        } else if (token[0] == 'A') {
            arguments->operands[argumentIndex] = oneMinusSource != qfalse ? GL_ONE_MINUS_SRC_ALPHA : GL_SRC_ALPHA;
        } else {
            goto invalid_source;
        }

        switch (token[1]) {
        case 't':
            arguments->sources[argumentIndex] = GL_TEXTURE;
            break;
        case 'c':
            arguments->sources[argumentIndex] = GL_CONSTANT_EXT;
            break;
        case 'f':
            arguments->sources[argumentIndex] = GL_PRIMARY_COLOR_EXT;
            break;
        case 'p':
            arguments->sources[argumentIndex] = GL_PREVIOUS_EXT;
            break;
        default:
            goto invalid_source;
        }

        if (MatchShaderTokenOnLine(text, argumentIndex + 1 == argumentCount ? ")" : ",", "texEnvCombine function") == qfalse) {
            return qfalse;
        }
        continue;

    invalid_source:
        ri.Printf(R_PRINT_WARNING,
                  "WARNING: '%s' is not a valid source for texEnvCombine "
                  "function in shader '%s'\n",
                  token, rendererParsedShader.name);
        return qfalse;
    }

    arguments->scale = 1.0f;
    token = Com_ParseOnLine(text);
    if (token[0] == '\0')
        return qtrue;

    if (token[0] != '*') {
        ri.Printf(R_PRINT_WARNING,
                  "WARNING: unknown token '%s' following texEnvCombine "
                  "function in shader '%s'\n",
                  token, rendererParsedShader.name);
        return qfalse;
    }

    token = Com_ParseOnLine(text);
    if (token[0] == '\0') {
        ri.Printf(R_PRINT_WARNING,
                  "WARNING: missing scale for texEnvCombine function "
                  "in shader '%s'\n",
                  rendererParsedShader.name);
        return qfalse;
    }

    arguments->scale = (float)atof(token);
    if (arguments->scale == 1.0f || arguments->scale == 2.0f || arguments->scale == 4.0f) {
        return qtrue;
    }

    ri.Printf(R_PRINT_WARNING,
              "WARNING: bad scale '%s' for texEnvCombine function "
              "in shader '%s'\n",
              token, rendererParsedShader.name);
    return qfalse;
}

/* Source: CoDUOMP.exe 0x004f8ba0..0x004f8ff2.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f8ba0_004f8ff2.mcode.
 * Name: exact same-module Mac symbol ParseTextureEnvCombine.
 *
 * The combine record is attached to the bundle before any syntax checks.
 * Consequently a failed parse retains both the allocation and every field
 * written before the failure; no cleanup or rollback is performed. */
qboolean ParseTextureEnvCombine(textureBundle_t *bundle, char **text)
{
    shader_texture_combine_t *combine;
    const char *token;

    bundle->textureEnvMode = GL_COMBINE_ARB;
    combine = ri.Hunk_Alloc(sizeof(*combine));
    bundle->textureCombine = combine;

    combine->rgb.operation = GL_REPLACE;
    combine->rgb.scale = 1.0f;
    combine->rgb.sources[0] = GL_TEXTURE;
    combine->rgb.sources[1] = GL_TEXTURE;
    combine->rgb.sources[2] = GL_TEXTURE;
    combine->rgb.operands[0] = GL_SRC_COLOR;
    combine->rgb.operands[1] = GL_SRC_COLOR;
    combine->rgb.operands[2] = GL_SRC_ALPHA;

    combine->alpha.operation = GL_REPLACE;
    combine->alpha.scale = 1.0f;
    combine->alpha.sources[0] = GL_TEXTURE;
    combine->alpha.sources[1] = GL_TEXTURE;
    combine->alpha.sources[2] = GL_TEXTURE;
    combine->alpha.operands[0] = GL_SRC_ALPHA;
    combine->alpha.operands[1] = GL_SRC_ALPHA;
    combine->alpha.operands[2] = GL_SRC_ALPHA;

    if (MatchShaderToken(text, "{", "texEnvCombine") == qfalse)
        return qfalse;

    Com_SkipRestOfLine(text);
    combine->environmentColor[0] = 1.0f;
    combine->environmentColor[1] = 1.0f;
    combine->environmentColor[2] = 1.0f;
    combine->environmentColor[3] = 1.0f;

    token = Com_Parse(text);
    if (token[0] == '\0')
        goto missing_rgb_equation;

    if (Q_stricmpn(token, "const", R_SHADER_TOKEN_COMPARE_LIMIT) == 0) {
        if (MatchShaderTokenOnLine(text, "=", "texEnvCombine const") == qfalse) {
            return qfalse;
        }
        if (MatchShaderTokenOnLine(text, "(", "texEnvCombine const") == qfalse) {
            return qfalse;
        }

        token = Com_ParseOnLine(text);
        if (token[0] == '\0') {
            ri.Printf(R_PRINT_WARNING,
                      "WARNING: missing color in texEnvCombine const "
                      "in shader '%s'\n",
                      rendererParsedShader.name);
            return qfalse;
        }
        combine->environmentColor[0] = (float)atof(token);

        token = Com_ParseOnLine(text);
        if (token[0] == '\0')
            goto truncated_constant_color;
        combine->environmentColor[1] = (float)atof(token);

        token = Com_ParseOnLine(text);
        if (token[0] == '\0')
            goto truncated_constant_color;
        combine->environmentColor[2] = (float)atof(token);

        token = Com_ParseOnLine(text);
        if (token[0] != ')' && token[0] != '\0') {
            combine->environmentColor[3] = (float)atof(token);
            token = Com_Parse(text);
        }
        if (token[0] != ')') {
            ri.Printf(R_PRINT_WARNING,
                      "WARNING: missing ')' in texEnvCombine const "
                      "in shader '%s'\n",
                      rendererParsedShader.name);
            return qfalse;
        }

        token = Com_ParseOnLine(text);
        if (token[0] == '\0') {
            token = Com_Parse(text);
        } else if (token[0] == '*') {
            token = Com_ParseOnLine(text);
            if (Q_stricmp(token, "identityLighting") != 0) {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: expected 'identityLighting' in "
                          "texEnvCombine const in shader '%s'\n",
                          rendererParsedShader.name);
                return qfalse;
            }
            combine->environmentColor[0] *= tr.identityLight;
            combine->environmentColor[1] *= tr.identityLight;
            combine->environmentColor[2] *= tr.identityLight;
            token = Com_Parse(text);
        } else {
            ri.Printf(R_PRINT_WARNING,
                      "WARNING: unexpected '%s' in texEnvCombine const "
                      "in shader '%s'\n",
                      token, rendererParsedShader.name);
            return qfalse;
        }
    }

    if (token[0] == '\0')
        goto missing_rgb_equation;
    if (Q_stricmpn(token, "rgb", R_SHADER_TOKEN_COMPARE_LIMIT) != 0) {
        goto missing_rgb_equation;
    }

    if (ParseTexEnvCombineFunction(&combine->rgb, qfalse, text) == qfalse) {
        return qfalse;
    }

    token = Com_Parse(text);
    if (token[0] != '\0' && Q_stricmp(token, "alpha") == 0) {
        if (combine->rgb.operation == GL_DOT3_RGBA_ARB) {
            ri.Printf(R_PRINT_WARNING,
                      "WARNING: alpha not allowed in texEnvCombine if rgb "
                      "is DOT3_RGBA_ARB in shader '%s'\n",
                      rendererParsedShader.name);
            return qfalse;
        }

        if (ParseTexEnvCombineFunction(&combine->alpha, qtrue, text) == qfalse) {
            return qfalse;
        }
        token = Com_Parse(text);
    } else {
        combine->alpha.operation = GL_REPLACE;
        combine->alpha.scale = 1.0f;
        combine->alpha.sources[0] = GL_TEXTURE;
        combine->alpha.operands[0] = GL_SRC_ALPHA;
    }

    if (token[0] != '}') {
        ri.Printf(R_PRINT_WARNING, "WARNING: missing '}' in texEnvCombine in shader '%s'\n", rendererParsedShader.name);
        return qfalse;
    }
    return qtrue;

truncated_constant_color:
    ri.Printf(R_PRINT_WARNING,
              "WARNING: truncated color in texEnvCombine const "
              "in shader '%s'\n",
              rendererParsedShader.name);
    return qfalse;

missing_rgb_equation:
    ri.Printf(R_PRINT_WARNING,
              "WARNING: missing 'rgb' equation in texEnvCombine "
              "in shader '%s'\n",
              rendererParsedShader.name);
    return qfalse;
}

/* Source: CoDUOMP.exe 0x004f99a0..0x004f9edc.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f99a0_004f9edd.mcode.
 * Name: exact same-module Mac symbol ParseImage.
 *
 * Options preceding the image name adjust the R_FindImageFile flags. Built-in
 * names return renderer-owned images directly. The original's optional
 * heightToNormal scale probe ungets an exact 1.0 token; although surprising,
 * that parser behavior is retained verbatim. */
qboolean ParseImage(char **text, qboolean allowTextureName, qboolean loadImage, renderer_image_track_t imageTrack,
                    const float colorScale[4], image_t **outImage)
{
    const char *imageName;
    uint32_t imageFlags = IMAGE_FLAG_NONE;
    float heightScale = 1.0f;
    qboolean only16Bit = qfalse;
    qboolean only32Bit = qfalse;

    *outImage = NULL;

    if ((rendererParsedShader.flags & SHADER_FLAG_USE_PICMIP2) != 0) {
        imageFlags |= IMAGE_FLAG_USE_PICMIP2;
    }
    if ((rendererParsedShader.flags & SHADER_FLAG_NO_PICMIP) == 0) {
        imageFlags |= IMAGE_FLAG_ALLOW_PICMIP;
    }
    if ((rendererParsedShader.flags & SHADER_FLAG_NO_MIPMAPS) == 0) {
        imageFlags |= IMAGE_FLAG_MIPMAP;
    }
    if ((rendererParsedShader.flags & SHADER_FLAG_NO_IMAGE_OVERBRIGHT) != 0) {
        imageFlags |= IMAGE_FLAG_NO_OVERBRIGHT;
    }

    for (;;) {
        imageName = Com_ParseOnLine(text);
        if (imageName[0] == '\0') {
            ri.Printf(R_PRINT_WARNING, "WARNING: missing image name in shader %s\n", rendererParsedShader.name);
            return qfalse;
        }

        if (Q_stricmpn(imageName, "nopicmip", R_SHADER_TOKEN_COMPARE_LIMIT) == 0) {
            imageFlags &= ~IMAGE_FLAG_ALLOW_PICMIP;
            continue;
        }
        if (Q_stricmpn(imageName, "picmip", R_SHADER_TOKEN_COMPARE_LIMIT) == 0) {
            imageFlags |= IMAGE_FLAG_ALLOW_PICMIP;
            continue;
        }
        if (Q_stricmpn(imageName, "nomipmaps", R_SHADER_TOKEN_COMPARE_LIMIT) == 0) {
            imageFlags &= ~IMAGE_FLAG_MIPMAP;
            continue;
        }
        if (Q_stricmpn(imageName, "mipmaps", R_SHADER_TOKEN_COMPARE_LIMIT) == 0) {
            imageFlags |= IMAGE_FLAG_MIPMAP;
            continue;
        }
        if (Q_stricmpn(imageName, "clamp", R_SHADER_TOKEN_COMPARE_LIMIT) == 0) {
            imageFlags |= IMAGE_FLAG_CLAMP_S | IMAGE_FLAG_CLAMP_T;
            continue;
        }
        if (Q_stricmpn(imageName, "clampx", R_SHADER_TOKEN_COMPARE_LIMIT) == 0 ||
            Q_stricmpn(imageName, "clamps", R_SHADER_TOKEN_COMPARE_LIMIT) == 0) {
            imageFlags |= IMAGE_FLAG_CLAMP_S;
            continue;
        }
        if (Q_stricmpn(imageName, "clampy", R_SHADER_TOKEN_COMPARE_LIMIT) == 0 ||
            Q_stricmpn(imageName, "clampt", R_SHADER_TOKEN_COMPARE_LIMIT) == 0) {
            imageFlags |= IMAGE_FLAG_CLAMP_T;
            continue;
        }
        if (Q_stricmp(imageName, "heightToNormal") == 0) {
            imageFlags |= IMAGE_FLAG_HEIGHT_TO_NORMAL;
            heightScale = (float)atof(Com_ParseOnLine(text));
            if (heightScale == 1.0f) {
                Com_UngetToken();
                heightScale = 1.0f;
            }
            continue;
        }
        if (Q_stricmp(imageName, "only16bit") == 0) {
            only16Bit = qtrue;
            continue;
        }
        if (Q_stricmp(imageName, "only32bit") == 0) {
            only32Bit = qtrue;
            continue;
        }
        if (Q_stricmp(imageName, "noopt") == 0) {
            imageFlags |= IMAGE_FLAG_NO_TEXTURE_SHEET;
            continue;
        }
        break;
    }

    if (Q_stricmpn(imageName, "$lightmap", R_SHADER_TOKEN_COMPARE_LIMIT) == 0) {
        if (colorScale != NULL) {
            ri.Printf(R_PRINT_WARNING,
                      "WARNING: shader '%s' tried to use multiplyImage "
                      "with image '%s'\n",
                      rendererParsedShader.name, imageName);
            return qfalse;
        }

        rendererParsedShader.surfaceFlags |= R_PARSE_IMAGE_LIGHTMAP_SOURCE_FLAG;
        if (rendererParsedShader.lightmapIndex < 0) {
            *outImage = tr.identityLightImage;
        } else {
            *outImage = tr.lightmaps[rendererParsedShader.lightmapIndex];
        }
        return qtrue;
    }

    if (Q_stricmpn(imageName, "$whiteimage", R_SHADER_TOKEN_COMPARE_LIMIT) == 0 ||
        Q_stricmpn(imageName, "*white", R_SHADER_TOKEN_COMPARE_LIMIT) == 0) {
        if (colorScale != NULL) {
            ri.Printf(R_PRINT_WARNING,
                      "WARNING: shader '%s' tried to use multiplyImage "
                      "with image '%s'\n",
                      rendererParsedShader.name, imageName);
            return qfalse;
        }
        rendererParsedShader.surfaceFlags |= R_PARSE_IMAGE_COLOR_SOURCE_FLAG;
        *outImage = tr.whiteImage;
        return qtrue;
    }

    if (Q_stricmpn(imageName, "$dlight", R_SHADER_TOKEN_COMPARE_LIMIT) == 0) {
        if (colorScale != NULL) {
            ri.Printf(R_PRINT_WARNING,
                      "WARNING: shader '%s' tried to use multiplyImage "
                      "with image '%s'\n",
                      rendererParsedShader.name, imageName);
            return qfalse;
        }
        *outImage = tr.dlightImage;
        return qtrue;
    }

    if (Q_stricmp(imageName, "$screen") == 0) {
        if (colorScale != NULL) {
            ri.Printf(R_PRINT_WARNING,
                      "WARNING: shader '%s' tried to use multiplyImage "
                      "with image '%s'\n",
                      rendererParsedShader.name, imageName);
            return qfalse;
        }
        *outImage = tr.screenImage;
        return qtrue;
    }

    if ((imageFlags & IMAGE_FLAG_HEIGHT_TO_NORMAL) == 0 && r_graymap->integer != 0 &&
        rendererParsedShader.lightmapIndex != R_PARSE_IMAGE_GRAYMAP_EXCLUDED_MODE_A &&
        rendererParsedShader.lightmapIndex != R_PARSE_IMAGE_GRAYMAP_EXCLUDED_MODE_B) {
        rendererParsedShader.surfaceFlags |= R_PARSE_IMAGE_COLOR_SOURCE_FLAG;
        *outImage = tr.grayImage;
    } else if (Q_stricmp(imageName, "$texturename") == 0) {
        rendererParsedShader.surfaceFlags |= R_PARSE_IMAGE_COLOR_SOURCE_FLAG;
        if (allowTextureName == qfalse) {
            ri.Printf(R_PRINT_WARNING,
                      "WARNING: $texturename used in shader '%s', "
                      "which is not a shader type file\n",
                      rendererParsedShader.name);
            return qfalse;
        }

        imageName = Com_ParseOnLine(text);
        if (Q_stricmp(imageName, ",") == 0)
            imageName = Com_ParseOnLine(text);
        else
            imageName = "";

        *outImage = R_FindImageFile(imageName, GL_TEXTURE_2D, imageFlags, R_IMAGE_TRACK_GENERATED_TEXTURE, colorScale, heightScale);
    }

    if (only16Bit != qfalse) {
        if (only32Bit != qfalse) {
            ri.Printf(R_PRINT_WARNING,
                      "WARNING: only16bit and only32bit are mutually "
                      "exclusive in shader %s\n",
                      rendererParsedShader.name);
            return qfalse;
        }
        if (glConfig.colorBits > R_PARSE_IMAGE_16_BIT_COLOR_DEPTH) {
            return qtrue;
        }
    } else if (only32Bit != qfalse && glConfig.colorBits <= R_PARSE_IMAGE_16_BIT_COLOR_DEPTH) {
        return qtrue;
    }

    if (loadImage != qfalse && *outImage == NULL) {
        if (tr.delayedImageGroup != 0 && rendererParsedShader.lightmapIndex == R_PARSE_IMAGE_NO_LIGHTMAP_MODE) {
            imageFlags &= ~IMAGE_FLAG_DELAYED_UPLOAD;
        }

        *outImage = R_FindImageFile(imageName, GL_TEXTURE_2D, imageFlags, imageTrack, colorScale, heightScale);
        if (*outImage == NULL) {
            /* NOT_FROM_ORIGINAL_SOURCE: the stock sfx shader retains this
             * Quake III rail-trail debug material, but no stock CoD or UO PK3
             * supplies its image. Suppress only that known orphan; preserve
             * the parse failure and every other missing-image warning. */
            if (Q_stricmp(rendererParsedShader.name, "railCore") == 0 && Q_stricmp(imageName, "gfx/misc/railcorethin_mono.tga") == 0) {
                return qfalse;
            }

            ri.Printf(R_PRINT_WARNING, "WARNING: Couldn't load image '%s' in shader %s\n", imageName, rendererParsedShader.name);
            return qfalse;
        }
    }

    return qtrue;
}

/* Source: CoDUOMP.exe 0x004f9ee0..0x004f9f75.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f9ee0_004f9f76.mcode.
 * Name and parameter order: exact same-module Mac symbol
 * ParseWaterMapPositiveFloat. Both original platforms return floating
 * 0.0f/1.0f status values even though the parsed value is written separately. */
float ParseWaterMapPositiveFloat(const char *parameterName, char **text, float *value)
{
    const char *token = Com_ParseOnLine(text);
    if (token[0] == '\0') {
        ri.Printf(R_PRINT_WARNING, "WARNING: waterMap missing %s in shader %s\n", parameterName, rendererParsedShader.name);
        return 0.0f;
    }

    *value = (float)atof(token);
    if (*value <= 0.0f) {
        ri.Printf(R_PRINT_WARNING, "WARNING: %s must be > 0 in waterMap in shader %s\n", parameterName, rendererParsedShader.name);
        return 0.0f;
    }

    return 1.0f;
}

/* Source: CoDUOMP.exe 0x004f9f80..0x004f9ffd.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f9f80_004f9ffe.mcode.
 * Name and parameter order: exact same-module Mac symbol ParseWaterMapFloat.
 * Like the positive variant, its status result is a float in both binaries. */
float ParseWaterMapFloat(const char *parameterName, char **text, float *value)
{
    const char *token = Com_ParseOnLine(text);
    if (token[0] == '\0') {
        ri.Printf(R_PRINT_WARNING, "WARNING: waterMap missing %s in shader %s\n", parameterName, rendererParsedShader.name);
        return 0.0f;
    }

    *value = (float)atof(token);
    return 1.0f;
}

/* Source: CoDUOMP.exe 0x004fa000..0x004fa0b7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004fa000_004fa0b8.mcode.
 * Name and parameter order: exact same-module Mac symbol ParseWaterMapInt. */
qboolean ParseWaterMapInt(const char *parameterName, char **text, int32_t minimum, int32_t maximum, int32_t *value)
{
    const char *token = Com_ParseOnLine(text);
    if (token[0] == '\0') {
        ri.Printf(R_PRINT_WARNING, "WARNING: waterMap missing %s in shader %s\n", parameterName, rendererParsedShader.name);
        return qfalse;
    }

    *value = coduo_crt_atoi(token);
    if (*value < minimum || *value > maximum) {
        ri.Printf(R_PRINT_WARNING,
                  "WARNING: %s must be >= %i and <= %i in waterMap "
                  "in shader %s\n",
                  parameterName, minimum, maximum, rendererParsedShader.name);
        return qfalse;
    }

    if (R_IsPowerOfTwo(*value) == qfalse) {
        ri.Printf(R_PRINT_WARNING,
                  "WARNING: %s must be a power of 2 in waterMap "
                  "in shader %s\n",
                  parameterName, rendererParsedShader.name);
        return qfalse;
    }

    return qtrue;
}

/* Source: CoDUOMP.exe 0x004fa0c0..0x004fa299.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004fa0c0_004fa29a.mcode.
 * Name and parameter order: exact same-module Mac symbol ParseWaterMap.
 *
 * The parser supplies the nine user-configurable fields. R_GetWaterTexture
 * owns the image, upload-frame state, and frequency buffers in the leading
 * four fields of the permanent cached record. */
qboolean ParseWaterMap(shaderStage_t *stage, char **text, int32_t bundleIndex)
{
    shader_water_map_t configuration;

    if (ParseWaterMapInt("texture width", text, R_WATER_MAP_MIN_TEXTURE_SIZE, R_WATER_MAP_MAX_TEXTURE_SIZE, &configuration.textureWidth) ==
            qfalse ||
        /* Both binaries reuse "texture width" here for the height error. */
        ParseWaterMapInt("texture width", text, R_WATER_MAP_MIN_TEXTURE_SIZE, R_WATER_MAP_MAX_TEXTURE_SIZE, &configuration.textureHeight) ==
            qfalse ||
        ParseWaterMapPositiveFloat("horizontal world length", text, &configuration.horizontalWorldLength) == 0.0f ||
        ParseWaterMapPositiveFloat("vertical world length", text, &configuration.verticalWorldLength) == 0.0f ||
        ParseWaterMapPositiveFloat("wind velocity", text, &configuration.windVelocity) == 0.0f ||
        ParseWaterMapFloat("wind x direction", text, &configuration.windDirection[0]) == 0.0f ||
        ParseWaterMapFloat("wind y direction", text, &configuration.windDirection[1]) == 0.0f) {
        return qfalse;
    }

    if (VectorNormalize2D(configuration.windDirection) == 0.0f) {
        ri.Printf(R_PRINT_WARNING, "WARNING: wind direction is 0 0 in waterMap in shader %s\n", rendererParsedShader.name);
        return qfalse;
    }

    if (ParseWaterMapPositiveFloat("amplitude", text, &configuration.amplitude) == 0.0f) {
        return qfalse;
    }

    /* Exact 0x44480000 float from both original binaries. */
    configuration.gravity = 800.0f;

    textureBundle_t *bundle = &stage->bundle[bundleIndex];
    bundle->waterMap = R_GetWaterTexture(&configuration);
    if (bundle->waterMap == NULL)
        return qfalse;

    bundle->image[0] = bundle->waterMap->image;
    return qtrue;
}

/* Source: CoDUOMP.exe 0x004fa2a0..0x004fa367.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004fa2a0_004fa368.mcode.
 * Name and parameter order: exact same-module Mac symbol ParseNVCullParm.
 * The original stores the GL comparison enumerants as raw words in the same
 * four-slot union used for ordinary floating texture-shader parameters. */
qboolean ParseNVCullParm(shaderStage_t *stage, char **text, int32_t bundleIndex, int32_t parameterIndex)
{
    const char *token = Com_ParseOnLine(text);
    shader_texture_shader_t *textureShader = stage->bundle[bundleIndex].textureShader;

    if (strcmp(token, "LESS_THAN_ZERO") == 0) {
        textureShader->parameters.cullModes[parameterIndex] = GL_LESS;
        return qtrue;
    }
    if (strcmp(token, "GEQUAL_TO_ZERO") == 0) {
        textureShader->parameters.cullModes[parameterIndex] = GL_GEQUAL;
        return qtrue;
    }

    ri.Printf(R_PRINT_WARNING,
              "WARNING: shader '%s' - cull argument must be "
              "'LESS_THAN_ZERO' or 'GEQUAL_TO_ZERO'\n",
              rendererParsedShader.name);
    return qfalse;
}

/* Source: CoDUOMP.exe 0x004fa370..0x004fa3fb.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004fa370_004fa3fc.mcode.
 * Name and parameter order: exact same-module Mac symbol ParseNVFloatParm. */
qboolean ParseNVFloatParm(shaderStage_t *stage, char **text, int32_t bundleIndex, int32_t parameterIndex)
{
    const char *token = Com_ParseOnLine(text);
    if (token[0] == '\0') {
        ri.Printf(R_PRINT_WARNING, "WARNING: shader '%s' - missing value in 'nvTexShader'\n", rendererParsedShader.name);
        return qfalse;
    }

    stage->bundle[bundleIndex].textureShader->parameters.floats[parameterIndex] = (float)atof(token);
    return qtrue;
}

/* Source: CoDUOMP.exe 0x004fa400..0x004fa5a2.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004fa400_004fa5a3.mcode.
 * Name and parameter order: exact same-module Mac symbol
 * ParseNVTextureInput. */
qboolean ParseNVTextureInput(shaderStage_t *stage, char **text, int32_t bundleIndex, qboolean allowExpandMapping)
{
    shader_texture_shader_t *textureShader;
    shader_texture_shader_t *previousTextureShader;
    const char *token;

    if (bundleIndex == 0) {
        ri.Printf(R_PRINT_WARNING,
                  "WARNING: texture op in 'nvTexShader' invalid for "
                  "first bundle in shader %s\n",
                  rendererParsedShader.name);
        return qfalse;
    }

    textureShader = stage->bundle[bundleIndex].textureShader;
    previousTextureShader = stage->bundle[bundleIndex - 1].textureShader;

    if (previousTextureShader == NULL || previousTextureShader->operation != GL_DOT_PRODUCT_NV) {
        switch (textureShader->operation) {
        case GL_DEPENDENT_AR_TEXTURE_2D_NV:
        case GL_DEPENDENT_GB_TEXTURE_2D_NV:
        case GL_DOT_PRODUCT_NV:
            break;

        case GL_DOT_PRODUCT_REFLECT_CUBE_MAP_NV:
        case GL_DOT_PRODUCT_CONST_EYE_REFLECT_CUBE_MAP_NV:
            if (previousTextureShader != NULL && previousTextureShader->operation == GL_DOT_PRODUCT_DIFFUSE_CUBE_MAP_NV) {
                break;
            }
            if (bundleIndex >= 2) {
                shader_texture_shader_t *twoBundlesBack = stage->bundle[bundleIndex - 2].textureShader;
                if (twoBundlesBack != NULL && twoBundlesBack->operation == GL_DOT_PRODUCT_NV) {
                    break;
                }
            }

            ri.Printf(R_PRINT_WARNING,
                      "WARNING: bundle %i (from 0) must use "
                      "'nvTexShader dot3' or "
                      "'nvTexShader dot3_diffuse_cubemap' in shader %s\n",
                      bundleIndex - 2, rendererParsedShader.name);
            return qfalse;

        default:
            ri.Printf(R_PRINT_WARNING,
                      "WARNING: bundle %i (from 0) must use "
                      "'nvTexShader dot3' in shader %s\n",
                      bundleIndex - 1, rendererParsedShader.name);
            return qfalse;
        }
    }

    token = Com_ParseOnLine(text);
    if (allowExpandMapping != qfalse) {
        textureShader->dotProductMapping = GL_UNSIGNED_IDENTITY_NV;
        if (strcmp(token, "expand") == 0) {
            textureShader->dotProductMapping = GL_EXPAND_NORMAL_NV;
            token = Com_ParseOnLine(text);
        }
    }

    if (strncmp(token, "tex", 3) == 0 && isdigit((unsigned char)token[3]) != 0 && token[3] - '0' < bundleIndex && token[4] == '\0') {
        textureShader->previousTextureInput = GL_TEXTURE0_ARB + (uint32_t)(token[3] - '0');
        return qtrue;
    }

    ri.Printf(R_PRINT_WARNING,
              "WARNING: input source to texture op in 'nvTexShader' "
              "in shader %s must be tex0%s\n",
              rendererParsedShader.name, bundleIndex != 0 ? va(" to tex%i", bundleIndex) : "");
    return qfalse;
}

/* Source: CoDUOMP.exe 0x004fa5c0..0x004fa6c5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004fa5c0_004fa6c6.mcode.
 * Name and parameter order: exact same-module Mac symbol
 * ParseNVTexShaderArgs. */
qboolean ParseNVTexShaderArgs(shaderStage_t *stage, char **text, int32_t bundleIndex, int32_t argumentCount,
                              const nv_texture_shader_argument_type_t *argumentTypes)
{
    if (MatchShaderTokenOnLine(text, "(", "nvTexShader function") == qfalse) {
        return qfalse;
    }

    for (int32_t argumentIndex = 0; argumentIndex < argumentCount; ++argumentIndex) {
        if (argumentIndex != 0 && MatchShaderTokenOnLine(text, ",", "nvTexShader argument list") == qfalse) {
            return qfalse;
        }

        switch (argumentTypes[argumentIndex]) {
        case NV_TEXTURE_SHADER_ARG_TEXTURE_INPUT:
            if (ParseNVTextureInput(stage, text, bundleIndex, qfalse) == qfalse) {
                return qfalse;
            }
            break;

        case NV_TEXTURE_SHADER_ARG_EXPANDABLE_TEXTURE_INPUT:
            if (ParseNVTextureInput(stage, text, bundleIndex, qtrue) == qfalse) {
                return qfalse;
            }
            break;

        case NV_TEXTURE_SHADER_ARG_CULL_MODE_0:
        case NV_TEXTURE_SHADER_ARG_CULL_MODE_1:
        case NV_TEXTURE_SHADER_ARG_CULL_MODE_2:
        case NV_TEXTURE_SHADER_ARG_CULL_MODE_3:
            if (ParseNVCullParm(stage, text, bundleIndex, argumentTypes[argumentIndex] - NV_TEXTURE_SHADER_ARG_CULL_MODE_0) == qfalse) {
                return qfalse;
            }
            break;

        case NV_TEXTURE_SHADER_ARG_FLOAT_0:
        case NV_TEXTURE_SHADER_ARG_FLOAT_1:
        case NV_TEXTURE_SHADER_ARG_FLOAT_2:
        case NV_TEXTURE_SHADER_ARG_FLOAT_3:
            if (ParseNVFloatParm(stage, text, bundleIndex, argumentTypes[argumentIndex] - NV_TEXTURE_SHADER_ARG_FLOAT_0) == qfalse) {
                return qfalse;
            }
            break;
        }
    }

    return MatchShaderTokenOnLine(text, ")", "nvTexShader function");
}

/* Source: CoDUOMP.exe 0x004fa6f0..0x004fab4d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004fa6f0_004fab4e.mcode.
 * Name and parameter order: exact same-module Mac symbol ParseNVTexShader. */
qboolean ParseNVTexShader(shaderStage_t *stage, char **text, int32_t bundleIndex)
{
    textureBundle_t *bundle;
    shader_texture_shader_t *textureShader;
    const char *operationName;

    if (rendererShaderRequirements[SHADER_REQUIREMENT_TEXTURE_SHADER_NV] == 0) {
        ri.Printf(R_PRINT_WARNING,
                  "WARNING: used nvTexShader without "
                  "'requires GL_NV_texture_shader' in shader '%s'\n",
                  rendererParsedShader.name);
        return qfalse;
    }

    bundle = &stage->bundle[bundleIndex];
    textureShader = ri.Hunk_Alloc(sizeof(*textureShader));
    bundle->textureShader = textureShader;

    operationName = Com_ParseOnLine(text);
    if (operationName[0] == '\0') {
        ri.Printf(R_PRINT_WARNING, "WARNING: missing arguments to nvTexShader in shader '%s'\n", rendererParsedShader.name);
        return qfalse;
    }

    /* The original is this ordered, unrolled comparison/argument-dispatch
     * chain; it has no operation-definition table. */
#define NV_MATCH_0(name_, operation_) \
    if (strcmp(operationName, (name_)) == 0) { \
        textureShader->operation = (operation_); \
        return ParseNVTexShaderArgs(stage, text, bundleIndex, 0, NULL); \
    }
#define NV_MATCH_1(name_, operation_, argument_) \
    if (strcmp(operationName, (name_)) == 0) { \
        const nv_texture_shader_argument_type_t argumentTypes[] = {argument_}; \
        textureShader->operation = (operation_); \
        return ParseNVTexShaderArgs(stage, text, bundleIndex, 1, argumentTypes); \
    }
#define NV_MATCH_4(name_, operation_, a_, b_, c_, d_) \
    if (strcmp(operationName, (name_)) == 0) { \
        const nv_texture_shader_argument_type_t argumentTypes[] = {a_, b_, c_, d_}; \
        textureShader->operation = (operation_); \
        return ParseNVTexShaderArgs(stage, text, bundleIndex, 4, argumentTypes); \
    }

    NV_MATCH_0("texture_2d", GL_TEXTURE_2D);
    NV_MATCH_0("texture_cube_map", GL_TEXTURE_CUBE_MAP_ARB);
    NV_MATCH_4("cull_fragment", GL_CULL_FRAGMENT_NV, NV_TEXTURE_SHADER_ARG_CULL_MODE_0, NV_TEXTURE_SHADER_ARG_CULL_MODE_1,
               NV_TEXTURE_SHADER_ARG_CULL_MODE_2, NV_TEXTURE_SHADER_ARG_CULL_MODE_3);
    NV_MATCH_0("pass_through", GL_PASS_THROUGH_NV);
    NV_MATCH_1("dependent_ar", GL_DEPENDENT_AR_TEXTURE_2D_NV, NV_TEXTURE_SHADER_ARG_TEXTURE_INPUT);
    NV_MATCH_1("dependent_gb", GL_DEPENDENT_GB_TEXTURE_2D_NV, NV_TEXTURE_SHADER_ARG_TEXTURE_INPUT);

#define NV_MATCH_DOT(name_, operation_) NV_MATCH_1(name_, operation_, NV_TEXTURE_SHADER_ARG_EXPANDABLE_TEXTURE_INPUT)
    NV_MATCH_DOT("dot_product_2d_1of2", GL_DOT_PRODUCT_NV);
    NV_MATCH_DOT("dot_product_cube_map_1of3", GL_DOT_PRODUCT_NV);
    NV_MATCH_DOT("dot_product_cube_map_2of3", GL_DOT_PRODUCT_NV);
    NV_MATCH_DOT("dot_product_depth_replace_1of2", GL_DOT_PRODUCT_NV);
    NV_MATCH_DOT("dot_product_reflect_cube_map_const_eye_1of3", GL_DOT_PRODUCT_NV);
    NV_MATCH_DOT("dot_product_reflect_cube_map_const_eye_2of3", GL_DOT_PRODUCT_NV);
    NV_MATCH_DOT("dot_product_reflect_cube_map_eye_from_qs_1of3", GL_DOT_PRODUCT_NV);
    NV_MATCH_DOT("dot_product_reflect_cube_map_eye_from_qs_2of3", GL_DOT_PRODUCT_NV);
    NV_MATCH_DOT("dot_product_cube_map_and_reflect_cube_map_const_eye_1of3", GL_DOT_PRODUCT_NV);
    NV_MATCH_DOT("dot_product_cube_map_and_reflect_cube_map_eye_from_qs_1of3", GL_DOT_PRODUCT_NV);
    NV_MATCH_DOT("dot_product_2d_2of2", GL_DOT_PRODUCT_TEXTURE_2D_NV);
    NV_MATCH_DOT("dot_product_cube_map_3of3", GL_DOT_PRODUCT_TEXTURE_CUBE_MAP_NV);
    NV_MATCH_DOT("dot_product_cube_map_and_reflect_cube_map_const_eye_2of3", GL_DOT_PRODUCT_DIFFUSE_CUBE_MAP_NV);
    NV_MATCH_DOT("dot_product_cube_map_and_reflect_cube_map_eye_from_qs_2of3", GL_DOT_PRODUCT_DIFFUSE_CUBE_MAP_NV);
    NV_MATCH_DOT("dot_product_reflect_cube_map_eye_from_qs_3of3", GL_DOT_PRODUCT_REFLECT_CUBE_MAP_NV);
    NV_MATCH_DOT("dot_product_cube_map_and_reflect_cube_map_eye_from_qs_3of3", GL_DOT_PRODUCT_REFLECT_CUBE_MAP_NV);
#undef NV_MATCH_DOT

    NV_MATCH_4("dot_product_reflect_cube_map_const_eye_3of3", GL_DOT_PRODUCT_CONST_EYE_REFLECT_CUBE_MAP_NV,
               NV_TEXTURE_SHADER_ARG_EXPANDABLE_TEXTURE_INPUT, NV_TEXTURE_SHADER_ARG_FLOAT_0, NV_TEXTURE_SHADER_ARG_FLOAT_1,
               NV_TEXTURE_SHADER_ARG_FLOAT_2);
    NV_MATCH_4("dot_product_cube_map_and_reflect_cube_map_const_eye_3of3", GL_DOT_PRODUCT_CONST_EYE_REFLECT_CUBE_MAP_NV,
               NV_TEXTURE_SHADER_ARG_EXPANDABLE_TEXTURE_INPUT, NV_TEXTURE_SHADER_ARG_FLOAT_0, NV_TEXTURE_SHADER_ARG_FLOAT_1,
               NV_TEXTURE_SHADER_ARG_FLOAT_2);

#undef NV_MATCH_4
#undef NV_MATCH_1
#undef NV_MATCH_0

    ri.Printf(R_PRINT_WARNING, "WARNING: unknown nvTexShader function '%s' in shader '%s'\n", operationName, rendererParsedShader.name);
    return qfalse;
}

/* Source: CoDUOMP.exe 0x004fab50..0x004facc6.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004fab50_004facc7.mcode.
 * Name and parameter order: exact same-module Mac symbol
 * ParseNVRC_ConstColor.
 *
 * A token other than const0/const1 is not an error here: it terminates the
 * leading constant-declaration sequence and is pushed back for the next
 * register-combiner parser. `finished` distinguishes that normal stop from a
 * malformed constant declaration. */
qboolean ParseNVRC_ConstColor(char **text, vec4_t constantColors[2], qboolean *finished)
{
    const char *token;
    int32_t constantIndex;

    *finished = qfalse;

    token = Com_Parse(text);
    if (coduo_crt_stricmp(token, "const0") == 0) {
        constantIndex = 0;
    } else if (coduo_crt_stricmp(token, "const1") == 0) {
        constantIndex = 1;
    } else {
        *finished = qtrue;
        Com_UngetToken();
        return qfalse;
    }

    if (MatchShaderToken(text, "=", "nvRegCombiners const") == qfalse || MatchShaderToken(text, "(", "nvRegCombiners const") == qfalse) {
        return qfalse;
    }

    for (int32_t component = 0; component < 4; ++component) {
        token = Com_Parse(text);
        const long double componentRaw = (long double)atof(token);
        constantColors[constantIndex][component] = (float)componentRaw;

        /* The original accepts any nonzero atof result and also accepts a
         * zero result whose token begins alphabetically. This deliberately
         * preserves that permissive legacy test rather than substituting a
         * stricter modern number parser. */
        /* 0x004fac3d stores the float component but compares the retained
         * atof result with zero. */
        if (componentRaw == 0.0L && coduo_crt_isalpha((int32_t)(int8_t)(uint8_t)token[0]) == 0) {
            ri.Printf(R_PRINT_WARNING,
                      "WARNING: unexpected token '%s' in nvRegCombiners "
                      "const in shader '%s'\n",
                      token, rendererParsedShader.name);
            return qfalse;
        }

        if (component == 3) {
            return MatchShaderToken(text, ")", "nvRegCombiners const");
        }
        if (MatchShaderToken(text, ",", "nvRegCombiners const") == qfalse) {
            return qfalse;
        }
    }

    return qfalse;
}

/* Source: CoDUOMP.exe 0x004facd0..0x004fad23.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004facd0_004fad24.mcode.
 * Name and parameter order: exact same-module Mac symbol
 * ParseNVRC_ConstColors. */
qboolean ParseNVRC_ConstColors(char **text, vec4_t constantColors[2], qboolean perStageConstants)
{
    qboolean finished;

    while (ParseNVRC_ConstColor(text, constantColors, &finished) != qfalse) {
        if (perStageConstants != qfalse && rendererShaderRequirements[SHADER_REQUIREMENT_REGISTER_COMBINERS2_NV] == 0) {
            ri.Printf(R_PRINT_WARNING,
                      "WARNING: bundle uses per-combiner constants without "
                      "'requires GL_NV_register_combiners2' in shader '%s'\n",
                      rendererParsedShader.name);
            return qfalse;
        }
    }

    return finished;
}

/* Source: CoDUOMP.exe 0x004fad30..0x004fad8e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004fad30_004fad8f.mcode.
 * Name and parameter order: exact same-module Mac symbol
 * ParseNVRC_RegisterFromTable. */
qboolean ParseNVRC_RegisterFromTable(const char *name, const renderer_nv_register_definition_t *definitions, int32_t definitionCount,
                                     uint32_t *registerName, uint32_t *componentUsage, uint32_t defaultComponentUsage)
{
    for (int32_t definitionIndex = 0; definitionIndex < definitionCount; ++definitionIndex) {
        const renderer_nv_register_definition_t *definition = &definitions[definitionIndex];
        if (coduo_crt_stricmp(definition->token, name) != 0)
            continue;

        *registerName = definition->glRegister;
        *componentUsage = definition->componentUsage;
        if (*componentUsage == 0)
            *componentUsage = defaultComponentUsage;
        return qtrue;
    }

    return qfalse;
}

/* ParseNVRC_ReadWriteRegister's original table. Each register has an
 * unsuffixed spelling whose component usage comes from the caller, plus
 * explicit .a, .rgb, and .b spellings.
 * PE_RELOCATION_VALUES_VERIFIED: verify_relocated_initializers.py locates the
 * table at 0x00591080 and follows all 32 original name pointers. */
static const renderer_nv_register_definition_t nvReadWriteRegisterDefinitions[] = {{"col0", GL_PRIMARY_COLOR_NV, 0},
                                                                                   {"col0.a", GL_PRIMARY_COLOR_NV, GL_ALPHA},
                                                                                   {"col0.rgb", GL_PRIMARY_COLOR_NV, GL_RGB},
                                                                                   {"col0.b", GL_PRIMARY_COLOR_NV, GL_BLUE},
                                                                                   {"col1", GL_SECONDARY_COLOR_NV, 0},
                                                                                   {"col1.a", GL_SECONDARY_COLOR_NV, GL_ALPHA},
                                                                                   {"col1.rgb", GL_SECONDARY_COLOR_NV, GL_RGB},
                                                                                   {"col1.b", GL_SECONDARY_COLOR_NV, GL_BLUE},
                                                                                   {"spare0", GL_SPARE0_NV, 0},
                                                                                   {"spare0.a", GL_SPARE0_NV, GL_ALPHA},
                                                                                   {"spare0.rgb", GL_SPARE0_NV, GL_RGB},
                                                                                   {"spare0.b", GL_SPARE0_NV, GL_BLUE},
                                                                                   {"spare1", GL_SPARE1_NV, 0},
                                                                                   {"spare1.a", GL_SPARE1_NV, GL_ALPHA},
                                                                                   {"spare1.rgb", GL_SPARE1_NV, GL_RGB},
                                                                                   {"spare1.b", GL_SPARE1_NV, GL_BLUE},
                                                                                   {"tex0", GL_TEXTURE0_ARB, 0},
                                                                                   {"tex0.a", GL_TEXTURE0_ARB, GL_ALPHA},
                                                                                   {"tex0.rgb", GL_TEXTURE0_ARB, GL_RGB},
                                                                                   {"tex0.b", GL_TEXTURE0_ARB, GL_BLUE},
                                                                                   {"tex1", GL_TEXTURE0_ARB + 1, 0},
                                                                                   {"tex1.a", GL_TEXTURE0_ARB + 1, GL_ALPHA},
                                                                                   {"tex1.rgb", GL_TEXTURE0_ARB + 1, GL_RGB},
                                                                                   {"tex1.b", GL_TEXTURE0_ARB + 1, GL_BLUE},
                                                                                   {"tex2", GL_TEXTURE0_ARB + 2, 0},
                                                                                   {"tex2.a", GL_TEXTURE0_ARB + 2, GL_ALPHA},
                                                                                   {"tex2.rgb", GL_TEXTURE0_ARB + 2, GL_RGB},
                                                                                   {"tex2.b", GL_TEXTURE0_ARB + 2, GL_BLUE},
                                                                                   {"tex3", GL_TEXTURE0_ARB + 3, 0},
                                                                                   {"tex3.a", GL_TEXTURE0_ARB + 3, GL_ALPHA},
                                                                                   {"tex3.rgb", GL_TEXTURE0_ARB + 3, GL_RGB},
                                                                                   {"tex3.b", GL_TEXTURE0_ARB + 3, GL_BLUE}};

/* Source: CoDUOMP.exe 0x004fad90..0x004faeb7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004fad90_004faeb8.mcode.
 * Name and parameter order: exact same-module Mac symbol
 * ParseNVRC_ReadWriteRegister. */
qboolean ParseNVRC_ReadWriteRegister(char **text, uint32_t *registerName, uint32_t *componentUsage, uint32_t defaultComponentUsage,
                                     qboolean *valid)
{
    const char *token = Com_Parse(text);

    if (ParseNVRC_RegisterFromTable(token, nvReadWriteRegisterDefinitions,
                                    (int32_t)(sizeof(nvReadWriteRegisterDefinitions) / sizeof(nvReadWriteRegisterDefinitions[0])),
                                    registerName, componentUsage, defaultComponentUsage) == qfalse) {
        Com_UngetToken();
        *valid = qtrue;
        return qfalse;
    }

    if (*registerName == GL_TEXTURE0_ARB + 2 && rendererShaderRequirements[SHADER_REQUIREMENT_MAX_TEXTURE_UNITS_3] == 0 &&
        rendererShaderRequirements[SHADER_REQUIREMENT_MAX_TEXTURE_UNITS_4] == 0) {
        ri.Printf(R_PRINT_WARNING,
                  "WARNING: register combiner uses tex2 without suitable "
                  "'requires GL_MAX_TEXTURE_UNITS_ARB' in shader '%s'\n",
                  rendererParsedShader.name);
        *valid = qfalse;
        return qfalse;
    }

    if (*registerName == GL_TEXTURE0_ARB + 3 && rendererShaderRequirements[SHADER_REQUIREMENT_MAX_TEXTURE_UNITS_4] == 0) {
        ri.Printf(R_PRINT_WARNING,
                  "WARNING: register combiner uses tex3 without suitable "
                  "'requires GL_MAX_TEXTURE_UNITS_ARB' in shader '%s'\n",
                  rendererParsedShader.name);
        *valid = qfalse;
        return qfalse;
    }

    *valid = qtrue;
    return qtrue;
}

/* Source: CoDUOMP.exe 0x004faec0..0x004faf61.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004faec0_004faf62.mcode.
 * Name and parameter order: exact same-module Mac symbol
 * ParseNVRC_GeneralOutputRegister. */
qboolean ParseNVRC_GeneralOutputRegister(char **text, uint32_t *registerName, uint32_t requiredComponentUsage, qboolean *valid)
{
    const char *token;
    uint32_t parsedComponentUsage;

    *valid = qtrue;

    token = Com_Parse(text);
    if (coduo_crt_stricmp(token, "discard") == 0) {
        *registerName = GL_DISCARD_NV;
        return qtrue;
    }

    Com_UngetToken();
    if (ParseNVRC_ReadWriteRegister(text, registerName, &parsedComponentUsage, requiredComponentUsage, valid) == qfalse) {
        return qfalse;
    }

    if (parsedComponentUsage == requiredComponentUsage)
        return qtrue;

    *valid = qfalse;
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    ri.Printf(R_PRINT_WARNING,
              "WARNING: output type must be %s in general combiner function "
              "in shader '%s'\n",
              requiredComponentUsage == GL_RGB ? "rgb" : "alpha", rendererParsedShader.name);
    return qfalse;
}

/* Additional source-only inputs accepted after the shared read/write table.
 * As in that table, an unsuffixed name inherits the caller's requested
 * component usage.
 * PE_RELOCATION_VALUES_VERIFIED: verify_relocated_initializers.py locates the
 * table at 0x00591200 and follows all 20 original name pointers. */
static const renderer_nv_register_definition_t nvReadOnlyRegisterDefinitions[] = {{"fog", GL_FOG, 0},
                                                                                  {"fog.a", GL_FOG, GL_ALPHA},
                                                                                  {"fog.rgb", GL_FOG, GL_RGB},
                                                                                  {"fog.b", GL_FOG, GL_BLUE},
                                                                                  {"zero", GL_ZERO, 0},
                                                                                  {"zero.a", GL_ZERO, GL_ALPHA},
                                                                                  {"zero.rgb", GL_ZERO, GL_RGB},
                                                                                  {"zero.b", GL_ZERO, GL_BLUE},
                                                                                  {"one", GL_ONE, 0},
                                                                                  {"one.a", GL_ONE, GL_ALPHA},
                                                                                  {"one.rgb", GL_ONE, GL_RGB},
                                                                                  {"one.b", GL_ONE, GL_BLUE},
                                                                                  {"const0", GL_CONSTANT_COLOR0_NV, 0},
                                                                                  {"const0.a", GL_CONSTANT_COLOR0_NV, GL_ALPHA},
                                                                                  {"const0.rgb", GL_CONSTANT_COLOR0_NV, GL_RGB},
                                                                                  {"const0.b", GL_CONSTANT_COLOR0_NV, GL_BLUE},
                                                                                  {"const1", GL_CONSTANT_COLOR1_NV, 0},
                                                                                  {"const1.a", GL_CONSTANT_COLOR1_NV, GL_ALPHA},
                                                                                  {"const1.rgb", GL_CONSTANT_COLOR1_NV, GL_RGB},
                                                                                  {"const1.b", GL_CONSTANT_COLOR1_NV, GL_BLUE}};

/* Source: CoDUOMP.exe 0x004faf70..0x004fafe0.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004faf70_004fafe1.mcode.
 * Name and parameter order: exact same-module Mac symbol
 * ParseNVRC_ReadRegister. */
qboolean ParseNVRC_ReadRegister(char **text, uint32_t *registerName, uint32_t *componentUsage, uint32_t defaultComponentUsage)
{
    qboolean valid;

    if (ParseNVRC_ReadWriteRegister(text, registerName, componentUsage, defaultComponentUsage, &valid) != qfalse) {
        return qtrue;
    }
    if (valid == qfalse)
        return qfalse;

    const char *token = Com_Parse(text);
    if (ParseNVRC_RegisterFromTable(token, nvReadOnlyRegisterDefinitions,
                                    (int32_t)(sizeof(nvReadOnlyRegisterDefinitions) / sizeof(nvReadOnlyRegisterDefinitions[0])),
                                    registerName, componentUsage, defaultComponentUsage) != qfalse) {
        return qtrue;
    }

    ri.Printf(R_PRINT_WARNING, "WARNING: missing register in nvRegCombiners in shader '%s'\n", rendererParsedShader.name);
    Com_UngetToken();
    return qfalse;
}

/* Source: CoDUOMP.exe 0x004faff0..0x004fb0a5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004faff0_004fb0a6.mcode.
 * Name and parameter order: exact same-module Mac symbol
 * ParseNVRC_FinalRegister. */
qboolean ParseNVRC_FinalRegister(char **text, uint32_t *registerName, uint32_t *componentUsage, uint32_t defaultComponentUsage)
{
    if (defaultComponentUsage == GL_RGB) {
        const char *token = Com_Parse(text);

        if (coduo_crt_stricmp(token, "final_product") == 0) {
            *registerName = GL_E_TIMES_F_NV;
            *componentUsage = GL_RGB;
            return qtrue;
        }
        if (coduo_crt_stricmp(token, "color_sum") == 0) {
            *registerName = GL_SPARE0_PLUS_SECONDARY_COLOR_NV;
            *componentUsage = GL_RGB;
            return qtrue;
        }
        Com_UngetToken();
    }

    return ParseNVRC_ReadRegister(text, registerName, componentUsage, defaultComponentUsage);
}

/* Source: CoDUOMP.exe 0x004fb0b0..0x004fb35c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004fb0b0_004fb35d.mcode.
 * Name and parameter order: exact same-module Mac symbol
 * ParseNVRC_GeneralMappedRegister. */
qboolean ParseNVRC_GeneralMappedRegister(char **text, renderer_combiner_input_t *input, uint32_t defaultComponentUsage)
{
    const char *token;
    qboolean negate = qfalse;
    qboolean parenthesizedMapping = qtrue;

    token = Com_Parse(text);
    if (strcmp(token, "-") == 0) {
        negate = qtrue;
        token = Com_Parse(text);
    }

    if (coduo_crt_stricmp(token, "expand") == 0) {
        input->mapping = negate != qfalse ? GL_EXPAND_NEGATE_NV : GL_EXPAND_NORMAL_NV;
    } else if (coduo_crt_stricmp(token, "half_bias") == 0) {
        input->mapping = negate != qfalse ? GL_HALF_BIAS_NEGATE_NV : GL_HALF_BIAS_NORMAL_NV;
    } else if (coduo_crt_stricmp(token, "unsigned") == 0) {
        if (negate != qfalse) {
            ri.Printf(R_PRINT_WARNING,
                      "'-' not valid with 'unsigned' in nvRegCombiners "
                      "in shader '%s'\n",
                      rendererParsedShader.name);
            return qfalse;
        }
        input->mapping = GL_UNSIGNED_IDENTITY_NV;
    } else if (coduo_crt_stricmp(token, "unsigned_invert") == 0) {
        if (negate != qfalse) {
            ri.Printf(R_PRINT_WARNING,
                      "'-' not valid with 'unsigned_invert' in "
                      "nvRegCombiners in shader '%s'\n",
                      rendererParsedShader.name);
            return qfalse;
        }
        input->mapping = GL_UNSIGNED_INVERT_NV;
    } else {
        input->mapping = negate != qfalse ? GL_SIGNED_NEGATE_NV : GL_SIGNED_IDENTITY_NV;
        Com_UngetToken();
        parenthesizedMapping = qfalse;
    }

    if (parenthesizedMapping != qfalse && MatchShaderToken(text, "(", "nvRegCombiners register mapping") == qfalse) {
        return qfalse;
    }

    if (ParseNVRC_FinalRegister(text, &input->input, &input->componentUsage, defaultComponentUsage) == qfalse) {
        return qfalse;
    }

    if (input->input == GL_FOG && input->componentUsage == GL_ALPHA) {
        ri.Printf(R_PRINT_WARNING,
                  "WARNING: tried to use non-existent fog.a in "
                  "nvRegCombiners in shader '%s'\n",
                  rendererParsedShader.name);
        return qfalse;
    }

    if (parenthesizedMapping != qfalse && MatchShaderToken(text, ")", "nvRegCombiners register mapping") == qfalse) {
        return qfalse;
    }

    /* GL_ONE is represented as GL_ZERO with the complementary mapping. The
     * result reaching the combiner is identical, while the stored register
     * falls within the hardware's final-input subset. */
    if (input->input == GL_ONE) {
        input->input = GL_ZERO;
        switch (input->mapping) {
        case GL_UNSIGNED_IDENTITY_NV:
            input->mapping = GL_UNSIGNED_INVERT_NV;
            break;
        case GL_UNSIGNED_INVERT_NV:
            input->mapping = GL_UNSIGNED_IDENTITY_NV;
            break;
        case GL_EXPAND_NORMAL_NV:
        case GL_SIGNED_IDENTITY_NV:
            input->mapping = GL_EXPAND_NEGATE_NV;
            break;
        case GL_EXPAND_NEGATE_NV:
        case GL_SIGNED_NEGATE_NV:
            input->mapping = GL_EXPAND_NORMAL_NV;
            break;
        case GL_HALF_BIAS_NORMAL_NV:
            input->mapping = GL_HALF_BIAS_NEGATE_NV;
            break;
        case GL_HALF_BIAS_NEGATE_NV:
            input->mapping = GL_HALF_BIAS_NORMAL_NV;
            break;
        }
    }

    return qtrue;
}

/* Source: CoDUOMP.exe 0x004fb380..0x004fb4b0.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004fb380_004fb4b1.mcode.
 * Name and parameter order: exact same-module Mac symbol
 * ParseNVRC_FinalMappedRegister. */
qboolean ParseNVRC_FinalMappedRegister(char **text, renderer_combiner_input_t *input, uint32_t defaultComponentUsage)
{
    const char *token;
    qboolean parenthesizedMapping = qfalse;

    token = Com_Parse(text);
    if (coduo_crt_stricmp(token, "unsigned") == 0) {
        input->mapping = GL_UNSIGNED_IDENTITY_NV;
        parenthesizedMapping = qtrue;
    } else if (coduo_crt_stricmp(token, "unsigned_invert") == 0) {
        input->mapping = GL_UNSIGNED_INVERT_NV;
        parenthesizedMapping = qtrue;
    } else {
        input->mapping = GL_UNSIGNED_IDENTITY_NV;
        Com_UngetToken();
    }

    if (parenthesizedMapping != qfalse && MatchShaderToken(text, "(", "nvRegCombiners final combiner register mapping") == qfalse) {
        return qfalse;
    }

    if (ParseNVRC_FinalRegister(text, &input->input, &input->componentUsage, defaultComponentUsage) == qfalse) {
        return qfalse;
    }

    if (defaultComponentUsage == GL_ALPHA && input->componentUsage == GL_RGB) {
        ri.Printf(R_PRINT_WARNING,
                  "WARNING: alpha component of final combiner can only "
                  "access alpha and blue in nvRegCombiners in shader '%s'\n",
                  rendererParsedShader.name);
        return qfalse;
    }

    if (parenthesizedMapping != qfalse && MatchShaderToken(text, ")", "nvRegCombiners register mapping") == qfalse) {
        return qfalse;
    }

    if (input->input == GL_ONE) {
        input->input = GL_ZERO;
        if (input->mapping == GL_UNSIGNED_IDENTITY_NV)
            input->mapping = GL_UNSIGNED_INVERT_NV;
        else if (input->mapping == GL_UNSIGNED_INVERT_NV)
            input->mapping = GL_UNSIGNED_IDENTITY_NV;
    }

    return qtrue;
}

/* Source: CoDUOMP.exe 0x004fb4c0..0x004fb5d9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004fb4c0_004fb5da.mcode.
 * Name and parameter order: exact same-module Mac symbol
 * ParseNVRC_SingleGeneralFunction. */
qboolean ParseNVRC_SingleGeneralFunction(char **text, renderer_combiner_portion_t *portion, uint32_t componentUsage, int32_t functionIndex,
                                         qboolean *valid)
{
    const int32_t firstInputIndex = functionIndex * 2;
    uint32_t *outputRegister = functionIndex == 0 ? &portion->output.abOutput : &portion->output.cdOutput;

    if (ParseNVRC_GeneralOutputRegister(text, outputRegister, componentUsage, valid) == qfalse) {
        return qfalse;
    }

    *valid = qfalse;
    if (MatchShaderToken(text, "=", "nvRegCombiners general combiner function") == qfalse) {
        return qfalse;
    }

    if (ParseNVRC_GeneralMappedRegister(text, &portion->inputs[firstInputIndex], componentUsage) == qfalse) {
        return qfalse;
    }

    const char *operatorToken = Com_Parse(text);
    qboolean dotProduct;
    if (strcmp(operatorToken, ".") == 0) {
        dotProduct = qtrue;
    } else if (strcmp(operatorToken, "*") == 0) {
        dotProduct = qfalse;
    } else {
        ri.Printf(R_PRINT_WARNING,
                  "WARNING: expected '.' or '*' operator in general "
                  "combiner function in shader '%s'\n",
                  rendererParsedShader.name);
        return qfalse;
    }

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (functionIndex == 0)
        portion->output.abDotProduct = (uint8_t)dotProduct;
    else
        portion->output.muxSum = (uint8_t)dotProduct;

    if (ParseNVRC_GeneralMappedRegister(text, &portion->inputs[firstInputIndex + 1], componentUsage) == qfalse) {
        return qfalse;
    }

    if (MatchShaderToken(text, ";", "nvRegCombiners general combiner function") == qfalse) {
        return qfalse;
    }

    *valid = qtrue;
    return qtrue;
}

/* Source: CoDUOMP.exe 0x004fb5e0..0x004fb73b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004fb5e0_004fb73c.mcode.
 * Name and parameter order: exact same-module Mac symbol
 * ParseNVRC_GeneralFunction. */
qboolean ParseNVRC_GeneralFunction(char **text, renderer_combiner_portion_t *portion, uint32_t componentUsage)
{
    qboolean valid;

    if (ParseNVRC_SingleGeneralFunction(text, portion, componentUsage, 0, &valid) == qfalse) {
        if (valid != qfalse) {
            ri.Printf(R_PRINT_WARNING,
                      "WARNING: general combiner doesn't specify any "
                      "function in shader '%s'\n",
                      rendererParsedShader.name);
        }
        return qfalse;
    }

    if (ParseNVRC_SingleGeneralFunction(text, portion, componentUsage, 1, &valid) == qfalse) {
        return valid;
    }

    if (portion->output.abDotProduct != 0 || portion->output.cdDotProduct != 0) {
        return qtrue;
    }

    if (ParseNVRC_GeneralOutputRegister(text, &portion->output.sumOutput, componentUsage, &valid) == qfalse) {
        return valid;
    }

    if (MatchShaderToken(text, "=", "sum/mux function in nvRegCombiners") == qfalse) {
        return qfalse;
    }

    const char *token = Com_Parse(text);
    if (coduo_crt_stricmp(token, "sum") == 0) {
        portion->output.muxSum = qfalse;
    } else if (coduo_crt_stricmp(token, "mux") == 0) {
        portion->output.muxSum = qtrue;
    } else {
        ri.Printf(R_PRINT_WARNING,
                  "WARNING: final function must be mux or sum in "
                  "nvRegCombiners in shader '%s'\n",
                  rendererParsedShader.name);
        return qfalse;
    }

    if (MatchShaderToken(text, "(", "sum/mux function in nvRegCombiners") == qfalse) {
        return qfalse;
    }
    if (MatchShaderToken(text, ")", "sum/mux function in nvRegCombiners") == qfalse) {
        return qfalse;
    }
    return MatchShaderToken(text, ";", "sum/mux function in nvRegCombiners");
}

/* Source: CoDUOMP.exe 0x004fb740..0x004fb92b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004fb740_004fb92c.mcode.
 * Name and parameter order: exact same-module Mac symbol
 * ParseNVRC_GeneralPortion. */
qboolean ParseNVRC_GeneralPortion(char **text, renderer_general_combiner_t *combiner, qboolean *valid)
{
    renderer_combiner_portion_t *portion;
    uint32_t componentUsage;

    *valid = qfalse;

    const char *token = Com_Parse(text);
    if (coduo_crt_stricmp(token, "rgb") == 0) {
        portion = &combiner->rgb;
        componentUsage = GL_RGB;
    } else if (coduo_crt_stricmp(token, "alpha") == 0) {
        portion = &combiner->alpha;
        componentUsage = GL_ALPHA;
    } else {
        *valid = qtrue;
        Com_UngetToken();
        return qfalse;
    }

    if (MatchShaderToken(text, "{", "nvRegCombiners general combiner") == qfalse) {
        return qfalse;
    }
    if (ParseNVRC_GeneralFunction(text, portion, componentUsage) == qfalse) {
        return qfalse;
    }

    token = Com_Parse(text);
    if (coduo_crt_stricmp(token, "bias_by_negative_one_half_scale_by_two") == 0) {
        portion->output.scale = GL_SCALE_BY_TWO_NV;
        portion->output.bias = GL_BIAS_BY_NEGATIVE_ONE_HALF_NV;
    } else if (coduo_crt_stricmp(token, "bias_by_negative_one_half") == 0) {
        portion->output.scale = GL_NONE;
        portion->output.bias = GL_BIAS_BY_NEGATIVE_ONE_HALF_NV;
    } else if (coduo_crt_stricmp(token, "scale_by_one_half") == 0) {
        portion->output.scale = GL_SCALE_BY_ONE_HALF_NV;
        portion->output.bias = GL_NONE;
    } else if (coduo_crt_stricmp(token, "scale_by_two") == 0) {
        portion->output.scale = GL_SCALE_BY_TWO_NV;
        portion->output.bias = GL_NONE;
    } else if (coduo_crt_stricmp(token, "scale_by_four") == 0) {
        portion->output.scale = GL_SCALE_BY_FOUR_NV;
        portion->output.bias = GL_NONE;
    } else {
        Com_UngetToken();
        *valid = MatchShaderToken(text, "}", "nvRegCombiners general combiner");
        return *valid;
    }

    if (MatchShaderToken(text, "(", "nvRegCombiners general combiner scale-and-bias") == qfalse) {
        return qfalse;
    }
    if (MatchShaderToken(text, ")", "nvRegCombiners general combiner scale-and-bias") == qfalse) {
        return qfalse;
    }
    if (MatchShaderToken(text, ";", "nvRegCombiners general combiner scale-and-bias") == qfalse) {
        return qfalse;
    }

    *valid = MatchShaderToken(text, "}", "nvRegCombiners general combiner");
    return *valid;
}

/* Source: CoDUOMP.exe 0x004fb930..0x004fb9ac.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004fb930_004fb9ad.mcode.
 * Name and parameter order: exact same-module Mac symbol ParseNVRC_General. */
qboolean ParseNVRC_General(char **text, renderer_general_combiner_t *combiner)
{
    qboolean valid;

    if (ParseNVRC_ConstColors(text, combiner->constantColors, qtrue) == qfalse) {
        return qfalse;
    }

    if (ParseNVRC_GeneralPortion(text, combiner, &valid) == qfalse) {
        if (valid != qfalse) {
            ri.Printf(R_PRINT_WARNING,
                      "WARNING: general combiner doesn't specify alpha "
                      "or rgb in shader '%s'\n",
                      rendererParsedShader.name);
        }
        return qfalse;
    }

    /* A second portion is optional. A non-portion token is ungot and reported
     * through valid=true; a malformed portion leaves valid=false. */
    (void)ParseNVRC_GeneralPortion(text, combiner, &valid);
    if (valid == qfalse)
        return qfalse;

    return MatchShaderToken(text, "}", "nvRegCombiners general combiner");
}

/* Source: CoDUOMP.exe 0x004fb9b0..0x004fbca6.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004fb9b0_004fbca7.mcode.
 * Name and parameter order: exact same-module Mac symbol
 * ParseNVRC_FinalMulSum. */
qboolean ParseNVRC_FinalMulSum(char **text, renderer_final_combiner_t *combiner)
{
    qboolean finalProductSeen = qfalse;
    qboolean clampColorSumSeen = qfalse;

    for (;;) {
        const char *token = Com_Parse(text);

        if (coduo_crt_stricmp(token, "final_product") == 0) {
            if (finalProductSeen != qfalse) {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: duplicate final_product in "
                          "nvRegCombiners in shader '%s'\n",
                          rendererParsedShader.name);
                return qfalse;
            }
            finalProductSeen = qtrue;

            if (ParseNVRC_FinalMappedRegister(text, &combiner->inputs[4], GL_RGB) == qfalse) {
                return qfalse;
            }
            if (MatchShaderToken(text, "*", "nvRegCombiners final combiner final_product") == qfalse) {
                return qfalse;
            }
            if (ParseNVRC_FinalMappedRegister(text, &combiner->inputs[5], GL_RGB) == qfalse) {
                return qfalse;
            }

            if (combiner->inputs[4].componentUsage != GL_RGB || combiner->inputs[5].componentUsage != GL_RGB) {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: final_product can only multiply rgb "
                          "in nvRegCombiners in shader '%s'\n",
                          rendererParsedShader.name);
                return qfalse;
            }

            if (MatchShaderToken(text, ";", "nvRegCombiners final combiner final_product") == qfalse) {
                return qfalse;
            }
            continue;
        }

        if (coduo_crt_stricmp(token, "clamp_color_sum") == 0) {
            if (clampColorSumSeen != qfalse) {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: duplicate clamp_color_sum in "
                          "nvRegCombiners in shader '%s'\n",
                          rendererParsedShader.name);
                return qfalse;
            }
            clampColorSumSeen = qtrue;

            if (MatchShaderToken(text, "(", "nvRegCombiners final combiner clamp_color_sum") == qfalse) {
                return qfalse;
            }
            if (MatchShaderToken(text, ")", "nvRegCombiners final combiner clamp_color_sum") == qfalse) {
                return qfalse;
            }
            if (MatchShaderToken(text, ";", "nvRegCombiners final combiner clamp_color_sum") == qfalse) {
                return qfalse;
            }

            combiner->clampColorSum = qtrue;
            continue;
        }

        Com_UngetToken();
        return qtrue;
    }
}

/* Source: CoDUOMP.exe 0x004fbcb0..0x004fbd7f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004fbcb0_004fbd80.mcode.
 * Name and parameter order: exact same-module Mac symbol ParseNVRC_FinalLerp. */
qboolean ParseNVRC_FinalLerp(char **text, renderer_final_combiner_t *combiner)
{
    static const char context[] = "final rgb lerp function in nvRegCombiners";

    if (MatchShaderToken(text, "(", context) == qfalse)
        return qfalse;
    if (ParseNVRC_FinalMappedRegister(text, &combiner->inputs[0], GL_RGB) == qfalse) {
        return qfalse;
    }

    if (combiner->inputs[0].input == GL_E_TIMES_F_NV) {
        ri.Printf(R_PRINT_WARNING,
                  "Cannot use final_product as first argument to lerp in "
                  "nvRegCombiners in shader '%s'\n",
                  rendererParsedShader.name);
        return qfalse;
    }

    if (MatchShaderToken(text, ",", context) == qfalse)
        return qfalse;
    if (ParseNVRC_FinalMappedRegister(text, &combiner->inputs[1], GL_RGB) == qfalse) {
        return qfalse;
    }
    if (MatchShaderToken(text, ",", context) == qfalse)
        return qfalse;
    if (ParseNVRC_FinalMappedRegister(text, &combiner->inputs[2], GL_RGB) == qfalse) {
        return qfalse;
    }

    return MatchShaderToken(text, ")", context);
}

/* Source: CoDUOMP.exe 0x004fbd80..0x004fc19a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004fbd80_004fc19b.mcode.
 * Name and parameter order: exact same-module Mac symbol
 * ParseNVRC_FinalRgbFunc.
 *
 * The final-combiner equation is A*B + (1-A)*C + D. This parser rewrites
 * source-level sums, products, and lerps into those four hardware inputs. */
qboolean ParseNVRC_FinalRgbFunc(char **text, renderer_final_combiner_t *combiner)
{
    static const char context[] = "final rgb function in nvRegCombiners";
    const renderer_combiner_input_t zero = {GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB};
    renderer_combiner_input_t first;
    renderer_combiner_input_t second;
    renderer_combiner_input_t third;
    const char *token = Com_Parse(text);

    if (coduo_crt_stricmp(token, "lerp") == 0) {
        if (ParseNVRC_FinalLerp(text, combiner) == qfalse)
            return qfalse;

        token = Com_Parse(text);
        if (strcmp(token, "+") == 0) {
            if (ParseNVRC_FinalMappedRegister(text, &combiner->inputs[3], GL_RGB) == qfalse) {
                return qfalse;
            }
            return MatchShaderToken(text, ";", context);
        }
        if (strcmp(token, ";") == 0) {
            combiner->inputs[3] = zero;
            return qtrue;
        }
        goto unexpected_token;
    }

    Com_UngetToken();
    if (ParseNVRC_FinalMappedRegister(text, &first, GL_RGB) == qfalse) {
        return qfalse;
    }

    token = Com_Parse(text);
    if (strcmp(token, "+") == 0) {
        token = Com_Parse(text);
        if (coduo_crt_stricmp(token, "lerp") == 0) {
            combiner->inputs[3] = first;
            if (ParseNVRC_FinalLerp(text, combiner) == qfalse)
                return qfalse;
            return MatchShaderToken(text, ";", context);
        }

        Com_UngetToken();
        if (ParseNVRC_FinalMappedRegister(text, &second, GL_RGB) == qfalse) {
            return qfalse;
        }

        token = Com_Parse(text);
        if (strcmp(token, "*") == 0) {
            if (ParseNVRC_FinalMappedRegister(text, &third, GL_RGB) == qfalse) {
                return qfalse;
            }

            if (second.input == GL_E_TIMES_F_NV) {
                if (third.input == GL_E_TIMES_F_NV) {
                    ri.Printf(R_PRINT_WARNING,
                              "cannot multiply final_product by itself in "
                              "nvRegcombiners in shader '%s'\n",
                              rendererParsedShader.name);
                    return qfalse;
                }
                combiner->inputs[0] = third;
                combiner->inputs[1] = second;
            } else {
                combiner->inputs[0] = second;
                combiner->inputs[1] = third;
            }
            combiner->inputs[2] = zero;
            combiner->inputs[3] = first;
            return MatchShaderToken(text, ";", context);
        }

        if (strcmp(token, ";") == 0) {
            combiner->inputs[0] = zero;
            combiner->inputs[1] = zero;
            combiner->inputs[2] = first;
            combiner->inputs[3] = second;
            return qtrue;
        }
        goto unexpected_token;
    }

    if (strcmp(token, "*") == 0) {
        if (ParseNVRC_FinalMappedRegister(text, &second, GL_RGB) == qfalse) {
            return qfalse;
        }

        if (first.input == GL_E_TIMES_F_NV) {
            if (second.input == GL_E_TIMES_F_NV) {
                ri.Printf(R_PRINT_WARNING,
                          "cannot multiply final_product by itself in "
                          "nvRegcombiners in shader '%s'\n",
                          rendererParsedShader.name);
                return qfalse;
            }
            combiner->inputs[0] = second;
            combiner->inputs[1] = first;
        } else {
            combiner->inputs[0] = first;
            combiner->inputs[1] = second;
        }
        combiner->inputs[2] = zero;

        token = Com_Parse(text);
        if (strcmp(token, "+") == 0) {
            if (ParseNVRC_FinalMappedRegister(text, &combiner->inputs[3], GL_RGB) == qfalse) {
                return qfalse;
            }
            return MatchShaderToken(text, ";", context);
        }
        if (strcmp(token, ";") == 0) {
            combiner->inputs[3] = zero;
            return qtrue;
        }
        goto unexpected_token;
    }

    if (strcmp(token, ";") == 0) {
        combiner->inputs[0] = zero;
        combiner->inputs[1] = zero;
        combiner->inputs[2] = zero;
        combiner->inputs[3] = first;
        return qtrue;
    }

unexpected_token:
    ri.Printf(R_PRINT_WARNING,
              "unexpected token '%s' in final rgb function in "
              "nvRegCombiners in shader '%s'\n",
              token, rendererParsedShader.name);
    return qfalse;
}

/* Source: CoDUOMP.exe 0x004fc1a0..0x004fc3e6.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004fc1a0_004fc3e7.mcode.
 * Name and parameter order: exact same-module Mac symbol
 * ParseNVRC_FinalRgbAlpha. */
qboolean ParseNVRC_FinalRgbAlpha(char **text, renderer_final_combiner_t *combiner)
{
    qboolean rgbSeen = qfalse;
    qboolean alphaSeen = qfalse;

    for (;;) {
        const char *token = Com_Parse(text);
        if (strcmp(token, "}") == 0)
            return qtrue;

        if (coduo_crt_stricmp(token, "out.rgb") == 0) {
            if (rgbSeen != qfalse) {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: duplicate rgb in final stage in "
                          "nvRegCombiners in shader '%s'\n",
                          rendererParsedShader.name);
                return qfalse;
            }
            rgbSeen = qtrue;

            if (MatchShaderToken(text, "=", "out.rgb in nvRegCombiners") == qfalse) {
                return qfalse;
            }
            if (ParseNVRC_FinalRgbFunc(text, combiner) == qfalse)
                return qfalse;
            continue;
        }

        if (coduo_crt_stricmp(token, "out.a") == 0) {
            if (alphaSeen != qfalse) {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: duplicate alpha in final stage "
                          "nvRegCombiners in shader '%s'\n",
                          rendererParsedShader.name);
                return qfalse;
            }
            alphaSeen = qtrue;

            if (MatchShaderToken(text, "=", "out.a in nvRegCombiners") == qfalse) {
                return qfalse;
            }
            if (ParseNVRC_FinalMappedRegister(text, &combiner->inputs[6], GL_ALPHA) == qfalse) {
                return qfalse;
            }
            if (MatchShaderToken(text, ";", "out.a in nvRegCombiners") == qfalse) {
                return qfalse;
            }
            continue;
        }

        ri.Printf(R_PRINT_WARNING,
                  "WARNING: unexpected '%s' in final stage in "
                  "nvRegCombiners in shader '%s'\n",
                  token, rendererParsedShader.name);
        return qfalse;
    }
}

/* Source: CoDUOMP.exe 0x004fc3f0..0x004fc40d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004fc3f0_004fc40e.mcode.
 * Name and parameter order: exact same-module Mac symbol ParseNVRC_Final. */
qboolean ParseNVRC_Final(char **text, renderer_final_combiner_t *combiner)
{
    if (ParseNVRC_FinalMulSum(text, combiner) == qfalse)
        return qfalse;
    return ParseNVRC_FinalRgbAlpha(text, combiner);
}

/* Source: CoDUOMP.exe 0x004fc410..0x004fc459.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004fc410_004fc45a.mcode.
 * Name and parameter roles: exact same-module Mac symbol
 * ParseNVRC_ClearGeneralCombinerPortion. */
void ParseNVRC_ClearGeneralCombinerPortion(renderer_combiner_portion_t *portion, uint32_t componentUsage)
{
    for (int32_t inputIndex = 0; inputIndex < R_NV_COMBINER_INPUT_COUNT; ++inputIndex) {
        portion->inputs[inputIndex].input = GL_ZERO;
        portion->inputs[inputIndex].mapping = GL_UNSIGNED_IDENTITY_NV;
        portion->inputs[inputIndex].componentUsage = componentUsage;
    }

    portion->output.abOutput = GL_DISCARD_NV;
    portion->output.cdOutput = GL_DISCARD_NV;
    portion->output.sumOutput = GL_DISCARD_NV;
    portion->output.scale = GL_NONE;
    portion->output.bias = GL_NONE;
    portion->output.abDotProduct = qfalse;
    portion->output.cdDotProduct = qfalse;
    portion->output.muxSum = qfalse;
}

/* Source: CoDUOMP.exe 0x004fc460..0x004fc8dc.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004fc460_004fc8dd.mcode.
 * Name and parameter order: exact same-module Mac symbol
 * ParseNVRegCombiners.
 *
 * The parser builds the complete 0x600-byte NV register-combiner description
 * on its stack, then copies it into permanent renderer hunk storage. General
 * stages inherit the two global constants before optional per-stage constant
 * declarations are parsed. */
qboolean ParseNVRegCombiners(shaderStage_t *stage, char **text)
{
    renderer_register_combiners_t combiners;

    if (rendererShaderRequirements[SHADER_REQUIREMENT_REGISTER_COMBINERS_NV] == 0 &&
        rendererShaderRequirements[SHADER_REQUIREMENT_REGISTER_COMBINERS2_NV] == 0) {
        ri.Printf(R_PRINT_WARNING,
                  "WARNING: stage uses nvRegCombiners without "
                  "'requires GL_NV_register_combiners(2)' in shader '%s'\n",
                  rendererParsedShader.name);
        return qfalse;
    }

    if (MatchShaderToken(text, "{", "nvRegCombiners") == qfalse) {
        return qfalse;
    }

    for (int32_t stageIndex = 0; stageIndex < R_NV_GENERAL_COMBINER_LIMIT; ++stageIndex) {
        ParseNVRC_ClearGeneralCombinerPortion(&combiners.general[stageIndex].rgb, GL_RGB);
        ParseNVRC_ClearGeneralCombinerPortion(&combiners.general[stageIndex].alpha, GL_ALPHA);
    }

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    memset(&combiners.final, 0, sizeof(combiners.final));
    for (int32_t inputIndex = 0; inputIndex < R_NV_FINAL_COMBINER_INPUT_COUNT; ++inputIndex) {
        combiners.final.inputs[inputIndex].input = GL_ZERO;
        combiners.final.inputs[inputIndex].mapping = GL_UNSIGNED_IDENTITY_NV;
        combiners.final.inputs[inputIndex].componentUsage = inputIndex == R_NV_FINAL_ALPHA_INPUT_INDEX ? GL_ALPHA : GL_RGB;
    }
    combiners.final.clampColorSum = qfalse;

    memset(combiners.constantColors, 0, sizeof(combiners.constantColors));
    if (ParseNVRC_ConstColors(text, combiners.constantColors, qfalse) == qfalse) {
        return qfalse;
    }

    for (int32_t stageIndex = 0; stageIndex < R_NV_GENERAL_COMBINER_LIMIT; ++stageIndex) {
        memcpy(combiners.general[stageIndex].constantColors, combiners.constantColors, sizeof(combiners.constantColors));
    }

    combiners.generalCombinerCount = 0;
    for (;;) {
        const char *token = Com_Parse(text);
        if (strcmp(token, "{") != 0) {
            Com_UngetToken();
            break;
        }

        if (combiners.generalCombinerCount == R_NV_REGISTER_COMBINERS1_GENERAL_LIMIT &&
            rendererShaderRequirements[SHADER_REQUIREMENT_REGISTER_COMBINERS2_NV] == 0) {
            ri.Printf(R_PRINT_WARNING,
                      "WARNING: stage uses more that 2 general combiners "
                      "without 'requires GL_NV_register_combiners2' in "
                      "shader '%s'\n",
                      rendererParsedShader.name);
            return qfalse;
        }

        if (combiners.generalCombinerCount == R_NV_GENERAL_COMBINER_LIMIT) {
            ri.Printf(R_PRINT_WARNING,
                      "WARNING: stage uses more that %i general combiners "
                      "in shader '%s'\n",
                      R_NV_GENERAL_COMBINER_LIMIT, rendererParsedShader.name);
            return qfalse;
        }

        if (ParseNVRC_General(text, &combiners.general[combiners.generalCombinerCount]) == qfalse) {
            return qfalse;
        }
        ++combiners.generalCombinerCount;
    }

    if (ParseNVRC_Final(text, &combiners.final) == qfalse)
        return qfalse;

    combiners.perStageConstants = qfalse;
    for (int32_t stageIndex = 0; stageIndex < combiners.generalCombinerCount; ++stageIndex) {
        if (memcmp(combiners.constantColors, combiners.general[stageIndex].constantColors, sizeof(combiners.constantColors)) != 0) {
            combiners.perStageConstants = qtrue;
            break;
        }
    }

    stage->registerCombiners = (renderer_register_combiners_t *)ri.Hunk_Alloc(sizeof(*stage->registerCombiners));
    memcpy(stage->registerCombiners, &combiners, sizeof(combiners));
    return qtrue;
}

/* Source: CoDUOMP.exe 0x004fc8e0..0x004fc950.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004fc8e0_004fc951.mcode.
 * Name: exact same-module Mac symbol ParseATIFS_ConstReg. */
uint32_t ParseATIFS_ConstReg(char **text)
{
    const char *token = Com_Parse(text);

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (coduo_crt_strnicmp(token, "const", 5) == 0 && token[5] >= '0' && token[5] <= '8' && token[6] == '\0') {
        return GL_CON_0_ATI + (uint32_t)(token[5] - '0');
    }

    Com_UngetToken();
    return GL_NONE;
}

/* Source: CoDUOMP.exe 0x004fc960..0x004fc9c8.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004fc960_004fc9c9.mcode.
 * Name: exact same-module Mac symbol ParseATIFS_Reg. */
uint32_t ParseATIFS_Reg(char **text)
{
    const char *token = Com_Parse(text);

    if ((token[0] == 'r' || token[0] == 'R') && token[1] >= '0' && token[1] <= '5' && token[2] == '\0') {
        return GL_REG_0_ATI + (uint32_t)(token[1] - '0');
    }

    Com_UngetToken();
    return GL_NONE;
}

/* Source: CoDUOMP.exe 0x004fc9d0..0x004fca40.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004fc9d0_004fca41.mcode.
 * Name: exact same-module Mac symbol ParseATIFS_TexCoord. */
uint32_t ParseATIFS_TexCoord(char **text)
{
    const char *token = Com_Parse(text);

    if (coduo_crt_strnicmp(token, "tc", 2) == 0 && token[2] >= '0' && token[2] <= '7' && token[3] == '\0') {
        return GL_TEXTURE0_ARB + (uint32_t)(token[2] - '0');
    }

    Com_UngetToken();
    return GL_NONE;
}

/* Source: CoDUOMP.exe 0x004fca50..0x004fcbbf.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004fca50_004fcbc0.mcode.
 * Name and parameter order: exact same-module Mac symbol
 * ParseATIFS_ConstDefs.
 *
 * A non-constant token normally ends this leading declaration sequence, so
 * ParseATIFS_ConstReg pushes it back and this routine succeeds. The original
 * accepts const8 even though the later ATI upload loop only visits const0..7;
 * that parser behavior is preserved here. */
qboolean ParseATIFS_ConstDefs(char **text, renderer_atifs_constant_definition_t *constantDefinitions)
{
    const uint32_t constantRegister = ParseATIFS_ConstReg(text);
    if (constantRegister == GL_NONE)
        return qtrue;

    if (MatchShaderToken(text, "=", "atiFragmentShader constant definitions") == qfalse ||
        MatchShaderToken(text, "(", "atiFragmentShader constant definitions") == qfalse) {
        return qfalse;
    }

    renderer_atifs_constant_definition_t *definition = &constantDefinitions[constantRegister - GL_CON_0_ATI];

    for (int32_t component = 0; component < 4; ++component) {
        const char *token = Com_Parse(text);
        const long double componentRaw = (long double)atof(token);
        definition->value[component] = (float)componentRaw;

        /* 0x004fcafd stores the rounded float but retains the atof result on
         * the x87 stack for the lexical-zero and range comparisons. */
        if ((componentRaw == 0.0L && token[0] != '0' && token[0] != '.') || componentRaw < 0.0L || componentRaw > 1.0L) {
            ri.Printf(R_PRINT_WARNING,
                      "WARNING: expected a number from 0 to 1, found '%s' "
                      "instead in atiFragmentShader constant in shader %s\n",
                      token, rendererParsedShader.name);
            return qfalse;
        }

        if (MatchShaderToken(text, component < 3 ? "," : ")", "atiFragmentShader constant definitions") == qfalse) {
            return qfalse;
        }
    }

    if (MatchShaderToken(text, ";", "atiFragmentShader constant definitions") == qfalse) {
        return qfalse;
    }

    definition->defined = qtrue;
    return qtrue;
}

/* Source: CoDUOMP.exe 0x004fcbc0..0x004fd0ef.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004fcbc0_004fd0f0.mcode.
 * Name and parameter order: exact same-module Mac symbol
 * ParseATIFS_TexReads.
 *
 * Routing statements are speculative because the following fragment-op
 * grammar also begins with r#. A mark restores the parser when the statement
 * lacks '=' or a recognized routing opcode, leaving that token sequence for
 * ParseATIFS_FragOps. */
qboolean ParseATIFS_TexReads(char **text, renderer_atifs_texture_read_t *textureReads, qboolean allowTemporaryRegisterSource)
{
    for (;;) {
        com_parse_mark_t mark;
        const char *operation;
        uint32_t destination;
        uint32_t source;

        Com_ParseSetMark(text, &mark);
        destination = ParseATIFS_Reg(text);
        if (destination == GL_NONE)
            break;

        const int32_t destinationIndex = (int32_t)(destination - GL_REG_0_ATI);
        renderer_atifs_texture_read_t *textureRead = &textureReads[destinationIndex];

        if (strcmp(Com_Parse(text), "=") != 0) {
            Com_ParseReturnToMark(text, &mark);
            break;
        }

        operation = Com_Parse(text);
        if (coduo_crt_stricmp(operation, "copy") == 0) {
            textureRead->sampleMap = qfalse;
            textureRead->swizzle = GL_SWIZZLE_STR_ATI;
        } else if (coduo_crt_stricmp(operation, "copy_dr") == 0) {
            textureRead->sampleMap = qfalse;
            textureRead->swizzle = GL_SWIZZLE_STR_DR_ATI;
        } else if (coduo_crt_stricmp(operation, "tex") == 0) {
            textureRead->sampleMap = qtrue;
            textureRead->swizzle = GL_SWIZZLE_STR_ATI;
        } else if (coduo_crt_stricmp(operation, "tex_dr") == 0) {
            textureRead->sampleMap = qtrue;
            textureRead->swizzle = GL_SWIZZLE_STR_DR_ATI;
        } else if (coduo_crt_stricmp(operation, "copy_stq") == 0) {
            textureRead->sampleMap = qfalse;
            textureRead->swizzle = GL_SWIZZLE_STQ_ATI;
        } else if (coduo_crt_stricmp(operation, "copy_stq_dq") == 0) {
            textureRead->sampleMap = qfalse;
            textureRead->swizzle = GL_SWIZZLE_STQ_DQ_ATI;
        } else if (coduo_crt_stricmp(operation, "tex_stq") == 0) {
            textureRead->sampleMap = qtrue;
            textureRead->swizzle = GL_SWIZZLE_STQ_ATI;
        } else if (coduo_crt_stricmp(operation, "tex_stq_dq") == 0) {
            textureRead->sampleMap = qtrue;
            textureRead->swizzle = GL_SWIZZLE_STQ_DQ_ATI;
        } else {
            Com_ParseReturnToMark(text, &mark);
            break;
        }

        if (textureRead->source != GL_NONE) {
            ri.Printf(R_PRINT_WARNING,
                      "WARNING: already encountered a routing instruction "
                      "for r%i in atiFragmentShader in shader '%s'\n",
                      destinationIndex, rendererParsedShader.name);
            return qfalse;
        }

        if (MatchShaderToken(text, "(", "routing instruction of atiFragmentShader") == qfalse) {
            return qfalse;
        }

        source = ParseATIFS_TexCoord(text);
        if (source == GL_NONE) {
            source = ParseATIFS_Reg(text);
            if (source == GL_NONE) {
                const char *unexpected = Com_Parse(text);
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: expected r# or tc# instead of '%s' in "
                          "routing instruction of atiFragmentShader in "
                          "shader '%s'\n",
                          unexpected, rendererParsedShader.name);
                return qfalse;
            }

            if (textureRead->swizzle == GL_SWIZZLE_STQ_ATI || textureRead->swizzle == GL_SWIZZLE_STQ_DQ_ATI) {
                /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: cannot use _stq forms with r# in "
                          "routing instruction of atiFragmentShader in "
                          "shader '%s'\n",
                          operation, rendererParsedShader.name);
                return qfalse;
            }
            if (allowTemporaryRegisterSource == qfalse) {
                /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: cannot use r# in routing instruction "
                          "of first phase of atiFragmentShader in shader "
                          "'%s'\n",
                          operation, rendererParsedShader.name);
                return qfalse;
            }
        }

        textureRead->source = source;
        if (MatchShaderToken(text, ")", "routing instruction of atiFragmentShader") == qfalse ||
            MatchShaderToken(text, ";", "routing instruction of atiFragmentShader") == qfalse) {
            return qfalse;
        }
    }

    for (int32_t readIndex = 0; readIndex < R_ATIFS_TEMP_REGISTER_COUNT; ++readIndex) {
        const renderer_atifs_texture_read_t *read = &textureReads[readIndex];
        if (read->source < GL_TEXTURE0_ARB || read->source > GL_TEXTURE8_ARB) {
            continue;
        }

        for (int32_t previousIndex = 0; previousIndex < readIndex; ++previousIndex) {
            const renderer_atifs_texture_read_t *previous = &textureReads[previousIndex];
            if (previous->source != GL_NONE && previous->source == read->source && previous->swizzle != read->swizzle) {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: cannot use tc%c with different "
                          "instruction suffixes (such as _stq and _stq_dq) "
                          "in atiFragmentShader in shader '%s'\n",
                          (int)('0' + (read->source - GL_TEXTURE0_ARB)), rendererParsedShader.name);
                return qfalse;
            }
        }
    }

    return qtrue;
}

/* Source: CoDUOMP.exe 0x004fd0f0..0x004fd288.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004fd0f0_004fd28d.mcode.
 * Name and parameter order: exact same-module Mac symbol
 * ParseATIFS_DestReg.
 *
 * Invalid input is not itself an error: the token is pushed back and the zero
 * destination tells ParseATIFS_FragOps that its instruction sequence ended. */
void ParseATIFS_DestReg(char **text, uint32_t *destination, uint32_t *rgbWriteMask, qboolean *writesRgb, qboolean *writesAlpha)
{
    const char *token;

    *destination = GL_NONE;
    *rgbWriteMask = GL_NONE;
    *writesRgb = qfalse;
    *writesAlpha = qfalse;

    token = Com_Parse(text);
    if ((token[0] != 'r' && token[0] != 'R') || token[1] < '0' || token[1] > '5') {
        Com_UngetToken();
        return;
    }

    *destination = GL_REG_0_ATI + (uint32_t)(token[1] - '0');
    if (token[2] == '\0') {
        *writesRgb = qtrue;
        return;
    }
    if (token[2] != '.') {
        Com_UngetToken();
        return;
    }

    const char first = token[3];
    const char second = token[4];
    if (second == '\0') {
        if (first == 'r' || first == 'R') {
            *rgbWriteMask = GL_RED_BIT_ATI;
            *writesRgb = qtrue;
            return;
        }
        if (first == 'g' || first == 'G') {
            *rgbWriteMask = GL_GREEN_BIT_ATI;
            *writesRgb = qtrue;
            return;
        }
        if (first == 'b' || first == 'B') {
            *rgbWriteMask = GL_BLUE_BIT_ATI;
            *writesRgb = qtrue;
            return;
        }
        if (first == 'a' || first == 'A') {
            *writesAlpha = qtrue;
            return;
        }
    } else if ((second == 'a' || second == 'A') && token[5] == '\0') {
        /* The retail routine sets this output before validating the first
         * component, so malformed two-letter masks retain writesAlpha. */
        *writesAlpha = qtrue;
        if (first == 'r' || first == 'R') {
            *rgbWriteMask = GL_RED_BIT_ATI;
            *writesRgb = qtrue;
            return;
        }
        if (first == 'g' || first == 'G') {
            *rgbWriteMask = GL_GREEN_BIT_ATI;
            *writesRgb = qtrue;
            return;
        }
        if (first == 'b' || first == 'B') {
            *rgbWriteMask = GL_BLUE_BIT_ATI;
            *writesRgb = qtrue;
            return;
        }
    } else if (coduo_crt_stricmp(&token[3], "rgba") == 0) {
        *rgbWriteMask = GL_RGB;
        *writesRgb = qtrue;
        *writesAlpha = qtrue;
        return;
    } else if (coduo_crt_stricmp(&token[3], "rgb") == 0) {
        *rgbWriteMask = GL_RGB;
        *writesRgb = qtrue;
        return;
    }

    Com_UngetToken();
}

/* Source: CoDUOMP.exe 0x004fd290..0x004fd41e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004fd290_004fd41f.mcode.
 * Name and parameter order: exact same-module Mac symbol
 * ParseATIFS_ArgReg. */
qboolean ParseATIFS_ArgReg(char **text, renderer_atifs_argument_t *argument)
{
    const char *token = Com_Parse(text);
    int32_t prefixLength;

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if ((token[0] == 'r' || token[0] == 'R') && token[1] >= '0' && token[1] <= '6') {
        argument->source = GL_REG_0_ATI + (uint32_t)(token[1] - '0');
        prefixLength = 2;
    } else if (coduo_crt_strnicmp(token, "const", 5) == 0 && token[5] >= '0' && token[5] <= '8') {
        argument->source = GL_CON_0_ATI + (uint32_t)(token[5] - '0');
        prefixLength = 6;
    } else if (coduo_crt_strnicmp(token, "col0", 4) == 0) {
        argument->source = GL_PRIMARY_COLOR_ARB;
        prefixLength = 4;
    } else if (coduo_crt_strnicmp(token, "col1", 4) == 0) {
        argument->source = GL_SECONDARY_INTERPOLATOR_ATI;
        prefixLength = 4;
    } else {
        goto invalid_argument;
    }

    if (token[prefixLength] == '\0')
        return qtrue;

    if (token[prefixLength] == '.' && token[prefixLength + 2] == '\0') {
        const char component = token[prefixLength + 1];
        if (component == 'r' || component == 'R') {
            argument->componentUsage = GL_RED;
            return qtrue;
        }
        if (component == 'g' || component == 'G') {
            argument->componentUsage = GL_GREEN;
            return qtrue;
        }
        if (component == 'b' || component == 'B') {
            argument->componentUsage = GL_BLUE;
            return qtrue;
        }
        if (component == 'a' || component == 'A') {
            argument->componentUsage = GL_ALPHA;
            return qtrue;
        }
    }

    if (token[prefixLength] == '.') {
        ri.Printf(R_PRINT_WARNING,
                  "WARNING: expected .r, .g, .b or .a instead of '.%s' "
                  "in function argument in atiFragmentShader in shader "
                  "'%s'\n",
                  &token[prefixLength + 1], rendererParsedShader.name);
        return qfalse;
    }

invalid_argument:
    ri.Printf(R_PRINT_WARNING,
              "WARNING: expected r#, const#, col0 or col1 instead of '%s' "
              "in function argument in atiFragmentShader in shader '%s'\n",
              token, rendererParsedShader.name);
    return qfalse;
}

/* Source: CoDUOMP.exe 0x004fd420..0x004fdd03.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004fd420_004fdd03.mcode.
 * Name and parameter order: exact same-module Mac symbol
 * ParseATIFS_FunctionArgs.
 *
 * ATI source modifiers are applied in complement, bias, scale, negate order.
 * The accepted expressions below are reduced to the exact modifier masks
 * emitted by the original parser. */
qboolean ParseATIFS_FunctionArgs(char **text, renderer_atifs_argument_t *arguments, int32_t argumentCount,
                                 qboolean *usesPrimaryOrSecondaryColor)
{
    for (int32_t argumentIndex = 0; argumentIndex < argumentCount; ++argumentIndex) {
        renderer_atifs_argument_t *argument = &arguments[argumentIndex];
        const char *token = Com_Parse(text);

        if (strcmp(token, "-") == 0) {
            token = Com_Parse(text);
            if (strcmp(token, "2") == 0) {
                token = Com_Parse(text);
                if (strcmp(token, "*") == 0) {
                    if (ParseATIFS_ArgReg(text, argument) == qfalse)
                        return qfalse;
                    argument->modifier = GL_2X_BIT_ATI | GL_NEGATE_BIT_ATI;
                } else {
                    Com_UngetToken();
                    argument->source = GL_ONE;
                    argument->modifier = GL_2X_BIT_ATI | GL_NEGATE_BIT_ATI;
                }
            } else if (strcmp(token, "0.5") == 0 || strcmp(token, ".5") == 0) {
                argument->source = GL_ZERO;
                argument->modifier = GL_BIAS_BIT_ATI;
            } else if (strcmp(token, "1") == 0) {
                argument->source = GL_ONE;
                argument->modifier = GL_NEGATE_BIT_ATI;
            } else {
                Com_UngetToken();
                if (ParseATIFS_ArgReg(text, argument) == qfalse)
                    return qfalse;

                token = Com_Parse(text);
                if (strcmp(token, "*") == 0) {
                    token = Com_Parse(text);
                    if (strcmp(token, "2") != 0) {
                        ri.Printf(R_PRINT_WARNING,
                                  "WARNING: expected 2 instead of '%s' in "
                                  "function argument in atiFragmentShader in "
                                  "shader '%s'\n",
                                  token, rendererParsedShader.name);
                        return qfalse;
                    }
                    argument->modifier = GL_2X_BIT_ATI | GL_NEGATE_BIT_ATI;
                } else {
                    Com_UngetToken();
                    argument->modifier = GL_NEGATE_BIT_ATI;
                }
            }
        } else if (strcmp(token, "2") == 0) {
            token = Com_Parse(text);
            if (strcmp(token, "*") == 0) {
                if (ParseATIFS_ArgReg(text, argument) == qfalse)
                    return qfalse;

                token = Com_Parse(text);
                if (strcmp(token, "-") == 0) {
                    token = Com_Parse(text);
                    if (strcmp(token, "1") == 0) {
                        argument->modifier = GL_2X_BIT_ATI | GL_BIAS_BIT_ATI;
                    } else if (strcmp(token, "2") == 0) {
                        /* Original quirk: this spelling receives the same
                         * complement-and-scale mask as "2 - 2 * arg". */
                        argument->modifier = GL_2X_BIT_ATI | GL_COMP_BIT_ATI;
                    } else {
                        ri.Printf(R_PRINT_WARNING,
                                  "WARNING: expected N to be 1 or 2 instead "
                                  "of '%s' in 2 * arg - N in "
                                  "atiFragmentShader in shader '%s'\n",
                                  token, rendererParsedShader.name);
                        return qfalse;
                    }
                } else {
                    Com_UngetToken();
                    argument->modifier = GL_2X_BIT_ATI;
                }
            } else if (strcmp(token, "-") == 0) {
                token = Com_Parse(text);
                if (strcmp(token, "2") == 0) {
                    if (MatchShaderToken(text, "*",
                                         "2 - 2 * arg in fragment argument in "
                                         "atiFragmentShader") == qfalse ||
                        ParseATIFS_ArgReg(text, argument) == qfalse) {
                        return qfalse;
                    }
                } else {
                    Com_UngetToken();
                    if (ParseATIFS_ArgReg(text, argument) == qfalse ||
                        MatchShaderToken(text, "*",
                                         "2 - arg * 2 in fragment argument in "
                                         "atiFragmentShader") == qfalse ||
                        MatchShaderToken(text, "2",
                                         "2 - arg * 2 in fragment argument in "
                                         "atiFragmentShader") == qfalse) {
                        return qfalse;
                    }
                }
                argument->modifier = GL_2X_BIT_ATI | GL_COMP_BIT_ATI;
            } else {
                Com_UngetToken();
                argument->source = GL_ONE;
                argument->modifier = GL_2X_BIT_ATI;
            }
        } else if (strcmp(token, "1") == 0) {
            token = Com_Parse(text);
            if (strcmp(token, "-") == 0) {
                token = Com_Parse(text);
                if (strcmp(token, "2") == 0) {
                    if (MatchShaderToken(text, "*",
                                         "1 - 2 * arg in fragment argument in "
                                         "atiFragmentShader") == qfalse ||
                        ParseATIFS_ArgReg(text, argument) == qfalse) {
                        return qfalse;
                    }
                    argument->modifier = GL_2X_BIT_ATI | GL_COMP_BIT_ATI | GL_NEGATE_BIT_ATI;
                } else {
                    Com_UngetToken();
                    if (ParseATIFS_ArgReg(text, argument) == qfalse)
                        return qfalse;

                    token = Com_Parse(text);
                    if (strcmp(token, "*") == 0) {
                        if (MatchShaderToken(text, "2",
                                             "1 - arg * 2 in fragment argument in "
                                             "atiFragmentShader") == qfalse) {
                            return qfalse;
                        }
                        argument->modifier = GL_2X_BIT_ATI | GL_COMP_BIT_ATI | GL_NEGATE_BIT_ATI;
                    } else {
                        Com_UngetToken();
                        argument->modifier = GL_COMP_BIT_ATI;
                    }
                }
            } else {
                Com_UngetToken();
                argument->source = GL_ONE;
            }
        } else if (strcmp(token, "0.5") == 0 || strcmp(token, ".5") == 0) {
            token = Com_Parse(text);
            if (strcmp(token, "-") == 0) {
                if (ParseATIFS_ArgReg(text, argument) == qfalse)
                    return qfalse;
                argument->modifier = GL_BIAS_BIT_ATI | GL_NEGATE_BIT_ATI;
            } else {
                Com_UngetToken();
                argument->source = GL_ONE;
                argument->modifier = GL_BIAS_BIT_ATI;
            }
        } else if (strcmp(token, "0") == 0) {
            argument->source = GL_ZERO;
        } else {
            Com_UngetToken();
            if (ParseATIFS_ArgReg(text, argument) == qfalse)
                return qfalse;

            token = Com_Parse(text);
            if (strcmp(token, "-") == 0) {
                token = Com_Parse(text);
                if (strcmp(token, "0.5") == 0 || strcmp(token, ".5") == 0) {
                    argument->modifier = GL_BIAS_BIT_ATI;
                } else if (strcmp(token, "1") == 0) {
                    argument->modifier = GL_COMP_BIT_ATI | GL_NEGATE_BIT_ATI;
                } else {
                    ri.Printf(R_PRINT_WARNING,
                              "WARNING: expected N to be 1 or .5 instead of "
                              "'%s' in arg - N in atiFragmentShader in "
                              "shader '%s'\n",
                              token, rendererParsedShader.name);
                    return qfalse;
                }
            } else if (strcmp(token, "*") == 0) {
                if (MatchShaderToken(text, "2",
                                     "arg * 2 in function argument in "
                                     "atiFragmentShader") == qfalse) {
                    return qfalse;
                }

                token = Com_Parse(text);
                if (strcmp(token, "-") == 0) {
                    token = Com_Parse(text);
                    if (strcmp(token, "1") == 0) {
                        argument->modifier = GL_2X_BIT_ATI | GL_BIAS_BIT_ATI;
                    } else if (strcmp(token, "2") == 0) {
                        /* Preserves the same original quirk as the
                         * commuted "2 * arg - 2" spelling above. */
                        argument->modifier = GL_2X_BIT_ATI | GL_COMP_BIT_ATI;
                    } else {
                        ri.Printf(R_PRINT_WARNING,
                                  "WARNING: expected N to be 1 or 2 instead "
                                  "of '%s' in arg * 2 - N in "
                                  "atiFragmentShader in shader '%s'\n",
                                  token, rendererParsedShader.name);
                        return qfalse;
                    }
                } else {
                    Com_UngetToken();
                    argument->modifier = GL_2X_BIT_ATI;
                }
            } else {
                Com_UngetToken();
            }
        }

        if (MatchShaderToken(text, argumentIndex == argumentCount - 1 ? ")" : ",", "instruction argument list in atiFragmentShader") ==
            qfalse) {
            return qfalse;
        }

        if (argument->source == GL_PRIMARY_COLOR_ARB || argument->source == GL_SECONDARY_INTERPOLATOR_ATI) {
            *usesPrimaryOrSecondaryColor = qtrue;
        }
    }

    return qtrue;
}

/* Source: CoDUOMP.exe 0x004fdd10..0x004fe553.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004fdd10_004fe553.mcode.
 * Name and signature: same-module Mac symbol ParseATIFS_FragOps, with the
 * three-argument Windows call sites proving the corrected parameter list.
 *
 * Color and alpha instructions are packed into the same hardware operation
 * pair when their write lanes permit it. A destination token with no explicit
 * mask and no recognized operation is speculative: restoring the parser mark
 * lets a following phase consume that token as a routing instruction. */
qboolean ParseATIFS_FragOps(char **text, renderer_atifs_operation_pair_t *operationPairs, qboolean *usesPrimaryOrSecondaryColor)
{
    int32_t pairIndex = 0;
    qboolean pairHasColor = qfalse;
    qboolean forceNextPair = qfalse;

    for (;;) {
        com_parse_mark_t mark;
        uint32_t destination;
        uint32_t rgbWriteMask;
        qboolean writesRgb;
        qboolean writesAlpha;
        uint32_t destinationModifier = GL_NONE;
        qboolean clamped = qfalse;
        qboolean prescaled = qfalse;
        uint32_t operation;
        int32_t argumentCount;
        const char *token;

        Com_ParseSetMark(text, &mark);
        ParseATIFS_DestReg(text, &destination, &rgbWriteMask, &writesRgb, &writesAlpha);
        if (destination == GL_NONE)
            return qtrue;

        if (MatchShaderToken(text, "=", "fragment instruction of atiFragmentShader") == qfalse) {
            return qfalse;
        }

        token = Com_Parse(text);
        if (coduo_crt_stricmp(token, "clamp") == 0) {
            clamped = qtrue;
            if (MatchShaderToken(text, "(",
                                 "clamp fragment instruction of "
                                 "atiFragmentShader") == qfalse) {
                return qfalse;
            }
            token = Com_Parse(text);
        }

        if (token[1] == '\0' && (token[0] == '2' || token[0] == '4' || token[0] == '8')) {
            if (token[0] == '2')
                destinationModifier = GL_2X_BIT_ATI;
            else if (token[0] == '4')
                destinationModifier = GL_4X_BIT_ATI;
            else
                destinationModifier = GL_8X_BIT_ATI;

            prescaled = qtrue;
            if (MatchShaderToken(text, "*",
                                 "prescaled fragment instruction of "
                                 "atiFragmentShader") == qfalse) {
                return qfalse;
            }
            token = Com_Parse(text);
        }

        if (coduo_crt_stricmp(token, "mov") == 0) {
            operation = GL_MOV_ATI;
            argumentCount = 1;
        } else if (coduo_crt_stricmp(token, "add") == 0) {
            operation = GL_ADD_ATI;
            argumentCount = 2;
        } else if (coduo_crt_stricmp(token, "sub") == 0) {
            operation = GL_SUB_ATI;
            argumentCount = 2;
        } else if (coduo_crt_stricmp(token, "mul") == 0) {
            operation = GL_MUL_ATI;
            argumentCount = 2;
        } else if (coduo_crt_stricmp(token, "dot3") == 0) {
            if (writesRgb == qfalse) {
                /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: dot3 cannot write only alpha in "
                          "atiTexShader in shader '%s'\n",
                          rendererParsedShader.name);
                return qfalse;
            }
            operation = GL_DOT3_ATI;
            argumentCount = 2;
        } else if (coduo_crt_stricmp(token, "dot4") == 0) {
            if (writesRgb == qfalse) {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: dot4 cannot write only alpha in "
                          "atiTexShader in shader '%s'\n",
                          rendererParsedShader.name);
                return qfalse;
            }
            operation = GL_DOT4_ATI;
            argumentCount = 2;
        } else if (coduo_crt_stricmp(token, "mad") == 0) {
            operation = GL_MAD_ATI;
            argumentCount = 3;
        } else if (coduo_crt_stricmp(token, "lerp") == 0) {
            operation = GL_LERP_ATI;
            argumentCount = 3;
        } else if (coduo_crt_stricmp(token, "cnd") == 0) {
            operation = GL_CND_ATI;
            argumentCount = 3;
        } else if (coduo_crt_stricmp(token, "cnd0") == 0) {
            operation = GL_CND0_ATI;
            argumentCount = 3;
        } else if (coduo_crt_stricmp(token, "dot2_add") == 0) {
            if (writesRgb == qfalse) {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: dot2_add cannot write only alpha in "
                          "atiTexShader in shader '%s'\n",
                          rendererParsedShader.name);
                return qfalse;
            }
            operation = GL_DOT2_ADD_ATI;
            argumentCount = 3;
        } else {
            if (clamped == qfalse && prescaled == qfalse && rgbWriteMask == GL_NONE) {
                Com_ParseReturnToMark(text, &mark);
                return qtrue;
            }
            ri.Printf(R_PRINT_WARNING,
                      "WARNING: unknown function '%s' in "
                      "atiFragmentShader in shader '%s'\n",
                      token, rendererParsedShader.name);
            return qfalse;
        }

        if ((writesRgb != qfalse && pairHasColor != qfalse) || forceNextPair != qfalse) {
            ++pairIndex;
            ++operationPairs;
            if (pairIndex >= R_ATIFS_OPERATION_PAIR_COUNT) {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: more than %i merged operations in "
                          "atiFragmentShader in shader '%s'\n",
                          R_ATIFS_OPERATION_PAIR_COUNT, rendererParsedShader.name);
                return qfalse;
            }
        }

        if (MatchShaderToken(text, "(", "fragment instruction of atiFragmentShader") == qfalse) {
            return qfalse;
        }

        renderer_atifs_instruction_t *instruction = writesAlpha != qfalse ? &operationPairs->alpha : &operationPairs->color;
        if (ParseATIFS_FunctionArgs(text, instruction->arguments, argumentCount, usesPrimaryOrSecondaryColor) == qfalse) {
            return qfalse;
        }

        if (prescaled == qfalse) {
            token = Com_Parse(text);
            if (token[1] == '\0' && (token[0] == '*' || token[0] == '/')) {
                const qboolean multiply = token[0] == '*';
                token = Com_Parse(text);
                if (token[1] != '\0' || (token[0] != '2' && token[0] != '4' && token[0] != '8')) {
                    ri.Printf(R_PRINT_WARNING,
                              "WARNING: bad scale '%s' in "
                              "atiFragmentShader in shader '%s'\n",
                              token, rendererParsedShader.name);
                    return qfalse;
                }

                if (token[0] == '2') {
                    destinationModifier = multiply != qfalse ? GL_2X_BIT_ATI : GL_HALF_BIT_ATI;
                } else if (token[0] == '4') {
                    destinationModifier = multiply != qfalse ? GL_4X_BIT_ATI : GL_QUARTER_BIT_ATI;
                } else {
                    destinationModifier = multiply != qfalse ? GL_8X_BIT_ATI : GL_EIGHTH_BIT_ATI;
                }
            } else {
                Com_UngetToken();
            }
        }

        if (clamped != qfalse) {
            if (MatchShaderToken(text, ")",
                                 "clamped fragment instruction of "
                                 "atiFragmentShader") == qfalse) {
                return qfalse;
            }
            destinationModifier |= GL_SATURATE_BIT_ATI;
        }

        if (MatchShaderToken(text, ";", "fragment instruction of atiFragmentShader") == qfalse) {
            return qfalse;
        }

        instruction->operation = operation;
        instruction->destination = destination;
        instruction->destinationModifier = destinationModifier;

        if (writesRgb != qfalse && writesAlpha != qfalse)
            operationPairs->color = operationPairs->alpha;

        if (writesRgb != qfalse) {
            operationPairs->color.destinationMask = rgbWriteMask == GL_RGB ? GL_NONE : rgbWriteMask;
        }

        pairHasColor = writesRgb;
        forceNextPair = writesAlpha != qfalse || operation == GL_DOT4_ATI;
    }
}

/* Source: CoDUOMP.exe 0x004fe560..0x004fe58f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004fe560_004fe58f.mcode.
 * Name and signature: exact same-module Mac symbol ParseATIFS_Phase. */
qboolean ParseATIFS_Phase(char **text, renderer_atifs_phase_t *phase, qboolean allowTemporaryRegisterSource,
                          qboolean *usesPrimaryOrSecondaryColor)
{
    if (ParseATIFS_TexReads(text, phase->textureReads, allowTemporaryRegisterSource) == qfalse) {
        return qfalse;
    }

    return ParseATIFS_FragOps(text, phase->operationPairs, usesPrimaryOrSecondaryColor);
}

/* Source: CoDUOMP.exe 0x004fe590..0x004fe633.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004fe590_004fe633.mcode.
 * Name and signature: exact same-module Mac symbol ATIFS_ColorOp. */
void ATIFS_ColorOp(const renderer_atifs_instruction_t *instruction)
{
    const renderer_atifs_argument_t *arguments = instruction->arguments;

    if (instruction->operation == GL_MOV_ATI) {
        qglColorFragmentOp1ATI(instruction->operation, instruction->destination, instruction->destinationMask,
                               instruction->destinationModifier, arguments[0].source, arguments[0].componentUsage, arguments[0].modifier);
    } else if (instruction->operation >= GL_ADD_ATI && instruction->operation <= GL_DOT4_ATI) {
        qglColorFragmentOp2ATI(instruction->operation, instruction->destination, instruction->destinationMask,
                               instruction->destinationModifier, arguments[0].source, arguments[0].componentUsage, arguments[0].modifier,
                               arguments[1].source, arguments[1].componentUsage, arguments[1].modifier);
    } else if (instruction->operation >= GL_MAD_ATI && instruction->operation <= GL_DOT2_ADD_ATI) {
        qglColorFragmentOp3ATI(instruction->operation, instruction->destination, instruction->destinationMask,
                               instruction->destinationModifier, arguments[0].source, arguments[0].componentUsage, arguments[0].modifier,
                               arguments[1].source, arguments[1].componentUsage, arguments[1].modifier, arguments[2].source,
                               arguments[2].componentUsage, arguments[2].modifier);
    }
}

/* Source: CoDUOMP.exe 0x004fe650..0x004fe6e7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004fe650_004fe6e7.mcode.
 * Name and signature: exact same-module Mac symbol ATIFS_AlphaOp. */
void ATIFS_AlphaOp(const renderer_atifs_instruction_t *instruction)
{
    const renderer_atifs_argument_t *arguments = instruction->arguments;

    if (instruction->operation == GL_MOV_ATI) {
        qglAlphaFragmentOp1ATI(instruction->operation, instruction->destination, instruction->destinationModifier, arguments[0].source,
                               arguments[0].componentUsage, arguments[0].modifier);
    } else if (instruction->operation >= GL_ADD_ATI && instruction->operation <= GL_DOT4_ATI) {
        qglAlphaFragmentOp2ATI(instruction->operation, instruction->destination, instruction->destinationModifier, arguments[0].source,
                               arguments[0].componentUsage, arguments[0].modifier, arguments[1].source, arguments[1].componentUsage,
                               arguments[1].modifier);
    } else if (instruction->operation >= GL_MAD_ATI && instruction->operation <= GL_DOT2_ADD_ATI) {
        qglAlphaFragmentOp3ATI(instruction->operation, instruction->destination, instruction->destinationModifier, arguments[0].source,
                               arguments[0].componentUsage, arguments[0].modifier, arguments[1].source, arguments[1].componentUsage,
                               arguments[1].modifier, arguments[2].source, arguments[2].componentUsage, arguments[2].modifier);
    }
}

/* Source: CoDUOMP.exe 0x004fe710..0x004fe9aa.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004fe710_004fe9aa.mcode.
 * Name and signature: exact same-module Mac symbol
 * ParseATIFragmentShader, with the Windows caller proving stage/text/compile
 * parameter roles.
 *
 * Primary and secondary interpolated colors may occur only in the last
 * phase. Their use in phase zero therefore terminates a one-phase program;
 * otherwise the parser requires a second phase before the closing brace. */
qboolean ParseATIFragmentShader(shaderStage_t *stage, char **text, qboolean compileShader)
{
    renderer_atifs_program_t program;
    qboolean usesPrimaryOrSecondaryColor = qfalse;
    int32_t phaseCount;

    if (rendererShaderRequirements[SHADER_REQUIREMENT_FRAGMENT_SHADER_ATI] == 0) {
        ri.Printf(R_PRINT_WARNING,
                  "WARNING: shader '%s' uses atiFragmentShader without "
                  "'requires GL_ATI_fragment_shader'\n",
                  rendererParsedShader.name);
        return qfalse;
    }

    if (MatchShaderToken(text, "{", "start of atiFragmentShader") == qfalse) {
        return qfalse;
    }

    memset(&program, 0, sizeof(program));
    if (ParseATIFS_ConstDefs(text, program.constantDefinitions) == qfalse ||
        ParseATIFS_Phase(text, &program.parsed.phases[0], qfalse, &usesPrimaryOrSecondaryColor) == qfalse) {
        return qfalse;
    }

    if (usesPrimaryOrSecondaryColor != qfalse) {
        const char *token = Com_Parse(text);
        if (strcmp(token, "}") != 0) {
            ri.Printf(R_PRINT_WARNING,
                      "WARNING: missing '}' at end of atiFragmentShader... "
                      "might be because 'col0' or 'col1' was referenced in "
                      "first phase in shader %s\n",
                      rendererParsedShader.name);
            return qfalse;
        }
        phaseCount = 1;
    } else {
        if (ParseATIFS_Phase(text, &program.parsed.phases[1], qtrue, &usesPrimaryOrSecondaryColor) == qfalse ||
            MatchShaderToken(text, "}", "end of atiFragmentShader") == qfalse) {
            return qfalse;
        }
        phaseCount = 2;
    }

    if (compileShader == qfalse)
        return qtrue;

    const uint32_t fragmentShader = (uint32_t)tr.fragmentShaderCount + 1u;
    (void)qglGetError();
    qglBindFragmentShaderATI(fragmentShader);
    qglBeginFragmentShaderATI();

    for (int32_t constantIndex = 0; constantIndex < 8; ++constantIndex) {
        const renderer_atifs_constant_definition_t *constant = &program.parsed.uploadConstants[constantIndex];
        if (constant->defined != qfalse) {
            qglSetFragmentShaderConstantATI(GL_CON_0_ATI + (uint32_t)constantIndex, constant->value);
        }
    }

    for (int32_t phaseIndex = 0; phaseIndex < phaseCount; ++phaseIndex) {
        const renderer_atifs_phase_t *phase = &program.parsed.phases[phaseIndex];

        for (int32_t registerIndex = 0; registerIndex < R_ATIFS_TEMP_REGISTER_COUNT; ++registerIndex) {
            const renderer_atifs_texture_read_t *textureRead = &phase->textureReads[registerIndex];
            if (textureRead->source == GL_NONE)
                continue;

            const uint32_t destination = GL_REG_0_ATI + (uint32_t)registerIndex;
            if (textureRead->sampleMap != qfalse) {
                qglSampleMapATI(destination, textureRead->source, textureRead->swizzle);
            } else {
                qglPassTexCoordATI(destination, textureRead->source, textureRead->swizzle);
            }
        }

        for (int32_t pairIndex = 0; pairIndex < R_ATIFS_OPERATION_PAIR_COUNT; ++pairIndex) {
            const renderer_atifs_operation_pair_t *pair = &phase->operationPairs[pairIndex];
            if (pair->color.operation == GL_NONE && pair->alpha.operation == GL_NONE) {
                break;
            }
            ATIFS_ColorOp(&pair->color);
            ATIFS_AlphaOp(&pair->alpha);
        }
    }

    qglEndFragmentShaderATI();
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    const uint32_t error = qglGetError();
    if (error != GL_NO_ERROR) {
        qglBindFragmentShaderATI(0);
        qglDeleteFragmentShaderATI(fragmentShader);
        ri.Printf(R_PRINT_WARNING,
                  "WARNING: glGetError() returned 0x%04X when compiling "
                  "atiFragmentShader in shader %s\n",
                  error, rendererParsedShader.name);
        return qfalse;
    }

    stage->fragmentShaderATI = fragmentShader;
    qglBindFragmentShaderATI(0);
    ++tr.fragmentShaderCount;
    return qtrue;
}

/* Source: CoDUOMP.exe 0x004fe9b0..0x004fea29.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004fe9b0_004fea29.mcode.
 * Name and signature: exact same-module Mac symbol ParseVertexProgram. */
qboolean ParseVertexProgram(shaderStage_t *stage, char **text)
{
    const char *name = Com_ParseOnLine(text);
    stage->vertexProgram = R_FindVertexProgram(name);
    if (stage->vertexProgram == NULL) {
        ri.Printf(R_PRINT_WARNING, "WARNING: couldn't load vertex program '%s' in shader '%s'\n", name, rendererParsedShader.name);
        return qfalse;
    }

    return qtrue;
}

/* Source: CoDUOMP.exe 0x004f9000..0x004f9417.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f9000_004f9418.mcode.
 * Name: exact same-module Mac symbol ParseStageRequirementsOperand.
 *
 * Capability names are converted to their current numeric values while their
 * dependency indices are retained for the later shader-feature audit. The
 * original accepts loose numeric spellings (including repeated decimal
 * points) and only inspects the first character of a cvar name. */
qboolean ParseStageRequirementsOperand(char **text, shader_requirement_operand_t *operand)
{
    const char *value;
    char *token;
    int32_t capabilityValue = 0;

    operand->type = SHADER_REQUIREMENT_OPERAND_NONE;
    operand->dependency = -1;
    operand->leadingNotCount = 0;

    for (;;) {
        token = Com_ParseOnLine(text);
        if (token[0] == '\0')
            return qfalse;

        while (*token == '!') {
            ++token;
            ++operand->leadingNotCount;
            if (operand->leadingNotCount == 0)
                return qfalse;
        }
        if (*token != '\0')
            break;
    }

    if (Q_stricmpn(token, "GL_MAX_TEXTURE_UNITS_ARB", R_SHADER_TOKEN_COMPARE_LIMIT) == 0) {
        operand->dependency = SHADER_REQUIREMENT_MAX_TEXTURE_UNITS_ARB;
        capabilityValue = qglActiveTextureARB != NULL ? glConfig.maxActiveTextures : 1;
    } else if (Q_stricmpn(token, "GL_ARB_texture_cube_map", R_SHADER_TOKEN_COMPARE_LIMIT) == 0) {
        operand->dependency = SHADER_REQUIREMENT_TEXTURE_CUBE_MAP_ARB;
        capabilityValue = glConfig.cubeMapAvailable;
    } else if (Q_stricmpn(token, "GL_ARB_texture_env_add", R_SHADER_TOKEN_COMPARE_LIMIT) == 0) {
        operand->dependency = SHADER_REQUIREMENT_TEXTURE_ENV_ADD_ARB;
        capabilityValue = glConfig.textureEnvAddAvailable;
    } else if (Q_stricmpn(token, "GL_ARB_texture_env_combine", R_SHADER_TOKEN_COMPARE_LIMIT) == 0) {
        operand->dependency = SHADER_REQUIREMENT_TEXTURE_ENV_COMBINE_ARB;
        capabilityValue = glConfig.textureEnvCombineAvailable;
    } else if (Q_stricmpn(token, "GL_ARB_texture_env_dot3", R_SHADER_TOKEN_COMPARE_LIMIT) == 0) {
        operand->dependency = SHADER_REQUIREMENT_TEXTURE_ENV_DOT3_ARB;
        capabilityValue = glConfig.textureEnvDot3Available;
    } else if (Q_stricmp(token, "GL_ARB_vertex_program") == 0) {
        operand->dependency = SHADER_REQUIREMENT_VERTEX_PROGRAM_ARB;
        capabilityValue = glConfig.vertexProgramAvailable;
    } else if (Q_stricmp(token, "GL_NV_register_combiners") == 0) {
        operand->dependency = SHADER_REQUIREMENT_REGISTER_COMBINERS_NV;
        capabilityValue = glConfig.registerCombinerMode >= R_REGISTER_COMBINERS_NV;
    } else if (Q_stricmp(token, "GL_NV_register_combiners2") == 0) {
        operand->dependency = SHADER_REQUIREMENT_REGISTER_COMBINERS2_NV;
        capabilityValue = glConfig.registerCombinerMode >= R_REGISTER_COMBINERS_NV2;
    } else if (Q_stricmp(token, "GL_NV_texture_shader") == 0) {
        operand->dependency = SHADER_REQUIREMENT_TEXTURE_SHADER_NV;
        capabilityValue = glConfig.textureShaderNVAvailable;
    } else if (Q_stricmp(token, "GL_ATI_fragment_shader") == 0) {
        operand->dependency = SHADER_REQUIREMENT_FRAGMENT_SHADER_ATI;
        capabilityValue = glConfig.fragmentShaderATIAvailable;
    }

    if (operand->dependency >= 0) {
        operand->type = SHADER_REQUIREMENT_OPERAND_NUMBER;
        Com_sprintf(operand->value, sizeof(operand->value), "%i", capabilityValue);
        value = operand->value;
    } else if (Q_stricmp(token, "cvar") == 0) {
        cvar_t *cvar;

        token = Com_ParseOnLine(text);
        if (coduo_crt_isalnum((unsigned char)token[0]) == 0 && token[0] != '_') {
            return qfalse;
        }

        cvar = ri.Cvar_Get(token, "", CVAR_LATCH);
        if (Q_stricmp(cvar->name, "BADNAME") == 0)
            return qfalse;
        value = cvar->string;
    } else {
        value = token;
    }

    operand->type = SHADER_REQUIREMENT_OPERAND_NUMBER;
    {
        const size_t length = strlen(value);
        size_t index = 0;

        if (length != 0 && (value[0] == '-' || value[0] == '+')) {
            index = 1;
        }
        for (; index < length; ++index) {
            if (isdigit((unsigned char)value[index]) == 0 && value[index] != '.') {
                operand->type = SHADER_REQUIREMENT_OPERAND_STRING;
                break;
            }
        }
        if (length == 0)
            operand->type = SHADER_REQUIREMENT_OPERAND_STRING;
    }

    if (operand->leadingNotCount != 0) {
        const qboolean valueIsNonzero = atof(value) != 0.0;
        const qboolean negatedValue = (operand->leadingNotCount & 1) != 0 ? !valueIsNonzero : valueIsNonzero;

        operand->type = SHADER_REQUIREMENT_OPERAND_NUMBER;
        operand->value[0] = negatedValue ? '1' : '0';
        operand->value[1] = '\0';
        return qtrue;
    }

    if (value != operand->value) {
        strncpy(operand->value, value, sizeof(operand->value) - 1);
        operand->value[sizeof(operand->value) - 1] = '\0';
    }
    return qtrue;
}

/* Source: CoDUOMP.exe 0x004f9420..0x004f9580.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f9420_004f9581.mcode.
 * Name: exact same-module Mac symbol UpdateRequiresCondition.
 *
 * This records which hardware features a successfully parsed expression
 * explicitly depends on. It intentionally recognizes only the comparison
 * shapes handled by the original; arbitrary equivalent Boolean expressions
 * do not acquire requirement flags. */
void UpdateRequiresCondition(shader_requirement_operator_t operatorKind, const shader_requirement_operand_t *first,
                             const shader_requirement_operand_t *second)
{
    const shader_requirement_operand_t *dependencyOperand = first;
    const shader_requirement_operand_t *literalOperand = second;
    static const shader_requirement_operator_t reverseOperator[] = {SHADER_REQUIREMENT_EQUAL,   SHADER_REQUIREMENT_NOT_EQUAL,
                                                                    SHADER_REQUIREMENT_GREATER, SHADER_REQUIREMENT_GREATER_OR_EQUAL,
                                                                    SHADER_REQUIREMENT_LESS,    SHADER_REQUIREMENT_LESS_OR_EQUAL};
    float literalValue;
    int32_t integerValue;

    if (first->dependency < 0) {
        if (second->dependency < 0)
            return;
        dependencyOperand = second;
        literalOperand = first;
        operatorKind = reverseOperator[operatorKind];
    } else if (second->dependency >= 0) {
        return;
    }

    if (literalOperand->type != SHADER_REQUIREMENT_OPERAND_NUMBER) {
        return;
    }

    /* 0x004f9462..0x004f9473 stores atof's result as float, but _ftol
     * consumes the retained x87 return value. */
    const long double literalValueRaw = (long double)atof(literalOperand->value);
    literalValue = (float)literalValueRaw;
    if (!isfinite(literalValueRaw) || literalValueRaw < -2147483648.0L || literalValueRaw >= 2147483648.0L) {
        return;
    }
    integerValue = (int32_t)literalValueRaw;
    if ((float)integerValue != literalValue)
        return;

    if (dependencyOperand->dependency != SHADER_REQUIREMENT_MAX_TEXTURE_UNITS_ARB) {
        const qboolean literalIsOne = literalValue == 1.0f;
        const qboolean dependencyIsNegated = (dependencyOperand->leadingNotCount & 1) != 0;
        qboolean recordDependency = qfalse;

        if (operatorKind == SHADER_REQUIREMENT_NOT_EQUAL) {
            recordDependency = dependencyIsNegated ? !literalIsOne : literalIsOne;
        } else if (operatorKind == SHADER_REQUIREMENT_EQUAL) {
            recordDependency = dependencyIsNegated ? literalIsOne : !literalIsOne;
        }

        if (recordDependency != qfalse) {
            rendererShaderRequirements[dependencyOperand->dependency] = 1;
        }
        return;
    }

    if (dependencyOperand->leadingNotCount != 0)
        return;

    if (operatorKind == SHADER_REQUIREMENT_GREATER)
        ++integerValue;
    else if (operatorKind != SHADER_REQUIREMENT_EQUAL && operatorKind != SHADER_REQUIREMENT_GREATER_OR_EQUAL)
        return;

    if (integerValue >= 3 && integerValue <= R_MAX_TEXTURE_UNITS) {
        rendererShaderRequirements[integerValue - 3] = 1;
    }
}

/* Source: CoDUOMP.exe 0x004f9590..0x004f9997.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f9590_004f9998.mcode.
 * Name: exact same-module Mac symbol ParseStageRequirements.
 *
 * The expression grammar is deliberately small: a value may stand alone or
 * participate in one comparison, and clauses may be joined recursively with
 * "||". Bitwise OR preserves the original eager evaluation of every clause. */
qboolean ParseStageRequirements(char **text, qboolean suppressDependencyUpdates)
{
    shader_requirement_operand_t first;
    shader_requirement_operand_t second;
    shader_requirement_operator_t operatorKind;
    char operatorText[8];
    char *token;
    qboolean result;

    if (ParseStageRequirementsOperand(text, &first) == qfalse) {
        ri.Error(ERR_DROP,
                 "\x15^1bad or missing arguments to 'requires' "
                 "in shader '%s'\n",
                 rendererParsedShader.name);
        return qtrue;
    }

    token = Com_ParseOnLine(text);
    if (token[0] == '\0' || strcmp(token, "||") == 0)
        goto evaluate_single_operand;

    if (strlen(token) > sizeof(operatorText) - 1) {
        ri.Error(ERR_DROP,
                 "\x15^1bad operator '%s' in 'requires' "
                 "in shader '%s'\n",
                 token, rendererParsedShader.name);
        return qtrue;
    }
    strcpy(operatorText, token);

    if (ParseStageRequirementsOperand(text, &second) == qfalse) {
        ri.Error(ERR_DROP,
                 "\x15^1bad or missing second operand to 'requires' "
                 "in shader '%s'\n",
                 rendererParsedShader.name);
        return qtrue;
    }

    if (first.type != second.type) {
        ri.Printf(R_PRINT_WARNING,
                  "WARNING: operands to 'requires' in shader '%s' "
                  "have different types, comparing as strings\n",
                  rendererParsedShader.name);
    }

    if (strcmp(operatorText, "==") == 0) {
        operatorKind = SHADER_REQUIREMENT_EQUAL;
        result = atof(first.value) == atof(second.value);
    } else if (strcmp(operatorText, "!=") == 0) {
        operatorKind = SHADER_REQUIREMENT_NOT_EQUAL;
        result = atof(first.value) != atof(second.value);
    } else if (strcmp(operatorText, ">") == 0) {
        operatorKind = SHADER_REQUIREMENT_GREATER;
        result = atof(first.value) > atof(second.value);
    } else if (strcmp(operatorText, ">=") == 0) {
        operatorKind = SHADER_REQUIREMENT_GREATER_OR_EQUAL;
        result = atof(first.value) >= atof(second.value);
    } else if (strcmp(operatorText, "<") == 0) {
        operatorKind = SHADER_REQUIREMENT_LESS;
        result = atof(first.value) < atof(second.value);
    } else if (strcmp(operatorText, "<=") == 0) {
        operatorKind = SHADER_REQUIREMENT_LESS_OR_EQUAL;
        result = atof(first.value) <= atof(second.value);
    } else {
        ri.Error(ERR_DROP,
                 "\x15^1unknown operator '%s' in 'requires' "
                 "for shader '%s'\n",
                 operatorText, rendererParsedShader.name);
        return qtrue;
    }

    token = Com_ParseOnLine(text);
    if (token[0] != '\0' && strcmp(token, "||") == 0) {
        return result | ParseStageRequirements(text, qtrue);
    }

    if (suppressDependencyUpdates == qfalse) {
        UpdateRequiresCondition(operatorKind, &first, &second);
    }
    return result;

evaluate_single_operand:
    result = atof(first.value) != 0.0;
    if (token[0] != '\0') {
        return result | ParseStageRequirements(text, qtrue);
    }

    if (suppressDependencyUpdates == qfalse && first.dependency >= 0 && first.dependency != SHADER_REQUIREMENT_MAX_TEXTURE_UNITS_ARB &&
        first.dependency != SHADER_REQUIREMENT_MAX_TEXTURE_UNITS_4 && (first.leadingNotCount & 1) == 0) {
        rendererShaderRequirements[first.dependency] = 1;
    }
    return result;
}

/* Source: CoDUOMP.exe 0x004fea30..0x005004a5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004fea30_005004a5.mcode.
 * Name: exact same-module Mac symbol ParseStage.
 *
 * `stageActive` is the accumulated result of the stage's `requires`
 * expressions. The parser still consumes and validates an inactive stage,
 * but ParseShader does not copy it into the live stage bank. The original
 * parser also deliberately builds tcMod's remaining line in a temporary
 * string and reparses that string through ParseTexMod. */
qboolean ParseStage(shaderStage_t *stage, char **text, qboolean allowTextureName, renderer_image_track_t imageTrack,
                    qboolean *outStageActive)
{
    vec4_t imageColorScale = {1.0f, 1.0f, 1.0f, 1.0f};
    uint32_t depthMaskBits = GLS_DEPTHMASK_TRUE;
    uint32_t srcBlendBits = 0;
    uint32_t dstBlendBits = 0;
    uint32_t alphaTestBits = 0;
    uint32_t depthFuncBits = 0;
    uint32_t lightingBits = 0;
    uint32_t textureShaderBits = 0;
    uint32_t registerCombinerBits = 0;
    uint32_t fragmentShaderBits = 0;
    uint32_t vertexProgramBits = 0;
    qboolean explicitDepthWrite = qfalse;
    qboolean stageActive = qtrue;
    qboolean multiplyImage = qfalse;
    int32_t bundleIndex = 0;

    stage->flags |= SHADER_STAGE_ACTIVE;
    stage->bundle[0].texCoordComponentCount = 2;
    stage->bundle[0].textureEnvMode = GL_MODULATE;
    memset(rendererShaderRequirements, 0, sizeof(rendererShaderRequirements));

    for (;;) {
        textureBundle_t *bundle = &stage->bundle[bundleIndex];
        char *token = Com_Parse(text);

        if (token[0] == '\0') {
            ri.Printf(R_PRINT_WARNING, "WARNING: no matching '}' found\n");
            return qfalse;
        }

        if (token[0] == '}') {
            if (stageActive != qfalse && bundle->image[0] == NULL) {
                ri.Printf(R_PRINT_WARNING, "WARNING: shader '%s' has bundle with no map\n", rendererParsedShader.name);
                return qfalse;
            }

            if (textureShaderBits != 0) {
                for (int32_t textureUnit = 0; textureUnit < glConfig.maxActiveTextures; ++textureUnit) {
                    textureBundle_t *textureBundle = &stage->bundle[textureUnit];
                    if (textureBundle->textureShader != NULL)
                        continue;

                    textureBundle->textureShader = ri.Hunk_Alloc(sizeof(*textureBundle->textureShader));
                    if (textureBundle->image[0] == NULL) {
                        textureBundle->textureShader->operation = 0;
                    } else {
                        textureBundle->textureShader->operation = textureBundle->image[0]->target;
                    }
                }
            }

            if (stage->rgbGen == CGEN_BAD) {
                if (srcBlendBits == 0 || srcBlendBits == GLS_SRCBLEND_ONE || srcBlendBits == GLS_SRCBLEND_SRC_ALPHA) {
                    stage->rgbGen = CGEN_IDENTITY_LIGHTING;
                } else {
                    stage->rgbGen = CGEN_IDENTITY;
                }
            }

            /* The source spellings `blendFunc GL_ONE GL_ZERO` collapse to
             * the renderer's no-blend representation and restore the depth
             * write that an ordinary blendFunc would have removed. */
            if (srcBlendBits == GLS_SRCBLEND_ONE && dstBlendBits == GLS_DSTBLEND_ZERO) {
                srcBlendBits = 0;
                dstBlendBits = 0;
                depthMaskBits = GLS_DEPTHMASK_TRUE;
            }

            if (stage->alphaGen == AGEN_UNSPECIFIED &&
                (stage->rgbGen == CGEN_IDENTITY || stage->rgbGen == CGEN_LIGHTING_AMBIENT || stage->rgbGen == CGEN_LIGHTING_DIFFUSE)) {
                stage->alphaGen = AGEN_IDENTITY;
            }

            stage->stateBits = depthMaskBits | srcBlendBits | dstBlendBits | alphaTestBits | depthFuncBits | lightingBits |
                               textureShaderBits | registerCombinerBits | fragmentShaderBits | vertexProgramBits;
            *outStageActive = stageActive;
            return qtrue;
        }

        if (registerCombinerBits != 0) {
            ri.Printf(R_PRINT_WARNING, "WARNING: nvRegCombiners must be the last thing "
                                       "in a stage\n");
            return qfalse;
        }

        if (Q_stricmp(token, "map") == 0) {
            image_t **outImage = &bundle->image[bundle->numImageAnimations];

            if (ParseImage(text, allowTextureName, stageActive, imageTrack, multiplyImage != qfalse ? imageColorScale : NULL, outImage) ==
                qfalse) {
                return qfalse;
            }

            if (*outImage != NULL) {
                image_t *image = *outImage;
                if ((rendererParsedShader.lightmapIndex >= 0 && image == tr.lightmaps[rendererParsedShader.lightmapIndex]) ||
                    image == tr.identityLightImage || image == tr.dlightImage) {
                    bundle->isLightmap = 1;
                }

                ++bundle->numImageAnimations;
                if (bundle->numImageAnimations > 1 && bundle->imageAnimationSpeed == 0.0f) {
                    ri.Printf(R_PRINT_WARNING,
                              "WARNING: multiple 'map' specifications "
                              "without preceding 'animmap' in shader'%s'\n",
                              rendererParsedShader.name);
                    return qfalse;
                }
            }
            continue;
        }

        if (Q_stricmp(token, "animMap") == 0 || Q_stricmp(token, "oneshotanimMap") == 0) {
            const qboolean oneShot = Q_stricmp(token, "oneshotanimMap") == 0 ? qtrue : qfalse;
            token = Com_ParseOnLine(text);
            rendererParsedShader.surfaceFlags |= SHADER_SURFACE_BASE_TEXCOORDS;

            if (token[0] == '\0') {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: missing parameter for 'animMmap' "
                          "keyword in shader '%s'\n",
                          rendererParsedShader.name);
                return qfalse;
            }

            bundle->imageAnimationSpeed = (float)atof(token);
            /* The Windows store is stage+0xca, hence bundle zero even if the
             * keyword appears after `nextbundle`. */
            stage->bundle[0].clampAnimation = (uint8_t)oneShot;
            Com_SkipRestOfLine(text);
            continue;
        }

        if (Q_stricmp(token, "videoMap") == 0) {
            rendererParsedShader.surfaceFlags |= SHADER_SURFACE_TIME_DEPENDENT | SHADER_SURFACE_BASE_TEXCOORDS;
            token = Com_ParseOnLine(text);
            if (token[0] == '\0') {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: missing parameter for 'videoMap' "
                          "keyword in shader '%s'\n",
                          rendererParsedShader.name);
                return qfalse;
            }
            if (multiplyImage != qfalse) {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: tried to use multiplyImage with "
                          "videoMap in shader '%s'\n",
                          rendererParsedShader.name);
                return qfalse;
            }

            bundle->videoMapHandle = ri.CIN_PlayCinematic(token, 0, 0, 256, 256, CIN_LOOP | CIN_SILENT | CIN_SHADER);
            if (bundle->videoMapHandle != -1) {
                bundle->isVideoMap = 1;
                bundle->image[0] = tr.scratchImages[bundle->videoMapHandle];
            }
            continue;
        }

        if (Q_stricmp(token, "cubeMap") == 0) {
            uint32_t imageFlags = IMAGE_FLAG_MIPMAP | IMAGE_FLAG_ALLOW_PICMIP | IMAGE_FLAG_CLAMP_S | IMAGE_FLAG_CLAMP_T;
            renderer_image_track_t cubeImageTrack = imageTrack;
            image_t *image = NULL;
            char *imageName;

            if ((rendererParsedShader.flags & SHADER_FLAG_USE_PICMIP2) != 0) {
                imageFlags |= IMAGE_FLAG_USE_PICMIP2;
            }
            if ((rendererParsedShader.flags & SHADER_FLAG_NO_MIPMAPS) != 0) {
                imageFlags &= ~IMAGE_FLAG_MIPMAP;
            }
            if ((rendererParsedShader.flags & SHADER_FLAG_NO_PICMIP) != 0) {
                imageFlags &= ~IMAGE_FLAG_ALLOW_PICMIP;
            }

            if (rendererShaderRequirements[SHADER_REQUIREMENT_TEXTURE_CUBE_MAP_ARB] == 0) {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: shader '%s' uses cubeMap without "
                          "'requires GL_ARB_texture_cube_map'\n",
                          rendererParsedShader.name);
                return qfalse;
            }

            imageName = Com_ParseOnLine(text);
            if (imageName[0] == '\0') {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: missing parameter for 'cubeMap' "
                          "in shader '%s'\n",
                          rendererParsedShader.name);
                return qfalse;
            }

            if (Q_stricmp(imageName, "$texturename") == 0) {
                if (allowTextureName == qfalse) {
                    ri.Printf(R_PRINT_WARNING,
                              "WARNING: $texturename used in shader '%s', "
                              "which is not a shader type file\n",
                              rendererParsedShader.name);
                    return qfalse;
                }
                imageName = Com_ParseOnLine(text);
                if (Q_stricmp(imageName, ",") == 0)
                    imageName = Com_ParseOnLine(text);
                else
                    imageName = "";
                cubeImageTrack = R_IMAGE_TRACK_GENERATED_TEXTURE;
            }

            if (stageActive != qfalse) {
                image = R_FindImageFile(imageName, GL_TEXTURE_CUBE_MAP_ARB, imageFlags, cubeImageTrack,
                                        multiplyImage != qfalse ? imageColorScale : NULL, 1.0f);
                if (image == NULL) {
                    ri.Printf(R_PRINT_WARNING,
                              "WARNING: R_FindImageFile could not find "
                              "'%s' in shader '%s'\n",
                              imageName, rendererParsedShader.name);
                    return qfalse;
                }

                bundle->image[bundle->numImageAnimations] = image;
                if (bundle->numImageAnimations == 0)
                    bundle->texCoordComponentCount = 3;
                ++bundle->numImageAnimations;
                if (bundle->numImageAnimations > 1 && bundle->imageAnimationSpeed == 0.0f) {
                    ri.Printf(R_PRINT_WARNING,
                              "WARNING: multiple 'cubemap' specifications "
                              "without preceding 'animmap' in shader'%s'\n",
                              rendererParsedShader.name);
                    return qfalse;
                }
            }
            Com_SkipRestOfLine(text);
            continue;
        }

        if (Q_stricmp(token, "waterMap") == 0) {
            if (bundle->numImageAnimations != 0) {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: multiple 'waterMap' must be only map "
                          "in shader '%s'\n",
                          rendererParsedShader.name);
                return qfalse;
            }
            if (ParseWaterMap(stage, text, bundleIndex) == qfalse)
                return qfalse;
            Com_SkipRestOfLine(text);
            continue;
        }

        if (Q_stricmp(token, "nvTexShader") == 0) {
            qboolean parsed;
            Com_SetSpaceDelimited(qfalse);
            parsed = ParseNVTexShader(stage, text, bundleIndex);
            Com_SetSpaceDelimited(qtrue);
            if (parsed == qfalse)
                return qfalse;
            Com_SkipRestOfLine(text);
            textureShaderBits = GLS_TEXTURE_SHADER_NV;
            continue;
        }

        if (Q_stricmp(token, "nvRegCombiners") == 0) {
            qboolean parsed;
            Com_SetSpaceDelimited(qfalse);
            parsed = ParseNVRegCombiners(stage, text);
            Com_SetSpaceDelimited(qtrue);
            if (parsed == qfalse)
                return qfalse;
            Com_SkipRestOfLine(text);
            registerCombinerBits = GLS_REGISTER_COMBINERS_NV;
            continue;
        }

        if (Q_stricmp(token, "atiFragmentShader") == 0) {
            qboolean parsed;
            Com_SetSpaceDelimited(qfalse);
            Com_SetParseNegativeNumbers(qfalse);
            parsed = ParseATIFragmentShader(stage, text, stageActive);
            Com_SetSpaceDelimited(qtrue);
            Com_SetParseNegativeNumbers(qtrue);
            if (parsed == qfalse)
                return qfalse;
            Com_SkipRestOfLine(text);
            fragmentShaderBits = GLS_FRAGMENT_SHADER_ATI;
            continue;
        }

        if (Q_stricmp(token, "alphaFunc") == 0) {
            if (bundleIndex != 0) {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: 'alphaFunc' not allowed in nextbundle "
                          "in shader '%s'\n",
                          rendererParsedShader.name);
                return qfalse;
            }
            token = Com_ParseOnLine(text);
            if (token[0] == '\0') {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: missing parameter for 'alphaFunc' "
                          "keyword in shader '%s'\n",
                          rendererParsedShader.name);
                return qfalse;
            }
            alphaTestBits = NameToAFunc(token);
            continue;
        }

        if (Q_stricmp(token, "depthfunc") == 0) {
            if (bundleIndex != 0) {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: 'depthFunc' not allowed in nextbundle "
                          "in shader '%s'\n",
                          rendererParsedShader.name);
                return qfalse;
            }
            token = Com_ParseOnLine(text);
            if (token[0] == '\0') {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: missing parameter for 'depthfunc' "
                          "keyword in shader '%s'\n",
                          rendererParsedShader.name);
                return qfalse;
            }
            if (Q_stricmp(token, "lequal") == 0) {
                depthFuncBits = 0;
            } else if (Q_stricmp(token, "equal") == 0) {
                depthFuncBits = GLS_DEPTHFUNC_EQUAL;
            } else if (Q_stricmp(token, "always") == 0) {
                depthFuncBits = GLS_DEPTHFUNC_GREATER;
            } else {
                ri.Printf(R_PRINT_WARNING, "WARNING: unknown depthfunc '%s' in shader '%s'\n", token, rendererParsedShader.name);
            }
            continue;
        }

        if (Q_stricmp(token, "detail") == 0) {
            if (bundleIndex != 0) {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: 'detail' not allowed in nextbundle "
                          "in shader '%s'\n",
                          rendererParsedShader.name);
                return qfalse;
            }
            stage->flags |= SHADER_STAGE_DETAIL;
            continue;
        }

        if (Q_stricmp(token, "perLight") == 0) {
            if (bundleIndex != 0) {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: 'perLight' not allowed in nextbundle "
                          "in shader '%s'\n",
                          rendererParsedShader.name);
                return qfalse;
            }
            stage->flags |= SHADER_STAGE_PER_LIGHT;
            continue;
        }

        if (Q_stricmp(token, "fog") == 0) {
            if (bundleIndex != 0) {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: 'fog' not allowed in nextbundle "
                          "in shader '%s'\n",
                          rendererParsedShader.name);
                return qfalse;
            }
            token = Com_ParseOnLine(text);
            if (token[0] == '\0') {
                ri.Printf(R_PRINT_WARNING, "WARNING: missing parm for fog in shader '%s'\n", rendererParsedShader.name);
            } else if (Q_stricmp(token, "on") == 0) {
                stage->flags |= SHADER_STAGE_FOG;
            } else {
                stage->flags &= ~SHADER_STAGE_FOG;
            }
            continue;
        }

        if (Q_stricmp(token, "blendfunc") == 0) {
            uint32_t parsedSrcBlend;
            uint32_t parsedDstBlend;

            token = Com_ParseOnLine(text);
            if (token[0] == '\0') {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: missing parm for blendFunc in "
                          "shader '%s'\n",
                          rendererParsedShader.name);
                continue;
            }

            if (Q_stricmp(token, "add") == 0) {
                parsedSrcBlend = GLS_SRCBLEND_ONE;
                parsedDstBlend = GLS_DSTBLEND_ONE;
            } else if (Q_stricmp(token, "filter") == 0) {
                parsedSrcBlend = GLS_SRCBLEND_DST_COLOR;
                parsedDstBlend = GLS_DSTBLEND_ZERO;
            } else if (Q_stricmp(token, "blend") == 0) {
                parsedSrcBlend = GLS_SRCBLEND_SRC_ALPHA;
                parsedDstBlend = GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
            } else {
                parsedSrcBlend = NameToSrcBlendMode(token);
                token = Com_ParseOnLine(text);
                if (token[0] == '\0') {
                    ri.Printf(R_PRINT_WARNING,
                              "WARNING: missing parm for localFunc in "
                              "shader '%s'\n",
                              rendererParsedShader.name);
                    continue;
                }
                parsedDstBlend = NameToDstBlendMode(token);
            }

            if (bundleIndex != 0) {
                if (parsedSrcBlend == GLS_SRCBLEND_ONE && parsedDstBlend == GLS_DSTBLEND_ONE) {
                    if (rendererShaderRequirements[SHADER_REQUIREMENT_TEXTURE_ENV_ADD_ARB] == 0) {
                        ri.Printf(R_PRINT_WARNING,
                                  "WARNING: shader '%s' uses optional "
                                  "GL_ARB_texture_env_add without suitable "
                                  "'requires' statement\n",
                                  rendererParsedShader.name);
                        return qfalse;
                    }
                    bundle->textureEnvMode = GL_ADD;
                } else if ((parsedSrcBlend == GLS_SRCBLEND_DST_COLOR && parsedDstBlend == GLS_DSTBLEND_ZERO) ||
                           (parsedSrcBlend == GLS_SRCBLEND_ZERO && parsedDstBlend == GLS_DSTBLEND_SRC_COLOR)) {
                    bundle->textureEnvMode = GL_MODULATE;
                } else if (parsedSrcBlend == GLS_SRCBLEND_SRC_ALPHA && parsedDstBlend == GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA) {
                    bundle->textureEnvMode = GL_DECAL;
                } else {
                    ri.Printf(R_PRINT_WARNING,
                              "WARNING: bad blendFunc for nextbundle %i "
                              "in shader '%s'\n",
                              bundleIndex, rendererParsedShader.name);
                    return qfalse;
                }

                /* State blending is owned by the first bundle; later bundle
                 * blend functions select only a fixed-function texenv. */
                parsedSrcBlend = srcBlendBits;
                parsedDstBlend = dstBlendBits;
            }

            srcBlendBits = parsedSrcBlend;
            dstBlendBits = parsedDstBlend;
            if (explicitDepthWrite == qfalse)
                depthMaskBits = 0;
            continue;
        }

        if (Q_stricmp(token, "rgbGen") == 0) {
            qboolean constantLighting;

            if (bundleIndex != 0) {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: 'rgbGen' not allowed in nextbundle "
                          "in shader '%s'\n",
                          rendererParsedShader.name);
                return qfalse;
            }
            token = Com_ParseOnLine(text);
            if (token[0] == '\0') {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: missing parameters for rgbGen in "
                          "shader '%s'\n",
                          rendererParsedShader.name);
                continue;
            }

            lightingBits = 0;
            if (Q_stricmp(token, "wave") == 0) {
                rendererParsedShader.surfaceFlags |= SHADER_SURFACE_TIME_DEPENDENT;
                ParseWaveForm(text, &stage->rgbWave);
                stage->rgbGen = CGEN_WAVEFORM;
                continue;
            }

            constantLighting = Q_stricmp(token, "constLighting") == 0 ? qtrue : qfalse;
            if (Q_stricmp(token, "const") == 0 || constantLighting != qfalse) {
                vec3_t color;
                (void)ParseVector(text, 3, color);
                if (constantLighting != qfalse) {
                    color[0] *= tr.identityLight;
                    color[1] *= tr.identityLight;
                    color[2] *= tr.identityLight;
                }
                for (int32_t component = 0; component < 3; ++component) {
                    stage->constantColor[component] = (uint8_t)(int32_t)(255.0f * color[component]);
                }
                stage->rgbGen = CGEN_CONSTANT;
            } else if (Q_stricmp(token, "identity") == 0) {
                stage->rgbGen = CGEN_IDENTITY;
            } else if (Q_stricmp(token, "identityLighting") == 0) {
                stage->rgbGen = CGEN_IDENTITY_LIGHTING;
            } else if (Q_stricmp(token, "entity") == 0) {
                stage->rgbGen = CGEN_ENTITY;
            } else if (Q_stricmp(token, "oneMinusEntity") == 0) {
                stage->rgbGen = CGEN_ONE_MINUS_ENTITY;
            } else if (Q_stricmp(token, "vertex") == 0) {
                rendererParsedShader.surfaceFlags |= SHADER_SURFACE_VERTEX_COLORS;
                stage->rgbGen = CGEN_VERTEX;
                if (stage->alphaGen == AGEN_UNSPECIFIED)
                    stage->alphaGen = AGEN_VERTEX;
            } else if (Q_stricmp(token, "exactVertex") == 0) {
                rendererParsedShader.surfaceFlags |= SHADER_SURFACE_VERTEX_COLORS;
                stage->rgbGen = CGEN_EXACT_VERTEX;
                if (stage->alphaGen == AGEN_UNSPECIFIED)
                    stage->alphaGen = AGEN_VERTEX;
            } else if (Q_stricmp(token, "lightingAmbient") == 0) {
                lightingBits = GLS_LIGHTING;
                stage->rgbGen = CGEN_LIGHTING_AMBIENT;
            } else if (Q_stricmp(token, "lightingDiffuse") == 0) {
                lightingBits = GLS_LIGHTING;
                stage->rgbGen = CGEN_LIGHTING_DIFFUSE;
            } else if (Q_stricmp(token, "oneMinusVertex") == 0) {
                rendererParsedShader.surfaceFlags |= SHADER_SURFACE_VERTEX_COLORS;
                stage->rgbGen = CGEN_ONE_MINUS_VERTEX;
            } else if (Q_stricmp(token, "lightingPrecalc") == 0) {
                stage->rgbGen = CGEN_LIGHTING_PRECALC;
            } else {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: unknown rgbGen parameter '%s' "
                          "in shader '%s'\n",
                          token, rendererParsedShader.name);
                Com_SkipRestOfLine(text);
            }
            continue;
        }

        if (Q_stricmp(token, "alphaGen") == 0) {
            if (bundleIndex != 0) {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: 'alphaGen' not allowed in nextbundle "
                          "in shader '%s'\n",
                          rendererParsedShader.name);
                return qfalse;
            }
            token = Com_ParseOnLine(text);
            if (token[0] == '\0') {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: missing parameters for alphaGen in "
                          "shader '%s'\n",
                          rendererParsedShader.name);
                continue;
            }

            if (Q_stricmp(token, "wave") == 0) {
                rendererParsedShader.surfaceFlags |= SHADER_SURFACE_TIME_DEPENDENT;
                ParseWaveForm(text, &stage->alphaWave);
                stage->alphaGen = AGEN_WAVEFORM;
            } else if (Q_stricmp(token, "const") == 0) {
                stage->constantColor[3] = (uint8_t)(int32_t)(255.0 * atof(Com_ParseOnLine(text)));
                stage->alphaGen = AGEN_CONSTANT;
            } else if (Q_stricmp(token, "constLighting") == 0) {
                stage->constantColor[3] = (uint8_t)(int32_t)(255.0 * tr.identityLight * atof(Com_ParseOnLine(text)));
                stage->alphaGen = AGEN_CONSTANT;
            } else if (Q_stricmp(token, "identity") == 0) {
                stage->alphaGen = AGEN_UNSPECIFIED;
            } else if (Q_stricmp(token, "entity") == 0) {
                stage->alphaGen = AGEN_ENTITY;
            } else if (Q_stricmp(token, "oneMinusEntity") == 0) {
                stage->alphaGen = AGEN_ONE_MINUS_ENTITY;
            } else if (Q_stricmp(token, "vertex") == 0) {
                rendererParsedShader.surfaceFlags |= SHADER_SURFACE_VERTEX_COLORS;
                stage->alphaGen = AGEN_VERTEX;
            } else if (Q_stricmp(token, "lightingSpecular") == 0) {
                stage->alphaGen = AGEN_LIGHTING_SPECULAR;
            } else if (Q_stricmp(token, "oneMinusVertex") == 0) {
                rendererParsedShader.surfaceFlags |= SHADER_SURFACE_VERTEX_COLORS;
                stage->alphaGen = AGEN_ONE_MINUS_VERTEX;
            } else if (Q_stricmp(token, "portal") == 0) {
                stage->alphaGen = AGEN_PORTAL;
                token = Com_ParseOnLine(text);
                if (token[0] == '\0') {
                    rendererParsedShader.portalRange = 256.0f;
                    ri.Printf(R_PRINT_WARNING,
                              "WARNING: missing range parameter for "
                              "alphaGen portal in shader '%s', "
                              "defaulting to 256\n",
                              rendererParsedShader.name);
                } else {
                    rendererParsedShader.portalRange = (float)atof(token);
                }
            } else if (Q_stricmp(token, "dot") == 0) {
                rendererParsedShader.surfaceFlags |= SHADER_SURFACE_REQUIRES_VERTEX_BASIS;
                stage->alphaGen = AGEN_DOT;
            } else if (Q_stricmp(token, "oneMinusDot") == 0) {
                rendererParsedShader.surfaceFlags |= SHADER_SURFACE_REQUIRES_VERTEX_BASIS;
                stage->alphaGen = AGEN_ONE_MINUS_DOT;
            } else if (Q_stricmp(token, "onePlusDot") == 0) {
                rendererParsedShader.surfaceFlags |= SHADER_SURFACE_REQUIRES_VERTEX_BASIS;
                stage->alphaGen = AGEN_ONE_PLUS_DOT;
            } else if (Q_stricmp(token, "negativeDot") == 0) {
                rendererParsedShader.surfaceFlags |= SHADER_SURFACE_REQUIRES_VERTEX_BASIS;
                stage->alphaGen = AGEN_NEGATIVE_DOT;
            } else {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: unknown alphaGen parameter '%s' "
                          "in shader '%s'\n",
                          token, rendererParsedShader.name);
            }
            continue;
        }

        if (Q_stricmp(token, "texgen") == 0 || Q_stricmp(token, "tcGen") == 0) {
            if (stageActive == qfalse) {
                Com_SkipRestOfLine(text);
                continue;
            }
            if (bundle->image[0] == NULL) {
                ri.Printf(R_PRINT_WARNING, "WARNING: texgen before image in shader '%s'\n", rendererParsedShader.name);
                return qfalse;
            }

            token = Com_ParseOnLine(text);
            if (token[0] == '\0') {
                ri.Printf(R_PRINT_WARNING, "WARNING: missing texgen parm in shader '%s'\n", rendererParsedShader.name);
                continue;
            }

            if (Q_stricmp(token, "normal") == 0) {
                rendererParsedShader.surfaceFlags |= SHADER_SURFACE_REQUIRES_VERTEX_BASIS;
                bundle->tcGen = TCGEN_NORMAL;
                bundle->texCoordComponentCount = 3;
            } else if (Q_stricmp(token, "tangent") == 0) {
                rendererParsedShader.surfaceFlags |= SHADER_SURFACE_REQUIRES_TANGENT;
                bundle->tcGen = TCGEN_TANGENT;
                bundle->texCoordComponentCount = 3;
            } else if (Q_stricmp(token, "binormal") == 0) {
                rendererParsedShader.surfaceFlags |= SHADER_SURFACE_REQUIRES_BITANGENT;
                bundle->tcGen = TCGEN_BITANGENT;
                bundle->texCoordComponentCount = 3;
            } else if (Q_stricmp(token, "tbn_x") == 0) {
                rendererParsedShader.surfaceFlags |= SHADER_SURFACE_REQUIRES_VERTEX_BASIS | SHADER_SURFACE_TANGENT_SPACE_INPUT_MASK;
                bundle->tcGen = TCGEN_TBN_S;
                bundle->texCoordComponentCount = 3;
            } else if (Q_stricmp(token, "tbn_y") == 0) {
                rendererParsedShader.surfaceFlags |= SHADER_SURFACE_REQUIRES_VERTEX_BASIS | SHADER_SURFACE_TANGENT_SPACE_INPUT_MASK;
                bundle->tcGen = TCGEN_TBN_T;
                bundle->texCoordComponentCount = 3;
            } else if (Q_stricmp(token, "tbn_z") == 0) {
                rendererParsedShader.surfaceFlags |= SHADER_SURFACE_REQUIRES_VERTEX_BASIS | SHADER_SURFACE_TANGENT_SPACE_INPUT_MASK;
                bundle->tcGen = TCGEN_TBN_R;
                bundle->texCoordComponentCount = 3;
            } else if (Q_stricmp(token, "eyeToVertex") == 0) {
                bundle->tcGen = TCGEN_CUBEMAP_EYE_TO_VERTEX;
                bundle->texCoordComponentCount = 3;
            } else if (Q_stricmp(token, "vertexToEye") == 0) {
                bundle->tcGen = TCGEN_CUBEMAP_VERTEX_TO_EYE;
                bundle->texCoordComponentCount = 3;
            } else if (Q_stricmp(token, "reflection") == 0) {
                rendererParsedShader.surfaceFlags |= SHADER_SURFACE_REQUIRES_VERTEX_BASIS;
                bundle->tcGen = TCGEN_CUBEMAP_REFLECTION;
                bundle->texCoordComponentCount = 3;
            } else if (Q_stricmp(token, "lightvector") == 0) {
                bundle->tcGen = TCGEN_CUBEMAP_LIGHT_VECTOR;
                bundle->texCoordComponentCount = 3;
            } else if (Q_stricmp(token, "lighthalfangle") == 0) {
                bundle->tcGen = TCGEN_CUBEMAP_LIGHT_HALF_ANGLE;
                bundle->texCoordComponentCount = 3;
            } else if (Q_stricmp(token, "sunhalfangle") == 0) {
                bundle->tcGen = TCGEN_CUBEMAP_SUN_HALF_ANGLE;
                bundle->texCoordComponentCount = 3;
            } else if (Q_stricmp(token, "nv_dot_product_reflect_cube_map_eye_from_qs_1of3") == 0) {
                rendererParsedShader.surfaceFlags |= SHADER_SURFACE_REQUIRES_VERTEX_BASIS | SHADER_SURFACE_TANGENT_SPACE_INPUT_MASK;
                bundle->tcGen = TCGEN_CUBEMAP_DOT3_REFLECT_S;
                bundle->texCoordComponentCount = 4;
            } else if (Q_stricmp(token, "nv_dot_product_reflect_cube_map_eye_from_qs_2of3") == 0) {
                rendererParsedShader.surfaceFlags |= SHADER_SURFACE_REQUIRES_VERTEX_BASIS | SHADER_SURFACE_TANGENT_SPACE_INPUT_MASK;
                bundle->tcGen = TCGEN_CUBEMAP_DOT3_REFLECT_T;
                bundle->texCoordComponentCount = 4;
            } else if (Q_stricmp(token, "nv_dot_product_reflect_cube_map_eye_from_qs_3of3") == 0) {
                rendererParsedShader.surfaceFlags |= SHADER_SURFACE_REQUIRES_VERTEX_BASIS | SHADER_SURFACE_TANGENT_SPACE_INPUT_MASK;
                bundle->tcGen = TCGEN_CUBEMAP_DOT3_REFLECT_R;
                bundle->texCoordComponentCount = 4;
            } else if (Q_stricmp(token, "environment") == 0) {
                rendererParsedShader.surfaceFlags |= SHADER_SURFACE_REQUIRES_VERTEX_BASIS;
                bundle->tcGen = TCGEN_ENVIRONMENT_MAPPED;
            } else if (Q_stricmp(token, "lightmap") == 0) {
                rendererParsedShader.surfaceFlags |= SHADER_SURFACE_LIGHTMAP_TEXCOORDS;
                bundle->tcGen = TCGEN_LIGHTMAP;
            } else if (Q_stricmp(token, "texture") == 0 || Q_stricmp(token, "base") == 0) {
                rendererParsedShader.surfaceFlags |= SHADER_SURFACE_BASE_TEXCOORDS;
                bundle->tcGen = TCGEN_TEXTURE;
            } else if (Q_stricmp(token, "vector") == 0) {
                (void)ParseVector(text, 3, bundle->tcGenVectors[0]);
                (void)ParseVector(text, 3, bundle->tcGenVectors[1]);
                bundle->tcGen = TCGEN_VECTOR;
            } else {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: unknown or invalid texgen parm '%s' "
                          "in shader '%s'\n",
                          token, rendererParsedShader.name);
            }
            continue;
        }

        if (Q_stricmp(token, "tcMod") == 0) {
            char tcModText[R_STAGE_TEXMOD_TEXT_SIZE] = {0};
            size_t tcModTextLength = 0;

            /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
            for (;;) {
                size_t remaining;
                size_t tokenLength;

                token = Com_ParseOnLine(text);
                if (token[0] == '\0')
                    break;

                tokenLength = strlen(token);
                remaining = sizeof(tcModText) - tcModTextLength;
                if (remaining < 2 || tokenLength > remaining - 2) {
                    ri.Printf(R_PRINT_WARNING, "WARNING: tcMod line is too long in shader '%s'\n", rendererParsedShader.name);
                    return qfalse;
                }

                memcpy(&tcModText[tcModTextLength], token, tokenLength);
                tcModTextLength += tokenLength;
                tcModText[tcModTextLength++] = ' ';
                tcModText[tcModTextLength] = '\0';
            }

            if (stageActive != qfalse) {
                char *tcModCursor = tcModText;
                ParseTexMod(stage, bundleIndex, &tcModCursor);
            }
            continue;
        }

        if (Q_stricmp(token, "depthwrite") == 0) {
            if (bundleIndex != 0) {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: 'depthwrite' not allowed in "
                          "nextbundle in shader '%s'\n",
                          rendererParsedShader.name);
                return qfalse;
            }
            depthMaskBits = GLS_DEPTHMASK_TRUE;
            explicitDepthWrite = qtrue;
            continue;
        }

        if (Q_stricmp(token, "nextbundle") == 0) {
            qboolean hasSuitableRequirement = qfalse;

            if (stageActive != qfalse && bundle->image[0] == NULL) {
                ri.Printf(R_PRINT_WARNING, "WARNING: shader '%s' has bundle with no map\n", rendererParsedShader.name);
                return qfalse;
            }

            ++bundleIndex;
            if (bundleIndex >= 2) {
                for (int32_t requiredUnits = bundleIndex + 1; requiredUnits <= R_MAX_TEXTURE_UNITS; ++requiredUnits) {
                    if (rendererShaderRequirements[requiredUnits - 3] != 0) {
                        hasSuitableRequirement = qtrue;
                        break;
                    }
                }
                if (hasSuitableRequirement == qfalse) {
                    ri.Printf(R_PRINT_WARNING,
                              "WARNING: shader '%s' has %i or more "
                              "bundle without suitable 'requires "
                              "GL_MAX_TEXTURE_UNITS_ARB\n",
                              rendererParsedShader.name, bundleIndex + 1);
                    return qfalse;
                }
            }

            bundle = &stage->bundle[bundleIndex];
            bundle->texCoordComponentCount = 2;
            bundle->textureEnvMode = GL_MODULATE;
            imageColorScale[0] = 1.0f;
            imageColorScale[1] = 1.0f;
            imageColorScale[2] = 1.0f;
            imageColorScale[3] = 1.0f;
            multiplyImage = qfalse;
            continue;
        }

        if (Q_stricmp(token, "requires") == 0) {
            if (bundleIndex != 0) {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: 'requires' not allowed in nextbundle "
                          "in shader '%s'\n",
                          rendererParsedShader.name);
                return qfalse;
            }
            if (bundle->image[0] != NULL) {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: 'requires' should be before textures "
                          "are specified in shader '%s'\n",
                          rendererParsedShader.name);
                return qfalse;
            }
            stageActive &= ParseStageRequirements(text, qfalse);
            continue;
        }

        if (Q_stricmp(token, "texEnvCombine") == 0) {
            qboolean parsed;
            if (rendererShaderRequirements[SHADER_REQUIREMENT_TEXTURE_ENV_COMBINE_ARB] == 0) {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: shader '%s' uses texEnvCombine "
                          "without 'requires GL_ARB_texture_env_combine'\n",
                          rendererParsedShader.name);
                return qfalse;
            }
            Com_SetSpaceDelimited(qfalse);
            parsed = ParseTextureEnvCombine(bundle, text);
            Com_SetSpaceDelimited(qtrue);
            if (parsed == qfalse)
                return qfalse;
            continue;
        }

        if (Q_stricmp(token, "vertexProgram") == 0) {
            if (bundleIndex != 0) {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: 'vertexProgram' not allowed in "
                          "nextbundle in shader '%s'\n",
                          rendererParsedShader.name);
                return qfalse;
            }
            if (rendererShaderRequirements[SHADER_REQUIREMENT_VERTEX_PROGRAM_ARB] == 0) {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: shader '%s' uses vertexProgram "
                          "without 'requires GL_ARB_vertex_program'\n",
                          rendererParsedShader.name);
                return qfalse;
            }
            vertexProgramBits = GLS_VERTEX_PROGRAM_ARB;
            if (ParseVertexProgram(stage, text) == qfalse)
                return qfalse;
            rendererParsedShader.surfaceFlags |= SHADER_SURFACE_REQUIRES_VERTEX_BASIS | SHADER_SURFACE_REQUIRES_NORMAL_ARRAY;
            stage->flags |= SHADER_STAGE_NORMAL_ARRAY;
            continue;
        }

        if (Q_stricmp(token, "multiplyImage") == 0) {
            int32_t argumentCount = 0;

            if (bundle->image[0] != NULL) {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: shader '%s' uses 'multiplyImage' "
                          "after the images are defined\n",
                          rendererParsedShader.name);
                return qfalse;
            }

            while (argumentCount < 4) {
                token = Com_ParseOnLine(text);
                if (token[0] == '\0')
                    break;

                if (Q_stricmp(token, "identityLighting") == 0) {
                    imageColorScale[argumentCount] = tr.identityLight;
                } else if (!isdigit((unsigned char)token[0]) && token[0] != '.' && token[0] != '+') {
                    ri.Printf(R_PRINT_WARNING,
                              "WARNING: argument '%s' to multiplyImage "
                              "in shader '%s' should be "
                              "'identityLighting' or a number >= 0\n",
                              token, rendererParsedShader.name);
                    return qfalse;
                } else {
                    imageColorScale[argumentCount] = (float)atof(token);
                }
                ++argumentCount;
            }

            if (argumentCount == 0) {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: missing arguments to 'multiplyImage' "
                          "in shader '%s'\n",
                          rendererParsedShader.name);
                return qfalse;
            }
            if (argumentCount == 2) {
                imageColorScale[3] = imageColorScale[2];
            } else if (argumentCount != 4) {
                imageColorScale[3] = 1.0f;
            }
            if (argumentCount < 3) {
                imageColorScale[1] = imageColorScale[0];
                imageColorScale[2] = imageColorScale[0];
            }
            multiplyImage = qtrue;
            continue;
        }

        ri.Printf(R_PRINT_WARNING, "WARNING: unknown parameter '%s' in shader '%s'\n", token, rendererParsedShader.name);
        return qfalse;
    }
}

/* Source: CoDUOMP.exe 0x00501100..0x00501895.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00501100_00501895.mcode.
 * Name and signature: exact same-module Mac symbol ParseShader.
 *
 * Inactive `requires` stages are parsed completely but discarded. ParseStage
 * may set shader-wide flags while doing so, so the original restores the
 * incoming flag word for an inactive stage while preserving its other parser
 * side effects. Active stages retain a private eight-bundle texmod bank. */
qboolean ParseShader(char **text, qboolean allowTextureName, renderer_image_track_t imageTrack)
{
    /* Exact source constant at 0x005b9c90 (0x3c8efa35): degrees to radians. */
    const float degreesToRadians = 0.017453292384743690f;
    int32_t activeStageCount = 0;
    const char *token = Com_Parse(text);

    if (token[0] != '{') {
        ri.Printf(R_PRINT_WARNING, "WARNING: expecting '{', found '%s' instead in shader '%s'\n", token, rendererParsedShader.name);
        return qfalse;
    }

    for (;;) {
        token = Com_Parse(text);
        if (token[0] == '\0') {
            ri.Printf(R_PRINT_WARNING, "WARNING: no concluding '}' in shader %s\n", rendererParsedShader.name);
            return qfalse;
        }

        if (token[0] == '}') {
            if (activeStageCount == 0 && (rendererParsedShader.flags & SHADER_FLAG_SKY) == 0) {
                return qfalse;
            }

            rendererParsedShader.flags |= SHADER_FLAG_EXPLICITLY_DEFINED;
            return qtrue;
        }

        if (token[0] == '{') {
            shaderStage_t temporaryStage;
            texModInfo_t temporaryTexMods[R_MAX_TEXTURE_UNITS][R_MAX_TEXTURE_MODIFIERS];
            qboolean stageActive;
            const uint32_t incomingShaderFlags = rendererParsedShader.flags;

            memset(&temporaryStage, 0, sizeof(temporaryStage));
            for (int32_t bundleIndex = 0; bundleIndex < R_MAX_TEXTURE_UNITS; ++bundleIndex) {
                temporaryStage.bundle[bundleIndex].texMods = temporaryTexMods[bundleIndex];
            }

            if (ParseStage(&temporaryStage, text, allowTextureName, imageTrack, &stageActive) == qfalse) {
                return qfalse;
            }

            if (stageActive != qfalse) {
                shaderStage_t *const stage = &rendererParsedShaderStages[activeStageCount];
                texModInfo_t(*const texMods)[R_MAX_TEXTURE_MODIFIERS] = rendererParsedShaderTexMods[activeStageCount];

                /* The original copies all four slots for all eight bundle;
                 * only entries below each bundle's numTexMods are live. */
                memcpy(texMods, temporaryTexMods, sizeof(temporaryTexMods));
                memcpy(stage, &temporaryStage, sizeof(*stage));
                for (int32_t bundleIndex = 0; bundleIndex < R_MAX_TEXTURE_UNITS; ++bundleIndex) {
                    stage->bundle[bundleIndex].texMods = texMods[bundleIndex];
                }
                stage->flags |= SHADER_STAGE_ACTIVE;
                ++activeStageCount;
            } else {
                rendererParsedShader.flags = incomingShaderFlags;
            }
            continue;
        }

        if (Q_stricmpn(token, "qer", 3) == 0 || Q_stricmpn(token, "radialNormals", R_SHADER_TOKEN_COMPARE_LIMIT) == 0) {
            Com_SkipRestOfLine(text);
            continue;
        }

        if (Q_stricmpn(token, "q3map_sun", R_SHADER_TOKEN_COMPARE_LIMIT) == 0) {
            float intensity;
            float azimuth;
            float elevation;
            float elevationCosine;

            tr.sunLight[0] = (float)atof(Com_ParseOnLine(text));
            tr.sunLight[1] = (float)atof(Com_ParseOnLine(text));
            tr.sunLight[2] = (float)atof(Com_ParseOnLine(text));
            (void)VectorNormalize(tr.sunLight);

            intensity = (float)atof(Com_ParseOnLine(text));
            tr.sunLight[0] *= intensity;
            tr.sunLight[1] *= intensity;
            tr.sunLight[2] *= intensity;

            azimuth = (float)atof(Com_ParseOnLine(text)) * degreesToRadians;
            elevation = (float)atof(Com_ParseOnLine(text)) * degreesToRadians;
            elevationCosine = cosf(elevation);
            tr.sunDirection[0] = cosf(azimuth) * elevationCosine;
            tr.sunDirection[1] = sinf(azimuth) * elevationCosine;
            tr.sunDirection[2] = sinf(elevation);
            continue;
        }

        if (Q_stricmpn(token, "deformVertexes", R_SHADER_TOKEN_COMPARE_LIMIT) == 0) {
            ParseDeform(text);
            continue;
        }

        if (Q_stricmpn(token, "tesssize", R_SHADER_TOKEN_COMPARE_LIMIT) == 0) {
            Com_SkipRestOfLine(text);
            continue;
        }

        if (Q_stricmp(token, "clampTime") == 0) {
            token = Com_ParseOnLine(text);
            if (token[0] != '\0')
                rendererParsedShader.clampTime = (float)atof(token);
            continue;
        }

        if (Q_stricmpn(token, "q3map", 5) == 0) {
            Com_SkipRestOfLine(text);
            continue;
        }

        if (Q_stricmp(token, "surfaceParm") == 0) {
            ParseSurfaceParm(text);
            continue;
        }

        if (Q_stricmp(token, "nomipmaps") == 0) {
            rendererParsedShader.flags |= SHADER_FLAG_NO_MIPMAPS;
            continue;
        }

        if (Q_stricmp(token, "nolightscale") == 0) {
            rendererParsedShader.flags |= SHADER_FLAG_NO_IMAGE_OVERBRIGHT;
            continue;
        }

        if (Q_stricmp(token, "nopicmip") == 0) {
            rendererParsedShader.flags |= SHADER_FLAG_NO_PICMIP;
            continue;
        }

        if (Q_stricmp(token, "picmip2") == 0) {
            rendererParsedShader.flags |= SHADER_FLAG_USE_PICMIP2;
            continue;
        }

        if (Q_stricmp(token, "polygonOffset") == 0) {
            rendererParsedShader.flags = (rendererParsedShader.flags & ~SHADER_FLAG_POLYGON_OFFSET_MASK) | SHADER_FLAG_POLYGON_OFFSET;
            continue;
        }

        if (Q_stricmp(token, "polygonOffset2") == 0) {
            rendererParsedShader.flags =
                (rendererParsedShader.flags & ~SHADER_FLAG_POLYGON_OFFSET_MASK) | SHADER_FLAG_POLYGON_OFFSET_DOUBLE;
            continue;
        }

        if (Q_stricmp(token, "polygonOffsetConst") == 0) {
            rendererParsedShader.flags =
                (rendererParsedShader.flags & ~SHADER_FLAG_POLYGON_OFFSET_MASK) | SHADER_FLAG_POLYGON_OFFSET_CONSTANT;
            continue;
        }

        if (Q_stricmp(token, "entityMergable") == 0) {
            rendererParsedShader.flags |= SHADER_FLAG_ENTITY_MERGABLE;
            continue;
        }

        if (Q_stricmp(token, "fogParms") == 0) {
            if (ParseVector(text, 3, rendererParsedShader.fogColor) == qfalse) {
                return qfalse;
            }

            token = Com_ParseOnLine(text);
            if (token[0] == '\0') {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: missing parm for 'fogParms' "
                          "keyword in shader '%s'\n",
                          rendererParsedShader.name);
                continue;
            }

            rendererParsedShader.fogDepthForOpaque = (float)atof(token);
            Com_SkipRestOfLine(text);
            continue;
        }

        if (Q_stricmp(token, "portal") == 0) {
            rendererParsedShader.surfaceFlags |= SHADER_SURFACE_REQUIRES_VERTEX_BASIS;
            rendererParsedShader.sort = SHADER_SORT_PORTAL;
            continue;
        }

        if (Q_stricmp(token, "skyparms") == 0) {
            ParseSkyParms(text, imageTrack);
            continue;
        }

        if (Q_stricmp(token, "sunfile") == 0) {
            token = Com_ParseOnLine(text);
            if (token[0] == '\0') {
                ri.Printf(R_PRINT_WARNING, "WARNING: missing sun name for 'sunfile'\n");
                continue;
            }

            tr.sunName[R_WORLD_NAME_SIZE - 1] = '\0';
            strncpy(tr.sunName, token, sizeof(tr.sunName));
            if (tr.sunName[R_WORLD_NAME_SIZE - 1] != '\0') {
                ri.Printf(R_PRINT_WARNING, "WARNING: name '%s' too long for sunfile\n", token);
                tr.sunName[0] = '\0';
            }
            continue;
        }

        if (Q_stricmp(token, "nofog") == 0) {
            rendererParsedShader.flags |= SHADER_FLAG_NO_FOG;
            continue;
        }

        if (Q_stricmp(token, "light") == 0) {
            (void)Com_ParseOnLine(text);
            continue;
        }

        if (Q_stricmp(token, "cull") == 0) {
            token = Com_ParseOnLine(text);
            if (token[0] == '\0') {
                ri.Printf(R_PRINT_WARNING, "WARNING: missing cull parms in shader '%s'\n", rendererParsedShader.name);
                continue;
            }

            if (Q_stricmp(token, "none") == 0 || Q_stricmp(token, "twosided") == 0 || Q_stricmp(token, "disable") == 0) {
                rendererParsedShader.cullType = CT_TWO_SIDED;
            } else if (Q_stricmp(token, "back") == 0 || Q_stricmp(token, "backside") == 0 || Q_stricmp(token, "backsided") == 0) {
                rendererParsedShader.cullType = CT_BACK_SIDED;
            } else {
                ri.Printf(R_PRINT_WARNING,
                          "WARNING: invalid cull parm '%s' "
                          "in shader '%s'\n",
                          token, rendererParsedShader.name);
            }
            continue;
        }

        if (Q_stricmp(token, "sort") == 0) {
            if (ParseSort(text) == qfalse)
                return qfalse;
            continue;
        }

        ri.Printf(R_PRINT_WARNING, "WARNING: unknown general shader parameter '%s' in '%s'\n", token, rendererParsedShader.name);
        return qfalse;
    }
}

/* Source: CoDUOMP.exe 0x005018a0..0x005018bd.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005018a0_005018be.mcode.
 * Name and no-argument signature: exact same-module Mac symbol
 * ComputeStageIteratorFunc. */
void ComputeStageIteratorFunc(void)
{
    rendererParsedShader.optimalStageIteratorFunc = tr.stageIteratorFunc;
    if ((rendererParsedShader.flags & SHADER_FLAG_SKY) != 0)
        rendererParsedShader.optimalStageIteratorFunc = RB_StageIteratorSky;
}

/* Source: CoDUOMP.exe 0x005018c0..0x00501b35.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005018c0_00501b35.mcode and its switch
 * tables at 0x00501b38..0x00501b85.
 * Name and no-argument signature: exact same-module Mac symbol
 * ComputeHardwareNeeds.
 *
 * This pass converts parser choices into the per-stage array flags consumed
 * by the fixed-function backends and the shader-wide surface requirements
 * used when choosing and building vertex formats. Active stages and bundle
 * are contiguous; the first inactive stage or zero texture-env mode ends the
 * corresponding scan. */
void ComputeHardwareNeeds(void)
{
    rendererParsedShader.lightingFlags = 0;

    for (int32_t stageIndex = 0; stageIndex < R_MAX_SHADER_STAGES; ++stageIndex) {
        shaderStage_t *const stage = &rendererParsedShaderStages[stageIndex];

        if ((stage->flags & SHADER_STAGE_ACTIVE) == 0)
            break;

        stage->flags |= SHADER_STAGE_HARDWARE_NEEDS_COMPUTED;
        switch (stage->rgbGen) {
        case CGEN_IDENTITY_LIGHTING:
        case CGEN_LIGHTING_PRECALC:
        case CGEN_CONSTANT:
            rendererParsedShader.surfaceFlags |= SHADER_SURFACE_COLOR_INPUT;
            break;

        case CGEN_IDENTITY:
            break;

        case CGEN_EXACT_VERTEX:
        case CGEN_VERTEX:
        case CGEN_ONE_MINUS_VERTEX:
            rendererParsedShader.surfaceFlags |= SHADER_SURFACE_COLOR_INPUT;
            stage->flags |= SHADER_STAGE_COLOR_ARRAY;
            break;

        case CGEN_LIGHTING_AMBIENT:
            rendererParsedShader.surfaceFlags |= SHADER_SURFACE_REQUIRES_VERTEX_BASIS | SHADER_SURFACE_REQUIRES_NORMAL_ARRAY;
            stage->flags |= SHADER_LIGHTING_AMBIENT | SHADER_STAGE_NORMAL_ARRAY;
            break;

        case CGEN_LIGHTING_DIFFUSE:
            rendererParsedShader.surfaceFlags |= SHADER_SURFACE_REQUIRES_VERTEX_BASIS | SHADER_SURFACE_REQUIRES_NORMAL_ARRAY;
            stage->flags |= SHADER_LIGHTING_DIFFUSE | SHADER_STAGE_NORMAL_ARRAY;
            break;

        default:
            rendererParsedShader.surfaceFlags |= SHADER_SURFACE_COLOR_INPUT | SHADER_SURFACE_DYNAMIC_COLORS;
            stage->flags |= SHADER_STAGE_COLOR_ARRAY;
            break;
        }

        switch (stage->alphaGen) {
        case AGEN_UNSPECIFIED:
        case AGEN_IDENTITY:
            break;

        case AGEN_CONSTANT:
            rendererParsedShader.surfaceFlags |= SHADER_SURFACE_COLOR_INPUT;
            break;

        case AGEN_VERTEX:
        case AGEN_ONE_MINUS_VERTEX:
            rendererParsedShader.surfaceFlags |= SHADER_SURFACE_COLOR_INPUT;
            stage->flags |= SHADER_STAGE_COLOR_ARRAY;
            break;

        default:
            rendererParsedShader.surfaceFlags |= SHADER_SURFACE_COLOR_INPUT | SHADER_SURFACE_DYNAMIC_COLORS;
            stage->flags |= SHADER_STAGE_COLOR_ARRAY;
            break;
        }

        if ((stage->flags & SHADER_STAGE_COLOR_ARRAY) == 0) {
            if (stage->rgbGen == CGEN_IDENTITY_LIGHTING) {
                stage->constantColor[0] = (uint8_t)tr.identityLightByte;
                stage->constantColor[1] = (uint8_t)tr.identityLightByte;
                stage->constantColor[2] = (uint8_t)tr.identityLightByte;
            } else if (stage->rgbGen == CGEN_IDENTITY) {
                stage->constantColor[0] = UINT8_MAX;
                stage->constantColor[1] = UINT8_MAX;
                stage->constantColor[2] = UINT8_MAX;
            }

            if (stage->alphaGen == AGEN_UNSPECIFIED || stage->alphaGen == AGEN_IDENTITY) {
                stage->constantColor[3] = UINT8_MAX;
            }
        }

        int32_t bundleCount = 0;
        while (bundleCount < R_MAX_TEXTURE_UNITS && stage->bundle[bundleCount].textureEnvMode != 0) {
            rendererParsedShader.surfaceFlags |= SHADER_SURFACE_TEXTURE_UNIT0 << (uint32_t)bundleCount;
            stage->flags |= SHADER_STAGE_TEXCOORD_ARRAY0 << (uint32_t)bundleCount;
            ++bundleCount;
        }

        for (int32_t bundleIndex = 0; bundleIndex < bundleCount; ++bundleIndex) {
            textureBundle_t *const bundle = &stage->bundle[bundleIndex];
            const uint32_t generatedTexcoordFlag = SHADER_SURFACE_GENERATED_TEXCOORD0 << (uint32_t)bundleIndex;

            if (bundle->numImageAnimations > 1) {
                rendererParsedShader.surfaceFlags |= SHADER_SURFACE_TIME_DEPENDENT | SHADER_SURFACE_ANIMATED_TEXTURES;
            } else {
                bundle->numImageAnimations = 0;
            }

            if (bundle->tcGen <= TCGEN_BAD || bundle->tcGen > TCGEN_TEXTURE) {
                rendererParsedShader.surfaceFlags |= generatedTexcoordFlag;
            }

            for (int32_t texModIndex = 0; texModIndex < bundle->numTexMods; ++texModIndex) {
                switch (bundle->texMods[texModIndex].type) {
                case TMOD_TURBULENT:
                case TMOD_SCROLL:
                case TMOD_STRETCH:
                case TMOD_ROTATE:
                case TMOD_ENTITY_TRANSLATE:
                    rendererParsedShader.surfaceFlags |= generatedTexcoordFlag;
                    break;

                default:
                    break;
                }
            }
        }

        rendererParsedShader.lightingFlags |= stage->flags;
    }

    const uint32_t lightingGeneratorFlags = rendererParsedShader.lightingFlags & SHADER_LIGHTING_ENTITY_MASK;
    if (lightingGeneratorFlags != 0 && (lightingGeneratorFlags & (lightingGeneratorFlags - 1)) != 0) {
        ri.Printf(R_PRINT_ALL,
                  "WARNING: shader '%s' uses more than one of rgbGen "
                  "lightingAmbient, lightingDiffuse, and lightingSpecular\n",
                  rendererParsedShader.name);
    }

    for (int32_t deformIndex = 0; deformIndex < rendererParsedShader.numDeforms; ++deformIndex) {
        if (rendererParsedShader.deforms[deformIndex].deformation == DEFORM_NORMALS) {
            rendererParsedShader.surfaceFlags |= SHADER_SURFACE_DEFORMED_NORMALS;
        } else {
            rendererParsedShader.surfaceFlags |= SHADER_SURFACE_DEFORMED_POSITIONS;
        }
    }
}

/* Source: CoDUOMP.exe 0x00501b90..0x00501c83.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00501b90_00501c83.mcode and the
 * blend-state acceptance table at 0x00501c8c..0x00501cf1.
 * Name and no-argument signature: exact same-module Mac symbol
 * CreateDlightStage.
 *
 * A conventional single-pass base-texture/lightmap shader can reuse a copy
 * of that pass for per-entity lights. The copied pass retains the base image,
 * replaces its lightmap with the renderer dynamic-light image, and switches
 * to an additive blend. Source-alpha shaders retain source-alpha weighting.
 * The image load at 0x00501c1a reads tr.dlightImage from original address
 * 0x048850bc; tr.grayImage is the distinct built-in at 0x048850cc. */
void CreateDlightStage(void)
{
    shaderStage_t *const sourceStage = &rendererParsedShaderStages[0];
    shaderStage_t *const dlightStage = &rendererParsedShaderStages[1];
    const uint32_t blendBits = sourceStage->stateBits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS);

    if (rendererParsedShader.numUnfoggedPasses != 1)
        return;
    if (sourceStage->bundle[0].image[0] == NULL || sourceStage->bundle[1].image[0] == NULL || sourceStage->bundle[2].image[0] != NULL) {
        return;
    }
    if (sourceStage->bundle[0].isLightmap != 0 || sourceStage->bundle[1].isLightmap == 0) {
        return;
    }
    if (sourceStage->bundle[0].textureEnvMode != GL_MODULATE || sourceStage->bundle[1].textureEnvMode != GL_MODULATE) {
        return;
    }

    switch (blendBits) {
    case 0:
    case GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO:
    case GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE:
    case GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE:
    case GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA:
        break;
    default:
        return;
    }

    *dlightStage = *sourceStage;
    dlightStage->bundle[1].image[0] = tr.dlightImage;
    dlightStage->stateBits &= ~(GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS);
    if ((blendBits & GLS_SRCBLEND_BITS) == GLS_SRCBLEND_SRC_ALPHA) {
        dlightStage->stateBits |= GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE;
    } else {
        dlightStage->stateBits |= GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
    }

    dlightStage->flags |= SHADER_STAGE_PER_LIGHT;
    rendererParsedShader.lightingFlags |= dlightStage->flags;
    rendererParsedShader.numUnfoggedPasses = 2;
}

/* Source: CoDUOMP.exe 0x00501d00..0x00501f51.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00501d00_00501f51.mcode and the exact
 * rule table at 0x005912f0..0x0059137f.
 * Name and no-argument qboolean signature: exact same-module Mac symbol
 * CollapseMultitexture.
 *
 * The first two active stages can be collapsed when their non-coordinate
 * hardware requirements, blending, generators, and waveforms are compatible.
 * The second stage's sole bundle is appended to the first stage, and the
 * remaining scratch stages are shifted down. FinishShader owns the matching
 * decrement of its live-stage count after a successful return. */
qboolean CollapseMultitexture(void)
{
    shaderStage_t *const firstStage = &rendererParsedShaderStages[0];
    shaderStage_t *const secondStage = &rendererParsedShaderStages[1];
    int32_t bundleCounts[2];
    const shader_multitexture_collapse_rule_t *rule = NULL;
    const uint32_t ignoredStageFlags = (SHADER_STAGE_TEXCOORD_ARRAY0 << 1) - 1;

    if (qglActiveTextureARB == NULL || (firstStage->flags & SHADER_STAGE_ACTIVE) == 0 || (secondStage->flags & SHADER_STAGE_ACTIVE) == 0) {
        return qfalse;
    }
    if (((firstStage->flags ^ secondStage->flags) & ~ignoredStageFlags) != 0) {
        return qfalse;
    }

    for (int32_t stageIndex = 0; stageIndex < 2; ++stageIndex) {
        const shaderStage_t *const stage = &rendererParsedShaderStages[stageIndex];
        int32_t bundleCount = 0;

        while (bundleCount < R_MAX_TEXTURE_UNITS && stage->bundle[bundleCount].textureEnvMode != 0) {
            ++bundleCount;
        }
        bundleCounts[stageIndex] = bundleCount;
    }

    if (bundleCounts[1] != 1 || bundleCounts[0] + 1 > glConfig.maxActiveTextures) {
        return qfalse;
    }

    const uint32_t firstBlendBits = firstStage->stateBits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS);
    const uint32_t secondBlendBits = secondStage->stateBits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS);
    for (size_t ruleIndex = 0; ruleIndex < sizeof(shaderMultitextureCollapseRules) / sizeof(shaderMultitextureCollapseRules[0]);
         ++ruleIndex) {
        const shader_multitexture_collapse_rule_t *const candidate = &shaderMultitextureCollapseRules[ruleIndex];
        if (candidate->firstBlendBits == firstBlendBits && candidate->secondBlendBits == secondBlendBits) {
            rule = candidate;
            break;
        }
    }
    if (rule == NULL)
        return qfalse;

    if (rule->textureEnvMode == GL_ADD) {
        if (glConfig.textureEnvAddAvailable == qfalse || firstStage->rgbGen != CGEN_IDENTITY) {
            return qfalse;
        }
    }

    if (firstStage->rgbGen != secondStage->rgbGen || firstStage->alphaGen != secondStage->alphaGen) {
        if (rule->textureEnvMode != GL_MODULATE || secondStage->rgbGen != CGEN_IDENTITY ||
            (secondStage->alphaGen != AGEN_UNSPECIFIED && secondStage->alphaGen != AGEN_IDENTITY)) {
            return qfalse;
        }
    }

    if (firstStage->rgbGen == CGEN_WAVEFORM && memcmp(&firstStage->rgbWave, &secondStage->rgbWave, sizeof(firstStage->rgbWave)) != 0) {
        return qfalse;
    }
    if (firstStage->alphaGen == AGEN_WAVEFORM &&
        memcmp(&firstStage->alphaWave, &secondStage->alphaWave, sizeof(firstStage->alphaWave)) != 0) {
        return qfalse;
    }

    if (bundleCounts[0] == 1 && firstStage->bundle[0].isLightmap != 0) {
        const textureBundle_t lightmapBundle = firstStage->bundle[0];
        firstStage->bundle[0] = secondStage->bundle[0];
        firstStage->bundle[1] = lightmapBundle;
    } else {
        firstStage->bundle[bundleCounts[0]] = secondStage->bundle[0];
    }
    firstStage->bundle[bundleCounts[0]].textureEnvMode = rule->textureEnvMode;
    firstStage->stateBits &= ~(GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS);
    firstStage->stateBits |= rule->collapsedBlendBits;

    memmove(&rendererParsedShaderStages[1], &rendererParsedShaderStages[2],
            sizeof(rendererParsedShaderStages[0]) * (R_MAX_SHADER_STAGES - 2));
    memset(&rendererParsedShaderStages[R_MAX_SHADER_STAGES - 1], 0, sizeof(rendererParsedShaderStages[0]));
    return qtrue;
}

/* Source: CoDUOMP.exe 0x00501f60..0x00502216.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00501f60_00502216.mcode.
 * Name and no-argument signature: exact same-module Mac symbol SortNewShader.
 *
 * The newest shader already occupies the last slot in tr.shaders. Insert it
 * into the ascending sort-order table, using the first two texture object
 * names as deterministic tie breakers. The Windows compiler shifts four
 * entries per iteration when possible; this source loop preserves the same
 * comparison and stable-insertion behavior. */
void SortNewShader(void)
{
    shader_t *const newShader = tr.shaders[tr.numShaders - 1];
    const float newSort = newShader->sort;
    uint32_t newFirstTexture = 0;
    uint32_t newSecondTexture = 0;
    int32_t sortedIndex;

    if (newShader->stages[0] != NULL) {
        shaderStage_t *const firstStage = newShader->stages[0];
        if (firstStage->bundle[0].image[0] != NULL) {
            newFirstTexture = firstStage->bundle[0].image[0]->texnum;
        }
        if (firstStage->bundle[1].image[0] != NULL) {
            newSecondTexture = firstStage->bundle[1].image[0]->texnum;
        }
    }

    for (sortedIndex = tr.numShaders - 2; sortedIndex >= 0; --sortedIndex) {
        shader_t *const existingShader = tr.sortedShaders[sortedIndex];

        if (existingShader->sort < newSort)
            break;

        if (existingShader->sort == newSort && existingShader->stages[0] != NULL) {
            shaderStage_t *const firstStage = existingShader->stages[0];
            image_t *const firstImage = firstStage->bundle[0].image[0];

            if (firstImage != NULL) {
                if (firstImage->texnum < newFirstTexture)
                    break;
                if (firstImage->texnum != newFirstTexture)
                    goto shift_existing_shader;
            }

            image_t *const secondImage = firstStage->bundle[1].image[0];
            if (secondImage != NULL && secondImage->texnum <= newSecondTexture) {
                break;
            }
        }

    shift_existing_shader:
        tr.sortedShaders[sortedIndex + 1] = existingShader;
        ++existingShader->sortedIndex;
    }

    newShader->sortedIndex = sortedIndex + 1;
    tr.sortedShaders[sortedIndex + 1] = newShader;
}

/* Source: CoDUOMP.exe 0x00502220..0x005023fd.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00502220_005023fd.mcode.
 * Name and return type: exact same-module Mac symbol
 * GeneratePermanentShader.
 *
 * Promote the parser scratch shader into hunk-owned storage, including an
 * independent copy of every active stage and texture-modifier array. The new
 * shader is installed in handle order, sorted for draw submission, and linked
 * into the name hash. */
shader_t *GeneratePermanentShader(void)
{
    if (tr.numShaders == R_MAX_SHADERS) {
        ri.Printf(R_PRINT_WARNING, "WARNING: GeneratePermanentShader - MAX_SHADERS hit\n");
        UpdateDelayLoadImagesForShader(&rendererParsedShader, qtrue);
        return tr.defaultShader;
    }

    shader_t *const shader = ri.Hunk_Alloc(sizeof(*shader));
    *shader = rendererParsedShader;

    tr.shaders[tr.numShaders] = shader;
    shader->index = tr.numShaders;
    tr.sortedShaders[tr.numShaders] = shader;
    shader->sortedIndex = tr.numShaders;
    tr.numShaders = (int32_t)((uint32_t)tr.numShaders + 1u);

    for (int32_t stageIndex = 0; stageIndex < shader->numUnfoggedPasses; ++stageIndex) {
        const shaderStage_t *const scratchStage = &rendererParsedShaderStages[stageIndex];

        if ((scratchStage->flags & SHADER_STAGE_ACTIVE) == 0) {
            shader->stages[stageIndex] = NULL;
            break;
        }

        shaderStage_t *const permanentStage = ri.Hunk_Alloc(sizeof(*permanentStage));
        *permanentStage = *scratchStage;
        shader->stages[stageIndex] = permanentStage;

        for (int32_t bundleIndex = 0; bundleIndex < R_MAX_TEXTURE_UNITS; ++bundleIndex) {
            const textureBundle_t *const scratchBundle = &scratchStage->bundle[bundleIndex];
            textureBundle_t *const permanentBundle = &permanentStage->bundle[bundleIndex];

            if (permanentBundle->numTexMods == 0) {
                permanentBundle->texMods = NULL;
                continue;
            }

            const uint32_t texModBytes = (uint32_t)permanentBundle->numTexMods * (uint32_t)sizeof(*permanentBundle->texMods);
            permanentBundle->texMods = ri.Hunk_Alloc(texModBytes);
            memcpy(permanentBundle->texMods, scratchBundle->texMods, texModBytes);
        }
    }

    UpdateDelayLoadImagesForShader(shader, MergableShader(shader) == qfalse);
    SortNewShader();

    const uint32_t hash = R_GenerateShaderHashValue(shader->name);
    shader->next = rendererShaderHashTable[hash];
    rendererShaderHashTable[hash] = shader;
    return shader;
}

/* Source: CoDUOMP.exe 0x00502400..0x005027ae.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00502400_005027ae.mcode.
 * Name and return type: exact same-module Mac symbol FinishShader.
 *
 * Normalize the parsed stage bank, discard disabled detail stages, derive
 * missing texture-coordinate generators and fixed-function state, collapse
 * compatible passes, and select the final iterator before promoting the
 * shader into permanent storage. */
shader_t *FinishShader(void)
{
    int32_t stageIndex = 0;
    qboolean hasLightmapStage = qfalse;

    if ((rendererParsedShader.flags & SHADER_FLAG_SKY) != 0)
        rendererParsedShader.sort = SHADER_SORT_SKY;
    if ((rendererParsedShader.flags & SHADER_FLAG_POLYGON_OFFSET_MASK) != 0 && rendererParsedShader.sort == SHADER_SORT_BAD) {
        rendererParsedShader.sort = SHADER_SORT_DECAL;
    }

    while (stageIndex < R_MAX_SHADER_STAGES) {
        shaderStage_t *stage = &rendererParsedShaderStages[stageIndex];

        if ((stage->flags & SHADER_STAGE_ACTIVE) == 0)
            break;

        if ((stage->flags & SHADER_STAGE_DETAIL) != 0 && r_detailtextures->integer == 0) {
            if (stageIndex < R_MAX_SHADER_STAGES - 1) {
                memmove(&rendererParsedShaderStages[stageIndex], &rendererParsedShaderStages[stageIndex + 1],
                        sizeof(rendererParsedShaderStages[0]) * (size_t)(R_MAX_SHADER_STAGES - stageIndex - 1));

                for (int32_t lastStage = R_MAX_SHADER_STAGES - 1; lastStage > stageIndex; --lastStage) {
                    if ((rendererParsedShaderStages[lastStage].flags & SHADER_STAGE_ACTIVE) != 0) {
                        memset(&rendererParsedShaderStages[lastStage], 0, sizeof(rendererParsedShaderStages[0]));
                        break;
                    }
                }
                --stageIndex;
            } else {
                memset(stage, 0, sizeof(*stage));
            }
            ++stageIndex;
            continue;
        }

        for (int32_t bundleIndex = 0; bundleIndex < R_MAX_TEXTURE_UNITS; ++bundleIndex) {
            textureBundle_t *const bundle = &stage->bundle[bundleIndex];

            for (int32_t imageIndex = 0; imageIndex < bundle->numImageAnimations; ++imageIndex) {
                if (bundle->image[imageIndex] == NULL) {
                    ri.Printf(R_PRINT_WARNING, "Shader %s has a missing image\n", rendererParsedShader.name);
                    stage->flags &= ~SHADER_STAGE_ACTIVE;
                    goto next_stage;
                }
                if (bundle->image[imageIndex]->target != bundle->image[0]->target) {
                    ri.Printf(R_PRINT_WARNING,
                              "Shader %s has non-uniform image types in an "
                              "animMap\n",
                              rendererParsedShader.name);
                    stage->flags &= ~SHADER_STAGE_ACTIVE;
                    goto next_stage;
                }
            }

            if (bundle->isLightmap != 0) {
                if (bundle->tcGen == TCGEN_BAD)
                    bundle->tcGen = TCGEN_LIGHTMAP;
                hasLightmapStage = qtrue;
            } else if (bundle->image[0] != NULL && bundle->image[0]->target == GL_TEXTURE_CUBE_MAP_ARB) {
                if (bundle->tcGen == TCGEN_BAD) {
                    bundle->tcGen = TCGEN_CUBEMAP_REFLECTION;
                }
            } else if (bundle->tcGen == TCGEN_BAD) {
                bundle->tcGen = TCGEN_TEXTURE;
            }
        }

        if (rendererParsedShader.sort == SHADER_SORT_WATER) {
            /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): retail water
             * shaders select either blended NVIDIA/ATI stages or an opaque
             * fixed-function fallback. Blending clears depth writes, but the
             * fallback otherwise retains them and can mask companion shoreline
             * geometry on hardware without those obsolete vendor extensions.
             * Give every water-sorted path the accelerated path's depth-mask
             * behavior without depending on a map or shader name. */
            stage->stateBits &= ~GLS_DEPTHMASK_TRUE;
        }

        if ((rendererParsedShader.flags & SHADER_FLAG_POLYGON_OFFSET_DOUBLE) != 0) {
            stage->stateBits |= GLS_POLYGON_OFFSET_DOUBLE;
        } else if ((rendererParsedShader.flags & SHADER_FLAG_POLYGON_OFFSET) != 0) {
            stage->stateBits |= GLS_POLYGON_OFFSET;
        } else if ((rendererParsedShader.flags & SHADER_FLAG_POLYGON_OFFSET_CONSTANT) != 0) {
            stage->stateBits |= GLS_POLYGON_OFFSET_ZERO_FACTOR;
        }

        if ((rendererParsedShader.flags & SHADER_FLAG_NO_FOG) == 0 || (stage->flags & SHADER_STAGE_FOG) != 0) {
            stage->stateBits |= GLS_FOG;
        }

        if ((stage->stateBits & GLS_ATEST_BITS) == 0) {
            const uint32_t blendBits = stage->stateBits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS);
            if (blendBits == (GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE) ||
                blendBits == (GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA)) {
                stage->stateBits |= GLS_ATEST_GT_0;
            }
        }

        if ((stage->stateBits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS)) != 0 &&
            (rendererParsedShaderStages[0].stateBits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS)) != 0 &&
            rendererParsedShader.sort == SHADER_SORT_BAD) {
            if ((stage->stateBits & GLS_DEPTHMASK_TRUE) != 0) {
                rendererParsedShader.sort = SHADER_SORT_SEE_THROUGH;
            } else if ((rendererParsedShaderStages[0].stateBits & GLS_DSTBLEND_BITS) == GLS_DSTBLEND_ONE) {
                rendererParsedShader.sort = SHADER_SORT_ADDITIVE;
            } else {
                rendererParsedShader.sort = SHADER_SORT_BLEND;
            }
        }

    next_stage:
        ++stageIndex;
    }

    if (rendererParsedShader.sort == SHADER_SORT_BAD)
        rendererParsedShader.sort = SHADER_SORT_OPAQUE;

    while (stageIndex > 1 && CollapseMultitexture() != qfalse) {
        --stageIndex;
    }

    if (rendererParsedShader.lightmapIndex >= 0 && hasLightmapStage == qfalse) {
        ri.Printf(R_PRINT_ALL, "WARNING: shader '%s' has lightmap but no lightmap stage!\n", rendererParsedShader.name);
        rendererParsedShader.lightmapIndex = -1;
    }

    rendererParsedShader.numUnfoggedPasses = stageIndex;
    if (stageIndex == 0)
        rendererParsedShader.sort = SHADER_SORT_FOG;

    if ((rendererParsedShader.lightingFlags & SHADER_LIGHTING_PER_ENTITY) == 0) {
        CreateDlightStage();
    }
    ComputeHardwareNeeds();
    ComputeStageIteratorFunc();
    return GeneratePermanentShader();
}
