#ifndef CODUOMP_CLIENT_CONSOLE_H
#define CODUOMP_CLIENT_CONSOLE_H

#include <stddef.h>
#include <stdint.h>

#include "cgame.h"
#include "../q_shared.h"
#include "qcommon/console_field_types.h"
#include "qcommon/q_key_types.h"

enum {
    /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): retain four times the
     * retail scrollback so a noisy map load remains available at native-width
     * resolutions. The stock source keeps the 65,536-cell layout. */
    CON_TEXT_CELL_COUNT = 262144,
    CON_DUMP_BUFFER_SIZE = CON_TEXT_CELL_COUNT + 2,
    CON_COMPLETION_MATCH_SIZE = 1024,
    CON_MINICON_MIN_LINES = 0,
    CON_MINICON_MAX_LINES = 100,
    CON_EMPTY_TEXT_CELL = 0x0720
};

typedef struct key_state_s {
    qboolean down;       /* original +0x00 */
    int32_t repeatCount; /* original +0x04 */
    char *binding;       /* original +0x08 */
} key_state_t;

typedef enum console_message_destination_e {
    CON_DEST_MINICONSOLE = 0,
    CON_DEST_GAME_MESSAGE = 1,
    CON_DEST_BOLD_GAME_MESSAGE = 2,
    CON_DEST_SUBTITLE = 3,
    CON_DEST_NONE = 4
} console_message_destination_t;

/* The stock console_t at 0x04e1cc20 has the original layout. Each 16-bit
 * text cell stores its character in the low byte and color in the high byte.
 * The default compatibility build enlarges only the in-process text ring; the
 * following fields move with it and do not cross a serialized or module ABI.
 * The next retail object begins after visiblePixelHeight at 0x04e3cc4c. */
typedef struct console_state_s {
    qboolean initialized;                         /* original +0x00000 */
    uint16_t text[CON_TEXT_CELL_COUNT];           /* original +0x00004 */
    int32_t currentLine;                          /* original +0x20004 */
    int32_t lineCursor;                           /* original +0x20008 */
    int32_t displayLine;                          /* original +0x2000c */
    console_message_destination_t
        lastMessageDestination;                   /* original +0x20010 */
    int32_t lineWidth;                            /* original +0x20014 */
    int32_t totalLines;                           /* original +0x20018 */
    float xAdjust;                                /* original +0x2001c */
    float displayFrac;                            /* original +0x20020 */
    float finalFrac;                              /* original +0x20024 */
    int32_t visiblePixelHeight;                   /* original +0x20028 */
} console_state_t;

typedef struct console_message_window_s {
    int32_t *lineStartTimes; /* original +0x00 */
    int32_t *lineEndTimes;   /* original +0x04 */
    int32_t *lineIndices;    /* original +0x08: console line for each slot */
    int32_t activeLineIndex; /* original +0x0c: next circular slot */
    int32_t lineCapacity;    /* original +0x10 */
    int32_t visibleLineCount;/* original +0x14 */
    int32_t scrollTime;      /* original +0x18, milliseconds */
    int32_t fadeInTime;      /* original +0x1c, milliseconds */
    int32_t fadeOutTime;     /* original +0x20, milliseconds */
} console_message_window_t;

extern console_message_window_t con_gameMessageWindow;
extern console_message_window_t con_boldGameMessageWindow;
extern console_message_window_t con_miniConsoleWindow;
extern console_message_window_t con_subtitleWindow;
extern console_input_field_t con_inputField;
extern console_input_field_t chatField;
extern console_input_field_t con_historyFields[CON_HISTORY_FIELD_COUNT];
extern int32_t con_historyLine;
extern int32_t con_nextHistoryLine;
extern console_state_t con;
extern qboolean coduomp_console_manually_scrolled;
extern key_state_t keyStates[MAX_KEYS];
extern qboolean key_overstrikeMode;
extern qboolean chat_team;
extern qboolean chat_squad;
extern int32_t chat_playerNum;
extern cvar_t *con_restricted;
extern cvar_t *con_debug;
extern cvar_t *scr_conspeed;
extern cvar_t *cl_noprint;
extern cvar_t *con_gameMessageTime;
extern cvar_t *con_boldGameMessageTime;
extern cvar_t *con_miniconTime;
extern cvar_t *con_minicon;
extern cvar_t *con_miniconLines;
extern vec4_t con_miniconColor;
extern int32_t con_fieldWidthPixels;
extern float con_fieldCharWidth;
extern float con_fieldCharHeight;
extern const char *completionString;
extern int32_t completionMatchCount;
extern char completionShortestMatch[CON_COMPLETION_MATCH_SIZE];

void Con_ClearNotify(void);
void Con_ClearMiniConsole(void);
void Con_ClearSubtitles(void);
void Con_InitMessageWindow(console_message_window_t *window,
                           int32_t *lineStartTimes, int32_t *lineEndTimes,
                           int32_t *lineIndices, int32_t lineCapacity,
                           int32_t visibleLineCount, int32_t scrollTime,
                           int32_t fadeInTime, int32_t fadeOutTime);
void Con_ClearMessageWindow(console_message_window_t *window);
void Con_UpdateMessageWindowLine(console_message_window_t *window,
                                 qboolean advanceLine,
                                 int32_t messageTime);
void Con_UpdateNotifyLine(console_message_destination_t destination,
                          qboolean advanceLine, int32_t messageTime);
void Con_Linefeed(console_message_destination_t destination,
                  int32_t messageTime);
void Con_CheckResize(void);
void Con_Close(void);
void Con_Bottom(void);
void Con_Top(void);
void Con_PageDown(void);
void Con_PageUp(void);
void Con_RunConsole(void);
void Con_DrawConsole(void);
void coduomp_console_draw_compat(void);
void Con_DrawSolidConsole(float fraction);
void Con_DrawMessageWindow(console_message_window_t *window,
                           int32_t x, int32_t y, float alpha,
                           int32_t drawMode);
void Con_DrawMessageWindowBottomUp(console_message_window_t *window,
                                   int32_t x, int32_t y, float alpha,
                                   qboolean centered);
void Con_DrawStringOnHUD(const uint16_t *encodedText,
                         int32_t encodedCount, int32_t x, int32_t y,
                         float alpha, qboolean centered);
void Con_DrawSubtitles(int32_t x, int32_t y, float alpha,
                       int32_t drawMode);
void Con_DrawMiniConsole(int32_t x, int32_t y, float alpha);
void Con_DrawBoldMessages(int32_t x, int32_t y, float alpha,
                          int32_t drawMode);
void Con_DrawNotify(int32_t x, int32_t y, float alpha,
                    int32_t drawMode);
void Con_Init(void);
void Con_OneTimeInit(void);
int32_t CL_ConsolePrint_AddLine(
    const char *text, console_message_destination_t destination,
    int32_t messageTime, int32_t lineWidth, int32_t initialColor);
void CL_AddConsoleInfoColor(const vec3_t color,
                            int32_t firstComponentTag);
void CL_AddConsoleInfoChar(const char *text, int32_t initialColor);
void CL_ConsolePrint(const char *text,
                     console_message_destination_t destination,
                     int32_t messageTime, int32_t lineWidth);
void CL_DeathMessagePrint(
    console_message_destination_t destination, const char *attackerName,
    const float *attackerColor, const char *weaponName,
    const float *weaponColor, const char *victimName,
    float weaponIconWidth, float weaponIconHeight,
    const float *victimColor, int32_t messageTime);
void Con_DrawInput(void);
void Con_DrawSay(int32_t y);
void CL_ConsoleFixPosition(void);
void Con_JumpToDemoEnd_f(void);
void Con_ToggleConsole_f(void);
void Con_MessageMode_f(void);
void Con_MessageMode2_f(void);
void Con_MessageMode3_f(void);
void Con_MessageSquad_f(void);
void Con_Clear_f(void);
void Con_Dump_f(void);
void Console_Key(int32_t key);
void Message_Key(int32_t key);

void Field_Clear(console_input_field_t *field);
void Field_CompleteCommand(console_input_field_t *field);
void Field_CharEvent(console_input_field_t *field, int32_t character);
void Field_Paste(console_input_field_t *field);
void Field_KeyDownEvent(console_input_field_t *field, int32_t key);
void Field_AdjustScroll(console_input_field_t *field);
void Field_Draw(console_input_field_t *field, int32_t x, int32_t y);
void FindMatches(const char *candidate);
void PrintMatches(const char *candidate);
void PrintCvarMatches(const char *candidate);
void keyConcatArgs(void);
void ConcatRemaining(const char *source, const char *separator);
void CompleteCommand(void);
void PbClientCompleteCommand(char *command, int32_t commandSize);
void PbServerCompleteCommand(char *command, int32_t commandSize);

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(console_input_field_t) == 0x4,
               "console_input_field_t original alignment");
_Static_assert(offsetof(console_input_field_t, cursor) == 0x000,
               "console_input_field_t cursor offset");
_Static_assert(sizeof(((console_input_field_t *)0)->cursor) == 0x004,
               "console_input_field_t cursor extent");
_Static_assert(offsetof(console_input_field_t, scroll) == 0x004,
               "console_input_field_t scroll offset");
_Static_assert(sizeof(((console_input_field_t *)0)->scroll) == 0x004,
               "console_input_field_t scroll extent");
_Static_assert(offsetof(console_input_field_t, widthInChars) == 0x008,
               "console_input_field_t widthInChars offset");
_Static_assert(sizeof(((console_input_field_t *)0)->widthInChars) == 0x004,
               "console_input_field_t widthInChars extent");
_Static_assert(offsetof(console_input_field_t, widthInPixels) == 0x00c,
               "console_input_field_t widthInPixels offset");
_Static_assert(sizeof(((console_input_field_t *)0)->widthInPixels) == 0x004,
               "console_input_field_t widthInPixels extent");
_Static_assert(offsetof(console_input_field_t, charWidth) == 0x010,
               "console_input_field_t charWidth offset");
_Static_assert(sizeof(((console_input_field_t *)0)->charWidth) == 0x004,
               "console_input_field_t charWidth extent");
_Static_assert(offsetof(console_input_field_t, charHeight) == 0x014,
               "console_input_field_t charHeight offset");
_Static_assert(sizeof(((console_input_field_t *)0)->charHeight) == 0x004,
               "console_input_field_t charHeight extent");
_Static_assert(offsetof(console_input_field_t, fixedSize) == 0x018,
               "console_input_field_t fixedSize offset");
_Static_assert(sizeof(((console_input_field_t *)0)->fixedSize) == 0x004,
               "console_input_field_t fixedSize extent");
_Static_assert(offsetof(console_input_field_t, buffer) == 0x1c,
               "console_input_field_t buffer offset");
_Static_assert(sizeof(((console_input_field_t *)0)->buffer) == 0x100,
               "console_input_field_t buffer extent");
_Static_assert(sizeof(console_input_field_t) == 0x11c,
               "console_input_field_t original size");


_Static_assert(_Alignof(console_message_window_t) == 0x4,
               "console_message_window_t original alignment");
_Static_assert(offsetof(console_message_window_t, lineStartTimes) == 0x00,
               "console_message_window_t lineStartTimes offset");
_Static_assert(sizeof(((console_message_window_t *)0)->lineStartTimes) == 0x04,
               "console_message_window_t lineStartTimes extent");
_Static_assert(offsetof(console_message_window_t, lineEndTimes) == 0x04,
               "console_message_window_t lineEndTimes offset");
_Static_assert(sizeof(((console_message_window_t *)0)->lineEndTimes) == 0x04,
               "console_message_window_t lineEndTimes extent");
_Static_assert(offsetof(console_message_window_t, lineIndices) == 0x08,
               "console_message_window_t lineIndices offset");
_Static_assert(sizeof(((console_message_window_t *)0)->lineIndices) == 0x04,
               "console_message_window_t lineIndices extent");
_Static_assert(offsetof(console_message_window_t, activeLineIndex) == 0x0c,
               "console_message_window_t activeLineIndex offset");
_Static_assert(sizeof(((console_message_window_t *)0)->activeLineIndex) == 0x04,
               "console_message_window_t activeLineIndex extent");
_Static_assert(offsetof(console_message_window_t, lineCapacity) == 0x10,
               "console_message_window_t lineCapacity offset");
_Static_assert(sizeof(((console_message_window_t *)0)->lineCapacity) == 0x04,
               "console_message_window_t lineCapacity extent");
_Static_assert(offsetof(console_message_window_t, visibleLineCount) == 0x14,
               "console_message_window_t visibleLineCount offset");
_Static_assert(sizeof(((console_message_window_t *)0)->visibleLineCount) ==
                   0x04,
               "console_message_window_t visibleLineCount extent");
_Static_assert(offsetof(console_message_window_t, scrollTime) == 0x18,
               "console_message_window_t scrollTime offset");
_Static_assert(sizeof(((console_message_window_t *)0)->scrollTime) == 0x04,
               "console_message_window_t scrollTime extent");
_Static_assert(offsetof(console_message_window_t, fadeInTime) == 0x1c,
               "console_message_window_t fadeInTime offset");
_Static_assert(sizeof(((console_message_window_t *)0)->fadeInTime) == 0x04,
               "console_message_window_t fadeInTime extent");
_Static_assert(offsetof(console_message_window_t, fadeOutTime) == 0x20,
               "console_message_window_t fadeOutTime offset");
_Static_assert(sizeof(((console_message_window_t *)0)->fadeOutTime) == 0x04,
               "console_message_window_t fadeOutTime extent");
_Static_assert(sizeof(console_message_window_t) == 0x24,
               "console_message_window_t original size");
#endif

#endif
