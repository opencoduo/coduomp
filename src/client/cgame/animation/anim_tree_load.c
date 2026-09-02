// Source: uo_cgame_mp_x86.dll 0x30016360..0x300163ce
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30016360_300163ce.mcode

#include "../client_recovered.h"

#include <stddef.h>
#include <stdint.h>

/*
 * CGScr_LoadAnimTrees (0x30016360) — top-level cgame script anim-tree loader. Drives
 * the full "multiplayer" player-animation-tree load: it opens the script anim-tree
 * load pass, registers the four body-part bone indices, installs the parse/load
 * context, precaches the trees through an engine allocator callback, resolves the
 * master tree by name, closes the load pass, finalizes per-part animation data, and
 * finally instantiates a per-entity anim tree for every entity slot.
 *
 * Name: BEHAVIOR + CALL GRAPH. The .mcode's size-matched "Q_rsqrt" guess is REJECTED
 * outright — this function contains no x87/float math, no reciprocal-sqrt bit trick;
 * it is a control-flow subsystem-init routine. It sits at the head of the anim-tree
 * loader family already recovered here (BG_FindAnims 0x30005b50,
 * BG_FindAnimTrees 0x30005be0, BG_LoadAnimTreeInstances 0x30005c40, all
 * of which reference it as their caller) and issues the script Begin/Precache/End
 * anim-tree traps. The same-module PPC bank names this exact cgame function
 * CGScr_LoadAnimTrees; adopted (behavioral evidence, not size).
 *
 * Machine-code trace (0x30016360..0x300163cd):
 *   mov  eax,0x9008 ; call __chkstk        ; probe/reserve the 0x9008-byte frame
 *   mov  eax,[__security_cookie] ; mov [frame+0x9004],eax   ; /GS cookie install
 *   mov  dword [esp],0                      ; loadCtxA = NULL (stack local at frame+0)
 *   call [cg_scriptImports.beginLoadAnimTrees] ; open the anim-tree load pass
 *   call BG_FindAnims                       ; 0x30005b50
 *   lea  eax,[esp] ; push eax               ; &runtimeAnimationCount
 *   lea  ecx,[esp+8] ; push ecx             ; runtimeAnimations (frame+4; esp already
 *                                            ; -4 from pushing &runtimeAnimationCount)
 *   push 0x3053a440 ; call CGScr_InitAnimTreeParse  ; install parse/load context
 *   push CG_AllocAnimTree ; call [cg_scriptImports.precacheAnimTrees]
 *   call BG_FindAnimTrees                   ; 0x30005be0 (resolves "multiplayer")
 *   call [cg_scriptImports.endLoadAnimTrees] ; close the anim-tree load pass
 *   call BG_FinalizePlayerAnims              ; 0x300016a0
 *   call BG_LoadAnimTreeInstances           ; 0x30005c40
 *   mov  ecx,[frame+0x9004] ; call __security_check_cookie ; /GS verify
 *   add  esp,0x9018 ; ret                    ; reclaim frame + pushed cdecl args
 *
 * The frame geometry proves its source locals rather than an opaque pair of context
 * words: frame+0x0000 is the zero-initialized runtime-animation count; frame+0x0004
 * begins 0x9000 bytes of bg_runtime_animation_t entries (512 * 0x48); and the /GS
 * cookie is at frame+0x9004. BG_AnimParseAnimScript retains pointers to the count
 * and array only for the duration of this load, through globals 0x300a5108 and
 * 0x300a7820. The __chkstk probe and /GS cookie are automatic MSVC boilerplate.
 *
 * The pushed cdecl arguments (3 to CGScr_InitAnimTreeParse, 1 to Scr_PrecacheAnimTrees
 * = 0x10 bytes) are never individually cleaned; they are reclaimed together with the
 * frame by the final `add esp,0x9018` (0x9008 reserved + 0x10 pushed).
 */
void CGScr_LoadAnimTrees(void)
{
    int32_t runtimeAnimationCount = 0; /* frame+0x0000 */
    bg_runtime_animation_t
        runtimeAnimations[BG_ANIM_MAX_ANIMATIONS]; /* frame+0x0004..+0x9003 */

    _Static_assert(sizeof(runtimeAnimations) == 0x9000,
                   "CGScr_LoadAnimTrees runtime array must match the original frame");

    Scr_BeginLoadAnimTrees();

    BG_FindAnims();

    BG_AnimParseAnimScript(&bgs.animationTable, runtimeAnimations,
                           &runtimeAnimationCount);

    Scr_PrecacheAnimTrees(CG_AllocAnimTree);

    BG_FindAnimTrees();

    Scr_EndLoadAnimTrees();

    BG_FinalizePlayerAnims();

    BG_LoadAnimTreeInstances();
}
