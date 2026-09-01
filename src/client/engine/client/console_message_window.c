#include "console.h"

enum {
    CON_STANDARD_MESSAGE_CAPACITY = 8,
    CON_STANDARD_VISIBLE_LINES = 3,
    CON_STANDARD_SCROLL_MSEC = 250,
    CON_STANDARD_FADE_IN_MSEC = 250,
    CON_STANDARD_FADE_OUT_MSEC = 500
};

/* Original Win32 backing arrays occupy the storage immediately before their
 * respective window records at 0x04e3ccac, 0x04e3cd30, and 0x04e3cdb4. */
static int32_t
    con_gameMessageStartTimes[CON_STANDARD_MESSAGE_CAPACITY]; /* 0x04e3cc4c */
static int32_t
    con_gameMessageEndTimes[CON_STANDARD_MESSAGE_CAPACITY]; /* 0x04e3cc6c */
static int32_t
    con_gameMessageLineIndices[CON_STANDARD_MESSAGE_CAPACITY]; /* 0x04e3cc8c */
static int32_t
    con_boldMessageStartTimes[CON_STANDARD_MESSAGE_CAPACITY]; /* 0x04e3ccd0 */
static int32_t
    con_boldMessageEndTimes[CON_STANDARD_MESSAGE_CAPACITY]; /* 0x04e3ccf0 */
static int32_t
    con_boldMessageLineIndices[CON_STANDARD_MESSAGE_CAPACITY]; /* 0x04e3cd10 */
static int32_t
    con_subtitleStartTimes[CON_STANDARD_MESSAGE_CAPACITY]; /* 0x04e3cd54 */
static int32_t
    con_subtitleEndTimes[CON_STANDARD_MESSAGE_CAPACITY]; /* 0x04e3cd74 */
static int32_t
    con_subtitleLineIndices[CON_STANDARD_MESSAGE_CAPACITY]; /* 0x04e3cd94 */

/* Original Win32 mini-console arrays are the three 100-dword regions at
 * 0x04e3cdd8, 0x04e3cf68, and 0x04e3d0f8. */
static int32_t
    con_miniconStartTimes[CON_MINICON_MAX_LINES]; /* 0x04e3cdd8 */
static int32_t
    con_miniconEndTimes[CON_MINICON_MAX_LINES]; /* 0x04e3cf68 */
static int32_t
    con_miniconLineIndices[CON_MINICON_MAX_LINES]; /* 0x04e3d0f8 */

cvar_t *con_gameMessageTime;
cvar_t *con_boldGameMessageTime;
cvar_t *con_miniconTime;
cvar_t *con_minicon;
cvar_t *con_miniconLines;
vec4_t con_miniconColor;

/* Source: CoDUOMP.exe 0x004095e0..0x00409616.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004095e0_00409617.mcode.
 * Name and signature: same-module Mac symbol Con_InitMessageWindow; argument
 * roles are proved by the original Con_OneTimeInit stores and update path. */
void Con_InitMessageWindow(console_message_window_t *window,
                           int32_t *lineStartTimes, int32_t *lineEndTimes,
                           int32_t *lineIndices, int32_t lineCapacity,
                           int32_t visibleLineCount, int32_t scrollTime,
                           int32_t fadeInTime, int32_t fadeOutTime)
{
    window->lineStartTimes = lineStartTimes;
    window->lineEndTimes = lineEndTimes;
    window->lineIndices = lineIndices;
    window->activeLineIndex = 0;
    window->lineCapacity = lineCapacity;
    window->visibleLineCount = visibleLineCount;
    window->scrollTime = scrollTime;
    window->fadeInTime = fadeInTime;
    window->fadeOutTime = fadeOutTime;
}

/* Source: CoDUOMP.exe 0x00409bb0..0x00409db9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00409bb0_00409dba.mcode.
 * Name and signature: exact same-module Mac symbol Con_OneTimeInit. */
void Con_OneTimeInit(void)
{
    con_gameMessageTime = Cvar_Get("con_gamemessagetime", "5", 0);
    con_boldGameMessageTime = Cvar_Get("con_boldgamemessagetime", "8", 0);
    con_miniconTime = Cvar_Get("con_minicontime", "4", CVAR_ARCHIVE);
    con_minicon = Cvar_Get("con_minicon", "0", CVAR_ARCHIVE);
    con_miniconLines = Cvar_Get("con_miniconlines", "5", CVAR_ARCHIVE);

    Con_InitMessageWindow(
        &con_gameMessageWindow, con_gameMessageStartTimes,
        con_gameMessageEndTimes, con_gameMessageLineIndices,
        CON_STANDARD_MESSAGE_CAPACITY, CON_STANDARD_VISIBLE_LINES,
        CON_STANDARD_SCROLL_MSEC, CON_STANDARD_FADE_IN_MSEC,
        CON_STANDARD_FADE_OUT_MSEC);
    Con_InitMessageWindow(
        &con_boldGameMessageWindow, con_boldMessageStartTimes,
        con_boldMessageEndTimes, con_boldMessageLineIndices,
        CON_STANDARD_MESSAGE_CAPACITY, CON_STANDARD_VISIBLE_LINES,
        CON_STANDARD_SCROLL_MSEC, CON_STANDARD_FADE_IN_MSEC,
        CON_STANDARD_FADE_OUT_MSEC);
    Con_InitMessageWindow(
        &con_subtitleWindow, con_subtitleStartTimes, con_subtitleEndTimes,
        con_subtitleLineIndices, CON_STANDARD_MESSAGE_CAPACITY,
        CON_STANDARD_VISIBLE_LINES, CON_STANDARD_SCROLL_MSEC,
        CON_STANDARD_FADE_IN_MSEC, CON_STANDARD_FADE_OUT_MSEC);

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if (con_miniconLines->integer < CON_MINICON_MIN_LINES) {
        Cvar_Set("con_miniconlines", "0");
    } else if (con_miniconLines->integer > CON_MINICON_MAX_LINES) {
        Cvar_Set("con_miniconlines", va("%d", CON_MINICON_MAX_LINES));
    }
    Con_InitMessageWindow(
        &con_miniConsoleWindow, con_miniconStartTimes, con_miniconEndTimes,
        con_miniconLineIndices, con_miniconLines->integer,
        0, 0, 0, 0);

    for (int32_t component = 0; component < 4; ++component)
        con_miniconColor[component] = 1.0f;

    con.lineWidth = -1;
    Con_CheckResize();
    con.initialized = qtrue;
}

/* Source: CoDUOMP.exe 0x00409960..0x00409acd.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00409960_00409ace.mcode.
 * Name: exact same-module Mac symbol Con_UpdateMessageWindowLine. The original
 * samples the millisecond clock separately for each stored value; the repeated
 * calls below preserve that observable ordering. */
void Con_UpdateMessageWindowLine(console_message_window_t *window,
                                 qboolean advanceLine,
                                 int32_t messageTime)
{
    int32_t slot = window->activeLineIndex;

    window->lineStartTimes[slot] = (int32_t)Sys_Milliseconds();
    window->lineEndTimes[slot] = (int32_t)(
        Sys_Milliseconds() + (uint32_t)messageTime);
    window->lineIndices[slot] = con.currentLine;

    if (advanceLine == qfalse || window->lineCapacity <= 0)
        return;

    window->activeLineIndex =
        (window->activeLineIndex + 1) % window->lineCapacity;

    for (int32_t visibleLine = 0;
         visibleLine < window->visibleLineCount; ++visibleLine) {
        slot = (window->activeLineIndex + visibleLine) %
               window->lineCapacity;

        const int32_t now = (int32_t)Sys_Milliseconds();
        const int32_t endTime = window->lineEndTimes[slot];
        if ((int32_t)((uint32_t)endTime -
                      (uint32_t)window->fadeOutTime) > now) {
            const uint32_t lineDuration =
                (uint32_t)endTime - (uint32_t)window->lineStartTimes[slot];
            const uint32_t adjustedStart =
                Sys_Milliseconds() + (uint32_t)window->fadeOutTime -
                lineDuration;
            window->lineStartTimes[slot] = (int32_t)adjustedStart;
            window->lineEndTimes[slot] = (int32_t)(
                Sys_Milliseconds() + (uint32_t)window->fadeOutTime);
        }
    }
}

/* Source: CoDUOMP.exe 0x00409ad0..0x00409b2a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00409ad0_00409b2b.mcode.
 * Name: exact same-module Mac symbol Con_UpdateNotifyLine. */
void Con_UpdateNotifyLine(console_message_destination_t destination,
                          qboolean advanceLine, int32_t messageTime)
{
    console_message_window_t *window;

    if (con.currentLine < 0)
        return;

    switch (destination) {
    case CON_DEST_MINICONSOLE:
        window = &con_miniConsoleWindow;
        break;
    case CON_DEST_GAME_MESSAGE:
        window = &con_gameMessageWindow;
        break;
    case CON_DEST_BOLD_GAME_MESSAGE:
        window = &con_boldGameMessageWindow;
        break;
    case CON_DEST_SUBTITLE:
        window = &con_subtitleWindow;
        break;
    default:
        return;
    }

    Con_UpdateMessageWindowLine(window, advanceLine, messageTime);
}

/* Source: CoDUOMP.exe 0x00409b40..0x00409ba7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00409b40_00409ba8.mcode.
 * Name and signature: exact same-module Mac symbol Con_Linefeed; parameter
 * roles are proved by its call sites and Con_UpdateNotifyLine dispatch. */
void Con_Linefeed(console_message_destination_t destination,
                  int32_t messageTime)
{
    Con_UpdateNotifyLine(destination, qtrue, messageTime);

    if (con.displayLine == con.currentLine)
        ++con.displayLine;
    con.lineCursor = 0;
    ++con.currentLine;

    const int32_t row = con.currentLine % con.totalLines;
    for (int32_t column = 0; column < con.lineWidth; ++column)
        con.text[row * con.lineWidth + column] = CON_EMPTY_TEXT_CELL;
}
