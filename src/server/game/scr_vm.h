#ifndef SCR_VM_H
#define SCR_VM_H

#include <stddef.h>
#include "recovered_game.h"
#include "game_globals.h"

/*
 * Script VM function prototypes.
 * These functions interface with the Call of Duty scripting engine.
 */

typedef enum script_spawn_field_type_e {
    SCRIPT_SPAWN_FIELD_INT = 0,
    SCRIPT_SPAWN_FIELD_FLOAT = 3,
    SCRIPT_SPAWN_FIELD_CSTRING = 4,
    SCRIPT_SPAWN_FIELD_STRING = 5,
    SCRIPT_SPAWN_FIELD_VECTOR = 6,
    SCRIPT_SPAWN_FIELD_ENTITY = 7,
    SCRIPT_SPAWN_FIELD_VECTOR_Y = 9,
    SCRIPT_SPAWN_FIELD_OBJECT = 10,
    SCRIPT_SPAWN_FIELD_MODEL = 11,
    SCRIPT_SPAWN_FIELD_LIGHT = 12
} script_spawn_field_type_t;

/* Scr_ConstructMessageString's diagnostic/localization mode. The stock
 * function carries these six names in its indexed mode-name table. */
typedef enum script_message_mode_e {
    SCRIPT_MESSAGE_MODE_GAME = 0,
    SCRIPT_MESSAGE_MODE_CVAR_VALUE = 1,
    SCRIPT_MESSAGE_MODE_HINT_STRING = 2,
    SCRIPT_MESSAGE_MODE_ANNOUNCEMENT = 3,
    SCRIPT_MESSAGE_MODE_CLIENT_CVAR_VALUE = 4,
    SCRIPT_MESSAGE_MODE_CLIENT_CHAT = 5
} script_message_mode_t;

typedef struct script_method_s {
    const char *name;
    script_method_callback_t callback;
} script_method_t;

typedef struct script_gametype_info_s {
    char *script;
    char *displayName;
    qboolean teamBased;
} script_gametype_info_t;

enum {
    SCRIPT_OBJECT_ENTITY = 0,
    SCRIPT_OBJECT_HUDELEM = 1,
    SCRIPT_OBJECT_VEHICLE_NODE = 2,
    SCRIPT_CLASS_MAP_COUNT = 3,
    GAMETYPE_MAX_COUNT = 32
};

/* Stock `g_scr_data` is one zero-initialized 0x1bc-byte object. All 30 direct
 * GOT-based references prove the handles at +0x00/+0x08..+0x1c, the count at
 * +0x20, 32 twelve-byte gametype rows at +0x24, and three eight-byte class-map
 * rows at +0x1a4. No original reference reads or writes the dword at +0x04;
 * the W8 reserved-field pass therefore classifies it only as unused storage,
 * without inventing a semantic identity. Native pointer members widen
 * normally on 64-bit hosts. */
typedef struct scr_data_s {
    uint32_t levelScriptMain;
    uint32_t unused04;
    uint32_t gametypeScriptMain;
    uint32_t gametypeStart;
    uint32_t playerConnect;
    uint32_t playerDisconnect;
    uint32_t playerDamage;
    uint32_t playerKilled;
    int32_t gametypeCount;
    script_gametype_info_t gametypes[GAMETYPE_MAX_COUNT];
    script_class_map_entry_t classMap[SCRIPT_CLASS_MAP_COUNT];
} scr_data_t;

extern scr_data_t g_scr_data;

#if UINTPTR_MAX == UINT32_MAX
GAME_STATIC_ASSERT(scr_data_i386_layout, offsetof(scr_data_t, levelScriptMain) == 0x00 &&
                                             offsetof(scr_data_t, gametypeScriptMain) == 0x08 &&
                                             offsetof(scr_data_t, gametypeCount) == 0x20 && offsetof(scr_data_t, gametypes) == 0x24 &&
                                             offsetof(scr_data_t, classMap) == 0x1a4 && sizeof(scr_data_t) == 0x1bc);
#endif

/* Parameter reading */
extern uint32_t Scr_GetNumParam(void);
extern script_variable_type_t Scr_GetType(uint32_t index);
extern script_variable_type_t Scr_GetPointerType(uint32_t index);
extern qboolean Scr_GetBool(uint32_t index);
extern int Scr_GetInt(uint32_t index);
extern float Scr_GetFloat(uint32_t index);
extern const char *Scr_GetString(uint32_t index);
extern const char *Scr_GetIString(uint32_t index);
extern const char *Scr_GetDebugString(uint32_t index);
extern uint16_t Scr_GetConstString(uint32_t index);
extern uint16_t Scr_GetConstIString(uint32_t index);
extern gentity_t *Scr_GetEntity(uint32_t index);
extern uint32_t Scr_GetEntityNum(uint32_t index, int *classnum);
extern uint32_t Scr_GetFunc(uint32_t index);
extern void Scr_GetVector(uint32_t index, float *value);
extern scr_anim_t Scr_GetAnim(uint32_t index, XAnimTree *runtimeTree);
extern script_anim_tree_ref_t Scr_GetAnimTree(uint32_t index);

/* Return values */
extern void Scr_AddInt(int value);
extern void Scr_AddFloat(float value);
extern void Scr_AddBool(qboolean value);
extern void Scr_AddAnim(uint32_t anim);
extern void Scr_AddString(const char *value);
extern void Scr_AddIString(const char *value);
extern void Scr_AddConstString(uint16_t value);
extern void Scr_AddUndefined(void);
extern void Scr_AddEntity(gentity_t *ent);
extern void Scr_AddEntityNum(int entityNum, int classnum);
extern void Scr_AddVector(const float *value);
extern void Scr_AddObject(uint16_t objectId);
extern void Scr_AddStruct(void);
extern void Scr_MakeArray(void);
extern void Scr_AddArray(void);
extern void Scr_AddArrayStringIndexed(uint16_t key);

/* Errors */
extern void Scr_Error(const char *message);
extern void Scr_ErrorWithDialogMessage(const char *message, const char *dialogMessage);
extern void Scr_ParamError(int32_t index, const char *message);
extern void Scr_ObjectError(const char *message);
extern void Scr_LocalizationError(uint32_t index, const char *message);

/* Strings */
extern const char *SL_ConvertToString(uint16_t stringId);
extern uint16_t SL_GetString(const char *value, uint8_t user);
extern uint16_t SL_GetLowercaseString(const char *value, uint8_t user);
extern uint16_t SL_FindLowercaseString(const char *value);
extern uint16_t Scr_AllocString(const char *value, int user);
extern void Scr_SetString(uint16_t *slot, uint16_t value);
extern void Scr_ConstructMessageString(uint32_t index, char *buffer, uint32_t size, script_message_mode_t mode);

/* Hudelems */
extern void Scr_FreeHudElem(game_hudElem_t *elem);
extern void Scr_AddHudElem(game_hudElem_t *elem);
extern void Scr_AddClassField(uint16_t classnum, const char *name, uint16_t fieldIndex);

/* Client fields */
extern void Scr_GetGenericField(void *base, int type, size_t offset);
extern void Scr_SetGenericField(void *base, int type, size_t offset);

/* Entity/object fields */
extern void Scr_SetDynamicEntityField(int entityNum, int classnum, uint16_t fieldName);
extern void Scr_FreeEntityNum(int entityNum, int classnum);
extern uint16_t Scr_GetEntityId(int entityNum, int classnum);
extern void Scr_CopyEntityNum(int sourceEntityNum, int destEntityNum, int classnum);
extern void Scr_SetObjectField(int classnum, int objectNum, int fieldIndex);
extern void Scr_GetObjectField(int classnum, int objectNum, int fieldIndex);
extern void Scr_SetClassMap(script_class_map_entry_t *classMap, uint32_t classCount);
extern void Scr_RemoveClassMap(void);
extern void Scr_AddFields(const char *path, const char *extension);
extern uint16_t Scr_FindField(const char *name, int *type);
extern uint32_t Scr_GetOffset(uint16_t classnum, const char *name);
extern void GScr_SetDynamicEntityField(gentity_t *ent, uint16_t fieldName);

/* Lifecycle */
extern script_function_callback_t Scr_GetFunction(const char **name, int *developerOnly);
extern script_method_callback_t Scr_GetMethod(const char **name, int *developerOnly);
extern script_method_callback_t ScriptBuiltin_GetMethod(const char **name);
extern void Scr_ParseGameTypeList(void);
extern const char *Scr_GetGameTypeNameForScript(const char *scriptName);
extern qboolean Scr_IsValidGameType(const char *scriptName);
extern void Scr_Init(int32_t debugReport, int32_t developerScript, int32_t developer);
extern void Scr_Shutdown(void);
extern void Scr_Abort(void);
extern void Scr_SetLoading(int loading);
extern void Scr_AllocGameVariable(void);
extern void Scr_FreeGameVariable(qboolean freeAll);
extern void Scr_FreeScripts(uint8_t mode);
extern void GScr_FreeScripts(void);
extern void Scr_ShutdownSystem(uint8_t mode);
extern void Scr_PlayerConnect(gentity_t *ent);
extern void Scr_PlayerDisconnect(gentity_t *ent);
extern void Scr_PlayerDamage(gentity_t *target, gentity_t *inflictor, gentity_t *attacker, int damage, int flags, int mod, int weapon,
                             const float *point, const float *dir, int hitLocation);
extern void Scr_PlayerKilled(gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod, int weapon, const float *dir,
                             int hitLocation);
extern float Scr_Vehicle_DamageScale(gentity_t *vehicle, gentity_t *attacker, gentity_t *inflictor, const float *point, int mod);
extern void Scr_Vehicle_Pain(gentity_t *ent, gentity_t *attacker, int damage, const float *point, int mod, const float *dir,
                             int hitLocation);
extern void Scr_Vehicle_Die(gentity_t *ent, gentity_t *inflictor, gentity_t *attacker, int damage, int mod, int weapon, const float *dir,
                            int hitLocation);
extern void Scr_Vehicle_Controller(gentity_t *ent, uint32_t *partBits);
extern void Scr_Notify(gentity_t *ent, uint16_t event, int paramCount);
extern void Scr_NotifyNum(int32_t entityNum, int32_t classnum, uint16_t event, uint32_t paramCount);
extern void Scr_NotifyId(uint16_t objectId, uint16_t event, uint32_t paramCount);

/* Animation */
extern void Scr_FindAnim(const char *treeName, const char *animName, scr_anim_t *outAnim);
#if defined(WINDOWS_BEHAVIOR)
extern XAnim *Scr_FindAnimTree(const char *treeName);
#else
extern script_anim_tree_ref_t Scr_FindAnimTree(const char *treeName);
#endif
extern uint32_t Scr_GetAnimsIndex(XAnim *anims);
extern XAnim *Scr_GetAnims(uint32_t animsIndex);

/* System */
extern void Scr_InitSystem(uint32_t system, uint32_t time);
extern qboolean Scr_IsSystemActive(uint8_t system);
extern void Scr_SetTime(uint32_t time);
extern void Scr_RunCurrentThreads(void);
extern void Scr_ResetTimeout(void);
extern void Scr_MakeGameMessage(int entityNum, const char *command);

/* Loading/save and thread wrappers */
extern void Scr_GetChecksum(uint32_t checksum[3]);
extern qboolean Scr_HasSourceFiles(void);
extern void Scr_SaveSource(script_source_io_fn_t writeData);
extern void Scr_LoadSource(script_source_io_fn_t readData);
extern void Scr_SkipSource(script_source_io_fn_t readData);
extern void Scr_SavePre(void *file);
extern void Scr_SavePost(void *file);
extern void Scr_SaveShutdown(void);
extern void Scr_LoadPre(void *file, int scriptRunning);
extern void Scr_LoadShutdown(void);
extern void *Scr_LoadRead(uint32_t size);
extern qboolean Scr_LoadScript(const char *scriptName);
extern uint32_t Scr_GetFunctionHandle(const char *scriptName, const char *labelName);
extern void Scr_BeginLoadScripts(void);
extern void Scr_BeginLoadAnimTrees(void);
extern void Scr_EndLoadScripts(void);
extern void Scr_EndLoadAnimTrees(void);
extern void Scr_LoadGameType(void);
extern void Scr_LoadLevel(void);
extern void Scr_StartupGameType(void);
extern void Scr_PrecacheAnimTrees(script_anim_tree_alloc_t alloc);
extern uint16_t Scr_ExecThread(uint32_t handle, uint32_t paramCount);
extern uint16_t Scr_ExecEntThread(gentity_t *ent, uint32_t handle, int paramCount);
extern uint16_t Scr_ExecEntThreadNum(int32_t entityNum, int32_t classnum, uint32_t handle, uint32_t paramCount);
extern void Scr_AddExecThread(uint32_t handle, uint32_t paramCount);
extern void Scr_AddExecEntThreadNum(int32_t entityNum, int32_t classnum, uint32_t handle, uint32_t paramCount);
extern qboolean Scr_IsThreadAlive(uint16_t threadId);
extern void Scr_FreeThread(uint16_t threadId);
extern uint16_t Scr_ConvertThreadToSave(uint16_t threadId);
extern uint16_t Scr_ConvertThreadFromLoad(uint16_t threadId);
extern uint16_t Scr_CreateCanonicalFilename(const char *filename);
extern script_callback_fn_t *Scr_FarHook(script_callback_fn_t *engineCallbacks);

/* Game-side script helpers and field bridges */
extern uint16_t G_NewString(const char *value);
extern void GScr_AddVector(const float *value);
extern void GScr_AddEntity(gentity_t *ent);
extern void Scr_SetOrigin(gentity_t *ent);
extern void Scr_SetAngles(gentity_t *ent);
extern void Scr_SetHealth(gentity_t *ent);
extern int GScr_GetScriptMenuIndex(const char *menuName);
extern int GScr_GetStatusIconIndex(const char *name);
extern int GScr_GetHeadIconIndex(const char *name);
extern int16_t GScr_GetVehicleNodeIndex(uint32_t paramIndex);
extern void GScr_AddFieldsForClient(uint16_t classnum);
extern void GScr_AddFieldsForHudElems(void);
extern void GScr_GetVehicleNodeField(int entityNum, int fieldIndex);
extern void Scr_SetClientField(gclient_t *client, int fieldIndex);
extern void Scr_GetClientField(gclient_t *client, int fieldIndex);
extern void Scr_SetHudElemField(int elemIndex, int fieldIndex);
extern void Scr_GetHudElemField(int elemIndex, int fieldIndex);
extern void Scr_FreeHudElemConstStrings(game_hudElem_t *elem);

#endif /* SCR_VM_H */
