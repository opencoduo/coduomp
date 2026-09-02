#ifndef SHARED_SCRIPT_ANIM_H
#define SHARED_SCRIPT_ANIM_H

#include "qcommon/script_runtime_types.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int32_t script_activeAnimTreeHandle;
extern int32_t script_animCheckEnabled;
extern int32_t script_animCommentDepth;
extern uint16_t script_animCurrentTreeRoot;
extern uint16_t script_animCurrentUsingTree;
extern const char *script_animParseStart;
extern char *script_animParseState;
extern uint32_t script_animTreeChecksum;
extern int32_t script_animTreeCounts[SCRIPT_ANIM_SLOT_COUNT];
extern uint16_t script_animTreeHandles[SCRIPT_ANIM_SLOT_COUNT][SCRIPT_ANIM_TREE_SLOT_COUNT];
extern uint16_t script_animTreeRoot;
extern XAnim *script_animTrees[SCRIPT_ANIM_SLOT_COUNT][SCRIPT_ANIM_TREE_SLOT_COUNT];

void SetAnimCheck(int32_t enabled);
void AnimTreeCompileError(const char *message);
uint32_t GetAnimTreeParseProperties(void);
qboolean AnimTreeParseInternal(uint16_t parentHandle, uint16_t treeHandle, qboolean addEmptyVoid, qboolean addLoopingVoid,
                               qboolean forceLoadedChildren);
void Scr_AnimTreeParse(char *source, uint16_t parentHandle, uint16_t treeHandle);
int32_t Scr_GetAnimTreeSize(uint16_t handle);
void *Hunk_AllocXAnimPrecache(size_t size);
void ConnectScriptToAnim(uint16_t unresolvedRoot, int32_t nodeIndex, uint16_t treeName, uint16_t animName, int32_t treeIndex);
uint32_t Scr_CreateAnimationTree(uint16_t sourceHandle, uint16_t unresolvedRoot, XAnim *tree, uint32_t firstChildIndex,
                                 const char *nodeName, uint32_t nodeIndex, uint16_t treeName, int32_t treeIndex);
void Scr_CheckAnimsDefined(uint16_t unresolvedRoot, uint16_t treeName);
void Scr_PrecacheAnimationTree(uint16_t sourceHandle);
void Scr_EmitAnimationInternal(char *animRef, uint16_t animHandle, uint16_t treeHandle, uint32_t sourcePos);
void Scr_EmitAnimation(char *animRef, uint16_t animHandle, uint32_t sourcePos);
uint16_t Scr_UsingTreeInternal(const char *treeName, int32_t *treeIndex);
void Scr_UsingTree(const char *animTreeName, uint32_t sourcePos);
qboolean Scr_LoadAnimTreeInternal(const char *animTreeName, uint16_t parentHandle, uint16_t treeHandle);
void Scr_LoadAnimTreeAtIndex(int32_t treeIndex, script_anim_tree_alloc_t allocCallback);
#if defined(WINDOWS_BEHAVIOR)
XAnim *CODUO_SCRIPT_CDECL Scr_FindAnimTree(const char *filename);
#else
script_anim_tree_ref_t CODUO_SCRIPT_CDECL Scr_FindAnimTree(const char *filename);
#endif
void CODUO_SCRIPT_CDECL Scr_FindAnim(const char *treeName, const char *animName, scr_anim_t *animRef);

/* Native-width compatibility ownership; original i386 has no sidecars. */
void coduo_script_compat_anim_release_unresolved_ref_sidecars(void);

#ifdef __cplusplus
}
#endif

#endif
