#include "msg_delta.h"

#include "compat/coduo_fp_conversion.h"
#include "compat/coduo_int32_bits.h"
#include "compat/coduo_x87emu.h"
#include "q_cvar.h"
#include "q_string.h"

#include <string.h>

void Com_Printf(const char *format, ...);
void Com_Error(errorParm_t code, const char *format, ...);
extern cvar_t *cl_shownet;

/*
 * Complete delta-message codec shared by the Windows client/listen server and
 * Linux dedicated server.  The descriptor tables below have identical field
 * order and wire modes in both retained binaries; their offsets resolve to the
 * same shared entity, archived-entity, client, player-state, objective, and
 * HUD layouts.  Function ranges are CoDUOMP.exe 0x00449de0..0x0044cf71 and
 * coduo_lnxded 0x08080b1d..0x08083884.
 *
 * The only result-affecting platform split is the established x87 conversion
 * width: Windows consumes the low dword of `_ftol2`'s signed-qword result,
 * while Linux uses a direct signed-dword FISTP.  EMULATE_X87 remains an
 * independent arithmetic implementation for either behavior selection.
 */

enum {
    MSG_USERCMD_STANCE_UPMOVE = -127,
    MSG_SMALL_MOVE_LIMIT = 10,
    MSG_SMALL_MOVE_POSITIVE = 127,
    MSG_SMALL_MOVE_NEGATIVE = -127,
    MSG_MOVE_POSITIVE_FLAG = 0x01,
    MSG_MOVE_NEGATIVE_FLAG = 0x02,
    MSG_HOR_MOVE_RIGHT_POSITIVE = 0x04,
    MSG_HOR_MOVE_RIGHT_NEGATIVE = 0x08,
    MSG_USERCMD_TIME_DELTA_LIMIT = 256,
    MSG_USERCMD_CONTROL_BITS = 1,
    MSG_USERCMD_HORIZONTAL_MOVE_BITS = 4,
    MSG_USERCMD_VERTICAL_MOVE_BITS = 2,
    MSG_USERCMD_BUTTON_HIGH_BITS = 6,
    MSG_USERCMD_WEAPON_BITS = 7,
    MSG_USERCMD_FIRE_BUTTON = 0x01,
    MSG_DELTA_DEFAULT_FLOAT_BITS = 0,
    MSG_DELTA_ANGLE16_BITS = -100,
    MSG_DELTA_SMALL_INT_BIAS = 4096,
    MSG_DELTA_SMALL_INT_RANGE = 8192,
    MSG_DELTA_SMALL_INT_LOW_BITS = 5,
    MSG_DELTA_BYTE_BITS = 8,
    MSG_DELTA_BYTE_BIT_MASK = 7,
    MSG_SHOWNET_DELTA_STRUCT = 2,
    MSG_SHOWNET_ALL = -1,
    MSG_ENTITY_NUMBER_BITS = 10,
    MSG_CLIENT_NUMBER_BITS = 6,
    MSG_HUD_ELEM_COUNT_BITS = 6,
    MSG_HUD_ELEM_LAST_FIELD_BITS = 5,
    MSG_PLAYERSTATE_STAT_BITS = 6,
    MSG_PLAYERSTATE_STAT_COMPACT_INDEX = 3,
    MSG_PLAYERSTATE_STAT_COMPACT_BITS = 6,
    MSG_PLAYERSTATE_STAT_BYTE_INDEX = PLAYERSTATE_STAT_COUNT - 1,
    MSG_PLAYERSTATE_AMMO_GROUP_COUNT = 4,
    MSG_PLAYERSTATE_AMMO_GROUP_SIZE = 16,
    MSG_PLAYERSTATE_VEHICLE_POSITION_COUNT = 7,
    MSG_OBJECTIVE_STATE_BITS = 3,
    MSG_SHOWNET_PLAYERSTATE = -2,
    MSG_SHOWNET_VERBOSE = 4
};

#define MSG_ANGLE_TO_SHORT_SCALE 182.04444885253906f
#define MSG_ANGLE_TO_SHORT_NEGATIVE_SCALE (-MSG_ANGLE_TO_SHORT_SCALE)

/* Source: CoDUOMP.exe 0x005c5698 (.data). Indexed by the keyed message
 * readers with a proven 0..32 bit count. */
static const uint32_t msg_bitmaskTable[MSG_BITMASK_TABLE_COUNT] = {
    UINT32_C(0x00000000), UINT32_C(0x00000001), UINT32_C(0x00000003), UINT32_C(0x00000007), UINT32_C(0x0000000f), UINT32_C(0x0000001f),
    UINT32_C(0x0000003f), UINT32_C(0x0000007f), UINT32_C(0x000000ff), UINT32_C(0x000001ff), UINT32_C(0x000003ff), UINT32_C(0x000007ff),
    UINT32_C(0x00000fff), UINT32_C(0x00001fff), UINT32_C(0x00003fff), UINT32_C(0x00007fff), UINT32_C(0x0000ffff), UINT32_C(0x0001ffff),
    UINT32_C(0x0003ffff), UINT32_C(0x0007ffff), UINT32_C(0x000fffff), UINT32_C(0x001fffff), UINT32_C(0x003fffff), UINT32_C(0x007fffff),
    UINT32_C(0x00ffffff), UINT32_C(0x01ffffff), UINT32_C(0x03ffffff), UINT32_C(0x07ffffff), UINT32_C(0x0fffffff), UINT32_C(0x1fffffff),
    UINT32_C(0x3fffffff), UINT32_C(0x7fffffff), UINT32_C(0xffffffff)};

#define MSG_ENTITY_NETFIELD(name_, member_, bits_) {(name_), (int32_t)offsetof(entityState_t, member_), (bits_)}

/* Source: CoDUOMP.exe 0x0058ec68..0x0058ef37 (.rdata). Each original
 * descriptor is {string VA, entityState_t byte offset, wire bit mode}.
 * Native source pointers widen, while offsetof keeps the described record
 * member explicit and portable.
 * PE_RELOCATION_VALUES_VERIFIED: verify_relocated_initializers.py follows all
 * 60 original string pointers and proves their ordered source targets. */
static const netField_t msg_defaultEntityNetFields[MSG_ENTITY_NETFIELD_COUNT] = {
    MSG_ENTITY_NETFIELD("pos.trTime", pos.trTime, 32),
    MSG_ENTITY_NETFIELD("pos.trBase[0]", pos.trBase[0], 0),
    MSG_ENTITY_NETFIELD("pos.trBase[1]", pos.trBase[1], 0),
    MSG_ENTITY_NETFIELD("pos.trDelta[1]", pos.trDelta[1], 0),
    MSG_ENTITY_NETFIELD("pos.trDelta[0]", pos.trDelta[0], 0),
    MSG_ENTITY_NETFIELD("angles2[1]", angles2[1], 0),
    MSG_ENTITY_NETFIELD("apos.trBase[1]", apos.trBase[1], -100),
    MSG_ENTITY_NETFIELD("apos.trBase[0]", apos.trBase[0], -100),
    MSG_ENTITY_NETFIELD("pos.trBase[2]", pos.trBase[2], 0),
    MSG_ENTITY_NETFIELD("pos.trDelta[2]", pos.trDelta[2], 0),
    MSG_ENTITY_NETFIELD("eventSequence", eventSequence, 8),
    MSG_ENTITY_NETFIELD("eType", eType, 8),
    MSG_ENTITY_NETFIELD("eFlags", eFlags, 24),
    MSG_ENTITY_NETFIELD("groundEntityNum", groundEntityNum, 10),
    MSG_ENTITY_NETFIELD("legsAnim", legsAnim, 10),
    MSG_ENTITY_NETFIELD("clientNum", clientNum, 8),
    MSG_ENTITY_NETFIELD("apos.trBase[2]", apos.trBase[2], -100),
    MSG_ENTITY_NETFIELD("events[0]", events[0], 8),
    MSG_ENTITY_NETFIELD("index", index, 9),
    MSG_ENTITY_NETFIELD("events[1]", events[1], 8),
    MSG_ENTITY_NETFIELD("events[2]", events[2], 8),
    MSG_ENTITY_NETFIELD("events[3]", events[3], 8),
    MSG_ENTITY_NETFIELD("eventParm", eventParm, 8),
    MSG_ENTITY_NETFIELD("torsoAnim", torsoAnim, 10),
    MSG_ENTITY_NETFIELD("surfType", surfType, 8),
    MSG_ENTITY_NETFIELD("scale", scale, 8),
    MSG_ENTITY_NETFIELD("otherEntityNum", otherEntityNum, 10),
    MSG_ENTITY_NETFIELD("fWaistPitch", fWaistPitch, 0),
    MSG_ENTITY_NETFIELD("pos.trType", pos.trType, 8),
    MSG_ENTITY_NETFIELD("xmodel", xmodel, 9),
    MSG_ENTITY_NETFIELD("angles2[0]", angles2[0], 0),
    MSG_ENTITY_NETFIELD("fTorsoHeight", fTorsoHeight, 0),
    MSG_ENTITY_NETFIELD("fTorsoPitch", fTorsoPitch, 0),
    MSG_ENTITY_NETFIELD("apos.trType", apos.trType, 8),
    MSG_ENTITY_NETFIELD("solid", solid, 24),
    MSG_ENTITY_NETFIELD("weapon", weapon, 7),
    MSG_ENTITY_NETFIELD("apos.trTime", apos.trTime, 32),
    MSG_ENTITY_NETFIELD("apos.trDelta[0]", apos.trDelta[0], 0),
    MSG_ENTITY_NETFIELD("eventParms[0]", eventParms[0], 8),
    MSG_ENTITY_NETFIELD("pos.trDuration", pos.trDuration, 32),
    MSG_ENTITY_NETFIELD("animMovetype", animMovetype, 4),
    MSG_ENTITY_NETFIELD("eventParms[1]", eventParms[1], 8),
    MSG_ENTITY_NETFIELD("apos.trDelta[2]", apos.trDelta[2], 0),
    MSG_ENTITY_NETFIELD("eventParms[2]", eventParms[2], 8),
    MSG_ENTITY_NETFIELD("eventParms[3]", eventParms[3], 8),
    MSG_ENTITY_NETFIELD("leanf", leanf, 0),
    MSG_ENTITY_NETFIELD("apos.trDelta[1]", apos.trDelta[1], 0),
    MSG_ENTITY_NETFIELD("loopSound", loopSound, 8),
    MSG_ENTITY_NETFIELD("attackerEntityNum", attackerEntityNum, 10),
    MSG_ENTITY_NETFIELD("iHeadIcon", iHeadIcon, 4),
    MSG_ENTITY_NETFIELD("iHeadIconTeam", iHeadIconTeam, 2),
    MSG_ENTITY_NETFIELD("apos.trDuration", apos.trDuration, 32),
    MSG_ENTITY_NETFIELD("time", time, 32),
    MSG_ENTITY_NETFIELD("time2", time2, 32),
    MSG_ENTITY_NETFIELD("origin2[0]", origin2[0], 0),
    MSG_ENTITY_NETFIELD("origin2[1]", origin2[1], 0),
    MSG_ENTITY_NETFIELD("origin2[2]", origin2[2], 0),
    MSG_ENTITY_NETFIELD("angles2[2]", angles2[2], 0),
    MSG_ENTITY_NETFIELD("constantLight", constantLight, 32),
    MSG_ENTITY_NETFIELD("dmgFlags", dmgFlags, 32)};

/* Source: CoDUOMP.exe 0x0058ef38..0x0058f207 (.rdata). Vehicle entities use
 * different priorities and encodings for their trajectory/orientation fields.
 * PE_RELOCATION_VALUES_VERIFIED: verify_relocated_initializers.py follows all
 * 60 original string pointers and proves their ordered source targets. */
static const netField_t msg_vehicleEntityNetFields[MSG_ENTITY_NETFIELD_COUNT] = {
    MSG_ENTITY_NETFIELD("pos.trBase[2]", pos.trBase[2], 0),
    MSG_ENTITY_NETFIELD("pos.trBase[0]", pos.trBase[0], 0),
    MSG_ENTITY_NETFIELD("pos.trBase[1]", pos.trBase[1], 0),
    MSG_ENTITY_NETFIELD("apos.trBase[2]", apos.trBase[2], -100),
    MSG_ENTITY_NETFIELD("apos.trBase[1]", apos.trBase[1], -100),
    MSG_ENTITY_NETFIELD("apos.trBase[0]", apos.trBase[0], -100),
    MSG_ENTITY_NETFIELD("angles2[1]", angles2[1], -100),
    MSG_ENTITY_NETFIELD("time", time, -16),
    MSG_ENTITY_NETFIELD("time2", time2, 6),
    MSG_ENTITY_NETFIELD("origin2[0]", origin2[0], 8),
    MSG_ENTITY_NETFIELD("origin2[2]", origin2[2], 8),
    MSG_ENTITY_NETFIELD("angles2[0]", angles2[0], -100),
    MSG_ENTITY_NETFIELD("origin2[1]", origin2[1], -100),
    MSG_ENTITY_NETFIELD("angles2[2]", angles2[2], -100),
    MSG_ENTITY_NETFIELD("leanf", leanf, -100),
    MSG_ENTITY_NETFIELD("loopSound", loopSound, 8),
    MSG_ENTITY_NETFIELD("eType", eType, 8),
    MSG_ENTITY_NETFIELD("eFlags", eFlags, 24),
    MSG_ENTITY_NETFIELD("eventSequence", eventSequence, 8),
    MSG_ENTITY_NETFIELD("events[0]", events[0], 8),
    MSG_ENTITY_NETFIELD("events[1]", events[1], 8),
    MSG_ENTITY_NETFIELD("events[2]", events[2], 8),
    MSG_ENTITY_NETFIELD("events[3]", events[3], 8),
    MSG_ENTITY_NETFIELD("eventParm", eventParm, 8),
    MSG_ENTITY_NETFIELD("eventParms[0]", eventParms[0], 8),
    MSG_ENTITY_NETFIELD("eventParms[1]", eventParms[1], 8),
    MSG_ENTITY_NETFIELD("eventParms[2]", eventParms[2], 8),
    MSG_ENTITY_NETFIELD("eventParms[3]", eventParms[3], 8),
    MSG_ENTITY_NETFIELD("iHeadIcon", iHeadIcon, 6),
    MSG_ENTITY_NETFIELD("iHeadIconTeam", iHeadIconTeam, 2),
    MSG_ENTITY_NETFIELD("animMovetype", animMovetype, 8),
    MSG_ENTITY_NETFIELD("index", index, 9),
    MSG_ENTITY_NETFIELD("torsoAnim", torsoAnim, 10),
    MSG_ENTITY_NETFIELD("surfType", surfType, 8),
    MSG_ENTITY_NETFIELD("scale", scale, 8),
    MSG_ENTITY_NETFIELD("otherEntityNum", otherEntityNum, 10),
    MSG_ENTITY_NETFIELD("fWaistPitch", fWaistPitch, 0),
    MSG_ENTITY_NETFIELD("pos.trType", pos.trType, 8),
    MSG_ENTITY_NETFIELD("xmodel", xmodel, 9),
    MSG_ENTITY_NETFIELD("fTorsoHeight", fTorsoHeight, 0),
    MSG_ENTITY_NETFIELD("fTorsoPitch", fTorsoPitch, 0),
    MSG_ENTITY_NETFIELD("apos.trType", apos.trType, 8),
    MSG_ENTITY_NETFIELD("solid", solid, 24),
    MSG_ENTITY_NETFIELD("weapon", weapon, 7),
    MSG_ENTITY_NETFIELD("apos.trTime", apos.trTime, 32),
    MSG_ENTITY_NETFIELD("apos.trDelta[0]", apos.trDelta[0], 0),
    MSG_ENTITY_NETFIELD("apos.trDelta[2]", apos.trDelta[2], 0),
    MSG_ENTITY_NETFIELD("apos.trDelta[1]", apos.trDelta[1], 0),
    MSG_ENTITY_NETFIELD("attackerEntityNum", attackerEntityNum, 10),
    MSG_ENTITY_NETFIELD("apos.trDuration", apos.trDuration, 32),
    MSG_ENTITY_NETFIELD("constantLight", constantLight, 32),
    MSG_ENTITY_NETFIELD("dmgFlags", dmgFlags, 32),
    MSG_ENTITY_NETFIELD("pos.trTime", pos.trTime, 32),
    MSG_ENTITY_NETFIELD("pos.trDuration", pos.trDuration, 32),
    MSG_ENTITY_NETFIELD("groundEntityNum", groundEntityNum, 10),
    MSG_ENTITY_NETFIELD("legsAnim", legsAnim, 10),
    MSG_ENTITY_NETFIELD("clientNum", clientNum, 8),
    MSG_ENTITY_NETFIELD("pos.trDelta[1]", pos.trDelta[1], 0),
    MSG_ENTITY_NETFIELD("pos.trDelta[0]", pos.trDelta[0], 0),
    MSG_ENTITY_NETFIELD("pos.trDelta[2]", pos.trDelta[2], 0)};

#undef MSG_ENTITY_NETFIELD

#define MSG_ARCHIVED_ENTITY_NETFIELD(name_, member_, bits_) {(name_), (int32_t)offsetof(archivedEntity_t, member_), (bits_)}

/* Source: CoDUOMP.exe 0x0058f208..0x0058f537 (.rdata). The descriptor offsets
 * prove an entityState_t prefix followed by svFlags, singleClient, absmin, and
 * absmax.
 * PE_RELOCATION_VALUES_VERIFIED: verify_relocated_initializers.py follows all
 * 68 original string pointers and proves their ordered source targets. */
static const netField_t msg_archivedEntityNetFields[MSG_ARCHIVED_ENTITY_NETFIELD_COUNT] = {
    MSG_ARCHIVED_ENTITY_NETFIELD("absmin[1]", absmin[1], 0),
    MSG_ARCHIVED_ENTITY_NETFIELD("absmax[1]", absmax[1], 0),
    MSG_ARCHIVED_ENTITY_NETFIELD("absmin[0]", absmin[0], 0),
    MSG_ARCHIVED_ENTITY_NETFIELD("absmax[0]", absmax[0], 0),
    MSG_ARCHIVED_ENTITY_NETFIELD("absmin[2]", absmin[2], 0),
    MSG_ARCHIVED_ENTITY_NETFIELD("absmax[2]", absmax[2], 0),
    MSG_ARCHIVED_ENTITY_NETFIELD("pos.trBase[1]", state.pos.trBase[1], 0),
    MSG_ARCHIVED_ENTITY_NETFIELD("pos.trBase[0]", state.pos.trBase[0], 0),
    MSG_ARCHIVED_ENTITY_NETFIELD("eType", state.eType, 8),
    MSG_ARCHIVED_ENTITY_NETFIELD("eFlags", state.eFlags, 24),
    MSG_ARCHIVED_ENTITY_NETFIELD("pos.trBase[2]", state.pos.trBase[2], 0),
    MSG_ARCHIVED_ENTITY_NETFIELD("svFlags", svFlags, 32),
    MSG_ARCHIVED_ENTITY_NETFIELD("groundEntityNum", state.groundEntityNum, 10),
    MSG_ARCHIVED_ENTITY_NETFIELD("apos.trBase[1]", state.apos.trBase[1], 0),
    MSG_ARCHIVED_ENTITY_NETFIELD("clientNum", state.clientNum, 8),
    MSG_ARCHIVED_ENTITY_NETFIELD("apos.trBase[0]", state.apos.trBase[0], 0),
    MSG_ARCHIVED_ENTITY_NETFIELD("index", state.index, 9),
    MSG_ARCHIVED_ENTITY_NETFIELD("apos.trBase[2]", state.apos.trBase[2], 0),
    MSG_ARCHIVED_ENTITY_NETFIELD("eventSequence", state.eventSequence, 8),
    MSG_ARCHIVED_ENTITY_NETFIELD("events[0]", state.events[0], 8),
    MSG_ARCHIVED_ENTITY_NETFIELD("legsAnim", state.legsAnim, 10),
    MSG_ARCHIVED_ENTITY_NETFIELD("events[1]", state.events[1], 8),
    MSG_ARCHIVED_ENTITY_NETFIELD("events[2]", state.events[2], 8),
    MSG_ARCHIVED_ENTITY_NETFIELD("events[3]", state.events[3], 8),
    MSG_ARCHIVED_ENTITY_NETFIELD("weapon", state.weapon, 7),
    MSG_ARCHIVED_ENTITY_NETFIELD("pos.trType", state.pos.trType, 8),
    MSG_ARCHIVED_ENTITY_NETFIELD("xmodel", state.xmodel, 9),
    MSG_ARCHIVED_ENTITY_NETFIELD("pos.trTime", state.pos.trTime, 32),
    MSG_ARCHIVED_ENTITY_NETFIELD("apos.trType", state.apos.trType, 8),
    MSG_ARCHIVED_ENTITY_NETFIELD("solid", state.solid, 24),
    MSG_ARCHIVED_ENTITY_NETFIELD("pos.trDuration", state.pos.trDuration, 32),
    MSG_ARCHIVED_ENTITY_NETFIELD("eventParms[0]", state.eventParms[0], 8),
    MSG_ARCHIVED_ENTITY_NETFIELD("torsoAnim", state.torsoAnim, 10),
    MSG_ARCHIVED_ENTITY_NETFIELD("pos.trDelta[0]", state.pos.trDelta[0], 0),
    MSG_ARCHIVED_ENTITY_NETFIELD("pos.trDelta[1]", state.pos.trDelta[1], 0),
    MSG_ARCHIVED_ENTITY_NETFIELD("angles2[1]", state.angles2[1], 0),
    MSG_ARCHIVED_ENTITY_NETFIELD("angles2[0]", state.angles2[0], 0),
    MSG_ARCHIVED_ENTITY_NETFIELD("animMovetype", state.animMovetype, 4),
    MSG_ARCHIVED_ENTITY_NETFIELD("pos.trDelta[2]", state.pos.trDelta[2], 0),
    MSG_ARCHIVED_ENTITY_NETFIELD("otherEntityNum", state.otherEntityNum, 10),
    MSG_ARCHIVED_ENTITY_NETFIELD("eventParms[1]", state.eventParms[1], 8),
    MSG_ARCHIVED_ENTITY_NETFIELD("surfType", state.surfType, 8),
    MSG_ARCHIVED_ENTITY_NETFIELD("eventParm", state.eventParm, 8),
    MSG_ARCHIVED_ENTITY_NETFIELD("eventParms[2]", state.eventParms[2], 8),
    MSG_ARCHIVED_ENTITY_NETFIELD("scale", state.scale, 8),
    MSG_ARCHIVED_ENTITY_NETFIELD("eventParms[3]", state.eventParms[3], 8),
    MSG_ARCHIVED_ENTITY_NETFIELD("fTorsoHeight", state.fTorsoHeight, 0),
    MSG_ARCHIVED_ENTITY_NETFIELD("fWaistPitch", state.fWaistPitch, 0),
    MSG_ARCHIVED_ENTITY_NETFIELD("fTorsoPitch", state.fTorsoPitch, 0),
    MSG_ARCHIVED_ENTITY_NETFIELD("apos.trTime", state.apos.trTime, 32),
    MSG_ARCHIVED_ENTITY_NETFIELD("apos.trDelta[0]", state.apos.trDelta[0], 0),
    MSG_ARCHIVED_ENTITY_NETFIELD("apos.trDelta[2]", state.apos.trDelta[2], 0),
    MSG_ARCHIVED_ENTITY_NETFIELD("singleClient", singleClient, 6),
    MSG_ARCHIVED_ENTITY_NETFIELD("leanf", state.leanf, 0),
    MSG_ARCHIVED_ENTITY_NETFIELD("apos.trDelta[1]", state.apos.trDelta[1], 0),
    MSG_ARCHIVED_ENTITY_NETFIELD("loopSound", state.loopSound, 8),
    MSG_ARCHIVED_ENTITY_NETFIELD("attackerEntityNum", state.attackerEntityNum, 10),
    MSG_ARCHIVED_ENTITY_NETFIELD("iHeadIcon", state.iHeadIcon, 4),
    MSG_ARCHIVED_ENTITY_NETFIELD("iHeadIconTeam", state.iHeadIconTeam, 2),
    MSG_ARCHIVED_ENTITY_NETFIELD("apos.trDuration", state.apos.trDuration, 32),
    MSG_ARCHIVED_ENTITY_NETFIELD("time", state.time, 32),
    MSG_ARCHIVED_ENTITY_NETFIELD("time2", state.time2, 32),
    MSG_ARCHIVED_ENTITY_NETFIELD("origin2[0]", state.origin2[0], 0),
    MSG_ARCHIVED_ENTITY_NETFIELD("origin2[1]", state.origin2[1], 0),
    MSG_ARCHIVED_ENTITY_NETFIELD("origin2[2]", state.origin2[2], 0),
    MSG_ARCHIVED_ENTITY_NETFIELD("angles2[2]", state.angles2[2], 0),
    MSG_ARCHIVED_ENTITY_NETFIELD("constantLight", state.constantLight, 32),
    MSG_ARCHIVED_ENTITY_NETFIELD("dmgFlags", state.dmgFlags, 32)};

#undef MSG_ARCHIVED_ENTITY_NETFIELD

#define MSG_CLIENT_NETFIELD(name_, member_, bits_) {(name_), (int32_t)offsetof(clientState_t, member_), (bits_)}

/* Source: CoDUOMP.exe 0x0058f538..0x0058f63f (.rdata).
 * PE_RELOCATION_VALUES_VERIFIED: verify_relocated_initializers.py follows all
 * 22 original string pointers and proves their ordered source targets. */
static const netField_t msg_clientNetFields[MSG_CLIENT_NETFIELD_COUNT] = {
    MSG_CLIENT_NETFIELD("team", team, 2),
    MSG_CLIENT_NETFIELD("name[0]", name[0], 32),
    MSG_CLIENT_NETFIELD("name[4]", name[4], 32),
    MSG_CLIENT_NETFIELD("attachModelIndex[0]", attachModelIndex[0], 8),
    MSG_CLIENT_NETFIELD("attachModelIndex[2]", attachModelIndex[2], 8),
    MSG_CLIENT_NETFIELD("attachModelIndex[3]", attachModelIndex[3], 8),
    MSG_CLIENT_NETFIELD("modelindex", modelindex, 8),
    MSG_CLIENT_NETFIELD("attachModelIndex[1]", attachModelIndex[1], 8),
    MSG_CLIENT_NETFIELD("name[8]", name[8], 32),
    MSG_CLIENT_NETFIELD("name[12]", name[12], 32),
    MSG_CLIENT_NETFIELD("attachModelIndex[4]", attachModelIndex[4], 8),
    MSG_CLIENT_NETFIELD("name[16]", name[16], 32),
    MSG_CLIENT_NETFIELD("attachTagIndex[0]", attachTagIndex[0], 5),
    MSG_CLIENT_NETFIELD("attachTagIndex[1]", attachTagIndex[1], 5),
    MSG_CLIENT_NETFIELD("attachTagIndex[2]", attachTagIndex[2], 5),
    MSG_CLIENT_NETFIELD("attachTagIndex[3]", attachTagIndex[3], 5),
    MSG_CLIENT_NETFIELD("attachTagIndex[4]", attachTagIndex[4], 5),
    MSG_CLIENT_NETFIELD("attachModelIndex[5]", attachModelIndex[5], 8),
    MSG_CLIENT_NETFIELD("attachTagIndex[5]", attachTagIndex[5], 5),
    MSG_CLIENT_NETFIELD("name[20]", name[20], 32),
    MSG_CLIENT_NETFIELD("name[24]", name[24], 32),
    MSG_CLIENT_NETFIELD("name[28]", name[28], 32)};

#undef MSG_CLIENT_NETFIELD

#define MSG_HUD_ELEM_NETFIELD(name_, member_, bits_) {(name_), (int32_t)offsetof(hudElem_t, member_), (bits_)}

/* Source: CoDUOMP.exe 0x005c5768..0x005c58cf (.data). The original mutable
 * placement is incidental; no instruction writes the descriptors after image
 * load. The final unused78 dword is outside this wire table.
 * PE_RELOCATION_VALUES_VERIFIED: verify_relocated_initializers.py follows all
 * 30 original string pointers and proves their ordered source targets. */
static const netField_t msg_hudElemNetFields[MSG_HUD_ELEM_NETFIELD_COUNT] = {MSG_HUD_ELEM_NETFIELD("color.rgba", color.rgba, 32),
                                                                             MSG_HUD_ELEM_NETFIELD("type", type, 4),
                                                                             MSG_HUD_ELEM_NETFIELD("fontScale", fontScale, 0),
                                                                             MSG_HUD_ELEM_NETFIELD("y", y, 10),
                                                                             MSG_HUD_ELEM_NETFIELD("x", x, 10),
                                                                             MSG_HUD_ELEM_NETFIELD("alignY", alignY, 2),
                                                                             MSG_HUD_ELEM_NETFIELD("alignX", alignX, 2),
                                                                             MSG_HUD_ELEM_NETFIELD("time", timerValue, 32),
                                                                             MSG_HUD_ELEM_NETFIELD("font", font, 4),
                                                                             MSG_HUD_ELEM_NETFIELD("text", text, 8),
                                                                             MSG_HUD_ELEM_NETFIELD("shaderIndex", materialIndex, 8),
                                                                             MSG_HUD_ELEM_NETFIELD("width", width, 10),
                                                                             MSG_HUD_ELEM_NETFIELD("height", height, 10),
                                                                             MSG_HUD_ELEM_NETFIELD("sort", sortKey, 0),
                                                                             MSG_HUD_ELEM_NETFIELD("fromColor.rgba", fromColor.rgba, 32),
                                                                             MSG_HUD_ELEM_NETFIELD("fadeStartTime", fadeStartTime, 32),
                                                                             MSG_HUD_ELEM_NETFIELD("fadeTime", fadeTime, 16),
                                                                             MSG_HUD_ELEM_NETFIELD("scaleStartTime", scaleStartTime, 32),
                                                                             MSG_HUD_ELEM_NETFIELD("scaleTime", scaleTime, 16),
                                                                             MSG_HUD_ELEM_NETFIELD("fromHeight", scaleFromHeight, 10),
                                                                             MSG_HUD_ELEM_NETFIELD("value", value, 0),
                                                                             MSG_HUD_ELEM_NETFIELD("label", label, 8),
                                                                             MSG_HUD_ELEM_NETFIELD("fromWidth", scaleFromWidth, 10),
                                                                             MSG_HUD_ELEM_NETFIELD("moveStartTime", moveStartTime, 32),
                                                                             MSG_HUD_ELEM_NETFIELD("moveTime", moveTime, 16),
                                                                             MSG_HUD_ELEM_NETFIELD("fromX", moveFromX, 10),
                                                                             MSG_HUD_ELEM_NETFIELD("fromY", moveFromY, 10),
                                                                             MSG_HUD_ELEM_NETFIELD("duration", rotationPeriodMs, 32),
                                                                             MSG_HUD_ELEM_NETFIELD("SCOORD", shaderRightTexcoord, 32),
                                                                             MSG_HUD_ELEM_NETFIELD("TCOORD", shaderBottomTexcoord, 32)};

#undef MSG_HUD_ELEM_NETFIELD

#define MSG_PLAYERSTATE_NETFIELD(name_, member_, bits_) {(name_), (int32_t)offsetof(playerState_t, member_), (bits_)}

/* Source: CoDUOMP.exe 0x0058f640..0x0058fb97 (.rdata).
 * PE_RELOCATION_VALUES_VERIFIED: verify_relocated_initializers.py follows all
 * 114 original string pointers and proves their ordered source targets. */
static const netField_t msg_playerStateNetFields[MSG_PLAYERSTATE_NETFIELD_COUNT] = {
    MSG_PLAYERSTATE_NETFIELD("commandTime", commandTime, 32),
    MSG_PLAYERSTATE_NETFIELD("origin[1]", psOrigin[1], 0),
    MSG_PLAYERSTATE_NETFIELD("origin[0]", psOrigin[0], 0),
    MSG_PLAYERSTATE_NETFIELD("origin[2]", psOrigin[2], 0),
    MSG_PLAYERSTATE_NETFIELD("viewangles[1]", viewAngles[1], -100),
    MSG_PLAYERSTATE_NETFIELD("viewangles[0]", viewAngles[0], -100),
    MSG_PLAYERSTATE_NETFIELD("iCompassFriendInfo", compassFriendInfo, 32),
    MSG_PLAYERSTATE_NETFIELD("eFlags", entityStateFlags, 24),
    MSG_PLAYERSTATE_NETFIELD("bobCycle", bobCycle, 8),
    MSG_PLAYERSTATE_NETFIELD("viewHeightCurrent", viewHeightCurrent, 0),
    MSG_PLAYERSTATE_NETFIELD("eventSequence", eventIndex, 8),
    MSG_PLAYERSTATE_NETFIELD("legsAnim", legsAnim, 10),
    MSG_PLAYERSTATE_NETFIELD("pm_flags", playerStateFlags, 24),
    MSG_PLAYERSTATE_NETFIELD("delta_angles[1]", deltaAngles[1], 16),
    MSG_PLAYERSTATE_NETFIELD("velocity[1]", velocity[1], 0),
    MSG_PLAYERSTATE_NETFIELD("velocity[0]", velocity[0], 0),
    MSG_PLAYERSTATE_NETFIELD("iCompassTankInfo", compassTankInfo, 32),
    MSG_PLAYERSTATE_NETFIELD("speed", speed, 16),
    MSG_PLAYERSTATE_NETFIELD("mins[0]", playerMins[0], 0),
    MSG_PLAYERSTATE_NETFIELD("mins[1]", playerMins[1], 0),
    MSG_PLAYERSTATE_NETFIELD("maxs[0]", playerMaxs[0], 0),
    MSG_PLAYERSTATE_NETFIELD("maxs[1]", playerMaxs[1], 0),
    MSG_PLAYERSTATE_NETFIELD("maxs[2]", playerMaxs[2], 0),
    MSG_PLAYERSTATE_NETFIELD("proneViewHeight", proneViewHeight, -8),
    MSG_PLAYERSTATE_NETFIELD("crouchViewHeight", crouchViewHeight, -8),
    MSG_PLAYERSTATE_NETFIELD("standViewHeight", standViewHeight, -8),
    MSG_PLAYERSTATE_NETFIELD("deadViewHeight", deadViewHeight, -8),
    MSG_PLAYERSTATE_NETFIELD("walkSpeedScale", walkSpeedScale, 0),
    MSG_PLAYERSTATE_NETFIELD("runSpeedScale", runSpeedScale, 0),
    MSG_PLAYERSTATE_NETFIELD("sprintSpeedScale", sprintSpeedScale, 0),
    MSG_PLAYERSTATE_NETFIELD("proneSpeedScale", proneSpeedScale, 0),
    MSG_PLAYERSTATE_NETFIELD("crouchSpeedScale", crouchSpeedScale, 0),
    MSG_PLAYERSTATE_NETFIELD("strafeSpeedScale", strafeSpeedScale, 0),
    MSG_PLAYERSTATE_NETFIELD("backSpeedScale", backSpeedScale, 0),
    MSG_PLAYERSTATE_NETFIELD("leanSpeedScale", leanSpeedScale, 0),
    MSG_PLAYERSTATE_NETFIELD("fatigueScale", fatigueScale, 0),
    MSG_PLAYERSTATE_NETFIELD("lastSprintTime", lastSprintTime, 0),
    MSG_PLAYERSTATE_NETFIELD("friction", friction, 0),
    MSG_PLAYERSTATE_NETFIELD("groundEntityNum", groundEntityNum, 10),
    MSG_PLAYERSTATE_NETFIELD("viewHeightTarget", viewHeightTarget, -8),
    MSG_PLAYERSTATE_NETFIELD("events[0]", events[0], 8),
    MSG_PLAYERSTATE_NETFIELD("weapons[0]", weaponBits[0], 32),
    MSG_PLAYERSTATE_NETFIELD("weaponslots[0]", weaponSlots[0], 32),
    MSG_PLAYERSTATE_NETFIELD("gravity", gravity, 16),
    MSG_PLAYERSTATE_NETFIELD("serverCursorHintString", serverCursorHintString, -8),
    MSG_PLAYERSTATE_NETFIELD("viewmodelIndex", viewModelIndex, 8),
    MSG_PLAYERSTATE_NETFIELD("events[1]", events[1], 8),
    MSG_PLAYERSTATE_NETFIELD("events[2]", events[2], 8),
    MSG_PLAYERSTATE_NETFIELD("events[3]", events[3], 8),
    MSG_PLAYERSTATE_NETFIELD("delta_angles[0]", deltaAngles[0], 16),
    MSG_PLAYERSTATE_NETFIELD("weapon", currentWeapon, 7),
    MSG_PLAYERSTATE_NETFIELD("movementDir", movementDir, 8),
    MSG_PLAYERSTATE_NETFIELD("viewHeightLerpTarget", viewHeightLerpTarget, -8),
    MSG_PLAYERSTATE_NETFIELD("weaponslots[4]", weaponSlots[4], 32),
    MSG_PLAYERSTATE_NETFIELD("proneDirection", proneDirection, 0),
    MSG_PLAYERSTATE_NETFIELD("aimSpreadScale", aimSpreadScale, 0),
    MSG_PLAYERSTATE_NETFIELD("weapAnim", weaponAnim, 10),
    MSG_PLAYERSTATE_NETFIELD("jumpTime", lastJumpCommandTime, 32),
    MSG_PLAYERSTATE_NETFIELD("velocity[2]", velocity[2], 0),
    MSG_PLAYERSTATE_NETFIELD("weaponTime", weaponTime, -16),
    MSG_PLAYERSTATE_NETFIELD("proneTorsoPitch", proneTorsoPitch, 0),
    MSG_PLAYERSTATE_NETFIELD("proneDirectionPitch", proneDirectionPitch, 0),
    MSG_PLAYERSTATE_NETFIELD("fTorsoPitch", torsoPitch, 0),
    MSG_PLAYERSTATE_NETFIELD("fWaistPitch", waistPitch, 0),
    MSG_PLAYERSTATE_NETFIELD("fTorsoHeight", torsoHeight, 0),
    MSG_PLAYERSTATE_NETFIELD("weaponstate", weaponState, 4),
    MSG_PLAYERSTATE_NETFIELD("torsoTimer", torsoTimer, 16),
    MSG_PLAYERSTATE_NETFIELD("torsoAnim", torsoAnim, 10),
    MSG_PLAYERSTATE_NETFIELD("eventParms[0]", eventParms[0], 8),
    MSG_PLAYERSTATE_NETFIELD("vLadderVec[0]", ladderNormal[0], 0),
    MSG_PLAYERSTATE_NETFIELD("eventParms[3]", eventParms[3], 8),
    MSG_PLAYERSTATE_NETFIELD("viewHeightLerpDown", viewHeightLerpDown, 1),
    MSG_PLAYERSTATE_NETFIELD("weaponDelay", weaponDelay, -16),
    MSG_PLAYERSTATE_NETFIELD("eventParms[1]", eventParms[1], 8),
    MSG_PLAYERSTATE_NETFIELD("viewHeightLerpTime", viewHeightLerpTime, 32),
    MSG_PLAYERSTATE_NETFIELD("eventParms[2]", eventParms[2], 8),
    MSG_PLAYERSTATE_NETFIELD("vLadderVec[1]", ladderNormal[1], 0),
    MSG_PLAYERSTATE_NETFIELD("fWeaponPosFrac", adsFraction, 0),
    MSG_PLAYERSTATE_NETFIELD("pm_type", pmType, 8),
    MSG_PLAYERSTATE_NETFIELD("legsTimer", legsTimer, 16),
    MSG_PLAYERSTATE_NETFIELD("fJumpOriginZ", jumpOriginZ, 0),
    MSG_PLAYERSTATE_NETFIELD("leanf", leanFraction, 0),
    MSG_PLAYERSTATE_NETFIELD("damageEvent", damageEvent, 8),
    MSG_PLAYERSTATE_NETFIELD("damageYaw", damageYaw, 8),
    MSG_PLAYERSTATE_NETFIELD("damagePitch", damagePitch, 8),
    MSG_PLAYERSTATE_NETFIELD("damageCount", damageCount, 7),
    MSG_PLAYERSTATE_NETFIELD("weaponrechamber[0]", weaponRechamberBits[0], 32),
    MSG_PLAYERSTATE_NETFIELD("grenadeTimeLeft", grenadeTimeLeft, -16),
    MSG_PLAYERSTATE_NETFIELD("pm_time", pmTime, -16),
    MSG_PLAYERSTATE_NETFIELD("iFoliageSoundTime", foliageSoundTime, 32),
    MSG_PLAYERSTATE_NETFIELD("iFatigueSoundTime", fatigueSoundTime, 32),
    MSG_PLAYERSTATE_NETFIELD("deltaTime", deltaTime, 32),
    MSG_PLAYERSTATE_NETFIELD("serverCursorHint", serverCursorHint, 8),
    MSG_PLAYERSTATE_NETFIELD("serverCursorHintVal", serverCursorHintVal, 8),
    MSG_PLAYERSTATE_NETFIELD("shellshockIndex", motionState.shellshock.index, 4),
    MSG_PLAYERSTATE_NETFIELD("shellshockTime", motionState.shellshock.time, 32),
    MSG_PLAYERSTATE_NETFIELD("shellshockDuration", motionState.shellshock.duration, 16),
    MSG_PLAYERSTATE_NETFIELD("delta_angles[2]", deltaAngles[2], 16),
    MSG_PLAYERSTATE_NETFIELD("vLadderVec[2]", ladderNormal[2], 0),
    MSG_PLAYERSTATE_NETFIELD("clientNum", psClientNum, 8),
    MSG_PLAYERSTATE_NETFIELD("weapons[1]", weaponBits[1], 32),
    MSG_PLAYERSTATE_NETFIELD("weaponrechamber[1]", weaponRechamberBits[1], 32),
    MSG_PLAYERSTATE_NETFIELD("viewangles[2]", viewAngles[2], -100),
    MSG_PLAYERSTATE_NETFIELD("viewHeightLerpPosAdj", viewHeightLerpPosAdj, 0),
    MSG_PLAYERSTATE_NETFIELD("mins[2]", playerMins[2], 0),
    MSG_PLAYERSTATE_NETFIELD("viewlocked", viewLocked, 8),
    MSG_PLAYERSTATE_NETFIELD("viewlocked_entNum", viewLockedEntityNum, 16),
    MSG_PLAYERSTATE_NETFIELD("vehPos", vehiclePosition, 4),
    MSG_PLAYERSTATE_NETFIELD("vehType", vehicleType, 4),
    MSG_PLAYERSTATE_NETFIELD("vehMotion", vehicleMotion, 2),
    MSG_PLAYERSTATE_NETFIELD("weapons[2]", weaponBits[2], 32),
    MSG_PLAYERSTATE_NETFIELD("weapons[3]", weaponBits[3], 32),
    MSG_PLAYERSTATE_NETFIELD("weaponrechamber[2]", weaponRechamberBits[2], 32),
    MSG_PLAYERSTATE_NETFIELD("weaponrechamber[3]", weaponRechamberBits[3], 32)};

#undef MSG_PLAYERSTATE_NETFIELD

#define MSG_OBJECTIVE_NETFIELD(name_, member_, bits_) {(name_), (int32_t)offsetof(objective_t, member_), (bits_)}

/* Source: CoDUOMP.exe 0x005c5720..0x005c5767 (.data).
 * PE_RELOCATION_VALUES_VERIFIED: the six pointer fields target, in order,
 * "origin[0]", "origin[1]", "origin[2]", "icon", "entNum", and "teamNum".
 * The state member is transmitted separately as a three-bit selector. */
static const netField_t msg_objectiveNetFields[MSG_OBJECTIVE_NETFIELD_COUNT] = {
    MSG_OBJECTIVE_NETFIELD("origin[0]", origin[0], 0), MSG_OBJECTIVE_NETFIELD("origin[1]", origin[1], 0),
    MSG_OBJECTIVE_NETFIELD("origin[2]", origin[2], 0), MSG_OBJECTIVE_NETFIELD("icon", icon, 12),
    MSG_OBJECTIVE_NETFIELD("entNum", entityNum, 10),   MSG_OBJECTIVE_NETFIELD("teamNum", teamNum, 4)};

#undef MSG_OBJECTIVE_NETFIELD

/* Source: CoDUOMP.exe 0x00449de0..0x00449e20.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00449de0_00449e21.mcode.
 * Name adopted from the matching recovered server-engine primitive; the Mac
 * linker emitted no traceback name for this small helper. */
void MSG_WriteDeltaValue(msg_t *message, int32_t oldValue, int32_t newValue, int32_t bitCount)
{
    if (oldValue == newValue) {
        MSG_WriteBit0(message);
        return;
    }

    MSG_WriteBit1(message);
    MSG_WriteBits(message, newValue, bitCount);
}

/* Source: CoDUOMP.exe 0x00449e30..0x00449e82.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00449e30_00449e83.mcode.
 * Name adopted from the matching recovered server-engine primitive. */
int32_t MSG_ReadDeltaValue(msg_t *message, int32_t oldValue, int32_t bitCount)
{
    if (MSG_ReadBit(message) == 0)
        return oldValue;
    return MSG_ReadBits(message, bitCount);
}

/* Source: CoDUOMP.exe 0x00449e90..0x00449ed4.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00449e90_00449ed5.mcode.
 * Name and signature: exact same-module Mac symbol MSG_WriteDeltaKey. */
void MSG_WriteDeltaKey(msg_t *message, uint32_t key, int32_t oldValue, int32_t newValue, int32_t bitCount)
{
    if (oldValue == newValue) {
        MSG_WriteBit0(message);
        return;
    }

    MSG_WriteBit1(message);
    MSG_WriteBits(message, (int32_t)(key ^ (uint32_t)newValue), bitCount);
}

/* Source: CoDUOMP.exe 0x00449ee0..0x00449f43.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00449ee0_00449f44.mcode.
 * Name and signature: exact same-module Mac symbol MSG_ReadDeltaKey. */
int32_t MSG_ReadDeltaKey(msg_t *message, uint32_t key, int32_t oldValue, int32_t bitCount)
{
    if (MSG_ReadBit(message) == 0)
        return oldValue;
    return MSG_ReadBits(message, bitCount) ^ (int32_t)(msg_bitmaskTable[bitCount] & key);
}

/* Source: CoDUOMP.exe 0x00449f50..0x00449f5a.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00449f50_00449f5b.mcode.
 * Name and signature: exact same-module Mac symbol MSG_WriteKey. */
void MSG_WriteKey(msg_t *message, uint32_t key, int32_t value, int32_t bitCount)
{
    MSG_WriteBits(message, (int32_t)(key ^ (uint32_t)value), bitCount);
}

/* Source: CoDUOMP.exe 0x00449f60..0x00449f7a.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00449f60_00449f7b.mcode.
 * Name and signature: exact same-module Mac symbol MSG_ReadKey. */
int32_t MSG_ReadKey(msg_t *message, uint32_t key, int32_t bitCount)
{
    return MSG_ReadBits(message, bitCount) ^ (int32_t)(msg_bitmaskTable[bitCount] & key);
}

/* Source: CoDUOMP.exe 0x00449f80..0x00449fd9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00449f80_00449fda.mcode.
 * Name and signature: exact same-module Mac symbol MSG_WriteDeltaKeyByte. */
void MSG_WriteDeltaKeyByte(msg_t *message, uint32_t key, int8_t oldValue, uint32_t newValue)
{
    if (oldValue == (int8_t)newValue) {
        MSG_WriteBit0(message);
        return;
    }

    MSG_WriteBit1(message);
    MSG_WriteByte(message, (int32_t)(key ^ newValue));
}

/* Source: CoDUOMP.exe 0x00449fe0..0x0044a050.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00449fe0_0044a051.mcode.
 * Name and signature: exact same-module Mac symbol MSG_ReadDeltaKeyByte. */
uint32_t MSG_ReadDeltaKeyByte(msg_t *message, uint8_t key, uint32_t oldValue)
{
    if (MSG_ReadBit(message) == 0)
        return oldValue;
    return (uint8_t)(MSG_ReadByte(message) ^ key);
}

/* Source: CoDUOMP.exe 0x0044a060..0x0044a0c4.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0044a060_0044a0c5.mcode.
 * Name and signature: exact same-module Mac symbol MSG_WriteDeltaKeyShort. */
void MSG_WriteDeltaKeyShort(msg_t *message, uint32_t key, int16_t oldValue, uint32_t newValue)
{
    if (oldValue == (int16_t)newValue) {
        MSG_WriteBit0(message);
        return;
    }

    MSG_WriteBit1(message);
    MSG_WriteShort(message, (int32_t)(key ^ newValue));
}

/* Source: CoDUOMP.exe 0x0044a0d0..0x0044a149.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0044a0d0_0044a14a.mcode.
 * Name and signature: exact same-module Mac symbol MSG_ReadDeltaKeyShort. */
int32_t MSG_ReadDeltaKeyShort(msg_t *message, uint16_t key, int32_t oldValue)
{
    if (MSG_ReadBit(message) == 0)
        return oldValue;
    return (int16_t)(key ^ (uint16_t)MSG_ReadShort(message));
}

/* Source: CoDUOMP.exe 0x0044a150..0x0044a1d6.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0044a150_0044a1d7.mcode.
 * Name and signature: exact same-module Mac symbol
 * MSG_WriteReliableCommandToBuffer. The original full-buffer path always
 * terminates output[outputSize - 1]. */
void MSG_WriteReliableCommandToBuffer(const char *input, char *output, int32_t outputSize)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (outputSize <= 0) {
        Com_Printf("WARNING: Reliable command output size is invalid (%i)\n", outputSize);
        return;
    }

    const int32_t length = (int32_t)strlen(input);
    if (length >= outputSize) {
        Com_Printf("WARNING: Reliable command is too long (%i/%i) and will be "
                   "truncated: '%s'\n",
                   length, outputSize, input);
    }
    if (length == 0)
        Com_Printf("WARNING: Empty reliable command\n");

    int32_t index;
    for (index = 0; index < outputSize && input[index] != '\0'; ++index) {
        uint8_t character = (uint8_t)input[index];
        if (character == UINT8_C(0x92))
            character = '\'';
        else if (character > UINT8_C(0x7f))
            character = '.';
        if (character == '%')
            character = '.';
        output[index] = (char)character;
    }

    if (index < outputSize)
        output[index] = '\0';
    else
        output[outputSize - 1] = '\0';
}

/* Source: CoDUOMP.exe 0x0044a1e0..0x0044a2bf.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0044a1e0_0044a2c0.mcode.
 * Name and signature: exact same-module Mac symbol MSG_SetDefaultUserCmd.
 * The negative angle scale is exact PE float bits 0xc3360b61; retaining the
 * original negate/subtract form preserves the instruction-level arithmetic. */
void MSG_SetDefaultUserCmd(const playerState_t *playerState, usercmd_t *command)
{
    memset(command, 0, sizeof(*command));
    command->weapon = (uint8_t)playerState->currentWeapon;

    for (int32_t axis = 0; axis < 2; ++axis) {
#if defined(WINDOWS_BEHAVIOR)
#if EMULATE_X87
        const x87f scaledAngle = x87f_mul(x87f_load_f32(playerState->viewAngles[axis]), x87f_load_f32(MSG_ANGLE_TO_SHORT_NEGATIVE_SCALE));
        const int64_t convertedAngle = x87f_store_i64_trunc(scaledAngle);
        const int32_t negativeEncodedAngle = coduo_int32_from_bits((uint32_t)(uint64_t)convertedAngle);
#else
        const int32_t negativeEncodedAngle =
            coduo_fp_to_i32_extended((long double)playerState->viewAngles[axis] * (long double)MSG_ANGLE_TO_SHORT_NEGATIVE_SCALE);
#endif
        command->angles[axis] = (-playerState->deltaAngles[axis] - negativeEncodedAngle) & UINT16_MAX;
#else
#if EMULATE_X87
        const int32_t encodedAngle =
            x87f_store_i32_trunc(x87f_mul(x87f_load_f32(playerState->viewAngles[axis]), x87f_load_f32(MSG_ANGLE_TO_SHORT_SCALE)));
#else
        const int32_t encodedAngle =
            coduo_fp_to_i32_extended((long double)playerState->viewAngles[axis] * (long double)MSG_ANGLE_TO_SHORT_SCALE);
#endif
        command->angles[axis] = (encodedAngle - playerState->deltaAngles[axis]) & UINT16_MAX;
#endif
    }

    if ((playerState->playerStateFlags & PSF_ACTIVE_PLAYER) == 0)
        return;

    if ((playerState->entityStateFlags & EF_PRONE) != 0) {
        command->wbuttons |= PM_WBUTTON_PRONE;
        command->upmove = MSG_USERCMD_STANCE_UPMOVE;
    } else if ((playerState->entityStateFlags & EF_CROUCHING) != 0) {
        command->wbuttons |= PM_WBUTTON_CROUCH;
        command->upmove = MSG_USERCMD_STANCE_UPMOVE;
    }

    if (playerState->leanFraction > 0.0f)
        command->wbuttons |= PM_WBUTTON_LEAN_RIGHT;
    else if (playerState->leanFraction < 0.0f)
        command->wbuttons |= PM_WBUTTON_LEAN_LEFT;

    if (playerState->adsFraction != 0.0f)
        command->buttons |= PM_BUTTON_ADS;
    if ((playerState->playerStateFlags & PMF_SPRINTING) != 0)
        command->buttons |= PM_BUTTON_SPRINT;
}

/* Source: CoDUOMP.exe 0x0044a2c0..0x0044a2e9.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_0044a2c0_0044a2ea.mcode.
 * Name and signature: exact same-module Mac symbol MSG_HorMoveFrom. */
uint32_t MSG_HorMoveFrom(int32_t forwardMove, int32_t rightMove)
{
    uint32_t packedMove = 0;
    if (forwardMove > MSG_SMALL_MOVE_LIMIT)
        packedMove = MSG_MOVE_POSITIVE_FLAG;
    else if (forwardMove < -MSG_SMALL_MOVE_LIMIT)
        packedMove = MSG_MOVE_NEGATIVE_FLAG;

    if (rightMove > MSG_SMALL_MOVE_LIMIT)
        packedMove |= MSG_HOR_MOVE_RIGHT_POSITIVE;
    else if (rightMove < -MSG_SMALL_MOVE_LIMIT)
        packedMove |= MSG_HOR_MOVE_RIGHT_NEGATIVE;
    return packedMove;
}

/* Source: CoDUOMP.exe 0x0044a2f0..0x0044a307.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_0044a2f0_0044a308.mcode.
 * Name and signature: exact same-module Mac symbol MSG_VertMoveFrom. */
uint32_t MSG_VertMoveFrom(int32_t upMove)
{
    if (upMove > MSG_SMALL_MOVE_LIMIT)
        return MSG_MOVE_POSITIVE_FLAG;
    if (upMove < -MSG_SMALL_MOVE_LIMIT)
        return MSG_MOVE_NEGATIVE_FLAG;
    return 0;
}

/* Source: CoDUOMP.exe 0x0044a310..0x0044a339.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_0044a310_0044a33a.mcode.
 * Name and signature: exact same-module Mac symbol MSG_HorMoveTo. */
void MSG_HorMoveTo(uint32_t packedMove, int8_t *forwardMove, int8_t *rightMove)
{
    if ((packedMove & MSG_MOVE_POSITIVE_FLAG) != 0)
        *forwardMove = MSG_SMALL_MOVE_POSITIVE;
    else if ((packedMove & MSG_MOVE_NEGATIVE_FLAG) != 0)
        *forwardMove = MSG_SMALL_MOVE_NEGATIVE;
    else
        *forwardMove = 0;

    if ((packedMove & MSG_HOR_MOVE_RIGHT_POSITIVE) != 0)
        *rightMove = MSG_SMALL_MOVE_POSITIVE;
    else if ((packedMove & MSG_HOR_MOVE_RIGHT_NEGATIVE) != 0)
        *rightMove = MSG_SMALL_MOVE_NEGATIVE;
    else
        *rightMove = 0;
}

/* Source: CoDUOMP.exe 0x0044a340..0x0044a352.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_0044a340_0044a353.mcode.
 * Name and signature: exact same-module Mac symbol MSG_VertMoveTo. */
void MSG_VertMoveTo(uint32_t packedMove, int8_t *upMove)
{
    if ((packedMove & MSG_MOVE_POSITIVE_FLAG) != 0)
        *upMove = MSG_SMALL_MOVE_POSITIVE;
    else if ((packedMove & MSG_MOVE_NEGATIVE_FLAG) != 0)
        *upMove = MSG_SMALL_MOVE_NEGATIVE;
    else
        *upMove = 0;
}

/* Source: CoDUOMP.exe 0x0044a360..0x0044a76b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0044a360_0044a770.mcode.
 * Name and signature: exact same-module Mac symbol MSG_WriteDeltaUsercmdKey.
 * The two control bits select unchanged, the compact pitch/yaw/horizontal
 * form, or the complete command form. */
void MSG_WriteDeltaUsercmdKey(msg_t *message, uint32_t key, const usercmd_t *from, const usercmd_t *to)
{
    const uint32_t oldHorizontalMove = MSG_HorMoveFrom(from->forwardmove, from->rightmove);
    const uint32_t newHorizontalMove = MSG_HorMoveFrom(to->forwardmove, to->rightmove);
    const uint32_t oldVerticalMove = MSG_VertMoveFrom(from->upmove);
    const uint32_t newVerticalMove = MSG_VertMoveFrom(to->upmove);

    const uint32_t commandTimeDelta = (uint32_t)to->commandTime - (uint32_t)from->commandTime;
    if (commandTimeDelta < MSG_USERCMD_TIME_DELTA_LIMIT) {
        MSG_WriteBit1(message);
        MSG_WriteByte(message, (int32_t)commandTimeDelta);
    } else {
        MSG_WriteBit0(message);
        MSG_WriteLong(message, to->commandTime);
    }

    const qboolean commonFieldsUnchanged = (from->buttons >> MSG_USERCMD_CONTROL_BITS) == (to->buttons >> MSG_USERCMD_CONTROL_BITS) &&
                                           from->wbuttons == to->wbuttons && from->weapon == to->weapon &&
                                           from->angles[2] == to->angles[2] && oldVerticalMove == newVerticalMove;
    if (commonFieldsUnchanged) {
        const qboolean allFieldsUnchanged = from->angles[0] == to->angles[0] && from->angles[1] == to->angles[1] &&
                                            (from->buttons & MSG_USERCMD_FIRE_BUTTON) == (to->buttons & MSG_USERCMD_FIRE_BUTTON) &&
                                            oldHorizontalMove == newHorizontalMove;
        if (allFieldsUnchanged) {
            MSG_WriteKey(message, key, qfalse, MSG_USERCMD_CONTROL_BITS);
            return;
        }

        MSG_WriteKey(message, key, qtrue, MSG_USERCMD_CONTROL_BITS);
        MSG_WriteKey(message, key, qfalse, MSG_USERCMD_CONTROL_BITS);
        key ^= (uint32_t)to->commandTime;
        MSG_WriteKey(message, key, to->buttons & MSG_USERCMD_FIRE_BUTTON, MSG_USERCMD_CONTROL_BITS);
        MSG_WriteDeltaKeyShort(message, key, (int16_t)from->angles[0], (uint32_t)to->angles[0]);
        MSG_WriteDeltaKeyShort(message, key, (int16_t)from->angles[1], (uint32_t)to->angles[1]);
        MSG_WriteDeltaKey(message, key, (int32_t)oldHorizontalMove, (int32_t)newHorizontalMove, MSG_USERCMD_HORIZONTAL_MOVE_BITS);
        return;
    }

    MSG_WriteKey(message, key, qtrue, MSG_USERCMD_CONTROL_BITS);
    MSG_WriteKey(message, key, qtrue, MSG_USERCMD_CONTROL_BITS);
    MSG_WriteKey(message, key, to->buttons & MSG_USERCMD_FIRE_BUTTON, MSG_USERCMD_CONTROL_BITS);
    MSG_WriteDeltaKeyShort(message, key, (int16_t)from->angles[0], (uint32_t)to->angles[0]);
    MSG_WriteDeltaKeyShort(message, key, (int16_t)from->angles[1], (uint32_t)to->angles[1]);
    MSG_WriteDeltaKey(message, key, (int32_t)oldHorizontalMove, (int32_t)newHorizontalMove, MSG_USERCMD_HORIZONTAL_MOVE_BITS);

    key ^= (uint32_t)to->commandTime;
    MSG_WriteDeltaKeyShort(message, key, (int16_t)from->angles[2], (uint32_t)to->angles[2]);
    MSG_WriteDeltaKey(message, key, from->buttons >> MSG_USERCMD_CONTROL_BITS, to->buttons >> MSG_USERCMD_CONTROL_BITS,
                      MSG_USERCMD_BUTTON_HIGH_BITS);
    MSG_WriteDeltaKeyByte(message, key, (int8_t)from->wbuttons, to->wbuttons);
    MSG_WriteDeltaKey(message, key, (int32_t)oldVerticalMove, (int32_t)newVerticalMove, MSG_USERCMD_VERTICAL_MOVE_BITS);
    MSG_WriteDeltaKey(message, key, from->weapon, to->weapon, MSG_USERCMD_WEAPON_BITS);
}

/* Source: CoDUOMP.exe 0x0044a770..0x0044aa55.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0044a770_0044aa56.mcode.
 * Name and signature: exact same-module Mac symbol MSG_ReadDeltaUsercmdKey.
 * The destination begins as an exact copy of the base command, after which
 * only fields selected by the keyed control bits are replaced. */
void MSG_ReadDeltaUsercmdKey(msg_t *message, uint32_t key, const usercmd_t *from, usercmd_t *to)
{
    memcpy(to, from, sizeof(*to));

    if (MSG_ReadBit(message) == 0)
        to->commandTime = MSG_ReadLong(message);
    else
        to->commandTime = (int32_t)((uint32_t)from->commandTime + (uint32_t)MSG_ReadByte(message));

    if (MSG_ReadKey(message, key, MSG_USERCMD_CONTROL_BITS) == qfalse)
        return;

    to->buttons &= (uint8_t)~MSG_USERCMD_FIRE_BUTTON;
    if (MSG_ReadKey(message, key, MSG_USERCMD_CONTROL_BITS) == qfalse) {
        key ^= (uint32_t)to->commandTime;
        to->buttons |= (uint8_t)MSG_ReadKey(message, key, MSG_USERCMD_CONTROL_BITS);
        to->angles[0] = MSG_ReadDeltaKeyShort(message, (uint16_t)key, from->angles[0]) & UINT16_MAX;
        to->angles[1] = MSG_ReadDeltaKeyShort(message, (uint16_t)key, from->angles[1]) & UINT16_MAX;
        MSG_HorMoveTo((uint32_t)MSG_ReadDeltaKey(message, key, (int32_t)MSG_HorMoveFrom(from->forwardmove, from->rightmove),
                                                 MSG_USERCMD_HORIZONTAL_MOVE_BITS),
                      &to->forwardmove, &to->rightmove);
        return;
    }

    to->buttons |= (uint8_t)MSG_ReadKey(message, key, MSG_USERCMD_CONTROL_BITS);
    to->angles[0] = MSG_ReadDeltaKeyShort(message, (uint16_t)key, from->angles[0]) & UINT16_MAX;
    to->angles[1] = MSG_ReadDeltaKeyShort(message, (uint16_t)key, from->angles[1]) & UINT16_MAX;
    MSG_HorMoveTo((uint32_t)MSG_ReadDeltaKey(message, key, (int32_t)MSG_HorMoveFrom(from->forwardmove, from->rightmove),
                                             MSG_USERCMD_HORIZONTAL_MOVE_BITS),
                  &to->forwardmove, &to->rightmove);

    key ^= (uint32_t)to->commandTime;
    to->angles[2] = MSG_ReadDeltaKeyShort(message, (uint16_t)key, from->angles[2]) & UINT16_MAX;
    to->buttons = (uint8_t)((MSG_ReadDeltaKey(message, key, from->buttons >> MSG_USERCMD_CONTROL_BITS, MSG_USERCMD_BUTTON_HIGH_BITS)
                             << MSG_USERCMD_CONTROL_BITS) |
                            (to->buttons & MSG_USERCMD_FIRE_BUTTON));
    to->wbuttons = (uint8_t)MSG_ReadDeltaKeyByte(message, (uint8_t)key, from->wbuttons);
    MSG_VertMoveTo((uint32_t)MSG_ReadDeltaKey(message, key, (int32_t)MSG_VertMoveFrom(from->upmove), MSG_USERCMD_VERTICAL_MOVE_BITS),
                   &to->upmove);
    to->weapon = (uint8_t)MSG_ReadDeltaKey(message, key, from->weapon, MSG_USERCMD_WEAPON_BITS);
}

/* Source: CoDUOMP.exe 0x0044aa60..0x0044abe7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0044aa60_0044abe8.mcode.
 * Name and descriptor role: exact same-module Mac symbol MSG_WriteDeltaField;
 * the Windows loads prove the descriptor's offset and bit-mode members. */
void MSG_WriteDeltaField(msg_t *message, const void *from, const void *to, const netField_t *field)
{
    int32_t oldValue;
    int32_t newValue;

    /* Descriptor-driven records are the intentional raw-data boundary here:
     * every described field is a four-byte scalar at a byte offset. */
    memcpy(&oldValue, (const uint8_t *)from + field->offset, sizeof(oldValue));
    memcpy(&newValue, (const uint8_t *)to + field->offset, sizeof(newValue));

    if (oldValue == newValue) {
        MSG_WriteBit0(message);
        return;
    }

    MSG_WriteBit1(message);
    if (field->bits == MSG_DELTA_DEFAULT_FLOAT_BITS) {
        float newFloat;
        int32_t rounded;
        int32_t biased;

        memcpy(&newFloat, &newValue, sizeof(newFloat));
#if EMULATE_X87
        const x87f convertedFloat = x87f_load_f32(newFloat);
#if defined(WINDOWS_BEHAVIOR)
        const int64_t convertedInteger = x87f_store_i64_trunc(convertedFloat);
        rounded = coduo_int32_from_bits((uint32_t)(uint64_t)convertedInteger);
#else
        rounded = x87f_store_i32_trunc(convertedFloat);
#endif
#else
        rounded = coduo_fp_to_i32_extended((long double)newFloat);
#endif

        if (newFloat == 0.0f) {
            MSG_WriteBit0(message);
            return;
        }

        MSG_WriteBit1(message);
        biased = (int32_t)((uint32_t)rounded + MSG_DELTA_SMALL_INT_BIAS);
        if ((float)rounded == newFloat && biased >= 0 && biased < MSG_DELTA_SMALL_INT_RANGE) {
            MSG_WriteBit0(message);
            MSG_WriteBits(message, biased, MSG_DELTA_SMALL_INT_LOW_BITS);
            MSG_WriteByte(message, biased >> MSG_DELTA_SMALL_INT_LOW_BITS);
        } else {
            MSG_WriteBit1(message);
            MSG_WriteLong(message, newValue);
        }
        return;
    }

    if (field->bits == MSG_DELTA_ANGLE16_BITS) {
        float newFloat;

        if (newValue == 0) {
            MSG_WriteBit0(message);
            return;
        }

        memcpy(&newFloat, &newValue, sizeof(newFloat));
        MSG_WriteBit1(message);
        MSG_WriteAngle16(message, newFloat);
        return;
    }

    if (newValue == 0) {
        MSG_WriteBit0(message);
        return;
    }

    MSG_WriteBit1(message);
    {
        uint32_t bitCount = field->bits < 0 ? 0u - (uint32_t)field->bits : (uint32_t)field->bits;
        uint32_t remaining = bitCount;
        const uint32_t lowBits = bitCount & MSG_DELTA_BYTE_BIT_MASK;
        int32_t value = newValue;

        if (lowBits != 0) {
            MSG_WriteBits(message, value, (int32_t)lowBits);
            value = coduo_int32_sar(coduo_int32_bits(value), lowBits);
            remaining -= lowBits;
        }
        while (remaining != 0) {
            MSG_WriteByte(message, value);
            value = coduo_int32_sar(coduo_int32_bits(value), MSG_DELTA_BYTE_BITS);
            remaining -= MSG_DELTA_BYTE_BITS;
        }
    }
}

/* Source: CoDUOMP.exe 0x0044abf0..0x0044accc.
 * Evidence: direct PE disassembly of a function omitted by Ghidra's original
 * function inventory. Name and signature: exact same-module Mac symbol
 * MSG_WriteDeltaFields. */
void MSG_WriteDeltaFields(msg_t *message, const void *from, const void *to, qboolean force, int32_t count, const netField_t *fields)
{
    if (force == qfalse) {
        int32_t fieldIndex;

        for (fieldIndex = 0; fieldIndex < count; ++fieldIndex) {
            int32_t oldValue;
            int32_t newValue;

            memcpy(&oldValue, (const uint8_t *)from + fields[fieldIndex].offset, sizeof(oldValue));
            memcpy(&newValue, (const uint8_t *)to + fields[fieldIndex].offset, sizeof(newValue));
            if (oldValue != newValue)
                break;
        }
        if (fieldIndex >= count) {
            MSG_WriteBit0(message);
            return;
        }
    }

    MSG_WriteBit1(message);
    for (int32_t fieldIndex = 0; fieldIndex < count; ++fieldIndex)
        MSG_WriteDeltaField(message, from, to, &fields[fieldIndex]);
}

/* Source: CoDUOMP.exe 0x0044acd0..0x0044aefa.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0044acd0_0044aefb.mcode.
 * Name and signature: exact same-module Mac symbol MSG_WriteDeltaStruct;
 * Windows call sites prove the descriptor and callback argument ordering. */
void MSG_WriteDeltaStruct(msg_t *message, const void *from, const void *to, qboolean force, int32_t count, int32_t numberBits,
                          const netField_t *fields, msg_delta_extra_writer_t extraWriter, qboolean writeForceBit)
{
    int32_t recordNumber;

    if (to == NULL) {
        memcpy(&recordNumber, from, sizeof(recordNumber));
        if (cl_shownet != NULL && (cl_shownet->integer >= MSG_SHOWNET_DELTA_STRUCT || cl_shownet->integer == MSG_SHOWNET_ALL)) {
            Com_Printf("W|%3i: #%-3i remove\n", message->cursize, recordNumber);
        }
        if (writeForceBit != qfalse)
            MSG_WriteBit1(message);
        MSG_WriteBits(message, recordNumber, numberBits);
        if (extraWriter != NULL)
            extraWriter(message, NULL);
        MSG_WriteBit1(message);
        return;
    }

    int32_t lastChanged = 0;
    for (int32_t fieldIndex = 0; fieldIndex < count; ++fieldIndex) {
        int32_t oldValue;
        int32_t newValue;

        memcpy(&oldValue, (const uint8_t *)from + fields[fieldIndex].offset, sizeof(oldValue));
        memcpy(&newValue, (const uint8_t *)to + fields[fieldIndex].offset, sizeof(newValue));
        if (oldValue != newValue)
            lastChanged = fieldIndex + 1;
    }

    if (lastChanged == 0) {
        if (force == qfalse)
            return;

        memcpy(&recordNumber, to, sizeof(recordNumber));
        if (writeForceBit != qfalse)
            MSG_WriteBit1(message);
        MSG_WriteBits(message, recordNumber, numberBits);
        if (extraWriter != NULL)
            extraWriter(message, to);
        MSG_WriteBit0(message);
        MSG_WriteBit0(message);
        return;
    }

    memcpy(&recordNumber, to, sizeof(recordNumber));
    if (writeForceBit != qfalse)
        MSG_WriteBit1(message);
    MSG_WriteBits(message, recordNumber, numberBits);
    if (extraWriter != NULL)
        extraWriter(message, to);
    MSG_WriteBit0(message);
    MSG_WriteBit1(message);
    MSG_WriteByte(message, lastChanged);
    for (int32_t fieldIndex = 0; fieldIndex < lastChanged; ++fieldIndex)
        MSG_WriteDeltaField(message, from, to, &fields[fieldIndex]);
}

/* Source: CoDUOMP.exe 0x0044af00..0x0044af4f.
 * Evidence: repaired executable-gap record. Exact same-module Mac symbol
 * MSG_WriteDeltaEntity_ChangedCallback. The extra bit selects the vehicle
 * entity netfield table on the corresponding reader path. */
void MSG_WriteDeltaEntity_ChangedCallback(msg_t *message, const void *entityState)
{
    if (entityState != NULL && ((const entityState_t *)entityState)->eType == ET_VEHICLE) {
        MSG_WriteBit1(message);
    } else {
        MSG_WriteBit0(message);
    }
}

/* Source: CoDUOMP.exe 0x0044af50..0x0044afb8.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0044af50_0044afb9.mcode.
 * Name and signature: exact same-module Mac symbol MSG_WriteDeltaEntity. */
void MSG_WriteDeltaEntity(msg_t *message, const entityState_t *from, const entityState_t *to, qboolean force)
{
    const netField_t *fields = msg_defaultEntityNetFields;

    if (to != NULL && to->eType == ET_VEHICLE)
        fields = msg_vehicleEntityNetFields;

    MSG_WriteDeltaStruct(message, from, to, force, MSG_ENTITY_NETFIELD_COUNT, MSG_ENTITY_NUMBER_BITS, fields,
                         MSG_WriteDeltaEntity_ChangedCallback, qfalse);
}

/* Source: CoDUOMP.exe 0x0044afc0..0x0044afdb.
 * Evidence: repaired executable-gap record. Name and signature: exact
 * same-module Mac symbol MSG_WriteDeltaArchivedEntity. */
void MSG_WriteDeltaArchivedEntity(msg_t *message, const archivedEntity_t *from, const archivedEntity_t *to, qboolean force)
{
    MSG_WriteDeltaStruct(message, from, to, force, MSG_ARCHIVED_ENTITY_NETFIELD_COUNT, MSG_ENTITY_NUMBER_BITS, msg_archivedEntityNetFields,
                         NULL, qfalse);
}

/* Source: CoDUOMP.exe 0x0044afe0..0x0044b02d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0044afe0_0044b02e.mcode.
 * Name and signature: exact same-module Mac symbol MSG_WriteDeltaClient. The
 * MSVC stack-cookie epilogue is compiler support, not maintained source. */
void MSG_WriteDeltaClient(msg_t *message, const clientState_t *from, const clientState_t *to, qboolean force)
{
    clientState_t nullClient;

    if (from == NULL) {
        memset(&nullClient, 0, sizeof(nullClient));
        from = &nullClient;
    }

    MSG_WriteDeltaStruct(message, from, to, force, MSG_CLIENT_NETFIELD_COUNT, MSG_CLIENT_NUMBER_BITS, msg_clientNetFields, NULL, qtrue);
}

/* Source: CoDUOMP.exe 0x0044b030..0x0044b1df.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0044b030_0044b1e0.mcode.
 * Name and signature: exact same-module Mac symbol MSG_ReadDeltaField. */
void MSG_ReadDeltaField(msg_t *message, const void *from, void *to, const netField_t *field, qboolean print)
{
    uint8_t *destination = (uint8_t *)to + field->offset;

    if (MSG_ReadBit(message) == 0) {
        memcpy(destination, (const uint8_t *)from + field->offset, sizeof(int32_t));
        return;
    }

    if (field->bits == MSG_DELTA_DEFAULT_FLOAT_BITS) {
        float value;

        if (MSG_ReadBit(message) == 0) {
            value = 0.0f;
        } else if (MSG_ReadBit(message) == 0) {
            /* The low bits are consumed from the stream before the byte;
             * sequence the two reads so no operand-order choice can swap
             * them. */
            const int32_t integralLowBits = MSG_ReadBits(message, MSG_DELTA_SMALL_INT_LOW_BITS);
            const int32_t integralValue = integralLowBits + (MSG_ReadByte(message) - 128) * (1 << MSG_DELTA_SMALL_INT_LOW_BITS);

            value = (float)integralValue;
            memcpy(destination, &value, sizeof(value));
            if (print != qfalse)
                Com_Printf("%s:%i ", field->name, integralValue);
            return;
        } else {
            const int32_t valueBits = MSG_ReadLong(message);

            memcpy(&value, &valueBits, sizeof(value));
            memcpy(destination, &value, sizeof(value));
            if (print != qfalse)
                Com_Printf("%s:%f ", field->name, (double)value);
            return;
        }

        memcpy(destination, &value, sizeof(value));
        return;
    }

    if (field->bits == MSG_DELTA_ANGLE16_BITS) {
        const float value = MSG_ReadBit(message) == 0 ? 0.0f : MSG_ReadAngle16(message);

        memcpy(destination, &value, sizeof(value));
        return;
    }

    if (MSG_ReadBit(message) == 0) {
        const int32_t value = 0;

        memcpy(destination, &value, sizeof(value));
        return;
    }

    {
        const uint32_t bitCount = field->bits < 0 ? 0u - (uint32_t)field->bits : (uint32_t)field->bits;
        const uint32_t lowBits = bitCount & MSG_DELTA_BYTE_BIT_MASK;
        uint32_t value = lowBits == 0 ? 0u : (uint32_t)MSG_ReadBits(message, (int32_t)lowBits);

        for (uint32_t bit = lowBits; bit < bitCount; bit += MSG_DELTA_BYTE_BITS) {
            value |= (uint32_t)MSG_ReadByte(message) << (bit & 31u);
        }
        memcpy(destination, &value, sizeof(value));
        if (print != qfalse)
            Com_Printf("%s:%i ", field->name, (int32_t)value);
    }
}

/* Source: CoDUOMP.exe 0x0044b1e0..0x0044b276.
 * Evidence: repaired executable-gap record. Name and signature: exact
 * same-module Mac symbol MSG_ReadDeltaFields. */
void MSG_ReadDeltaFields(msg_t *message, const void *from, void *to, int32_t count, const netField_t *fields)
{
    if (MSG_ReadBit(message) != 0) {
        for (int32_t fieldIndex = 0; fieldIndex < count; ++fieldIndex) {
            MSG_ReadDeltaField(message, from, to, &fields[fieldIndex], qfalse);
        }
        return;
    }

    for (int32_t fieldIndex = 0; fieldIndex < count; ++fieldIndex) {
        memcpy((uint8_t *)to + fields[fieldIndex].offset, (const uint8_t *)from + fields[fieldIndex].offset, sizeof(int32_t));
    }
}

/* Source: CoDUOMP.exe 0x0044b280..0x0044b41c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0044b280_0044b41d.mcode.
 * Name and signature: exact same-module Mac symbol MSG_ReadDeltaStruct. */
qboolean MSG_ReadDeltaStruct(msg_t *message, const void *from, void *to, int32_t number, int32_t count, const netField_t *fields)
{
    if (MSG_ReadBit(message) != 0) {
        if (cl_shownet != NULL && (cl_shownet->integer >= MSG_SHOWNET_DELTA_STRUCT || cl_shownet->integer == MSG_SHOWNET_ALL)) {
            Com_Printf("%3i: #%-3i remove\n", message->readcount, number);
        }
        return qtrue;
    }

    if (MSG_ReadBit(message) == 0) {
        memcpy(to, from, (size_t)count * sizeof(int32_t) + sizeof(int32_t));
        return qfalse;
    }

    int32_t lastChanged = MSG_ReadByte(message);
    /* NOT_FROM_ORIGINAL_SOURCE: require a decoded field count before forming
     * any descriptor pointer. */
    if (lastChanged < 0) {
        Com_Error(ERR_DROP, "\x15"
                            "MSG_ReadDeltaStruct: truncated field count");
        return qtrue;
    }
    if (lastChanged > count)
        lastChanged = count;

    qboolean print = qfalse;
    if (cl_shownet != NULL && (cl_shownet->integer >= MSG_SHOWNET_DELTA_STRUCT || cl_shownet->integer == MSG_SHOWNET_ALL)) {
        int32_t displayedNumber;

        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        memcpy(&displayedNumber, to, sizeof(displayedNumber));
        print = qtrue;
        Com_Printf("%3i: #%-3i ", message->readcount, displayedNumber);
    }

    memcpy(to, &number, sizeof(number));
    for (int32_t fieldIndex = 0; fieldIndex < lastChanged; ++fieldIndex) {
        MSG_ReadDeltaField(message, from, to, &fields[fieldIndex], print);
    }
    for (int32_t fieldIndex = lastChanged; fieldIndex < count; ++fieldIndex) {
        memcpy((uint8_t *)to + fields[fieldIndex].offset, (const uint8_t *)from + fields[fieldIndex].offset, sizeof(int32_t));
    }
    return qfalse;
}

/* Source: CoDUOMP.exe 0x0044b420..0x0044b493.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0044b420_0044b494.mcode.
 * Name and signature: exact same-module Mac symbol MSG_ReadDeltaEntity. */
qboolean MSG_ReadDeltaEntity(msg_t *message, const entityState_t *from, entityState_t *to, int32_t number)
{
    const netField_t *fields = MSG_ReadBit(message) != 0 ? msg_vehicleEntityNetFields : msg_defaultEntityNetFields;

    return MSG_ReadDeltaStruct(message, from, to, number, MSG_ENTITY_NETFIELD_COUNT, fields);
}

/* Source: CoDUOMP.exe 0x0044b4a0..0x0044b4b8.
 * Evidence: repaired executable-gap record. Name and signature: exact
 * same-module Mac symbol MSG_ReadDeltaArchivedEntity. */
qboolean MSG_ReadDeltaArchivedEntity(msg_t *message, const archivedEntity_t *from, archivedEntity_t *to, int32_t number)
{
    return MSG_ReadDeltaStruct(message, from, to, number, MSG_ARCHIVED_ENTITY_NETFIELD_COUNT, msg_archivedEntityNetFields);
}

/* Source: CoDUOMP.exe 0x0044b4c0..0x0044b504.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0044b4c0_0044b505.mcode.
 * Name and signature: exact same-module Mac symbol MSG_ReadDeltaClient. The
 * MSVC stack-cookie epilogue is compiler support, not maintained source. */
qboolean MSG_ReadDeltaClient(msg_t *message, const clientState_t *from, clientState_t *to, int32_t clientNum)
{
    clientState_t nullClient;

    if (from == NULL) {
        memset(&nullClient, 0, sizeof(nullClient));
        from = &nullClient;
    }

    return MSG_ReadDeltaStruct(message, from, to, clientNum, MSG_CLIENT_NETFIELD_COUNT, msg_clientNetFields);
}

/* Source: CoDUOMP.exe 0x0044b510..0x0044b686.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0044b510_0044b687.mcode.
 * Name and signature: exact same-module Mac symbol MSG_WriteDeltaHudElems. */
void MSG_WriteDeltaHudElems(msg_t *message, const hudElem_t *from, const hudElem_t *to, int32_t maxHudElems)
{
    int32_t hudElemCount = 0;

    while (hudElemCount < maxHudElems && to[hudElemCount].type != HE_TYPE_NONE) {
        ++hudElemCount;
    }
    MSG_WriteBits(message, hudElemCount, MSG_HUD_ELEM_COUNT_BITS);

    for (int32_t elemIndex = 0; elemIndex < hudElemCount; ++elemIndex) {
        int32_t lastChanged = 0;

        for (int32_t fieldIndex = 0; fieldIndex < MSG_HUD_ELEM_NETFIELD_COUNT; ++fieldIndex) {
            int32_t oldValue;
            int32_t newValue;

            memcpy(&oldValue, (const uint8_t *)&from[elemIndex] + msg_hudElemNetFields[fieldIndex].offset, sizeof(oldValue));
            memcpy(&newValue, (const uint8_t *)&to[elemIndex] + msg_hudElemNetFields[fieldIndex].offset, sizeof(newValue));
            if (oldValue != newValue)
                lastChanged = fieldIndex;
        }

        MSG_WriteBits(message, lastChanged, MSG_HUD_ELEM_LAST_FIELD_BITS);
        for (int32_t fieldIndex = 0; fieldIndex <= lastChanged; ++fieldIndex) {
            MSG_WriteDeltaField(message, &from[elemIndex], &to[elemIndex], &msg_hudElemNetFields[fieldIndex]);
        }
    }
}

/* Source: CoDUOMP.exe 0x0044b690..0x0044b7a9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0044b690_0044b7aa.mcode.
 * Name and signature: exact same-module Mac symbol MSG_ReadDeltaHudElems. */
void MSG_ReadDeltaHudElems(msg_t *message, const hudElem_t *from, hudElem_t *to, int32_t maxHudElems)
{
    const int32_t hudElemCount = MSG_ReadBits(message, MSG_HUD_ELEM_COUNT_BITS);

    for (int32_t elemIndex = 0; elemIndex < hudElemCount; ++elemIndex) {
        const int32_t lastChanged = MSG_ReadBits(message, MSG_HUD_ELEM_LAST_FIELD_BITS);

        /* NOT_FROM_ORIGINAL_SOURCE: the decoded last-field index must belong
         * to the HUD descriptor table before traversal begins. */
        if (lastChanged >= MSG_HUD_ELEM_NETFIELD_COUNT) {
            Com_Error(ERR_DROP,
                      "\x15"
                      "MSG_ReadDeltaHudElems: invalid field index %i",
                      lastChanged);
            return;
        }
        for (int32_t fieldIndex = 0; fieldIndex <= lastChanged; ++fieldIndex) {
            MSG_ReadDeltaField(message, &from[elemIndex], &to[elemIndex], &msg_hudElemNetFields[fieldIndex], qfalse);
        }
        for (int32_t fieldIndex = lastChanged + 1; fieldIndex < MSG_HUD_ELEM_NETFIELD_COUNT; ++fieldIndex) {
            const netField_t *field = &msg_hudElemNetFields[fieldIndex];

            memcpy((uint8_t *)&to[elemIndex] + field->offset, (const uint8_t *)&from[elemIndex] + field->offset, sizeof(int32_t));
        }
    }

    memset(&to[hudElemCount], 0, (size_t)(maxHudElems - hudElemCount) * sizeof(*to));
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the player-state field
 * encoding inlined by MSVC throughout MSG_WriteDeltaPlayerstate. Unlike the
 * generic struct codec, player-state float fields have no separate zero bit. */
static void coduomp_msg_write_delta_playerstate_field(msg_t *message, const playerState_t *from, const playerState_t *to,
                                                      const netField_t *field)
{
    int32_t oldValue;
    int32_t newValue;

    memcpy(&oldValue, (const uint8_t *)from + field->offset, sizeof(oldValue));
    memcpy(&newValue, (const uint8_t *)to + field->offset, sizeof(newValue));
    if (oldValue == newValue) {
        MSG_WriteBit0(message);
        return;
    }

    MSG_WriteBit1(message);
    if (field->bits == MSG_DELTA_DEFAULT_FLOAT_BITS) {
        float newFloat;
        int32_t rounded;

        memcpy(&newFloat, &newValue, sizeof(newFloat));
#if EMULATE_X87
        const x87f convertedFloat = x87f_load_f32(newFloat);
#if defined(WINDOWS_BEHAVIOR)
        const int64_t convertedInteger = x87f_store_i64_trunc(convertedFloat);
        rounded = coduo_int32_from_bits((uint32_t)(uint64_t)convertedInteger);
#else
        rounded = x87f_store_i32_trunc(convertedFloat);
#endif
#else
        rounded = coduo_fp_to_i32_extended((long double)newFloat);
#endif

        const int32_t biased = (int32_t)((uint32_t)rounded + MSG_DELTA_SMALL_INT_BIAS);
        if ((float)rounded == newFloat && biased >= 0 && biased < MSG_DELTA_SMALL_INT_RANGE) {
            MSG_WriteBit0(message);
            MSG_WriteBits(message, biased, MSG_DELTA_SMALL_INT_LOW_BITS);
            MSG_WriteByte(message, biased >> MSG_DELTA_SMALL_INT_LOW_BITS);
        } else {
            MSG_WriteBit1(message);
            MSG_WriteLong(message, newValue);
        }
        return;
    }

    if (field->bits == MSG_DELTA_ANGLE16_BITS) {
        if (newValue == 0) {
            MSG_WriteBit0(message);
        } else {
            float angle;

            memcpy(&angle, &newValue, sizeof(angle));
            MSG_WriteBit1(message);
            MSG_WriteAngle16(message, angle);
        }
        return;
    }

    uint32_t bitCount = field->bits < 0 ? 0u - (uint32_t)field->bits : (uint32_t)field->bits;
    int32_t value = newValue;
    const uint32_t lowBits = bitCount & MSG_DELTA_BYTE_BIT_MASK;

    if (lowBits != 0) {
        MSG_WriteBits(message, value, (int32_t)lowBits);
        value = coduo_int32_sar(coduo_int32_bits(value), lowBits);
        bitCount -= lowBits;
    }
    while (bitCount != 0) {
        MSG_WriteByte(message, value);
        value = coduo_int32_sar(coduo_int32_bits(value), MSG_DELTA_BYTE_BITS);
        bitCount -= MSG_DELTA_BYTE_BITS;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the corresponding
 * player-state field decoder inlined by MSG_ReadDeltaPlayerstate. */
static void coduomp_msg_read_delta_playerstate_field(msg_t *message, const playerState_t *from, playerState_t *to, const netField_t *field,
                                                     qboolean print)
{
    uint8_t *destination = (uint8_t *)to + field->offset;

    if (MSG_ReadBit(message) == 0) {
        memcpy(destination, (const uint8_t *)from + field->offset, sizeof(int32_t));
        return;
    }

    if (field->bits == MSG_DELTA_DEFAULT_FLOAT_BITS) {
        float value;

        if (MSG_ReadBit(message) == 0) {
            /* The low bits are consumed from the stream before the byte;
             * sequence the two reads so no operand-order choice can swap
             * them. */
            const int32_t integralLowBits = MSG_ReadBits(message, MSG_DELTA_SMALL_INT_LOW_BITS);
            const int32_t integralValue = integralLowBits + (MSG_ReadByte(message) - 128) * (1 << MSG_DELTA_SMALL_INT_LOW_BITS);

            value = (float)integralValue;
            if (print != qfalse)
                Com_Printf("%s:%i ", field->name, integralValue);
        } else {
            const int32_t valueBits = MSG_ReadLong(message);

            memcpy(&value, &valueBits, sizeof(value));
            if (print != qfalse)
                Com_Printf("%s:%f ", field->name, (double)value);
        }
        memcpy(destination, &value, sizeof(value));
        return;
    }

    if (field->bits == MSG_DELTA_ANGLE16_BITS) {
        const float value = MSG_ReadBit(message) == 0 ? 0.0f : MSG_ReadAngle16(message);

        memcpy(destination, &value, sizeof(value));
        return;
    }

    const uint32_t bitCount = field->bits < 0 ? 0u - (uint32_t)field->bits : (uint32_t)field->bits;
    const uint32_t lowBits = bitCount & MSG_DELTA_BYTE_BIT_MASK;
    uint32_t value = lowBits == 0 ? 0u : (uint32_t)MSG_ReadBits(message, (int32_t)lowBits);

    for (uint32_t bit = lowBits; bit < bitCount; bit += MSG_DELTA_BYTE_BITS) {
        value |= (uint32_t)MSG_ReadByte(message) << (bit & 31u);
    }
    memcpy(destination, &value, sizeof(value));
    if (print != qfalse)
        Com_Printf("%s:%i ", field->name, (int32_t)value);
}

/* Source: CoDUOMP.exe 0x0044b7b0..0x0044c5b1.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0044b7b0_0044c5b2.mcode.
 * Name and signature: exact same-module Mac symbol MSG_WriteDeltaPlayerstate. */
void MSG_WriteDeltaPlayerstate(msg_t *message, const playerState_t *from, const playerState_t *to)
{
    playerState_t nullState;

    if (from == NULL) {
        memset(&nullState, 0, sizeof(nullState));
        from = &nullState;
    }

    int32_t lastChanged = 0;
    for (int32_t fieldIndex = 0; fieldIndex < MSG_PLAYERSTATE_NETFIELD_COUNT; ++fieldIndex) {
        int32_t oldValue;
        int32_t newValue;

        memcpy(&oldValue, (const uint8_t *)from + msg_playerStateNetFields[fieldIndex].offset, sizeof(oldValue));
        memcpy(&newValue, (const uint8_t *)to + msg_playerStateNetFields[fieldIndex].offset, sizeof(newValue));
        if (oldValue != newValue)
            lastChanged = fieldIndex + 1;
    }
    MSG_WriteByte(message, lastChanged);
    for (int32_t fieldIndex = 0; fieldIndex < lastChanged; ++fieldIndex) {
        coduomp_msg_write_delta_playerstate_field(message, from, to, &msg_playerStateNetFields[fieldIndex]);
    }

    uint32_t statBits = 0;
    for (int32_t statIndex = 0; statIndex < PLAYERSTATE_STAT_COUNT; ++statIndex) {
        if (from->stats[statIndex] != to->stats[statIndex])
            statBits |= 1u << statIndex;
    }
    if (statBits == 0) {
        MSG_WriteBit0(message);
    } else {
        MSG_WriteBit1(message);
        MSG_WriteBits(message, (int32_t)statBits, MSG_PLAYERSTATE_STAT_BITS);
        for (int32_t statIndex = 0; statIndex < PLAYERSTATE_STAT_COUNT; ++statIndex) {
            if ((statBits & (1u << statIndex)) == 0)
                continue;
            if (statIndex == MSG_PLAYERSTATE_STAT_COMPACT_INDEX) {
                MSG_WriteBits(message, to->stats[statIndex], MSG_PLAYERSTATE_STAT_COMPACT_BITS);
            } else if (statIndex == MSG_PLAYERSTATE_STAT_BYTE_INDEX) {
                MSG_WriteByte(message, to->stats[statIndex]);
            } else {
                MSG_WriteShort(message, to->stats[statIndex]);
            }
        }
    }

    uint32_t ammoBits[MSG_PLAYERSTATE_AMMO_GROUP_COUNT] = {0};
    for (int32_t group = 0; group < MSG_PLAYERSTATE_AMMO_GROUP_COUNT; ++group) {
        for (int32_t item = 0; item < MSG_PLAYERSTATE_AMMO_GROUP_SIZE; ++item) {
            const int32_t index = group * MSG_PLAYERSTATE_AMMO_GROUP_SIZE + item;
            if (from->ammo[index] != to->ammo[index])
                ammoBits[group] |= 1u << item;
        }
    }
    if ((ammoBits[0] | ammoBits[1] | ammoBits[2] | ammoBits[3]) == 0) {
        MSG_WriteBit0(message);
    } else {
        MSG_WriteBit1(message);
        for (int32_t group = 0; group < MSG_PLAYERSTATE_AMMO_GROUP_COUNT; ++group) {
            if (ammoBits[group] == 0) {
                MSG_WriteBit0(message);
                continue;
            }
            MSG_WriteBit1(message);
            MSG_WriteShort(message, (int32_t)ammoBits[group]);
            for (int32_t item = 0; item < MSG_PLAYERSTATE_AMMO_GROUP_SIZE; ++item) {
                if ((ammoBits[group] & (1u << item)) != 0) {
                    const int32_t index = group * MSG_PLAYERSTATE_AMMO_GROUP_SIZE + item;
                    MSG_WriteShort(message, to->ammo[index]);
                }
            }
        }
    }

    for (int32_t group = 0; group < MSG_PLAYERSTATE_AMMO_GROUP_COUNT; ++group) {
        uint32_t clipBits = 0;
        for (int32_t item = 0; item < MSG_PLAYERSTATE_AMMO_GROUP_SIZE; ++item) {
            const int32_t index = group * MSG_PLAYERSTATE_AMMO_GROUP_SIZE + item;
            if (from->clips[index] != to->clips[index])
                clipBits |= 1u << item;
        }
        if (clipBits == 0) {
            MSG_WriteBit0(message);
            continue;
        }
        MSG_WriteBit1(message);
        MSG_WriteShort(message, (int32_t)clipBits);
        for (int32_t item = 0; item < MSG_PLAYERSTATE_AMMO_GROUP_SIZE; ++item) {
            if ((clipBits & (1u << item)) != 0) {
                const int32_t index = group * MSG_PLAYERSTATE_AMMO_GROUP_SIZE + item;
                MSG_WriteShort(message, to->clips[index]);
            }
        }
    }

    if (memcmp(from->objectives, to->objectives, sizeof(to->objectives)) == 0) {
        MSG_WriteBit0(message);
    } else {
        MSG_WriteBit1(message);
        for (int32_t objectiveIndex = 0; objectiveIndex < PLAYERSTATE_OBJECTIVE_COUNT; ++objectiveIndex) {
            MSG_WriteBits(message, to->objectives[objectiveIndex].state, MSG_OBJECTIVE_STATE_BITS);
            MSG_WriteDeltaFields(message, &from->objectives[objectiveIndex], &to->objectives[objectiveIndex], qfalse,
                                 MSG_OBJECTIVE_NETFIELD_COUNT, msg_objectiveNetFields);
        }
    }

    if (memcmp(from->hudCurrent, to->hudCurrent, sizeof(to->hudCurrent) + sizeof(to->hudArchival)) == 0) {
        MSG_WriteBit0(message);
    } else {
        MSG_WriteBit1(message);
        MSG_WriteDeltaHudElems(message, from->hudArchival, to->hudArchival, PLAYERSTATE_HUD_ELEM_COUNT);
        MSG_WriteDeltaHudElems(message, from->hudCurrent, to->hudCurrent, PLAYERSTATE_HUD_ELEM_COUNT);
    }
}

/* Source: CoDUOMP.exe 0x0044c5c0..0x0044cf71.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0044c5c0_0044cf72.mcode.
 * Name and signature: exact same-module Mac symbol MSG_ReadDeltaPlayerstate. */
void MSG_ReadDeltaPlayerstate(msg_t *message, const playerState_t *from, playerState_t *to)
{
    playerState_t nullState;

    if (from == NULL) {
        memset(&nullState, 0, sizeof(nullState));
        from = &nullState;
    }
    memcpy(to, from, sizeof(*to));

    qboolean print = qfalse;
    if (cl_shownet != NULL && (cl_shownet->integer >= MSG_SHOWNET_DELTA_STRUCT || cl_shownet->integer == MSG_SHOWNET_PLAYERSTATE)) {
        print = qtrue;
        Com_Printf("%3i: playerstate ", message->readcount);
    }

    const int32_t lastChanged = MSG_ReadByte(message);
    /* NOT_FROM_ORIGINAL_SOURCE: the decoded field count must be representable
     * by the player-state descriptor table before traversal begins. */
    if (lastChanged < 0 || lastChanged > MSG_PLAYERSTATE_NETFIELD_COUNT) {
        Com_Error(ERR_DROP,
                  "\x15"
                  "MSG_ReadDeltaPlayerstate: invalid field count %i",
                  lastChanged);
        return;
    }
    for (int32_t fieldIndex = 0; fieldIndex < lastChanged; ++fieldIndex) {
        coduomp_msg_read_delta_playerstate_field(message, from, to, &msg_playerStateNetFields[fieldIndex], print);
    }

    if (MSG_ReadBit(message) != 0) {
        const uint32_t statBits = (uint32_t)MSG_ReadBits(message, MSG_PLAYERSTATE_STAT_BITS);
        if (cl_shownet != NULL && cl_shownet->integer == MSG_SHOWNET_VERBOSE) {
            Com_Printf("%s ", "PS_STATS");
        }
        for (int32_t statIndex = 0; statIndex < PLAYERSTATE_STAT_COUNT; ++statIndex) {
            if ((statBits & (1u << statIndex)) == 0)
                continue;
            if (statIndex == MSG_PLAYERSTATE_STAT_COMPACT_INDEX) {
                to->stats[statIndex] = MSG_ReadBits(message, MSG_PLAYERSTATE_STAT_COMPACT_BITS);
            } else if (statIndex == MSG_PLAYERSTATE_STAT_BYTE_INDEX) {
                to->stats[statIndex] = MSG_ReadByte(message);
            } else {
                to->stats[statIndex] = MSG_ReadShort(message);
            }
        }
    }

    if (MSG_ReadBit(message) != 0) {
        for (int32_t group = 0; group < MSG_PLAYERSTATE_AMMO_GROUP_COUNT; ++group) {
            if (MSG_ReadBit(message) == 0)
                continue;
            if (cl_shownet != NULL && cl_shownet->integer == MSG_SHOWNET_VERBOSE) {
                Com_Printf("%s ", "PS_AMMO");
            }
            const uint32_t ammoBits = (uint32_t)MSG_ReadShort(message);
            for (int32_t item = 0; item < MSG_PLAYERSTATE_AMMO_GROUP_SIZE; ++item) {
                if ((ammoBits & (1u << item)) != 0) {
                    const int32_t index = group * MSG_PLAYERSTATE_AMMO_GROUP_SIZE + item;
                    to->ammo[index] = MSG_ReadShort(message);
                }
            }
        }
    }

    for (int32_t group = 0; group < MSG_PLAYERSTATE_AMMO_GROUP_COUNT; ++group) {
        if (MSG_ReadBit(message) == 0)
            continue;
        if (cl_shownet != NULL && cl_shownet->integer == MSG_SHOWNET_VERBOSE) {
            Com_Printf("%s ", "PS_AMMOCLIP");
        }
        const uint32_t clipBits = (uint32_t)MSG_ReadShort(message);
        for (int32_t item = 0; item < MSG_PLAYERSTATE_AMMO_GROUP_SIZE; ++item) {
            if ((clipBits & (1u << item)) != 0) {
                const int32_t index = group * MSG_PLAYERSTATE_AMMO_GROUP_SIZE + item;
                to->clips[index] = MSG_ReadShort(message);
            }
        }
    }

    if (MSG_ReadBit(message) != 0) {
        for (int32_t objectiveIndex = 0; objectiveIndex < PLAYERSTATE_OBJECTIVE_COUNT; ++objectiveIndex) {
            to->objectives[objectiveIndex].state = (objectiveState_t)MSG_ReadBits(message, MSG_OBJECTIVE_STATE_BITS);
            MSG_ReadDeltaFields(message, &from->objectives[objectiveIndex], &to->objectives[objectiveIndex], MSG_OBJECTIVE_NETFIELD_COUNT,
                                msg_objectiveNetFields);
        }
    }

    if (MSG_ReadBit(message) != 0) {
        MSG_ReadDeltaHudElems(message, from->hudArchival, to->hudArchival, PLAYERSTATE_HUD_ELEM_COUNT);
        MSG_ReadDeltaHudElems(message, from->hudCurrent, to->hudCurrent, PLAYERSTATE_HUD_ELEM_COUNT);
    }

    /* NOT_FROM_ORIGINAL_SOURCE: after consuming the complete delta, require
     * the view-lock identifier to belong to the coordinated entity domain. */
    if ((uint32_t)to->viewLockedEntityNum >= (uint32_t)MAX_GENTITIES) {
        Com_Error(ERR_DROP,
                  "\x15"
                  "MSG_ReadDeltaPlayerstate: invalid view-lock entity %i",
                  to->viewLockedEntityNum);
        return;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: after consuming the complete delta, require
     * every raw weapon-slot byte to belong to the registered weapon domain. */
    for (int32_t slot = 0; slot < WEAPSLOT_COUNT; ++slot) {
        if (to->weaponSlots[slot] < 0) {
            Com_Error(ERR_DROP,
                      "\x15"
                      "MSG_ReadDeltaPlayerstate: invalid weapon %i in slot %i",
                      (int32_t)to->weaponSlots[slot], slot);
            return;
        }
    }

    /* NOT_FROM_ORIGINAL_SOURCE: after consuming the complete delta, require
     * the vehicle position to belong to the supported seat/tag domain. */
    if (to->vehiclePosition < 0 || to->vehiclePosition >= MSG_PLAYERSTATE_VEHICLE_POSITION_COUNT) {
        Com_Error(ERR_DROP,
                  "\x15"
                  "MSG_ReadDeltaPlayerstate: invalid vehicle position %i",
                  to->vehiclePosition);
        return;
    }
}
