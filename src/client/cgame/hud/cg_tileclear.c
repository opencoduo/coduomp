#include "../client_recovered.h"
#include "../globals.h"

// Source: uo_cgame_mp_x86.dll 0x3001d160..0x3001d200
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001d160_3001d200.mcode
//
// CG_TileClear — fill the letterbox border around a cropped 3D view with a tiling
// shader. When the 3D view does not cover the whole backbuffer, the four regions
// outside it (top, bottom, left, right) must be repainted each frame so stale
// pixels don't show through; this routine tiles cgs.media.backTileShader across
// each of those four edges via CG_TileClearBox (0x3001d0d0). When the view IS the
// full screen it returns without drawing anything.
//
// NAME: matches Quake3 CG_TileClear exactly — the "skip if the render rect equals
// (0,0,vidWidth,vidHeight)" guard followed by four CG_TileClearBox edge draws is
// the canonical CG_TileClear shape. The same-module PPC bank
// (cgame_mp/inputs/ppc_function_names.tsv) lists cgame_mp!CG_TileClear as the parent
// of cgame_mp!CG_TileClearBox, and its helper here (0x3001d0d0) is CG_TileClearBox.
// The .mcode size-matched guess "InitWeaponInfo" is REJECTED: InitWeaponInfo parses
// and populates weapon-config structures; this function reads a view rectangle and
// vidWidth/vidHeight, tests for full-screen coverage, and issues four 2D tile-fill
// draws — no weapon data, no parsing. Size matching (win 0xa0 == 0xa0) is forbidden
// by the contract and is not evidence.
//
// GLOBALS consumed (resolved during this reconstruction, renamed at their
// definitions in globals.h/globals.c):
//   cg_refdef.x      = [0x30487a78]  render rect x      (EAX)
//   cg_refdef.y      = [0x30487a7c]  render rect y      (ESI)
//   cg_refdef.width  = [0x30487a80]  render rect width  (ECX)
//   cg_refdef.height = [0x30487a84]  render rect height (EDI)
//   cgs_glconfig.vidWidth  = [0x30447a88]  backbuffer width  (EBX)
//   cgs_glconfig.vidHeight = [0x30447a8c]  backbuffer height (EBP)
//   cgs_media_backTileShader = [0x3044b6f0] tiling shader handle (reloaded into
//                                           EAX before each CG_TileClearBox call).
//
// FULL-SCREEN GUARD (0x3001d16e..0x3001d198): the code draws unless ALL of
//   rx == 0 && ry == 0 && rw == vidWidth && rh == vidHeight
// hold. Instruction proof:
//   TEST EAX,EAX ; JNZ draw      -> rx != 0        => draw
//   TEST ESI,ESI ; JNZ draw      -> ry != 0        => draw
//   CMP  ECX,EBX ; JNZ draw      -> rw != vidWidth => draw
//   CMP  EDI,EBP ; JZ  skip      -> rh == vidHeight => (all four equal) skip
//
// The four CG_TileClearBox argument tuples below were derived by a byte-exact
// stack simulation of the interleaved PUSH / MOV [ESP+n] stream (each callee takes
// hShader in EAX plus arg0..arg3 as cdecl stack slots [ESP+4..+0x10], and does not
// clean the caller's args — the caller balances with ADD ESP,0x40 at 0x3001d1f5).
// Note the integer edge arithmetic is computed in registers and forwarded as-is:
//   edgeXR = rx + rw - 1   (LEA [ECX+EAX-1] at 0x3001d19f, spilled and reloaded)
//   edgeYB = ry + rh - 1   (LEA [EDI+ESI-1] at 0x3001d1b1)
// CG_TileClearBox maps these four edges to ordinary x/y/w/h and derives tiled
// texture coordinates from the integer edges before forwarding the shader.
//
// i386 calling convention: standard cdecl, no arguments (RET with no immediate at
// 0x3001d1ff; the trailing ADD ESP,8 unwinds the two-dword local scratch reserved
// by SUB ESP,8 at entry). EBX/EBP/ESI/EDI save/restore is ABI, not source behavior.

void CG_TileClear(void)
{
    /* 0x3001d160..0x3001d186 load x, width, vidWidth, vidHeight, y, height in
     * exactly this order before testing the captured x. */
    int32_t rx = coduo_int32_from_bits((uint32_t)cg_refdef.x);
    int32_t rw = coduo_int32_from_bits((uint32_t)cg_refdef.width);
    int32_t vidWidth  = cgs_glconfig.vidWidth;
    int32_t vidHeight = cgs_glconfig.vidHeight;
    int32_t ry = coduo_int32_from_bits((uint32_t)cg_refdef.y);
    int32_t rh = coduo_int32_from_bits((uint32_t)cg_refdef.height);

    // Full-screen 3D view: nothing outside it to clear.
    if (rx == 0 && ry == 0 && rw == vidWidth && rh == vidHeight) {
        return;
    }

    // Precomputed inclusive right/bottom edges (rx+rw-1, ry+rh-1), matching the two
    // LEA instructions; computed with 32-bit wrapping semantics.
    int32_t edgeXR = coduo_int32_from_bits((uint32_t)rx + (uint32_t)rw - 1u);
    int32_t edgeYB = coduo_int32_from_bits((uint32_t)ry + (uint32_t)rh - 1u);

    int32_t hShader = (int32_t)cgs_media_backTileShader;

    // CALL1 (0x3001d1b5): top strip. args (arg0..arg3) = (0, 0, vidWidth, ry).
    CG_TileClearBox(hShader, 0, 0, vidWidth, ry);

    // CALL2 (0x3001d1c6): bottom strip. args = (0, edgeYB, vidWidth, vidHeight-edgeYB).
    hShader = (int32_t)cgs_media_backTileShader;
    CG_TileClearBox(hShader, 0, edgeYB, vidWidth,
                    coduo_int32_from_bits((uint32_t)vidHeight -
                                     (uint32_t)edgeYB));

    // CALL3 (0x3001d1dc): left strip. args = (0, ry, rx, rh).
    // (arg2 = rx via ECX reloaded from the spilled rx at [ESP+0x30]; EDI recomputed
    //  as (edgeYB - ry) + 1 = rh.)
    {
        int32_t leftX = rx;
        hShader = (int32_t)cgs_media_backTileShader;
        int32_t leftHeight = coduo_int32_from_bits(
            (uint32_t)edgeYB - (uint32_t)ry + 1u);
        CG_TileClearBox(hShader, 0, ry, leftX, leftHeight);

        // CALL4 (0x3001d1f0): right strip. args = (edgeXR, ry, vidWidth-edgeXR, rh).
        // (arg0/EAX = edgeXR reloaded from the spilled rx+rw-1 at [ESP+0x44];
        //  arg2 = vidWidth - edgeXR via SUB EBX,EAX.)
        int32_t rightWidth = coduo_int32_from_bits(
            (uint32_t)vidWidth - (uint32_t)edgeXR);
        hShader = (int32_t)cgs_media_backTileShader;
        CG_TileClearBox(hShader, edgeXR, ry, rightWidth, leftHeight);
    }
}
