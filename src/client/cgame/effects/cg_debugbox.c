// Source: uo_cgame_mp_x86.dll 0x3001d970..0x3001d9f0
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001d970_3001d9f0.mcode
//
// CG_DebugBox — draw the 12 edges of the axis-aligned box [mins, maxs] as debug
// lines in `color`. Same-module PPC bank (cgame_mp.dll) lists CG_DebugBox; the
// mechanical name guess "YawToAxis" is REJECTED (it is a size match only, takes
// (float yaw, float *axis), and does not build box corners or draw lines).
//
// Register-arg ABI (this i386 build): mins in EAX (moved to ESI), maxs in EDX,
// color in EBX, param in EDI. EBX and EDI are never written by this function;
// they arrive from the caller (e.g. 0x30006e94 sets EBX=&{0,0.5,0.5,1} and
// EDI=[0x30452e4c]) and are forwarded unchanged to every trap call. Modeled here
// as ordinary parameters. Callee saves/restores ESI (push esi / pop esi); the
// caller-side push of EDI/EBX at each call site is argument passing, not a save.
//
// Two-phase body:
//  1) corner build (EAX = 0..7): for each of the 8 box corners, select each of
//     the 3 float components from maxs if the matching low bit of the index is
//     set, else from mins:  X<-bit0, Y<-bit1, Z<-bit2. FLD/FSTP copy one float
//     (32-bit) at a time — no arithmetic, just component selection.
//  2) edge draw (ESI = 0,8,...,0x58; 12 iterations): read the corner-index pair
//     for edge i from cg_debugBoxEdges[i] and call the debug-line trap with the
//     two selected corners, forwarding color and param. Six syscall args are
//     pushed and cleaned by ADD ESP,0x18: (id, &corner[a], &corner[b], color,
//     param, 0).

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

void CG_DebugBox(const vec3_t mins, const vec3_t maxs, const float color[4], int param)
{
    /* The 8 corners of the box, built component-by-component below.
     * corners[i] lives at [esp+...]; the .mcode writes X, then Y, then Z. */
    vec3_t corners[8];
    int i;

    /* Phase 1: 3001d980..3001d9b1 — build the 8 corners.
     * For corner index i, low bit b selects component from maxs (bit set) or
     * mins (bit clear): X from bit 0, Y from bit 1, Z from bit 2. */
    for (i = 0; i < 8; i++) {          /* INC EAX; CMP EAX,8; JL */
        corners[i][0] = (i & 1) ? maxs[0] : mins[0];   /* TEST AL,1 -> [EDX]/[ESI] */
        corners[i][1] = (i & 2) ? maxs[1] : mins[1];   /* TEST AL,2 -> [EDX+4]/[ESI+4] */
        corners[i][2] = (i & 4) ? maxs[2] : mins[2];   /* TEST AL,4 -> [EDX+8]/[ESI+8] */
    }

    /* Phase 2: 3001d9b3..3001d9e9 — one debug line per box edge.
     * ESI steps 0,8,...,0x58 over cg_debugBoxEdges (8 bytes per entry); the loop
     * exits when ESI reaches 0x60 (CMP ESI,0x60; JC/JB back). That is exactly the
     * 12 entries of cg_debugBoxEdges[12][2]. */
    for (i = 0; i < 12; i++) {         /* ADD ESI,8; CMP ESI,0x60; JB */
        uint32_t a = cg_debugBoxEdges[i][0];   /* MOV EAX,[ESI+0x300718b8] */
        uint32_t b = cg_debugBoxEdges[i][1];   /* MOV EAX,[ESI+0x300718bc] */
        /* PUSH 0; PUSH EDI(param); PUSH EBX(color); PUSH ECX(&corner[b]);
         * PUSH EAX(&corner[a]); PUSH 0xca; CALL cgame_syscall; ADD ESP,0x18 */
        cgame_syscall(CG_ADD_DEBUG_LINE, (intptr_t)corners[a], (intptr_t)corners[b], (intptr_t)color, (int32_t)param, 0);
    }
}
