// Source: uo_cgame_mp_x86.dll 0x30005c40..0x30005c96
//         uo_game_mp_x86.dll  0x200059f0..0x20005a1f
//         game.mp.uo.i386.so  RVA 0x0001f9ea..0x0001fa4d

#include "bg_animation.h"

#include "bg_animation_services.h"

#include <stdint.h>

/*
 * BG_LoadAnimTreeInstances — create a per-client anim-tree instance from the
 * loaded master "multiplayer" animation tree.
 *
 * The loader-setup function immediately before this one (0x30005be0) resolves the
 * master tree by name ("multiplayer"; failure prints "Could not find animation
 * tree '%s'") and stores it in bgs.multiplayerAnimTree (0x305e1f20). This function
 * reads that master handle once and creates one tree for every element of the
 * common 64-row bgs.clientinfo table.  The Windows cgame and game loops are
 * instruction-identical apart from their globals and syscall ordinals; the
 * Linux game loop has the same 64 stores in its unoptimized PIC form.  The
 * supporting Mac game and cgame symbols retain the same canonical name.
 *
 * Cgame alone follows the common loop with eight corpse-row stores.  That
 * client-owned suffix remains in its local service adapter; the game adapter is
 * intentionally empty.  This preserves each complete original function while
 * keeping the non-common storage out of the shared source tree.
 *
 * This is not a hudelem destroy (the .mcode's broad-corpus
 * "script_method_hudelem_destroy" guess is rejected: it registers anim trees, never
 * touching a hudElem_t). It loads per-entity anim-tree instances, and the
 * symbolized Mac cgame names the corresponding function BG_LoadAnimTreeInstances.
 *
 * Cgame machine-code trace (0x30005c40..0x30005c95):
 *   push esi ; push edi                          ; save
 *   mov  edi,[bgs.multiplayerAnimTree]            ; master tree, loaded ONCE, held in EDI
 *   mov  esi,cg_playerAnimTrees                  ; ESI = &array[0]
 * loop1 (0x30005c50):                            ; lea ecx,[ecx] is a no-op alignment pad
 *   push edi ; push 134 ; call [cgame_syscall]   ; trap_XAnimCreateTree(master)
 *   mov  [esi],eax                               ; element.animTree = handle
 *   add  esi,0x4d0                               ; ++element (stride 0x4d0)
 *   add  esp,8                                   ; pop the 2 trap args
 *   cmp  esi,0x305f57f8 ; jl loop1               ; while &element < base + 64*0x4d0 (signed)
 *   mov  esi,0x3044cfc4                          ; ESI = &record[0].field_4c4
 * loop2 (0x30005c74):
 *   push edi ; push 134 ; call [cgame_syscall]   ; trap_XAnimCreateTree(master)
 *   mov  [esi],eax                               ; element.animTree = handle
 *   add  esi,0x4d0 ; add esp,8
 *   cmp  esi,0x3044f644 ; jl loop2               ; while &element < base + 8*0x4d0 (signed)
 *   pop  edi ; pop esi ; ret
 *
 * Both loops reuse the single EDI-held master handle (no reload). The bounds are
 * exact multiples of the 0x4d0 stride ((0x305f57f8-0x305e23f8)/0x4d0 = 64,
 * (0x3044f644-0x3044cfc4)/0x4d0 = 8), so the byte-pointer walk is a plain
 * array[i].animTree store. The compare is JL (signed), preserved as a signed index
 * loop; all addresses are below 0x80000000 so it never wraps in practice.
 */
void BG_LoadAnimTreeInstances(void)
{
    XAnim *masterTree = bgs.multiplayerAnimTree;
    int32_t i;

    for (i = 0; i < BGS_CLIENTINFO_COUNT; i++) {
        bgs.clientinfo[i].animTree = trap_XAnimCreateTree(masterTree);
    }

    bg_compat_load_additional_anim_tree_instances(masterTree);
}
