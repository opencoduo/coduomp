#include "backend.h"

#include "gl_api.h"
#include "gl_state.h"
#include "renderer_cvars.h"
#include "compat/coduo_native_x87.h"
#include "../math/vector_math.h"
#include "../platform/crt_boundary.h"

#include <math.h>
#include <string.h>

enum renderer_sky_box_set_e {
    R_SKY_BOX_OUTER = 0,
    R_SKY_BOX_INNER = 1
};

enum {
    R_SUN_SURFACE_VERTEX_COMPONENTS = 4,
    R_SKY_SURFACE_VERTEX_COMPONENTS = 3,
    R_SUN_FLARE_ID = -1,
    R_SKY_GRID_SIDE = 9,
    R_SKY_GRID_CENTER = 4,
    R_SKY_TRIANGLE_VERTEX_COUNT = 3,
    R_SKY_INITIAL_CLIP_STAGE = 0,
    R_SKY_CLIP_PLANE_COUNT = 6,
    R_SKY_MAX_CLIP_VERTICES = 64,
    R_SKY_MAX_INPUT_CLIP_VERTICES = R_SKY_MAX_CLIP_VERTICES - 2,
    R_SKY_SUBDIVISION_COUNT = R_SKY_GRID_SIDE - 1,
    R_SUN_CVAR_COUNT = 20,
    R_SUN_CVAR_GROUP_PREFIX_LENGTH = 8,
    R_SUN_FILE_BUFFER_SIZE = 65536,
    R_SUN_INITIAL_UPDATE_MSEC = 10,
    R_SUN_SPRITE_SHADER_LIGHTMAP_MODE = -3,
    R_SUN_SPRITE_SHADER_USAGE = 4,
    R_SKY_VERTEX_COUNT =
        R_SKYBOX_FACE_COUNT * R_SKY_GRID_SIDE * R_SKY_GRID_SIDE,
    R_SKY_MAX_INDEX_COUNT =
        (R_SKY_GRID_SIDE - 1) * (R_SKY_GRID_SIDE - 1) * 4
};

/* The original descriptor is rendererWorldData.skyVertexStorage at
 * 0x0388bfa8, reached through the fixed pointer at 0x0389c4d0.
 * R_BuildSkyBox fills it according to the selected static-memory backend. */
#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(renderer_sky_vertex_t) == 0x04,
               "i386 sky-vertex alignment changed");
_Static_assert(offsetof(renderer_sky_vertex_t, texCoord) == 0x00,
               "i386 sky-vertex texture-coordinate offset changed");
_Static_assert(sizeof(((renderer_sky_vertex_t *)0)->texCoord) == 0x08,
               "i386 sky-vertex texture-coordinate extent changed");
_Static_assert(offsetof(renderer_sky_vertex_t, position) == 0x08,
               "i386 sky-vertex position offset changed");
_Static_assert(sizeof(((renderer_sky_vertex_t *)0)->position) == 0x10,
               "i386 sky-vertex position extent changed");
_Static_assert(sizeof(renderer_sky_vertex_t) == 0x18,
               "original i386 sky-vertex stride changed");

_Static_assert(_Alignof(renderer_sky_vertex_base_t) == 0x04,
               "i386 sky-vertex base-union alignment changed");
_Static_assert(offsetof(renderer_sky_vertex_base_t, vertices) == 0x00,
               "i386 sky-vertex client-base offset changed");
_Static_assert(sizeof(((renderer_sky_vertex_base_t *)0)->vertices) == 0x04,
               "i386 sky-vertex client-base extent changed");
_Static_assert(offsetof(renderer_sky_vertex_base_t, bufferObject) == 0x00,
               "i386 sky-vertex object-base offset changed");
_Static_assert(sizeof(((renderer_sky_vertex_base_t *)0)->bufferObject) ==
                   0x04,
               "i386 sky-vertex object-base extent changed");
_Static_assert(sizeof(renderer_sky_vertex_base_t) == 0x04,
               "original i386 sky-vertex base-union size changed");

_Static_assert(_Alignof(renderer_sky_vertex_storage_t) == 0x04,
               "i386 sky-storage alignment changed");
_Static_assert(offsetof(renderer_sky_vertex_storage_t, memorySource) == 0x00,
               "i386 sky-storage memory-source offset changed");
_Static_assert(sizeof(((renderer_sky_vertex_storage_t *)0)->memorySource) ==
                   0x04,
               "i386 sky-storage memory-source extent changed");
_Static_assert(offsetof(renderer_sky_vertex_storage_t, backend) == 0x04,
               "i386 sky-storage backend offset changed");
_Static_assert(sizeof(((renderer_sky_vertex_storage_t *)0)->backend) == 0x04,
               "i386 sky-storage backend extent changed");
_Static_assert(offsetof(renderer_sky_vertex_storage_t, base) == 0x08,
               "i386 sky-storage base offset changed");
_Static_assert(sizeof(((renderer_sky_vertex_storage_t *)0)->base) == 0x04,
               "i386 sky-storage base extent changed");
_Static_assert(offsetof(renderer_sky_vertex_storage_t, objectOffset) == 0x0c,
               "i386 sky-storage object offset changed");
_Static_assert(sizeof(((renderer_sky_vertex_storage_t *)0)->objectOffset) ==
                   0x04,
               "i386 sky-storage object extent changed");
_Static_assert(sizeof(renderer_sky_vertex_storage_t) == 0x10,
               "original i386 sky-storage descriptor size changed");
#endif

/* Exact 0x005b9cf4 float; semantically 1.0f / 640.0f. */
static const float r_sunFlareViewportScale = 0.0015625000232830644f;
/* Exact 0x005b9d00 double; semantically 2^-30. */
static const double r_integerRoundingBias =
    9.31322574615478515625e-10;
/* Exact 0x0389c574/0x0389ada0 values used to keep sky sampling inside an
 * image's edge texels: 1/256 and 255/256. */
static const float r_skyTexCoordMin = 0.00390625f;
static const float r_skyTexCoordMax = 0.99609375f;
/* Exact cloud-sphere constants at 0x005b9ecc..0x005b9ed4. */
static const float r_skyCloudWorldRadius = 4096.0f;
static const float r_skyCloudWorldDiameter = 8192.0f;
static const float r_skyCloudWorldRadiusSquared = 16777216.0f;
/* Exact +/-pi floats at 0x005b9ca4/0x005b9ca8. */
static const float r_negativePi = -3.1415927410125732f;
static const float r_pi = 3.1415927410125732f;
/* Exact 0x005b9da8 float; semantically 1.0f / 180.0f. */
static const float r_inverseDegreesPerHalfTurn =
    0.0055555556900799274f;
/* Exact 0x005b9d30 tangent-scale float used to turn the configured angular
 * sprite size into a displacement around the unit sun direction. */
static const float r_sunSpriteAngularScale =
    0.0013110929867252707f;
/* Exact 0x005b9d2c float selecting a stable reference direction. */
static const float r_sunSpriteReferenceThreshold =
    0.9900000095367432f;

/* Original pointer tables at 0x005ceae0 and 0x005ceb30. Their order is also
 * the serialized .sun-file order and the order printed by R_SunHelp_f.
 * PE_RELOCATION_VALUES_VERIFIED: all twenty cvar-name targets match the PE. */
static const char *const rendererSunCvarNames[R_SUN_CVAR_COUNT] = {
    "r_sunsprite_shader",
    "r_sunsprite_size",
    "r_sunflare_shader",
    "r_sunflare_min_size",
    "r_sunflare_min_angle",
    "r_sunflare_max_size",
    "r_sunflare_max_angle",
    "r_sunflare_max_alpha",
    "r_sunflare_fadein",
    "r_sunflare_fadeout",
    "r_sunblind_min_angle",
    "r_sunblind_max_angle",
    "r_sunblind_max_darken",
    "r_sunblind_fadein",
    "r_sunblind_fadeout",
    "r_sunglare_min_angle",
    "r_sunglare_max_angle",
    "r_sunglare_max_lighten",
    "r_sunglare_fadein",
    "r_sunglare_fadeout"
};

/* Original pointer table 0x005ceb30.
 * PE_RELOCATION_VALUES_VERIFIED: all twenty description targets match. */
static const char *const rendererSunCvarDescriptions[R_SUN_CVAR_COUNT] = {
    "name for static sprite; can be any shader",
    "diameter in pixels at 640x480 and 80 fov",
    "name for flare effect; can be any shader",
    "smallest size of flare effect in pixels at 640x480",
    "angle from sun in degrees outside which effect is 0",
    "largest size of flare effect in pixels at 640x480",
    "angle from sun in degrees inside which effect is max",
    "0-1 vertex color and alpha of sun at max effect",
    "time in seconds to fade alpha from 0% to 100%",
    "time in seconds to fade alpha from 100% to 0%",
    "angle from sun in degres outside which blinding is 0",
    "angle from sun in degres inside which blinding is max",
    "0-1 fraction for how black the world is at max blind",
    "time in seconds to fade blind from 0% to 100%",
    "time in seconds to fade blind from 100% to 0%",
    "angle from sun in degres outside which glare is 0",
    "angle from sun in degres inside which glare is max",
    "0-1 fraction for how white the world is at max glare",
    "time in seconds to fade glare from 0% to 100%",
    "time in seconds to fade glare from 100% to 0%"
}; /* original 0x005ceb30 */

static float rendererSkyTexCoordMin;
static float rendererSkyTexCoordMax;

/* Original 0x0389b540..0x0389b59f. The first dimension is the sky s/t
 * coordinate and the second is the cube side. ClearSkyBox and the clipping
 * chain produce these bounds; DrawSkyBox snaps them to the 9x9 vertex grid. */
static float rendererSkyMins[2][R_SKYBOX_FACE_COUNT];
static float rendererSkyMaxs[2][R_SKYBOX_FACE_COUNT];

/* Original 0x0388bfa8 descriptor and its selected-pointer slot at 0x0389c4d0.
 * RE_LoadWorldMap publishes the descriptor before any sky construction. */
static renderer_sky_vertex_storage_t *rendererSkyBox;

/* Source: CoDUOMP.exe 0x005151a0..0x005151a5.
 * Name and signature: exact same-module Mac symbol R_SetSkyBox. */
void R_SetSkyBox(renderer_sky_vertex_storage_t *storage)
{
    rendererSkyBox = storage;
}

/* Original 0x0389ada8..0x0389b53f and 0x0389b5a0..0x0389c4cf.
 * R_InitSkyTexCoords fills one ray/sphere intersection scale and one angular
 * cloud coordinate for every point of every cube face. */
static float rendererSkyCloudTexP
    [R_SKYBOX_FACE_COUNT][R_SKY_GRID_SIDE][R_SKY_GRID_SIDE];
static vec2_t rendererSkyCloudTexCoords
    [R_SKYBOX_FACE_COUNT][R_SKY_GRID_SIDE][R_SKY_GRID_SIDE];

/* Exact six-dword table at original 0x005ceac4. Sky shader face arrays use a
 * different order from the clipping/grid side order. */
static const int32_t rendererSkyFaceImageOrder[R_SKYBOX_FACE_COUNT] = {
    0, 2, 1, 3, 4, 5
};

/* Original 0x005cec10 table. Positive entries select source coordinate n-1;
 * negative entries select and negate source coordinate -n-1. */
static const int32_t
    rendererSkyAxisMap[R_SKYBOX_FACE_COUNT][R_SKY_TRIANGLE_VERTEX_COUNT] = {
        { 3, -1,  2},
        {-3,  1,  2},
        { 1,  3,  2},
        {-1, -3,  2},
        {-2, -1,  3},
        { 2, -1, -3}
    };

/* Original 0x005ceb80 plane normals used by ClipSkyPolygon. */
static const vec3_t rendererSkyClipNormals[R_SKY_CLIP_PLANE_COUNT] = {
    { 1.0f,  1.0f, 0.0f},
    { 1.0f, -1.0f, 0.0f},
    { 0.0f, -1.0f, 1.0f},
    { 0.0f,  1.0f, 1.0f},
    { 1.0f,  0.0f, 1.0f},
    {-1.0f,  0.0f, 1.0f}
};

/* Original 0x005cebc8 table used to project a dominant cube face into sky
 * s/t coordinates. It uses the same signed one-based selector convention as
 * rendererSkyAxisMap. */
static const int32_t rendererSkyProjectionMap
    [R_SKYBOX_FACE_COUNT][R_SKY_TRIANGLE_VERTEX_COUNT] = {
        {-2,  3,  1},
        { 2,  3, -1},
        { 1,  3,  2},
        {-1,  3, -2},
        {-2, -1,  3},
        {-2,  1, -3}
    };

/* Exact original +/-0.1f clipping thresholds at 0x005b9cc0/0x005b9cc4. */
static const float rendererSkyClipEpsilon = 0.10000000149011612f;

renderer_sun_state_t rendererSunState;

/* Source: CoDUOMP.exe 0x00513c50..0x00513ec7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00513c50_00513ec8.mcode. Exact
 * same-module Mac symbol/signature AddSkyPolygon. Windows proves the dominant
 * axis tie ordering, signed projection table, 0.001 double threshold, and all
 * four bound comparisons. */
void AddSkyPolygon(int32_t vertexCount, vec3_t *vertices)
{
    vec3_t sum = {0.0f, 0.0f, 0.0f};
    long double sumXRaw = 0.0L;
    long double sumYRaw = 0.0L;
    vec3_t absoluteSum;
    int32_t axis;
    int32_t vertexIndex = 0;

    /* 0x00513c50..0x00513d11 is a four-way MSVC unroll. X and Y remain
     * live in x87 registers across its particular spill points while Z is
     * periodically rounded through the stack slot. Preserve that dependency
     * chain because the dominant-axis and sign decisions consume it. */
    for (; vertexIndex + 3 < vertexCount; vertexIndex += 4) {
        sumXRaw += (long double)vertices[vertexIndex][0];
        sumYRaw = (long double)sum[1] + vertices[vertexIndex][1];
        sum[2] = (float)((long double)sum[2] +
                         vertices[vertexIndex][2]);

        sumXRaw += (long double)vertices[vertexIndex + 1][0];
        sum[0] = (float)sumXRaw;
        sumYRaw += (long double)vertices[vertexIndex + 1][1];
        {
            long double sumZRaw =
                (long double)sum[2] + vertices[vertexIndex + 1][2];

            sumXRaw = (long double)sum[0] +
                      vertices[vertexIndex + 2][0];
            sum[0] = (float)sumXRaw;
            sumYRaw += (long double)vertices[vertexIndex + 2][1];
            sum[1] = (float)sumYRaw;
            sumZRaw += (long double)vertices[vertexIndex + 2][2];
            sum[2] = (float)sumZRaw;
        }

        sumXRaw = (long double)sum[0] +
                  vertices[vertexIndex + 3][0];
        sumYRaw = (long double)sum[1] +
                  vertices[vertexIndex + 3][1];
        sum[1] = (float)sumYRaw;
        sum[2] = (float)((long double)sum[2] +
                         vertices[vertexIndex + 3][2]);
    }
    for (; vertexIndex < vertexCount; ++vertexIndex) {
        sumXRaw += (long double)vertices[vertexIndex][0];
        sumYRaw += (long double)vertices[vertexIndex][1];
        sum[2] = (float)((long double)sum[2] +
                         vertices[vertexIndex][2]);
    }
    sum[0] = (float)sumXRaw;
    sum[1] = (float)sumYRaw;

    absoluteSum[0] = (float)fabsl(sumXRaw);
    absoluteSum[1] = (float)fabsl(sumYRaw);
    absoluteSum[2] = fabsf(sum[2]);
    if (absoluteSum[0] > absoluteSum[1] &&
        absoluteSum[0] > absoluteSum[2]) {
        axis = sumXRaw < 0.0L ? 1 : 0;
    } else if (absoluteSum[1] > absoluteSum[2] &&
               absoluteSum[1] > absoluteSum[0]) {
        axis = sum[1] < 0.0f ? 3 : 2;
    } else {
        axis = sum[2] < 0.0f ? 5 : 4;
    }

    for (vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
        const int32_t divisorSelector =
            rendererSkyProjectionMap[axis][2];
        const int32_t sSelector = rendererSkyProjectionMap[axis][0];
        const int32_t tSelector = rendererSkyProjectionMap[axis][1];
        float divisor;
        float s;
        long double tRaw;

        if (divisorSelector < 0) {
            divisor = -vertices[vertexIndex][-divisorSelector - 1];
        } else {
            divisor = vertices[vertexIndex][divisorSelector - 1];
        }
        /* Exact 0x005b9b78 double. The original source comparison promotes
         * the float divisor to double. */
        if ((double)divisor < 0.001)
            continue;

        if (sSelector < 0) {
            s = (float)((-1.0L / (long double)divisor) *
                        vertices[vertexIndex][-sSelector - 1]);
        } else {
            s = (float)((long double)vertices[vertexIndex][sSelector - 1] /
                        (long double)divisor);
        }
        if (tSelector < 0) {
            tRaw = (-1.0L / (long double)divisor) *
                   vertices[vertexIndex][-tSelector - 1];
        } else {
            tRaw = (long double)vertices[vertexIndex][tSelector - 1] /
                   (long double)divisor;
        }
        if (s < rendererSkyMins[0][axis])
            rendererSkyMins[0][axis] = s;
        if (tRaw < (long double)rendererSkyMins[1][axis])
            rendererSkyMins[1][axis] = (float)tRaw;
        if (s > rendererSkyMaxs[0][axis])
            rendererSkyMaxs[0][axis] = s;
        if (tRaw > (long double)rendererSkyMaxs[1][axis])
            rendererSkyMaxs[1][axis] = (float)tRaw;
    }
}

typedef enum renderer_sky_clip_side_e {
    R_SKY_CLIP_FRONT = 0,
    R_SKY_CLIP_BACK = 1,
    R_SKY_CLIP_ON_PLANE = 2
} renderer_sky_clip_side_t;

/* Source: CoDUOMP.exe 0x00513ed0..0x00514200.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00513ed0_00514201.mcode. Exact
 * same-module Mac symbol/signature ClipSkyPolygon. The input needs one spare
 * vector because the original body closes the polygon in place before
 * splitting it; every maintained caller provides that storage. */
void ClipSkyPolygon(int32_t vertexCount, vec3_t *vertices,
                    int32_t clipStage)
{
    renderer_sky_clip_side_t sides[R_SKY_MAX_CLIP_VERTICES];
    float distances[R_SKY_MAX_CLIP_VERTICES];
    vec3_t clipped[2][R_SKY_MAX_CLIP_VERTICES];
    int32_t clippedCount[2] = {0, 0};
    qboolean hasFront = qfalse;
    qboolean hasBack = qfalse;
    int32_t vertexIndex;

    if (vertexCount > R_SKY_MAX_INPUT_CLIP_VERTICES) {
        ri.Error(ERR_DROP, "\x15" "ClipSkyPolygon: MAX_CLIP_VERTS");
    }

    if (clipStage == R_SKY_CLIP_PLANE_COUNT) {
        AddSkyPolygon(vertexCount, vertices);
        return;
    }

    for (vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
        const float distance =
            vertices[vertexIndex][0] *
                rendererSkyClipNormals[clipStage][0] +
            vertices[vertexIndex][1] *
                rendererSkyClipNormals[clipStage][1] +
            vertices[vertexIndex][2] *
                rendererSkyClipNormals[clipStage][2];

        distances[vertexIndex] = distance;
        if (distance > rendererSkyClipEpsilon) {
            sides[vertexIndex] = R_SKY_CLIP_FRONT;
            hasFront = qtrue;
        } else if (distance < -rendererSkyClipEpsilon) {
            sides[vertexIndex] = R_SKY_CLIP_BACK;
            hasBack = qtrue;
        } else {
            sides[vertexIndex] = R_SKY_CLIP_ON_PLANE;
        }
    }

    if (hasFront == qfalse || hasBack == qfalse) {
        ClipSkyPolygon(vertexCount, vertices, clipStage + 1);
        return;
    }

    sides[vertexCount] = sides[0];
    distances[vertexCount] = distances[0];
    vertices[vertexCount][0] = vertices[0][0];
    vertices[vertexCount][1] = vertices[0][1];
    vertices[vertexCount][2] = vertices[0][2];

    for (vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
        int32_t outputSide;

        switch (sides[vertexIndex]) {
        case R_SKY_CLIP_FRONT:
            clipped[0][clippedCount[0]][0] = vertices[vertexIndex][0];
            clipped[0][clippedCount[0]][1] = vertices[vertexIndex][1];
            clipped[0][clippedCount[0]][2] = vertices[vertexIndex][2];
            ++clippedCount[0];
            break;

        case R_SKY_CLIP_BACK:
            clipped[1][clippedCount[1]][0] = vertices[vertexIndex][0];
            clipped[1][clippedCount[1]][1] = vertices[vertexIndex][1];
            clipped[1][clippedCount[1]][2] = vertices[vertexIndex][2];
            ++clippedCount[1];
            break;

        case R_SKY_CLIP_ON_PLANE:
            for (outputSide = 0; outputSide < 2; ++outputSide) {
                clipped[outputSide][clippedCount[outputSide]][0] =
                    vertices[vertexIndex][0];
                clipped[outputSide][clippedCount[outputSide]][1] =
                    vertices[vertexIndex][1];
                clipped[outputSide][clippedCount[outputSide]][2] =
                    vertices[vertexIndex][2];
                ++clippedCount[outputSide];
            }
            break;
        }

        if (sides[vertexIndex] == R_SKY_CLIP_ON_PLANE ||
            sides[vertexIndex + 1] == R_SKY_CLIP_ON_PLANE ||
            sides[vertexIndex] == sides[vertexIndex + 1]) {
            continue;
        }

        {
            const float fraction =
                distances[vertexIndex] /
                (distances[vertexIndex] - distances[vertexIndex + 1]);
            vec3_t intersection;
            int32_t component;

            for (component = 0;
                 component < R_SKY_TRIANGLE_VERTEX_COUNT;
                 ++component) {
                intersection[component] =
                    vertices[vertexIndex][component] +
                    fraction *
                        (vertices[vertexIndex + 1][component] -
                         vertices[vertexIndex][component]);
                clipped[0][clippedCount[0]][component] =
                    intersection[component];
                clipped[1][clippedCount[1]][component] =
                    intersection[component];
            }
            ++clippedCount[0];
            ++clippedCount[1];
        }
    }

    ClipSkyPolygon(clippedCount[0], clipped[0], clipStage + 1);
    ClipSkyPolygon(clippedCount[1], clipped[1], clipStage + 1);
}

/* Source: CoDUOMP.exe 0x00514210..0x00514300.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00514210_00514301.mcode. Exact
 * same-module Mac symbol ClearSkyBox. Windows stores prove both complete
 * 2x6 arrays and the exact +/-9999.0f sentinels. */
static void ClearSkyBox(void)
{
    int32_t side;

    for (side = 0; side < R_SKYBOX_FACE_COUNT; ++side) {
        rendererSkyMins[0][side] = 9999.0f;
        rendererSkyMins[1][side] = 9999.0f;
        rendererSkyMaxs[0][side] = -9999.0f;
        rendererSkyMaxs[1][side] = -9999.0f;
    }
}

/* Source: CoDUOMP.exe 0x00514310..0x0051440d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00514310_0051440e.mcode. The Mac
 * RB_ClipSkyPolygons signature independently proves the tessellation argument
 * that MSVC LTCG carries in ESI in the Windows caller. */
void RB_ClipSkyPolygons(shaderCommands_t *tessellation)
{
    vec3_t triangle[R_SKY_TRIANGLE_VERTEX_COUNT + 1];
    int32_t firstIndex;

    ClearSkyBox();

    for (firstIndex = 0;
         firstIndex < tessellation->indexCount;
         firstIndex += R_SKY_TRIANGLE_VERTEX_COUNT) {
        int32_t triangleVertex;

        for (triangleVertex = 0;
             triangleVertex < R_SKY_TRIANGLE_VERTEX_COUNT;
             ++triangleVertex) {
            const uint16_t vertexIndex =
                tessellation->indexes[firstIndex + triangleVertex];
            const int32_t componentOffset =
                (int32_t)vertexIndex * tessellation->vertexComponentCount;

            triangle[triangleVertex][0] =
                tessellation->xyz[componentOffset + 0] -
                backEnd.viewParms.orientation.origin[0];
            triangle[triangleVertex][1] =
                tessellation->xyz[componentOffset + 1] -
                backEnd.viewParms.orientation.origin[1];
            triangle[triangleVertex][2] =
                tessellation->xyz[componentOffset + 2] -
                backEnd.viewParms.orientation.origin[2];
        }

        ClipSkyPolygon(R_SKY_TRIANGLE_VERTEX_COUNT, triangle,
                       R_SKY_INITIAL_CLIP_STAGE);
    }
}

/* Source: CoDUOMP.exe 0x00514410..0x0051454d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00514410_0051454e.mcode. Exact
 * same-module Mac symbol and five-argument signature MakeSkyVec. Windows
 * instructions prove the signed axis table, homogeneous zero, and clamped
 * sky-image texcoord transformation. */
void MakeSkyVec(float s, float t, int32_t axis,
                vec2_t texCoord, vec4_t position)
{
    const float source[R_SKY_TRIANGLE_VERTEX_COUNT] = {s, t, 1.0f};
    float skyS;
    float skyT;
    int32_t component;

    for (component = 0;
         component < R_SKY_TRIANGLE_VERTEX_COUNT;
         ++component) {
        const int32_t sourceSelector =
            rendererSkyAxisMap[axis][component];

        if (sourceSelector < 0) {
            position[component] = -source[-sourceSelector - 1];
        } else {
            position[component] = source[sourceSelector - 1];
        }
    }
    position[3] = 0.0f;

    skyS = (s + 1.0f) * 0.5f;
    skyT = (t + 1.0f) * 0.5f;
    if (skyS < rendererSkyTexCoordMin)
        skyS = rendererSkyTexCoordMin;
    else if (skyS > rendererSkyTexCoordMax)
        skyS = rendererSkyTexCoordMax;
    if (skyT < rendererSkyTexCoordMin)
        skyT = rendererSkyTexCoordMin;
    else if (skyT > rendererSkyTexCoordMax)
        skyT = rendererSkyTexCoordMax;

    if (texCoord != NULL) {
        texCoord[0] = skyS;
        texCoord[1] = 1.0f - skyT;
    }
}

/* Source: CoDUOMP.exe 0x00514aa0..0x00514cee.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00514aa0_00514cef.mcode. Exact
 * same-module Mac symbol and four-argument signature FillCloudySkySide.
 * Windows proves the inclusive 9-point grid walks through zero-extended
 * 16-bit loop values and dimensions, cloud-coordinate lookup, vertex-limit
 * check, and both triangle windings. */
static void FillCloudySkySide(const int32_t mins[2],
                              const int32_t maxs[2], int32_t side,
                              qboolean addIndexes)
{
    const int32_t vertexStart = tess.vertexCount;
    const uint16_t sWidth = (uint16_t)(
        (uint16_t)maxs[0] - (uint16_t)mins[0] + 1u);
    const uint16_t tHeight = (uint16_t)(
        (uint16_t)maxs[1] - (uint16_t)mins[1] + 1u);
    const int32_t maxSBound = (int32_t)(
        (uint32_t)maxs[0] + R_SKY_GRID_CENTER);
    const int32_t maxTBound = (int32_t)(
        (uint32_t)maxs[1] + R_SKY_GRID_CENTER);
    uint16_t t;
    uint16_t s;

    for (t = (uint16_t)(
             (uint16_t)mins[1] + R_SKY_GRID_CENTER);
         (int32_t)(uint32_t)t <= maxTBound;
         t = (uint16_t)(t + 1u)) {
        for (s = (uint16_t)(
                 (uint16_t)mins[0] + R_SKY_GRID_CENTER);
             (int32_t)(uint32_t)s <= maxSBound;
             s = (uint16_t)(s + 1u)) {
            float *position =
                &tess.xyz[tess.vertexCount *
                          tess.vertexComponentCount];

            MakeSkyVec((float)((int32_t)(uint32_t)s -
                               R_SKY_GRID_CENTER) * 0.25f,
                       (float)((int32_t)(uint32_t)t -
                               R_SKY_GRID_CENTER) * 0.25f,
                       side, NULL, position);
            tess.texCoords[R_TESS_BASE_TEXCOORD_SET]
                          [tess.vertexCount][0] =
                rendererSkyCloudTexCoords[side][t][s][0];
            tess.texCoords[R_TESS_BASE_TEXCOORD_SET]
                          [tess.vertexCount][1] =
                rendererSkyCloudTexCoords[side][t][s][1];

            ++tess.vertexCount;
            if (tess.vertexCount >= R_MAX_TESS_VERTICES) {
                ri.Error(
                    ERR_DROP,
                    "\x15" "SHADER_MAX_VERTEXES hit in "
                    "FillCloudySkySide()\n");
            }
        }
    }

    if (addIndexes == qfalse)
        return;

    for (t = 0;
         (int32_t)(uint32_t)t < (int32_t)(uint32_t)tHeight - 1;
         t = (uint16_t)(t + 1u)) {
        for (s = 0;
             (int32_t)(uint32_t)s < (int32_t)(uint32_t)sWidth - 1;
             s = (uint16_t)(s + 1u)) {
            const uint32_t vertex =
                (uint32_t)vertexStart +
                (uint32_t)t * (uint32_t)sWidth +
                (uint32_t)s;

            tess.indexes[tess.indexCount++] = (uint16_t)vertex;
            tess.indexes[tess.indexCount++] =
                (uint16_t)(vertex + sWidth);
            tess.indexes[tess.indexCount++] = (uint16_t)(vertex + 1);
            tess.indexes[tess.indexCount++] =
                (uint16_t)(vertex + sWidth);
            tess.indexes[tess.indexCount++] =
                (uint16_t)(vertex + sWidth + 1);
            tess.indexes[tess.indexCount++] = (uint16_t)(vertex + 1);
        }
    }
}

/* Source: CoDUOMP.exe 0x00514cf0..0x00514f06.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00514cf0_00514f07.mcode. The exact
 * same-module Mac body preserves the otherwise optimized-away shader argument
 * in the source signature. The function deliberately omits cube side 5. */
static void FillCloudBox(shader_t *shader, int32_t stageIndex)
{
    int32_t side;

    (void)shader;
    for (side = 0; side < R_SKYBOX_FACE_COUNT; ++side) {
        int32_t mins[2];
        int32_t maxs[2];
        int32_t coordinate;

        if (side == R_SKYBOX_FACE_COUNT - 1)
            continue;

        rendererSkyMins[0][side] =
            (float)((long double)floor((double)(
                        (long double)rendererSkyMins[0][side] * 4.0L)) *
                    0.25L);
        rendererSkyMins[1][side] =
            (float)((long double)floor((double)(
                        (long double)rendererSkyMins[1][side] * 4.0L)) *
                    0.25L);
        rendererSkyMaxs[0][side] =
            (float)((long double)ceil((double)(
                        (long double)rendererSkyMaxs[0][side] * 4.0L)) *
                    0.25L);
        const long double maxTRaw =
            (long double)ceil((double)(
                (long double)rendererSkyMaxs[1][side] * 4.0L)) * 0.25L;
        rendererSkyMaxs[1][side] = (float)maxTRaw;

        if (rendererSkyMins[0][side] >= rendererSkyMaxs[0][side] ||
            (long double)rendererSkyMins[1][side] >= maxTRaw) {
            continue;
        }

        mins[0] = (int32_t)(rendererSkyMins[0][side] * 4.0f);
        mins[1] = (int32_t)(rendererSkyMins[1][side] * 4.0f);
        maxs[0] = (int32_t)(rendererSkyMaxs[0][side] * 4.0f);
        maxs[1] = (int32_t)(rendererSkyMaxs[1][side] * 4.0f);
        for (coordinate = 0; coordinate < 2; ++coordinate) {
            if (mins[coordinate] < -R_SKY_GRID_CENTER)
                mins[coordinate] = -R_SKY_GRID_CENTER;
            else if (mins[coordinate] > R_SKY_GRID_CENTER)
                mins[coordinate] = R_SKY_GRID_CENTER;
            if (maxs[coordinate] < -R_SKY_GRID_CENTER)
                maxs[coordinate] = -R_SKY_GRID_CENTER;
            else if (maxs[coordinate] > R_SKY_GRID_CENTER)
                maxs[coordinate] = R_SKY_GRID_CENTER;
        }

        FillCloudySkySide(mins, maxs, side,
                          stageIndex == 0 ? qtrue : qfalse);
    }
}

/* Source: CoDUOMP.exe 0x00514f80..0x00515198.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00514f80_00515199.mcode. Exact
 * same-module Mac symbol R_BuildSkyBox. Windows proves backend priority
 * ARB-VBO, NV vertex-array range, ATI object buffer, then client hunk memory;
 * it also proves the ATI pool-selection rewrite to storage mode 3. */
static void R_BuildSkyBox(const renderer_sky_vertex_t skyVertices
                             [R_SKY_VERTEX_COUNT])
{
    renderer_sky_vertex_storage_t *storage = rendererSkyBox;
    const size_t skyVertexBytes = sizeof(renderer_sky_vertex_t) *
                                  R_SKY_VERTEX_COUNT;

    if (glConfig.vertexBufferObjectAvailable != qfalse) {
        storage->memorySource = tr.defaultStorageMode;
        storage->backend = R_SKY_VERTEX_BACKEND_ARB_BUFFER;
        storage->base.bufferObject = R_CreateBufferARB(
            GL_ARRAY_BUFFER_ARB, skyVertexBytes, skyVertices,
            GL_STATIC_DRAW_ARB);
        if (storage->base.bufferObject != 0)
            return;
    } else if (glConfig.vertexArrayRangeMode !=
               R_VERTEX_ARRAY_RANGE_NONE) {
        uint8_t *memory;

        storage->memorySource = R_AllocMemoryNV(
            R_STATIC_VERTEX_MEMORY_PRIMARY, skyVertexBytes, &memory);
        storage->backend = R_SKY_VERTEX_BACKEND_NV_MEMORY;
        storage->base.vertices = (renderer_sky_vertex_t *)memory;
        memcpy(storage->base.vertices, skyVertices, skyVertexBytes);
        return;
    } else if (glConfig.vertexArrayObjectATIAvailable != qfalse) {
        size_t objectOffset = 0;
        const renderer_static_vertex_memory_source_t memorySource =
            R_AllocMemoryATI(R_STATIC_VERTEX_MEMORY_PRIMARY,
                             skyVertexBytes, &objectOffset);

        storage->memorySource = memorySource;
        storage->backend = R_SKY_VERTEX_BACKEND_ATI_OBJECT;
        if (memorySource == R_STATIC_VERTEX_MEMORY_PRIMARY ||
            memorySource == R_STATIC_VERTEX_MEMORY_SECONDARY) {
            const renderer_static_vertex_memory_base_t bufferBase =
                memorySource == R_STATIC_VERTEX_MEMORY_PRIMARY
                    ? tr.staticVertexMemoryPrimary
                    : tr.staticVertexMemorySecondary;

            storage->memorySource = R_STATIC_VERTEX_MEMORY_HUNK;
            storage->base.bufferObject = bufferBase.atiObjectBuffer;
            storage->objectOffset = (uint32_t)objectOffset;
            qglUpdateObjectBufferATI(
                storage->base.bufferObject, storage->objectOffset,
                (int32_t)skyVertexBytes, skyVertices, GL_PRESERVE_ATI);
            return;
        }
    }

    storage->memorySource = R_STATIC_VERTEX_MEMORY_HUNK;
    storage->backend = R_SKY_VERTEX_BACKEND_CLIENT;
    storage->base.vertices = ri.Hunk_Alloc(skyVertexBytes);
    memcpy(storage->base.vertices, skyVertices, skyVertexBytes);
}

/* Source: CoDUOMP.exe 0x005151b0..0x0051544a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005151b0_0051544b.mcode. Exact
 * same-module Mac symbol and float argument R_InitSkyTexCoords. Windows proves
 * the 6x9x9 traversal, ray/cloud-sphere quadratic, angular lookup generation,
 * and final R_BuildSkyBox upload. */
void R_InitSkyTexCoords(float cloudHeight)
{
    renderer_sky_vertex_t skyVertices[R_SKY_VERTEX_COUNT];
    const float cloudHeightSquared = cloudHeight * cloudHeight;
    const float cloudHeightSquaredPlusRadiusSquared =
        cloudHeightSquared + r_skyCloudWorldRadiusSquared;
    int32_t side;

    rendererSkyTexCoordMax = 1.0f;
    rendererSkyTexCoordMin = 0.0f;
    /* The original store targets backEnd.viewParms.zFar at 0x0489a4a0.
     * RB_DrawSurfs replaces it with the active view value before rendering. */
    backEnd.viewParms.zFar = (float)R_SKY_SUBDIVISION_COUNT;

    for (side = 0; side < R_SKYBOX_FACE_COUNT; ++side) {
        int32_t t;

        for (t = 0; t < R_SKY_GRID_SIDE; ++t) {
            int32_t s;

            for (s = 0; s < R_SKY_GRID_SIDE; ++s) {
                const int32_t vertexIndex =
                    side * R_SKY_GRID_SIDE * R_SKY_GRID_SIDE +
                    t * R_SKY_GRID_SIDE + s;
                renderer_sky_vertex_t *vertex =
                    &skyVertices[vertexIndex];
                const float skyS =
                    (float)(s - R_SKY_GRID_CENTER) * 0.25f;
                const float skyT =
                    (float)(t - R_SKY_GRID_CENTER) * 0.25f;
                float xSquared;
                float ySquared;
                float zSquared;
                float lengthSquared;
                float radicand;
                float root;
                float numerator;
                float inverseDenominator;
                float rayScale;
                vec3_t cloudDirection;
                float angle;

                MakeSkyVec(skyS, skyT, side, vertex->texCoord,
                           vertex->position);
                xSquared = vertex->position[0] * vertex->position[0];
                ySquared = vertex->position[1] * vertex->position[1];
                zSquared = vertex->position[2] * vertex->position[2];
                lengthSquared = (xSquared + ySquared) + zSquared;
                radicand =
                    (xSquared + ySquared) * cloudHeightSquared +
                    lengthSquared * cloudHeight *
                        r_skyCloudWorldDiameter +
                    cloudHeightSquaredPlusRadiusSquared * zSquared;
                root = (float)sqrt((double)radicand);
                numerator = (root + root) -
                            vertex->position[2] *
                                r_skyCloudWorldDiameter;
                inverseDenominator =
                    1.0f / (lengthSquared + lengthSquared);
                rayScale = numerator * inverseDenominator;
                rendererSkyCloudTexP[side][t][s] = rayScale;

                cloudDirection[0] = vertex->position[0] * rayScale;
                cloudDirection[1] = vertex->position[1] * rayScale;
                cloudDirection[2] =
                    vertex->position[2] * rayScale +
                    r_skyCloudWorldRadius;
                (void)VectorNormalize(cloudDirection);

                angle = (float)acos((double)cloudDirection[0]);
                if (angle > r_pi)
                    angle = r_pi;
                else if (angle < r_negativePi)
                    angle = r_negativePi;
                rendererSkyCloudTexCoords[side][t][s][0] = angle;

                angle = (float)acos((double)cloudDirection[1]);
                if (angle > r_pi)
                    angle = r_pi;
                else if (angle < r_negativePi)
                    angle = r_negativePi;
                rendererSkyCloudTexCoords[side][t][s][1] = angle;
            }
        }
    }

    R_BuildSkyBox(skyVertices);
}

/* Source: CoDUOMP.exe 0x00515450..0x00515494.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00515450_00515495.mcode. Exact
 * same-module Mac symbol and string argument R_FindSunSpriteShader. The
 * Windows return register proves the shader result consumed by its caller;
 * each live stage has depth writes disabled. */
shader_t *R_FindSunSpriteShader(const char *name)
{
    shader_t *shader = R_FindShader(
        name, R_SUN_SPRITE_SHADER_LIGHTMAP_MODE,
        qfalse,
        R_SUN_SPRITE_SHADER_USAGE);
    int32_t stageIndex;

    for (stageIndex = 0; stageIndex < R_MAX_SHADER_STAGES; ++stageIndex) {
        shaderStage_t *stage = shader->stages[stageIndex];

        if (stage == NULL)
            break;
        stage->stateBits &= ~GLS_DEPTHMASK_TRUE;
    }
    return shader;
}

/* Source: CoDUOMP.exe 0x005154a0..0x005156f5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005154a0_005156f6.mcode. Exact
 * same-module Mac symbol and float argument R_SetSunSpriteSize. Windows proves
 * both cross-product orders, the 0.99 reference-axis threshold, all four
 * corner signs, homogeneous zeros, and the retained configured size. */
void R_SetSunSpriteSize(float spriteSize)
{
    vec3_t reference;
    vec3_t right;
    vec3_t up;
    vec3_t rightPlusUp;
    vec3_t rightMinusUp;
    const float scale = spriteSize * r_sunSpriteAngularScale;
    int32_t component;

    if (tr.sunDirection[2] * tr.sunDirection[2] <=
        r_sunSpriteReferenceThreshold) {
        reference[0] = tr.sunDirection[1];
        reference[1] = -tr.sunDirection[0];
        reference[2] = 0.0f;
    } else {
        reference[0] = 1.0f;
        reference[1] = 0.0f;
        reference[2] = 0.0f;
    }

    right[0] = tr.sunDirection[1] * reference[2] -
               tr.sunDirection[2] * reference[1];
    right[1] = tr.sunDirection[2] * reference[0] -
               tr.sunDirection[0] * reference[2];
    right[2] = tr.sunDirection[0] * reference[1] -
               tr.sunDirection[1] * reference[0];
    (void)VectorNormalize(right);
    right[0] *= scale;
    right[1] *= scale;
    right[2] *= scale;

    up[0] = right[1] * tr.sunDirection[2] -
            right[2] * tr.sunDirection[1];
    up[1] = right[2] * tr.sunDirection[0] -
            right[0] * tr.sunDirection[2];
    up[2] = right[0] * tr.sunDirection[1] -
            right[1] * tr.sunDirection[0];

    for (component = 0; component < 3; ++component) {
        rightPlusUp[component] = right[component] + up[component];
        rightMinusUp[component] = right[component] - up[component];
        rendererSunState.spriteVertices[0][component] =
            tr.sunDirection[component] + rightPlusUp[component];
        rendererSunState.spriteVertices[1][component] =
            tr.sunDirection[component] + rightMinusUp[component];
        rendererSunState.spriteVertices[2][component] =
            tr.sunDirection[component] - rightPlusUp[component];
        rendererSunState.spriteVertices[3][component] =
            tr.sunDirection[component] - rightMinusUp[component];
    }
    rendererSunState.spriteVertices[0][3] = 0.0f;
    rendererSunState.spriteVertices[1][3] = 0.0f;
    rendererSunState.spriteVertices[2][3] = 0.0f;
    rendererSunState.spriteVertices[3][3] = 0.0f;
    rendererSunState.spriteSize = spriteSize;
}

/* Source: CoDUOMP.exe 0x00515700..0x0051598e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00515700_0051598f.mcode. Exact
 * same-module Mac symbol R_SetSunFromCvars. Windows proves every cvar source,
 * destination field, degree-to-radian multiply order, cosine, half-size, and
 * millisecond conversion. */
void R_SetSunFromCvars(void)
{
    float milliseconds;

    rendererSunState.spriteShader = NULL;
    if (r_sunsprite_shader->string[0] != '\0') {
        rendererSunState.spriteShader =
            R_FindSunSpriteShader(r_sunsprite_shader->string);
    }
    R_SetSunSpriteSize(r_sunsprite_size->value);

    rendererSunState.flareShader = NULL;
    if (r_sunflare_shader->string[0] != '\0') {
        rendererSunState.flareShader = R_FindShader(
            r_sunflare_shader->string,
            R_SUN_SPRITE_SHADER_LIGHTMAP_MODE,
            qfalse,
            R_SUN_SPRITE_SHADER_USAGE);
    }
    rendererSunState.flareMinHalfSize =
        r_sunflare_min_size->value * 0.5f;
    rendererSunState.flareMinCosAngle = coduo_x87_fcos_to_f32(
        (long double)r_sunflare_min_angle->value * (long double)r_pi *
        (long double)r_inverseDegreesPerHalfTurn);
    rendererSunState.flareMaxHalfSize =
        r_sunflare_max_size->value * 0.5f;
    rendererSunState.flareMaxCosAngle = coduo_x87_fcos_to_f32(
        (long double)r_sunflare_max_angle->value * (long double)r_pi *
        (long double)r_inverseDegreesPerHalfTurn);
    rendererSunState.flareMaxAlpha = r_sunflare_max_alpha->value;

    milliseconds = r_sunflare_fadein->value * 1000.0f;
    rendererSunState.flareFadeInMsec = (int32_t)lrint(
        (double)milliseconds + r_integerRoundingBias);
    milliseconds = r_sunflare_fadeout->value * 1000.0f;
    rendererSunState.flareFadeOutMsec = (int32_t)lrint(
        (double)milliseconds + r_integerRoundingBias);

    rendererSunState.blindMinCosAngle = coduo_x87_fcos_to_f32(
        (long double)r_sunblind_min_angle->value * (long double)r_pi *
        (long double)r_inverseDegreesPerHalfTurn);
    rendererSunState.blindMaxCosAngle = coduo_x87_fcos_to_f32(
        (long double)r_sunblind_max_angle->value * (long double)r_pi *
        (long double)r_inverseDegreesPerHalfTurn);
    rendererSunState.blindMaxDarken = r_sunblind_max_darken->value;
    milliseconds = r_sunblind_fadein->value * 1000.0f;
    rendererSunState.blindFadeInMsec = (int32_t)lrint(
        (double)milliseconds + r_integerRoundingBias);
    milliseconds = r_sunblind_fadeout->value * 1000.0f;
    rendererSunState.blindFadeOutMsec = (int32_t)lrint(
        (double)milliseconds + r_integerRoundingBias);

    rendererSunState.glareMinCosAngle = coduo_x87_fcos_to_f32(
        (long double)r_sunglare_min_angle->value * (long double)r_pi *
        (long double)r_inverseDegreesPerHalfTurn);
    rendererSunState.glareMaxCosAngle = coduo_x87_fcos_to_f32(
        (long double)r_sunglare_max_angle->value * (long double)r_pi *
        (long double)r_inverseDegreesPerHalfTurn);
    rendererSunState.glareMaxLighten = r_sunglare_max_lighten->value;
    milliseconds = r_sunglare_fadein->value * 1000.0f;
    rendererSunState.glareFadeInMsec = (int32_t)lrint(
        (double)milliseconds + r_integerRoundingBias);
    milliseconds = r_sunglare_fadeout->value * 1000.0f;
    rendererSunState.glareFadeOutMsec = (int32_t)lrint(
        (double)milliseconds + r_integerRoundingBias);
}

/* Source: CoDUOMP.exe 0x00515990..0x00515a10.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00515990_00515a11.mcode. The Windows
 * instructions prove the 64 KiB stack buffer, 20-name table, save-helper
 * result gate, exact path format, byte count, and FS_WriteFile call. The
 * same-module Mac symbol provides R_SaveSunFromCvars. */
void R_SaveSunFromCvars(const char *sunName)
{
    char buffer[R_SUN_FILE_BUFFER_SIZE];

    if (ri.Com_SaveCvarsToBuffer(rendererSunCvarNames, R_SUN_CVAR_COUNT,
                                 buffer, sizeof(buffer)) != qfalse) {
        ri.FS_WriteFile(va("scripts/%s.sun", sunName), buffer,
                        (int32_t)strlen(buffer));
    }
}

/* Source: CoDUOMP.exe 0x00515a20..0x00515a84.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00515a20_00515a85.mcode. Windows proves
 * the path, read/failure branches, 20-name load-helper call, configuration
 * refresh gate, and unconditional free after a successful read. The
 * same-module Mac symbol provides R_LoadSunThroughCvars. */
void R_LoadSunThroughCvars(const char *sunName)
{
    const char *fileName = va("scripts/%s.sun", sunName);
    char *fileBuffer = NULL;

    if (ri.FS_ReadFile(fileName, (void **)&fileBuffer) < 0) {
        ri.Printf(R_PRINT_ALL,
                  "^3WARNING: couldn't load sun file '%s'\n", fileName);
        return;
    }

    if (ri.Com_LoadCvarsFromBuffer(rendererSunCvarNames,
                                   R_SUN_CVAR_COUNT,
                                   fileBuffer, fileName) != qfalse) {
        R_SetSunFromCvars();
    }
    ri.FS_FreeFile(fileBuffer);
}

/* Source: CoDUOMP.exe 0x00515a90..0x00515abd.
 * Evidence: the original executable gap at this range and direct objdump
 * disassembly. Ghidra omitted the function record even though R_Register
 * installs this address for r_savesun. The same-module Mac symbol confirms
 * R_SaveSun_f. */
void R_SaveSun_f(void)
{
    if (ri.Cmd_Argc() != 2) {
        ri.Printf(R_PRINT_ALL,
                  "USAGE: r_savesun <sunname>\n"
                  "  sunname must not have an extension\n");
        return;
    }

    R_SaveSunFromCvars(ri.Cmd_Argv(1));
}

/* Source: CoDUOMP.exe 0x00515ac0..0x00515b08.
 * Evidence: the original executable gap at this range and direct objdump
 * disassembly. The machine code proves the argc and r_cheats gates and the
 * tail call to R_LoadSunThroughCvars. R_Register and the same-module Mac
 * symbol confirm R_LoadSun_f. */
void R_LoadSun_f(void)
{
    if (ri.Cmd_Argc() != 2) {
        ri.Printf(R_PRINT_ALL,
                  "USAGE: r_loadsun <sunname>\n"
                  "  sunname must not have an extension\n");
        return;
    }
    if (r_cheats->integer == 0) {
        ri.Printf(R_PRINT_ALL,
                  "You must have cheats enabled to use r_loadsun\n");
        return;
    }

    R_LoadSunThroughCvars(ri.Cmd_Argv(1));
}

/* Source: CoDUOMP.exe 0x00515b10..0x00515bae.
 * Evidence: the original executable gap at this range and direct objdump
 * disassembly. Windows proves all five headings, the 20-entry iteration,
 * eight-character case-insensitive group comparison, conditional blank line,
 * and name/description print. The same-module Mac symbol confirms
 * R_SunHelp_f. */
void R_SunHelp_f(void)
{
    int32_t cvarIndex;

    ri.Printf(R_PRINT_ALL, "\n=== SUN COMMANDS ===\n");
    ri.Printf(R_PRINT_ALL,
              "r_loadsun <sunname> -- loads sun from "
              "'scripts/<sunname>.sun'\n");
    ri.Printf(R_PRINT_ALL,
              "r_savesun <sunname> -- saves sun as "
              "'scripts/<sunname>.sun'\n");
    ri.Printf(R_PRINT_ALL, "\n=== SUN CVARS ===\n");
    ri.Printf(R_PRINT_ALL,
              "(must have r_suntest set to 1 to tweak these values)\n");

    for (cvarIndex = 0; cvarIndex < R_SUN_CVAR_COUNT; ++cvarIndex) {
        if (cvarIndex > 0 &&
            coduo_crt_strnicmp(rendererSunCvarNames[cvarIndex - 1],
                                 rendererSunCvarNames[cvarIndex],
                                 R_SUN_CVAR_GROUP_PREFIX_LENGTH) != 0) {
            ri.Printf(R_PRINT_ALL, "\n");
        }
        ri.Printf(R_PRINT_ALL, "^2%-22s^7 %s\n",
                  rendererSunCvarNames[cvarIndex],
                  rendererSunCvarDescriptions[cvarIndex]);
    }
}

/* Source: CoDUOMP.exe 0x004ef370..0x004ef423.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004ef370_004ef424.mcode. Name and the
 * call to RB_AddFlare are corroborated by the same-module Mac symbols. */
void RB_UpdateSunFlare(shader_t *shader, float alpha, float size,
                       float spriteSize, int32_t fadeInMsec,
                       int32_t fadeOutMsec)
{
    renderer_flare_source_t source;
    float scaledRadius;

    source.id = R_SUN_FLARE_ID;
    source.shader = shader;
    source.origin[0] = tr.world->sunLight->position[0];
    source.origin[1] = tr.world->sunLight->position[1];
    source.origin[2] = tr.world->sunLight->position[2];
    source.depthOffset = 0.0f;
    source.color[0] = alpha;
    source.color[1] = alpha;
    source.color[2] = alpha;
    source.color[3] = 1.0f;
    source.size = size;

    /* The Windows body rounds this float once, adds the exact double 2^-30,
     * then uses the current x87 integer-conversion mode. This is the original
     * viewport-width scaling, not pointer or ABI arithmetic. */
    scaledRadius = (float)(
        (long double)backEnd.viewParms.viewportWidth *
        (long double)spriteSize *
        (long double)r_sunFlareViewportScale);
    source.screenRadius = FastRound(scaledRadius);
    source.fadeInMsec = fadeInMsec;
    source.fadeOutMsec = fadeOutMsec;
    source.active = qtrue;

    RB_AddFlare(&source, NULL);
}

/* Source: CoDUOMP.exe 0x00515cd0..0x00515d5f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00515cd0_00515d60.mcode. */
void RB_CalcSunFlare(float sunViewDot)
{
    long double fraction;

    if (rendererSunState.flareShader == NULL ||
        sunViewDot <= rendererSunState.flareMinCosAngle) {
        return;
    }

    if (sunViewDot >= rendererSunState.flareMaxCosAngle) {
        fraction = 1.0L;
    } else {
        fraction =
            ((long double)sunViewDot -
             (long double)rendererSunState.flareMinCosAngle) /
            ((long double)rendererSunState.flareMaxCosAngle -
             (long double)rendererSunState.flareMinCosAngle);
    }

    RB_UpdateSunFlare(
        rendererSunState.flareShader,
        (float)((long double)rendererSunState.flareMaxAlpha * fraction),
        (float)((long double)rendererSunState.flareMinHalfSize +
                (long double)rendererSunState.flareMaxHalfSize * fraction),
        rendererSunState.spriteSize,
        rendererSunState.flareFadeInMsec,
        rendererSunState.flareFadeOutMsec);
}

/* Source: CoDUOMP.exe 0x00515d60..0x00515ed6.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00515d60_00515ed7.mcode. Windows proves
 * the view/sun dot product, initial ten-millisecond step, persistent update
 * time, both angular ramps, visibility scaling, fade directions, persistent
 * fractions, and output scaling. The same-module Mac symbol and ABI prove the
 * float visibility followed by blind/glare output pointers. */
void RB_CalcSunBlind(float sunVisibility, float *blindFraction,
                     float *glareFraction)
{
    const int32_t currentTime = backEnd.refdef.time;
    int32_t elapsedTime;
    float sunViewDot;

    if (rendererSunState.lastUpdateTime == 0) {
        elapsedTime = R_SUN_INITIAL_UPDATE_MSEC;
    } else {
        elapsedTime = (int32_t)(
            (uint32_t)currentTime -
            (uint32_t)rendererSunState.lastUpdateTime);
    }
    rendererSunState.lastUpdateTime = currentTime;

    sunViewDot = (float)(
        (long double)backEnd.viewParms.orientation.axis[0][0] *
            tr.sunDirection[0] +
        ((long double)backEnd.viewParms.orientation.axis[0][1] *
             tr.sunDirection[1] +
         (long double)backEnd.viewParms.orientation.axis[0][2] *
             tr.sunDirection[2]));

    if (rendererSunState.blindMaxDarken <= 0.0f) {
        *blindFraction = 0.0f;
    } else {
        long double targetFractionRaw;

        if (sunViewDot <= rendererSunState.blindMinCosAngle) {
            targetFractionRaw = 0.0L;
        } else if (sunViewDot >= rendererSunState.blindMaxCosAngle) {
            targetFractionRaw = 1.0L;
        } else {
            targetFractionRaw =
                ((long double)sunViewDot -
                 rendererSunState.blindMinCosAngle) /
                ((long double)rendererSunState.blindMaxCosAngle -
                 rendererSunState.blindMinCosAngle);
        }
        const float targetFraction =
            (float)(targetFractionRaw * (long double)sunVisibility);
        const long double updatedFractionRaw = R_UpdateOverTime(
            rendererSunState.currentBlindFraction, targetFraction,
            rendererSunState.blindFadeInMsec,
            rendererSunState.blindFadeOutMsec, elapsedTime);
        rendererSunState.currentBlindFraction = (float)updatedFractionRaw;
        *blindFraction = (float)(updatedFractionRaw *
                                 rendererSunState.blindMaxDarken);
    }

    if (rendererSunState.glareMaxLighten <= 0.0f) {
        *glareFraction = 0.0f;
    } else {
        long double targetFractionRaw;

        if (sunViewDot <= rendererSunState.glareMinCosAngle) {
            targetFractionRaw = 0.0L;
        } else if (sunViewDot >= rendererSunState.glareMaxCosAngle) {
            targetFractionRaw = 1.0L;
        } else {
            targetFractionRaw =
                ((long double)sunViewDot -
                 rendererSunState.glareMinCosAngle) /
                ((long double)rendererSunState.glareMaxCosAngle -
                 rendererSunState.glareMinCosAngle);
        }
        const float targetFraction =
            (float)(targetFractionRaw * (long double)sunVisibility);
        const long double updatedFractionRaw = R_UpdateOverTime(
            rendererSunState.currentGlareFraction, targetFraction,
            rendererSunState.glareFadeInMsec,
            rendererSunState.glareFadeOutMsec, elapsedTime);
        rendererSunState.currentGlareFraction = (float)updatedFractionRaw;
        *glareFraction = (float)(updatedFractionRaw *
                                 rendererSunState.glareMaxLighten);
    }
}

/* Source: CoDUOMP.exe 0x00515f80..0x00515f9e.
 * Evidence: direct objdump of a function omitted from Ghidra's function table.
 * The same-module Mac R_ClearSun body and its RE_ClearFlares caller prove that
 * only the two persistent fade fractions and their update time are reset. */
void R_ClearSun(void)
{
    rendererSunState.currentBlindFraction = 0.0f;
    rendererSunState.currentGlareFraction = 0.0f;
    rendererSunState.lastUpdateTime = 0;
}

/* Source: CoDUOMP.exe 0x00515fa0..0x00515fb4.
 * Evidence: direct objdump of a function omitted from Ghidra's function table.
 * Windows proves a zero fill of exactly the complete 156-byte i386 sun state;
 * the same-module Mac R_FlushSun body and RE_LoadWorldMap caller corroborate
 * the full-state ownership. */
void R_FlushSun(void)
{
    memset(&rendererSunState, 0, sizeof(rendererSunState));
}

/* Source: CoDUOMP.exe 0x00515bb0..0x00515cc1.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00515bb0_00515cc2.mcode. The Windows
 * optimizer inlines RB_SelectStorage; the Mac call graph preserves the source
 * helper boundary. */
void RB_DrawSunSprite(void)
{
    if (rendererSunState.spriteShader == NULL)
        return;

    RB_SelectStorage(tr.defaultStorageMode);
    RB_BeginSurface(rendererSunState.spriteShader,
                    R_SUN_SURFACE_VERTEX_COMPONENTS);
    tess.stageIterator = tr.stageIteratorFunc;

    memcpy(tess.xyz, rendererSunState.spriteVertices,
           sizeof(rendererSunState.spriteVertices));

    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][0][0] = 1.0f;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][0][1] = 1.0f;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][1][0] = 0.0f;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][1][1] = 1.0f;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][2][0] = 0.0f;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][2][1] = 0.0f;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][3][0] = 1.0f;
    tess.texCoords[R_TESS_BASE_TEXCOORD_SET][3][1] = 0.0f;

    tess.vertexCount = 4;
    tess.indexes[0] = 0;
    tess.indexes[1] = 1;
    tess.indexes[2] = 2;
    tess.indexes[3] = 0;
    tess.indexes[4] = 2;
    tess.indexes[5] = 3;
    tess.indexCount = 6;

    RB_EndSurface();
}

/* Source: CoDUOMP.exe 0x00515ee0..0x00515f4c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00515ee0_00515f4d.mcode. */
void RB_AddSunEffects(void)
{
    long double sunViewDot;
    float storedSunViewDot;

    if (tr.world->sunLight == NULL)
        return;

    if (r_suntest->integer != 0)
        R_SetSunFromCvars();

    sunViewDot =
        (long double)backEnd.viewParms.orientation.axis[0][2] *
            tr.sunDirection[2] +
        (long double)backEnd.viewParms.orientation.axis[0][1] *
            tr.sunDirection[1] +
        (long double)backEnd.viewParms.orientation.axis[0][0] *
            tr.sunDirection[0];
    storedSunViewDot = (float)sunViewDot;
    if (sunViewDot <= 0.0f)
        return;

    RB_DrawSunSprite();
    RB_CalcSunFlare(storedSunViewDot);
}

/* Source: CoDUOMP.exe 0x00515f50..0x00515f75. This source function was
 * absent from Ghidra's function table; the executable-gap bytes, aligned INT3
 * boundaries, callers, and same-module Mac RB_DrawSun symbol prove it. */
void RB_DrawSun(void)
{
    shader_t *shader;

    if (r_drawSun->integer == 0)
        return;

    shader = tess.shader;
    RB_AddSunEffects();
    RB_BeginSurface(shader, R_SKY_SURFACE_VERTEX_COMPONENTS);
}

/* Source: CoDUOMP.exe 0x00514f10..0x00514f7c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00514f10_00514f7d.mcode. The exact
 * same-module Mac RB_BuildCloudData body preserves both this boundary and the
 * two-argument FillCloudBox source call that Windows register allocation
 * obscures. */
void RB_BuildCloudData(shaderCommands_t *tessellation)
{
    shader_t *shader;
    int32_t stageIndex;

    tessellation->indexCount = 0;
    tessellation->vertexCount = 0;
    tessellation->vertexComponentCount = R_SUN_SURFACE_VERTEX_COMPONENTS;
    rendererSkyTexCoordMin = r_skyTexCoordMin;
    rendererSkyTexCoordMax = r_skyTexCoordMax;

    shader = tessellation->shader;
    if (shader->skyCloudHeight == 0.0f)
        return;

    for (stageIndex = 0; stageIndex < R_MAX_SHADER_STAGES; ++stageIndex) {
        if (tessellation->activeStages[stageIndex] == NULL)
            break;
        FillCloudBox(shader, stageIndex);
    }
}

/* Source: CoDUOMP.exe 0x00514550..0x00514a83.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00514550_00514a84.mcode. Name,
 * source-level helper boundaries, and the four storage cases are corroborated
 * by the exact same-module Mac DrawSkyBox symbol. Windows machine code proves
 * the bound rounding, clamp limits, quad winding, face remap, GL ordering, and
 * counter updates below. */
void DrawSkyBox(shader_t *shader, int32_t boxSet)
{
    renderer_sky_vertex_storage_t *storage = rendererSkyBox;
    uint16_t indexes[R_SKY_MAX_INDEX_COUNT];
    qboolean arraysLocked = qfalse;
    int32_t side;

    RB_EndMultitexture();
    RB_SelectStorage(storage->memorySource);
    GL_State(GLS_DEPTHMASK_TRUE);
    GL_Cull(CT_FRONT_SIDED);
    GL_ClientState(GLS_CLIENT_TEXCOORD0_ARRAY |
                   GLS_CLIENT_VERTEX_ARRAY);
    qglColor3f(tr.identityLight, tr.identityLight, tr.identityLight);

    if (boxSet != R_SKY_BOX_OUTER) {
        qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        qglEnable(GL_BLEND);
        GL_TexEnv(GL_MODULATE);
    }

    switch (storage->backend) {
    case R_SKY_VERTEX_BACKEND_CLIENT:
        qglTexCoordPointer(2, GL_FLOAT,
                          (int32_t)sizeof(renderer_sky_vertex_t),
                          &storage->base.vertices[0].texCoord);
        qglVertexPointer(4, GL_FLOAT,
                         (int32_t)sizeof(renderer_sky_vertex_t),
                         &storage->base.vertices[0].position);
        if (qglLockArraysEXT != NULL) {
            qglLockArraysEXT(0, R_SKY_VERTEX_COUNT);
            arraysLocked = qtrue;
        }
        break;

    case R_SKY_VERTEX_BACKEND_ARB_BUFFER:
        qglBindBufferARB(GL_ARRAY_BUFFER_ARB, storage->base.bufferObject);
        qglTexCoordPointer(
            2, GL_FLOAT, (int32_t)sizeof(renderer_sky_vertex_t), NULL);
        qglVertexPointer(
            4, GL_FLOAT, (int32_t)sizeof(renderer_sky_vertex_t),
            (const void *)(uintptr_t)offsetof(renderer_sky_vertex_t,
                                              position));
        break;

    case R_SKY_VERTEX_BACKEND_ATI_OBJECT:
        qglArrayObjectATI(
            GL_TEXTURE_COORD_ARRAY, 2, GL_FLOAT,
            (int32_t)sizeof(renderer_sky_vertex_t),
            storage->base.bufferObject, storage->objectOffset);
        qglArrayObjectATI(
            GL_VERTEX_ARRAY, 4, GL_FLOAT,
            (int32_t)sizeof(renderer_sky_vertex_t),
            storage->base.bufferObject,
            storage->objectOffset +
                (uint32_t)offsetof(renderer_sky_vertex_t, position));
        break;

    case R_SKY_VERTEX_BACKEND_NV_MEMORY:
        qglTexCoordPointer(2, GL_FLOAT,
                          (int32_t)sizeof(renderer_sky_vertex_t),
                          &storage->base.vertices[0].texCoord);
        qglVertexPointer(4, GL_FLOAT,
                         (int32_t)sizeof(renderer_sky_vertex_t),
                         &storage->base.vertices[0].position);
        break;

    default:
        return;
    }

    for (side = 0; side < R_SKYBOX_FACE_COUNT; ++side) {
        int32_t minS;
        int32_t minT;
        int32_t maxS;
        int32_t maxT;
        int32_t indexCount = 0;
        int32_t t;
        int32_t s;
        image_t *image;

        /* Exact original constants are 4.0f and 0.25f: snap each bound to
         * the 9-point grid before deciding whether this side is visible. */
        rendererSkyMins[0][side] =
            (float)(floor((double)rendererSkyMins[0][side] * 4.0) *
                    0.25);
        rendererSkyMins[1][side] =
            (float)(floor((double)rendererSkyMins[1][side] * 4.0) *
                    0.25);
        rendererSkyMaxs[0][side] =
            (float)(ceil((double)rendererSkyMaxs[0][side] * 4.0) *
                    0.25);
        rendererSkyMaxs[1][side] =
            (float)(ceil((double)rendererSkyMaxs[1][side] * 4.0) *
                    0.25);

        if (rendererSkyMins[0][side] >= rendererSkyMaxs[0][side] ||
            rendererSkyMins[1][side] >= rendererSkyMaxs[1][side]) {
            continue;
        }

        minS = (int32_t)(rendererSkyMins[0][side] * 4.0f);
        minT = (int32_t)(rendererSkyMins[1][side] * 4.0f);
        maxS = (int32_t)(rendererSkyMaxs[0][side] * 4.0f);
        maxT = (int32_t)(rendererSkyMaxs[1][side] * 4.0f);

        if (minS < -R_SKY_GRID_CENTER)
            minS = -R_SKY_GRID_CENTER;
        else if (minS > R_SKY_GRID_CENTER)
            minS = R_SKY_GRID_CENTER;
        if (minT < -R_SKY_GRID_CENTER)
            minT = -R_SKY_GRID_CENTER;
        else if (minT > R_SKY_GRID_CENTER)
            minT = R_SKY_GRID_CENTER;
        if (maxS < -R_SKY_GRID_CENTER)
            maxS = -R_SKY_GRID_CENTER;
        else if (maxS > R_SKY_GRID_CENTER)
            maxS = R_SKY_GRID_CENTER;
        if (maxT < -R_SKY_GRID_CENTER)
            maxT = -R_SKY_GRID_CENTER;
        else if (maxT > R_SKY_GRID_CENTER)
            maxT = R_SKY_GRID_CENTER;

        minS += R_SKY_GRID_CENTER;
        minT += R_SKY_GRID_CENTER;
        maxS += R_SKY_GRID_CENTER;
        maxT += R_SKY_GRID_CENTER;

        for (t = minT; t < maxT; ++t) {
            for (s = minS; s < maxS; ++s) {
                const int32_t vertex =
                    side * R_SKY_GRID_SIDE * R_SKY_GRID_SIDE +
                    t * R_SKY_GRID_SIDE + s;

                indexes[indexCount + 0] = (uint16_t)(vertex + 1);
                indexes[indexCount + 1] = (uint16_t)vertex;
                indexes[indexCount + 2] =
                    (uint16_t)(vertex + R_SKY_GRID_SIDE);
                indexes[indexCount + 3] =
                    (uint16_t)(vertex + R_SKY_GRID_SIDE + 1);
                indexCount += 4;
            }
        }

        if (boxSet != R_SKY_BOX_OUTER) {
            image = shader->skyInnerBox[rendererSkyFaceImageOrder[side]];
        } else {
            image = shader->skyOuterBox[rendererSkyFaceImageOrder[side]];
        }
        GL_Bind(image);
        GL_DrawElements(GL_QUADS, indexCount, GL_UNSIGNED_SHORT, indexes);

        if (r_showtris->integer != 0) {
            qglPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            qglDisable(GL_TEXTURE_2D);
            qglColor3f(1.0f, 1.0f, 1.0f);
            GL_DrawElements(GL_QUADS, indexCount, GL_UNSIGNED_SHORT,
                            indexes);
            qglEnable(GL_TEXTURE_2D);
            qglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }

        backEnd.pc.indexCount = (int32_t)(
            (uint32_t)backEnd.pc.indexCount +
            (uint32_t)indexCount);
        backEnd.pc.vertexCount = (int32_t)(
            (uint32_t)backEnd.pc.vertexCount +
            (uint32_t)R_SKY_VERTEX_COUNT);
    }

    if (arraysLocked != qfalse)
        qglUnlockArraysEXT();
    if (boxSet != R_SKY_BOX_OUTER)
        qglDisable(GL_BLEND);
    if (storage->backend == R_SKY_VERTEX_BACKEND_ARB_BUFFER)
        qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
}

/* Source: CoDUOMP.exe 0x00515fc0..0x005161b5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00515fc0_005161b6.mcode. The complete
 * function was absent from Ghidra's function table and was recovered from the
 * executable gap. Name and source-level call sequence: exact same-module Mac
 * symbol RB_StageIteratorSky and its call graph.
 *
 * The Windows build inlines RB_DrawSun, R_FogOff, and both GLimp_LogComment
 * file guards; this maintained source restores those existing source helper
 * boundaries.
 * The iterator's portalPass parameter is part of the common stage-iterator
 * signature but is not read by the original body. */
void RB_StageIteratorSky(qboolean portalPass)
{
    shader_t *shader;

    (void)portalPass;

    if (r_fastsky->integer != 0) {
        tess.indexCount = 0;
        RB_DrawSun();
        return;
    }

    if (rendererFogCount != 0 &&
        (backEnd.refdef.rdflags & RDF_SKYBOX_PORTAL) == 0) {
        return;
    }

    /* 0x00516019..0x0051603d: the embedded view fog controls the decision
     * when registered. Otherwise only an active world fog can suppress the
     * sky; JLE 0x0051603f makes the no-fog case draw by default. */
    if ((backEnd.viewParms.glFog.registered != qfalse &&
         backEnd.viewParms.glFog.drawSky == qfalse) ||
        (backEnd.viewParms.glFog.registered == qfalse &&
         rendererCurrentFogIndex > 0 &&
         rendererFogs[R_FOG_WORLD_VIEW].drawSky == qfalse)) {
        return;
    }

    GLimp_LogComment("--- RB_StageIteratorSky ---\n");
    backEnd.refdef.rdflags |= RDF_DRAWING_SKYBOX;

    RB_ClipSkyPolygons(&tess);
    if (r_showsky->integer != 0)
        qglDepthRange(0.0, 0.0);
    else
        qglDepthRange(1.0, 1.0);

    R_FogOff();

    shader = tess.shader;
    if (shader->skyOuterBox[0] != NULL &&
        shader->skyOuterBox[0] != tr.defaultImage) {
        DrawSkyBox(shader, R_SKY_BOX_OUTER);
    }

    tess.indexCount = 0;
    RB_DrawSun();

    RB_BuildCloudData(&tess);
    if (tess.indexCount != 0)
        tr.stageIteratorFunc(qfalse);

    shader = tess.shader;
    if (shader->skyInnerBox[0] != NULL &&
        shader->skyInnerBox[0] != tr.defaultImage) {
        DrawSkyBox(shader, R_SKY_BOX_INNER);
    }

    R_FogOn();
    qglDepthRange(0.0, 1.0);
    backEnd.refdef.rdflags &= ~RDF_DRAWING_SKYBOX;
    backEnd.skyRenderedThisView = qtrue;
    GLimp_LogComment("----------\n");
}
