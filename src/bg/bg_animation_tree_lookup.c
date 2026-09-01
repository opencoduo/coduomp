#include "bg_animation.h"

#include "bg_animation_tree_binding.h"

/*
 * Complete shared player-animation tree lookup cluster.
 *
 * The Windows cgame/game bodies agree instruction for instruction apart from
 * relocated globals, imports, and strings:
 *
 *   uo_cgame_mp_x86.dll  BG_FindAnims     0x30005b50
 *   uo_game_mp_x86.dll   BG_FindAnims     0x20005900
 *   uo_cgame_mp_x86.dll  BG_FindAnimTree  0x30005bb0
 *   uo_game_mp_x86.dll   BG_FindAnimTree  0x20005960
 *   uo_cgame_mp_x86.dll  BG_FindAnimTrees 0x30005be0
 *   uo_game_mp_x86.dll   BG_FindAnimTrees 0x20005990
 *
 * Linux game RVAs 0x0001f830, 0x0001f8e8, and 0x0001f94a retain the same
 * calls, strings, error gate, and bgs field copies.  Its ABI returns the
 * one-pointer script_anim_tree_ref_t aggregate through a hidden destination;
 * the earlier recovered XAnim ** output parameter was that hidden return slot,
 * not an original source argument.  The public type follows the already-proven
 * Scr_FindAnimTree platform ABI while the returned tree pointer is identical.
 */

void BG_FindAnims(void)
{
    Scr_FindAnim("multiplayer", "root", &bgs.rootAnimHandle);
    Scr_FindAnim("multiplayer", "torso", &bgs.torsoAnimHandle);
    Scr_FindAnim("multiplayer", "legs", &bgs.legsAnimHandle);
    Scr_FindAnim("multiplayer", "turning", &bgs.turningAnimHandle);
}

#if defined(WINDOWS_BEHAVIOR)
XAnim *BG_FindAnimTree(const char *treeName, qboolean errorIfMissing)
{
    XAnim *tree = Scr_FindAnimTree(treeName);

    if (tree == NULL && errorIfMissing != qfalse) {
        Com_Error(ERR_DROP,
                  "\x15" "Could not find animation tree '%s'", treeName);
    }
    return tree;
}
#else
script_anim_tree_ref_t BG_FindAnimTree(const char *treeName,
                                       qboolean errorIfMissing)
{
    script_anim_tree_ref_t tree = Scr_FindAnimTree(treeName);

    if (tree.tree == NULL && errorIfMissing != qfalse) {
        Com_Error(ERR_DROP,
                  "\x15" "Could not find animation tree '%s'", treeName);
    }
    return tree;
}
#endif

void BG_FindAnimTrees(void)
{
#if defined(WINDOWS_BEHAVIOR)
    bgs.multiplayerAnimTree = BG_FindAnimTree("multiplayer", qtrue);
#else
    bgs.multiplayerAnimTree = BG_FindAnimTree("multiplayer", qtrue).tree;
#endif
    bgs.animationTable.animTreeHandle = bgs.multiplayerAnimTree;
    bgs.resolvedTorsoAnimHandle = bgs.torsoAnimHandle;
    bgs.resolvedLegsAnimHandle = bgs.legsAnimHandle;
    bgs.resolvedTurningAnimHandle = bgs.turningAnimHandle;
}
