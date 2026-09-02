#include "../client_recovered.h"
#include "../globals.h"
#include "qcommon/collision_map_types.h"

#include <stdint.h>

enum {
    CG_VIEW_INFO_TEXT_CAPACITY = 4096
};

// Source: uo_cgame_mp_x86.dll 0x3001b2b0..0x3001b35a
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001b2b0_3001b35a.mcode
//
// CG_DrawViewInfoOverlay — a cgame client HUD debug/diagnostic overlay. It asks
// the engine (cgame trap 110) to format the current view/camera state into three
// text lines, and if that query succeeds it draws the three lines as stacked HUD
// text elements at x=8, y=240/256/272 via CG_Trap54DrawElement (the trap-54 2D
// draw emitter, 0x3001cff0).
//
// Naming: the .mcode header guess `script_method_player_setplayerangles` is
// REJECTED. That is a server-side GScr script method that sets a player's view
// angles; this function takes no arguments, sets nothing, and is a cgame-client
// draw routine that issues cgame VM syscalls (trap 110 then three trap-54 draws)
// through the cgame_syscall pointer *0x30085e9c. The match was pure size
// (win 0xaa == server 0xaa), which the contract forbids as a naming basis. The
// exact original CoD symbol is unresolved (no cgame syscall-id table recovered to
// identify the trap-110 service), so the function carries a role name.
//
// Machine-code structure (proven instruction-by-instruction; let P be the frame
// base, i.e. ESP right after __chkstk reserves the 0x2044-byte frame):
//
//   3001b2b0 MOV EAX,0x2044 ; CALL 0x30060a30   __chkstk: probe/reserve the
//                                                0x2044-byte frame (compiler).
//   3001b2ba MOV EAX,[0x30081650]  \  MSVC /GS prologue: snapshot __security_cookie
//   3001b2c4 MOV [ESP+0x2044],EAX  /  into the frame's canary slot (frame+0x2040).
//
//   -- trap 110 query, args pushed high-address-first (= reverse arg order) --
//   3001b2bf PUSH 0x1000               bufSize = 4096
//   3001b2cb LEA EAX,[ESP+0x1044] ; PUSH EAX     &lineC   (P + 0x1040)
//   3001b2d3 LEA ECX,[ESP+0x48]   ; PUSH ECX     &lineB   (P + 0x40)
//   3001b2d8 LEA EDX,[ESP+0xc]    ; PUSH EDX     &lineA   (P + 0)
//   3001b2dd PUSH 0x30487a9c            &(refdef scalar after cg_refdef.vieworg)
//   3001b2e2 PUSH 0x30487a90            &cg_refdef.vieworg (camera origin vec3)
//   3001b2e7 PUSH 0x6e                  trap id CG_GET_VIEW_INFO (110)
//   3001b2e9 CALL *0x30085e9c           EAX = cgame_syscall(110, ...)
//   3001b2ef ADD ESP,0x1c               caller-clean the 7 pushed dwords
//   3001b2f2 TEST EAX,EAX ; JZ 0x3001b347   skip the draws when the query fails
//
//   -- three HUD text lines (only when the query returned nonzero) --
//   3001b2f6 PUSH 0x3f800000 (1.0f)            \ CG_Trap54DrawElement(
//   3001b2fb LEA EAX,[ESP+0x4] ; PUSH EAX       (=&lineA, P+0)   8.0f, 240.0f,
//   3001b300 PUSH 0x43700000 (240.0f)            &lineA, 1.0f)
//   3001b305 PUSH 0x41000000 (8.0f)            /
//   3001b30a CALL 0x3001cff0
//   3001b30f PUSH 0x3f800000 (1.0f)            \ CG_Trap54DrawElement(
//   3001b314 LEA ECX,[ESP+0x54] ; PUSH ECX      (=&lineB, P+0x40) 8.0f, 256.0f,
//   3001b319 PUSH 0x43800000 (256.0f)            &lineB, 1.0f)
//   3001b31e PUSH 0x41000000 (8.0f)            /
//   3001b323 CALL 0x3001cff0
//   3001b328 PUSH 0x3f800000 (1.0f)            \ CG_Trap54DrawElement(
//   3001b32d LEA EDX,[ESP+0x1064] ; PUSH EDX    (=&lineC, P+0x1040) 8.0f, 272.0f,
//   3001b335 PUSH 0x43880000 (272.0f)            &lineC, 1.0f)
//   3001b33a PUSH 0x41000000 (8.0f)            /
//   3001b33f CALL 0x3001cff0
//   3001b344 ADD ESP,0x30               caller-clean all 12 dwords (3 x 4 args).
//
//   3001b347 MOV ECX,[ESP+0x2040]  \  /GS epilogue: reload the canary and verify
//   3001b34e CALL 0x30061639       /  via __security_check_cookie.
//   3001b353 ADD ESP,0x2044 ; RET     release the frame, return (void).
//
// The __chkstk probe (0x3001b2b0), the cookie snapshot (0x3001b2ba/0x3001b2c4),
// and the reload+check epilogue (0x3001b347/0x3001b34e) are compiler-generated
// MSVC /GS + large-frame code, emitted because this function owns the large stack
// buffers; they are not source statements and are omitted from the body below.
//
// Argument order to CG_Trap54DrawElement (proven from the reversed push order and
// the callee reconstruction in FUN_3001cff0_3001d06f.c): the 8.0f (0x41000000)
// bit pattern is the `position` dword forwarded opaquely; the 240/256/272 float is
// `yBase` (the callee adds +14.0f); the LEA'd buffer is `data`; the 1.0f
// (0x3f800000) is the trailing `flags` dword. The floats are passed as their raw
// 32-bit bit patterns through the trap ABI, matching the callee.
//
// The trap-110 argument at 0x30487a9c is the refdef scalar immediately following
// the cg_refdef.vieworg vec3 (0x30487a90..0x30487a98). Its precise identity is not
// resolved here (it is only forwarded as a pointer to the unrecovered trap-110
// service), so it is referenced by its existing mechanical globals.h name.

void CG_DrawViewInfoOverlay(void)
{
    /* Three text buffers the trap-110 query fills. Their sizes come from the
     * frame layout (materialName at frame+0, surfaceFlags at frame+0x40,
     * contents at frame+0x1040) and the 4096-byte size argument passed for the
     * latter two. The material-name capacity is the fixed BSP field extent. */
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    char materialName[BSP_SHADER_NAME_FIELD_SIZE];
    char surfaceFlags[CG_VIEW_INFO_TEXT_CAPACITY];
    char contents[CG_VIEW_INFO_TEXT_CAPACITY];

    /* cgame_syscall(110, &cg_refdef.vieworg, &cg_refdef.viewaxis[0],
     *               materialName, surfaceFlags, contents, 4096)
     * returns nonzero on success.
     * The two global vec3 addresses (view origin + forward direction, adjacent in
     * memory) are forwarded as pointers; the three buffers are output text lines. */
    if (coduo_int32_from_bits((uint32_t)cgame_syscall(CG_GET_VIEW_INFO, (intptr_t)cg_refdef.vieworg, (intptr_t)cg_refdef.viewaxis[0],
                                                      (intptr_t)materialName, (intptr_t)surfaceFlags, (intptr_t)contents,
                                                      (intptr_t)sizeof(surfaceFlags))) != 0) {
        /* Draw the three filled lines as stacked HUD text elements. position is
         * the 8.0f dword forwarded opaquely; yBase steps 240 -> 256 -> 272; flags
         * is the 1.0f dword. */
        CG_DrawSmallString(8.0f, 240.0f, materialName, 1.0f);
        CG_DrawSmallString(8.0f, 256.0f, surfaceFlags, 1.0f);
        CG_DrawSmallString(8.0f, 272.0f, contents, 1.0f);
    }
}
