#include "collision_static_models.h"

#include "compat/crt/msvc_compat.h"
#include "collision_world_sector.h"
#include "qcommon/com_parse.h"
#include "math/q_math.h"
#include "qcommon/hunk.h"
#include "qcommon/q_shared_types.h"
#include "animation/xmodel.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

void Com_Error(errorParm_t code, const char *format, ...);
extern char *cm_entityString;

enum {
    CM_STATIC_MODEL_AXIS_COUNT = 3,
    CM_STATIC_MODEL_NAME_BUFFER_SIZE = MAX_QPATH,
    CM_STATIC_MODEL_XMODEL_PREFIX_LENGTH = 7
};
/* NOT_FROM_ORIGINAL_SOURCE: classifies a key before Com_Parse reuses its
 * shared token buffer, avoiding the retail key/value stack copies. */
typedef enum cmStaticModelEntityField_e {
    CM_STATIC_MODEL_FIELD_OTHER,
    CM_STATIC_MODEL_FIELD_CLASSNAME,
    CM_STATIC_MODEL_FIELD_MODEL,
    CM_STATIC_MODEL_FIELD_ORIGIN,
    CM_STATIC_MODEL_FIELD_ANGLES,
    CM_STATIC_MODEL_FIELD_SCALE_VECTOR,
    CM_STATIC_MODEL_FIELD_SCALE
} cmStaticModelEntityField_t;
typedef char
    cm_static_model_name_has_prefix_and_payload_capacity[CM_STATIC_MODEL_NAME_BUFFER_SIZE >= CM_STATIC_MODEL_XMODEL_PREFIX_LENGTH + 2 ? 1
                                                                                                                                      : -1];

#if !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "collision_static_models.c requires a target behavior"
#endif

/*
 * Complete static collision-model construction and entity loader:
 *
 *   CoDUOMP.exe  0x004230e0..0x00423796
 *   coduo_lnxded 0x0805366c..0x08053fa1
 *
 * Platform bodies retain their proven allocator, parser/CRT, and
 * floating-point behavior. Security checks now guard the original fixed
 * token-buffer operations.
 */
#if defined(WINDOWS_BEHAVIOR)

enum {
    CM_STATIC_MODEL_HUNK_ALIGNMENT = 32
};
/* Source: CoDUOMP.exe 0x004230e0..0x004230ed, recovered from the
 * executable gap. Name: exact same-module Mac symbol
 * CM_Hunk_AllocXModelMesh. */
void *CM_Hunk_AllocXModelMesh(size_t size)
{
    return Hunk_AllocAlignInternal(size, CM_STATIC_MODEL_HUNK_ALIGNMENT);
}

/* Source: CoDUOMP.exe 0x004230f0..0x004230fd, recovered from the
 * executable gap. Name: exact same-module Mac symbol
 * CM_Hunk_AllocXModel. */
void *CM_Hunk_AllocXModel(size_t size)
{
    return Hunk_AllocAlignInternal(size, CM_STATIC_MODEL_HUNK_ALIGNMENT);
}

/* Source: CoDUOMP.exe 0x00423100..0x004232e8.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00423100_004232e9.mcode.
 * Name: exact same-module Mac symbol CM_CreateStaticModel. */
void CM_CreateStaticModel(worldSectorAreaLink_t *areaLink, const char *modelName, const vec3_t origin, const vec3_t angles,
                          const vec3_t scale)
{
    if (modelName == NULL || modelName[0] == '\0') {
        Com_Error(ERR_DROP,
                  "\x15"
                  "Invalid static model name @ %f %f %f\n",
                  (double)origin[0], (double)origin[1], (double)origin[2]);
    }
    if (scale[0] == 0.0f) {
        Com_Error(ERR_DROP,
                  "\x15"
                  "Static model [%s] has x scale of 0.0\n",
                  modelName);
    }
    if (scale[1] == 0.0f) {
        Com_Error(ERR_DROP,
                  "\x15"
                  "Static model [%s] has y scale of 0.0\n",
                  modelName);
    }
    if (scale[2] == 0.0f) {
        Com_Error(ERR_DROP,
                  "\x15"
                  "Static model [%s] has z scale of 0.0\n",
                  modelName);
    }

    areaLink->model = XModelPrecache(modelName, XMODEL_LOAD_SURFACES, CM_Hunk_AllocXModelMesh, CM_Hunk_AllocXModel);
    areaLink->origin[0] = origin[0];
    areaLink->origin[1] = origin[1];
    areaLink->origin[2] = origin[2];

    axis_t scaledAxis;
    AnglesToAxis(angles, scaledAxis);
    for (int32_t row = 0; row < CM_STATIC_MODEL_AXIS_COUNT; ++row) {
        for (int32_t column = 0; column < CM_STATIC_MODEL_AXIS_COUNT; ++column) {
            scaledAxis[row][column] = (float)((long double)scaledAxis[row][column] * (long double)scale[row]);
        }
    }

    MatrixInverse(scaledAxis, areaLink->inverseAxis);
    if (XModelGetStaticBounds(areaLink->model, scaledAxis, areaLink->linkMins, areaLink->linkMaxs) == qfalse) {
        return;
    }

    for (int32_t axis = 0; axis < CM_STATIC_MODEL_AXIS_COUNT; ++axis) {
        areaLink->linkMins[axis] = (float)((long double)areaLink->linkMins[axis] + (long double)origin[axis]);
        areaLink->linkMaxs[axis] = (float)((long double)areaLink->linkMaxs[axis] + (long double)origin[axis]);
    }

    if (XModelGetContents(areaLink->model) != 0)
        CM_LinkStaticModel(areaLink);
}

/* Source: CoDUOMP.exe 0x004232f0..0x00423796.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004232f0_00423797.mcode.
 * Name: exact same-module Mac symbol CM_LoadStaticModels. The first pass
 * counts misc_model entities, then the second parses the same entity string
 * into the exactly sized allocation. The source-only safety adaptation
 * classifies each key before Com_Parse reuses its token buffer and bounds the
 * only token retained beyond the current parse step: the model name. */
void CM_LoadStaticModels(void)
{
    char *parseData = cm_entityString;
    cm_staticModelCount = 0;
    cm_staticModels = NULL;

    for (;;) {
        const char *token = Com_Parse(&parseData);
        if (token[0] == '\0' || token[0] != '{') {
            break;
        }

        qboolean isMiscModel = qfalse;
        for (;;) {
            token = Com_Parse(&parseData);
            if (token[0] == '\0' || token[0] == '}') {
                break;
            }

            /* NOT_FROM_ORIGINAL_SOURCE: this pass observes only classname;
             * preserve its classification before the shared token is reused. */
            const qboolean isClassnameKey = coduo_crt_stricmp(token, "classname") == 0 ? qtrue : qfalse;
            token = Com_Parse(&parseData);
            if (token[0] == '\0')
                break;

            if (isClassnameKey != qfalse && coduo_crt_stricmp(token, "misc_model") == 0) {
                isMiscModel = qtrue;
            }
        }

        if (isMiscModel != qfalse)
            cm_staticModelCount++;
    }

    if (cm_staticModelCount == 0)
        return;

    cm_staticModels = Hunk_AllocAlignInternal((size_t)cm_staticModelCount * sizeof(cm_staticModels[0]), CM_STATIC_MODEL_HUNK_ALIGNMENT);
    parseData = cm_entityString;

    int32_t staticModelIndex = 0;
    for (;;) {
        const char *token = Com_Parse(&parseData);
        if (token[0] == '\0' || token[0] != '{') {
            break;
        }

        vec3_t origin = {0.0f, 0.0f, 0.0f};
        vec3_t angles = {0.0f, 0.0f, 0.0f};
        vec3_t scale = {1.0f, 1.0f, 1.0f};
        char modelName[CM_STATIC_MODEL_NAME_BUFFER_SIZE];
        modelName[0] = '\0';
        qboolean isMiscModel = qfalse;

        for (;;) {
            token = Com_Parse(&parseData);
            if (token[0] == '\0' || token[0] == '}') {
                break;
            }

            /* NOT_FROM_ORIGINAL_SOURCE: classify the key before the shared
             * parse token is reused, then consume its value directly. */
            cmStaticModelEntityField_t field = CM_STATIC_MODEL_FIELD_OTHER;
            if (coduo_crt_stricmp(token, "classname") == 0) {
                field = CM_STATIC_MODEL_FIELD_CLASSNAME;
            } else if (coduo_crt_stricmp(token, "model") == 0) {
                field = CM_STATIC_MODEL_FIELD_MODEL;
            } else if (coduo_crt_stricmp(token, "origin") == 0) {
                field = CM_STATIC_MODEL_FIELD_ORIGIN;
            } else if (coduo_crt_stricmp(token, "angles") == 0) {
                field = CM_STATIC_MODEL_FIELD_ANGLES;
            } else if (coduo_crt_stricmp(token, "modelscale_vec") == 0) {
                field = CM_STATIC_MODEL_FIELD_SCALE_VECTOR;
            } else if (coduo_crt_stricmp(token, "modelscale") == 0) {
                field = CM_STATIC_MODEL_FIELD_SCALE;
            }
            token = Com_Parse(&parseData);
            if (token[0] == '\0')
                break;

            if (field == CM_STATIC_MODEL_FIELD_CLASSNAME) {
                if (coduo_crt_stricmp(token, "misc_model") == 0) {
                    isMiscModel = qtrue;
                }
            } else if (field == CM_STATIC_MODEL_FIELD_MODEL) {
                /* NOT_FROM_ORIGINAL_SOURCE: this is the only parsed token
                 * retained in a fixed-size field. */
                if (strlen(token) >= sizeof(modelName)) {
                    Com_Error(ERR_DROP, "\x15"
                                        "CM_LoadStaticModels: model name exceeds MAX_QPATH");
                }
                strcpy(modelName, token);
            } else if (field == CM_STATIC_MODEL_FIELD_ORIGIN) {
                (void)sscanf(token, "%f %f %f", &origin[0], &origin[1], &origin[2]);
            } else if (field == CM_STATIC_MODEL_FIELD_ANGLES) {
                (void)sscanf(token, "%f %f %f", &angles[0], &angles[1], &angles[2]);
            } else if (field == CM_STATIC_MODEL_FIELD_SCALE_VECTOR) {
                (void)sscanf(token, "%f %f %f", &scale[0], &scale[1], &scale[2]);
            } else if (field == CM_STATIC_MODEL_FIELD_SCALE) {
                scale[0] = (float)atof(token);
                scale[1] = scale[0];
                scale[2] = scale[0];
            }
        }

        if (isMiscModel != qfalse) {
            /* NOT_FROM_ORIGINAL_SOURCE: require the complete model prefix
             * before advancing to the asset name. */
            if (strlen(modelName) < CM_STATIC_MODEL_XMODEL_PREFIX_LENGTH) {
                Com_Error(ERR_DROP, "\x15"
                                    "CM_LoadStaticModels: invalid misc_model name");
            }
            /* Retail accepts any seven-byte prefix; only the safe skip and
             * following name are part of this security boundary. */
            CM_CreateStaticModel(&cm_staticModels[staticModelIndex], modelName + CM_STATIC_MODEL_XMODEL_PREFIX_LENGTH, origin, angles,
                                 scale);
            staticModelIndex++;
        }
    }
}
#else
void *CM_Hunk_AllocXModelMesh(size_t size)
{
    return Hunk_AllocInternal(size);
}

void *CM_Hunk_AllocXModel(size_t size)
{
    return Hunk_AllocInternal(size);
}

void CM_CreateStaticModel(worldSectorAreaLink_t *areaLink, const char *modelName, const vec3_t origin, const vec3_t angles,
                          const vec3_t scale)
{
    if (modelName == NULL || modelName[0] == '\0') {
        Com_Error(1,
                  "\x15"
                  "Invalid static model name @ %f %f %f\n",
                  (double)origin[0], (double)origin[1], (double)origin[2]);
    }
    if (scale[0] == 0.0f) {
        Com_Error(1,
                  "\x15"
                  "Static model [%s] has x scale of 0.0\n",
                  modelName);
    }
    if (scale[1] == 0.0f) {
        Com_Error(1,
                  "\x15"
                  "Static model [%s] has y scale of 0.0\n",
                  modelName);
    }
    if (scale[2] == 0.0f) {
        Com_Error(1,
                  "\x15"
                  "Static model [%s] has z scale of 0.0\n",
                  modelName);
    }

    areaLink->model = XModelPrecache(modelName, XMODEL_LOAD_SURFACES, CM_Hunk_AllocXModelMesh, CM_Hunk_AllocXModel);
    areaLink->origin[0] = origin[0];
    areaLink->origin[1] = origin[1];
    areaLink->origin[2] = origin[2];

    axis_t axis;
    AnglesToAxis(angles, axis);

    vec3_t scaledAxis[3];
    for (int32_t row = 0; row < CM_STATIC_MODEL_AXIS_COUNT; ++row) {
        for (int32_t column = 0; column < CM_STATIC_MODEL_AXIS_COUNT; ++column) {
            scaledAxis[row][column] = axis[row][column] * scale[row];
        }
    }

    MatrixInverse(scaledAxis, areaLink->inverseAxis);

    if (XModelGetStaticBounds(areaLink->model, scaledAxis, areaLink->linkMins, areaLink->linkMaxs) != qfalse) {
        for (int32_t axisIndex = 0; axisIndex < CM_STATIC_MODEL_AXIS_COUNT; ++axisIndex) {
            areaLink->linkMins[axisIndex] += origin[axisIndex];
            areaLink->linkMaxs[axisIndex] += origin[axisIndex];
        }

        if (XModelGetContents(areaLink->model) != 0) {
            CM_LinkStaticModel(areaLink);
        }
    }
}

void CM_LoadStaticModels(void)
{
    char *parseData = cm_entityString;
    cm_staticModelCount = 0;
    cm_staticModels = NULL;

    for (;;) {
        const char *token = Com_Parse(&parseData);
        if (token[0] == '\0' || token[0] != '{') {
            break;
        }

        qboolean isMiscModel = qfalse;
        while ((token = Com_Parse(&parseData))[0] != '\0' && token[0] != '}') {
            /* NOT_FROM_ORIGINAL_SOURCE: this pass observes only classname;
             * preserve its classification before the shared token is reused. */
            const qboolean isClassnameKey = strcasecmp(token, "classname") == 0 ? qtrue : qfalse;
            token = Com_Parse(&parseData);
            if (token[0] == '\0') {
                break;
            }

            if (isClassnameKey != qfalse && strcasecmp(token, "misc_model") == 0) {
                isMiscModel = qtrue;
            }
        }

        if (isMiscModel != qfalse) {
            cm_staticModelCount++;
        }
    }

    if (cm_staticModelCount == 0) {
        return;
    }

    cm_staticModels = Hunk_AllocInternal((size_t)cm_staticModelCount * sizeof(cm_staticModels[0]));
    parseData = cm_entityString;

    int32_t staticModelIndex = 0;
    for (;;) {
        const char *open = Com_Parse(&parseData);
        if (open[0] == '\0' || open[0] != '{') {
            break;
        }

        vec3_t origin = {0.0f, 0.0f, 0.0f};
        vec3_t angles = {0.0f, 0.0f, 0.0f};
        vec3_t scale = {1.0f, 1.0f, 1.0f};
        char modelName[CM_STATIC_MODEL_NAME_BUFFER_SIZE];
        qboolean isMiscModel = qfalse;

        modelName[0] = '\0';

        const char *token;
        while ((token = Com_Parse(&parseData))[0] != '\0' && token[0] != '}') {
            /* NOT_FROM_ORIGINAL_SOURCE: classify the key before the shared
             * parse token is reused, then consume its value directly. */
            cmStaticModelEntityField_t field = CM_STATIC_MODEL_FIELD_OTHER;
            if (strcasecmp(token, "classname") == 0) {
                field = CM_STATIC_MODEL_FIELD_CLASSNAME;
            } else if (strcasecmp(token, "model") == 0) {
                field = CM_STATIC_MODEL_FIELD_MODEL;
            } else if (strcasecmp(token, "origin") == 0) {
                field = CM_STATIC_MODEL_FIELD_ORIGIN;
            } else if (strcasecmp(token, "angles") == 0) {
                field = CM_STATIC_MODEL_FIELD_ANGLES;
            } else if (strcasecmp(token, "modelscale_vec") == 0) {
                field = CM_STATIC_MODEL_FIELD_SCALE_VECTOR;
            } else if (strcasecmp(token, "modelscale") == 0) {
                field = CM_STATIC_MODEL_FIELD_SCALE;
            }
            token = Com_Parse(&parseData);
            if (token[0] == '\0') {
                break;
            }

            if (field == CM_STATIC_MODEL_FIELD_CLASSNAME) {
                if (strcasecmp(token, "misc_model") == 0) {
                    isMiscModel = qtrue;
                }
            } else if (field == CM_STATIC_MODEL_FIELD_MODEL) {
                /* NOT_FROM_ORIGINAL_SOURCE: this is the only parsed token
                 * retained in a fixed-size field. */
                if (strlen(token) >= sizeof(modelName)) {
                    Com_Error(ERR_DROP, "\x15"
                                        "CM_LoadStaticModels: model name exceeds MAX_QPATH");
                }
                strcpy(modelName, token);
            } else if (field == CM_STATIC_MODEL_FIELD_ORIGIN) {
                sscanf(token, "%f %f %f", &origin[0], &origin[1], &origin[2]);
            } else if (field == CM_STATIC_MODEL_FIELD_ANGLES) {
                sscanf(token, "%f %f %f", &angles[0], &angles[1], &angles[2]);
            } else if (field == CM_STATIC_MODEL_FIELD_SCALE_VECTOR) {
                sscanf(token, "%f %f %f", &scale[0], &scale[1], &scale[2]);
            } else if (field == CM_STATIC_MODEL_FIELD_SCALE) {
                scale[0] = (float)atof(token);
                scale[1] = scale[0];
                scale[2] = scale[0];
            }
        }

        if (isMiscModel != qfalse) {
            /* NOT_FROM_ORIGINAL_SOURCE: require the complete model prefix
             * before advancing to the asset name. */
            if (strlen(modelName) < CM_STATIC_MODEL_XMODEL_PREFIX_LENGTH) {
                Com_Error(ERR_DROP, "\x15"
                                    "CM_LoadStaticModels: invalid misc_model name");
            }
            /* Retail accepts any seven-byte prefix; only the safe skip and
             * following name are part of this security boundary. */
            CM_CreateStaticModel(&cm_staticModels[staticModelIndex], modelName + CM_STATIC_MODEL_XMODEL_PREFIX_LENGTH, origin, angles,
                                 scale);
            staticModelIndex++;
        }
    }
}
#endif
