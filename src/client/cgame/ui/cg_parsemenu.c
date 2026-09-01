// Source: uo_cgame_mp_x86.dll 0x3002d110..0x3002d1f4
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002d110_3002d1f4.mcode
//
// CG_ParseMenu — open one menu source file through the engine PC parser and
// dispatch its top-level "menudef" / "assetGlobalDef" blocks.
//
// Name adjudication: the .mcode "# name CG_HudElemShaderHeight" is REJECTED. It was
// assigned purely by byte-size match (win 0xe4 == corpus 0xe4), which the naming
// rules forbid, and there is no HUD-element/shader work here. The behavior — open a
// named source via the PC-parser load trap (0x61) with a "ui_mp/testhud.menu"
// fallback, read tokens (0x63), match the "assetGlobalDef"/"menudef" keywords and
// call the asset parser / Menu_New, then free the source (0x62) — is exactly
// the cgame menu-source parser. The Mac CG_ParseMenu body has the same parser-trap
// sequence and the same CG_Asset_Parse / Menu_New calls; its sole caller
// CG_Load_Menu hands it a menu filename token. This resolves the source name.
//
// Custom ABI (i386, cdecl body, RET with no imm — caller stack cleanup):
//   * filename : incoming stack argument 0 (read at [ESP+0x418] after SUB ESP,0x414).
//   * loadMode  : incoming register argument in ESI. ESI is neither saved in the
//     prologue nor written in the body; it is PUSHed as the menu-init loadMode for
//     Menu_New (0x3005ad40) and CG_Asset_Parse (0x3002cb40). The sole caller
//     (CG_Load_Menu) leaves its own `loadMode` in ESI across the call.
//   * sourceHandle: returned by CG_PC_LOAD_SOURCE and kept in EBX. Menu_New's
//     parser callee consumes this same live EBX value, independently of the
//     loadMode value pushed at 0x3002d1b6. Native C therefore passes both values
//     explicitly at that boundary.
// EBX (the PC source handle) is the only callee-saved register the function saves
// (PUSH EBX / POP EBX). The function sets no meaningful return value — its sole
// caller ignores the result — so it is modeled as void (EAX at RET is whatever the
// last trap left; there is no XOR EAX,EAX / MOV EAX,1 to establish a return code).
//
// /GS stack-cookie: the prologue stashes __security_cookie (0x30081650) at
// [ESP+0x410] and the epilogue verifies it via __security_check_cookie (0x30061639).
// That is an MSVC codegen artifact, not source-level behavior; omitted from the body.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

#include <stdint.h>

// The top-level keyword strings compared against each parsed token (.rdata).
// 0x30077c7c = "assetGlobalDef", 0x30077c74 = "menudef" (per globals.mcode). The
// fallback source name 0x30077c8c = "ui_mp/testhud.menu".
static const char kAssetGlobalDefKeyword[] = "assetGlobalDef"; // 0x30077c7c
static const char kMenuDefKeyword[]        = "menudef";        // 0x30077c74
static const char kFallbackMenuFile[]      = "ui_mp/testhud.menu"; // 0x30077c8c

// Q_stricmpn(99999, ...) degenerates to an unbounded case-insensitive compare
// (the caller idiom for Q_stricmp). EAX = 0x1869f at 0x3002d177 / 0x3002d19f.
enum { STRICMP_NO_LIMIT = 99999 };

// Byte compared against token.string[0] at 0x3002d170 to detect the closing brace.
enum { CLOSE_BRACE = '}' };

void CG_ParseMenu(int32_t loadMode, const char *filename)
{
    pc_token_t token;
    int sourceHandle;

    // 0x3002d122..0x3002d135: open the named source; on a null handle retry with
    // the built-in "ui_mp/testhud.menu" fallback.
    sourceHandle = trap_PC_LoadSource(filename);
    if (sourceHandle == 0) {
        // 0x3002d13c..0x3002d150
        sourceHandle = trap_PC_LoadSource(kFallbackMenuFile);
        if (sourceHandle == 0) {
            // 0x3002d150 JZ 0x3002d1e0: nothing opened; return without freeing.
            return;
        }
    }

    // 0x3002d156..0x3002d169: read the first token; if the source is empty, free
    // and return.
    if (!trap_PC_ReadToken(sourceHandle, &token)) {
        // 0x3002d169 JZ 0x3002d1d4
        trap_PC_FreeSource(sourceHandle); // 0x3002d1d4
        return;
    }

    // 0x3002d170: dispatch loop over top-level keyword blocks.
    for (;;) {
        // 0x3002d170: a '}' at the start of the current token closes the file.
        if ((unsigned char)token.string[0] == CLOSE_BRACE) {
            // 0x3002d175 JZ 0x3002d1d4
            trap_PC_FreeSource(sourceHandle); // 0x3002d1d4
            return;
        }

        // 0x3002d177..0x3002d18c: "assetGlobalDef" block.
        if (Q_stricmpn(token.string, kAssetGlobalDefKeyword, STRICMP_NO_LIMIT) == 0) {
            // 0x3002d18e..0x3002d19b: parse the asset block (source handle in ECX,
            // loadMode on the stack); a zero return aborts the whole file.
            if (CG_Asset_Parse(sourceHandle, loadMode) == 0) {
                // 0x3002d19b JZ 0x3002d1d4
                trap_PC_FreeSource(sourceHandle); // 0x3002d1d4
                return;
            }
            // 0x3002d19d JMP 0x3002d1bf: fall through to read the next token.
        } else if (Q_stricmpn(token.string, kMenuDefKeyword, STRICMP_NO_LIMIT) == 0) {
            // 0x3002d19f..0x3002d1bc: "menudef" block -> allocate/parse a menuDef_t.
            Menu_New(sourceHandle, loadMode);
        }
        // else (0x3002d1b4 JNZ 0x3002d1bf): unknown keyword is skipped; the block
        // parsers are responsible for consuming their own braces.

        // 0x3002d1bf..0x3002d1d2: read the next token; loop while one is produced,
        // otherwise fall out to the free path.
        if (!trap_PC_ReadToken(sourceHandle, &token)) {
            break; // 0x3002d1d2 falls through to 0x3002d1d4
        }
        // 0x3002d1d2 JNZ 0x3002d170: continue the dispatch loop.
    }

    // 0x3002d1d4: release the PC parser source.
    trap_PC_FreeSource(sourceHandle);
}
