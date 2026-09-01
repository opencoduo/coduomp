/*
 * Source reconstruction for HUD element lifecycle helpers.
 *
 * Binary SHA-256:
 * 4d2391c10e234559ace294fb69d96741cad522653c361f13f626f4dc1891acb7
 */

#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "compat/coduo_x87emu.h" /* defines EMULATE_X87; x87 shim when it is 1 */
#include "recovered_game.h"
#include "game_globals.h"
#include "scr_vm.h"
#include "qcommon/com_sprintf.h"
#include "qcommon/q_string.h"

typedef struct hudelem_field_s hudelem_field_t;
typedef void (*hudelem_field_callback_t)(game_hudElem_t *elem,
                                          int fieldIndex);
typedef void (*hudelem_method_callback_t)(int elemIndex);

struct hudelem_field_s {
    const char *name;
    size_t offset;
    int32_t type;
    hudelem_field_callback_t setter;
    hudelem_field_callback_t getter;
};

typedef struct hudelem_method_s {
    const char *name;
    hudelem_method_callback_t callback;
} hudelem_method_t;

#define HUDELEM_COUNT 2048
#define HUDELEM_GLOBAL_CLIENT ENTITYNUM_NONE
#define HUDELEM_UPDATE_ARCHIVED 1u
#define HUDELEM_UPDATE_CURRENT 2u
#define HUDELEM_CLIENT_SLOT_LIMIT 64
#define HUDELEM_WORDS 34
#define HUDELEM_BYTES (HUDELEM_WORDS * (int)sizeof(uint32_t))
#define HUDELEM_FIELD_ERROR_BUFFER_SIZE 2048
#define HUDELEM_SECONDS_TO_MILLISECONDS 1000.0f
#define HUDELEM_MILLISECONDS_TO_SECONDS 0.001f

int G_LocalizedStringIndex(const char *value);
int G_ShaderIndex(const char *name);

void HudElem_SetFontScale(game_hudElem_t *elem, int fieldIndex); /* 0x520ed */
void HudElem_SetFont(game_hudElem_t *elem, int fieldIndex);      /* 0x5214c */
void HudElem_GetFont(game_hudElem_t *elem, int fieldIndex);      /* 0x5219e */
void HudElem_SetAlignX(game_hudElem_t *elem, int fieldIndex);    /* 0x521f0 */
void HudElem_GetAlignX(game_hudElem_t *elem, int fieldIndex);    /* 0x52242 */
void HudElem_SetAlignY(game_hudElem_t *elem, int fieldIndex);    /* 0x52294 */
void HudElem_GetAlignY(game_hudElem_t *elem, int fieldIndex);    /* 0x522e6 */
void HudElem_SetColor(game_hudElem_t *elem, int fieldIndex);     /* 0x51e2f */
void HudElem_GetColor(game_hudElem_t *elem, int fieldIndex);     /* 0x51fad */
void HudElem_SetAlpha(game_hudElem_t *elem, int fieldIndex);     /* 0x5201e */
void HudElem_GetAlpha(game_hudElem_t *elem, int fieldIndex);     /* 0x520b6 */
void HudElem_SetLocalizedString(game_hudElem_t *elem, int fieldIndex);     /* 0x51d81 */
void HudElem_SetBoolean(game_hudElem_t *elem, int fieldIndex);  /* 0x51ddc */

game_hudElem_t g_hudelems[HUDELEM_COUNT];

static const hudelem_field_t hudelemFields[] = {
    {"x", offsetof(game_hudElem_t, client.x), SCRIPT_SPAWN_FIELD_INT, 0, 0},
    {"y", offsetof(game_hudElem_t, client.y), SCRIPT_SPAWN_FIELD_INT, 0, 0},
    {"fontscale", offsetof(game_hudElem_t, client.fontScale), SCRIPT_SPAWN_FIELD_FLOAT,
     HudElem_SetFontScale, 0},
    {"font", offsetof(game_hudElem_t, client.font), SCRIPT_SPAWN_FIELD_INT,
     HudElem_SetFont, HudElem_GetFont},
    {"alignx", offsetof(game_hudElem_t, client.alignX), SCRIPT_SPAWN_FIELD_INT,
     HudElem_SetAlignX, HudElem_GetAlignX},
    {"aligny", offsetof(game_hudElem_t, client.alignY), SCRIPT_SPAWN_FIELD_INT,
     HudElem_SetAlignY, HudElem_GetAlignY},
    {"color", offsetof(game_hudElem_t, client.color), SCRIPT_SPAWN_FIELD_INT,
     HudElem_SetColor, HudElem_GetColor},
    {"alpha", offsetof(game_hudElem_t, client.color), SCRIPT_SPAWN_FIELD_INT,
     HudElem_SetAlpha, HudElem_GetAlpha},
    {"label", offsetof(game_hudElem_t, client.label), SCRIPT_SPAWN_FIELD_INT,
     HudElem_SetLocalizedString, 0},
    {"sort", offsetof(game_hudElem_t, client.sortKey), SCRIPT_SPAWN_FIELD_FLOAT, 0, 0},
    {"archived", offsetof(game_hudElem_t, archived), SCRIPT_SPAWN_FIELD_INT,
     HudElem_SetBoolean, 0},
};

static const char *const hudelemFontNames[] = {
    "default",
    "bigfixed",
    "smallfixed",
};

static const char *const hudelemHorizontalAlignNames[] = {
    "left",
    "center",
    "right",
};

static const char *const hudelemVerticalAlignNames[] = {
    "top",
    "middle",
    "bottom",
};

/* NOT_FROM_ORIGINAL_SOURCE: helper for HudElem_UpdateClient slot-count copy logic. */
static void game_compat_hud_elem_copy_to_client_slot(
    hudElem_t *destBase, int *slotCount, const game_hudElem_t *elem)
{
    int slot = *slotCount;

    *slotCount = slot + 1;
    if (*slotCount < HUDELEM_CLIENT_SLOT_LIMIT) {
        memcpy(&destBase[slot], &elem->client, sizeof(destBase[slot]));
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: factored color/alpha float-to-byte clamp. */
static uint8_t game_compat_hud_elem_color_byte_from_float(float value)
{
    if (value > 1.0f) {
        value = 255.0f;
    } else if (value >= 0.0f) {
        value *= 255.0f;
    } else {
        value = 0.0f;
    }

    /* Stock helper 0x53335..0x53372 adds 0.5f and truncates the still-live
     * x87 sum. The helper argument itself is already rounded to float. */
#if EMULATE_X87
    return (uint8_t)x87f_store_i32_trunc(
        x87f_add(x87f_load_f32(value), x87f_load_f32(0.5f)));
#else
    return (uint8_t)game_compat_int32_from_long_double_trunc(
        (long double)value + 0.5L);
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: factored ceil helper used by timer/clock methods. */
static int game_compat_hud_elem_ceil_milliseconds(float value)
{
    /* 0x53373: the original static rounds via the CRT ceil() on the
     * widened double, then truncates; no float32/int round-trip. */
    return game_compat_int32_from_long_double_trunc(
        (long double)ceil((double)value));
}

/* NOT_FROM_ORIGINAL_SOURCE: factored round helper used by transition methods. */
static int game_compat_hud_elem_round_milliseconds(float value)
{
    /* Same stock helper at 0x53335: no binary32 spill between faddp and
     * truncating fistp. */
#if EMULATE_X87
    return x87f_store_i32_trunc(
        x87f_add(x87f_load_f32(value), x87f_load_f32(0.5f)));
#else
    return game_compat_int32_from_long_double_trunc(
        (long double)value + 0.5L);
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: shared fade/scale/move time validation. */
static void game_compat_hud_elem_validate_transition_time(float value, const char *tooLow,
                                           const char *tooHigh)
{
    if (value <= 0.0f) {
        Scr_ParamError(0, va(tooLow, (double)value));
    } else if (value > 60.0f) {
        Scr_ParamError(0, va(tooHigh, (double)value));
    }
}

/* VERIFIED_DECOMPILER(0x51908, 61908_FUN_00061908.c, VERIFY-HUDELEM-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void HudElem_ClearTypeSettings(game_hudElem_t *elem)
{
    elem->client.width = 0;
    elem->client.height = 0;
    elem->client.materialIndex = 0;
    elem->client.scaleFromWidth = 0;
    elem->client.scaleFromHeight = 0;
    elem->client.scaleStartTime = 0;
    elem->client.scaleTime = 0;
    elem->client.timerValue = 0;
    elem->client.rotationPeriodMs = 0;
    elem->client.value = 0;
    elem->client.text = 0;
}

/* VERIFIED_DECOMPILER(0x5197b, 6197b_FUN_0006197b.c, VERIFY-HUDELEM-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void HudElem_SetDefaults(game_hudElem_t *elem)
{
    elem->client.type = HE_TYPE_TEXT;
    elem->client.x = 0;
    elem->client.y = 0;
    elem->client.fontScale = 1.0f;
    elem->client.font = 0;
    elem->client.alignX = 0;
    elem->client.alignY = 0;
    elem->client.color.rgba = UINT32_MAX;
    elem->client.fromColor.rgba = 0;
    elem->client.fadeStartTime = 0;
    elem->client.fadeTime = 0;
    elem->client.label = 0;
    elem->client.sortKey = 0;
    elem->archived = qtrue;
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    elem->client.moveFromX = 0;
    elem->client.moveFromY = 0;
    elem->client.moveStartTime = 0;
    elem->client.moveTime = 0;
    HudElem_ClearTypeSettings(elem);
}

/* VERIFIED_DECOMPILER(0x51a1c, 61a1c_HudElem_Alloc.c, VERIFY-NEXT-004-HUDELEM-2026-06-17): DATAFLOW_VERIFIED - 2048-slot scan, default initialization call, client/team stores at +0x7c/+0x80, null exhaustion return, and allocated pointer return checked against current decompiler output. */
game_hudElem_t *HudElem_Alloc(int clientNum, int team)
{
    for (int index = 0; index < HUDELEM_COUNT; index++) {
        game_hudElem_t *elem = &g_hudelems[index];

        if (elem->client.type == HE_TYPE_NONE) {
            HudElem_SetDefaults(elem);
            elem->clientNum = clientNum;
            elem->team = team;
            return elem;
        }
    }

    return 0;
}

/* VERIFIED_DECOMPILER(0x51af3, 61af3_HudElem_Free.c, VERIFY-NEXT-004-HUDELEM-2026-06-17): DATAFLOW_VERIFIED - Scr_FreeHudElem call order, type-zero store at element base, and void return checked against current decompiler output. */
void HudElem_Free(game_hudElem_t *elem)
{
    Scr_FreeHudElem(elem);
    elem->client.type = HE_TYPE_NONE;
}

/* VERIFIED_DECOMPILER(0x51b1f, 61b1f_HudElem_ClientDisconnect.c, VERIFY-NEXT-004-HUDELEM-2026-06-17): DATAFLOW_VERIFIED - 2048-slot scan, active-type guard, entity-number comparison through first gentity_t word, HudElem_Free argument, and void return checked against current decompiler output. */
void HudElem_ClientDisconnect(const gentity_t *ent)
{
    for (int index = 0; index < HUDELEM_COUNT; index++) {
        game_hudElem_t *elem = &g_hudelems[index];

        if (elem->client.type != HE_TYPE_NONE &&
            elem->clientNum == ent->s.number) {
            HudElem_Free(elem);
        }
    }
}

/* VERIFIED_DECOMPILER(0x51bad, 61bad_HudElem_DestroyAll.c, VERIFY-NEXT-004-HUDELEM-2026-06-17): DATAFLOW_VERIFIED - 2048-slot active scan, HudElem_Free argument, 0x44000-byte global memset, and void return checked against current decompiler output. */
void HudElem_DestroyAll(void)
{
    for (int index = 0; index < HUDELEM_COUNT; index++) {
        if (g_hudelems[index].client.type != HE_TYPE_NONE) {
            HudElem_Free(&g_hudelems[index]);
        }
    }

    memset(g_hudelems, 0, sizeof(g_hudelems));
}

/* VERIFIED_DECOMPILER(0x524ab, 624ab_GScr_NewHudElem.c, VERIFY-NEXT-004-HUDELEM-2026-06-17): DATAFLOW_VERIFIED - global client 0x3ff allocation, zero team argument, out-of-hudelems error path, Scr_AddHudElem argument, and void return checked against current decompiler output. */
void GScr_NewHudElem(void)
{
    game_hudElem_t *elem = HudElem_Alloc(HUDELEM_GLOBAL_CLIENT, 0);

    if (elem == 0) {
        Scr_Error("out of hudelems");
    }

    Scr_AddHudElem(elem);
}

/* VERIFIED_DECOMPILER(0x524f9, 624f9_GScr_NewClientHudElem.c, VERIFY-NEXT-004-HUDELEM-2026-06-17): DATAFLOW_VERIFIED - Scr_GetEntity(0), client pointer guard at gentity +0x160, entity-number allocation argument, zero team, error path, and Scr_AddHudElem checked against current decompiler output. */
void GScr_NewClientHudElem(void)
{
    gentity_t *ent = Scr_GetEntity(0);
    game_hudElem_t *elem;

    if (ent->client == 0) {
        Scr_ParamError(0, "not a client");
    }

    elem = HudElem_Alloc(ent->s.number, 0);
    if (elem == 0) {
        Scr_Error("out of hudelems");
    }

    Scr_AddHudElem(elem);
}

/* VERIFIED_DECOMPILER(0x52579, 62579_GScr_NewTeamHudElem.c, VERIFY-NEXT-004-HUDELEM-2026-06-17): DATAFLOW_VERIFIED - const-string comparisons, allies/axis/spectator values 2/1/3, ParamError message path with fallback team zero, global client allocation, error path, and Scr_AddHudElem checked against current decompiler output. */
void GScr_NewTeamHudElem(void)
{
    uint16_t teamName = Scr_GetConstString(0);
    int team;
    game_hudElem_t *elem;

    if (teamName == scr_const_allies) {
        team = TEAM_ALLIES;
    } else if (teamName == scr_const_axis) {
        team = TEAM_AXIS;
    } else if (teamName == scr_const_spectator) {
        team = TEAM_SPECTATOR;
    } else {
        Scr_ParamError(0,
                       va("team \"%s\" should be \"allies\", \"axis\", or \"spectator\"",
                          Scr_GetString(0)));
        team = 0;
    }

    elem = HudElem_Alloc(HUDELEM_GLOBAL_CLIENT, team);
    if (elem == 0) {
        Scr_Error("out of hudelems");
    }

    Scr_AddHudElem(elem);
}

/* VERIFIED_DECOMPILER(0x52338, 62338_Scr_GetHudElemField.c, VERIFY-HUDELEM-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void Scr_GetHudElemField(int elemIndex, int fieldIndex)
{
    game_hudElem_t *elem = &g_hudelems[elemIndex];
    const hudelem_field_t *field = &hudelemFields[fieldIndex];

    if (field->getter == 0) {
        Scr_GetGenericField(elem, field->type, field->offset);
    } else {
        field->getter(elem, fieldIndex);
    }
}

/* VERIFIED_DECOMPILER(0x523c7, 623c7_Scr_SetHudElemField.c, VERIFY-HUDELEM-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void Scr_SetHudElemField(int elemIndex, int fieldIndex)
{
    game_hudElem_t *elem = &g_hudelems[elemIndex];
    const hudelem_field_t *field = &hudelemFields[fieldIndex];

    if (field->setter == 0) {
        Scr_SetGenericField(elem, field->type, field->offset);
    } else {
        field->setter(elem, fieldIndex);
    }
}

/* VERIFIED_DECOMPILER(0x52456, 62456_Scr_FreeHudElemConstStrings.c, VERIFY-HUDELEM-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void Scr_FreeHudElemConstStrings(game_hudElem_t *elem)
{
    for (uint32_t index = 0; index < sizeof(hudelemFields) / sizeof(hudelemFields[0]);
         index++) {
        const hudelem_field_t *field = &hudelemFields[index];

        if (field->type == 5) {
            Scr_SetString((uint16_t *)(void *)&((uint8_t *)elem)[field->offset], 0);
        }
    }
}

/* VERIFIED_DECOMPILER(0x52659, 62659_GScr_AddFieldsForHudElems.c, VERIFY-HUDELEM-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void GScr_AddFieldsForHudElems(void)
{
    for (uint32_t index = 0; index < sizeof(hudelemFields) / sizeof(hudelemFields[0]);
         index++) {
        const hudelem_field_t *field = &hudelemFields[index];

        switch (field->type) {
        case 0:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 10:
        case 11:
            Scr_AddClassField(
                g_scr_data.classMap[SCRIPT_OBJECT_HUDELEM].classnum,
                field->name, (uint16_t)index);
            break;
        default:
            break;
        }
    }
}

/* VERIFIED_DECOMPILER(0x51c38, 61c38_FUN_00061c38.c, VERIFY-HUDELEM-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void HudElem_SetEnumString(game_hudElem_t *elem, const hudelem_field_t *field,
                           const char *const *values, int valueCount)
{
    const char *value = Scr_GetString(0);

    for (int index = 0; index < valueCount; index++) {
        if (Q_stricmp(value, values[index]) == 0) {
            *(int32_t *)(void *)&((uint8_t *)elem)[field->offset] = index;
            return;
        }
    }

    /* NOT_FROM_ORIGINAL_SOURCE: bound every stage of the invalid-enum
     * diagnostic to its fixed destination before the existing script error. */
    char message[HUDELEM_FIELD_ERROR_BUFFER_SIZE];

    Com_sprintf(message, sizeof(message),
                "\"%s\" is not a valid value for hudelem field "
                "\"%s\"\nShould be one of:",
                value, field->name);
    for (int index = 0; index < valueCount; index++) {
        Q_strcat(message, sizeof(message), " ");
        Q_strcat(message, sizeof(message), values[index]);
    }

    Scr_Error(message);
}

/* VERIFIED_DECOMPILER(0x51d43, 61d43_FUN_00061d43.c, VERIFY-HUDELEM-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void HudElem_GetEnumString(game_hudElem_t *elem, const hudelem_field_t *field,
                           const char *const *values)
{
    Scr_AddString(values[*(int32_t *)(void *)&((uint8_t *)elem)[field->offset]]);
}

/* VERIFIED_DECOMPILER(0x51d81, 61d81_FUN_00061d81.c, VERIFY-HUDELEM-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void HudElem_SetLocalizedString(game_hudElem_t *elem, int fieldIndex)
{
    const char *value = Scr_GetIString(0);
    int localizedStringIndex = G_LocalizedStringIndex(value);

    *(int32_t *)(void *)&((uint8_t *)elem)[hudelemFields[fieldIndex].offset] =
        localizedStringIndex;
}

/* VERIFIED_DECOMPILER(0x51ddc, 61ddc_FUN_00061ddc.c, VERIFY-HUDELEM-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void HudElem_SetBoolean(game_hudElem_t *elem, int fieldIndex)
{
    qboolean archived = Scr_GetBool(0);

    *(qboolean *)(void *)&((uint8_t *)elem)[hudelemFields[fieldIndex].offset] =
        archived;
}

/* VERIFIED_DECOMPILER(0x526f0, 626f0_script_method_hudelem_settext.c, VERIFY-HUDELEM-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void HECmd_SetText(int elemIndex)
{
    game_hudElem_t *elem = &g_hudelems[elemIndex];
    const char *value = Scr_GetIString(0);

    HudElem_ClearTypeSettings(elem);
    elem->client.type = HE_TYPE_TEXT;
    elem->client.text = G_LocalizedStringIndex(value);
}

/* VERIFIED_DECOMPILER(0x52779, 62779_script_method_hudelem_setshader.c, VERIFY-HUDELEM-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void HECmd_SetShader(int elemIndex)
{
    game_hudElem_t *elem = &g_hudelems[elemIndex];
    int paramCount = Scr_GetNumParam();
    int materialIndex;
    int width;
    int height;
    float rightTexCoord = 1.0f;
    float bottomTexCoord = 1.0f;

    if (paramCount != 1 && paramCount != 3 && paramCount != 5) {
        Scr_Error(
            "USAGE: <hudelem> setShader(\"shadername\"[, optional_width, optional_height,optional_right_texcoord,optional_bottom_texcoord]);");
    }

    materialIndex = G_ShaderIndex(Scr_GetString(0));
    if (paramCount == 1) {
        width = 0;
        height = 0;
    } else {
        width = Scr_GetInt(1);
        if (width < 0) {
            Scr_ParamError(1, va("width %i < 0", width));
        }

        height = Scr_GetInt(2);
        if (height < 0) {
            Scr_ParamError(2, va("height %i < 0", height));
        }

        if (paramCount == 5) {
            rightTexCoord = Scr_GetFloat(3);
            bottomTexCoord = Scr_GetFloat(4);
        }
    }

    HudElem_ClearTypeSettings(elem);
    elem->client.type = HE_TYPE_SHADER;
    elem->client.materialIndex = materialIndex;
    elem->client.width = width;
    elem->client.height = height;
    elem->client.shaderRightTexcoord = rightTexCoord;
    elem->client.shaderBottomTexcoord = bottomTexCoord;
}

/* VERIFIED_DECOMPILER(0x528ea, 628ea_FUN_000628ea.c, VERIFY-HUDELEM-PACKET-2026-06-17): DATAFLOW_VERIFIED */
static void HECmd_SetTimer_Internal(int elemIndex, int elemType, const char *methodName)
{
    game_hudElem_t *elem = &g_hudelems[elemIndex];
    int paramCount = Scr_GetNumParam();
    int targetTime;

    if (paramCount != 1) {
        Scr_Error(va("USAGE: <hudelem> %s(time_in_seconds);\n", methodName));
    }

    targetTime =
        game_compat_hud_elem_ceil_milliseconds(Scr_GetFloat(0) *
                                 HUDELEM_SECONDS_TO_MILLISECONDS);
    if (targetTime < 1 && elemType != HE_TYPE_TIMER_UP) {
        Scr_ParamError(
            0,
            va("time %g should be > 0",
               /* 0x5296a: bare fild of targetTime, no float32 rounding. */
               (double)(targetTime * HUDELEM_MILLISECONDS_TO_SECONDS)));
    }

    HudElem_ClearTypeSettings(elem);
    elem->client.type = (hudElemType_t)elemType;
    elem->client.timerValue = coduo_int32_from_bits(
        (uint32_t)targetTime + (uint32_t)level.time);
}

/* VERIFIED_DECOMPILER(0x529c5, 629c5_FUN_000629c5.c, VERIFY-HUDELEM-PACKET-2026-06-17): DATAFLOW_VERIFIED */
static void HECmd_SetClock_Internal(int elemIndex, int elemType, const char *methodName)
{
    game_hudElem_t *elem = &g_hudelems[elemIndex];
    int paramCount = Scr_GetNumParam();
    int targetTime;
    int duration;
    int materialIndex;
    int width;
    int height;

    if (paramCount != 3 && paramCount != 5) {
        Scr_Error(va(
            "USAGE: <hudelem> %s(time_in_seconds, total_clock_time_in_seconds, shadername[, width, height]);\n",
            methodName));
    }

    targetTime =
        game_compat_hud_elem_ceil_milliseconds(Scr_GetFloat(0) *
                                 HUDELEM_SECONDS_TO_MILLISECONDS);
    if (targetTime < 1 && elemType != HE_TYPE_CLOCK_UP) {
        Scr_ParamError(
            0,
            va("time %g should be > 0",
               /* 0x52a4f: bare fild of targetTime, no float32 rounding. */
               (double)(targetTime * HUDELEM_MILLISECONDS_TO_SECONDS)));
    }

    duration =
        game_compat_hud_elem_ceil_milliseconds(Scr_GetFloat(1) *
                                 HUDELEM_SECONDS_TO_MILLISECONDS);
    if (duration < 1) {
        Scr_ParamError(
            1,
            va("duration %g should be > 0",
               /* 0x52aa1: bare fild of duration, no float32 rounding. */
               (double)(duration * HUDELEM_MILLISECONDS_TO_SECONDS)));
    }

    materialIndex = G_ShaderIndex(Scr_GetString(2));
    if (paramCount == 3) {
        width = 0;
        height = 0;
    } else {
        width = Scr_GetInt(3);
        if (width < 0) {
            Scr_ParamError(3, va("width %i < 0", width));
        }

        height = Scr_GetInt(4);
        if (height < 0) {
            Scr_ParamError(4, va("height %i < 0", height));
        }
    }

    HudElem_ClearTypeSettings(elem);
    elem->client.type = (hudElemType_t)elemType;
    elem->client.timerValue = coduo_int32_from_bits(
        (uint32_t)targetTime + (uint32_t)level.time);
    elem->client.rotationPeriodMs = duration;
    elem->client.materialIndex = materialIndex;
    elem->client.width = width;
    elem->client.height = height;
}

/* VERIFIED_DECOMPILER(0x52bc1, 62bc1_script_method_hudelem_settimer.c, VERIFY-HUDELEM-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void HECmd_SetTimer(int elemIndex)
{
    HECmd_SetTimer_Internal(elemIndex, HE_TYPE_TIMER, "setTimer");
}

/* VERIFIED_DECOMPILER(0x52bf6, 62bf6_script_method_hudelem_settimerup.c, VERIFY-HUDELEM-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void HECmd_SetTimerUp(int elemIndex)
{
    HECmd_SetTimer_Internal(elemIndex, HE_TYPE_TIMER_UP, "setTimerUp");
}

/* VERIFIED_DECOMPILER(0x52c2b, 62c2b_script_method_hudelem_settenthstimer.c, VERIFY-HUDELEM-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void HECmd_SetTenthsTimer(int elemIndex)
{
    HECmd_SetTimer_Internal(elemIndex, HE_TYPE_TENTHS_TIMER,
                         "setTenthsTimer");
}

/* VERIFIED_DECOMPILER(0x52c60, 62c60_script_method_hudelem_settenthstimerup.c, VERIFY-HUDELEM-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void HECmd_SetTenthsTimerUp(int elemIndex)
{
    HECmd_SetTimer_Internal(elemIndex, HE_TYPE_TENTHS_TIMER_UP,
                         "setTenthsTimerUp");
}

/* VERIFIED_DECOMPILER(0x52c95, 62c95_script_method_hudelem_setclock.c, VERIFY-HUDELEM-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void HECmd_SetClock(int elemIndex)
{
    HECmd_SetClock_Internal(elemIndex, HE_TYPE_CLOCK, "setClock");
}

/* VERIFIED_DECOMPILER(0x52cca, 62cca_script_method_hudelem_setclockup.c, VERIFY-HUDELEM-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void HECmd_SetClockUp(int elemIndex)
{
    HECmd_SetClock_Internal(elemIndex, HE_TYPE_CLOCK_UP, "setClockUp");
}

/* VERIFIED_DECOMPILER(0x52cff, 62cff_script_method_hudelem_setvalue.c, VERIFY-HUDELEM-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void HECmd_SetValue(int elemIndex)
{
    game_hudElem_t *elem = &g_hudelems[elemIndex];
    float value = Scr_GetFloat(0);

    HudElem_ClearTypeSettings(elem);
    elem->client.type = HE_TYPE_VALUE;
    elem->client.value = value;
}

/* VERIFIED_DECOMPILER(0x52d7e, 62d7e_script_method_hudelem_fadeovertime.c, VERIFY-HUDELEM-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void HECmd_FadeOverTime(int elemIndex)
{
    game_hudElem_t *elem = &g_hudelems[elemIndex];
    float fadeTimeSeconds = Scr_GetFloat(0);

    game_compat_hud_elem_validate_transition_time(fadeTimeSeconds, "fade time %g <= 0",
                                   "fade time %g > 60");
    elem->client.fadeStartTime = level.time;
    elem->client.fadeTime =
        game_compat_hud_elem_round_milliseconds(fadeTimeSeconds *
                                  HUDELEM_SECONDS_TO_MILLISECONDS);
    elem->client.fromColor = elem->client.color;
}

/* VERIFIED_DECOMPILER(0x52e6c, 62e6c_script_method_hudelem_scaleovertime.c, VERIFY-HUDELEM-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void HECmd_ScaleOverTime(int elemIndex)
{
    game_hudElem_t *elem = &g_hudelems[elemIndex];
    int paramCount = Scr_GetNumParam();
    float scaleTimeSeconds;
    int width;
    int height;

    if (paramCount != 3) {
        Scr_Error("hudelem scaleOverTime(time_in_seconds, new_width, new_height)");
    }

    scaleTimeSeconds = Scr_GetFloat(0);
    game_compat_hud_elem_validate_transition_time(scaleTimeSeconds, "scale time %g <= 0",
                                   "scale time %g > 60");
    width = Scr_GetInt(1);
    height = Scr_GetInt(2);

    elem->client.scaleStartTime = level.time;
    elem->client.scaleTime =
        game_compat_hud_elem_round_milliseconds(scaleTimeSeconds *
                                  HUDELEM_SECONDS_TO_MILLISECONDS);
    elem->client.scaleFromWidth = elem->client.width;
    elem->client.scaleFromHeight = elem->client.height;
    elem->client.width = width;
    elem->client.height = height;
}

/* VERIFIED_DECOMPILER(0x52fae, 62fae_script_method_hudelem_moveovertime.c, VERIFY-HUDELEM-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void HECmd_MoveOverTime(int elemIndex)
{
    game_hudElem_t *elem = &g_hudelems[elemIndex];
    float moveTimeSeconds = Scr_GetFloat(0);

    game_compat_hud_elem_validate_transition_time(moveTimeSeconds, "move time %g <= 0",
                                   "move time %g > 60");
    elem->client.moveStartTime = level.time;
    elem->client.moveTime =
        game_compat_hud_elem_round_milliseconds(moveTimeSeconds *
                                  HUDELEM_SECONDS_TO_MILLISECONDS);
    elem->client.moveFromX = elem->client.x;
    elem->client.moveFromY = elem->client.y;
}

/* VERIFIED_DECOMPILER(0x530a8, 630a8_script_method_hudelem_reset.c, VERIFY-HUDELEM-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void HECmd_Reset(int elemIndex)
{
    HudElem_SetDefaults(&g_hudelems[elemIndex]);
}

/* VERIFIED_DECOMPILER(0x530e1, 630e1_script_method_hudelem_destroy.c, VERIFY-HUDELEM-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void HECmd_Destroy(int elemIndex)
{
    game_hudElem_t *elem = &g_hudelems[elemIndex];

    if (elem->client.type == HE_TYPE_NONE) {
        Scr_Error("tried to free invalid hud element\n");
    } else {
        HudElem_Free(elem);
    }
}

static const hudelem_method_t hudelemMethods[] = {
    {"settext", HECmd_SetText},
    {"setshader", HECmd_SetShader},
    {"settimer", HECmd_SetTimer},
    {"settimerup", HECmd_SetTimerUp},
    {"settenthstimer", HECmd_SetTenthsTimer},
    {"settenthstimerup", HECmd_SetTenthsTimerUp},
    {"setclock", HECmd_SetClock},
    {"setclockup", HECmd_SetClockUp},
    {"setvalue", HECmd_SetValue},
    {"fadeovertime", HECmd_FadeOverTime},
    {"scaleovertime", HECmd_ScaleOverTime},
    {"moveovertime", HECmd_MoveOverTime},
    {"reset", HECmd_Reset},
    {"destroy", HECmd_Destroy},
};

/* VERIFIED_DECOMPILER(0x53138, 63138_HudElem_GetMethod.c, VERIFY-HUDELEM-PACKET-2026-06-17): DATAFLOW_VERIFIED */
hudelem_method_callback_t HudElem_GetMethod(const char **name)
{
    for (uint32_t index = 0; index < sizeof(hudelemMethods) / sizeof(hudelemMethods[0]);
         index++) {
        if (strcmp(*name, hudelemMethods[index].name) == 0) {
            *name = hudelemMethods[index].name;
            return hudelemMethods[index].callback;
        }
    }

    return 0;
}

/* VERIFIED_DECOMPILER(0x531b3, 631b3_HudElem_UpdateClient.c, VERIFY-HUDELEM-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void HudElem_UpdateClient(gclient_t *client, int clientNum, uint32_t updateFlags)
{
    hudElem_t *archivedHudElems = client->ps.hudArchival;
    hudElem_t *currentHudElems = client->ps.hudCurrent;
    int archivedCount = 0;
    int currentCount = 0;

    if ((updateFlags & HUDELEM_UPDATE_ARCHIVED) != 0) {
        memset(archivedHudElems, 0, sizeof(client->ps.hudArchival));
    }
    if ((updateFlags & HUDELEM_UPDATE_CURRENT) != 0) {
        memset(currentHudElems, 0, sizeof(client->ps.hudCurrent));
    }

    for (uint32_t index = 0; index < HUDELEM_COUNT; index++) {
        const game_hudElem_t *elem = &g_hudelems[index];

        if (elem->client.type == HE_TYPE_NONE) {
            continue;
        }
        if (elem->team != 0 && elem->team != client->sessionTeam) {
            continue;
        }
        if (elem->clientNum != HUDELEM_GLOBAL_CLIENT && elem->clientNum != clientNum) {
            continue;
        }

        if (elem->archived == 0) {
            if ((updateFlags & HUDELEM_UPDATE_CURRENT) != 0) {
                game_compat_hud_elem_copy_to_client_slot(currentHudElems, &currentCount, elem);
            }
        } else if ((updateFlags & HUDELEM_UPDATE_ARCHIVED) != 0) {
            game_compat_hud_elem_copy_to_client_slot(archivedHudElems, &archivedCount, elem);
        }
    }
}

/* VERIFIED_DECOMPILER(0x51e2f, 61e2f_FUN_00061e2f.c, VERIFY-HUDELEM-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void HudElem_SetColor(game_hudElem_t *elem, int fieldIndex)
{
    vec3_t color;
    uint8_t *packedColor = (uint8_t *)(void *)&elem->client.color;

    (void)fieldIndex;

    Scr_GetVector(0, color);
    packedColor[0] = game_compat_hud_elem_color_byte_from_float(color[0]);
    packedColor[1] = game_compat_hud_elem_color_byte_from_float(color[1]);
    packedColor[2] = game_compat_hud_elem_color_byte_from_float(color[2]);
}

/* VERIFIED_DECOMPILER(0x51fad, 61fad_FUN_00061fad.c, VERIFY-HUDELEM-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void HudElem_GetColor(game_hudElem_t *elem, int fieldIndex)
{
    const uint8_t *packedColor =
        (const uint8_t *)(const void *)&elem->client.color;
    vec3_t color;

    (void)fieldIndex;

    /* 0x51fc7/0x51fe1/0x51ffb: bare fild of the color bytes, no float32
     * rounding of the ints. */
    color[0] = packedColor[0] * (1.0f / 255.0f);
    color[1] = packedColor[1] * (1.0f / 255.0f);
    color[2] = packedColor[2] * (1.0f / 255.0f);
    Scr_AddVector(color);
}

/* VERIFIED_DECOMPILER(0x5201e, 6201e_FUN_0006201e.c, VERIFY-HUDELEM-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void HudElem_SetAlpha(game_hudElem_t *elem, int fieldIndex)
{
    uint8_t *packedColor = (uint8_t *)(void *)&elem->client.color;

    (void)fieldIndex;

    packedColor[3] = game_compat_hud_elem_color_byte_from_float(Scr_GetFloat(0));
}

/* VERIFIED_DECOMPILER(0x520b6, 620b6_FUN_000620b6.c, VERIFY-HUDELEM-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void HudElem_GetAlpha(game_hudElem_t *elem, int fieldIndex)
{
    const uint8_t *packedColor =
        (const uint8_t *)(const void *)&elem->client.color;

    (void)fieldIndex;

    /* 0x520d0: bare fild of the alpha byte, no float32 rounding. */
    Scr_AddFloat(packedColor[3] * (1.0f / 255.0f));
}

/* VERIFIED_DECOMPILER(0x520ed, 620ed_FUN_000620ed.c, VERIFY-HUDELEM-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void HudElem_SetFontScale(game_hudElem_t *elem, int fieldIndex)
{
    float fontScale = Scr_GetFloat(0);

    (void)fieldIndex;

    if (fontScale <= 0.0f) {
        Scr_Error(va("font scale was %g; should be > 0", (double)fontScale));
    }

    elem->client.fontScale = fontScale;
}

/* VERIFIED_DECOMPILER(0x5214c, 6214c_FUN_0006214c.c, VERIFY-HUDELEM-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void HudElem_SetFont(game_hudElem_t *elem, int fieldIndex)
{
    HudElem_SetEnumString(elem, &hudelemFields[fieldIndex], hudelemFontNames, 3);
}

/* VERIFIED_DECOMPILER(0x5219e, 6219e_FUN_0006219e.c, VERIFY-HUDELEM-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void HudElem_GetFont(game_hudElem_t *elem, int fieldIndex)
{
    HudElem_GetEnumString(elem, &hudelemFields[fieldIndex], hudelemFontNames);
}

/* VERIFIED_DECOMPILER(0x521f0, 621f0_FUN_000621f0.c, VERIFY-HUDELEM-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void HudElem_SetAlignX(game_hudElem_t *elem, int fieldIndex)
{
    HudElem_SetEnumString(elem, &hudelemFields[fieldIndex], hudelemHorizontalAlignNames,
                          3);
}

/* VERIFIED_DECOMPILER(0x52242, 62242_FUN_00062242.c, VERIFY-HUDELEM-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void HudElem_GetAlignX(game_hudElem_t *elem, int fieldIndex)
{
    HudElem_GetEnumString(elem, &hudelemFields[fieldIndex], hudelemHorizontalAlignNames);
}

/* VERIFIED_DECOMPILER(0x52294, 62294_FUN_00062294.c, VERIFY-HUDELEM-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void HudElem_SetAlignY(game_hudElem_t *elem, int fieldIndex)
{
    HudElem_SetEnumString(elem, &hudelemFields[fieldIndex], hudelemVerticalAlignNames, 3);
}

/* VERIFIED_DECOMPILER(0x522e6, 622e6_FUN_000622e6.c, VERIFY-HUDELEM-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void HudElem_GetAlignY(game_hudElem_t *elem, int fieldIndex)
{
    HudElem_GetEnumString(elem, &hudelemFields[fieldIndex], hudelemVerticalAlignNames);
}
