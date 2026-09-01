// Source: uo_cgame_mp_x86.dll 0x3001a5b0..0x3001a604
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001a5b0_3001a604.mcode
//
// Behavior: latch the current cgame overlay-effect state into a persistent
// module-static block, gated by a "cgame ready to draw" flag and a nonzero
// primary field of the cgame state object. This is the "begin/seed" half of a
// two-part animated screen overlay; the draw half is FUN_300303a0..30030c5a,
// which is in fact this function's ONLY caller (call at 0x300303db) and then
// immediately consumes the block it wrote.
//
// Name adjudication: the .mcode header names this PM_JumpForSurface. That is a
// pure SIZE match (win 0x54 == the cgame_mp.dll PPC PM_JumpForSurface size 0x54)
// and is REJECTED per the no-size-matching rule. PM_JumpForSurface is Pmove
// code; this function touches no pmove state at all. It reads cgame globals
// (cg_time at 0x304831b0, the cgame-ready flag at 0x304831c0, the 137-ref cgame
// state pointer at 0x30459160) and writes an overlay-effect state block at
// 0x3048ae08.. consumed by an animated drawer that fades via CG_FadeColor
// (FUN_3001d200). No stronger source name is proven from current evidence, so
// the symbol is named provisionally by its proven role with an uncertainty note.
//
// Proven roles of the latched block (from the consumer FUN_300303a0 and the
// CG_FadeColor callee FUN_3001d200 which does `cg_time - startTime` vs
// `duration`):
//   0x3048ae0c <- cg_time           == CG_FadeColor start time (startMsec)
//   0x3048ae10 <- overlayFadeDuration (single-ref .data constant 0x3052fc4c)
//                                    == CG_FadeColor duration (totalMsec)
//   0x3048ae08 <- cg.state+0x5d0 (also the ready check); used by the consumer as
//                                  a small table index (table[idx*4] @0x3044b6f8)
//   0x3048ae14 <- cg.state+0x5d4 (consumer FILDs it -> a latched integer)
//   0x3048ae18 <- cg.state+0x5d8 (consumer reads it as a pointer/handle)
//
// The pointer at 0x30459160 is cg_snap (snapshot_t*), proven by
// CG_InstallSnapshotResetEffects (0x3003c9d0). Snapshot +0x5d0..+0x5d8 are
// playerState +0x5c4..+0x5cc: serverCursorHint, serverCursorHintVal,
// serverCursorHintString.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

// The persistent overlay-effect state block (0x3048ae08..0x3048ae18) and the
// single-ref fade-duration constant (0x3052fc4c) are declared in globals.h.
// cg_overlayFadeStartTime / cg_overlayFadeDuration are the CG_FadeColor inputs
// (proven); the other three keep their mechanical names until the drawer's
// subsystem is fully reconstructed.

void CG_LatchOverlaySource(void)
{
    snapshot_t *snap;
    uint32_t startTime;
    uint32_t durationBits;

    // 0x3001a5b0: MOV EAX,[0x304831c0] / TEST / JNZ ret.
    // Non-zero "cgame ready to draw" gate: bail out while not ready.
    if (cg_thirdPerson != 0) {
        return;
    }

    // 0x3001a5b9: MOV EAX,[0x30459160]; MOV ECX,[EAX+0x5d0]; TEST ECX,ECX; JZ ret.
    snap = cg_snap;
    if (snap->ps.serverCursorHint == CURSOR_HINT_OFF) {
        return;
    }

    // 0x3001a5c8..0x3001a5da: both inputs are loaded before either output.
    startTime = cg_time;
    durationBits = (uint32_t)cg_hintFadeTime_vmCvar.integer;
    cg_overlayFadeStartTime = startTime;
    cg_overlayFadeDuration = durationBits;

    // 0x3001a5e0..0x3001a5fe: latch the three source fields (re-reads +0x5d0).
    // The three latched source fields are named by their proven consumer roles in
    // the drawer CG_DrawCursorhint (0x300303a0): serverCursorHint is the hint-kind
    // selector, serverCursorHintVal a 0..255 color byte, and
    // serverCursorHintString a signed
    // command/config-string index.
    cg_usableHintKind = snap->ps.serverCursorHint;
    cg_usableHintColorByte = snap->ps.serverCursorHintVal;
    cg_usableHintCommandIndex = snap->ps.serverCursorHintString;
    // 0x3001a603: RET.
}
