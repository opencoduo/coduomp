#include "backend.h"

#include "compat/coduo_native_x87.h"
#include "../platform/crt_boundary.h"
#include "gl_api.h"
#include "gl_state.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum {
    R_WATER_FFT_TABLE_SIZE = 256,
    R_WATER_WAVE_TABLE_SIZE = 1024,
    R_WATER_WAVE_TABLE_MASK = R_WATER_WAVE_TABLE_SIZE - 1,
    R_WATER_MAX_MAPS = 16,
    R_WATER_IMAGE_TRACK = 8,
    R_WATER_I386_MAP_BYTES = 52,
    R_WATER_MAX_TEXTURE_DIMENSION = 256,
    R_WATER_MAX_TEXELS = R_WATER_MAX_TEXTURE_DIMENSION * R_WATER_MAX_TEXTURE_DIMENSION,
    R_WATER_X86_SHIFT_COUNT_MASK = 31
};

#define R_WATER_TWO_PI_F 6.2831854820251465f /* 0x40c90fdb */
#define R_WATER_FFT_ANGLE_STEP_F 0.024543693289160728f /* 0x3cc90fdb: 2*pi/256 */
#define R_WATER_RANDOM_UNIT_F 0.000030517578125f /* 0x38000000: 1/32768 */
#define R_WATER_PHASE_SCALE_F 162.97465515136719f /* 0x4322f983: 1024/(2*pi) */
#define R_WATER_TIME_SCALE_F 0.0010000000474974513f /* 0x3a83126f: milliseconds to seconds */
#define R_WATER_NORMAL_SCALE_F 127.0f /* 0x42fe0000 */
#define R_WATER_EQ_DIRECTION_EPSILON 0.0000000001 /* 0x3ddb7cdfd9d7bdbb */
#define R_WATER_EQ_SCALAR_EPSILON 0.10000000000000001 /* 0x3fb999999999999a */
#define R_WATER_EQ_FINE_EPSILON 0.0010000000474974513 /* 0x3f50624de0000000 */

/* Original 0x0389e790..0x0389ead0 and 0x0389f2d0. R_InitWater clears the
 * count; R_GetWaterTexture owns insertion into the sixteen-record cache. */
static shader_water_map_t rendererWaterMaps[R_WATER_MAX_MAPS];
static int32_t rendererWaterMapCount;

/* Original 0x0389ead0 and 0x0389f2d8. FFT_Init fills both tables once during
 * renderer initialization. */
static renderer_water_complex_t rendererWaterFftTrig[R_WATER_FFT_TABLE_SIZE];
static int32_t rendererWaterFftBitReverse[R_WATER_FFT_TABLE_SIZE];

/* Source: CoDUOMP.exe 0x0051c790..0x0051c79e.
 * Name and source boundary: exact same-module Mac symbol R_InitWater. The
 * Windows compiler also emits these two operations inline at
 * 0x004c505d..0x004c5068. */
void R_InitWater(void)
{
    rendererWaterMapCount = 0;
    FFT_Init();
}

/* Source: CoDUOMP.exe 0x0051be90..0x0051bf5c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0051be90_0051bf5d.mcode.
 * Name and source boundary: exact same-module Mac symbol FFT_Init. */
void FFT_Init(void)
{
    for (int32_t value = 0; value < R_WATER_FFT_TABLE_SIZE; ++value) {
        int32_t reversed = 0;

        for (int32_t bit = 0; bit < 8; ++bit) {
            if ((value & (1 << bit)) != 0)
                reversed |= 1 << (7 - bit);
        }
        rendererWaterFftBitReverse[value] = reversed;

        const float angle = (float)value * R_WATER_FFT_ANGLE_STEP_F;
        rendererWaterFftTrig[value].real = cosf(angle);
        rendererWaterFftTrig[value].imaginary = sinf(angle);
    }
}

/* Source: CoDUOMP.exe 0x0051bf60..0x0051c17d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0051bf60_0051c17e.mcode.
 * Name and source boundary: exact same-module Mac symbol FFT. The initial
 * radix-four pass and later radix-two stages preserve the DLL's operation
 * ordering; callers supply power-of-two counts no larger than 256. */
void FFT(renderer_water_complex_t *data, int32_t log2Count, int32_t stride)
{
    const int32_t count = 1 << log2Count;

    for (int32_t index = 0; index < count; ++index) {
        const int32_t reversed = rendererWaterFftBitReverse[index] >> (8 - log2Count);

        if (reversed < index) {
            const int32_t sourceIndex = index * stride;
            const int32_t destinationIndex = reversed * stride;
            const renderer_water_complex_t temporary = data[sourceIndex];

            data[sourceIndex] = data[destinationIndex];
            data[destinationIndex] = temporary;
        }
    }

    for (int32_t index = 0; index < count; index += 4) {
        renderer_water_complex_t a = data[(index + 0) * stride];
        renderer_water_complex_t b = data[(index + 1) * stride];
        renderer_water_complex_t c = data[(index + 2) * stride];
        renderer_water_complex_t d = data[(index + 3) * stride];
        renderer_water_complex_t *outA = &data[(index + 0) * stride];
        renderer_water_complex_t *outB = &data[(index + 1) * stride];
        renderer_water_complex_t *outC = &data[(index + 2) * stride];
        renderer_water_complex_t *outD = &data[(index + 3) * stride];
        const float realAB = a.real + b.real;
        const float realCD = c.real + d.real;
        const float imaginaryAB = a.imaginary + b.imaginary;
        const float imaginaryCD = c.imaginary + d.imaginary;
        const float realDifferenceAB = a.real - b.real;
        const float realDifferenceCD = c.real - d.real;
        const float imaginaryDifferenceAB = a.imaginary - b.imaginary;
        const float imaginaryDifferenceCD = c.imaginary - d.imaginary;

        outA->real = realAB + realCD;
        outC->real = realAB - realCD;
        outA->imaginary = imaginaryAB + imaginaryCD;
        outC->imaginary = imaginaryAB - imaginaryCD;
        outB->real = realDifferenceAB - imaginaryDifferenceCD;
        outD->real = realDifferenceAB + imaginaryDifferenceCD;
        outB->imaginary = imaginaryDifferenceAB + realDifferenceCD;
        outD->imaginary = imaginaryDifferenceAB - realDifferenceCD;
    }

    int32_t span = 4;
    int32_t twiddleShift = 5;
    while (span < count) {
        const int32_t doubleSpan = span * 2;

        for (int32_t twiddleIndex = 0; twiddleIndex < span; ++twiddleIndex) {
            const renderer_water_complex_t twiddle = rendererWaterFftTrig[twiddleIndex << twiddleShift];

            for (int32_t index = twiddleIndex; index < count; index += doubleSpan) {
                renderer_water_complex_t *lower = &data[index * stride];
                renderer_water_complex_t *upper = &data[(index + span) * stride];
                const float productReal = twiddle.real * upper->real - twiddle.imaginary * upper->imaginary;
                const float productImaginary = twiddle.imaginary * upper->real + twiddle.real * upper->imaginary;
                const renderer_water_complex_t lowerValue = *lower;

                upper->real = lowerValue.real - productReal;
                upper->imaginary = lowerValue.imaginary - productImaginary;
                lower->real = lowerValue.real + productReal;
                lower->imaginary = lowerValue.imaginary + productImaginary;
            }
        }

        --twiddleShift;
        span = doubleSpan;
    }
}

/* Source: CoDUOMP.exe 0x0051c180..0x0051c20a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0051c180_0051c20b.mcode.
 * Name and source boundary: exact same-module Mac symbol GaussianRandom. */
void GaussianRandom(float *real, float *imaginary)
{
    float x;
    long double y;
    long double radiusSquared;

    do {
        x = (float)((long double)coduo_crt_rand() * (long double)R_WATER_RANDOM_UNIT_F * 2.0L - 1.0L);
        y = (long double)coduo_crt_rand() * (long double)R_WATER_RANDOM_UNIT_F * 2.0L - 1.0L;
        radiusSquared = y * y + (long double)x * (long double)x;
    } while (radiusSquared > 1.0L);

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    const long double scale = sqrtl((-2.0L * logl(radiusSquared)) / radiusSquared);
    *real = (float)((long double)x * scale);
    *imaginary = (float)(y * scale);
}

/* Source: CoDUOMP.exe 0x0051c210..0x0051c3cc.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0051c210_0051c3cd.mcode.
 * Name and source boundary: exact same-module Mac symbol
 * PickWaterFrequencies. The spectrum chain retains its x87 intermediates;
 * only the proved wave-step, length-scale, wave-number, angular-frequency,
 * and output stores round to binary32. An unordered wind dot follows the
 * original spectrum-producing branch. */
void PickWaterFrequencies(shader_water_map_t *waterMap)
{
    const float rowWaveStep =
        (float)((long double)R_WATER_TWO_PI_F / ((long double)waterMap->textureHeight * waterMap->horizontalWorldLength));
    const float columnWaveStep =
        (float)((long double)R_WATER_TWO_PI_F / ((long double)waterMap->textureWidth * waterMap->verticalWorldLength));
    const long double windVelocitySquared = (long double)waterMap->windVelocity * waterMap->windVelocity;
    const float windLengthScaleSquared = (float)((windVelocitySquared / (long double)waterMap->gravity) * windVelocitySquared);
    const int32_t halfHeight = waterMap->textureHeight / 2;
    const int32_t halfWidth = waterMap->textureWidth / 2;
    int32_t frequencyIndex = 0;

    for (int32_t row = -halfHeight; row < halfHeight; ++row) {
        const float rowWaveNumber = (float)((long double)row * rowWaveStep);
        const float rowWaveNumberSquared = (float)((long double)rowWaveNumber * rowWaveNumber);

        for (int32_t column = -halfWidth; column < halfWidth; ++column) {
            const float columnWaveNumber = (float)((long double)column * columnWaveStep);
            float gaussianReal;
            float gaussianImaginary;

            /* The DLL consumes both CRT random values before evaluating the
             * spectrum gate for this cell. */
            GaussianRandom(&gaussianReal, &gaussianImaginary);

            const long double waveNumberSquared = (long double)columnWaveNumber * columnWaveNumber + rowWaveNumberSquared;
            const float angularFrequencySquared = (float)(sqrtl(waveNumberSquared) * waterMap->gravity);
            const long double windDotWave =
                (long double)columnWaveNumber * waterMap->windDirection[1] + (long double)rowWaveNumber * waterMap->windDirection[0];

            if (!(windDotWave <= 0.0L)) {
                const long double exponential = expl(-1.0L / (waveNumberSquared * windLengthScaleSquared));
                const long double waveNumberSixth = (waveNumberSquared * waveNumberSquared) * waveNumberSquared;
                const long double spectrum =
                    (((long double)waterMap->amplitude * exponential) / waveNumberSixth) * (windDotWave * windDotWave);
                const long double scale = (long double)waterMap->amplitude * sqrtl(0.5L * spectrum);

                waterMap->initialFrequencies[frequencyIndex].real = (float)((long double)gaussianReal * scale);
                waterMap->initialFrequencies[frequencyIndex].imaginary = (float)((long double)gaussianImaginary * scale);
                waterMap->angularFrequencies[frequencyIndex] = sqrtf(angularFrequencySquared);
            } else {
                waterMap->initialFrequencies[frequencyIndex].real = 0.0f;
                waterMap->initialFrequencies[frequencyIndex].imaginary = 0.0f;
                waterMap->angularFrequencies[frequencyIndex] = 0.0f;
            }
            frequencyIndex = (int32_t)((uint32_t)frequencyIndex + 1u);
        }
    }
}

/* Source: CoDUOMP.exe 0x0051c3d0..0x0051c4c0.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0051c3d0_0051c4c1.mcode.
 * Name and source boundary: exact same-module Mac symbol
 * WaterFrequenciesAtTime. The +255 phase offset is the exact DLL operation,
 * not a conventional quarter-wave value inferred from the table size. */
void WaterFrequenciesAtTime(renderer_water_complex_t *frequencies, const shader_water_map_t *waterMap, float timeSeconds)
{
    const int32_t halfHeight = waterMap->textureHeight / 2;
    const int32_t halfWidth = waterMap->textureWidth / 2;
    int32_t index = 0;

    for (int32_t row = -halfHeight; row < halfHeight; ++row) {
        for (int32_t column = -halfWidth; column < halfWidth; ++column) {
            const float angularFrequency = waterMap->angularFrequencies[index];

            if (angularFrequency == 0.0f) {
                frequencies[index].real = 0.0f;
                frequencies[index].imaginary = 0.0f;
            } else {
                const long double retainedPhase = (long double)timeSeconds * angularFrequency * R_WATER_PHASE_SCALE_F;
                const float storedPhase = (float)retainedPhase;
                const int32_t phase = coduo_x87_fistp_i32((long double)storedPhase);
                const int32_t sineIndex = (int32_t)((uint32_t)phase & R_WATER_WAVE_TABLE_MASK);
                const int32_t cosineIndex = (int32_t)(((uint32_t)phase + 255u) & R_WATER_WAVE_TABLE_MASK);

                frequencies[index].real = waterMap->initialFrequencies[index].real * tr.sinTable[cosineIndex];
                frequencies[index].imaginary = waterMap->initialFrequencies[index].imaginary * tr.sinTable[sineIndex];
            }
            index = (int32_t)((uint32_t)index + 1u);
        }
    }
}

/* Source: CoDUOMP.exe 0x0051c4d0..0x0051c55a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0051c4d0_0051c55b.mcode.
 * Name and source boundary: exact same-module Mac symbol
 * WaterAmplitudesFromFrequencies. */
void WaterAmplitudesFromFrequencies(renderer_water_complex_t *amplitudes, const shader_water_map_t *waterMap)
{
    uint32_t rowLog2 = 0;
    uint32_t columnLog2 = 0;

    while ((1u << (rowLog2 & R_WATER_X86_SHIFT_COUNT_MASK)) != (uint32_t)waterMap->textureHeight) {
        rowLog2 += 1u;
    }
    while ((1u << (columnLog2 & R_WATER_X86_SHIFT_COUNT_MASK)) != (uint32_t)waterMap->textureWidth) {
        columnLog2 += 1u;
    }

    for (int32_t row = 0; row < waterMap->textureHeight; ++row) {
        FFT(&amplitudes[row * waterMap->textureWidth], (int32_t)columnLog2, 1);
    }
    for (int32_t column = 0; column < waterMap->textureWidth; ++column) {
        FFT(&amplitudes[column], (int32_t)rowLog2, waterMap->textureWidth);
    }
}

/* Source: CoDUOMP.exe 0x0051c560..0x0051c69e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0051c560_0051c69f.mcode.
 * Name and source boundary: exact same-module Mac symbol
 * WaterNormalsFromAmplitudes. The wrapped neighbors and the 32-bit squared
 * texel count reproduce the original flattened power-of-two addressing. The
 * nested row/column traversal and shift/add packing preserve the PE's exact
 * control flow and carry behavior. */
void WaterNormalsFromAmplitudes(uint32_t *normalPixels, renderer_water_complex_t *amplitudes, const shader_water_map_t *waterMap)
{
    const int32_t texelCount = (int32_t)((uint32_t)waterMap->textureWidth * (uint32_t)waterMap->textureHeight);
    const int32_t wrappedTexelCountSquared = (int32_t)((uint32_t)texelCount * (uint32_t)texelCount);
    const float normalZSquared = (float)wrappedTexelCountSquared;
    const uint32_t texelMask = (uint32_t)texelCount - 1u;

    for (int32_t index = 0; index < texelCount; ++index) {
        const float real = amplitudes[index].real;
        const float imaginary = amplitudes[index].imaginary;

        amplitudes[index].real = (float)sqrtl((long double)imaginary * imaginary + (long double)real * real);
    }

    uint32_t currentIndex = 0;
    uint32_t previousColumnIndex = (uint32_t)waterMap->textureWidth - 1u;
    uint32_t previousRowIndex = (uint32_t)texelCount - (uint32_t)waterMap->textureWidth;

    for (int32_t row = 0; row < waterMap->textureHeight; ++row) {
        uint32_t columnsRemaining = (uint32_t)waterMap->textureWidth;
        const uint32_t rowStart = currentIndex;

        do {
            const long double columnDifference = (long double)amplitudes[previousColumnIndex].real - amplitudes[currentIndex].real;
            const long double rowDifference = (long double)amplitudes[previousRowIndex].real - amplitudes[currentIndex].real;
            const long double denominator =
                sqrtl(rowDifference * rowDifference + (columnDifference * columnDifference + (long double)normalZSquared));
            const long double scale = (long double)R_WATER_NORMAL_SCALE_F / denominator;
            const int32_t normalZ = coduo_x87_fistp_i32((long double)texelCount * scale);
            const int32_t normalY = coduo_x87_fistp_i32(rowDifference * scale);
            const int32_t normalX = coduo_x87_fistp_i32(columnDifference * scale);
            uint32_t packedNormal = UINT32_C(0x0000ff7f) + (uint32_t)normalZ;

            packedNormal <<= 8;
            packedNormal += 127u + (uint32_t)normalY;
            packedNormal <<= 8;
            packedNormal += 127u + (uint32_t)normalX;
            normalPixels[currentIndex] = packedNormal;

            previousColumnIndex = currentIndex;
            currentIndex += 1u;
            previousRowIndex += 1u;
        } while (--columnsRemaining != 0);

        previousColumnIndex += (uint32_t)waterMap->textureWidth;
        previousRowIndex = (rowStart + (uint32_t)texelCount) & texelMask;
    }
}

/* Source: CoDUOMP.exe 0x0051c6a0..0x0051c786.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0051c6a0_0051c787.mcode.
 * Name and source boundary: exact same-module Mac symbol WatersEquivalent. */
qboolean WatersEquivalent(const shader_water_map_t *first, const shader_water_map_t *second)
{
    if (first->textureWidth != second->textureWidth || first->textureHeight != second->textureHeight ||
        first->horizontalWorldLength != second->horizontalWorldLength || first->verticalWorldLength != second->verticalWorldLength ||
        fabsl((long double)first->amplitude - second->amplitude) > R_WATER_EQ_FINE_EPSILON ||
        fabsl((long double)first->gravity - second->gravity) > R_WATER_EQ_SCALAR_EPSILON ||
        fabsl((long double)first->windVelocity - second->windVelocity) > R_WATER_EQ_SCALAR_EPSILON) {
        return qfalse;
    }

    const long double firstLengthSquared =
        (long double)first->windDirection[0] * first->windDirection[0] + (long double)first->windDirection[1] * first->windDirection[1];
    const long double secondLengthSquared =
        (long double)second->windDirection[0] * second->windDirection[0] + (long double)second->windDirection[1] * second->windDirection[1];
    const long double directionDot =
        (long double)first->windDirection[0] * second->windDirection[0] + (long double)first->windDirection[1] * second->windDirection[1];
    const long double directionCosine = directionDot / (sqrtl(firstLengthSquared * secondLengthSquared) + R_WATER_EQ_DIRECTION_EPSILON);

    return fabsl(directionCosine - 1.0L) <= R_WATER_EQ_FINE_EPSILON ? qtrue : qfalse;
}

/* Source: CoDUOMP.exe 0x0051c7a0..0x0051c8fd.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0051c7a0_0051c8fe.mcode.
 * Name and source boundary: exact same-module Mac symbol R_GetWaterTexture.
 * The PE inlines R_CreateImage into its allocation/create/free sequence; the
 * maintained call restores that independently proved source-level boundary.
 * Its generated image suffix is the 52-byte i386 map-record offset, not the
 * logical map index. */
shader_water_map_t *R_GetWaterTexture(const shader_water_map_t *configuration)
{
    int32_t mapIndex;

    for (mapIndex = 0; mapIndex < rendererWaterMapCount; ++mapIndex) {
        if (WatersEquivalent(&rendererWaterMaps[mapIndex], configuration))
            return &rendererWaterMaps[mapIndex];
    }

    if (mapIndex == R_WATER_MAX_MAPS) {
        ri.Error(ERR_DROP, "\x15map uses more than %i waterMap textures", R_WATER_MAX_MAPS);
    }

    shader_water_map_t *waterMap = &rendererWaterMaps[mapIndex];
    const int32_t texelCount = (int32_t)((uint32_t)configuration->textureWidth * (uint32_t)configuration->textureHeight);
    const uint32_t frequencyBytes = (uint32_t)texelCount * 8u;
    const uint32_t allocationBytes = (uint32_t)texelCount * 12u;
    const uint32_t normalPixelBytes = (uint32_t)texelCount * 4u;
    const int32_t mapByteOffset = (int32_t)((uint32_t)mapIndex * R_WATER_I386_MAP_BYTES);

    /* ORIGINAL_BINARY_BEHAVIOR: 0x0051c814..0x0051c81b copies all thirteen
     * dwords (0x34 bytes) from the partially initialized parser temporary.
     * The image and two frequency pointers are replaced below, but the +0x04
     * uploadFrame word is not. RB_UploadWaterTexture subsequently compares
     * that copied stack value with tr.frameCount. Do not zero-initialize this
     * record: doing so would remove an original-machine-code behavior. */
    *waterMap = *configuration;
    waterMap->initialFrequencies = ri.Hunk_Alloc((size_t)allocationBytes);
    waterMap->angularFrequencies = (float *)((uint8_t *)waterMap->initialFrequencies + frequencyBytes);
    PickWaterFrequencies(waterMap);

    uint8_t *normalPixels = ri.Hunk_AllocateTempMemory((size_t)normalPixelBytes);
    memset(normalPixels, UINT8_MAX, (size_t)normalPixelBytes);
    waterMap->image = R_CreateImage(va("*water%i", mapByteOffset), normalPixels, configuration->textureWidth, configuration->textureHeight,
                                    GL_RGBA, 0, R_WATER_IMAGE_TRACK, NULL);
    ri.Hunk_FreeTempMemory(normalPixels);

    if (waterMap->image == NULL)
        return NULL;

    ++rendererWaterMapCount;
    return waterMap;
}

/* Source: CoDUOMP.exe 0x0051c900..0x0051c9f1.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0051c900_0051c9f2.mcode.
 * Name and source boundary: exact same-module Mac symbol
 * RB_UploadWaterTexture. The two fixed-size arrays reproduce the DLL's
 * maximum 256x256 temporary stack workspace. */
void RB_UploadWaterTexture(shader_water_map_t *waterMap, int32_t time, int32_t textureUnit)
{
    renderer_water_complex_t amplitudes[R_WATER_MAX_TEXELS];
    uint32_t normalPixels[R_WATER_MAX_TEXELS];

    if (waterMap->uploadFrame == tr.frameCount)
        return;

    waterMap->uploadFrame = tr.frameCount;
    WaterFrequenciesAtTime(amplitudes, waterMap, (float)time * R_WATER_TIME_SCALE_F);
    WaterAmplitudesFromFrequencies(amplitudes, waterMap);
    WaterNormalsFromAmplitudes(normalPixels, amplitudes, waterMap);
    GL_SelectTexture(textureUnit);
    GL_Bind(waterMap->image);
    qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, waterMap->image->width, waterMap->image->height, GL_RGBA, GL_UNSIGNED_BYTE, normalPixels);
}
