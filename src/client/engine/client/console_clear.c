#include "console.h"

#include <stddef.h>
#include <string.h>

/* Original Win32 record is at 0x04e3ccac. */
console_message_window_t con_gameMessageWindow;
/* Original Win32 record is at 0x04e3cd30. */
console_message_window_t con_boldGameMessageWindow;
/* Original Win32 record is at 0x04e3d288. */
console_message_window_t con_miniConsoleWindow;
/* Original Win32 record is at 0x04e3cdb4. */
console_message_window_t con_subtitleWindow;
/* Registered by the client common initialization; original pointer
 * 0x04929080 and cvar name "cl_running" are both directly referenced. */
cvar_t *cl_running;

/* NOT_FROM_ORIGINAL_SOURCE: the separately emitted Con_ClearMessageWindow
 * multiplies lineCapacity by four before its memset-shaped clear. Its inlined
 * retail consumers instead execute one dword store per unsigned capacity
 * count, so this boundary preserves that distinct operation count. */
static void coduomp_con_zero_dwords(int32_t *values, uint32_t count)
{
    while (count != 0) {
        *values++ = 0;
        --count;
    }
}

/* Source: CoDUOMP.exe 0x00409300..0x0040933c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00409300_0040933d.mcode.
 * Name and signature: exact same-module Mac symbol Con_ClearMessageWindow. */
void Con_ClearMessageWindow(console_message_window_t *window)
{
    const uint32_t startByteCount =
        (uint32_t)window->lineCapacity * (uint32_t)sizeof(uint32_t);
    memset(window->lineStartTimes, 0, (size_t)startByteCount);

    const uint32_t endByteCount =
        (uint32_t)window->lineCapacity * (uint32_t)sizeof(uint32_t);
    memset(window->lineEndTimes, 0, (size_t)endByteCount);
    window->activeLineIndex = 0;
}

/* Source: CoDUOMP.exe 0x00409340..0x0040938a.
 * Name: exact same-module Mac symbol Con_ClearNotify. */
void Con_ClearNotify(void)
{
    coduomp_con_zero_dwords(
        con_gameMessageWindow.lineStartTimes,
        (uint32_t)con_gameMessageWindow.lineCapacity);
    coduomp_con_zero_dwords(
        con_gameMessageWindow.lineEndTimes,
        (uint32_t)con_gameMessageWindow.lineCapacity);
    con_gameMessageWindow.activeLineIndex = 0;

    coduomp_con_zero_dwords(
        con_boldGameMessageWindow.lineStartTimes,
        (uint32_t)con_boldGameMessageWindow.lineCapacity);
    coduomp_con_zero_dwords(
        con_boldGameMessageWindow.lineEndTimes,
        (uint32_t)con_boldGameMessageWindow.lineCapacity);
    con_boldGameMessageWindow.activeLineIndex = 0;
}

/* Source: CoDUOMP.exe 0x00409390..0x004093b5.
 * Evidence: repaired function record
 * coduomp/mcode/CoDUOMP/FUN_00409390_004093b6.mcode.
 * Name: exact same-module Mac symbol Con_ClearMiniConsole. */
void Con_ClearMiniConsole(void)
{
    int32_t lineCapacity = con_miniConsoleWindow.lineCapacity;

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if (lineCapacity < CON_MINICON_MIN_LINES) {
        lineCapacity = CON_MINICON_MIN_LINES;
    } else if (lineCapacity > CON_MINICON_MAX_LINES) {
        lineCapacity = CON_MINICON_MAX_LINES;
    }
    coduomp_con_zero_dwords(
        con_miniConsoleWindow.lineStartTimes,
        (uint32_t)lineCapacity);
    coduomp_con_zero_dwords(
        con_miniConsoleWindow.lineEndTimes,
        (uint32_t)lineCapacity);
    con_miniConsoleWindow.activeLineIndex = 0;
}

/* Source: CoDUOMP.exe 0x004093c0..0x004093e5.
 * Evidence: repaired function record
 * coduomp/mcode/CoDUOMP/FUN_004093c0_004093e6.mcode.
 * Name: exact same-module Mac symbol Con_ClearSubtitles. */
void Con_ClearSubtitles(void)
{
    coduomp_con_zero_dwords(
        con_subtitleWindow.lineStartTimes,
        (uint32_t)con_subtitleWindow.lineCapacity);
    coduomp_con_zero_dwords(
        con_subtitleWindow.lineEndTimes,
        (uint32_t)con_subtitleWindow.lineCapacity);
    con_subtitleWindow.activeLineIndex = 0;
}

/* Source: CoDUOMP.exe 0x0040b490..0x0040b52e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040b490_0040b52f.mcode.
 * Name and signature: exact same-module Mac symbol Con_Close. */
void Con_Close(void)
{
    if (cl_running->integer == 0)
        return;

    memset(con_inputField.buffer, 0, sizeof(con_inputField.buffer));
    con_inputField.cursor = 0;
    con_inputField.scroll = 0;
    con_inputField.widthInChars = CON_INPUT_BUFFER_SIZE;

    Con_ClearNotify();
    /* The shared mini-console clear applies the original-bug capacity guard. */
    Con_ClearMiniConsole();
    Con_ClearSubtitles();
    cls.keyCatchers &= ~KEYCATCH_CONSOLE;
    con.finalFrac = 0.0f;
    con.displayFrac = 0.0f;
}
