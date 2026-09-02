#include "../client_recovered.h"
#include "../globals.h"

#include <stddef.h>
#include <string.h>

// Source: uo_cgame_mp_x86.dll 0x3001ca20..0x3001ca9a
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001ca20_3001ca9a.mcode
//
// CG_DrawRect — draw the outline (border) of the virtual-640x480 rectangle
// (x, y, width, height) with thickness `size`, in `color` (an r,g,b,a float[4]).
// It sets the 2D draw color with trap_R_SetColor (cgame trap 72), passing a local
// float[4] whose r,g,b are copied straight from the caller's color and whose alpha
// is color[3] scaled by the global HUD fade factor cg_hudAlpha_vmCvar.value. It then draws
// the top+bottom bars (CG_DrawTopBottom) and the left+right bars (CG_DrawSides),
// each forwarded the same (x, y, width, height, size), and finally resets the draw
// color to white with trap_R_SetColor(NULL). This is the sibling of CG_FillRect
// named in the trap_R_SetColor evidence cluster.
//
// The .mcode's size-matched "AnglesSubtract" guess is rejected: the body sets a
// draw color and issues two HUD line-bar draws bracketed by trap(72,...); it does
// not subtract angles. Same-module PPC bank lists cgame_mp!CG_DrawRect (which calls
// CG_DrawTopBottom then CG_DrawSides), matching this call graph.
//
// Machine-code proof (all args ESP-relative on entry; no frame pointer). Let E be
// the entry ESP, so [E+4]=x, [E+8]=y, [E+0xC]=width, [E+0x10]=height, [E+0x14]=size,
// [E+0x18]=color. SUB ESP,0x10 reserves the local float[4] `dc` (at [E-0x10..E-4]):
//   MOV EAX,[E+0x18]                        ; color pointer (6th arg)
//   FLD  [0x304583c8]                       ; cg_hudAlpha_vmCvar.value
//   MOV EDX,[EAX+0x4]                        ; color[1] (g)
//   FMUL [EAX+0xc]                           ; cg_hudAlpha_vmCvar.value * color[3] (alpha)
//   MOV ECX,[EAX]                            ; color[0] (r)
//   PUSH EBX/EBP                             ; callee-saved
//   FSTP [ESP+0x14] (= [E-4])                ; dc[3] = fadeAlphaScale*alpha
//   PUSH ESI
//   MOV [ESP+0x10] (= [E-0xc]),EDX           ; dc[1] = color[1]
//   PUSH EDI
//   LEA EDX,[ESP+0x10] (= [E-0x10])          ; EDX = &dc
//   MOV [ESP+0x10] (= [E-0x10]),ECX          ; dc[0] = color[0]
//   MOV ECX,[EAX+0x8]                         ; color[2] (b)
//   PUSH EDX / PUSH 0x48 / CALL [cgame_syscall]  ; trap_R_SetColor(dc)   (0x48=72)
//   MOV [ESP+0x20] (= [E-0x8]),ECX           ; dc[2] = color[2]  (stored before the call reads dc)
//   MOV ESI,[E+0x14]=size ; EDI=[E+0x10]=height ; EBX=[E+0xc]=width ; EBP=[E+8]=y ; EAX=[E+4]=x
//   PUSH ESI/EDI/EBX/EBP/EAX / CALL 0x3001c980   ; CG_DrawTopBottom(x,y,width,height,size)
//   MOV ECX,[E+4]=x  (EAX was clobbered by the call; x reloaded)
//   PUSH ESI/EDI/EBX/EBP/ECX / CALL 0x3001c8e0   ; CG_DrawSides(x,y,width,height,size)
//   PUSH 0 / PUSH 0x48 / CALL [cgame_syscall]    ; trap_R_SetColor(NULL)
//   ADD ESP,0x38 ; POP EDI/ESI/EBP/EBX ; ADD ESP,0x10 ; RET   ; caller-cleaned (cdecl)
//
// dc[2] is written after the trap_R_SetColor(dc) PUSHes but before CALL executes
// (0x3001ca4f MOV precedes 0x3001ca53 CALL), so all four components are valid when
// the trap reads them. Only the alpha component is scaled; r,g,b pass through raw.

void CG_DrawRect(float x, float y, float width, float height, float size,
                 const float *color)
{
    vec4_t dc;
    long double hudAlpha = (long double)cg_hudAlpha_vmCvar.value;
    uint32_t greenBits;
    uint32_t redBits;
    uint32_t blueBits;
    float alpha;

    /* 0x3001ca2d loads green as a raw dword while the HUD-alpha carrier is
     * live, then performs the alpha multiply, then loads red. Blue is not read
     * until immediately before the direct syscall. */
    memcpy(&greenBits, &color[1], sizeof(greenBits));
    alpha = (float)(hudAlpha * (long double)color[3]);
    memcpy(&redBits, &color[0], sizeof(redBits));
    dc[3] = alpha;
    memcpy(&dc[1], &greenBits, sizeof(dc[1]));
    memcpy(&dc[0], &redBits, sizeof(dc[0]));
    memcpy(&blueBits, &color[2], sizeof(blueBits));
    memcpy(&dc[2], &blueBits, sizeof(dc[2]));

    cgame_syscall(CG_R_SETCOLOR, (intptr_t)dc);

    /* The first call snapshots size, height, width, y, x in that order. Those
     * four non-x values remain live across the call; only x is reloaded for the
     * sibling call. */
    {
        float sizeArg = size;
        float heightArg = height;
        float widthArg = width;
        float yArg = y;
        float xArg = x;
        CG_DrawTopBottom(xArg, yArg, widthArg, heightArg, sizeArg);
        xArg = x;
        CG_DrawSides(xArg, yArg, widthArg, heightArg, sizeArg);
    }

    cgame_syscall(CG_R_SETCOLOR, 0);
}
