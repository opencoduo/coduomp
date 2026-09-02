// Source: uo_cgame_mp_x86.dll 0x30019050..0x3001918c
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30019050_3001918c.mcode
//
// CG_PriorityCenterPrint — queue a center-screen HUD message with a priority.
//
// The mechanical .mcode `# name PM_Weapon_AllowReload` is a SIZE-GUESS and is
// wrong: it was assigned only because a broad cgame corpus name happened to
// share the win size 0x13c. The machine code proves this is the center-print
// routine — it localizes the message via CG_TranslateMessage(str, "Center Print")
// (.rdata tag @0x30076cdc), copies 1023 bytes with the CRT strncpy into the
// cg_centerPrintString[1024] buffer, sets cg_centerPrintTime/Y/CharWidth,
// word-wraps the text and counts lines.
// The center-string renderer at 0x300191b0 reads the same globals back
// (cg_centerPrintTime @0x300191bd, CharWidth/Y FILD @0x30019220/0x30019256, priority
// @0x300191fd), confirming the identity.
//
// i386 ABI (proven from callers 0x30022560 and the wrapper thunk 0x30019190):
//   str      in [esp+0x10]  (arg0, char *)
//   y        in [esp+0x14]  (arg1, float)
//   charWidth in [esp+0x18] (arg2, float)
//   priority in EAX         (register argument)
// Stack args are caller-cleaned (callers do ADD ESP,0xc); EAX is the register-
// passed priority. The register argument is an ABI detail, declared here as a
// normal C parameter.

#include "client/cgame/globals.h"
#include "client/cgame/client_recovered.h"

#include <string.h>

/* Word-wrap column limit: a line is flagged for wrapping once it reaches 75
 * drawable glyphs (CMP ESI,0x4b at 0x300190c8). */
enum {
    CG_CENTERPRINT_WRAP_COLUMN = 75
};

/* Display duration added to cg_time when the print is queued (ADD ECX,0x7d0 at
 * 0x300190fc). */
enum {
    CG_CENTERPRINT_DURATION_MS = 2000
};

/* Direct CRT strncpy count for the 1024-byte cg_centerPrintString buffer (PUSH
 * 0x3ff at 0x3001907b); the routine separately NUL-forces byte 1023. */
enum {
    CG_CENTERPRINT_COPY_COUNT = 1023
};

void CG_PriorityCenterPrint(const char *str, float y, float charWidth, int32_t priority)
{
    const char *localized;
    char *cursor;
    int32_t c;
    int32_t charsThisLine;  /* ESI in the wrap loop */
    int32_t wrapPending;    /* EDI in the wrap loop */

    /* If a center print is already active and the new request is lower priority,
     * leave the current one untouched. (0x30019054..0x30019066) */
    if (cg_centerPrintTime != 0 && priority < cg_centerPrintPriority) {
        return;
    }

    /* Localize/format the message, then copy it into the center-print buffer.
     * (0x3001906c..0x30019086) */
    localized = CG_TranslateMessage(str, "Center Print");
    (void)strncpy(cg_centerPrintString, localized, (size_t)CG_CENTERPRINT_COPY_COUNT);

    /* Record the priority of this (now pending) print and force the final byte
     * of the buffer to NUL. (0x30019090, 0x3001909d) */
    cg_centerPrintPriority = priority;
    cg_centerPrintString[MAX_STRING_CHARS - 1] = '\0';

    /* First pass: word-wrap. Walk the buffer via the
     * CG_SE_READ_CHAR_FROM_STRING encoded-character iterator, which advances
     * one byte or one valid two-byte Asian pair and does not interpret color
     * codes. Once a line reaches CG_CENTERPRINT_WRAP_COLUMN characters, the
     * next space is rewritten to '\n'. An explicit '\n' resets the counters.
     * (0x300190a4..0x300190f0) — skipped entirely when the buffer is empty. */
    cursor = cg_centerPrintString;
    charsThisLine = 0;
    wrapPending = 0;
    if (cg_centerPrintString[0] != '\0') {
        do {
            c = coduo_int32_from_bits((uint32_t)cgame_syscall(CG_SE_READ_CHAR_FROM_STRING, (intptr_t)&cursor, 0));
            if (c == '\n') {
                wrapPending = 0;
                charsThisLine = 0;
            } else {
                charsThisLine = coduo_int32_from_bits((uint32_t)charsThisLine + 1u);
                if (charsThisLine >= CG_CENTERPRINT_WRAP_COLUMN) {
                    wrapPending = 1;
                }
                /* Only break on a space once the line is over-long. */
                if (wrapPending && c == ' ') {
                    /* The iterator has already advanced past the space, so the
                     * space itself is at cursor[-1]. (MOV byte [EDX-1],0xa) */
                    cursor[-1] = '\n';
                    wrapPending = 0;
                    charsThisLine = 0;
                }
            }
        } while (*cursor != '\0');
    }

    /* Latch the display time (cg_time + 2000ms) and the rounded screen position.
     * _ftol2 (0x3006be3c) truncates each float coordinate toward zero.
     * (0x300190f2..0x3001911b) */
    uint32_t printTimeBase = cg_time;       /* 0x300190f2 MOV ECX,[cg_time] */
    long double yLive = (long double)y;     /* 0x300190f8 FLD y */
    cg_centerPrintTime = coduo_int32_from_bits(printTimeBase + (uint32_t)CG_CENTERPRINT_DURATION_MS);
    int32_t yInteger = coduo_fp_to_i32_extended(yLive);
    long double charWidthLive = (long double)charWidth; /* 0x3001910d FLD */
    cg_centerPrintY = yInteger;             /* 0x30019111 store first result */
    cg_centerPrintCharWidth = coduo_fp_to_i32_extended(charWidthLive);

    /* Second pass: count the number of lines (starts at 1). A raw '\n' counts a
     * line; a literal "\\n" escape sequence also counts a line and the 'n' is
     * consumed. (0x30019120..0x30019186) — skipped when the buffer is empty. */
    cg_centerPrintLines = 1;
    cursor = cg_centerPrintString;
    if (cg_centerPrintString[0] != '\0') {
        do {
            c = coduo_int32_from_bits((uint32_t)cgame_syscall(CG_SE_READ_CHAR_FROM_STRING, (intptr_t)&cursor, 0));
            if (c == '\n') {
                cg_centerPrintLines = coduo_int32_from_bits((uint32_t)cg_centerPrintLines + 1u);
            } else if (c == '\\' && *cursor == 'n') {
                cg_centerPrintLines = coduo_int32_from_bits((uint32_t)cg_centerPrintLines + 1u);
                cursor++;
            }
        } while (*cursor != '\0');
    }
}
