// Source: uo_cgame_mp_x86.dll 0x3002d200..0x3002d2c3
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002d200_3002d2c3.mcode
//
// CG_Load_Menu — parse a brace-delimited list of menu filenames from the shared
// Com parse cursor and load each named menu file.
//
// The menu file format for a `loadmenu` directive is:
//     loadmenu { "ui_mp/main.menu" "ui_mp/hud.menu" ... }
// The caller (CG_LoadMenus, 0x3002d3c0) matches the "loadmenu" keyword and then
// invokes this to consume the `{ ... }` filename list. This function expects the
// next token to be `{`, then loops reading filename tokens and calling
// CG_ParseMenu on each until it reaches the closing `}`.
//
// Returns qtrue when the block closes cleanly on `}`; qfalse on a missing opening
// `{`, end-of-text (Com_Parse returned NULL), or an empty token.
//
// Name derivation: same-module PPC bank (cgame_mp.dll CG_Load_Menu) + callgraph.
// The .mcode's size-matched "ItemParse_origin" guess is REJECTED — ItemParse_origin
// is a single-keyword vector-parse handler, whereas this is a brace-block
// filename-list loop that dispatches CG_ParseMenu (which itself matches the
// "menudef"/"assetGlobalDef" keywords and opens the referenced file).

#include "client/cgame/client_recovered.h"

// The brace/close tokens compared against. The .rdata datum at 0x30072764 is the
// single-character C string "}" (bytes 7d 00 00 00); the exporter attributed the
// dword to the adjacent "BG_ParseConditions: no conditions found" string, but the
// machine code (MOV ECX,0x30072764 ; Q_stricmpn) uses it as a NUL-terminated "}".
enum { OPEN_BRACE = '{' };

// Universal Q_stricmp limit: Q_stricmpn(99999, ...) degenerates to a full compare.
enum { STRICMP_NO_LIMIT = 99999 };

// EAX at entry (modeled as `loadMode`) is moved to a preserved register (ESI) and
// forwarded unchanged to CG_ParseMenu. The sole caller supplies the scalar menu
// loadMode value 5, so this is an int32_t value rather than a host pointer.
qboolean CG_Load_Menu(int32_t loadMode, char **parse)
{
    char *token;

    // 0x3002d208-0x3002d236: inlined Com parser unget/mark restore. When a token
    // was pushed back (ungetToken != 0), restore the saved cursor into *parse and
    // the saved line, then clear ungetToken. This mirrors Com_Parse's own unget
    // handling and re-runs before every Com_Parse call below.
    if (com_parseSession->ungetToken != 0) {
        *parse = com_parseSession->savedParse;
        com_parseSession->ungetToken = 0;
        com_parseSession->line = com_parseSession->savedLine;
    }

    // 0x3002d236-0x3002d246: read the opening token; require '{'.
    token = Com_ParseExt(parse, qtrue);
    if ((unsigned char)token[0] != OPEN_BRACE) {
        // 0x3002d248: not an open brace -> fail.
        return qfalse;
    }

    // 0x3002d250: loop reading filename tokens until '}'.
    for (;;) {
        // 0x3002d250-0x3002d27e: unget/mark restore (as above).
        if (com_parseSession->ungetToken != 0) {
            *parse = com_parseSession->savedParse;
            com_parseSession->ungetToken = 0;
            com_parseSession->line = com_parseSession->savedLine;
        }

        // 0x3002d27e-0x3002d28d: read the next token.
        token = Com_ParseExt(parse, qtrue);
        if (token == 0) {
            // 0x3002d28d: end of text without a closing brace -> fail.
            return qfalse;
        }

        // 0x3002d28f-0x3002d2a2: token equals "}" -> block closed cleanly.
        if (Q_stricmpn(token, "}", STRICMP_NO_LIMIT) == 0) {
            // 0x3002d2b4: success.
            return qtrue;
        }

        // 0x3002d2a4-0x3002d2a7: an empty token also terminates with failure.
        if (token[0] == '\0') {
            return qfalse;
        }

        // 0x3002d2a9-0x3002d2b2: load the menu file named by this token, then loop.
        // ESI (= this function's incoming `loadMode`) is still live and is the
        // register argument forwarded to CG_ParseMenu (0x3002d2a9 PUSH EDI
        // supplies the filename; ESI supplies the loadMode register).
        CG_ParseMenu(loadMode, token);
    }
}
