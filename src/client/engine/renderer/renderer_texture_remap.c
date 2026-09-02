#include "backend.h"

/* Source: CoDUOMP.exe 0x00504740..0x00504776.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00504740_00504777.mcode.
 * Name: same-module Mac symbol R_PicmipForImageFlags. MSVC LTCG also emits
 * the same operation inline in R_SetupTextureCoordinateRemap. */
int32_t R_PicmipForImageFlags(uint32_t imageFlags)
{
    int32_t picmip = (imageFlags & IMAGE_FLAG_USE_PICMIP2) != 0 ? r_picmip2->integer : r_picmip->integer;

    if ((imageFlags & IMAGE_FLAG_ALLOW_PICMIP) == 0)
        return 0;
    if (picmip < 0)
        return 0;
    if (picmip > 3)
        return 3;
    return picmip;
}

/* Source: CoDUOMP.exe 0x00503b20..0x00503c0f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00503b20_00503c0f.mcode.
 * Name and ordinary five-argument signature: exact same-module Mac symbol
 * R_SetupTextureCoordinateRemap. Windows instructions prove the primary-image
 * and texture-sheet fields, image-flag picmip selection, sheet-relative
 * offsets, coordinate-axis selection, and the two post-picmip scales. */
void R_SetupTextureCoordinateRemap(shader_t *shader, vec2_t scale, vec2_t offset, int32_t *sourceUIndex, int32_t *sourceVIndex)
{
    image_t *image = shader->primaryImage;
    image_t *textureSheet = image->link.textureSheet;
    int32_t picmip;

    *sourceUIndex = image->width < image->height;
    *sourceVIndex = 1 - *sourceUIndex;
    picmip = R_PicmipForImageFlags(image->flags);

    scale[0] = 1.0f / (float)textureSheet->width;
    scale[1] = 1.0f / (float)textureSheet->height;
    offset[0] = (float)image->state.sheet.x * scale[0];
    offset[1] = (float)image->state.sheet.y * scale[1];

    scale[*sourceUIndex] *= (float)(image->width >> picmip);
    scale[*sourceVIndex] *= (float)(image->height >> picmip);
}

/* Source: CoDUOMP.exe 0x00503c10..0x00503d41.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00503c10_00503d41.mcode.
 * Name and ordinary shader/count/coordinate-array signature: exact
 * same-module Mac symbol R_RemapTextureCoordinatesForSheet.
 *
 * Both source components are captured before either destination component is
 * written. That is required for rotated sheet entries, where the source axes
 * are swapped and an in-place first write would otherwise destroy the second
 * source value. */
void R_RemapTextureCoordinatesForSheet(shader_t *shader, int32_t vertexCount, vec2_t *texCoords)
{
    vec2_t scale;
    vec2_t offset;
    int32_t sourceUIndex;
    int32_t sourceVIndex;

    R_SetupTextureCoordinateRemap(shader, scale, offset, &sourceUIndex, &sourceVIndex);

    for (int32_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
        const float sourceU = texCoords[vertexIndex][sourceUIndex];
        const float sourceV = texCoords[vertexIndex][sourceVIndex];

        texCoords[vertexIndex][0] = sourceU * scale[0] + offset[0];
        texCoords[vertexIndex][1] = sourceV * scale[1] + offset[1];
    }
}
