#include "backend.h"

#include "gl_api.h"
#include "gl_state.h"

/* Source: CoDUOMP.exe 0x004bf820..0x004bf927.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004bf820_004bf927.mcode.
 * Name and signature: same-module Mac symbol RE_UploadCinematic, with argument
 * positions proved by the Windows stack accesses. The display width/height are
 * part of the shared renderer API but this upload-only path does not use them. */
void RE_UploadCinematic(int32_t width, int32_t height,
                        int32_t columns, int32_t rows,
                        const uint8_t *data, int32_t client,
                        qboolean dirty)
{
    image_t *image = tr.scratchImages[client];

    (void)width;
    (void)height;

    GL_Bind(image);
    if (image->width == columns && image->height == rows) {
        if (dirty) {
            qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, columns, rows,
                             GL_RGBA, GL_UNSIGNED_BYTE, data);
        }
        return;
    }

    image->uploadWidth = (uint16_t)columns;
    image->width = (uint16_t)columns;
    image->uploadHeight = (uint16_t)rows;
    image->height = (uint16_t)rows;
    qglTexImage2D(GL_TEXTURE_2D, 0, 3, columns, rows, 0,
                  GL_RGBA, GL_UNSIGNED_BYTE, data);
    qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

/* Source: CoDUOMP.exe 0x004bf340..0x004bf818.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004bf340_004bf818.mcode.
 * Name and signature: same-module Mac symbol RE_StretchRaw, with all nine
 * arguments independently proved by Windows stack accesses. MSVC inlined the
 * original R_SyncRenderThread and RB immediate-mode calls into this body; they
 * remain source-level calls here. */
void RE_StretchRaw(int32_t x, int32_t y, int32_t width, int32_t height,
                   int32_t columns, int32_t rows, const uint8_t *data,
                   int32_t client, qboolean dirty)
{
    image_t *image;
    int32_t columnPower = 0;
    int32_t rowPower = 0;
    int32_t uploadStart = 0;
    float columnsFloat;
    float rowsFloat;
    float s0;
    float s1;
    float t0;
    float t1;

    if (!tr.registered)
        return;

    R_SyncRenderThread();
    /* NOT_FROM_ORIGINAL_SOURCE: command execution can leave the widescreen
     * backdrop in the pending 2D batch. Submit it before drawing the raw frame
     * so the later frame swap cannot paint that backdrop over the cinematic. */
    if (tess.indexCount != 0)
        RB_EndSurface();

    if (r_speeds->integer != 0)
        uploadStart = ri.Milliseconds();

    if (columns > 1) {
        do {
            ++columnPower;
        } while ((1 << columnPower) < columns);
    }
    if (rows > 1) {
        do {
            ++rowPower;
        } while ((1 << rowPower) < rows);
    }
    if ((1 << columnPower) != columns || (1 << rowPower) != rows) {
        ri.Error(ERR_DROP,
                 "\x15" "Draw_StretchRaw: size not a power of 2: %i by %i",
                 columns, rows);
    }

    image = tr.scratchImages[client];
    GL_Bind(image);
    if (image->width == columns && image->height == rows) {
        if (dirty) {
            qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, columns, rows,
                             GL_RGBA, GL_UNSIGNED_BYTE, data);
        }
    } else {
        image->uploadWidth = (uint16_t)columns;
        image->width = (uint16_t)columns;
        image->uploadHeight = (uint16_t)rows;
        image->height = (uint16_t)rows;
        qglTexImage2D(GL_TEXTURE_2D, 0, 3, columns, rows, 0,
                      GL_RGBA, GL_UNSIGNED_BYTE, data);
        qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    if (r_speeds->integer != 0) {
        ri.Printf(R_PRINT_ALL, "qglTexSubImage2D %i, %i: %i msec\n",
                  columns, rows, ri.Milliseconds() - uploadStart);
    }

    RB_SetGL2D();
    RB_BeginImmediateMode();
    RB_glColor3f(tr.identityLight, tr.identityLight, tr.identityLight);
    RB_glBegin(GL_QUADS);

    columnsFloat = (float)columns;
    rowsFloat = (float)rows;
    s0 = 0.5f / columnsFloat;
    t0 = 0.5f / rowsFloat;

    RB_glTexCoord2f(s0, t0);
    RB_glVertex2i(x, y);
    s1 = (columnsFloat - 0.5f) / columnsFloat;
    RB_glTexCoord2f(s1, t0);
    RB_glVertex2i(x + width, y);
    t1 = (rowsFloat - 0.5f) / rowsFloat;
    RB_glTexCoord2f(s1, t1);
    RB_glVertex2i(x + width, y + height);
    RB_glTexCoord2f(s0, t1);
    RB_glVertex2i(x, y + height);

    RB_glEnd();
    RB_EndImmediateMode();
}
