// Source: uo_cgame_mp_x86.dll 0x300316b0..0x300316f1
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300316b0_300316f1.mcode

#include "../client_recovered.h"

#include <stddef.h>
#include <stdint.h>

/* Layout guards proving the machine-code offsets/stride this function relies on
 * (verified at 4-byte i386 pointer width). The 0x4d0 element stride matches the
 * `IMUL EAX,EAX,0x4d0` at 0x300316bb; the field offsets match the [EAX+0x305e1f34]
 * (infoValid, +0x00) and [EAX+0x305e1f60] (team, +0x2c) accesses. */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#if UINTPTR_MAX == 0xFFFFFFFFu
_Static_assert(sizeof(clientInfo_t) == 0x4d0, "bgs.clientinfo element stride");
#endif
_Static_assert(offsetof(clientInfo_t, infoValid) == 0x00, "infoValid +0x00");
_Static_assert(offsetof(clientInfo_t, team) == 0x2c, "team +0x2c");
_Static_assert(offsetof(snapshot_t, ps.psClientNum) == 0xe0, "cg_snap->ps.psClientNum +0xe0");
_Static_assert(offsetof(rectDef_t, h) == 0x0c, "rect->h +0x0c");
#endif

/*
 * CG_EmitLocalTeamBackground (0x300316b0) — a member of the same HUD emit family as
 * CG_DrawPlayerLocation (0x30031280). It draws the team-colored background bar behind
 * the local player's HUD element:
 *
 *   ps = &bgs.clientinfo[cg_snap->ps.psClientNum];  // stride 0x4d0, base 0x305e1f34
 *   if (ps->infoValid == 0) return;          // no valid local state -> draw nothing
 *   CG_DrawTeamBackground(ps->team,             // element +0x2c (base 0x305e1f60)
 *                         rect->x, rect->y,
 *                         rect->w, rect->h,
 *                         color[3]);               // alpha at color +0x0c
 *
 * CG_DrawTeamBackground (0x30017dd0) selects a team color (team 1 -> red, team 2 -> blue,
 * otherwise draws nothing) and stretch-pics the cgs.media hudColorBar shader scaled to the
 * screen; its `team` argument arrives in EAX (i386 __usercall), which is why this function
 * loads ps->team into EAX right before the CALL.
 *
 * ABI (proven from the sole caller at 0x30032344): the rect object arrives in ECX
 * (`LEA ECX,[ESP+0x10]` at the caller, a rectDef_t holding four floats x,y,w,h), and a
 * single cdecl stack word `color` (the owner-draw RGBA vector) is pushed; this function
 * reads only color[3] as the draw alpha. The trailing `ADD ESP,0x14` unwinds the five
 * dwords pushed for CG_DrawTeamBackground; the caller's `ADD ESP,0x4` cleans the one
 * incoming stack word. Ends in a plain RET.
 *
 * Name adjudication: the .mcode header's size-matched "CMD_VEH_MakeVehicleUsable" guess is
 * REJECTED — this function performs no vehicle-state write and issues no command; it reads
 * the local player's per-client state and forwards a rect + team to a 2D draw helper. It is
 * the local-player team-background member of the HUD emit family; provisional name by proven
 * role, exact CoD symbol unresolved. ps->team is the +0x2c per-client field folded into
 * clientInfo_t (supersedes the mechanical g_data_cg_asset_parse_305e1f60 fragment).
 *
 * Instruction map:
 *   300316b0 MOV  EAX,[0x30459160]        EAX = cg_snap
 *   300316b5 MOV  EAX,[EAX+0xe0]          EAX = cg_snap->ps.psClientNum
 *   300316bb IMUL EAX,EAX,0x4d0           EAX = clientNum * sizeof(clientInfo_t)
 *   300316c1 MOV  EDX,[EAX+0x305e1f34]    EDX = ps->infoValid
 *   300316c7 TEST EDX,EDX / 300316c9 JZ 0x300316f0  if (infoValid == 0) -> return
 *   300316cb MOV  EDX,[ESP+0x4]           EDX = color
 *   300316cf MOV  EDX,[EDX+0xc]           EDX = color[3] (alpha)
 *   300316d2 MOV  EAX,[EAX+0x305e1f60]    EAX = ps->team  (CG_DrawTeamBackground team, in EAX)
 *   300316d8 PUSH EDX                     push alpha        (5th stack arg)
 *   300316d9 MOV  EDX,[ECX+0xc] / 300316dc PUSH EDX         push rect->h (f_c)
 *   300316dd MOV  EDX,[ECX+0x8] / 300316e0 PUSH EDX         push rect->w (f_8)
 *   300316e1 MOV  EDX,[ECX+0x4]           EDX = rect->y (f_4)
 *   300316e4 MOV  ECX,[ECX]               ECX = rect->x (f_0)
 *   300316e6 PUSH EDX                     push rect->y
 *   300316e7 PUSH ECX                     push rect->x
 *   300316e8 CALL 0x30017dd0              CG_DrawTeamBackground(EAX=team, x,y,w,h,alpha)
 *   300316ed ADD  ESP,0x14                unwind the five pushed dwords
 *   300316f0 RET
 */
void CG_EmitLocalTeamBackground(rectDef_t *rect, const vec4_t color)
{
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    const int32_t clientNum = cg_snap->ps.psClientNum;
    if ((uint32_t)clientNum >= (uint32_t)MAX_CLIENTS) {
        Com_Error(ERR_DROP,
                  "\x15" "CG_EmitLocalTeamBackground: "
                  "invalid client number %i",
                  clientNum);
        return;
    }
    const clientInfo_t *ps = &bgs.clientinfo[clientNum];

    /* No valid local player state yet -> draw nothing. */
    if (ps->infoValid == 0)
        return;

    /* Preserve 0x300316cb..0x300316e4 load order before the call: alpha,
     * team, then h/w/y/x from the rect. */
    const float alpha = color[3];
    const int32_t team = ps->team;
    const float h = rect->h;
    const float w = rect->w;
    const float y = rect->y;
    const float x = rect->x;

    CG_DrawTeamBackground(team, x, y, w, h, alpha);
}
