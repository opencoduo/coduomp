#include "../client_recovered.h"
#include "../globals.h"

// Source: uo_cgame_mp_x86.dll 0x30018a90..0x30018bbe
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30018a90_30018bbe.mcode
//
// CG_DrawDisconnect — draw the "connection interrupted" HUD warning (the centered
// CGAME_CONNECTIONINTERRUPTED text plus the blinking gfx/2d/net.tga icon) when the
// client's newest generated usercmd has not yet been acknowledged by a snapshot in
// a plausible time window (i.e. input is stalled / the link is dropping frames).
//
// The .mcode header's mechanical name `script_method_player_setreverb` is REJECTED:
// this is a server-side script-VM name matched only by byte size (0x12e). The body
// runs no script args and sets no reverb — it is a cgame HUD draw. The identity is
// proven by its data: it looks up the localized string "CGAME_CONNECTIONINTERRUPTED"
// (0x30076d00) via CG_SafeTranslateString_Internal and registers/draws the connection-lost icon
// shader "gfx/2d/net.tga" (0x30076cf0). The mechanical globals export even labelled
// the byte-direction table head (0x30085f20) owner=cg_drawdisconnect, corroborating
// that this address band is CG_DrawDisconnect.
//
// Stall gate (matches the id-Tech/CoD CG_DrawDisconnect logic):
//   cmdNum       = trap_GetCurrentCmdNumber();          // trap 0x53
//   trap_GetUserCmd(cmdNum - (CMD_BACKUP - 1), &cmd);   // trap 0x54; -0x7f
//   if (cmd.commandTime <= cg_snap->ps.commandTime) return;  // JLE: cmd already covered
//   if (cmd.commandTime >  cg.time)                return;  // JG : cmd is from the future
// Only when the oldest still-buffered cmd sits strictly between the last applied
// command time and now does the warning draw.
//
// Machine-code notes (all locals are ESP-relative; no frame pointer; caller-cleaned):
//   - trap 0x53 takes no args; EAX = current cmd number. The four 1.0f writes at
//     [ESP+0x14..0x20] lay down a white RGBA vector; the text-paint call later passes
//     its address (LEA EAX,[ESP+0x38] at 0x30018b69 after the intervening pushes).
//   - trap 0x54(cmdNum, &cmd) fills the usercmd buffer; cmd.commandTime is buffer+0x0,
//     read back from the stack slot at [ESP+0x30].
//   - text width: EAX = trap 52(str, 0, 1/3f, 0); FILD width; Q_rint -> nearest int.
//     x = (640 - roundedWidth) / 2  (SAR by 1 after CDQ/SUB, signed round-toward-zero).
//   - draw text: trap 54((float)x, 100.0f, 0, 1/3f, &scratch, str, 0, 0, 3).
//   - icon blink: `TEST AH,0x2` == (cg.time & 0x200); when set, skip the icon.
//   - icon: h = CG_RegisterMaterial("gfx/2d/net.tga", 5) (generic shader register);
//     CG_DrawPic(296.0f, 416.0f, 48.0f, 48.0f, h).

/* CMD_BACKUP is the id-Tech usercmd ring size (128). The oldest cmd still buffered
 * is `currentCmdNumber - CMD_BACKUP + 1`; the code subtracts 0x7f == CMD_BACKUP-1. */
enum {
    CMD_BACKUP = 128
};

/* cg.time blink mask: the net icon is shown only while this bit of the game clock is
 * clear, producing the ~2 Hz flash. (TEST AH,0x2 == cg.time & 0x200.) */
#define CG_DISCONNECT_BLINK_MASK ((uint32_t)0x200)

/* Text metrics/draw constants proven from the .rdata immediates in this function. */
enum {
    CG_DISCONNECT_TEXT_STYLE = 3,   /* trap-54 final mode word (PUSH 3) */
    CG_DISCONNECT_ICON_FLAG = 5,   /* CG_RegisterMaterial NoMip/type flag */
};
#define CG_VIRTUAL_SCREEN_WIDTH 640.0f   /* MOV ECX,0x280 == 640 */
#define CG_DISCONNECT_TEXT_SCALE (1.0f / 3.0f) /* 0x3eaaaaab */
#define CG_DISCONNECT_TEXT_Y 100.0f   /* 0x42c80000 */
#define CG_DISCONNECT_ICON_X 296.0f   /* 0x43940000 */
#define CG_DISCONNECT_ICON_Y 416.0f   /* 0x43d00000 */
#define CG_DISCONNECT_ICON_W 48.0f    /* 0x42400000 */
#define CG_DISCONNECT_ICON_H 48.0f    /* 0x42400000 */

void CG_DrawDisconnect(void)
{
    /* 0x30018a95..0x30018ab4: white RGBA at the local frame's +0x10..+0x1c.
     * 0x30018b69 later takes this exact local's address for CG_R_TEXT_PAINT. */
    vec4_t textColor = {1.0f, 1.0f, 1.0f, 1.0f};

    /* 0x30018a93..0x30018ab5: EAX = trap_GetCurrentCmdNumber(). */
    int32_t currentCmdNumber = coduo_int32_from_bits((uint32_t)cgame_syscall(CG_GET_CURRENT_CMD_NUMBER));

    /* 0x30018abb..0x30018ac6: fetch the oldest still-buffered usercmd into `cmd`.
     * The buffer's first dword is the cmd's commandTime. */
    usercmd_t cmd;
    cgame_syscall(CG_GET_USER_CMD, coduo_int32_from_bits((uint32_t)currentCmdNumber - (uint32_t)(CMD_BACKUP - 1)), (intptr_t)&cmd);
    int32_t cmdServerTime = cmd.commandTime; /* [ESP+0x30] = &cmd.commandTime */

    /* 0x30018acc..0x30018aea: draw only when the oldest buffered cmd is newer than the
     * last applied command time and not ahead of the current game clock. */
    if (cmdServerTime <= cg_snap->ps.commandTime) {   /* JLE 0x30018bba */
        return;
    }
    if (cmdServerTime > coduo_int32_from_bits(cg_time)) {  /* JG  0x30018bba */
        return;
    }

    /* 0x30018af1..0x30018b00: localized "connection interrupted" label. domain=EAX,
     * reference=ECX per CG_SafeTranslateString_Internal's register ABI. */
    char *text = CG_SafeTranslateString_Internal(cg_localizationContext, cg_connectionInterruptedLocalizationKey);

    /* 0x30018b02..0x30018b34: measure the label. trap 52 returns the rendered pixel
     * width in EAX for (text, 0, 1/3f, 0). */
    int32_t textWidthRaw =
        coduo_int32_from_bits((uint32_t)cgame_syscall(CG_R_TEXT_WIDTH, (intptr_t)text, 0, CG_FloatBits(CG_DISCONNECT_TEXT_SCALE), 0));

    /* 0x30018b38..0x30018b60: feed the integer width through _ftol2 (FILD width;
     * truncation is therefore an identity for every int32_t input) and
     * center it in the 640-wide virtual screen. The SAR-by-1 after CDQ/SUB is a signed
     * divide-by-2 rounding toward zero. 0x30018b38 FILD feeds _ftol2 (0x30018b3c)
     * directly with no FSTP DWORD, so the width enters exact -- no (float) cast. */
    int32_t textWidth = coduo_fp_to_i32_extended(textWidthRaw);
    int32_t centeredWidth = coduo_int32_from_bits(640u - (uint32_t)textWidth);
    int32_t x = centeredWidth / 2;

    /* 0x30018b48..0x30018b7d: draw the centered label at y=100 with scale 1/3.
     * Args mirror the trap-54 text/draw shape:
     *   (xFloat, yFloat, 0, scale, &scratch, text, 0, 0, style). */
    cgame_syscall(CG_R_TEXT_PAINT, CG_FloatBits((float)x),          /* FILD x; FSTP -> float bits */
                  CG_FloatBits(CG_DISCONNECT_TEXT_Y), 0, CG_FloatBits(CG_DISCONNECT_TEXT_SCALE), (intptr_t)&textColor[0], (intptr_t)text, 0,
                  0, CG_DISCONNECT_TEXT_STYLE);

    /* 0x30018b83..0x30018b8f: blink — hide the icon while cg.time bit 0x200 is set. */
    if (cg_time & CG_DISCONNECT_BLINK_MASK) {         /* JNZ 0x30018bba */
        return;
    }

    /* 0x30018b91..0x30018bb7: register and draw the connection-lost net icon. */
    qhandle_t icon = CG_RegisterMaterial(cg_connectionInterruptedIconPath, CG_DISCONNECT_ICON_FLAG);
    CG_DrawPic(CG_DISCONNECT_ICON_X, CG_DISCONNECT_ICON_Y, CG_DISCONNECT_ICON_W, CG_DISCONNECT_ICON_H, icon);
}
