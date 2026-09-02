// Source: uo_cgame_mp_x86.dll 0x3001bee0..0x3001bfd9
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001bee0_3001bfd9.mcode

#include "../client_recovered.h"

#include <stdint.h>

/*
 * CG_DrawFollowingMessage (0x3001bee0)
 *
 * Draws the horizontally-centered "following <player>" HUD banner while the local
 * player is spectating another client in first person. It is the follow-mode twin
 * of CG_DrawSpectatorMessage (0x3001b720): identical measure/center/draw shape
 * (cgame trap 52 to measure, trap 54 to draw a centered white string), but it
 * first gates on the follow flag, resolves the followed player's name, and builds
 * the banner text through the localize service (trap 57) instead of translating a
 * fixed key.
 *
 * Name adjudication: the .mcode header's size-matched guess
 * "script_method_hudelem_moveovertime" is REJECTED. This is not a hudElem script
 * method: it takes no script/console arguments, parses nothing, sets no start/end
 * lerp fields, and performs no interpolation. Its behavior is a fixed 2D HUD draw
 * driven off cg_snap. The guess was purely a size collision (win size 0xf9 == a
 * game_mp_uo function of the same byte count) with no behavioral basis. The engine
 * services behind traps 52/54/57 are unproven (no cgame syscall-id table
 * recovered), so they keep the honest CG_R_TEXT_WIDTH/54/57 role ids used across the
 * emitter family (see client_recovered.h); this function keeps a behavioral name.
 *
 * Follow gate, proven 0x3001bee3..0x3001bf16:
 *   EAX = cg_snap;                                 (MOV EAX,[0x30459160])
 *   if (!(cg_snap->ps.playerStateFlags & 0x40000))  (TEST [EAX+0x18],0x40000; JNZ)
 *       return 0;                                  (XOR EAX,EAX; ADD ESP,0x20; RET)
 * 0x40000 is PSF_FOLLOWING (the follow-view flag; see globals.h). The
 * scratch color slots at [ESP+0x10..0x1c] are pre-filled with 1.0f (0x3f800000)
 * before the gate is even tested; they become the white draw color further down.
 *
 * Followed-name resolution, proven 0x3001bf17..0x3001bf3c:
 *   idx  = cg_snap->ps.psClientNum;                     (MOV EAX,[EAX+0xe0])
 *   elem = &bgs.clientinfo[idx];               (IMUL EAX,0x4d0; base 0x305e1f34)
 *   name = elem->infoValid ? elem->name : "?";
 *      (MOV ECX,[EAX+0x305e1f34] == elem->infoValid (+0x00); TEST ECX,ECX;
 *       JZ -> PUSH "?" (0x30076a14); else LEA EAX,[EAX+0x305e1f40] == &elem->name (+0x40))
 * The "?" fallback string is the 1-char literal at 0x30076a14 (byte 0x3f, NUL).
 *
 * Banner text, proven 0x3001bf3c..0x3001bf54:
 *   text = cgame_syscall(CG_SE_LOCALIZE_MESSAGE,
 *                        va("CGAME_FOLLOWING\x15: %s", name),  // 0x30076a18 fmt
 *                        "spectator follow string");           // 0x30076a30 key
 * va (0x3004e8a0, q_shared ring-buffer formatter) is called with (fmt, name); its
 * two arg dwords stay on the stack and are cleaned together with the three trap-57
 * arg dwords by the single ADD ESP,0x14 at 0x3001bf54. The format string embeds a
 * literal control byte 0x15 between the "CGAME_FOLLOWING" token and ": %s"
 * (matching the .rdata bytes exactly). trap 57 returns the localized text (-> ESI).
 *
 * Measure (trap 52), proven from the push trace 0x3001bf57..0x3001bf85
 * (5 dwords: id + 4 args, low id first):
 *   width = cgame_syscall(CG_R_TEXT_WIDTH, text, 0, CG_FloatBits(1/3), 0);
 * The 1/3 scale (0x3eaaaaab) is forwarded as a raw float dword built in a stack
 * slot ([ESP+0x8]) and PUSHed by value, mirroring the sibling measure site.
 *
 * Center computation, proven 0x3001bf8f..0x3001bfba:
 *   EDX = 640 - width;               (MOV EDX,0x280; SUB EDX,EAX)   integer subtract
 *   FILD  (640 - width)              st0 = (float)(640 - width)     (0x3001bfa3)
 *   FMUL  [0x3007bce8] = 0.5f        st0 = (640 - width) * 0.5f      (0x3001bfb0)
 *   FSTP  x                          x = centered coordinate         (0x3001bfba)
 * Unlike CG_DrawSpectatorMessage (which does the whole thing in x87 with 640.0f as
 * an .rdata float and FSUBR), this variant subtracts 640 as an INTEGER first, then
 * FILDs the result. 0.5f is the exact .rdata constant at 0x3007bce8 (dumped:
 * 0x3007bce0=1.0,bce4=2.0,bce8=0.5,bcec=0.0 — the 0.5f slot, not an inferred one).
 *
 * Draw (trap 54), proven from the push trace 0x3001bf8b..0x3001bfc6
 * (10 dwords: id + 9 args, low id first). Stack slots resolved by absolute-ESP
 * tracking through all pushes:
 *   cgame_syscall(CG_R_TEXT_PAINT,
 *                 CG_FloatBits(x),        // centered x           (PUSH ECX 0x3001bfc3)
 *                 CG_FloatBits(404.0f),   // y = 404.0f (0x43ca0000, PUSH EAX 0x3001bfbe)
 *                 0,                       // style slot (0 here)  (PUSH EAX 0x3001bf9a)
 *                 CG_FloatBits(1/3),       // scale                (PUSH EDX 0x3001bfb7)
 *                 &color,                  // white rgba vec4      (PUSH ECX 0x3001bfb6)
 *                 text,                    // localized text       (PUSH ESI 0x3001bfab)
 *                 0,                       //                      (PUSH 0   0x3001bfb8)
 *                 0,                       //                      (PUSH 0   0x3001bf8d)
 *                 3);                      // mode                 (PUSH 3   0x3001bf8b)
 * The style slot is 0 here (vs 4 in CG_DrawSpectatorMessage) and y is 404.0f (vs
 * 443.0f); every other slot matches the sibling's 10-slot family shape. The color
 * argument points at the local vec4 white {1,1,1,1} written at 0x3001bee8..0x3001bf00
 * (four 0x3f800000 dwords) and passed by address (LEA ECX,[ESP+0x38] at 0x3001bfac).
 *
 * Float dwords are forwarded to the variadic traps by raw bit pattern (the i386
 * code PUSHes the 4-byte float word, never promoted to double); CG_FloatBits
 * reproduces that exactly, matching the sibling emitters.
 *
 * Return value: 0 on the no-follow early-out, 1 after drawing (MOV EAX,0x1 at
 * 0x3001bfcf). Modelled as qboolean (qfalse/qtrue) — "did it draw?".
 *
 * ABI: no incoming source arguments (the frame is pure scratch). The trailing
 * ADD ESP,0x3c unwinds the draw-path scratch plus the pushed trap args; the final
 * POP ESI / ADD ESP,0x20 restore the PUSH ESI + SUB ESP,0x20 prologue, then a plain
 * RET (no callee cleanup of caller args).
 */

/* Fixed CG_R_TEXT_PAINT draw parameters, proven from the pushed immediates. */
enum {
    CG_FOLLOW_STYLE = 0, /* style slot (0 here; 4 in CG_DrawSpectatorMessage)      */
    CG_FOLLOW_MODE = 3, /* trailing PUSH 3 (int) mode                             */
};
#define CG_FOLLOW_SCREEN_WIDTH 640          /* 0x280; integer centering width       */
#define CG_FOLLOW_HALF 0.5f         /* 0x3007bce8; center = (w - width)*0.5f */
#define CG_FOLLOW_Y 404.0f       /* 0x43ca0000; fixed y coordinate        */
#define CG_FOLLOW_SCALE (1.0f / 3.0f)/* 0x3eaaaaab; measure + draw scale      */

/*
 * The follow-banner format ("CGAME_FOLLOWING" localize token, a literal 0x15
 * control byte, then ": %s"). The 0x15 byte is in the original .rdata literal
 * (0x30076a18) and is preserved exactly. The paired key "spectator follow string"
 * (0x30076a30) is the localize-service reference argument. These two ASCII
 * literals are single-use to this function, so they stay file-local.
 */
static const char CG_FOLLOW_FORMAT[] = "CGAME_FOLLOWING\x15: %s";
static const char CG_FOLLOW_KEY[] = "spectator follow string";

qboolean CG_DrawFollowingMessage(void)
{
    /* 0x3001bee3 snapshots cg_snap before writing the color, then uses that
     * exact pointer for both the flag and followed-client-number reads. */
    snapshot_t *snap = cg_snap;
    vec4_t color = {1.0f, 1.0f, 1.0f, 1.0f};
    const char *name;
    const char *text;
    int32_t width;
    float x;

    /* Only draw the banner while following another client in first person. */
    if (!(snap->ps.playerStateFlags & PSF_FOLLOWING)) {
        return qfalse;
    }

    /* Resolve the followed client's name; "?" when that slot has no live state. */
    {
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
        const int32_t clientNum = snap->ps.psClientNum;
        if ((uint32_t)clientNum >= (uint32_t)MAX_CLIENTS) {
            Com_Error(ERR_DROP,
                      "\x15"
                      "CG_DrawFollowingMessage: "
                      "invalid client number %i",
                      clientNum);
            return qfalse;
        }
        clientInfo_t *elem = &bgs.clientinfo[clientNum];
        name = elem->infoValid ? elem->name : "?";
    }

    /* Localize "following <name>" through the config-string localize service. */
    text = (const char *)(intptr_t)cgame_syscall(CG_SE_LOCALIZE_MESSAGE, (intptr_t)va(CG_FOLLOW_FORMAT, name), (intptr_t)CG_FOLLOW_KEY);

    /* Measure the localized banner at the draw scale (trap 52). */
    width =
        coduo_int32_from_bits((uint32_t)cgame_syscall(CG_R_TEXT_WIDTH, (intptr_t)text, CG_FOLLOW_STYLE, CG_FloatBits(CG_FOLLOW_SCALE), 0));

    /* Center horizontally: SUB wraps in a dword before FILD, then the x87 value
     * is multiplied by the binary32 0.5 constant and stored as binary32. */
    {
        int32_t centeredWidth = coduo_int32_from_bits((uint32_t)CG_FOLLOW_SCREEN_WIDTH - (uint32_t)width);
        x = (float)((long double)centeredWidth * (long double)CG_FOLLOW_HALF);
    }

    cgame_syscall(CG_R_TEXT_PAINT, CG_FloatBits(x), CG_FloatBits(CG_FOLLOW_Y), CG_FOLLOW_STYLE, CG_FloatBits(CG_FOLLOW_SCALE),
                  (intptr_t)color, (intptr_t)text, 0, 0, CG_FOLLOW_MODE);

    return qtrue;
}
