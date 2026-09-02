// Source: uo_cgame_mp_x86.dll 0x3002d2d0..0x3002d46d
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002d2d0_3002d46d.mcode
//
// CG_LoadMenus — load and register every menu named by a menu-list file. Strict
// translation of the Windows i386 machine code.
//
// Behavior proven from the machine code and the referenced .rdata strings:
//   1. Snapshot the engine milliseconds (CG_MILLISECONDS) as the load start time.
//   2. Open `menuFile` (arriving in EDI) via CG_FS_FOPEN_FILE (mode FS_READ = 0),
//      writing the file handle into a local out slot; the trap returns the byte
//      length. If the returned handle is 0 (open failed), CG_ERROR-print
//      "^3menu file not found: %s, using default\n" and retry with the built-in
//      default "ui_mp/hud.txt" (0x300769a8). If that default also fails, CG_ERROR
//      "^1default menu file not found: ui/hud.txt, unable to continue!\n".
//   3. If the file length is >= MAX_MENULIST_FILE (4096), CG_ERROR
//      "^1menu file too large: %s is %i, max allowed is %i", close the file, and
//      return without loading anything.
//   4. Otherwise read the text into cg_menuListText[4096] via CG_FS_READ, close the
//      file (CG_FS_FCLOSE_FILE), NUL-terminate at [length], and Com_Compress() the
//      buffer in place (strips comments/whitespace runs).
//   5. Reset menuCount to 0, point a local parse cursor at the buffer, then walk the
//      top-level tokens with Com_Parse. Skip tokens until the "loadmenu" keyword;
//      for each "loadmenu" dispatch CG_Load_Menu to consume its `{ "file" ... }`
//      list. Stop at end-of-text, an empty token, or a top-level "}".
//   6. Print "UI menu load time = %d milli seconds\n" with (endMs - startMs).
//
// Naming: the .mcode carries the size-guessed broad-corpus name BG_SetupWeaponAlts
// (win size 0x19d ~ matched 0x19c). REJECTED: there is no weapon-record walk here —
// this is the ui_shared menu-list loader. The real name CG_LoadMenus comes from the
// same-module PPC bank (cgame_mp.dll) and the callgraph: its only caller
// (FUN_3002da90) selects the menu-list filename and calls this, and it dispatches
// CG_Load_Menu on each "loadmenu" block. Note that the mechanical export also
// mislabeled menuCount (0x30134d40) and the text buffer (0x300da888) with the same
// wrong `bg_setupweaponalts` owner; both are corrected in globals.{h,c}.
//
// Register ABI: `menuFile` is the incoming EDI register argument (the caller loads
// EDI before the CALL). `loadMode` is the single incoming cdecl STACK argument (the
// caller pushes the literal 5); it is read only to be forwarded, in EAX, to
// CG_Load_Menu. The prologue reserves 8 bytes of locals (the file-handle out slot
// and the parse cursor) and saves EBX/EBP/ESI; the trailing RET has no immediate
// (cdecl caller cleanup of the one stack arg). EBX is the constant 0 throughout.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

// The default menu-list file used when the requested one cannot be opened.
static const char *const CG_DEFAULT_MENU_FILE = "ui_mp/hud.txt"; // 0x300769a8

// The top-level keyword that introduces a "{ ... }" menu-file list, and the tokens
// that terminate the top-level scan. "}" is the .rdata "}" at 0x30072764.
static const char *const CG_LOADMENU_KEYWORD = "loadmenu"; // 0x30077bc8
static const char *const CG_CLOSE_BRACE_TOKEN = "}";       // 0x30072764

// Universal Q_stricmp limit: Q_stricmpn(99999, ...) degenerates to a full compare.
enum { STRICMP_NO_LIMIT = 99999 };

void CG_LoadMenus(int32_t loadMode, const char *menuFile)
{
    int32_t startMs;        // EBP: milliseconds at load start
    int32_t fileHandle;     // local out slot at [ESP+0xC]
    int32_t length;         // ESI: byte length returned by CG_FS_FOPEN_FILE
    char *text;         // local parse cursor at [ESP+0x10]

    // 0x3002d2d6: startMs = cgame_syscall(CG_MILLISECONDS).
    startMs = coduo_int32_from_bits((uint32_t)cgame_syscall(CG_MILLISECONDS));

    // 0x3002d2e2..0x3002d2fc: open the requested file (mode FS_READ). The trap
    // writes the handle into &fileHandle and returns the byte length in ESI.
    fileHandle = 0;
    length = coduo_int32_from_bits((uint32_t)cgame_syscall(
        CG_FS_FOPEN_FILE, (intptr_t)menuFile, (intptr_t)&fileHandle, FS_READ));

    // 0x3002d2fa CMP EAX,EBX(0) ; JNZ 0x3002d349: EAX is the handle just written.
    // A zero handle means the open failed.
    if (fileHandle == 0) {
        // 0x3002d2fe..0x3002d312: report and switch to the default file.
        cgame_syscall(CG_ERROR,
                      (intptr_t)va("^3menu file not found: %s, using default\n",
                                            menuFile));

        // 0x3002d312..0x3002d330: re-open the default "ui_mp/hud.txt".
        fileHandle = 0;
        length = coduo_int32_from_bits((uint32_t)cgame_syscall(
            CG_FS_FOPEN_FILE, (intptr_t)CG_DEFAULT_MENU_FILE,
            (intptr_t)&fileHandle, FS_READ));

        // 0x3002d32e CMP EAX,EBX(0) ; JNZ 0x3002d349.
        if (fileHandle == 0) {
            // 0x3002d332..0x3002d346: the default is missing too.
            /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
            cgame_syscall(CG_ERROR,
                          (intptr_t)va("^1default menu file not found: "
                                                "ui/hud.txt, unable to continue!\n",
                                                menuFile));
        }
    }

    // 0x3002d349 CMP ESI,0x1000 ; JL 0x3002d382: length compared signed against 4096.
    if (length >= MAX_MENULIST_FILE) {
        // 0x3002d351..0x3002d372: file too large -> report, close, and give up.
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        cgame_syscall(CG_ERROR,
                      (intptr_t)va("^1menu file too large: %s is %i, "
                                            "max allowed is %i",
                                            menuFile, length, MAX_MENULIST_FILE));
        cgame_syscall(CG_FS_FCLOSE_FILE, fileHandle);
        // 0x3002d37b..0x3002d381: epilogue, return void.
        return;
    }

    // 0x3002d382..0x3002d3a8: read the whole file into the shared 4096-byte buffer,
    // close it, and NUL-terminate at the byte length.
    cgame_syscall(CG_FS_READ,
                  (intptr_t)cg_menuListText,
                  length,
                  fileHandle);
    cgame_syscall(CG_FS_FCLOSE_FILE, fileHandle);
    cg_menuListText[length] = '\0'; // 0x3002d39c MOV byte[ESI + 0x300da888],BL(0)

    // 0x3002d3ab..0x3002d3b0: strip comments/whitespace runs in place.
    Com_Compress(cg_menuListText);

    // 0x3002d3b5: reset the registered-menu count before (re)loading.
    menuCount = 0;

    // 0x3002d3bb: start the top-level parse cursor at the buffer start.
    text = cg_menuListText;

    // 0x3002d3c3..0x3002d448: top-level token scan. Read tokens with Com_Parse and,
    // for each "loadmenu" keyword, dispatch CG_Load_Menu to consume its brace list.
    for (;;) {
        char *token;

        // 0x3002d3c3..0x3002d3ec: inlined Com parser unget/mark restore. When a
        // token was pushed back, restore the saved cursor into `text` and the saved
        // line, then clear ungetToken (mirrors Com_Parse's own unget handling).
        if (com_parseSession->ungetToken != 0) {
            text = com_parseSession->savedParse;
            com_parseSession->ungetToken = 0;
            com_parseSession->line = com_parseSession->savedLine;
        }

        // 0x3002d3ec..0x3002d3f8: read the next token (allowLineBreaks = qtrue).
        token = Com_ParseExt(&text, qtrue);

        // 0x3002d3fd JZ, 0x3002d403 JZ, 0x3002d407 JZ: stop on end-of-text, an empty
        // token, or a top-level '}' (byte test on token[0]).
        if (token == 0)
            break;
        if (token[0] == '\0')
            break;
        if ((unsigned char)token[0] == '}')
            break;

        // 0x3002d40b..0x3002d41e: a token equal to "}" also ends the scan. (Redundant
        // with the byte test above, but the machine code performs both.)
        if (Q_stricmpn(token, CG_CLOSE_BRACE_TOKEN, STRICMP_NO_LIMIT) == 0)
            break;

        // 0x3002d420..0x3002d433: skip any token that is not "loadmenu".
        if (Q_stricmpn(token, CG_LOADMENU_KEYWORD, STRICMP_NO_LIMIT) != 0)
            continue;

        // 0x3002d435..0x3002d448: consume the "loadmenu { ... }" list. `loadMode`
        // (the incoming stack arg) is forwarded in EAX; &text is the parse cursor.
        // CG_Load_Menu returns qtrue while more may follow; qfalse ends the scan.
        if (CG_Load_Menu(loadMode, &text) == 0)
            break;
    }

    // 0x3002d44e..0x3002d463: report the elapsed load time. One CG_MILLISECONDS read for
    // the end time; 0x3002d456 SUB EAX,EBP forms (endMs - startMs).
    Com_Printf("UI menu load time = %d milli seconds\n",
               coduo_int32_from_bits(
                   (uint32_t)cgame_syscall(CG_MILLISECONDS) -
                   (uint32_t)startMs));
}
