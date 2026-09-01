#include "gl_state.h"

#include "backend.h"
#include "gl_api.h"

/* Source: CoDUOMP.exe 0x004bd990..0x004bda33.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004bd990_004bda33.mcode.
 * Name: same-module Mac symbol GL_Bind. */
void GL_Bind(image_t *image)
{
    const int32_t textureUnit = glState.currenttmu;
    uint32_t texnum;

    /* The DLL substitutes only the TEXNUM (EDI), never the image pointer (ESI).
     * Preserve that behavior for r_nobind: image->target, image->frameUsed, and
     * the qglBindTexture target still come from the caller's image. */
    if (image == NULL) {
        ri.Printf(R_PRINT_WARNING, "GL_Bind: NULL image\n");
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        image = tr.defaultImage;
    }
    texnum = image->texnum;

    if (r_nobind->integer != 0 && tr.dlightImage != NULL)
        texnum = tr.dlightImage->texnum;

    if (glState.currentTextureTargets[textureUnit] != image->target) {
        if (glState.currentTextureTargets[textureUnit] != 0)
            qglDisable(glState.currentTextureTargets[textureUnit]);
        qglEnable(image->target);
        glState.currentTextureTargets[textureUnit] = image->target;
    }

    if (glState.currenttextures[textureUnit] != texnum) {
        image->frameUsed = tr.frameCount;
        glState.currenttextures[textureUnit] = texnum;
        qglBindTexture(image->target, texnum);
    }
}

/* Source: CoDUOMP.exe 0x004bda40..0x004bda73.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004bda40_004bda74.mcode.
 * Provisional name: role proved by the per-TMU target cache and matching
 * qglDisable/qglEnable calls; no same-module Mac symbol survives for this
 * Windows helper. */
void GL_SetTextureTarget(uint32_t target)
{
    const int32_t textureUnit = glState.currenttmu;

    if (glState.currentTextureTargets[textureUnit] == target)
        return;

    if (glState.currentTextureTargets[textureUnit] != 0)
        qglDisable(glState.currentTextureTargets[textureUnit]);
    if (target != 0)
        qglEnable(target);
    glState.currentTextureTargets[textureUnit] = target;
}

/* Source: CoDUOMP.exe 0x004bda80..0x004bdab6.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004bda80_004bdab7.mcode.
 * Name: same-module Mac symbol GL_SelectTexture. */
void GL_SelectTexture(int32_t textureUnit)
{
    const uint32_t glTextureUnit = GL_TEXTURE0_ARB + (uint32_t)textureUnit;

    if (glState.currenttmu != textureUnit) {
        qglActiveTextureARB(glTextureUnit);
        glState.currenttmu = textureUnit;
    }
    if (glState.currentClientTmu != textureUnit) {
        qglClientActiveTextureARB(glTextureUnit);
        glState.currentClientTmu = textureUnit;
    }
}

/* Source: CoDUOMP.exe 0x004bdac0..0x004bdbae.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004bdac0_004bdbaf.mcode.
 * Provisional name: the Windows-only helper binds image1 on TMU 1 followed by
 * image0 on TMU 0 and leaves both server/client selectors on TMU 0. */
void GL_BindMultitexture(image_t *image0, image_t *image1)
{
    uint32_t texnum1 = image1->texnum;
    uint32_t texnum0 = image0->texnum;

    /* 0x4bdad7 r_nobind override (same as GL_Bind): when r_nobind && dlightImage,
     * BOTH texture-unit bind texnums become dlightImage->texnum (0x4bdae7
     * ESI=dlight->texnum; 0x4bdaea EDI=ESI), while frameUsed is still written to the
     * real image0/image1. A prior pass omitted this override entirely. */
    if (r_nobind->integer != 0 && tr.dlightImage != NULL) {
        texnum1 = tr.dlightImage->texnum;
        texnum0 = tr.dlightImage->texnum;
    }

    if (glState.currenttextures[1] != texnum1) {
        GL_SelectTexture(1);
        image1->frameUsed = tr.frameCount;
        glState.currenttextures[1] = texnum1;
        qglBindTexture(GL_TEXTURE_2D, texnum1);
    }

    if (glState.currenttextures[0] != texnum0) {
        GL_SelectTexture(0);
        image0->frameUsed = tr.frameCount;
        glState.currenttextures[0] = texnum0;
        qglBindTexture(GL_TEXTURE_2D, texnum0);
    }
}

/* Source: CoDUOMP.exe 0x004bdbb0..0x004bdc15.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004bdbb0_004bdc16.mcode.
 * Name and enum roles: same-module Mac symbol GL_Cull plus the proved
 * GL_CULL_FACE/GL_FRONT/GL_BACK calls. */
void GL_Cull(cullType_t cullType)
{
    if (glState.faceCulling == cullType)
        return;

    if (cullType == CT_TWO_SIDED) {
        qglDisable(GL_CULL_FACE);
        glState.faceCulling = cullType;
        return;
    }

    if (glState.faceCulling == CT_TWO_SIDED)
        qglEnable(GL_CULL_FACE);

    if (cullType == CT_BACK_SIDED)
        qglCullFace(backEnd.viewParms.isMirror ? GL_FRONT : GL_BACK);
    else
        qglCullFace(backEnd.viewParms.isMirror ? GL_BACK : GL_FRONT);

    glState.faceCulling = cullType;
}

/* Source: CoDUOMP.exe 0x004bdc20..0x004bdcbd.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004bdc20_004bdcbe.mcode.
 * Name: same-module Mac symbol GL_TexEnv. */
void GL_TexEnv(int32_t environment)
{
    if (glState.texEnv[glState.currenttmu] == environment)
        return;

    glState.texEnv[glState.currenttmu] = environment;
    switch (environment) {
    case GL_MODULATE:
    case GL_ADD:
    case GL_REPLACE:
    case GL_DECAL:
        qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, environment);
        break;
    default:
        ri.Error(ERR_DROP, "\x15GL_TexEnv: invalid env '%x' passed\n",
                 environment);
        break;
    }
}

/* Source: CoDUOMP.exe 0x004bdcc0..0x004bdce5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004bdcc0_004bdce6.mcode.
 * Name: same-module Mac symbol GL_Normalize. The argument is the exact GL
 * normalize/rescale capability, or zero to leave both disabled. */
void GL_Normalize(uint32_t target)
{
    if (glState.normalizeTarget == target)
        return;

    if (glState.normalizeTarget != 0)
        qglDisable(glState.normalizeTarget);
    glState.normalizeTarget = target;
    if (target != 0)
        qglEnable(target);
}

/* Source: CoDUOMP.exe 0x004bdcf0..0x004be0fb.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004bdcf0_004be0fc.mcode.
 * Name: same-module Mac symbol GL_State. The Windows body proves the CoD/UO
 * extensions for fixed-function lighting/fog, three polygon-offset modes, and
 * the four optional vertex/fragment-program capabilities. */
void GL_State(uint32_t stateBits)
{
    uint32_t diff = stateBits ^ glState.glStateBits;
    uint32_t sourceFactor;
    uint32_t destinationFactor;

    if (diff == 0)
        return;

    if ((diff & GLS_DEPTH_BITS) != 0) {
        if ((diff & GLS_DEPTHTEST_DISABLE) != 0) {
            if ((stateBits & GLS_DEPTHTEST_DISABLE) != 0)
                qglDisable(GL_DEPTH_TEST);
            else
                qglEnable(GL_DEPTH_TEST);
        }

        if ((stateBits & GLS_DEPTHFUNC_EQUAL) != 0)
            qglDepthFunc(GL_EQUAL);
        else if ((stateBits & GLS_DEPTHFUNC_GREATER) != 0)
            /* 0x4bdd45 PUSH 0x207 = GL_ALWAYS, not GL_GEQUAL (0x206): this state bit
             * installs an always-pass depth test (siblings EQUAL->0x202, default
             * LEQUAL->0x203 confirm the exact reads). A prior pass used GL_GEQUAL. */
            qglDepthFunc(GL_ALWAYS);
        else
            qglDepthFunc(GL_LEQUAL);
    }

    if ((diff & GLS_DEPTHMASK_TRUE) != 0)
        qglDepthMask((stateBits & GLS_DEPTHMASK_TRUE) != 0);

    if ((diff & GLS_POLYMODE_LINE) != 0) {
        qglPolygonMode(GL_FRONT_AND_BACK,
                       (stateBits & GLS_POLYMODE_LINE) != 0
                           ? GL_LINE : GL_FILL);
    }

    if ((diff & GLS_ATEST_BITS) != 0) {
        switch (stateBits & GLS_ATEST_BITS) {
        case 0:
            qglDisable(GL_ALPHA_TEST);
            break;
        case GLS_ATEST_GT_0:
            qglEnable(GL_ALPHA_TEST);
            qglAlphaFunc(GL_GREATER, 0.0f);
            break;
        case GLS_ATEST_LT_128:
            qglEnable(GL_ALPHA_TEST);
            qglAlphaFunc(GL_LESS, 0.5f);
            break;
        case GLS_ATEST_GE_128:
            qglEnable(GL_ALPHA_TEST);
            qglAlphaFunc(GL_GEQUAL, 0.5f);
            break;
        default:
            break;
        }
    }

    if ((diff & GLS_LIGHTING) != 0) {
        if ((stateBits & GLS_LIGHTING) != 0 &&
            tr.world->lightIndexCount != 0 && r_fullbright->integer == 0) {
            qglEnable(GL_LIGHTING);
        } else {
            qglDisable(GL_LIGHTING);
            if ((stateBits & GLS_LIGHTING) != 0) {
                if ((glState.clientStateBits & GLS_CLIENT_COLOR_ARRAY) != 0) {
                    qglDisableClientState(GL_COLOR_ARRAY);
                    glState.clientStateBits &= ~GLS_CLIENT_COLOR_ARRAY;
                }
                qglColor3f(tr.identityLight, tr.identityLight,
                           tr.identityLight);
                stateBits &= ~GLS_LIGHTING;
            }
        }
    }

    if ((diff & GLS_FOG) != 0) {
        if ((stateBits & GLS_FOG) != 0) {
            R_FogOn();
        } else if ((glState.glStateBits & GLS_FOG) != 0) {
            qglDisable(GL_FOG);
            glState.glStateBits &= ~GLS_FOG;
        }
    }

    if ((diff & GLS_POLYGON_OFFSET_BITS) != 0) {
        if ((stateBits & GLS_POLYGON_OFFSET_BITS) != 0) {
            qglEnable(GL_POLYGON_OFFSET_FILL);
            if ((stateBits & GLS_POLYGON_OFFSET) != 0) {
                qglPolygonOffset(r_polygonOffsetFactor->value,
                                 r_polygonOffsetUnits->value);
            } else if ((stateBits & GLS_POLYGON_OFFSET_DOUBLE) != 0) {
                qglPolygonOffset(r_polygonOffsetFactor->value * 2.0f,
                                 r_polygonOffsetUnits->value * 2.0f);
            } else {
                qglPolygonOffset(0.0f, r_polygonOffsetUnits->value);
            }
        } else {
            qglDisable(GL_POLYGON_OFFSET_FILL);
        }
    }

    if ((diff & GLS_VERTEX_PROGRAM_ARB) != 0) {
        if ((stateBits & GLS_VERTEX_PROGRAM_ARB) != 0)
            qglEnable(GL_VERTEX_PROGRAM_ARB);
        else
            qglDisable(GL_VERTEX_PROGRAM_ARB);
    }
    if ((diff & GLS_TEXTURE_SHADER_NV) != 0) {
        if ((stateBits & GLS_TEXTURE_SHADER_NV) != 0)
            qglEnable(GL_TEXTURE_SHADER_NV);
        else
            qglDisable(GL_TEXTURE_SHADER_NV);
    }
    if ((diff & GLS_REGISTER_COMBINERS_NV) != 0) {
        if ((stateBits & GLS_REGISTER_COMBINERS_NV) != 0)
            qglEnable(GL_REGISTER_COMBINERS_NV);
        else
            qglDisable(GL_REGISTER_COMBINERS_NV);
    }
    if ((diff & GLS_FRAGMENT_SHADER_ATI) != 0) {
        if ((stateBits & GLS_FRAGMENT_SHADER_ATI) != 0)
            qglEnable(GL_FRAGMENT_SHADER_ATI);
        else
            qglDisable(GL_FRAGMENT_SHADER_ATI);
    }

    glState.glStateBits = stateBits;

    if ((diff & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS)) == 0)
        return;

    if ((stateBits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS)) == 0) {
        qglDisable(GL_BLEND);
        R_SetFogColor();
        return;
    }

    qglEnable(GL_BLEND);
    switch (stateBits & GLS_SRCBLEND_BITS) {
    case GLS_SRCBLEND_ZERO: sourceFactor = GL_ZERO; break;
    case GLS_SRCBLEND_ONE: sourceFactor = GL_ONE; break;
    case GLS_SRCBLEND_DST_COLOR: sourceFactor = GL_DST_COLOR; break;
    case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:
        sourceFactor = GL_ONE_MINUS_DST_COLOR;
        break;
    case GLS_SRCBLEND_SRC_ALPHA: sourceFactor = GL_SRC_ALPHA; break;
    case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA:
        sourceFactor = GL_ONE_MINUS_SRC_ALPHA;
        break;
    case GLS_SRCBLEND_DST_ALPHA: sourceFactor = GL_DST_ALPHA; break;
    case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:
        sourceFactor = GL_ONE_MINUS_DST_ALPHA;
        break;
    case GLS_SRCBLEND_ALPHA_SATURATE:
        sourceFactor = GL_SRC_ALPHA_SATURATE;
        break;
    default:
        sourceFactor = GL_ONE;
        ri.Error(ERR_DROP,
                 "\x15GL_State: invalid src blend state bits\n");
        break;
    }

    switch (stateBits & GLS_DSTBLEND_BITS) {
    case GLS_DSTBLEND_ZERO: destinationFactor = GL_ZERO; break;
    case GLS_DSTBLEND_ONE: destinationFactor = GL_ONE; break;
    case GLS_DSTBLEND_SRC_COLOR: destinationFactor = GL_SRC_COLOR; break;
    case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR:
        destinationFactor = GL_ONE_MINUS_SRC_COLOR;
        break;
    case GLS_DSTBLEND_SRC_ALPHA: destinationFactor = GL_SRC_ALPHA; break;
    case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA:
        destinationFactor = GL_ONE_MINUS_SRC_ALPHA;
        break;
    case GLS_DSTBLEND_DST_ALPHA: destinationFactor = GL_DST_ALPHA; break;
    case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA:
        destinationFactor = GL_ONE_MINUS_DST_ALPHA;
        break;
    default:
        destinationFactor = GL_ONE;
        ri.Error(ERR_DROP,
                 "\x15GL_State: invalid dst blend state bits\n");
        break;
    }

    qglBlendFunc(sourceFactor, destinationFactor);
    R_SetFogColor();
}
