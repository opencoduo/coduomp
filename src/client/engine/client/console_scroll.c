#include "console.h"

enum {
    CON_PAGE_SCROLL_LINES = 2
};

/* Source: CoDUOMP.exe 0x0040b410..0x0040b43a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040b410_0040b43b.mcode.
 * Name and signature: exact same-module Mac symbol Con_PageUp. */
void Con_PageUp(void)
{
    con.displayLine = (int32_t)(
        (uint32_t)con.displayLine - (uint32_t)CON_PAGE_SCROLL_LINES);
    if ((int32_t)((uint32_t)con.currentLine -
                  (uint32_t)con.displayLine) >= con.totalLines) {
        con.displayLine = (int32_t)(
            (uint32_t)con.currentLine - (uint32_t)con.totalLines + 1u);
    }
    coduomp_console_manually_scrolled = qtrue;
}

/* Source: CoDUOMP.exe 0x0040b440..0x0040b45d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040b440_0040b45e.mcode.
 * Name and signature: exact same-module Mac symbol Con_PageDown. */
void Con_PageDown(void)
{
    con.displayLine = (int32_t)(
        (uint32_t)con.displayLine + (uint32_t)CON_PAGE_SCROLL_LINES);
    if (con.displayLine > con.currentLine)
        con.displayLine = con.currentLine;
    coduomp_console_manually_scrolled =
        con.displayLine != con.currentLine;
}

/* Source: CoDUOMP.exe 0x0040b460..0x0040b47d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040b460_0040b47e.mcode.
 * Name and signature: exact same-module Mac symbol Con_Top. */
void Con_Top(void)
{
    con.displayLine = con.totalLines;
    const int32_t oldestLine = (int32_t)(
        (uint32_t)con.currentLine - (uint32_t)con.totalLines);
    if (oldestLine >= con.totalLines)
        con.displayLine = (int32_t)((uint32_t)oldestLine + 1u);
    coduomp_console_manually_scrolled = qtrue;
}

/* Source: CoDUOMP.exe 0x0040b480..0x0040b48a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040b480_0040b48b.mcode.
 * Name and signature: exact same-module Mac symbol Con_Bottom. */
void Con_Bottom(void)
{
    con.displayLine = con.currentLine;
    coduomp_console_manually_scrolled = qfalse;
}
