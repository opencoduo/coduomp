// Source: uo_cgame_mp_x86.dll 0x30032780..0x300327ec
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30032780_300327ec.mcode
//
// CG_KeyEvent — cgame VM key-event handler (vmMain command index 7, invoked as
// handler(key, down); key in EAX, down in ECX from the caller at 0x3002afeb).
// It processes only scoreboard scroll keys, and only while the scoreboard is
// showing and only on key-press; every other key/state returns qfalse (key not
// consumed).
//
// Machine-code facts proven:
//   30032780  TEST ECX,ECX / JZ  -> return qfalse when down == 0 (key-release).
//   30032784  MOV ECX,[cg_scoreboardShowing] / TEST / JZ -> return qfalse when
//             the scoreboard is not showing.
//   3003278e..300327b6  CMP EAX against the six scroll keycodes (all 32-bit,
//             exact-equality JZ/JNZ); any non-match returns qfalse.
//   300327b8  down keys -> CALL CG_ScrollScoreboardDown, then return qtrue.
//   300327c3  up keys   -> inline scroll-up:
//                 MOV EAX,[cg_scoreboardScrollPos]; TEST EAX,EAX; JLE done  (signed <= 0)
//                 SUB EAX,[cg_scoreboardScrollStep_vmCvar.integer]; store;
//                 JNS done  (signed: keep when result >= 0)
//                 else store 0; done -> return qtrue.
//   The rejected size-matched header guess "Com_ParseOnLine" (char *(void*)) does
//   not match: this takes two register ints, returns qboolean, and drives the
//   scoreboard scroll globals — nothing token-parser related.

#include "client/cgame/client_recovered.h"

qboolean CG_KeyEvent(int32_t key, qboolean down)
{
    /* 0x30032780: only key-press events are considered. */
    if (!down) {
        return qfalse;
    }

    /* 0x30032784: scroll keys are live only while the scoreboard is showing. */
    if (!cg_scoreboardShowing) {
        return qfalse;
    }

    switch (key) {
    /* 0x3003278e / 0x30032795 / 0x3003279c: scroll the scoreboard down. */
    case K_MWHEELDOWN:  /* 0xcd */
    case K_PGDN:        /* 0xa3 */
    case K_KP_PGDN:     /* 0xbe */
        CG_ScrollScoreboardDown();
        return qtrue;

    /* 0x300327a3 / 0x300327aa / 0x300327b1: scroll the scoreboard up inline. */
    case K_MWHEELUP:  /* 0xce */
    case K_PGUP:      /* 0xa4 */
    case K_KP_PGUP:   /* 0xb8 */
        /* 0x300327c3: JLE is signed; only scroll up when already past the top. */
        if (cg_scoreboardScrollPos > 0) {
            /* 0x300327cc is a target dword SUB. Windows/i386 MSVC preserves
             * modulo-2^32 arithmetic even when the signed result wraps. */
            cg_scoreboardScrollPos =
                coduo_int32_from_bits((uint32_t)cg_scoreboardScrollPos - (uint32_t)cg_scoreboardScrollStep_vmCvar.integer);
            /* 0x300327d7: JNS -> clamp a negative result up to the top line. */
            if (cg_scoreboardScrollPos < 0) {
                cg_scoreboardScrollPos = 0;
            }
        }
        return qtrue;

    /* 0x300327b6: any other key is not consumed. */
    default:
        return qfalse;
    }
}
