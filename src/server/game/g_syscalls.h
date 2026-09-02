#ifndef G_SYSCALLS_H
#define G_SYSCALLS_H

#include "qcommon/game_module_abi_types.h"
#include "recovered_game.h"
#include "qcommon/qtime_types.h"

extern game_syscall_t g_syscall;

/*
 * System call prototypes for engine interface.
 * These functions are provided by the engine via syscall table.
 */

/* Error handling */
extern void trap_Printf(const char *message);
extern void trap_Error(const char *message);
extern void trap_Error_Localized(const char *message);

/* Trace/Physics */
extern void trap_Trace(trace_t *trace, const float *start, const float *mins, const float *maxs, const float *end, int passEntityNum,
                       int contentMask);
extern void trap_TraceCapsule(trace_t *trace, const float *start, const float *mins, const float *maxs, const float *end, int passEntityNum,
                              int contentMask);
extern void trap_SightTrace(int32_t *traceResult, const float *start, const float *mins, const float *maxs, const float *end,
                            int passEntityNum, int passOwnerNum, int contentMask);
extern void trap_SightTraceCapsule(int32_t *traceResult, const float *start, const float *mins, const float *maxs, const float *end,
                                   int passEntityNum, int passOwnerNum, int contentMask);
extern void trap_CM_BoxTrace(trace_t *trace, const float *start, const float *end, const float *mins, const float *maxs, int model,
                             int brushMask);
extern void trap_CM_CapsuleTrace(trace_t *trace, const float *start, const float *end, const float *mins, const float *maxs, int model,
                                 int brushMask);
extern int trap_CM_BoxSightTrace(const float *start, const float *end, const float *mins, const float *maxs, int model, int brushMask);
extern int trap_CM_CapsuleSightTrace(const float *start, const float *end, const float *mins, const float *maxs, int model, int brushMask);
extern void trap_LocationalTrace(trace_t *trace, const float *start, const float *end, int passEntityNum, int contentMask,
                                 const void *priorityMap);
extern int trap_PointContents(const float *point, int passEntityNum, int contentMask);
extern qboolean trap_InPVS(const float *p1, const float *p2);
extern qboolean trap_InPVSIgnorePortals(const float *p1, const float *p2);
extern int trap_InSnapshot(const float *origin, int entityNum);
extern void trap_AdjustAreaPortalState(gentity_t *ent, qboolean open);
extern qboolean trap_AreasConnected(int area1, int area2);
extern int trap_EntitiesInBox(const float *mins, const float *maxs, int *entityList, int maxcount, int contentMask);
extern int trap_SightTraceToEntity(const float *start, const float *mins, const float *maxs, const float *end, int entityNum,
                                   int contentMask);
extern qboolean trap_EntityContact(const float *mins, const float *maxs, gentity_t *ent);
extern qboolean trap_EntityContactCapsule(const float *mins, const float *maxs, gentity_t *ent);

/* Entity Management */
extern void trap_LinkEntity(gentity_t *ent);
extern void trap_UnlinkEntity(gentity_t *ent);
extern void trap_DObjCreate(DObjModel *models, uint16_t modelCount, XAnimTree *tree, int entityNum, uint16_t gameId);
extern qboolean trap_DObjExists(gentity_t *ent);
extern void trap_SafeDObjFree(int entityNum, qboolean freeAll);
extern int trap_DObjNumBones(gentity_t *ent);
extern int trap_DObjGetBoneIndex(gentity_t *ent, const char *tagName);
extern float *trap_DObjGetMatrixArray(gentity_t *ent);
extern void trap_DObjDumpInfo(gentity_t *ent);
extern qboolean trap_DObjCreateSkelForBone(gentity_t *ent, int boneIndex);
extern qboolean trap_DObjCreateSkelForBones(gentity_t *ent, uint32_t *partBits);
extern int trap_DObjUpdateServerTime(gentity_t *ent, float serverTime, qboolean notify);
extern void trap_DObjDisplayAnim(gentity_t *ent);
extern void trap_DObjInitServerTime(gentity_t *ent, float serverTime);
extern void trap_DObjGetHierarchyBits(gentity_t *ent, int boneIndex, uint32_t *partBits);
extern void trap_DObjCalcAnim(gentity_t *ent, uint32_t *partBits);
extern void trap_DObjCalcSkel(gentity_t *ent, uint32_t *partBits);
extern DObjAnimMat *trap_DObjGetRotTransArray(gentity_t *ent);
extern qboolean trap_DObjSetRotTransIndex(gentity_t *ent, uint32_t *partBits, int boneIndex);
extern qboolean trap_DObjSetControlRotTransIndex(gentity_t *ent, uint32_t *partBits, int boneIndex);
extern void trap_DObjGetBounds(gentity_t *ent, vec3_t mins, vec3_t maxs);
extern XAnimTree *trap_DObjGetTree(gentity_t *ent);
extern qboolean trap_XModelExists(const char *modelName);
extern XModel *trap_XModelGet(const char *modelName);
extern int trap_XModelNumBones(XModel *model);
extern const uint16_t *trap_XModelGetBoneNames(XModel *model);
extern void trap_XModelDebugBoxes(gentity_t *ent);

/*
 * XAnim trap ABI notes:
 * packed animation refs remain uint32_t high/low tree-index+anim-index
 * values, while runtime tree handles use XAnimTree * so native64 does not
 * truncate engine-owned pointers that i386 carried in a single word.
 */
extern int trap_XAnimGetAnimTreeSize(XAnim *anims);
extern qboolean trap_XAnimHasTime(uint32_t anim);
extern qboolean trap_XAnimIsPrimitive(uint32_t anim);
extern const char *trap_XAnimGetAnimName(uint32_t anim);
extern int trap_XAnimGetLength(XAnim *anims, uint16_t animIndex);
extern float trap_XAnimGetLengthSeconds(uint32_t anim);
extern void trap_XAnimGetRelDelta(uint32_t anim, float *rotationDelta, float *moveDelta, float startTime, float endTime);
extern void trap_XAnimGetAbsDelta(uint32_t anim, float *rotationDelta, float *moveDelta, float time);
extern qboolean trap_XAnimIsLooped(uint32_t anim);
extern float trap_XAnimGetWeight(XAnimTree *tree, uint32_t anim);
extern float trap_XAnimGetTime(XAnimTree *tree, uint32_t anim);
extern qboolean trap_XAnimNotetrackExists(uint32_t anim, uint16_t notetrack);
extern qboolean trap_XAnimHasFinished(XAnimTree *tree, uint32_t anim);
extern int trap_XAnimGetNumChildren(uint32_t anim);
extern uint32_t *trap_XAnimGetChildAt(uint32_t *out, uint32_t anim, int childIndex);
extern XAnimTree *trap_XAnimCreateTree(XAnim *anims);
extern XAnimTree *trap_XAnimCreateSmallTree(XAnim *anims);
extern void trap_XAnimFreeSmallTree(XAnimTree *tree);
extern void trap_XAnimCloneAnimTree(XAnimTree *fromTree, XAnimTree *toTree);
extern XAnim *trap_XAnimGetAnims(XAnimTree *tree);
extern uint32_t *trap_XAnimGetRoot(uint32_t *out, XAnimTree *tree);
extern void trap_XAnimClearTreeGoalWeights(XAnimTree *tree, uint32_t anim, float blendTime);
extern void trap_XAnimClearGoalWeight(XAnimTree *tree, uint32_t anim, float blendTime);
extern void trap_XAnimClearTreeGoalWeightsStrict(XAnimTree *tree, uint32_t anim, float blendTime);
extern void trap_XAnimSetCompleteGoalWeightKnob(XAnimTree *tree, uint32_t anim, float weight, float blendTime, float rate,
                                                uint16_t notifyName, qboolean restart);
extern void trap_XAnimSetCompleteGoalWeightKnobAll(XAnimTree *tree, uint32_t anim, uint32_t knob, float weight, float blendTime, float rate,
                                                   uint16_t notifyName, qboolean restart);
extern void trap_XAnimSetAnimRate(XAnimTree *tree, uint32_t anim, float rate);
extern void trap_XAnimSetTime(XAnimTree *tree, uint32_t anim, float time);
extern void trap_XAnimSetGoalWeightKnob(XAnimTree *tree, uint32_t anim, float weight, float blendTime, float rate, uint16_t notifyName,
                                        qboolean restart);
extern void trap_XAnimClearTree(XAnimTree *tree);
extern void trap_XAnimSetCompleteGoalWeight(XAnimTree *tree, uint32_t anim, float weight, float blendTime, float rate, uint16_t notifyName,
                                            qboolean restart);
extern void trap_XAnimSetGoalWeight(XAnimTree *tree, uint32_t anim, float weight, float blendTime, float rate, uint16_t notifyName,
                                    qboolean restart);
extern void trap_XAnimCalcAbsDelta(XAnimTree *tree, uint32_t anim, float *rotationDelta, float *moveDelta);
extern void trap_XAnimCalcDelta(XAnimTree *tree, uint32_t anim, float *rotationDelta, float *moveDelta, int weightSelector);
extern void trap_XAnimLoadAnimTree(XAnimTree *tree);
extern void trap_XAnimSaveAnimTree(XAnimTree *tree);

/* Cvar */
extern void trap_Cvar_Register(vmCvar_t *cvar, const char *name, const char *value, int flags);
extern void trap_Cvar_Set(const char *name, const char *value);
extern void trap_Cvar_Update(vmCvar_t *cvar);
extern void trap_Cvar_VariableStringBuffer(const char *name, char *buffer, int size);
extern int trap_Cvar_VariableIntegerValue(const char *name);
extern float trap_Cvar_VariableValue(const char *name);

/* Configstring */
extern void trap_GetConfigstring(int index, char *buffer, int bufferLength);
extern const char *trap_GetConfigstringConst(int index);
extern void trap_SetConfigstring(int index, const char *value);

/* Client Info */
extern void trap_GetUserinfo(int clientNum, char *buffer, int bufferSize);
extern void trap_SetUserinfo(int clientNum, const char *info);
extern void trap_GetUsercmd(int clientNum, usercmd_t *command);
extern int trap_GetArchivedClientInfo(int clientNum, int32_t *archiveTime, playerState_t *playerState, clientState_t *meta);
extern int trap_IsLocalClient(int clientNum);
extern int trap_GetGuid(int clientNum);
extern int trap_GetClientPing(int clientNum);

/* Server Commands */
extern void trap_SendServerCommand(int clientNum, int reliable, const char *command);
extern void trap_SendConsoleCommand(int executionTime, const char *text);
extern void trap_DropClient(int clientNum, const char *reason);
extern gentity_t *trap_AddTestClient(void);
extern void trap_SetArchive(qboolean enabled);

/* Filesystem */
extern int trap_FS_FOpenFile(const char *path, int *handle, fsMode_t mode);
extern void trap_FS_FCloseFile(int handle);
extern void trap_FS_Read(void *buffer, int length, int handle);
extern void trap_FS_Write(const void *buffer, int length, int handle);
extern void trap_FS_Rename(const char *from, const char *to);
extern int trap_FS_GetFileList(const char *path, const char *extension, char *listBuffer, int bufferSize);
extern int trap_MapExists(const char *mapName);

/* Debug */
extern void trap_AddDebugString(const float *origin, const float *color, float scale, const char *text);
extern void trap_AddDebugLine(const float *start, const float *end, const float *color, int depthTest, int duration);
extern int trap_SurfaceTypeFromName(const char *name);
extern const char *trap_SurfaceTypeToName(int surfaceType);

/* Sound aliases */
extern const char *trap_Com_SoundAliasString(const char *name);
extern void trap_Com_PickSoundAlias(const char *name, void *alias);
extern int trap_Com_SoundAliasIndex(const snd_alias_t *alias);

/* Command Args */
extern int trap_Argc(void);
extern void trap_Argv(int arg, char *buffer, int bufferLength);
extern qboolean trap_GetEntityToken(char *buffer, int bufferSize);

/* Misc */
extern int trap_Milliseconds(void);
extern void trap_RealTime(qtime_t *qtime);
extern void trap_SnapVector(float *vec);
extern void trap_GetServerinfo(char *buffer, int bufferSize);
extern void trap_LocateGameData(gentity_t *gEnts, int numGEntities, int sizeofGEntity, playerState_t *clients, int sizeofClient);
extern void trap_SetBrushModel(gentity_t *ent);
extern void trap_FreeClientScriptPers(void);
extern weaponInfo_t **trap_GetWeaponInfoMemory(int bytes, int *alreadyLoaded);
extern void trap_FreeWeaponInfoMemory(int mode);
extern void trap_ResetEntityParsePoint(void);

/* Hunk memory */
extern void *trap_Hunk_AllocInternal(size_t size);
extern void *trap_Hunk_AllocLowInternal(size_t size);
extern void *trap_Hunk_AllocAlignInternal(size_t size, int alignment);
extern void *trap_Hunk_AllocLowAlignInternal(size_t size, int alignment);
extern void *trap_Hunk_AllocateTempMemoryInternal(size_t size);
extern void trap_Hunk_FreeTempMemoryInternal(void *ptr);
extern void *trap_Z_MallocInternal(size_t size);
extern void trap_Z_FreeInternal(void *ptr);

#endif /* G_SYSCALLS_H */
