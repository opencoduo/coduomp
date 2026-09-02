#include "script_runtime_host.h"

#include "qcommon/file_data.h"
#include "script_anim.h"
#include "script_compile_load.h"
#include "script_error_reporting.h"
#include "script_memory.h"
#include "script_source_positions.h"
#include "script_string.h"
#include "script_variable.h"
#include "animation/xanim.h"
#include "animation/xanim_asset_load.h"
#include "qcommon/com_sprintf.h"

#include <stdio.h>
#include <string.h>

enum {
    SCRIPT_ANIM_TREE_ROOT_NODE_INDEX = 0,
    SCRIPT_ANIM_TREE_FIRST_CHILD_NODE_INDEX = 1,
    SCRIPT_ANIM_TREE_FIRST_TABLE_INDEX = 1,
    SCRIPT_ANIM_STRING_USER_NONE = 0,
    SCRIPT_ANIM_TREE_STRING_USER = 2,
    SCRIPT_ANIM_NAME_TYPE = 4,
    SCRIPT_ANIM_SOURCE_NONE = 0,
    SCRIPT_ANIM_HASH_MULTIPLIER = 31
};

/* Original pointer table 0x005ca6e8.
 * PE_RELOCATION_VALUES_VERIFIED: verify_relocated_initializers.py follows all
 * three ordered animation-property name targets. */
static const char *const script_animPropertyNames[] = {
    "loopsync",
    "nonloopsync",
    "complete"
};

/* Source: CoDUOMP.exe 0x00478460..0x00478465.
 * Name and signature: exact same-module Mac symbol SetAnimCheck. */
void SetAnimCheck(int32_t enabled)
{
    script_animCheckEnabled = enabled;
}

/* Source: CoDUOMP.exe 0x00478470..0x004784c6.
 * Name: exact same-module Mac symbol AnimTreeCompileError.  The Windows compiler
 * inlines Com_GetLastTokenPos and Com_EndParseSession before reporting the
 * offset relative to script_animParseStart. */
void AnimTreeCompileError(const char *message)
{
    const char *errorPos = Com_GetLastTokenPos();
    Com_EndParseSession();
    CompileError((uint32_t)(errorPos - script_animParseStart),
                          "%s", message);
}

/* Source: CoDUOMP.exe 0x004784d0..0x004785d3.
 * Name: exact same-module Mac symbol GetAnimTreeParseProperties. */
uint32_t GetAnimTreeParseProperties(void)
{
    uint32_t flags = 0;
    for (;;) {
        const char *token = Com_ParseOnLine(&script_animParseState);
        if (token[0] == '\0')
            return flags;

        int32_t propertyIndex = 0;
        while (propertyIndex < SCRIPT_ANIM_PROPERTY_NAME_COUNT &&
               SCRIPT_STRICMP(
                   token, script_animPropertyNames[propertyIndex]) != 0) {
            ++propertyIndex;
        }

        switch (propertyIndex) {
        case 0:
            flags |= SCRIPT_ANIM_PROPERTY_LOOPSYNC;
            break;
        case 1:
            flags |= SCRIPT_ANIM_PROPERTY_NONLOOPSYNC;
            break;
        case 2:
            flags |= SCRIPT_ANIM_PROPERTY_COMPLETE;
            break;
        default:
            AnimTreeCompileError("unknown anim property");
            break;
        }
    }
}

/* Source: CoDUOMP.exe 0x004786b0..0x00478e1a.
 * Name/signature: same-module Mac symbol
 * AnimTreeParseInternal(unsigned short, unsigned short, bool, bool, bool).
 * The recovered Linux engine independently proves the same source structure;
 * every condition and mutation below is also matched to the Windows body. */
qboolean AnimTreeParseInternal(uint16_t parentHandle,
                               uint16_t treeHandle,
                               qboolean addEmptyVoid,
                               qboolean addLoopingVoid,
                               qboolean forceLoadedChildren)
{
    uint16_t animNameHandle = 0;
    uint16_t animNodeHandle = 0;
    uint32_t propertyFlags = 0;
    qboolean missingFromTree = qfalse;
    qboolean hitEndOfFile = qtrue;

    for (;;) {
        const char *token = Com_Parse(&script_animParseState);
        if (script_animParseState == NULL)
            break;

        if (Scr_IsIdentifier(token) != qfalse) {
            if (missingFromTree != qfalse)
                RemoveVariable(parentHandle, animNameHandle);

            animNameHandle = SL_GetLowercaseString_(
                token, SCRIPT_ANIM_TREE_STRING_USER,
                SCRIPT_ANIM_NAME_TYPE);
            if (FindVariable(parentHandle, animNameHandle) != 0)
                AnimTreeCompileError("duplicate animation");
            animNodeHandle =
                GetVariable(parentHandle, animNameHandle);

            missingFromTree = qfalse;
            if (forceLoadedChildren == qfalse &&
                FindVariable(treeHandle, animNameHandle) == 0 &&
                script_animCheckEnabled == 0) {
                missingFromTree = qtrue;
            }

            propertyFlags = 0;
            token = Com_ParseOnLine(&script_animParseState);
            if (token[0] == '\0')
                continue;

            if (Scr_IsIdentifier(token) != qfalse)
                AnimTreeCompileError("FIXME: aliases not yet implemented");
            if (token[0] != ':' || token[1] != '\0')
                AnimTreeCompileError("bad token");

            propertyFlags = GetAnimTreeParseProperties();
            token = Com_Parse(&script_animParseState);
            if (token[0] != '{' || token[1] != '\0') {
                AnimTreeCompileError(
                    "properties cannot be applied to primitive animations");
            }
        }

        if (token[0] == '{') {
            if (token[1] != '\0')
                AnimTreeCompileError("bad token");

            token = Com_ParseOnLine(&script_animParseState);
            if (token[0] != '\0')
                AnimTreeCompileError("token not allowed after '{'");
            if (animNodeHandle == 0) {
                AnimTreeCompileError(
                    "no animation specified for this block");
            }

            uint16_t childNodeHandle =
                GetArray(animNodeHandle);
            const qboolean childForceLoaded =
                forceLoadedChildren != qfalse ||
                ((propertyFlags & SCRIPT_ANIM_PROPERTY_COMPLETE) != 0 &&
                 missingFromTree == qfalse);
            if (AnimTreeParseInternal(
                    childNodeHandle, treeHandle,
                    missingFromTree == qfalse,
                    (propertyFlags & SCRIPT_ANIM_PROPERTY_LOOPSYNC) != 0,
                    childForceLoaded) != qfalse) {
                AnimTreeCompileError("unexpected end of file");
            }

            if (GetArraySize(childNodeHandle) == 0) {
                RemoveVariable(parentHandle, animNameHandle);
            } else {
                VariableValue value;
                value.payload = propertyFlags;
                value.type = SCRIPT_VAR_INT;
                const uint16_t propertyHandle = GetArrayVariable(
                    childNodeHandle, SCRIPT_ANIM_TREE_ROOT_NODE_INDEX);
                SetVariableValue(propertyHandle, &value);
            }

            animNodeHandle = 0;
            missingFromTree = qfalse;
            continue;
        }

        if (token[0] == '}') {
            if (token[1] != '\0')
                AnimTreeCompileError("bad token");
            token = Com_ParseOnLine(&script_animParseState);
            if (token[0] != '\0')
                AnimTreeCompileError("token not allowed after '}'");
            hitEndOfFile = qfalse;
            break;
        }

        AnimTreeCompileError("bad token");
    }

    if (missingFromTree != qfalse)
        RemoveVariable(parentHandle, animNameHandle);

    if (addEmptyVoid != qfalse &&
        GetArraySize(parentHandle) == 0) {
        animNameHandle = SL_GetString_(
            addLoopingVoid != qfalse ? "void_loop" : "void",
            SCRIPT_ANIM_STRING_USER_NONE, SCRIPT_ANIM_NAME_TYPE);
        GetVariable(parentHandle, animNameHandle);
        SL_RemoveRefToString(animNameHandle);
    }

    return hitEndOfFile;
}

/* Source: CoDUOMP.exe 0x00478e20..0x00478e92.
 * Name/signature: same-module Mac symbol
 * Scr_AnimTreeParse(const char *, unsigned short, unsigned short). */
void Scr_AnimTreeParse(char *source, uint16_t parentHandle,
                       uint16_t treeHandle)
{
    Com_BeginParseSession("Scr_AnimTreeParse");
    script_animParseState = source;
    script_animParseStart = source;
    if (AnimTreeParseInternal(parentHandle, treeHandle,
                              qtrue, qfalse, qfalse) == qfalse) {
        AnimTreeCompileError("bad token");
    }
    Com_EndParseSession();
}

/* Source: CoDUOMP.exe 0x00478eb0..0x00478f57.
 * Name/signature: same-module Mac symbol Scr_GetAnimTreeSize(unsigned short). */
int32_t Scr_GetAnimTreeSize(uint16_t handle)
{
    int32_t nodeCount = 0;
    for (uint16_t child = FindNextSibling(handle);
         child != 0; child = FindNextSibling(child)) {
        const uint32_t name = GetVariableName(child);
        if (name >= SCRIPT_VARIABLE_NODE_COUNT)
            continue;

        if (GetVarType(child) == SCRIPT_VAR_OBJECT) {
            nodeCount += Scr_GetAnimTreeSize(
                FindObject(child));
        } else {
            ++nodeCount;
        }
    }

    if (nodeCount != 0)
        ++nodeCount;
    return nodeCount;
}

/* Source: CoDUOMP.exe 0x00478ea0..0x00478eae, recovered from the executable
 * gap inventory. Name: exact same-module Mac symbol Hunk_AllocXAnimPrecache.
 * The original alignment is 4 on i386; using the asset-entry alignment keeps
 * that layout there and satisfies the native pointer-bearing XAnim records. */
void *Hunk_AllocXAnimPrecache(size_t size)
{
    return SCRIPT_HUNK_ALLOC_ALIGN(size, _Alignof(fileData_t));
}

#if UINTPTR_MAX > UINT32_MAX
/* NOT_FROM_ORIGINAL_SOURCE: widened hosts need an explicit pointer-bearing
 * list node. Original i386 overloads the same four-byte bytecode word: while
 * unresolved it contains a pointer to the previous word, and after resolution
 * it contains the packed uint32_t animation reference. On 64-bit the pointer
 * is wider. SCRIPT_OP_GET_ANIMATION reads the four-byte reference and then
 * zero-extends it into its host-width value payload; widening the bytecode word
 * itself would move following code and would not fit Scr_FindAnim's uint32_t
 * output contract. Keep only the temporary full-width pointer in this sidecar.
 * The char pointer is the original Mac API's full-width cell address, not a
 * narrowed address. */
typedef struct script_anim_unresolved_ref_s {
    char *target;
    struct script_anim_unresolved_ref_s *next;
    struct script_anim_unresolved_ref_s *allocationNext;
} script_anim_unresolved_ref_t;

static script_anim_unresolved_ref_t *script_animUnresolvedSidecars;
#endif

/* Source: CoDUOMP.exe 0x00478f60..0x00479005.
 * Name/signature: exact same-module Mac symbol
 * ConnectScriptToAnim(unsigned short, int, unsigned short, unsigned short,
 * int).  The unresolved references form a temporary linked list until the
 * parser can replace each output slot with its packed tree/node index. */
void ConnectScriptToAnim(uint16_t unresolvedRoot,
                         int32_t nodeIndex,
                         uint16_t treeName,
                         uint16_t animName,
                         int32_t treeIndex)
{
    const uint16_t unresolvedHandle =
        FindVariable(unresolvedRoot, animName);
    if (unresolvedHandle == 0)
        return;

    VariableValue *storedValue = GetVariableValueAddress(unresolvedHandle);
    if (storedValue->payload == 0) {
        Com_Error(ERR_DROP,
                  "\x15" "duplicate animation '%s' in 'animtrees/%s.atr'",
                  SL_ConvertToString(animName),
                  SL_ConvertToString(treeName));
    }

    const uint32_t packedAnimRef =
        ((uint32_t)(uint16_t)treeIndex << SCR_ANIM_TREE_INDEX_SHIFT) |
        (uint16_t)nodeIndex;
#if UINTPTR_MAX > UINT32_MAX
    script_anim_unresolved_ref_t *ref =
        (script_anim_unresolved_ref_t *)storedValue->payload;
    while (ref != NULL) {
        script_anim_unresolved_ref_t *next = ref->next;
        memcpy(ref->target, &packedAnimRef, sizeof(packedAnimRef));
        ref = next;
    }
#else
    char *ref = (char *)storedValue->payload;
    while (ref != NULL) {
        uint32_t nextPayload;
        memcpy(&nextPayload, ref, sizeof(nextPayload));
        char *next = (char *)(uintptr_t)nextPayload;
        memcpy(ref, &packedAnimRef, sizeof(packedAnimRef));
        ref = next;
    }
#endif
    storedValue->payload = 0;
}

/* Source: CoDUOMP.exe 0x00479060..0x004792dd.
 * Name/signature: exact same-module Mac symbol
 * Scr_CreateAnimationTree(unsigned short, unsigned short, XAnim_s *,
 * unsigned int, const char *, unsigned int, unsigned short, int).
 * The Windows body inlines XAnimSetParentNode and XAnimSetLeafNode; calling
 * their independently recovered source preserves the same writes. */
uint32_t Scr_CreateAnimationTree(uint16_t sourceHandle,
                                 uint16_t unresolvedRoot,
                                 XAnim *tree,
                                 uint32_t firstChildIndex,
                                 const char *nodeName,
                                 uint32_t nodeIndex,
                                 uint16_t treeName,
                                 int32_t treeIndex)
{
    uint16_t childCount = 0;
    uint16_t propertyFlags = 0;

    for (uint16_t child = FindNextSibling(sourceHandle);
         child != 0; child = FindNextSibling(child)) {
        if (GetVariableName(child) <
            SCRIPT_VARIABLE_NODE_COUNT) {
            ++childCount;
        }
    }

    const uint16_t propertyHandle = FindArrayVariable(
        sourceHandle, SCRIPT_ANIM_TREE_ROOT_NODE_INDEX);
    if (propertyHandle != 0) {
        propertyFlags =
            (uint16_t)GetVariableValueAddress(propertyHandle)->payload;
    }

    script_animTreeChecksum =
        script_animTreeChecksum * SCRIPT_ANIM_HASH_MULTIPLIER + nodeIndex;
    script_animTreeChecksum =
        script_animTreeChecksum * SCRIPT_ANIM_HASH_MULTIPLIER +
        firstChildIndex;
    script_animTreeChecksum =
        script_animTreeChecksum * SCRIPT_ANIM_HASH_MULTIPLIER + childCount;
    script_animTreeChecksum =
        script_animTreeChecksum * SCRIPT_ANIM_HASH_MULTIPLIER + propertyFlags;

    XAnimSetParentNode(tree, (uint16_t)nodeIndex, nodeName,
                       (uint16_t)firstChildIndex, childCount, propertyFlags);

    uint32_t childNodeIndex = firstChildIndex;
    uint32_t nextFreeNodeIndex = firstChildIndex + childCount;
    for (uint16_t child = FindNextSibling(sourceHandle);
         child != 0; child = FindNextSibling(child)) {
        const uint32_t name = GetVariableName(child);
        if (name >= SCRIPT_VARIABLE_NODE_COUNT)
            continue;

        const uint16_t animName = (uint16_t)name;
        ConnectScriptToAnim(unresolvedRoot, (int32_t)childNodeIndex,
                            treeName, animName, treeIndex);
        if (GetVarType(child) == SCRIPT_VAR_OBJECT) {
            nextFreeNodeIndex = Scr_CreateAnimationTree(
                FindObject(child), unresolvedRoot, tree,
                nextFreeNodeIndex, SL_ConvertToString(animName),
                childNodeIndex, treeName, treeIndex);
        } else {
            script_animTreeChecksum =
                script_animTreeChecksum * SCRIPT_ANIM_HASH_MULTIPLIER +
                childNodeIndex;
            XAnimSetLeafNode(tree, (uint16_t)childNodeIndex,
                             SL_ConvertToString(animName));
        }
        ++childNodeIndex;
    }

    return nextFreeNodeIndex;
}

/* Source: CoDUOMP.exe 0x004792e0..0x004793ea.
 * Name/signature: exact same-module Mac symbol
 * Scr_CheckAnimsDefined(unsigned short, unsigned short). */
void Scr_CheckAnimsDefined(uint16_t unresolvedRoot, uint16_t treeName)
{
    for (uint16_t child = FindNextSibling(unresolvedRoot);
         child != 0; child = FindNextSibling(child)) {
        const uint16_t animName =
            (uint16_t)GetVariableName(child);
        VariableValue *storedValue = GetVariableValueAddress(child);
        if (storedValue->payload == 0)
            continue;

        const char *message = va(
            "animation '%s' not defined in anim tree '%s'",
            SL_ConvertToString(animName),
            SL_ConvertToString(treeName));
#if UINTPTR_MAX > UINT32_MAX
        script_anim_unresolved_ref_t *ref =
            (script_anim_unresolved_ref_t *)storedValue->payload;
        uint8_t *codePos = (uint8_t *)ref->target;
#else
        uint8_t *codePos = (uint8_t *)storedValue->payload;
#endif
        const qboolean isLoadedCode = ScriptCode_IsLoadedCodePos(codePos);
        const qboolean isDeveloperCode =
            Scr_IsInDeveloperOpcodeMemory(codePos);
        if (isLoadedCode == qfalse && isDeveloperCode == qfalse) {
            Com_Error(ERR_DROP, "\x15" "%s", message);
        } else {
            CompileError2(codePos, "%s", message);
        }
    }
}

/* Source: CoDUOMP.exe 0x004793f0..0x004794b0.
 * Name/signature: exact same-module Mac symbol
 * Scr_PrecacheAnimationTree(unsigned short). */
void Scr_PrecacheAnimationTree(uint16_t sourceHandle)
{
    for (uint16_t child = FindNextSibling(sourceHandle);
         child != 0; child = FindNextSibling(child)) {
        const uint32_t name = GetVariableName(child);
        if (name >= SCRIPT_VARIABLE_NODE_COUNT)
            continue;

        if (GetVarType(child) == SCRIPT_VAR_OBJECT) {
            Scr_PrecacheAnimationTree(FindObject(child));
        } else {
            XAnimLoadFile(SL_ConvertToString((uint16_t)name),
                          Hunk_AllocXAnimPrecache);
        }
    }
}

#if UINTPTR_MAX > UINT32_MAX
/* NOT_FROM_ORIGINAL_SOURCE: the i386 binary stores its temporary unresolved
 * animation-ref link in the 32-bit output field itself. Native builds keep
 * those host pointers in a pointer-width side node until the tree resolves. */
static script_anim_unresolved_ref_t *
coduo_script_compat_anim_alloc_unresolved_ref(
    char *target, script_anim_unresolved_ref_t *next)
{
    script_anim_unresolved_ref_t *ref = Z_MallocInternal(sizeof(*ref));

    ref->target = target;
    ref->next = next;
    ref->allocationNext = script_animUnresolvedSidecars;
    script_animUnresolvedSidecars = ref;
    return ref;
}
#endif

/* NOT_FROM_ORIGINAL_SOURCE: original i386 stores unresolved links in the
 * four-byte output cells and owns no side allocations. Native-width builds
 * release their compatibility sidecars at animation-load phase boundaries. */
void coduo_script_compat_anim_release_unresolved_ref_sidecars(void)
{
#if UINTPTR_MAX > UINT32_MAX
    while (script_animUnresolvedSidecars != NULL) {
        script_anim_unresolved_ref_t *ref = script_animUnresolvedSidecars;

        script_animUnresolvedSidecars = ref->allocationNext;
        Z_FreeInternal(ref);
    }
#endif
}

/* Source: CoDUOMP.exe 0x004785e0..0x00478676.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004785e0_00478677.mcode.
 * Name/signature: exact same-module Mac symbol
 * Scr_EmitAnimationInternal(char *, unsigned short, unsigned short,
 * unsigned int).  Windows and Mac each issue one four-byte store through this
 * byte-addressed output; memcpy preserves that value without asserting the
 * alignment that the packed script-code caller does not provide. */
void Scr_EmitAnimationInternal(char *animRef, uint16_t animHandle,
                               uint16_t treeHandle, uint32_t sourcePos)
{
    if (script_animCommentDepth != 0) {
        CompileError(
            sourcePos,
            "cannot reference animation from /# ... #/ comment");
        return;
    }

    uint16_t existing =
        FindVariable(treeHandle, animHandle);
    if (existing == 0) {
        VariableValue value;

        existing = GetVariable(treeHandle, animHandle);
        const uint32_t emptyRef = 0;
        memcpy(animRef, &emptyRef, sizeof(emptyRef));
#if UINTPTR_MAX > UINT32_MAX
        value.payload = (uintptr_t)
            coduo_script_compat_anim_alloc_unresolved_ref(
            animRef, NULL);
#else
        value.payload = (uintptr_t)animRef;
#endif
        value.type = SCRIPT_VAR_CODEPOS;
        SetVariableValue(existing, &value);
        return;
    }

    VariableValue *storedValue = GetVariableValueAddress(existing);
#if UINTPTR_MAX > UINT32_MAX
    script_anim_unresolved_ref_t *head =
        (script_anim_unresolved_ref_t *)storedValue->payload;

    const uint32_t emptyRef = 0;
    memcpy(animRef, &emptyRef, sizeof(emptyRef));
    storedValue->payload = (uintptr_t)
        coduo_script_compat_anim_alloc_unresolved_ref(animRef, head);
#else
    const uint32_t previousRef = (uint32_t)storedValue->payload;
    memcpy(animRef, &previousRef, sizeof(previousRef));
    storedValue->payload = (uintptr_t)animRef;
#endif
}

/* Source: CoDUOMP.exe 0x00478680..0x004786ab.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00478680_004786ac.mcode.
 * Name and signature: exact same-module Mac symbol
 * Scr_EmitAnimation(char *, unsigned short, unsigned int). The output pointer
 * addresses the four-byte animation reference emitted into script code. */
void Scr_EmitAnimation(char *animRef, uint16_t animHandle,
                       uint32_t sourcePos)
{
    /* 0x00478681 and the inlined copy at 0x0047b00b both load the currently
     * selected #using_animtree object at 0x009d67a0, not the registry root. */
    if (script_animCurrentUsingTree == 0) {
        CompileError(
            sourcePos, "#using_animtree was not specified");
        return;
    }

    Scr_EmitAnimationInternal(
        animRef, animHandle, script_animCurrentUsingTree, sourcePos);
}

/* Source: CoDUOMP.exe 0x004794b0..0x0047961e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004794b0_0047961f.mcode.
 * Name/signature: exact same-module Mac symbol
 * Scr_UsingTreeInternal(const char *, int *). */
uint16_t Scr_UsingTreeInternal(const char *filename, int32_t *treeIndex)
{
    uint16_t filenameHandle = Scr_CreateCanonicalFilename(filename);
    uint16_t treeSlot = FindVariable(
        script_animTreeRoot, filenameHandle);
    int32_t slot = xanim_activePoolPayloadSlot;
    uint16_t tree;

    if (treeSlot == 0) {
        /* NOT_FROM_ORIGINAL_SOURCE: index zero is reserved in each handle row.
         * Reject a new name when the one-based registry is full, before the
         * variable is published; existing names remain usable at capacity. */
        if (script_animTreeCounts[slot] >=
            SCRIPT_ANIM_TREE_REGISTERED_CAPACITY) {
            SL_RemoveRefToString(filenameHandle);
            Com_Error(ERR_DROP,
                      "\x15" "maximum animation tree count exceeded (%i)",
                      SCRIPT_ANIM_TREE_REGISTERED_CAPACITY);
            *treeIndex = 0;
            return 0;
        }

        treeSlot = GetVariable(script_animTreeRoot,
                                           filenameHandle);
        tree = GetObject(treeSlot);
        script_animTreeCounts[slot]++;
        script_animTreeHandles[slot][script_animTreeCounts[slot]] =
            treeSlot;
        *treeIndex = script_animTreeCounts[slot];
    } else {
        tree = FindObject(treeSlot);
        *treeIndex = 0;

        for (int32_t index = SCRIPT_ANIM_TREE_FIRST_TABLE_INDEX;
             index <= script_animTreeCounts[slot]; ++index) {
            if (script_animTreeHandles[slot][index] == treeSlot) {
                *treeIndex = index;
                break;
            }
        }
    }

    uint16_t root = GetVariable(tree, SCRIPT_ANIM_TREE_ROOT_NODE_INDEX);
    root = GetArray(root);
    SL_RemoveRefToString(filenameHandle);
    return root;
}

/* Source: CoDUOMP.exe 0x00479620..0x00479650, recovered from an exporter
 * function-boundary gap.
 * Name and signature: exact same-module Mac symbol
 * Scr_UsingTree(const char *, unsigned int). The bad-name diagnostic and both
 * destination globals are independently matched by the recovered Linux
 * engine implementation. */
void Scr_UsingTree(const char *animTreeName, uint32_t sourcePos)
{
    if (Scr_IsIdentifier(animTreeName) == qfalse) {
        CompileError(sourcePos, "bad anim tree name");
        return;
    }

    script_animCurrentUsingTree =
        Scr_UsingTreeInternal(
            animTreeName, &script_activeAnimTreeHandle);
}

/* Source: CoDUOMP.exe 0x00479660..0x00479707.
 * Name/signature: exact same-module Mac symbol
 * Scr_LoadAnimTreeInternal(const char *, unsigned short, unsigned short).
 * The Windows compiler inlines the high-temporary-memory reset. */
qboolean Scr_LoadAnimTreeInternal(const char *animTreeName,
                                  uint16_t parentHandle,
                                  uint16_t treeHandle)
{
    char filename[MAX_QPATH];

    /* NOT_FROM_ORIGINAL_SOURCE: require the complete animation-tree qpath and
     * terminator to fit; do not substitute a truncated asset name. */
    if (strlen(animTreeName) >
        sizeof(filename) - sizeof("animtrees/") - sizeof(".atr") + 1) {
        Com_Error(ERR_DROP, "\x15" "animation tree path is too long");
        return qfalse;
    }
    Com_sprintf(filename, sizeof(filename), "animtrees/%s.atr",
                animTreeName);
    const char *savedSourcePos = script_sourcePos;
    char *source = Scr_AddSourceBuffer(filename, NULL, NULL);
    if (source == NULL)
        return qfalse;

    const char *savedSourceFilename = script_sourceFilename;
    script_sourceFilename = filename;
    Scr_AnimTreeParse(source, parentHandle, treeHandle);
    SCRIPT_HUNK_CLEAR_TEMP_HIGH();
    const qboolean loaded =
        GetArraySize(parentHandle) != 0 ? qtrue : qfalse;
    script_sourceFilename = savedSourceFilename;
    script_sourcePos = savedSourcePos;
    return loaded;
}

/* Source: CoDUOMP.exe 0x00479710..0x0047997f.
 * Name/signature: exact same-module Mac symbol
 * Scr_LoadAnimTreeAtIndex(int, void *(*)(int)).  The maintained allocation
 * callback uses size_t so the same source naturally widens on native builds. */
void Scr_LoadAnimTreeAtIndex(int32_t treeIndex,
                             script_anim_tree_alloc_t alloc)
{
    const int32_t slot = xanim_activePoolPayloadSlot;
    const uint16_t treeRecord = script_animTreeHandles[slot][treeIndex];
    const uint16_t treeName =
        (uint16_t)GetVariableName(treeRecord);
    const uint16_t treeData = FindObject(treeRecord);

    uint16_t loadedTreeHandle = FindVariable(
        treeData, SCRIPT_ANIM_TREE_FIRST_CHILD_NODE_INDEX);
    if (loadedTreeHandle != 0)
        return;

    const uint16_t sourceHandle = FindVariable(
        treeData, SCRIPT_ANIM_TREE_ROOT_NODE_INDEX);
    if (sourceHandle == 0) {
        script_animTrees[slot][treeIndex] = NULL;
        return;
    }

    const uint16_t unresolvedRoot =
        FindObject(sourceHandle);
    script_animCurrentTreeRoot = Scr_AllocArray();
    if (Scr_LoadAnimTreeInternal(SL_ConvertToString(treeName),
                                 script_animCurrentTreeRoot,
                                 unresolvedRoot) == qfalse) {
        RemoveVariable(script_animTreeRoot, treeName);
        RemoveRefToObject(script_animCurrentTreeRoot);
        script_animCurrentTreeRoot = 0;
        script_animTrees[slot][treeIndex] = NULL;
        return;
    }

    const int32_t nodeCount =
        Scr_GetAnimTreeSize(script_animCurrentTreeRoot);
    const char *treeNameString = SL_ConvertToString(treeName);
    fileData_t *asset =
        FS_GetDataForFile("animtrees", treeNameString, ".atr");
    const char *name =
        asset != NULL ? asset->name : "(savegame)";
    XAnim *tree = XAnimAllocTree(name, (uint32_t)nodeCount, alloc);
    const uint16_t rootName = SL_GetStringOfLen(
        "root", SCRIPT_ANIM_STRING_USER_NONE, sizeof("root"),
        SCRIPT_ANIM_NAME_TYPE);

    ConnectScriptToAnim(unresolvedRoot, SCRIPT_ANIM_TREE_ROOT_NODE_INDEX,
                        treeName, rootName, treeIndex);
    SL_RemoveRefToString(rootName);
    Scr_CreateAnimationTree(script_animCurrentTreeRoot, unresolvedRoot,
                            tree, SCRIPT_ANIM_TREE_FIRST_CHILD_NODE_INDEX,
                            "root", SCRIPT_ANIM_TREE_ROOT_NODE_INDEX,
                            treeName, treeIndex);
    Scr_CheckAnimsDefined(unresolvedRoot, treeName);
    Scr_PrecacheAnimationTree(script_animCurrentTreeRoot);
    RemoveVariable(treeData, SCRIPT_ANIM_TREE_ROOT_NODE_INDEX);
    RemoveRefToObject(script_animCurrentTreeRoot);
    script_animCurrentTreeRoot = 0;

    VariableValue value = {
        .payload = (uintptr_t)tree,
        .type = SCRIPT_VAR_CODEPOS
    };
    loadedTreeHandle = GetVariable(
        treeData, SCRIPT_ANIM_TREE_FIRST_CHILD_NODE_INDEX);
    SetVariableValue(loadedTreeHandle, &value);
    XAnimSetupSyncNodes(tree);
    script_animTrees[slot][treeIndex] = tree;
}

/* Source: CoDUOMP.exe 0x00479980..0x00479a28.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00479980_00479a29.mcode.
 *
 * The Linux body at 0x0809d1de..0x0809d28f performs the same lookup but its
 * source ABI returns the one-pointer script_anim_tree_ref_t aggregate through
 * a hidden destination pointer. Keep the complete ABI-specific functions
 * separate; the lookup and returned pointer value remain identical. */
#if defined(WINDOWS_BEHAVIOR)
XAnim *CODUO_SCRIPT_CDECL Scr_FindAnimTree(const char *filename)
{
    uint16_t filenameHandle = Scr_CreateCanonicalFilename(filename);
    uint16_t treeSlot = FindVariable(
        script_animTreeRoot, filenameHandle);
    SL_RemoveRefToString(filenameHandle);

    if (treeSlot == 0) {
        return NULL;
    }

    uint16_t tree = FindObject(treeSlot);
    uint16_t rootChild = FindVariable(
        tree, SCRIPT_ANIM_TREE_FIRST_CHILD_NODE_INDEX);
    if (rootChild == 0) {
        return NULL;
    }

    return (XAnim *)GetVariableValueAddress(rootChild)->payload;
}
#else
script_anim_tree_ref_t CODUO_SCRIPT_CDECL
Scr_FindAnimTree(const char *filename)
{
    script_anim_tree_ref_t result = {NULL};
    uint16_t filenameHandle = Scr_CreateCanonicalFilename(filename);
    uint16_t treeSlot = FindVariable(
        script_animTreeRoot, filenameHandle);
    SL_RemoveRefToString(filenameHandle);

    if (treeSlot != 0) {
        uint16_t tree = FindObject(treeSlot);
        uint16_t rootChild = FindVariable(
            tree, SCRIPT_ANIM_TREE_FIRST_CHILD_NODE_INDEX);
        if (rootChild != 0) {
            result.tree =
                (XAnim *)GetVariableValueAddress(rootChild)->payload;
        }
    }

    return result;
}
#endif

/* Source: CoDUOMP.exe 0x00479a30..0x00479ab6.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00479a30_00479ab7.mcode. */
void CODUO_SCRIPT_CDECL Scr_FindAnim(const char *treeName,
                                    const char *animName,
                                    scr_anim_t *animRef)
{
    uint16_t animHandle = SL_GetLowercaseString_(
        animName, SCRIPT_ANIM_STRING_USER_NONE, SCRIPT_ANIM_NAME_TYPE);
    int32_t treeIndex;
    uint16_t treeHandle =
        Scr_UsingTreeInternal(treeName, &treeIndex);

    Scr_EmitAnimationInternal((char *)animRef, animHandle, treeHandle,
                              SCRIPT_ANIM_SOURCE_NONE);
    SL_RemoveRefToString(animHandle);
}
