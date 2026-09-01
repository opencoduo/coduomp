#include "../client_recovered.h"

// Source: uo_cgame_mp_x86.dll 0x30018020..0x3001808b
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30018020_3001808b.mcode
//
// CG_DrawSnapshot — draw the one-line snapshot-timing debug HUD entry and return
// the Y coordinate of the next line.
//
// Name evidence. The verbatim format string "time:%i snap:%i cmd:%i" (0x30076d70)
// and the exact draw idiom identify this as stock CoD/Q3 CG_DrawSnapshot; the
// same-module PPC name bank lists CG_DrawSnapshot in cgame_mp.dll and, adjacent
// in the same overlay code, CG_DrawBigString / Q_DrawStrlen — the two callees
// here. The mechanical ".mcode" name "Scr_GetObjectField" was a size-match and is
// rejected.
//
// Machine-code proof (esp tracked relative to entry; entry [ESP+4] = first arg):
//   30018020 PUSH ECX                 reserve one 4-byte stack local (later holds
//                                     the Q_DrawStrlen result at [ESP+0xc]).
//   30018021 MOV EAX,[0x30447ab0]     cgs.serverCommandSequence  ("cmd:%i")
//   30018026 MOV ECX,[0x30459158]     cg.latestSnapshotNum       ("snap:%i")
//   3001802c MOV EDX,[0x30459160]     cg_snap (current snapshot ptr)
//   30018032 PUSH EAX                 va vararg 3 = cgs.serverCommandSequence
//   30018033 MOV EAX,[EDX+0x8]        cg_snap->serverTime        ("time:%i")
//   30018036 PUSH ECX                 va vararg 2 = cg.latestSnapshotNum
//   30018037 PUSH EAX                 va vararg 1 = cg_snap->serverTime
//   30018038 PUSH 0x30076d70          format "time:%i snap:%i cmd:%i"
//   3001803d CALL 0x3004e8a0          s = va(fmt, serverTime, latestSnapshotNum,
//                                              serverCommandSequence); EAX = s.
//   Argument order: va args are consumed in va_list order (first %i = the arg at
//   the lowest pushed address = cg_snap->serverTime), so the three %i map to
//   time / snap / cmd exactly as printed.
//
//   30018042 FLD  float [ESP+0x18]    st0 = y   (entry [ESP+4], the float param)
//   30018046 FADD float [0x3007bce4]  st0 = y + 2.0f      (2.0f)
//   3001804c ADD  ESP,0x10            pop the four va() arguments
//   3001804f PUSH 0x3f800000          scale = 1.0f        -> CG_DrawBigString arg4
//   30018054 PUSH EAX                 s                   -> CG_DrawBigString arg3
//   30018055 PUSH ECX                 reserve slot        -> CG_DrawBigString arg2
//   30018056 MOV  EDX,EAX             EDX = s  (Q_DrawStrlen takes str in EDX)
//   30018058 FSTP float [ESP]         store (y + 2.0f) into the arg2 slot (the Y
//                                     coordinate for CG_DrawBigString)
//   3001805b CALL 0x3004e790          w = Q_DrawStrlen(s)  (reads EDX only; the
//                                     three pushed slots are left for the next
//                                     call and NOT consumed here); EAX = w.
//   30018060 MOV  [ESP+0xc],EAX       store w into the reserved local
//   30018064 FILD dword [ESP+0xc]     st0 = (float)w   (signed int -> float)
//   30018068 PUSH ECX                 reserve slot        -> CG_DrawBigString arg1
//   30018069 FMUL float [0x3007bf00]  st0 = w * 16.0f     (16.0f = big glyph width)
//   3001806f FSUBR float [0x3007c040] st0 = 620.0f - w*16.0f  (620.0f right edge)
//   30018075 FSTP float [ESP]         store the X coordinate into arg1 slot
//   30018078 CALL 0x3001cf10          CG_DrawBigString(620.0f - w*16.0f, y + 2.0f,
//                                                       s, 1.0f)
//   3001807d FLD  float [ESP+0x18]    st0 = y   (same entry [ESP+4] param again)
//   30018081 FADD float [0x3007be04]  st0 = y + 20.0f     (20.0f line advance)
//   30018087 ADD  ESP,0x14            pop the CG_DrawBigString args + local
//   3001808a RET                      returns (y + 20.0f) in st0 (float return)
//
// Float constants (read from .rdata): 0x3007bce4 = 2.0f, 0x3007bf00 = 16.0f,
// 0x3007c040 = 620.0f, 0x3007be04 = 20.0f. The value pushed as the scale,
// 0x3f800000, is 1.0f.
//
// The single float parameter `y` is the current vertical HUD cursor; the routine
// draws the snapshot line right-aligned (x = 620 - 16*glyphs) two pixels below it
// and returns the Y of the next line (y + 20).

// 16px big-glyph metrics and screen geometry used to lay out the line. These are
// one-off layout constants for this overlay line; kept file-local.
enum { CG_BIGCHAR_WIDTH = 16 };   /* px advance per big glyph (16.0f) */
enum { CG_SNAPSHOT_LINE_HEIGHT = 20 }; /* px to next debug line (20.0f) */
enum { CG_SNAPSHOT_RIGHT_EDGE = 620 }; /* right screen edge for this line (620.0f) */
enum { CG_SNAPSHOT_LINE_Y_OFFSET = 2 };/* px below `y` the text is drawn (2.0f) */

float CG_DrawSnapshot(float y)
{
    const char *s;
    int w;

    int32_t commandSequence = cgs_serverCommandSequence;
    int32_t snapshotNumber = cg_latestSnapshotNum;
    snapshot_t *snapshot = cg_snap;
    int32_t serverTime = snapshot->serverTime;
    s = va("time:%i snap:%i cmd:%i",
           serverTime, snapshotNumber, commandSequence);

    /* The DLL computes and stores the draw Y before calling Q_DrawStrlen. */
    float drawY = (float)(
        (long double)y + (long double)CG_SNAPSHOT_LINE_Y_OFFSET);
    w = Q_DrawStrlen(s);

    /* 0x30018064 FILD w feeds 0x30018069 FMUL 16.0f directly with no float store,
     * so w enters the product exact; a (float)w cast would round it first (Class 4). */
    float drawX = (float)(
        (long double)CG_SNAPSHOT_RIGHT_EDGE -
        (long double)w * (long double)CG_BIGCHAR_WIDTH);
    CG_DrawBigString(drawX,
                     drawY,
                     s,
                     1.0f);

    return (long double)y + (long double)CG_SNAPSHOT_LINE_HEIGHT;
}
