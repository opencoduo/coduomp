#include "console.h"

#include "cgame.h"
#include "filesystem/filesystem.h"
#include "../platform/crt_boundary.h"

#include <stdlib.h>
#include <string.h>

/* Original Win32 console field and its source defaults are at 0x04e199c0 and
 * 0x005c3d58..0x005c3d63. CL_InitRenderer later changes the pixel width to
 * the active video width minus 32 while retaining the 8x16 glyph metrics. */
console_input_field_t con_inputField;
/* Original Win32 command-history fields occupy 0x04e1a840..0x04e1cbbf. */
console_input_field_t con_historyFields[CON_HISTORY_FIELD_COUNT];
/* Original Win32 history-selection counters at 0x04e199a8 and 0x04e199b0.
 * con_historyLine is the currently displayed entry; con_nextHistoryLine is
 * the insertion sequence number for the next submitted console command. */
int32_t con_historyLine;
int32_t con_nextHistoryLine;
int32_t con_fieldWidthPixels = 620;
float con_fieldCharWidth = 8.0f;
float con_fieldCharHeight = 16.0f;

/* key_overstrikeMode selects insert versus overwrite. Modifier state lives
 * in the owning keyStates[] table. */
qboolean key_overstrikeMode;

/* Registered by Con_Init as con_restricted; original pointer 0x04e1cc04. */
cvar_t *con_restricted;
cvar_t *con_debug;
cvar_t *scr_conspeed;
/* Registered as "cl_noprint"; original Win32 pointer is 0x04e1995c. */
cvar_t *cl_noprint;

/* Original Win32 message field and chat routing globals are at 0x04e1a720,
 * 0x04e1a700, 0x04e19ae0, and 0x04e19ae4. */
console_input_field_t chatField;
qboolean chat_team;
qboolean chat_squad;
int32_t chat_playerNum;

/* Original Win32 console state begins at 0x04e1cc20. */
console_state_t con;
/* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): distinguish an intentional
 * user backscroll from a temporary stock-created display/current mismatch.
 * This sidecar is absent from the stock source and leaves the recovered retail
 * console record unchanged there. */
qboolean coduomp_console_manually_scrolled;

enum {
    CON_CHAT_FIELD_WIDTH = 588,
    CON_TEAM_CHAT_FIELD_WIDTH = 543,
    CON_MESSAGE_COMMAND_SIZE = 1024,
    CON_WHEEL_SCROLL_PAGES = 3
};

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the identical field
 * reset and KEYCATCH_MESSAGE toggle in the four original message commands. */
static void Con_BeginMessageInput(int32_t widthInPixels)
{
    memset(&chatField, 0, sizeof(chatField));
    chatField.widthInChars = CON_INPUT_BUFFER_SIZE;
    chatField.widthInPixels = widthInPixels;
    chatField.charHeight = con_fieldCharHeight;
    cls.keyCatchers ^= KEYCATCH_MESSAGE;
}

/* Source: CoDUOMP.exe 0x00409620..0x004097f7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00409620_004097f8.mcode.
 * Name and source-level helper calls: exact same-module Mac symbol
 * Con_JumpToDemoEnd_f. The Windows optimizer inlines FS_Seek, FS_FTell, and
 * FS_filelength after the first calls to each helper. The optional argument
 * is the number of file bytes to leave unread at the end. If playback is
 * already within that trailing span, the original rewinds before scanning. */
void Con_JumpToDemoEnd_f(void)
{
    if (clc.demoPlayback == qfalse || clc.demoFile == 0) {
        return;
    }

    const int32_t trailingBytes = Cmd_Argc() >= 2 ? coduo_crt_atoi(Cmd_Argv(1)) : 0;

    int32_t currentPosition = FS_FTell(clc.demoFile);
    int32_t fileLength = FS_filelength(clc.demoFile);
    if ((int32_t)((uint32_t)fileLength - (uint32_t)trailingBytes) < currentPosition) {
        (void)FS_Seek(clc.demoFile, 0, FS_SEEK_ORIGIN_SET);
    }

    while (clc.demoFile != 0) {
        currentPosition = FS_FTell(clc.demoFile);
        fileLength = FS_filelength(clc.demoFile);
        const int32_t remainingBytes = (int32_t)((uint32_t)fileLength - (uint32_t)currentPosition);
        if (remainingBytes < trailingBytes)
            break;

        CL_ReadDemoMessage();
        cl.serverTime = cl.snap.serverTime;
        cls.realtime = (int32_t)((uint32_t)cl.snap.serverTime - (uint32_t)cl.serverTimeDelta);
    }
}

/* Source: CoDUOMP.exe 0x00409800..0x0040995f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00409800_00409960.mcode.
 * Name and signature: exact same-module Mac symbol Con_Init. */
void Con_Init(void)
{
    scr_conspeed = Cvar_Get("scr_conspeed", "3", 0);
    con_debug = Cvar_Get("con_debug", "0", CVAR_ARCHIVE);
    con_restricted = Cvar_Get("con_restricted", "0", CVAR_INIT);

    memset(&con_inputField, 0, sizeof(con_inputField));
    con_inputField.widthInChars = CON_INPUT_BUFFER_SIZE;
    con_inputField.widthInPixels = con_fieldWidthPixels;
    con_inputField.charWidth = con_fieldCharWidth;
    con_inputField.charHeight = con_fieldCharHeight;
    con_inputField.fixedSize = qtrue;

    for (int32_t history = 0; history < CON_HISTORY_FIELD_COUNT; ++history) {
        memset(&con_historyFields[history], 0, sizeof(con_historyFields[history]));
        con_historyFields[history].widthInChars = CON_INPUT_BUFFER_SIZE;
        con_historyFields[history].widthInPixels = con_fieldWidthPixels;
        con_historyFields[history].charWidth = con_fieldCharWidth;
        con_historyFields[history].charHeight = con_fieldCharHeight;
        con_historyFields[history].fixedSize = qtrue;
    }

    Cmd_AddCommand("toggleconsole", Con_ToggleConsole_f);
    Cmd_AddCommand("messagemode", Con_MessageMode_f);
    Cmd_AddCommand("messagemode2", Con_MessageMode2_f);
    Cmd_AddCommand("messagemode3", Con_MessageMode3_f);
    Cmd_AddCommand("messagesquad", Con_MessageSquad_f);
    Cmd_AddCommand("clear", Con_Clear_f);
    Cmd_AddCommand("condump", Con_Dump_f);
    Cmd_AddCommand("jumptodemoend", Con_JumpToDemoEnd_f);
}

/* Source: CoDUOMP.exe 0x00408ec0..0x00408f5c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00408ec0_00408f5d.mcode.
 * Name and signature: exact same-module Mac symbol Con_ToggleConsole_f. */
void Con_ToggleConsole_f(void)
{
    const qboolean openingConsole = (cls.keyCatchers & KEYCATCH_CONSOLE) == 0;

    if (cls.state == CA_DISCONNECTED && cls.keyCatchers == KEYCATCH_CONSOLE) {
        Cbuf_AddText("d1\n");
        cls.keyCatchers = 0;
        return;
    }

    if (con_restricted->integer != 0 && keyStates[K_SHIFT].down == qfalse && (cls.keyCatchers & KEYCATCH_CONSOLE) == 0) {
        return;
    }

    memset(&con_inputField, 0, sizeof(con_inputField));
    con_inputField.widthInChars = CON_INPUT_BUFFER_SIZE;
    con_inputField.widthInPixels = con_fieldWidthPixels;
    con_inputField.charWidth = con_fieldCharWidth;
    con_inputField.charHeight = con_fieldCharHeight;
    con_inputField.fixedSize = qtrue;
    if (openingConsole != qfalse && coduomp_console_manually_scrolled == qfalse) {
        con.displayLine = con.currentLine;
    }
    cls.keyCatchers ^= KEYCATCH_CONSOLE;
}

/* Source: CoDUOMP.exe 0x00408f60..0x00408fca.
 * Name and signature: exact same-module Mac symbol Con_MessageMode_f. */
void Con_MessageMode_f(void)
{
    chat_playerNum = -1;
    chat_team = qfalse;
    chat_squad = qfalse;
    Con_BeginMessageInput(CON_CHAT_FIELD_WIDTH);
}

/* Source: CoDUOMP.exe 0x00408fd0..0x0040903e.
 * Name and signature: exact same-module Mac symbol Con_MessageMode2_f. */
void Con_MessageMode2_f(void)
{
    chat_playerNum = -1;
    chat_team = qtrue;
    chat_squad = qfalse;
    Con_BeginMessageInput(CON_TEAM_CHAT_FIELD_WIDTH);
}

/* Source: CoDUOMP.exe 0x00409040..0x004090c8.
 * Name and signature: exact same-module Mac symbol Con_MessageMode3_f. */
void Con_MessageMode3_f(void)
{
    chat_playerNum = (int32_t)VM_Call(coduo_cgameVm, CGVM_CROSSHAIR_PLAYER, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    if (chat_playerNum < 0 || chat_playerNum >= MAX_CLIENTS) {
        chat_playerNum = -1;
        return;
    }

    chat_team = qfalse;
    chat_squad = qfalse;
    Con_BeginMessageInput(CON_CHAT_FIELD_WIDTH);
}

/* Source: CoDUOMP.exe 0x004090d0..0x0040913e.
 * Name and signature: exact same-module Mac symbol Con_MessageSquad_f. */
void Con_MessageSquad_f(void)
{
    chat_playerNum = -1;
    chat_team = qfalse;
    chat_squad = qtrue;
    Con_BeginMessageInput(CON_TEAM_CHAT_FIELD_WIDTH);
}

/* Source: CoDUOMP.exe 0x00409140..0x0040915d.
 * Name and signature: exact same-module Mac symbol Con_Clear_f. */
void Con_Clear_f(void)
{
    for (int32_t cell = 0; cell < CON_TEXT_CELL_COUNT; ++cell)
        con.text[cell] = CON_EMPTY_TEXT_CELL;
    con.displayLine = con.currentLine;
    coduomp_console_manually_scrolled = qfalse;
}

/* Source: CoDUOMP.exe 0x00409160..0x004092f5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00409160_004092f6.mcode.
 * Name and signature: exact same-module Mac symbol Con_Dump_f. */
void Con_Dump_f(void)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    char lineBuffer[CON_DUMP_BUFFER_SIZE];
    int32_t line;
    int32_t fileHandle;

    if (Cmd_Argc() != 2) {
        Com_Printf("usage: condump <filename>\n");
        return;
    }

    const char *filename = Cmd_Argv(1);
    Com_Printf("Dumped console text to %s.\n", filename);
    fileHandle = FS_FOpenFileWrite(filename);
    if (fileHandle == 0) {
        Com_Printf("ERROR: couldn't open.\n");
        return;
    }

    line = con.currentLine - con.totalLines + 1;
    while (line <= con.currentLine) {
        const uint16_t *cells = &con.text[(line % con.totalLines) * con.lineWidth];
        int32_t column;

        for (column = 0; column < con.lineWidth; ++column) {
            if ((char)(cells[column] & UINT16_C(0x00ff)) != ' ')
                break;
        }
        if (column != con.lineWidth)
            break;
        ++line;
    }

    lineBuffer[con.lineWidth] = '\0';
    for (; line <= con.currentLine; ++line) {
        const uint16_t *cells = &con.text[(line % con.totalLines) * con.lineWidth];
        int32_t length = con.lineWidth;

        for (int32_t column = 0; column < con.lineWidth; ++column)
            lineBuffer[column] = (char)(cells[column] & UINT16_C(0x00ff));

        while (length > 0 && lineBuffer[length - 1] == ' ')
            lineBuffer[--length] = '\0';
        lineBuffer[length++] = '\n';
        lineBuffer[length] = '\0';
        FS_Write(lineBuffer, length, fileHandle);
    }

    FS_FCloseFile(fileHandle);
}

/* Source: CoDUOMP.exe 0x0040e360..0x0040e714.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040e360_0040e715.mcode.
 * Name and signature: exact same-module Mac symbol Console_Key. The Windows
 * optimizer copies whole 284-byte history records with REP MOVSD and inlines
 * Con_Bottom as con.displayLine = con.currentLine. */
void Console_Key(int32_t key)
{
    /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): provide the terminal
     * Ctrl-W word erase convention without changing shared chat-field input.
     * Treat whitespace and underscores as delimiters, deleting delimiters
     * immediately before the cursor and then the preceding word while
     * retaining any command text after the cursor. */
    if (key == 'w' && keyStates[K_CTRL].down != qfalse) {
        int32_t deleteStart = con_inputField.cursor;

        while (deleteStart > 0 && (con_inputField.buffer[deleteStart - 1] == '_' ||
                                   coduo_crt_isspace((int32_t)(signed char)con_inputField.buffer[deleteStart - 1]))) {
            --deleteStart;
        }
        while (deleteStart > 0 && con_inputField.buffer[deleteStart - 1] != '_' &&
               !coduo_crt_isspace((int32_t)(signed char)con_inputField.buffer[deleteStart - 1])) {
            --deleteStart;
        }

        if (deleteStart != con_inputField.cursor) {
            memmove(&con_inputField.buffer[deleteStart], &con_inputField.buffer[con_inputField.cursor],
                    strlen(&con_inputField.buffer[con_inputField.cursor]) + 1);
            con_inputField.cursor = deleteStart;
            Field_AdjustScroll(&con_inputField);
        }
        return;
    }

    if (key == 'l' && keyStates[K_CTRL].down != qfalse) {
        Cbuf_AddText("clear\n");
        return;
    }

    if (key == K_ENTER || key == K_KP_ENTER) {
        char command[CON_MESSAGE_COMMAND_SIZE];

        if ((cl_autocmd->integer != 0 || cls.state != CA_ACTIVE) && con_inputField.buffer[0] != '\\' && con_inputField.buffer[0] != '/') {
            strncpy(command, con_inputField.buffer, sizeof(command) - 1);
            command[sizeof(command) - 1] = '\0';
            Com_sprintf(con_inputField.buffer, CON_INPUT_BUFFER_SIZE, "\\%s", command);
            ++con_inputField.cursor;
        }

        Com_Printf("]%s\n", con_inputField.buffer);

        if (con_inputField.buffer[0] != '\\' && con_inputField.buffer[0] != '/') {
            if (con_inputField.buffer[0] == '\0')
                return;

            Cbuf_AddText("cmd say ");
            Cbuf_AddText(con_inputField.buffer);
        } else {
            Cbuf_AddText(&con_inputField.buffer[1]);
        }
        Cbuf_AddText("\n");

        con_historyFields[con_nextHistoryLine % CON_HISTORY_FIELD_COUNT] = con_inputField;
        ++con_nextHistoryLine;
        con_historyLine = con_nextHistoryLine;

        memset(con_inputField.buffer, 0, sizeof(con_inputField.buffer));
        con_inputField.cursor = 0;
        con_inputField.scroll = 0;
        con_inputField.widthInChars = CON_INPUT_BUFFER_SIZE;
        con_inputField.widthInPixels = con_fieldWidthPixels;
        con_inputField.charWidth = con_fieldCharWidth;
        con_inputField.charHeight = con_fieldCharHeight;
        con_inputField.fixedSize = qtrue;

        if (cls.state == CA_DISCONNECTED)
            SCR_UpdateScreen();
        return;
    }

    if (key == K_TAB) {
        CompleteCommand();
        return;
    }

    if ((key == K_MWHEELUP && keyStates[K_SHIFT].down != qfalse) || key == K_UPARROW || key == K_KP_UPARROW ||
        (coduo_crt_tolower(key) == 'p' && keyStates[K_CTRL].down != qfalse)) {
        if (con_nextHistoryLine - con_historyLine < CON_HISTORY_FIELD_COUNT && con_historyLine > 0) {
            --con_historyLine;
        }
        con_inputField = con_historyFields[con_historyLine % CON_HISTORY_FIELD_COUNT];
        Field_AdjustScroll(&con_inputField);
        return;
    }

    if ((key == K_MWHEELDOWN && keyStates[K_SHIFT].down != qfalse) || key == K_DOWNARROW || key == K_KP_DOWNARROW ||
        (coduo_crt_tolower(key) == 'n' && keyStates[K_CTRL].down != qfalse)) {
        if (con_historyLine == con_nextHistoryLine)
            return;

        ++con_historyLine;
        con_inputField = con_historyFields[con_historyLine % CON_HISTORY_FIELD_COUNT];
        Field_AdjustScroll(&con_inputField);
        return;
    }

    if (key == K_PGUP) {
        Con_PageUp();
        return;
    }
    if (key == K_PGDN) {
        Con_PageDown();
        return;
    }
    if (key == K_MWHEELUP) {
        for (int32_t page = 0; page < (keyStates[K_CTRL].down != qfalse ? CON_WHEEL_SCROLL_PAGES : 1); ++page) {
            Con_PageUp();
        }
        return;
    }
    if (key == K_MWHEELDOWN) {
        for (int32_t page = 0; page < (keyStates[K_CTRL].down != qfalse ? CON_WHEEL_SCROLL_PAGES : 1); ++page) {
            Con_PageDown();
        }
        return;
    }
    if (key == K_HOME && keyStates[K_CTRL].down != qfalse) {
        Con_Top();
        return;
    }
    if (key == K_END && keyStates[K_CTRL].down != qfalse) {
        Con_Bottom();
        return;
    }

    Field_KeyDownEvent(&con_inputField, key);
}

/* Source: CoDUOMP.exe 0x0040e720..0x0040e835.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040e720_0040e836.mcode.
 * Name and signature: exact same-module Mac symbol Message_Key. The embedded
 * 0x15 byte in each command format is part of the original wire command. */
void Message_Key(int32_t key)
{
    if (key != K_ESCAPE && key != K_ENTER && key != K_KP_ENTER) {
        Field_KeyDownEvent(&chatField, key);
        return;
    }

    if (key != K_ESCAPE && chatField.buffer[0] != '\0' && cls.state == CA_ACTIVE) {
        char command[CON_MESSAGE_COMMAND_SIZE];

        if (chat_playerNum != -1) {
            Com_sprintf(command, sizeof(command), "tell %i \"\x15%s\"\n", chat_playerNum, chatField.buffer);
        } else if (chat_team != qfalse) {
            Com_sprintf(command, sizeof(command), "say_team \"\x15%s\"\n", chatField.buffer);
        } else if (chat_squad != qfalse) {
            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            Com_sprintf(command, sizeof(command), "say_squad \"\x15%s\"\n", chatField.buffer);
        } else {
            Com_sprintf(command, sizeof(command), "say \"\x15%s\"\n", chatField.buffer);
        }
        CL_AddReliableCommand(command);
    }

    cls.keyCatchers &= ~KEYCATCH_MESSAGE;
    memset(chatField.buffer, 0, sizeof(chatField.buffer));
    chatField.cursor = 0;
    chatField.scroll = 0;
    chatField.widthInChars = CON_INPUT_BUFFER_SIZE;
}
